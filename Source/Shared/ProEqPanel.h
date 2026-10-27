#pragma once

// =============================================================================
//  MixPilot — panneau de la bande sélectionnée de l'EQ libre
//  (posé en bas du spectre, comme une fenêtre flottante)
// =============================================================================

#include <juce_audio_processors/juce_audio_processors.h>
#include "UI.h"
#include "ProEQ.h"

namespace mp::ui
{
    class BandPanel : public juce::Component, private juce::Timer
    {
    public:
        BandPanel (juce::AudioProcessorValueTreeState& s, ProEqModel& m) : apvts (s), model (m)
        {
            power.setButtonText ("ON");   power.setClickingTogglesState (true);
            dyn.setButtonText ("DYN EQ");    dyn.setClickingTogglesState (true);
            prev.setButtonText ("<");     next.setButtonText (">");
            solo.setButtonText ("SOLO");  del.setButtonText ("DEL");  close.setButtonText ("X");
            power.setTooltip ("Turn this band on or off");
            dyn.setTooltip ("Dynamic EQ: the band only acts when its frequency zone goes over THRESH, up to RANGE");
            solo.setTooltip ("Listen only to the zone of this band");
            del.setTooltip ("Delete this band (or Alt-click its point)");
            prev.setTooltip ("Previous band");  next.setTooltip ("Next band");
            for (auto* b : { &power, &prev, &next, &dyn, &solo, &del, &close }) addAndMakeVisible (*b);

            prev.onClick  = [this] { step (-1); };
            next.onClick  = [this] { step (1); };
            solo.onClick  = [this] { model.setSolo (model.getSolo() == band ? -1 : band); refresh(); };
            del.onClick   = [this] { const int k = band; if (onSelect) onSelect (-1); model.deleteBand (k); };
            close.onClick = [this] { if (onSelect) onSelect (-1); };

            title.setFont (font (12.5f, true));
            note.setFont (font (11.0f));
            note.setColour (juce::Label::textColourId, col::textDim);
            note.setTooltip ("Nearest note and offset in cents");
            addAndMakeVisible (title);
            addAndMakeVisible (note);

            for (int t = 0; t < NumProTypes; ++t)    type.addItem (proTypeName (t), t + 1);
            for (int c = 0; c < NumProChannels; ++c) chan.addItem (proChannelName (c), c + 1);
            for (int sl = 0; sl < 4; ++sl)           slope.addItem (proSlopeName (sl), sl + 1);
            type.setTooltip ("Filter type");
            chan.setTooltip ("Which part of the signal the band acts on: both channels, Mid (centre), Side (edges), left or right");
            slope.setTooltip ("Steepness of the cut");
            for (auto* c : { &type, &chan, &slope }) addAndMakeVisible (*c);

            static const char* names[] = { "FREQ", "GAIN", "Q", "THRESH", "RANGE", "RATIO", "ATTACK", "RELEASE" };
            for (int i = 0; i < kKnobs; ++i)
            {
                auto& k = knobs[(size_t) i];
                k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
                k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 14);
                k.slider.setColour (juce::Slider::textBoxTextColourId, col::text);
                k.name.setText (names[i], juce::dontSendNotification);
                k.name.setFont (font (9.5f, true));
                k.name.setJustificationType (juce::Justification::centred);
                k.name.setColour (juce::Label::textColourId, col::textDim);
                addAndMakeVisible (k.slider);
                addAndMakeVisible (k.name);
            }
            knobs[3].slider.setTooltip ("Level of the band's zone above which the dynamic part starts acting");
            knobs[4].slider.setTooltip ("Maximum gain the dynamic part adds (negative = cut when it gets loud, positive = boost)");
            setVisible (false);
        }

        // Le spectre est prévenu pour sélectionner une autre bande (-1 = fermer)
        std::function<void (int)> onSelect;

        int getBand() const { return band; }

        void showBand (int k)
        {
            if (k == band && k >= 0) { refresh(); return; }
            attachments.clear();
            band = k;
            if (k < 0) { stopTimer(); setVisible (false); return; }

            using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
            using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
            using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
            static const ProField fields[] = { FFreq, FGain, FQ, FThr, FRange, FRatio, FAtt, FRel };
            for (int i = 0; i < kKnobs; ++i)
                attachments.push_back (std::make_unique<SA> (apvts, proParamId (k, fields[i]), knobs[(size_t) i].slider));
            attachments.push_back (std::make_unique<CA> (apvts, proParamId (k, FType),  type));
            attachments.push_back (std::make_unique<CA> (apvts, proParamId (k, FCh),    chan));
            attachments.push_back (std::make_unique<CA> (apvts, proParamId (k, FSlope), slope));
            attachments.push_back (std::make_unique<BA> (apvts, proParamId (k, FActive), power));
            attachments.push_back (std::make_unique<BA> (apvts, proParamId (k, FDyn),    dyn));

            const auto c = proBandColour (k);
            for (auto& kn : knobs) kn.slider.setColour (juce::Slider::rotarySliderFillColourId, c);
            power.setColour (juce::TextButton::buttonOnColourId, c.darker (0.2f));
            dyn.setColour (juce::TextButton::buttonOnColourId, c.darker (0.2f));
            title.setColour (juce::Label::textColourId, c);
            setVisible (true);
            refresh();
            startTimerHz (15);
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            if (band < 0) return;
            const auto b = getLocalBounds().toFloat().reduced (0.5f);
            const auto c = proBandColour (band);
            g.setColour (col::bg.withAlpha (0.94f));
            g.fillRoundedRectangle (b, 7.0f);
            g.setColour (c.withAlpha (0.75f));
            g.drawRoundedRectangle (b, 7.0f, 1.2f);
            // séparation réglages / dynamique
            const float x = (float) knobs[3].name.getX() - 6.0f;
            g.setColour (col::border);
            g.drawVerticalLine ((int) x, 38.0f, b.getBottom() - 8);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (8, 6);
            auto top = r.removeFromTop (22);
            power.setBounds (top.removeFromLeft (34));  top.removeFromLeft (4);
            prev.setBounds (top.removeFromLeft (20));
            title.setBounds (top.removeFromLeft (54));
            next.setBounds (top.removeFromLeft (20));   top.removeFromLeft (4);
            close.setBounds (top.removeFromRight (22)); top.removeFromRight (4);
            del.setBounds (top.removeFromRight (40));   top.removeFromRight (4);
            solo.setBounds (top.removeFromRight (48));  top.removeFromRight (4);
            dyn.setBounds (top.removeFromRight (58));   top.removeFromRight (6);
            note.setBounds (top.removeFromLeft (66));
            const int cw = juce::jmax (40, (top.getWidth() - 8) / 3);
            type.setBounds (top.removeFromLeft (cw));   top.removeFromLeft (4);
            chan.setBounds (top.removeFromLeft (cw));   top.removeFromLeft (4);
            slope.setBounds (top.removeFromLeft (cw));

            r.removeFromTop (10);
            const int kw = (r.getWidth() - 12) / kKnobs;
            for (int i = 0; i < kKnobs; ++i)
            {
                auto a = juce::Rectangle<int> (r.getX() + i * kw + (i >= 3 ? 12 : 0), r.getY(), kw, r.getHeight());
                knobs[(size_t) i].name.setBounds (a.removeFromTop (11));
                knobs[(size_t) i].slider.setBounds (a);
            }
        }

    private:
        static constexpr int kKnobs = 8;
        struct Knob { juce::Slider slider; juce::Label name; };

        void timerCallback() override { refresh(); }

        void refresh()
        {
            if (band < 0) return;
            const auto b = model.band (band);
            if (! b.on) { if (onSelect) onSelect (-1); return; }
            title.setText ("BAND " + juce::String (band + 1), juce::dontSendNotification);
            note.setText (noteName (b.freq), juce::dontSendNotification);
            const bool gain = proHasGain (b.type), dynOn = b.dyn && gain;
            knobs[1].slider.setEnabled (gain);
            knobs[1].slider.setAlpha (gain ? 1.0f : 0.35f);
            slope.setEnabled (proIsCut (b.type));
            slope.setAlpha (proIsCut (b.type) ? 1.0f : 0.4f);
            dyn.setEnabled (gain);
            for (int i = 3; i < kKnobs; ++i)
            {
                knobs[(size_t) i].slider.setEnabled (dynOn);
                knobs[(size_t) i].slider.setAlpha (dynOn ? 1.0f : 0.35f);
            }
            const bool soloed = model.getSolo() == band;
            solo.setToggleState (soloed, juce::dontSendNotification);
            solo.setColour (juce::TextButton::buttonOnColourId, col::warn.darker (0.2f));
        }

        void step (int dir)
        {
            for (int i = 1; i <= kProBands; ++i)
            {
                const int k = ((band + dir * i) % kProBands + kProBands) % kProBands;
                if (model.band (k).on) { if (onSelect) onSelect (k); return; }
            }
        }

        juce::AudioProcessorValueTreeState& apvts;
        ProEqModel& model;
        int band = -1;

        juce::TextButton power, prev, next, dyn, solo, del, close;
        juce::Label title, note;
        juce::ComboBox type, chan, slope;
        std::array<Knob, kKnobs> knobs;
        // attachments en dernier : détruits avant les composants
        std::vector<std::shared_ptr<void>> attachments;
    };
}
