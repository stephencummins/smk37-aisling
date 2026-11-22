// SPDX-License-Identifier: GPL-3.0-only
// The plugin without a host: a made-up transport, the output to a WAV (tools check the timing), a pack saved.
//   AislingTest PACK RATE SECS OUT.wav [solo=N] [bpm=N] [loop=BEATS] [notes=LAYER] [keys=LAYER] [stopped] [save=FILE] [set=ID:V,..] [shot=FILE.png]
#include "../Source/PluginProcessor.h"

struct Head : juce::AudioPlayHead
{
    bool playing = true;
    double ppq = 0.0, bpm = 120.0;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setIsPlaying (playing);
        p.setPpqPosition (ppq);
        p.setBpm (bpm);
        return p;
    }
};

int main (int argc, char** argv)
{
    if (argc < 5)
        return 2;
    juce::ScopedJuceInitialiser_GUI gui;
    const double rate = juce::String (argv[2]).getDoubleValue(), secs = juce::String (argv[3]).getDoubleValue();
    const int block = 480;
    AislingProcessor p;
    Head head;
    double loop = 0.0;
    int notes = -1, keys = -1;
    juce::String saveTo, shot;
    if (juce::String (argv[1]) != "-")
    {
        const auto why = p.loadPack (juce::File (argv[1]));
        if (why.isNotEmpty())
        {
            std::printf ("load: %s\n", why.toRawUTF8());
            return 1;
        }
    }
    auto set = [&p] (const juce::String& id, float v) {
        if (auto* q = p.apvts.getParameter (id))
            q->setValueNotifyingHost (q->convertTo0to1 (v));
        else
            std::printf ("no parameter %s\n", id.toRawUTF8());
    };
    for (int i = 5; i < argc; ++i)
    {
        const juce::String a (argv[i]);
        if (a.startsWith ("solo=")) set ("solo", (float) a.substring (5).getIntValue());
        else if (a.startsWith ("piece=")) p.setPiece (a.substring (6).getIntValue(), true);
        else if (a.startsWith ("bpm=")) head.bpm = a.substring (4).getDoubleValue();
        else if (a.startsWith ("loop=")) loop = a.substring (5).getDoubleValue();
        else if (a.startsWith ("notes=")) { notes = a.substring (6).getIntValue(); set ("midi", (float) notes); set ("own", 0.0f); }
        else if (a == "stopped") head.playing = false;
        else if (a.startsWith ("keys=")) keys = a.substring (5).getIntValue();   // the panel's keys, on layer 1..3
        else if (a.startsWith ("save=")) saveTo = a.substring (5);
        else if (a.startsWith ("shot=")) shot = a.substring (5);
        else if (a.startsWith ("set="))
            for (const auto& kv : juce::StringArray::fromTokens (a.substring (4), ",", ""))
                set (kv.upToFirstOccurrenceOf (":", false, false), kv.fromFirstOccurrenceOf (":", false, false).getFloatValue());
    }
    p.setPlayHead (&head);
    p.setRateAndBufferSizeDetails (rate, block);
    p.prepareToPlay (rate, block);
    juce::AudioBuffer<float> buf (2, block), all (2, (int) (secs * rate));
    juce::MidiBuffer midi;
    const double perBlock = head.bpm / 60.0 * block / rate;
    int at = 0, n = 0;
    for (; at + block <= all.getNumSamples(); at += block, ++n)
    {
        midi.clear();
        if (notes >= 0 && n % 50 == 0)                  // (a note each half second at 48 kHz, let go a quarter on)
            midi.addEvent (juce::MidiMessage::noteOn (1, 60 + (n / 50) % 12, (juce::uint8) 100), 7);
        if (notes >= 0 && n % 50 == 25)
            midi.addEvent (juce::MidiMessage::noteOff (1, 60 + (n / 50) % 12), 3);
        if (keys >= 0 && n % 50 == 0)
            p.keysLayer = keys, p.keys.noteOn (1, 60 + (n / 50) % 12, 0.8f);
        if (keys >= 0 && n % 50 == 25)
            p.keys.noteOff (1, 60 + (n / 50) % 12, 0.0f);
        p.processBlock (buf, midi);
        all.copyFrom (0, at, buf, 0, 0, block);
        all.copyFrom (1, at, buf, 1, 0, block);
        if (head.playing)
        {
            head.ppq += perBlock;
            if (loop > 0.0 && head.ppq >= loop)
                head.ppq -= loop;
        }
    }
    juce::File out (argv[4]);
    out.deleteFile();
    juce::WavAudioFormat wav;
    if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (new juce::FileOutputStream (out), rate, 2, 24, {}, 0)))
        w->writeFromAudioSampleBuffer (all, 0, at);
    std::printf ("peak %.4f latency %d\n", (double) all.getMagnitude (0, at), p.getLatencySamples());
    if (saveTo.isNotEmpty())
    {
        const auto why = p.savePack (juce::File (saveTo));
        std::printf ("save: %s\n", why.isEmpty() ? "ok" : why.toRawUTF8());
        std::printf ("%s\n", p.patchJson().toRawUTF8());
    }
    if (shot.isNotEmpty())                              // (the panel as it draws, no window)
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        const auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        juce::File f (shot);
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (img, os);
    }
    return 0;
}
