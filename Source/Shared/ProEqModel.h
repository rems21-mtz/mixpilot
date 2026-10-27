#pragma once

// =============================================================================
//  MixPilot — EQ libre : bandes ajoutées à la main par-dessus l'analyse.
//  Description commune au DSP (ProEQ.h) et à l'interface (UI.h).
// =============================================================================

#include <juce_core/juce_core.h>

namespace mp
{
    constexpr int kProBands = 12;

    enum ProType    { PBell, PLowShelf, PHighShelf, PLowCut, PHighCut, PNotch, NumProTypes };
    enum ProChannel { PStereo, PMid, PSide, PLeft, PRight, NumProChannels };
    enum ProField   { FOn, FActive, FType, FFreq, FGain, FQ, FSlope, FCh, FDyn, FThr, FRange, FAtt, FRel, FRatio, NumProFields };

    inline const char* proTypeName (int t)
    {
        static const char* n[] = { "Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut", "Notch" };
        return n[juce::jlimit (0, NumProTypes - 1, t)];
    }
    inline const char* proChannelName (int c)
    {
        static const char* n[] = { "Stereo", "Mid", "Side", "Left", "Right" };
        return n[juce::jlimit (0, NumProChannels - 1, c)];
    }
    inline const char* proSlopeName (int s)
    {
        static const char* n[] = { "12 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct" };
        return n[juce::jlimit (0, 3, s)];
    }

    // pb3Freq, pb3Gain…
    inline juce::String proParamId (int band, ProField f)
    {
        static const char* s[] = { "On", "Act", "Type", "Freq", "Gain", "Q", "Slope", "Ch", "Dyn", "Thr", "Range", "Att", "Rel", "Ratio" };
        return "pb" + juce::String (band + 1) + s[f];
    }

    // le gain n'a de sens que pour les cloches et les shelves (le dynamique aussi)
    inline bool proHasGain (int type) { return type == PBell || type == PLowShelf || type == PHighShelf; }
    inline bool proIsCut (int type)   { return type == PLowCut || type == PHighCut; }

    struct ProBand
    {
        bool  on = false;        // la bande existe
        bool  active = true;     // bouton marche/arrêt de la bande
        int   type = PBell;
        float freq = 1000.0f, gainDb = 0.0f, q = 1.0f;
        int   slope = 1;
        int   ch = PStereo;
        bool  dyn = false;
        float thr = -24.0f, range = -6.0f, att = 10.0f, rel = 120.0f, ratio = 2.0f;
        float dynDb = 0.0f;      // gain ajouté en ce moment par le dynamique (affichage)
    };

    // Note la plus proche et écart en cents (A4 = 440 Hz)
    inline juce::String noteName (float freq)
    {
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const float midi = 69.0f + 12.0f * std::log2 (juce::jmax (1.0f, freq) / 440.0f);
        const int n = juce::roundToInt (midi);
        const int cents = juce::roundToInt ((midi - (float) n) * 100.0f);
        return juce::String (names[((n % 12) + 12) % 12]) + juce::String (n / 12 - 1)
             + (cents == 0 ? juce::String() : (cents > 0 ? " +" : " ") + juce::String (cents) + "c");
    }

    // Accès de l'interface aux bandes (implémenté côté plugin, avec l'APVTS)
    struct ProEqModel
    {
        virtual ~ProEqModel() = default;
        virtual ProBand band (int k) const = 0;
        virtual void  setField (int k, ProField f, float value) = 0;
        virtual void  beginGesture (int k, ProField f) = 0;
        virtual void  endGesture (int k, ProField f) = 0;
        // réponse (dB) de la bande k aux fréquences données
        virtual void  bandCurve (int k, const float* freqs, float* outDb, int n) const = 0;
        virtual int   createBand (float freq, float gainDb, int type) = 0;   // -1 si toutes les bandes sont prises
        virtual void  deleteBand (int k) = 0;
        virtual int   getSolo() const = 0;
        virtual void  setSolo (int k) = 0;

        void setNow (int k, ProField f, float v) { beginGesture (k, f); setField (k, f, v); endGesture (k, f); }
        bool anyOn() const { for (int k = 0; k < kProBands; ++k) if (band (k).on) return true; return false; }
    };
}
