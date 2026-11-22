// SPDX-License-Identifier: GPL-3.0-only
// The panel, in the site's manner (web/brand/site.css): vellum, iron-gall ink, the card's green, rust for what is on;
// the bit-art wall down the margins, one bar, a hairline box with a header strip, lowercase mono controls, square corners.
#pragma once

#include "PluginProcessor.h"

class AislingLook : public juce::LookAndFeel_V4
{
public:
    AislingLook();
    juce::Font mono (float h) const;
    juce::Font display (float h, float track = 0.06f) const;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return mono (12.5f); }
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String&, juce::Point<int>, juce::Rectangle<int>) override;
    void drawTooltip (juce::Graphics&, const juce::String&, int, int) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return mono (12.5f); }
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override { return mono (12.5f); }
    void drawPopupMenuBackground (juce::Graphics&, int, int) override;
    juce::Font getLabelFont (juce::Label& l) override { return l.getProperties().contains ("display") ? display (18.0f, 0.04f) : mono (12.5f); }
    void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
    int getTabButtonBestWidth (juce::TabBarButton&, int) override;
    void drawTabButton (juce::TabBarButton&, juce::Graphics&, bool, bool) override;
    void drawTabAreaBehindFrontButton (juce::TabbedButtonBar&, juce::Graphics&, int, int) override {}
    void drawTabbedButtonBarBackground (juce::TabbedButtonBar&, juce::Graphics&) override {}

private:
    juce::Typeface::Ptr monoFace, displayFace;
};

class AislingEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AislingEditor (AislingProcessor&);
    ~AislingEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;
    struct LayerPage : juce::Component
    {
        LayerPage (AislingProcessor&, int layer);
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;   // a control touched: the layer plays its patch
        juce::ToggleButton on;
        juce::OwnedArray<juce::Label> labels;
        juce::OwnedArray<juce::Slider> sliders;
        juce::OwnedArray<juce::ComboBox> combos;
        std::vector<juce::Component*> controls;
        std::unique_ptr<APVTS::ButtonAttachment> onAtt;
        std::vector<std::unique_ptr<APVTS::SliderAttachment>> sAtt;
        std::vector<std::unique_ptr<APVTS::ComboBoxAttachment>> cAtt;
    };
    void refresh();
    void timerCallback() override;
    bool second = false;
    void say (const juce::String&);
    juce::Rectangle<int> bench() const;

    AislingProcessor& proc;
    AislingLook look;
    juce::Image wall;
    juce::TextButton load { "load a pack..." }, own { "aisling's own" }, save { "save into the pack..." }, copy { "copy as json" };
    juce::Label packLabel, status, soloL { {}, "solo" }, midiL { {}, "notes play" };
    juce::ComboBox piece, solo, midi;
    juce::ToggleButton ownNotes { "the piece plays its own sequence notes" }, run { "play while the host is stopped" }, skip { "skip the intros" };
    juce::TextButton fm1 { "send to the fm-1" };      // (a button that stays down: rust while it sends)
    juce::Label fm1L;
    juce::TooltipWindow tips { this, 500 };
    struct Tabs : juce::TabbedComponent
    {
        Tabs() : juce::TabbedComponent (juce::TabbedButtonBar::TabsAtTop) {}
        void currentTabChanged (int i, const juce::String&) override { if (onTab) onTab (i); }
        std::function<void (int)> onTab;
    } tabs;
    juce::MidiKeyboardComponent keys;
    juce::Label keysL;
    std::unique_ptr<APVTS::ComboBoxAttachment> soloAtt, midiAtt;
    std::unique_ptr<APVTS::ButtonAttachment> ownAtt, runAtt, skipAtt;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AislingEditor)
};
