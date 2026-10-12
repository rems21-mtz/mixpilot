#pragma once

// =============================================================================
//  MixPilot — révision du mix (structure de données, sans interface)
// =============================================================================

#include <juce_core/juce_core.h>
#include <vector>

namespace mp
{
    enum Severity { SevOk = 0, SevInfo, SevWarn, SevProblem };

    struct ReviewItem
    {
        int severity = SevOk;
        juce::String title;     // ex. "Basse" ou "Volume final"
        juce::String text;      // constat + action conseillée
    };

    struct ReviewSection
    {
        juce::String name;
        std::vector<ReviewItem> items;
    };

    struct MixReview
    {
        int score = 0;                      // 0..100
        juce::String verdict;
        int problems = 0, warnings = 0;
        std::vector<ReviewSection> sections;

        void add (int section, int sev, const juce::String& title, const juce::String& text)
        {
            sections[(size_t) section].items.push_back ({ sev, title, text });
            if (sev == SevProblem) ++problems;
            if (sev == SevWarn)    ++warnings;
        }
    };
}
