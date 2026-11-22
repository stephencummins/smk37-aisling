// SPDX-License-Identifier: GPL-3.0-only
// The plugin to an FM-1 on USB: the patches being made here, heard there as they change (the link's PATCH,
// firmware/src/app/link.c). Each layer that plays its Patch here plays it there, in the engine only: nothing on the
// FM-1 is stored or edited, and turning this off gives the piece its own voices back. Message thread, but the MIDI
// input's callback.
#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

class Fm1Link : private juce::Timer, private juce::MidiInputCallback
{
public:
    static constexpr int NL = 4, NF = 16;
    // (returns the piece chosen here, 0..7, or -1 when no pack is loaded, and that pack's name: the FM-1 gets a
    // patch only on that piece of that pack)
    struct Where { int piece; juce::String pack; };
    using Want = std::function<Where (uint8_t pt[NL][NF], bool on[NL])>;

    explicit Fm1Link (Want);
    ~Fm1Link() override;
    void setOn (bool);
    bool isOn() const { return phase != Phase::off && phase != Phase::ending; }
    juce::String status() const { return phase != Phase::up ? said : wrong.isNotEmpty() ? wrong : other ? juce::String ("The FM-1 is on another piece: its own voices.") : said; }

private:
    enum class Phase { off, seeking, hello, up, ending };
    void timerCallback() override;
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;
    bool find();
    void drop();
    void frame (std::initializer_list<int> head, const uint8_t* more = nullptr, int n = 0);
    void ask (int layer, bool on, const uint8_t* vals);
    bool next (const uint8_t pt[NL][NF], const bool on[NL]);

    Want want;
    std::atomic<int> fmPiece { -1 };                // (the piece the FM-1 says it is on)
    bool other = false;
    juce::String wrong;                             // (the FM-1 has another pack: said, and nothing sent)
    juce::SpinLock nameLock;
    char fmPack[32] {};                             // (the pack the FM-1 said it has, from its hello)
    Phase phase = Phase::off;
    juce::String said;
    std::unique_ptr<juce::MidiOutput> out;
    std::unique_ptr<juce::MidiInput> in;
    juce::uint32 tHello = 0, tBeat = 0, tSeek = 0, tAsk = 0, tHold = 0;

    // what the FM-1 has had: -1 nothing, 0 its own back, 1 the patch in sent
    int there[NL] { -1, -1, -1, -1 };
    uint8_t sent[NL][NF] {};
    juce::uint32 tSent[NL] {};
    bool pending = false, pOn = false;
    int pLayer = 0, pSeq = 0, pTries = 0, seq = 0;
    uint8_t pVals[NF] {}, pFrame[40] {};
    int pFrameN = 0;

    std::atomic<bool> helloSeen { false };
    std::atomic<int> ack { -1 };                        // seq << 8 | status
    std::atomic<juce::uint32> heard { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Fm1Link)
};
