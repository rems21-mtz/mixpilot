#pragma once

// =============================================================================
//  MixPilot — profils : genres, sorties, instruments
//  Toutes les valeurs "spectrales" sont en dB par bande d'octave
//  (31.5, 63, 125, 250, 500, 1k, 2k, 4k, 8k, 16k Hz).
// =============================================================================

#include <array>

namespace mp
{
    constexpr int kBands = 10;
    constexpr std::array<float, kBands> kBandFreqs { 31.5f, 63.0f, 125.0f, 250.0f, 500.0f,
                                                     1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f };

    using Bands = std::array<float, kBands>;

    // -------------------------------------------------------------------------
    //  Genres
    // -------------------------------------------------------------------------
    // L'ordre est conservé pour la compatibilité des projets : l'ancien "Hip-hop / Trap" (index 1) devient "Trap",
    // le nouveau style "Rap / Hip-hop" est ajouté à la fin.
    enum Genre { Electro = 0, Trap, Pop, Rock, Rap, NumGenres };
    constexpr int HipHop = Trap;   // ancien nom

    struct GenreProfile
    {
        const char* name;
        Bands masterCurve;       // forme spectrale cible du mix complet
        float glue;              // compression multibande (0..1)
        float width;             // largeur stéréo des aigus (1 = neutre)
        float warmth;            // saturation douce (0..1)
        float limiterReleaseMs;
        float compDensity;       // multiplicateur des ratios de compression des pistes
        float clip;              // part du volume obtenue par écrêtage doux (0..1)
        float sideTarget;        // énergie Side/Mid typique du style (largeur)
    };

    inline const GenreProfile& genreProfile (int g)
    {
        static const GenreProfile p[NumGenres] = {
            { "Electro (House / Techno)", { 4, 6, 3, 0, -1, -2, -3, -4, -6, -10 }, 0.55f, 1.15f, 0.25f,  60.0f, 1.20f, 0.70f, 0.14f },
            // Trap : sub-basse 808 très présente, charleys brillants, voix devant
            { "Trap",                     { 7, 7, 2,-1, -1, -2, -2, -3, -4,  -8 }, 0.50f, 1.10f, 0.30f,  70.0f, 1.10f, 0.65f, 0.10f },
            { "Pop",                      { 1, 3, 2, 0, -1, -2, -2, -3, -4,  -8 }, 0.45f, 1.10f, 0.20f, 100.0f, 1.15f, 0.45f, 0.12f },
            { "Rock / Acoustic",        {-2, 2, 2, 1,  0, -1, -2, -3, -5,  -9 }, 0.35f, 1.00f, 0.30f, 120.0f, 0.85f, 0.35f, 0.12f },
            // Rap / Hip-hop : voix très en avant et intelligible, kick/caisse claire qui claquent,
            // basses rondes (moins de sub que la trap), bas-médiums chaleureux, image centrée
            { "Rap / Hip-hop",            { 2, 5, 3, 1,  0, -1, -1, -2, -4,  -9 }, 0.50f, 1.00f, 0.40f,  90.0f, 1.15f, 0.50f, 0.08f },
        };
        return p[g < 0 || g >= NumGenres ? Pop : g];
    }

    // -------------------------------------------------------------------------
    //  Sorties (cibles de loudness)
    // -------------------------------------------------------------------------
    enum Output { Streaming = 0, Club, YouTube, SoundCloud, CD, Vinyl, NumOutputs };

    struct OutputProfile
    {
        const char* name;
        float lufs;          // loudness intégrée visée
        float ceilingDb;     // plafond true-peak
        float monoBassHz;    // 0 = désactivé
        float glueBoost;     // ajout de compression
        float highTameDb;    // atténuation des extrêmes aigus (vinyle)
    };

    inline const OutputProfile& outputProfile (int o)
    {
        static const OutputProfile p[NumOutputs] = {
            { "Streaming (Spotify / Apple)", -14.0f, -1.0f,   0.0f, 0.00f,  0.0f },
            { "Club / DJ",                    -8.0f, -0.3f, 120.0f, 0.20f,  0.0f },
            { "YouTube",                     -13.0f, -1.0f,   0.0f, 0.00f,  0.0f },
            { "SoundCloud",                  -11.0f, -1.0f,   0.0f, 0.10f,  0.0f },
            { "CD",                           -9.0f, -0.3f,   0.0f, 0.15f,  0.0f },
            { "Vinyl",                      -12.0f, -1.0f, 150.0f, 0.05f, -2.0f },
        };
        return p[o < 0 || o >= NumOutputs ? Streaming : o];
    }

    // Volume visé selon la sortie ET le style : les plateformes qui normalisent (Spotify, YouTube)
    // gardent leur valeur ; les sorties "fortes" (SoundCloud, club, CD, vinyle) suivent les usages du style.
    inline float outputLufs (int output, int genre)
    {
        static const float t[NumOutputs][NumGenres] = {
            //  Electro  Trap    Pop     Rock    Rap
            { -14.0f, -14.0f, -14.0f, -14.0f, -14.0f },   // Streaming (normalisé)
            {  -7.0f,  -7.0f,  -8.0f,  -8.5f,  -7.5f },   // Club
            { -13.0f, -13.0f, -13.0f, -13.0f, -13.0f },   // YouTube (normalisé)
            {  -8.0f,  -8.0f,  -9.0f, -10.0f,  -8.5f },   // SoundCloud
            {  -8.0f,  -8.0f,  -9.0f, -10.0f,  -8.5f },   // CD
            { -10.0f, -10.0f, -11.0f, -12.0f, -10.5f },   // Vinyle
        };
        if (output < 0 || output >= NumOutputs || genre < 0 || genre >= NumGenres) return -14.0f;
        return t[output][genre];
    }

    inline bool outputIsNormalised (int output) { return output == Streaming || output == YouTube; }

    // -------------------------------------------------------------------------
    //  Instruments
    // -------------------------------------------------------------------------
    // Nouveaux types ajoutés à la fin (compatibilité des projets) : instru complète et acapella (mashups)
    enum Instrument { Kick = 0, Snare, Hats, Bass, Vocal, Lead, Pad, Guitar, Other, Instrumental, Acapella, NumInstruments };

    inline bool isVoice (int inst) { return inst == Vocal || inst == Acapella; }

    struct InstrumentProfile
    {
        const char* name;
        Bands shape;          // forme spectrale idéale
        Bands weight;         // importance de chaque bande pour l'EQ (0 = ignorée)
        float hpfHz;          // nettoyage du bas
        float loudness;       // niveau cible dans le mix (LUFS, avant master)
        float ratio, attackMs, releaseMs, threshOffsetDb;   // compression
        int   priority;       // plus haut = garde sa place en cas de masquage
    };

    inline const InstrumentProfile& instrumentProfile (int i)
    {
        static const InstrumentProfile p[NumInstruments] = {
            { "Kick",          {  2,  4,  0, -6, -9,-10, -8, -7,-10,-16 }, { 1, 1, 1, 1, 1, .6f, .6f, .6f, .3f, 0 },     25, -20, 3.0f, 15,  80, 6, 8 },
            { "Snare / Clap",  {-14, -6,  1,  3,  1, -1, -1, -1, -3, -8 }, { 0, .3f, 1, 1, 1, 1, 1, 1, .6f, .3f },     90, -22, 4.0f,  5, 100, 5, 7 },
            { "Hi-hat / Perc", {-30,-25,-18,-12, -8, -4, -1,  1,  2,  0 }, { 0, 0, 0, .3f, .6f, 1, 1, 1, 1, .6f },     300, -28, 2.0f,  2,  60, 6, 2 },
            { "Bass",          {  1,  3,  2, -2, -6,-10,-13,-16,-20,-25 }, { .6f, 1, 1, 1, 1, .6f, .3f, 0, 0, 0 },     28, -21, 4.0f, 10, 120, 3, 6 },
            { "Vocal",         {-25,-12, -3,  0,  1,  1,  0, -1, -3, -7 }, { 0, .3f, 1, 1, 1, 1, 1, 1, 1, .5f },       90, -20, 3.5f,  5, 120, 3, 9 },
            { "Lead / Synth",  {-20,-10, -3,  0,  0,  0, -1, -2, -4, -8 }, { 0, .3f, .8f, 1, 1, 1, 1, 1, .8f, .4f },  120, -23, 3.0f, 10, 150, 4, 5 },
            { "Pad / Keys"   , {-16, -6,  0,  0, -1, -2, -3, -5, -7,-11 }, { 0, .5f, 1, 1, 1, 1, 1, .8f, .6f, .3f },  140, -26, 2.0f, 30, 300, 4, 3 },
            { "Guitar",        {-24,-12, -2,  1,  1,  0, -1, -3, -6,-12 }, { 0, .3f, 1, 1, 1, 1, 1, 1, .6f, .2f },     90, -23, 3.0f, 10, 150, 4, 4 },
            { "Other",         {  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 }, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },           30, -26, 2.0f, 10, 150, 6, 1 },
            // Instru complète (beat, version instrumentale) : traitée comme un mix entier, forme = courbe du style
            { "Full instrumental", { 4, 6, 3, 0, -1, -2, -3, -4, -6,-10 }, { .4f, .8f, 1, 1, 1, 1, 1, 1, .8f, .4f }, 22, -19, 1.6f, 30, 200, 4, 2 },
            // Acapella : voix seule (souvent déjà traitée), prioritaire sur tout
            { "A cappella",      {-25,-12, -3,  0,  1,  1,  0, -1, -3, -7 }, { 0, .3f, 1, 1, 1, 1, 1, 1, 1, .5f },      100, -20, 2.5f,  5, 120, 4, 10 },
        };
        return p[i < 0 || i >= NumInstruments ? Other : i];
    }

    // Décalage de niveau selon le genre (dB), [genre][instrument]
    inline float genreLevelOffset (int g, int inst)
    {
        static const float o[NumGenres][NumInstruments] = {
            //  Kick Snare Hats  Bass Vocal Lead  Pad  Guit Other Instru Acap
            {  1.0f, 0.0f, 0.0f, 1.0f,-1.0f, 0.5f, 0.0f,-1.0f, 0.0f, 0.5f,-0.5f },   // Electro
            {  0.0f, 0.5f, 1.0f, 2.0f, 1.5f,-0.5f,-1.0f,-1.0f, 0.0f, 0.0f, 1.0f },   // Trap (808 et voix devant)
            { -1.0f, 0.0f, 0.0f,-1.0f, 2.0f, 0.0f, 0.0f, 0.0f, 0.0f,-0.5f, 1.5f },   // Pop
            {  0.0f, 1.5f,-0.5f, 0.0f, 1.0f,-1.0f,-1.0f, 2.0f, 0.0f, 0.5f, 0.5f },   // Rock
            {  0.5f, 1.5f,-0.5f, 0.5f, 2.5f,-1.0f,-1.5f,-1.0f, 0.0f,-0.5f, 2.0f },   // Rap (voix très en avant, caisse claire qui claque)
        };
        if (g < 0 || g >= NumGenres || inst < 0 || inst >= NumInstruments) return 0.0f;
        return o[g][inst];
    }
}
