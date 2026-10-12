#pragma once

#include "TrackProcessor.h"
#include "../Shared/UI.h"

struct TrackHelpZone : public juce::Component, public juce::SettableTooltipClient
{
    TrackHelpZone() { setInterceptsMouseClicks (true, false); }
};

class TrackEditor : public juce::AudioProcessorEditor,
                    private juce::Timer
{
public:
    explicit TrackEditor (TrackProcessor&);
    ~TrackEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawDetection (juce::Graphics&, juce::Rectangle<int>);
    void drawCard (juce::Graphics&, int index);

    TrackProcessor& proc;
    mp::ui::StudioLook look;
    juce::TooltipWindow tooltipWindow { this, 450 };

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    TrackHelpZone linkZone, statusZone, detectZone, cardZones[5];

    juce::ComboBox instrumentBox, genreBox;
    juce::Slider amountSlider;
    juce::TextButton listenBtn, bypassBtn { "BYPASS" }, followBtn, helpBtn;
    juce::TextButton gainOn { "GAIN" }, eqOn { "EQ" }, compOn { "COMPRESSION" }, duckOn { "ESPACE" };
    mp::ui::SpectrumView spectrum;
    mp::ui::TipsBar tipsBar;
    std::unique_ptr<mp::ui::HelpOverlay> help;

    std::unique_ptr<SA> amountAtt;
    std::unique_ptr<CA> instAtt, genreAtt;
    std::vector<std::unique_ptr<BA>> buttonAtt;

    juce::Rectangle<int> statusArea, detectArea, linkArea, cards[5];
    float outShort = -70, compGr = 0, duckGr = 0, vDuckGr = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackEditor)
};
