#pragma once

#include "MasterProcessor.h"
#include "../Shared/UI.h"

// Zone passive qui affiche une bulle d'aide au survol
struct HelpZone : public juce::Component, public juce::SettableTooltipClient
{
    HelpZone() { setInterceptsMouseClicks (true, false); }
};

// Forme d'onde de la référence (clic = se placer)
class RefWaveform : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit RefWaveform (MasterProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent& e) override { seek (e); }
    void mouseDrag (const juce::MouseEvent& e) override { seek (e); }
    bool dragHover = false;
private:
    void seek (const juce::MouseEvent& e) { proc.seekReference ((double) e.x / juce::jmax (1, getWidth())); }
    MasterProcessor& proc;
};

class MasterEditor : public juce::AudioProcessorEditor,
                     public juce::FileDragAndDropTarget,
                     private juce::Timer
{
public:
    explicit MasterEditor (MasterProcessor&);
    ~MasterEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { waveform.dragHover = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override              { waveform.dragHover = false; repaint(); }

private:
    void timerCallback() override;
    void addKnob (juce::Slider&, const char* paramId, const juce::String& tip, juce::Colour = mp::ui::col::accent);
    void drawLoudnessPanel (juce::Graphics&, juce::Rectangle<int>);
    void drawTrackPanel (juce::Graphics&, juce::Rectangle<int>);
    void drawReferencePanel (juce::Graphics&, juce::Rectangle<int>);
    void drawCorrectionPanel (juce::Graphics&, juce::Rectangle<int>);
    void drawKnobTitle (juce::Graphics&, const juce::Slider&, const juce::String&);
    void chooseReferenceFile();

    MasterProcessor& proc;
    mp::ui::StudioLook look;
    juce::TooltipWindow tooltipWindow { this, 450 };

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    HelpZone statusZone, loudZone, tracksZone, refInfoZone, corrZone;

    juce::ComboBox genreBox, outputBox, targetBox;
    juce::Slider amountSlider, shareSlider;
    juce::TextButton assistantBtn { "ASSISTANT" }, compareBtn { "A/B" }, bypassBtn { "BYPASS" }, resetLufsBtn { "RESET" }, helpBtn;
    juce::TextButton eqOn { "EQ" }, dynOn { "DYNAMICS" }, imageOn { "IMAGE" }, maxOn { "MAXIMIZER" };
    juce::TextButton loadRefBtn, clearRefBtn { "X" }, listenRefBtn, matchRefBtn, refLevelBtn;

    juce::Slider eqSliders[mp::kBands + 1];
    juce::Slider glue, warmth, width, monoBass, push, clipKnob, ceiling, release;

    mp::ui::SpectrumView spectrum;
    RefWaveform waveform { proc };
    mp::ui::TipsBar tipsBar;
    std::unique_ptr<mp::ui::HelpOverlay> help;
    std::unique_ptr<mp::ui::ReviewOverlay> review;
    juce::TextButton reviewBtn;
    int reviewTick = 0;
    std::unique_ptr<juce::FileChooser> chooser;

    std::vector<std::unique_ptr<SA>> sliderAtt;
    std::vector<std::unique_ptr<BA>> buttonAtt;
    std::unique_ptr<CA> genreAtt, outputAtt, targetAtt;

    juce::Rectangle<int> statusArea, tipsArea, loudArea, tracksArea, eqArea, dynArea, imgArea, maxArea, refArea, corrArea;
    float shortTerm = -70, integrated = -70, truePeakDb = -100, glueGr = 0, limGr = 0;
    float tpHold = -100;
    std::vector<MasterProcessor::TrackInfo> tracks;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterEditor)
};
