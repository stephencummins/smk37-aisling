// SPDX-License-Identifier: GPL-3.0-only
#include "PluginProcessor.h"
#include "PluginEditor.h"

const AislingProcessor::Field AislingProcessor::FIELDS[NF] = {
    { "a", "Osc A", 0, 3, true },          { "width", "Pulse width", 5, 50, false },
    { "b", "Osc B", 0, 3, true },          { "semi", "B semitones", -24, 36, false },
    { "cents", "B cents", -50, 50, false }, { "level", "B level", 0, 100, false },
    { "fm", "FM", 0, 100, false },         { "fm_env", "FM by envelope", 0, 100, false },
    { "sub", "Sub", 0, 100, false },       { "noise", "Noise", 0, 100, false },
    { "cutoff", "Filter", -36, 36, false }, { "res", "Resonance", 0, 100, false },
    { "env", "Filter envelope", 0, 100, false }, { "key", "Key follow", 0, 100, false },
    { "attack", "Attack", 0, 100, false }, { "gain", "Gain", -24, 12, false },
};
const char* const AislingProcessor::LAYER_ID[NL] = { "drone", "seq", "lead", "seq2" };
const char* const AislingProcessor::LAYER_NAME[NL] = { "Drone", "Sequence", "Lead", "Sequence 2" };

namespace
{
std::atomic<AislingProcessor*> theOwner { nullptr };
std::atomic<juce::uint32> ownerSeen { 0 };       // when the owner last rendered (ms)
uint8_t plain[AislingProcessor::NL][AislingProcessor::NF];
std::once_flag once;
const float ENGINE_RATE = 44100.0f;
int signedOf (int f, uint8_t v) { return AislingProcessor::FIELDS[f].lo < 0 ? (int) (int8_t) v : (int) v; }
}

juce::AudioProcessorValueTreeState::ParameterLayout AislingProcessor::layout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout out;
    const StringArray shapes { "Saw", "Pulse", "Sine", "Organ" };
    for (int l = 0; l < NL; ++l)
    {
        auto group = std::make_unique<AudioProcessorParameterGroup> (LAYER_ID[l], LAYER_NAME[l], " ");
        group->addChild (std::make_unique<AudioParameterBool> (ParameterID { onId (l), 1 }, String (LAYER_NAME[l]) + " plays its Patch", false));
        for (int f = 0; f < NF; ++f)
        {
            const auto& fd = FIELDS[f];
            const String name = String (LAYER_NAME[l]) + " " + fd.name;
            const int def = signedOf (f, plain[l][f]);
            if (fd.shape)
                group->addChild (std::make_unique<AudioParameterChoice> (ParameterID { pid (l, f), 1 }, name, shapes, jlimit (0, 3, def)));
            else
                group->addChild (std::make_unique<AudioParameterInt> (ParameterID { pid (l, f), 1 }, name, fd.lo, fd.hi, jlimit (fd.lo, fd.hi, def)));
        }
        out.add (std::move (group));
    }
    out.add (std::make_unique<AudioParameterChoice> (ParameterID { "solo", 1 }, "Solo", StringArray { "All", "Drone", "Sequence", "Lead", "Sequence 2" }, 0));
    out.add (std::make_unique<AudioParameterChoice> (ParameterID { "midi", 1 }, "Notes play", StringArray { "Lead", "Sequence", "Sequence 2" }, 0));
    out.add (std::make_unique<AudioParameterBool> (ParameterID { "own", 1 }, "The piece's own Sequence notes", true));
    out.add (std::make_unique<AudioParameterBool> (ParameterID { "run", 1 }, "Play while the host is stopped", false));
    out.add (std::make_unique<AudioParameterBool> (ParameterID { "skip", 1 }, "Skip the intros", true));
    return out;
}

AislingProcessor::AislingProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AISLING", (std::call_once (once, [] { ag_init(); ag_defaults (plain); }), layout()))
{
    AislingProcessor* none = nullptr;
    theOwner.compare_exchange_strong (none, this);
    for (int l = 0; l < NL; ++l)
    {
        rawOn[l] = apvts.getRawParameterValue (onId (l));
        for (int f = 0; f < NF; ++f)
            raw[l][f] = apvts.getRawParameterValue (pid (l, f));
    }
    rawSolo = apvts.getRawParameterValue ("solo");
    rawMidi = apvts.getRawParameterValue ("midi");
    rawOwnNotes = apvts.getRawParameterValue ("own");
    rawRun = apvts.getRawParameterValue ("run");
    rawSkip = apvts.getRawParameterValue ("skip");
    inL.resize (8192);
    inR.resize (8192);
}

AislingProcessor::~AislingProcessor()
{
    AislingProcessor* me = this;
    theOwner.compare_exchange_strong (me, nullptr);
}

bool AislingProcessor::owner() const { return theOwner.load() == this; }

void AislingProcessor::fm1Send (bool on)
{
    if (fm1 == nullptr)
    {
        if (! on)
            return;
        fm1 = std::make_unique<Fm1Link> ([this] (uint8_t pt[NL][NF], bool lit[NL]) {
            patchBytes (pt);
            for (int l = 0; l < NL; ++l)
                lit[l] = rawOn[l]->load() > 0.5f;
            return Fm1Link::Where { packLen ? pieceIx.load() : -1, packLen ? packName() : juce::String() };
        });
    }
    fm1->setOn (on);
}

bool AislingProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void AislingProcessor::prepareToPlay (double sampleRate, int)
{
    ratio = (double) ENGINE_RATE / sampleRate;
    rsL.reset();
    rsR.reset();
    have = 0;
    wasPlaying = false;
    setLatencySamples (std::abs (ratio - 1.0) < 1e-9 ? 0 : (int) std::lround ((double) rsL.getBaseLatency() / ratio));
}

void AislingProcessor::patchBytes (uint8_t out[NL][NF]) const
{
    for (int l = 0; l < NL; ++l)
        for (int f = 0; f < NF; ++f)
            out[l][f] = (uint8_t) (int8_t) juce::jlimit (FIELDS[f].lo, FIELDS[f].hi, juce::roundToInt (raw[l][f]->load()));
}

// n engine frames, the host's clock in them: a tick sent on the frame it falls on (the engine takes its queue at
// each render's start, so a render stops at the tick)
void AislingProcessor::pull (float* l, float* r, int n)
{
    while (n > 0)
    {
        int k = juce::jmin (n, 256);
        if (extClock)
        {
            const double toTick = ((double) nextTickIx / 24.0 - engPpq) / beatsPerFrame;
            if (toTick < 0.5)
            {
                ag_clock (0);
                ++nextTickIx;
                continue;
            }
            k = juce::jmin (k, (int) std::ceil (toTick));
        }
        ag_render (l, r, k);
        l += k, r += k, n -= k;
        if (extClock)
            engPpq += k * beatsPerFrame;
    }
}

void AislingProcessor::produce (float* l, float* r, int n)
{
    if (std::abs (ratio - 1.0) < 1e-9)
    {
        pull (l, r, n);
        return;
    }
    while (n > 0)
    {
        const int k = juce::jmin (n, 512), need = (int) std::ceil (k * ratio) + 8;
        if (have < need)
        {
            pull (inL.data() + have, inR.data() + have, need - have);
            have = need;
        }
        const int used = rsL.process (ratio, inL.data(), l, k);
        rsR.process (ratio, inR.data(), r, k);
        have = juce::jmax (0, have - used);
        std::memmove (inL.data(), inL.data() + used, sizeof (float) * (size_t) have);
        std::memmove (inR.data(), inR.data() + used, sizeof (float) * (size_t) have);
        l += k, r += k, n -= k;
    }
}

void AislingProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    // The engine is one a process. It goes to the AISLING that is rendering: one a host keeps but no longer runs (a
    // removed device held for undo) gives it up after half a second, and a second one beside a running first stays silent.
    const juce::uint32 nowMs = juce::Time::getMillisecondCounter();
    AislingProcessor* was = theOwner.load();
    if (was != this && (was == nullptr || nowMs - ownerSeen.load() > 500u) && theOwner.compare_exchange_strong (was, this))
    {
        loadPending = true, sentValid = false, muteSent = handSent = skipSent = -1, tempoSent = 0;
        wasPlaying = freeOn = extClock = false;
        for (int& v : voiceSent)
            v = -1;
    }
    if (theOwner.load() == this)
        ownerSeen = nowMs;
    if (theOwner.load() != this || buffer.getNumChannels() < 2)
        return;

    const int skip = rawSkip->load() > 0.5f;
    if (skip != skipSent)
        loadPending = true, skipSent = skip;
    if (loadPending.exchange (false))
    {
        juce::SpinLock::ScopedTryLockType lock (packLock);
        if (lock.isLocked())
        {
            aLen = packLen;
            std::memcpy (aPack, pack, sizeof pack);
            ag_load (aLen ? aPack : nullptr, aLen, pieceIx.load(), skip);
            sentValid = false;
            for (auto& v : voiceSent)
                v = -1;
            tempoSent = 0;
            wasPlaying = false, freeOn = false;     // (from its top, in the host's time or its own)
        }
        else
            loadPending = true;
    }

    uint8_t now[NL][NF];
    patchBytes (now);
    for (int l = 0; l < NL; ++l)
    {
        if (! sentValid || std::memcmp (now[l], sent[l], NF) != 0)
        {
            ag_patch (l, now[l]);
            std::memcpy (sent[l], now[l], NF);
        }
        const int on = rawOn[l]->load() > 0.5f;
        if (on != voiceSent[l])
            ag_voice (l, on), voiceSent[l] = on;
    }
    sentValid = true;

    static const int SOLO[5] = { 0, 62, 61, 59, 39 };   // (all but the layer: the engine's mute bits)
    const int mute = SOLO[juce::jlimit (0, 4, juce::roundToInt (rawSolo->load()))];
    if (mute != muteSent)
        ag_mute ((uint32_t) mute), muteSent = mute;
    static const int MIDI_LAYER[3] = { 2, 1, 3 };
    const int layer = MIDI_LAYER[juce::jlimit (0, 2, juce::roundToInt (rawMidi->load()))];
    const int hand = rawOwnNotes->load() > 0.5f ? 0 : layer == 1 ? 1 : layer == 3 ? 2 : 0;
    if (hand != handSent)
        ag_hand ((uint32_t) hand), handSent = hand;

    bool playing = false;
    double ppq = 0.0, bpm = 120.0;
    if (auto* head = getPlayHead())
        if (auto pos = head->getPosition())
        {
            playing = pos->getIsPlaying() && pos->getPpqPosition().hasValue();
            ppq = pos->getPpqPosition().orFallback (0.0);
            bpm = pos->getBpm().orFallback (120.0);
        }
    beatsPerFrame = bpm / (60.0 * (double) ENGINE_RATE);
    if (playing)
    {
        const int ibpm = juce::roundToInt (bpm);
        const double p = ppq + have * beatsPerFrame;    // (the engine is ahead by what the resampler holds)
        if (ibpm != tempoSent)
            ag_tempo (ibpm), tempoSent = ibpm;
        if (! wasPlaying)
        {
            ag_clock (1);                               // from the top, its first sixteenth on the host's next
            nextTickIx = (juce::int64) std::ceil (p * 4.0 - 1e-6) * 6;
        }
        else if (std::abs (p - engPpq) > 0.02)          // (a loop or a jump: on in time, the sixteenths kept)
        {
            juce::int64 k = (juce::int64) std::ceil (p * 24.0 - 1e-6);
            while (((k - nextTickIx) % 6 + 6) % 6 != 0)
                ++k;
            nextTickIx = k;
        }
        engPpq = p;
        extClock = true, freeOn = false;
    }
    else
    {
        if (wasPlaying)
            ag_clock (3);
        extClock = false;
        const bool run = rawRun->load() > 0.5f;
        if (run != freeOn)
        {
            if (run)
                ag_tempo (ag_piece_tempo()), tempoSent = 0;
            ag_play (run);
            freeOn = run;
        }
    }
    wasPlaying = playing;

    float* l = buffer.getWritePointer (0);
    float* r = buffer.getWritePointer (1);
    int at = 0;
    keyMidi.clear();                                    // the panel's keys: at the block's start, on the page's layer
    keys.processNextMidiBuffer (keyMidi, 0, n, true);
    const int kl = keysLayer.load();
    for (const auto meta : keyMidi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn() && kl >= 1 && kl <= 3)
            ag_note (kl, m.getNoteNumber(), juce::jlimit (1, 127, (int) m.getVelocity()));
        else if (m.isNoteOff())                         // (only the Lead holds a note; letting go one it hasn't is nothing)
            ag_note (2, m.getNoteNumber(), 0);
    }
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const int t = juce::jlimit (at, n, meta.samplePosition);
        if (! m.isNoteOnOrOff())
            continue;
        if (t > at)
            produce (l + at, r + at, t - at), at = t;
        if (m.isNoteOn())
            ag_note (layer, m.getNoteNumber(), juce::jlimit (1, 127, (int) m.getVelocity()));
        else if (layer == 2)
            ag_note (2, m.getNoteNumber(), 0);
    }
    if (n > at)
        produce (l + at, r + at, n - at);
    midi.clear();
}

juce::String AislingProcessor::setPackBytes (const uint8_t* b, int len)
{
    static const char* const WHY[] = { "", "This isn't an AISLING sound pack.", "This pack needs a newer AISLING than this plugin.",
                                       "This pack is damaged: it doesn't read through." };
    if (len <= 0 || len > AG_PACK_MAX)
        return WHY[1];
    if (const int why = ag_check (b, len))
        return WHY[juce::jlimit (1, 3, why)];
    const juce::SpinLock::ScopedLockType lock (packLock);
    std::memcpy (pack, b, (size_t) len);
    packLen = len;
    return {};
}

juce::String AislingProcessor::loadPack (const juce::File& f)
{
    juce::MemoryBlock mb;
    if (! f.existsAsFile() || f.getSize() > AG_PACK_MAX || ! f.loadFileAsData (mb))
        return "This isn't an AISLING sound pack.";
    const auto why = setPackBytes ((const uint8_t*) mb.getData(), (int) mb.getSize());
    if (why.isNotEmpty())
        return why;
    packFile = f;
    heldFor = 0;
    setPiece (0, true);
    return {};
}

void AislingProcessor::useOwn()
{
    {
        const juce::SpinLock::ScopedLockType lock (packLock);
        packLen = 0;
    }
    packFile = juce::File();
    heldFor = 0;
    setPiece (0, true);
}

void AislingProcessor::setPiece (int i, bool take)
{
    if (take && heldFor == 1)                           // the piece being left keeps what was made on it, until
    {                                                   // another pack is loaded or the plugin closes
        auto& h = held[pieceIx.load()];
        patchBytes (h.pt);
        h.mask = 0;
        for (int l = 0; l < NL; ++l)
            if (auto* p = apvts.getParameter (onId (l)))
                h.mask |= (p->getValue() > 0.5f ? 1 : 0) << l;
        h.has = true;
    }
    pieceIx = juce::jlimit (0, 7, i);
    if (take)
    {
        if (heldFor != 1)                               // (a pack just loaded, or AISLING's own: nothing kept)
        {
            for (auto& h : held)
                h.has = false;
            heldFor = 1;
        }
        uint8_t pt[NL][NF];
        int mask = ag_peek (packLen ? pack : nullptr, packLen, pieceIx.load(), pt);
        if (const auto& h = held[pieceIx.load()]; h.has)
            std::memcpy (pt, h.pt, sizeof pt), mask = h.mask;
        for (int l = 0; l < NL; ++l)
        {
            if (auto* p = apvts.getParameter (onId (l)))
                p->setValueNotifyingHost ((mask >> l & 1) ? 1.0f : 0.0f);
            for (int f = 0; f < NF; ++f)
                if (auto* p = apvts.getParameter (pid (l, f)))
                    p->setValueNotifyingHost (p->convertTo0to1 ((float) signedOf (f, pt[l][f])));
        }
    }
    loadPending = true;
}

juce::String AislingProcessor::packName() const
{
    if (! packLen)
        return "AISLING's own pieces";
    char s[24];
    ag_pack_name (pack, s, (int) sizeof s);
    return s;
}

juce::StringArray AislingProcessor::pieceNames() const
{
    juce::StringArray out;
    for (int i = 0; i < 8; ++i)
    {
        char s[20];
        ag_piece_name (packLen ? pack : nullptr, packLen, i, s, (int) sizeof s);
        out.add (s);
    }
    return out;
}

juce::String AislingProcessor::savePack (const juce::File& f)
{
    if (! packLen)
        return "Load a pack first: AISLING's own pieces live in the firmware, not in a pack.";
    uint8_t pt[NL][NF], out[AG_PACK_MAX + 1];
    int mask = 0;
    patchBytes (pt);
    for (int l = 0; l < NL; ++l)
        mask |= (rawOn[l]->load() > 0.5f ? 1 : 0) << l;
    const int n = ag_save (pack, packLen, pieceIx.load(), mask, pt, out);
    if (n == -1)
        return "With these patches the pack is bigger than the FM-1 has room for (3,840 bytes).";
    if (n == -3)
        return "The pack can't be stored with its pieces in this order (the FM-1's update mark): change one setting by a step and save again.";
    if (n < 0)
        return "The pack doesn't read through: nothing was saved.";
    if (! f.replaceWithData (out, (size_t) n))
        return "The file couldn't be written.";
    setPackBytes (out, n);
    packFile = f;
    return {};
}

juce::String AislingProcessor::patchJson() const
{
    static const char* const SHAPE[4] = { "saw", "pulse", "sine", "organ" };
    uint8_t pt[NL][NF];
    patchBytes (pt);
    juce::StringArray layers;
    for (int l = 0; l < NL; ++l)
    {
        if (rawOn[l]->load() <= 0.5f)
            continue;
        juce::StringArray fields;
        for (int f = 0; f < NF; ++f)
        {
            const int v = signedOf (f, pt[l][f]);
            fields.add ("\"" + juce::String (FIELDS[f].id) + "\": " + (FIELDS[f].shape ? "\"" + juce::String (SHAPE[v & 3]) + "\"" : juce::String (v)));
        }
        layers.add ("  \"" + juce::String (LAYER_ID[l]) + "\": {" + fields.joinIntoString (", ") + "}");
    }
    return "\"patch\": {\n" + layers.joinIntoString (",\n") + "\n}";
}

void AislingProcessor::getStateInformation (juce::MemoryBlock& out)
{
    auto state = apvts.copyState();
    state.setProperty ("piece", pieceIx.load(), nullptr);
    state.setProperty ("pack", packLen ? juce::Base64::toBase64 (pack, (size_t) packLen) : juce::String(), nullptr);
    state.setProperty ("packFile", packFile.getFullPathName(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, out);
}

void AislingProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr)
        return;
    const auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid())
        return;
    juce::MemoryOutputStream bytes;
    const auto b64 = state.getProperty ("pack").toString();
    if (b64.isNotEmpty() && juce::Base64::convertFromBase64 (bytes, b64) && setPackBytes ((const uint8_t*) bytes.getData(), (int) bytes.getDataSize()).isEmpty())
        packFile = juce::File (state.getProperty ("packFile").toString());
    else
    {
        const juce::SpinLock::ScopedLockType lock (packLock);
        packLen = 0;
    }
    apvts.replaceState (state);                         // (the patches as they were left, over the pack's)
    setPiece ((int) state.getProperty ("piece", 0), false);
}

juce::AudioProcessorEditor* AislingProcessor::createEditor() { return new AislingEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AislingProcessor(); }
