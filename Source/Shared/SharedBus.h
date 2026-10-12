#pragma once

// =============================================================================
//  SharedBus — liaison temps réel entre MixPilot Master et MixPilot Track.
//  Une petite zone de mémoire partagée (POSIX shm) que toutes les instances
//  ouvrent, même si elles sont dans des plugins / processus différents.
//  Tous les champs sont atomiques : aucun verrou, compatible temps réel.
// =============================================================================

#include <atomic>
#include <cstdint>
#include <cstring>
#include "Profiles.h"

namespace mp
{
    constexpr int      kMaxTracks  = 64;
    constexpr uint32_t kBusMagic   = 0x4D504233;   // "MPB3"
    constexpr int64_t  kStaleMs    = 3000;         // instance considérée morte après 3 s

    static_assert (std::atomic<float>::is_always_lock_free,   "float atomics must be lock-free");
    static_assert (std::atomic<int64_t>::is_always_lock_free, "int64 atomics must be lock-free");

    enum TrackPhase { PhaseIdle = 0, PhaseListening, PhaseSettling, PhaseLocked };

    struct alignas (64) TrackSlot
    {
        std::atomic<uint64_t> owner      { 0 };   // 0 = libre
        std::atomic<int64_t>  heartbeat  { 0 };   // ms (horloge murale)
        std::atomic<int32_t>  instrument { Other };
        std::atomic<int32_t>  phase      { PhaseIdle };
        std::atomic<int32_t>  hasProfile { 0 };   // spectre appris valide
        std::atomic<int32_t>  listenId   { 0 };   // id de la session d'écoute terminée
        std::atomic<float>    loudness   { -70.0f };
        std::atomic<float>    learnedLoudness { -70.0f };
        std::atomic<float>    trimDb     { 0.0f };
        std::atomic<float>    gainReductionDb { 0.0f };
        std::atomic<float>    duckEnv    { 0.0f };   // enveloppe du kick, ou de présence d'une voix (0..1)
        std::atomic<float>    bands[kBands] {};      // spectre appris (dB)
        std::atomic<float>    eq[kBands]    {};      // correction appliquée (dB)
        std::atomic<float>    outBands[kBands] {};   // spectre estimé APRÈS correction (dB) : contribution au mix
        std::atomic<float>    duckDb     { 0.0f };   // profondeur de ducking active
        std::atomic<float>    inputPeakDb { -100.0f };
        std::atomic<float>    confidence { 0.0f };
        std::atomic<int32_t>  manual     { 0 };      // instrument choisi à la main
        // pour la révision du mix
        std::atomic<float>    targetLufs { -23.0f };  // niveau visé dans le mix
        std::atomic<float>    outIntegrated { -70.0f };  // niveau réel en sortie de piste (LUFS intégrés, silences exclus)
        std::atomic<float>    maskDb     { 0.0f };    // plus forte place laissée aux autres (dB, négatif)
        std::atomic<int32_t>  maskBand   { -1 };
        std::atomic<float>    voiceDuckDb { 0.0f };   // instru : creusement des médiums sous la voix
        std::atomic<float>    mixShareDb { 0.0f };    // plus forte correction demandée par le Master
        std::atomic<int32_t>  bypassed   { 0 };
        std::atomic<int32_t>  following  { 1 };
        std::atomic<uint32_t> name[8]       {};      // 32 caractères ASCII

        void setName (const char* s) noexcept
        {
            char buf[32] {};
            std::strncpy (buf, s, 31);
            for (int i = 0; i < 8; ++i)
            {
                uint32_t w; std::memcpy (&w, buf + i * 4, 4);
                name[i].store (w, std::memory_order_relaxed);
            }
        }

        void getName (char* out32) const noexcept
        {
            for (int i = 0; i < 8; ++i)
            {
                const uint32_t w = name[i].load (std::memory_order_relaxed);
                std::memcpy (out32 + i * 4, &w, 4);
            }
            out32[31] = 0;
        }
    };

    struct BusMemory
    {
        std::atomic<uint32_t> magic           { 0 };
        std::atomic<int64_t>  masterHeartbeat { 0 };
        std::atomic<uint64_t> masterOwner     { 0 };
        std::atomic<int32_t>  genre           { Pop };
        std::atomic<int32_t>  output          { Streaming };
        std::atomic<float>    amount          { 0.8f };
        std::atomic<int32_t>  listenRequest   { 0 };    // incrémenté par le Master
        std::atomic<int32_t>  listenSeconds   { 8 };
        std::atomic<float>    masterLufs      { -70.0f };
        // Correction du mix déléguée aux pistes (Master -> Pistes), en dB par octave
        std::atomic<float>    mixCorrection[kBands] {};
        std::atomic<int32_t>  mixCorrectionId { 0 };
        std::atomic<float>    trackShare      { 0.5f };  // part de la correction confiée aux pistes (0..1)
        TrackSlot             slots[kMaxTracks];
    };

    // Accès au bus (ouvre / crée la mémoire partagée au premier appel)
    BusMemory& bus();

    int64_t nowMs();
    void setTestClock (int64_t ms);   // tests hors-ligne uniquement (-1 = horloge réelle)
    uint64_t makeInstanceId();

    inline bool isAlive (int64_t heartbeat) { return nowMs() - heartbeat < kStaleMs; }
    inline bool masterPresent()             { return isAlive (bus().masterHeartbeat.load()); }

    // Réserve un emplacement pour une instance Track, ou -1 si plein
    int  claimSlot (uint64_t id);
    void releaseSlot (int slot, uint64_t id);
}
