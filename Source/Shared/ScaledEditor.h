#pragma once

// =============================================================================
//  MixPilot — fenêtre du plugin redimensionnable (coin en bas à droite ou bord
//  de la fenêtre de l'hôte). L'interface garde ses proportions et la taille
//  choisie est mémorisée pour les prochaines ouvertures.
// =============================================================================

#include <juce_audio_processors/juce_audio_processors.h>

namespace mp::ui
{
    // Préférences de l'utilisateur, communes à toutes les instances
    inline std::unique_ptr<juce::PropertiesFile> openSettings()
    {
        juce::PropertiesFile::Options o;
        o.applicationName     = "MixPilot";
        o.filenameSuffix      = "settings";
        o.folderName          = "Rems Plugins";
        o.osxLibrarySubFolder = "Application Support";
        o.millisecondsBeforeSaving = 400;
        return std::make_unique<juce::PropertiesFile> (o);
    }

    template <class View, class Processor>
    class ScaledEditor : public juce::AudioProcessorEditor
    {
    public:
        ScaledEditor (Processor& p, const char* settingsKey)
            : juce::AudioProcessorEditor (&p), view (p), key (settingsKey), settings (openSettings())
        {
            baseW = view.getWidth();
            baseH = view.getHeight();
            addAndMakeVisible (view);

            const double scale = juce::jlimit (kMin, kMax, settings->getDoubleValue (key, 1.0));
            setResizable (true, true);
            setResizeLimits (juce::roundToInt (baseW * kMin), juce::roundToInt (baseH * kMin),
                             juce::roundToInt (baseW * kMax), juce::roundToInt (baseH * kMax));
            getConstrainer()->setFixedAspectRatio ((double) baseW / (double) baseH);
            setSize (juce::roundToInt (baseW * scale), juce::roundToInt (baseH * scale));
            ready = true;
        }

        ~ScaledEditor() override { settings->saveIfNeeded(); }

        View& getView() { return view; }

        void resized() override
        {
            const float scale = (float) getWidth() / (float) baseW;
            view.setTransform (juce::AffineTransform::scale (scale));
            view.setBounds (0, 0, baseW, baseH);
            if (ready && std::abs (scale - lastSaved) > 0.001f)
            {
                lastSaved = scale;
                settings->setValue (key, (double) scale);
            }
        }

    private:
        static constexpr double kMin = 0.6, kMax = 2.0;
        View view;
        juce::String key;
        std::unique_ptr<juce::PropertiesFile> settings;
        int baseW = 1000, baseH = 700;
        float lastSaved = 0.0f;
        bool ready = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScaledEditor)
    };
}
