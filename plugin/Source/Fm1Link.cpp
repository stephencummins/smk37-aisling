// SPDX-License-Identifier: GPL-3.0-only
#include "Fm1Link.h"

namespace
{
    constexpr int H0 = 0x7D, H1 = 0x41, H2 = 0x4C, VER = 1;
    constexpr int HELLO = 1, BYE = 2, BEAT = 3, PATCH = 26, R_HELLO = 65, R_ACK = 66;
    constexpr int ST_OK = 0, ST_BAD = 1, ST_NONE = 5;
    constexpr int RESEND_MS = 200, TRIES = 25, LOST_MS = 4000;
}

Fm1Link::Fm1Link (Want w) : want (std::move (w)) {}

Fm1Link::~Fm1Link()                                 // (no time to wait for answers: each said once, apart)
{
    stopTimer();
    if (out != nullptr && (phase == Phase::up || phase == Phase::ending))
    {
        for (int l = 0; l < NL; ++l)
            if (there[l] == 1)
            {
                ask (l, false, nullptr);
                juce::Thread::sleep (40);
            }
        frame ({ BYE });
        juce::Thread::sleep (40);
        frame ({ BYE });
    }
    drop();
}

void Fm1Link::setOn (bool on)
{
    if (on == isOn())
        return;
    if (on)
    {
        phase = Phase::seeking, tSeek = 0, said = "Looking for an FM-1...";
        startTimer (40);
    }
    else if (phase == Phase::up)
        phase = Phase::ending, pending = false, said = {};
    else
    {
        drop();
        phase = Phase::off, said = {};
        stopTimer();
    }
}

bool Fm1Link::find()                                // by name: nothing goes to any other device
{
    juce::String outId, inId;
    for (auto& d : juce::MidiOutput::getAvailableDevices())
        if (d.name.containsIgnoreCase ("FM-1"))
        {
            outId = d.identifier;
            break;
        }
    for (auto& d : juce::MidiInput::getAvailableDevices())
        if (d.name.containsIgnoreCase ("FM-1"))
        {
            inId = d.identifier;
            break;
        }
    if (outId.isEmpty() || inId.isEmpty())
        return false;
    out = juce::MidiOutput::openDevice (outId);
    in = juce::MidiInput::openDevice (inId, this);
    if (out == nullptr || in == nullptr)
    {
        drop();
        return false;
    }
    in->start();
    return true;
}

void Fm1Link::drop()
{
    if (in != nullptr)
        in->stop();
    in.reset();
    out.reset();
    pending = false;
    for (auto& t : there)
        t = -1;
}

void Fm1Link::frame (std::initializer_list<int> head, const uint8_t* more, int n)
{
    uint8_t b[48] = { H0, H1, H2 };
    int k = 3;
    for (int v : head)
        b[k++] = (uint8_t) v;
    for (int i = 0; i < n; ++i)
        b[k++] = more[i];
    if (out != nullptr)
        out->sendMessageNow (juce::MidiMessage::createSysExMessage (b, k));
}

// PATCH: the layer (and 8: its own back), then the sixteen as two sevens each, low first
void Fm1Link::ask (int layer, bool on, const uint8_t* vals)
{
    seq = seq % 127 + 1;
    pFrameN = 0;
    pFrame[pFrameN++] = (uint8_t) (layer | (on ? 0 : 8));
    for (int i = 0; i < NF; ++i)
    {
        const int v = on ? (int) (int8_t) vals[i] & 0x3FFF : 0;
        pFrame[pFrameN++] = (uint8_t) (v & 0x7F);
        pFrame[pFrameN++] = (uint8_t) (v >> 7 & 0x7F);
    }
    pLayer = layer, pOn = on, pSeq = seq, pTries = 1, pending = true;
    if (on)
        std::memcpy (pVals, vals, NF);
    ack = -1;
    tAsk = juce::Time::getMillisecondCounter();
    frame ({ PATCH, pSeq }, pFrame, pFrameN);
}

bool Fm1Link::next (const uint8_t pt[NL][NF], const bool on[NL])
{
    for (int l = 0; l < NL; ++l)
    {
        // (said again each second while it stands: the FM-1 loads a piece's own patch whenever it takes the piece
        // up again, and a quick turn away and back is over before its state says so)
        if (on[l] && (there[l] != 1 || std::memcmp (pt[l], sent[l], NF) != 0
                      || juce::Time::getMillisecondCounter() - tSent[l] >= 1000))
        {
            ask (l, true, pt[l]);
            return true;
        }
        if (! on[l] && there[l] == 1)
        {
            ask (l, false, nullptr);
            return true;
        }
    }
    return false;
}

void Fm1Link::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounter();
    if (phase == Phase::seeking)
    {
        if (tSeek != 0 && now - tSeek < 1000)
            return;
        tSeek = now;
        if (! find())
        {
            said = "No FM-1 on USB.";
            return;
        }
        helloSeen = false, heard = now, tHello = 0, phase = Phase::hello;
    }
    if (phase == Phase::hello)
    {
        if (helloSeen.load())
        {
            phase = Phase::up, tBeat = now, tHold = 0, said = "The FM-1 plays these patches.";
            return;
        }
        if (now - heard.load() > LOST_MS)
        {
            drop();
            phase = Phase::seeking, said = "No answer: is the FM-1 on AISLING?";
            return;
        }
        if (now - tHello >= 500)
            tHello = now, frame ({ HELLO, VER });
        return;
    }
    if (phase != Phase::up && phase != Phase::ending)
        return;
    if (now - heard.load() > LOST_MS)
    {
        drop();
        if (phase == Phase::ending)
            phase = Phase::off, said = {}, stopTimer();
        else
            phase = Phase::seeking, said = "The FM-1 went away.";
        return;
    }
    if (pending)
    {
        const int a = ack.load();
        if (a >= 0 && (a >> 8) == pSeq)
        {
            const int st = a & 0xFF;
            pending = false;
            if (st == ST_OK)
            {
                there[pLayer] = pOn ? 1 : 0;
                if (pOn)
                    std::memcpy (sent[pLayer], pVals, NF), tSent[pLayer] = now;
                if (phase == Phase::up)
                    said = "The FM-1 plays these patches.";
            }
            else
            {
                tHold = now;                            // (asked again in a second: the bank may be turned to by then)
                if (! pOn)
                    there[pLayer] = 0;                  // (nothing of ours is there to take back)
                if (phase == Phase::up)
                    said = st == ST_NONE ? "On the FM-1, turn PRESETS to a pack piece."
                         : st == ST_BAD ? "The FM-1 needs AISLING 1.6 for this."
                                        : "The FM-1 refused the patch.";
            }
        }
        else if (now - tAsk >= (juce::uint32) RESEND_MS)
        {
            if (++pTries > TRIES)
                pending = false, tHold = now;
            else
                tAsk = now, frame ({ PATCH, pSeq }, pFrame, pFrameN);
        }
        return;
    }
    if (now - tBeat >= 1000)
    {
        tBeat = now, frame ({ BEAT });
        return;                                         // (never a request right behind it: it would be lost)
    }
    if (tHold != 0 && now - tHold < 1000)
        return;
    tHold = 0;
    if (phase == Phase::ending)
    {
        static const uint8_t none[NL][NF] {};
        static const bool off[NL] {};
        if (next (none, off))
            return;
        frame ({ BYE });
        juce::Thread::sleep (40);
        frame ({ BYE });
        drop();
        phase = Phase::off, said = {};
        stopTimer();
        return;
    }
    uint8_t pt[NL][NF];
    bool on[NL];
    const auto here = want (pt, on);
    const int mine = here.piece, its = fmPiece.load();
    juce::String has;
    {
        const juce::SpinLock::ScopedLockType lock (nameLock);
        has = fmPack;
    }
    wrong = mine >= 0 && has.isNotEmpty() && has != here.pack.substring (0, 31)
              ? "The FM-1 has the pack \"" + has + "\", not this one: nothing sent." : juce::String();
    other = mine >= 0 && its >= 0 && mine != its;       // another piece there: it keeps (gets back) its own voices
    if (other || wrong.isNotEmpty())
        for (auto& o : on)
            o = false;
    next (pt, on);
}

void Fm1Link::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m)
{
    if (! m.isSysEx())
        return;
    const auto* d = m.getSysExData();
    const int n = m.getSysExDataSize();
    if (n < 5 || d[0] != H0 || d[1] != H1 || d[2] != H2)
        return;
    heard = juce::Time::getMillisecondCounter();
    if (d[3] == R_HELLO)
    {
        const auto* a = d + 5;                          // (after the version: level, bank, the pack's version, name)
        const int na = n - 5, at = na > 9 ? 10 + a[9] : 0;
        char nm[32] {};
        if (at && na >= at + 5 && na >= at + 5 + a[at + 4])
            std::memcpy (nm, a + at + 5, (size_t) juce::jmin (31, (int) a[at + 4]));
        {
            const juce::SpinLock::ScopedLockType lock (nameLock);
            std::memcpy (fmPack, nm, sizeof fmPack);
        }
        helloSeen = true;
    }
    else if (d[3] == 67 && n >= 8)                      // STATE: view, flags, the piece
        fmPiece = d[7];
    else if (d[3] == R_ACK && n >= 6)
        ack = d[4] << 8 | d[5];
}
