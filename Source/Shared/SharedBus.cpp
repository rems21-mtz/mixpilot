#include "SharedBus.h"
#include <chrono>
#include <random>
#include <new>

#if defined (__APPLE__) || defined (__linux__)
 #include <sys/mman.h>
 #include <sys/stat.h>
 #include <fcntl.h>
 #include <unistd.h>
 #define MP_HAS_SHM 1
#elif defined (_WIN32)
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
 #define MP_HAS_SHM 0
 #define MP_HAS_WIN_SHM 1
#else
 #define MP_HAS_SHM 0
#endif

#ifndef MP_HAS_WIN_SHM
 #define MP_HAS_WIN_SHM 0
#endif

namespace mp
{
    namespace
    {
        // Une mémoire neuve est à zéro : les atomiques à 0 sont valides.
        // On pose juste les valeurs par défaut la première fois.
        BusMemory* initBus (void* mem)
        {
            auto* b = static_cast<BusMemory*> (mem);
            uint32_t expected = 0;
            if (b->magic.compare_exchange_strong (expected, kBusMagic))
            {
                b->genre.store (Pop);
                b->output.store (Streaming);
                b->amount.store (0.8f);
                b->listenSeconds.store (8);
                b->masterLufs.store (-70.0f);
            }
            return b;
        }

        BusMemory* openBus()
        {
           #if MP_HAS_WIN_SHM
            // Windows : mappage nommé partagé par toutes les instances de la session
            // (même hôte ou hôtes différents). La mémoire d'un nouveau mappage est à zéro.
            // Le handle reste ouvert pour que le mappage vive aussi longtemps que le plugin.
            HANDLE h = CreateFileMappingW (INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                                           0, (DWORD) sizeof (BusMemory), L"Local\\mixpilot.bus.v3");
            if (h != nullptr)
            {
                if (void* mem = MapViewOfFile (h, FILE_MAP_ALL_ACCESS, 0, 0, sizeof (BusMemory)))
                    return initBus (mem);
                CloseHandle (h);
            }
           #endif
           #if MP_HAS_SHM
            const char* shmName = "/mixpilot.bus.v3";
            const int fd = shm_open (shmName, O_CREAT | O_RDWR, 0666);
            if (fd >= 0)
            {
                bool sized = true;
                struct stat st {};
                if (fstat (fd, &st) == 0 && st.st_size < (off_t) sizeof (BusMemory))
                    sized = ftruncate (fd, (off_t) sizeof (BusMemory)) == 0;   // mémoire remplie de zéros

                void* mem = sized ? mmap (nullptr, sizeof (BusMemory), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0) : MAP_FAILED;
                close (fd);

                if (mem != MAP_FAILED)
                    return initBus (mem);
            }
           #endif
            // Repli : bus local au plugin (fonctionne si Master et Track sont dans le même binaire)
            static BusMemory local;
            local.magic.store (kBusMagic);
            return &local;
        }
    }

    BusMemory& bus()
    {
        static BusMemory* b = openBus();
        return *b;
    }

    static std::atomic<int64_t> testClockMs { -1 };
    void setTestClock (int64_t ms) { testClockMs.store (ms); }

    int64_t nowMs()
    {
        const int64_t t = testClockMs.load (std::memory_order_relaxed);
        if (t >= 0) return t;
        using namespace std::chrono;
        return duration_cast<milliseconds> (system_clock::now().time_since_epoch()).count();
    }

    uint64_t makeInstanceId()
    {
        static std::mt19937_64 rng { std::random_device{}() ^ (uint64_t) nowMs() };
        uint64_t v = 0;
        while (v == 0) v = rng();
        return v;
    }

    int claimSlot (uint64_t id)
    {
        auto& b = bus();
        for (int pass = 0; pass < 2; ++pass)
        {
            for (int i = 0; i < kMaxTracks; ++i)
            {
                auto& s = b.slots[i];
                uint64_t cur = s.owner.load();
                const bool free  = (cur == 0);
                const bool stale = (pass == 1 && cur != 0 && ! isAlive (s.heartbeat.load()));
                if ((free || stale) && s.owner.compare_exchange_strong (cur, id))
                {
                    s.heartbeat.store (nowMs());
                    s.phase.store (PhaseIdle);
                    s.hasProfile.store (0);
                    s.listenId.store (0);
                    s.trimDb.store (0.0f);
                    s.gainReductionDb.store (0.0f);
                    s.duckEnv.store (0.0f);
                    s.loudness.store (-70.0f);
                    for (int k = 0; k < kBands; ++k) { s.bands[k].store (-100.0f); s.eq[k].store (0.0f); s.outBands[k].store (-100.0f); }
                    s.duckDb.store (0.0f); s.inputPeakDb.store (-100.0f); s.confidence.store (0.0f); s.manual.store (0);
                    s.targetLufs.store (-23.0f); s.outIntegrated.store (-70.0f); s.maskDb.store (0.0f); s.maskBand.store (-1); s.voiceDuckDb.store (0.0f);
                    s.mixShareDb.store (0.0f); s.bypassed.store (0); s.following.store (1);
                    return i;
                }
            }
        }
        return -1;
    }

    void releaseSlot (int slot, uint64_t id)
    {
        if (slot < 0 || slot >= kMaxTracks) return;
        uint64_t expected = id;
        bus().slots[slot].owner.compare_exchange_strong (expected, 0);
    }
}
