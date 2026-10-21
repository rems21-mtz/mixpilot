#pragma once

// =============================================================================
//  MixPilot — interface commune (thème sombre "studio")
// =============================================================================

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "Profiles.h"
#include "Review.h"

namespace mp::ui
{
    namespace col
    {
        const juce::Colour bg        { 0xff121418 };
        const juce::Colour panel     { 0xff1b1e24 };
        const juce::Colour panelHi   { 0xff23272f };
        const juce::Colour border    { 0xff2e333c };
        const juce::Colour text      { 0xffd8dde6 };
        const juce::Colour textDim   { 0xff7f8794 };
        const juce::Colour accent    { 0xff4aa3ff };
        const juce::Colour accent2   { 0xff36d6c3 };
        const juce::Colour warn      { 0xffffa94d };
        const juce::Colour good      { 0xff5fd38d };
        const juce::Colour bad       { 0xffff5f6d };
        const juce::Colour purple    { 0xffb48cff };   // correction demandée par le Master / référence
    }

    inline juce::Font font (float h, bool bold = false)
    {
        return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
    }

    // -------------------------------------------------------------------------
    class StudioLook : public juce::LookAndFeel_V4
    {
    public:
        StudioLook()
        {
            setColour (juce::ComboBox::backgroundColourId, col::panelHi);
            setColour (juce::ComboBox::outlineColourId,    col::border);
            setColour (juce::ComboBox::textColourId,       col::text);
            setColour (juce::ComboBox::arrowColourId,      col::textDim);
            setColour (juce::PopupMenu::backgroundColourId, col::panel);
            setColour (juce::PopupMenu::textColourId,       col::text);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent.withAlpha (0.25f));
            setColour (juce::PopupMenu::highlightedTextColourId,       col::text);
            setColour (juce::TextButton::buttonColourId,   col::panelHi);
            setColour (juce::TextButton::buttonOnColourId, col::accent);
            setColour (juce::TextButton::textColourOffId,  col::text);
            setColour (juce::TextButton::textColourOnId,   juce::Colours::white);
            setColour (juce::Slider::textBoxTextColourId,  col::text);
            setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            setColour (juce::Label::textColourId, col::text);
        }

        void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                               float start, float end, juce::Slider& s) override
        {
            auto b = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
            const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
            const auto c  = b.getCentre();
            const float a = start + pos * (end - start);
            const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);

            juce::Path track; track.addCentredArc (c.x, c.y, r - 3, r - 3, 0, start, end, true);
            g.setColour (col::border);
            g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // arc bipolaire si la plage est centrée sur 0
            const bool bipolar = s.getMinimum() < 0 && s.getMaximum() > 0;
            const float from = bipolar ? start + (float) ((0 - s.getMinimum()) / (s.getMaximum() - s.getMinimum())) * (end - start) : start;
            if (std::abs (a - from) > 0.01f)
            {
                juce::Path v; v.addCentredArc (c.x, c.y, r - 3, r - 3, 0, juce::jmin (from, a), juce::jmax (from, a), true);
                g.setColour (accent);
                g.strokePath (v, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }

            const float kr = r - 9;
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2b3039), c.x, c.y - kr, juce::Colour (0xff181b20), c.x, c.y + kr, false));
            g.fillEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2);
            g.setColour (col::border.brighter (0.2f));
            g.drawEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2, 1.0f);

            juce::Path p;
            p.startNewSubPath (c.getPointOnCircumference (kr * 0.35f, a));
            p.lineTo (c.getPointOnCircumference (kr * 0.85f, a));
            g.setColour (col::text);
            g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                               juce::Slider::SliderStyle style, juce::Slider& s) override
        {
            if (style != juce::Slider::LinearVertical) { LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 0, style, s); return; }
            const float cx = (float) x + (float) w * 0.5f;
            const float zeroY = (float) y + (float) h * (float) (s.getMaximum() / (s.getMaximum() - s.getMinimum()));
            g.setColour (col::border);
            g.fillRoundedRectangle (cx - 1.5f, (float) y, 3.0f, (float) h, 1.5f);
            g.setColour (col::textDim.withAlpha (0.4f));
            g.drawHorizontalLine ((int) zeroY, cx - 6, cx + 6);
            const auto accent = s.findColour (juce::Slider::trackColourId);
            g.setColour (accent);
            g.fillRect (juce::Rectangle<float> (cx - 1.5f, juce::jmin (pos, zeroY), 3.0f, std::abs (pos - zeroY)));
            g.setColour (col::text);
            g.fillRoundedRectangle (cx - 7, pos - 3, 14, 6, 2);
        }

        void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
        {
            auto r = b.getLocalBounds().toFloat().reduced (0.5f);
            const bool on = b.getToggleState();
            auto fill = on ? b.findColour (juce::TextButton::buttonOnColourId) : b.findColour (juce::TextButton::buttonColourId);
            if (over) fill = fill.brighter (0.08f);
            if (down) fill = fill.darker (0.1f);
            g.setColour (fill);
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (on ? fill.brighter (0.3f) : col::border);
            g.drawRoundedRectangle (r, 4.0f, 1.0f);
        }

        // Bulles d'aide ------------------------------------------------------
        static juce::TextLayout tooltipLayout (const juce::String& text)
        {
            juce::AttributedString a;
            a.setJustification (juce::Justification::topLeft);
            a.append (text, font (12.5f), col::text);
            a.setLineSpacing (2.0f);
            juce::TextLayout tl; tl.createLayout (a, 300.0f);
            return tl;
        }

        juce::Rectangle<int> getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parent) override
        {
            const auto tl = tooltipLayout (text);
            const int w = (int) std::ceil (tl.getWidth()) + 24, h = (int) std::ceil (tl.getHeight()) + 18;
            return juce::Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 14,
                                         pos.y > parent.getCentreY() ? pos.y - (h + 8)  : pos.y + 18, w, h).constrainedWithin (parent);
        }

        void drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h) override
        {
            const auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
            g.setColour (juce::Colour (0xf0262a33)); g.fillRoundedRectangle (r, 6.0f);
            g.setColour (col::accent.withAlpha (0.6f)); g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
            tooltipLayout (text).draw (g, r.reduced (12.0f, 9.0f));
        }

        juce::Font getTextButtonFont (juce::TextButton&, int h) override { return font (juce::jmin (13.0f, (float) h * 0.5f), true); }
        juce::Font getComboBoxFont (juce::ComboBox&) override             { return font (13.0f); }
        juce::Font getPopupMenuFont() override                            { return font (13.0f); }
        juce::Font getLabelFont (juce::Label&) override                   { return font (12.0f); }
    };

    // -------------------------------------------------------------------------
    //  Petit bouton on/off de module
    // -------------------------------------------------------------------------
    inline void stylePower (juce::TextButton& b)
    {
        b.setClickingTogglesState (true);
        b.setColour (juce::TextButton::buttonOnColourId, col::accent.withAlpha (0.85f));
    }

    // -------------------------------------------------------------------------
    //  Analyseur de spectre (log 20 Hz – 20 kHz) + courbes
    // -------------------------------------------------------------------------
    class SpectrumView : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        std::vector<float> specIn, specOut;     // dB par bin FFT
        double sampleRate = 48000.0;
        int fftSize = 2048;
        std::function<float (float)> eqCurve;   // dB de correction
        bool showTarget = false;
        Bands target {};                         // forme cible (dB relatifs)
        Bands measured {};                       // spectre appris (dB)
        bool showMeasured = false;
        Bands refBands {};                       // spectre de la référence
        bool showRef = false;
        juce::String caption;

        // Points d'EQ déplaçables à la souris : vertical = gain, horizontal = fréquence.
        // eqPoints vide (ou non défini) = courbe en lecture seule.
        struct EqPoint { float freq = 1000.0f, gainDb = 0.0f; bool edited = false; };
        std::function<std::vector<EqPoint>()> eqPoints;
        std::function<void (int, float freq, float gainDb)> onPointMoved;
        std::function<void (int, bool starting)> onPointGesture;   // début / fin de glissement (automation)
        std::function<void (int)> onPointReset;                    // revient à la valeur de l'analyse
        std::function<void()> onResetAll;
        float pointMinDb = -12.0f, pointMaxDb = 12.0f;

        float xForFreq (float f) const
        {
            const auto b = getLocalBounds().toFloat();
            return b.getX() + b.getWidth() * (std::log10 (f / 20.0f) / 3.0f);
        }
        float freqForX (float x) const { return 20.0f * std::pow (1000.0f, x / (float) juce::jmax (1, getWidth())); }

        void paint (juce::Graphics& g) override
        {
            const auto b = getLocalBounds().toFloat();
            g.setColour (col::panel);
            g.fillRoundedRectangle (b, 6.0f);

            // Grille
            g.setFont (font (10.0f));
            for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
            {
                const float x = xForFreq (f);
                g.setColour (col::border.withAlpha (0.6f));
                g.drawVerticalLine ((int) x, b.getY() + 4, b.getBottom() - 14);
                g.setColour (col::textDim);
                g.drawText (f >= 1000 ? juce::String (f / 1000.0f, 0) + "k" : juce::String ((int) f),
                            juce::Rectangle<float> (x - 20, b.getBottom() - 14, 40, 12), juce::Justification::centred);
            }
            for (int db = -80; db <= 0; db += 20)
            {
                const float y = yForSpec ((float) db);
                g.setColour (col::border.withAlpha (0.35f));
                g.drawHorizontalLine ((int) y, b.getX(), b.getRight());
            }

            drawSpectrum (g, specIn,  col::textDim.withAlpha (0.25f), true);
            drawSpectrum (g, specOut, col::accent.withAlpha (0.9f),   false);

            if (showRef) drawBands (g, refBands, col::purple, false);
            if (showTarget) drawBands (g, target, col::warn, true);
            if (showMeasured) drawBands (g, measured, col::accent2.withAlpha (0.8f), false);

            // Courbe de correction (échelle ±12 dB autour du centre)
            if (eqCurve)
            {
                juce::Path p;
                const float mid = b.getCentreY();
                for (float x = b.getX(); x <= b.getRight(); x += 2.0f)
                {
                    const float db = juce::jlimit (-12.0f, 12.0f, eqCurve (freqForX (x)));
                    const float y = mid - db / 12.0f * (b.getHeight() * 0.42f);
                    if (x == b.getX()) p.startNewSubPath (x, y); else p.lineTo (x, y);
                }
                g.setColour (col::accent2.withAlpha (0.12f));
                juce::Path fill (p); fill.lineTo (b.getRight(), mid); fill.lineTo (b.getX(), mid); fill.closeSubPath();
                g.fillPath (fill);
                g.setColour (col::accent2);
                g.strokePath (p, juce::PathStrokeType (2.0f));
                g.setColour (col::accent2.withAlpha (0.3f));
                g.drawHorizontalLine ((int) mid, b.getX(), b.getRight());
            }

            if (caption.isNotEmpty())
            {
                g.setColour (col::textDim);
                g.setFont (font (11.0f, true));
                g.drawText (caption, b.reduced (10, 6), juce::Justification::topLeft);
            }

            drawPoints (g);
        }

        // ---- édition des points ---------------------------------------------
        void mouseMove (const juce::MouseEvent& e) override
        {
            const int h = pointAt (e.position);
            if (h != hover) { hover = h; repaint(); }
            setMouseCursor (h >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
        }

        void mouseExit (const juce::MouseEvent&) override
        {
            if (hover >= 0 && dragging < 0) { hover = -1; repaint(); }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            const auto pts = currentPoints();
            if (pts.empty()) return;
            const int i = pointAt (e.position);
            if (e.mods.isPopupMenu()) { showMenu (i); return; }
            if (i < 0) return;
            dragging = hover = i;
            dragStartFreq = pts[(size_t) i].freq;
            dragStartGain = pts[(size_t) i].gainDb;
            if (onPointGesture) onPointGesture (i, true);
            setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (dragging < 0 || ! onPointMoved) return;
            // Maj : réglage fin
            const float k = e.mods.isShiftDown() ? 0.25f : 1.0f;
            const auto d = e.position - e.mouseDownPosition;
            const float x = juce::jlimit (0.0f, (float) getWidth(), xForFreq (dragStartFreq) + d.x * k);
            const float db = juce::jlimit (pointMinDb, pointMaxDb, gainForY (yForGain (dragStartGain) + d.y * k));
            onPointMoved (dragging, freqForX (x), db);
            repaint();
        }

        void mouseUp (const juce::MouseEvent&) override
        {
            if (dragging < 0) return;
            if (onPointGesture) onPointGesture (dragging, false);
            dragging = -1;
            repaint();
        }

        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            const int i = pointAt (e.position);
            if (i >= 0 && onPointReset) { onPointReset (i); repaint(); }
        }

        juce::String getTooltip() override
        {
            // pas d'infobulle pendant qu'on manipule un point : l'étiquette du point suffit
            return hover >= 0 || dragging >= 0 ? juce::String() : SettableTooltipClient::getTooltip();
        }

    private:
        int hover = -1, dragging = -1;
        float dragStartFreq = 1000.0f, dragStartGain = 0.0f;

        std::vector<EqPoint> currentPoints() const { return eqPoints ? eqPoints() : std::vector<EqPoint> {}; }

        float yForGain (float db) const
        {
            const auto b = getLocalBounds().toFloat();
            return b.getCentreY() - db / 12.0f * (b.getHeight() * 0.42f);
        }
        float gainForY (float y) const
        {
            const auto b = getLocalBounds().toFloat();
            return (b.getCentreY() - y) / juce::jmax (1.0f, b.getHeight() * 0.42f) * 12.0f;
        }

        int pointAt (juce::Point<float> p) const
        {
            const auto pts = currentPoints();
            int best = -1; float bestD = 11.0f;
            for (size_t i = 0; i < pts.size(); ++i)
            {
                const float d = p.getDistanceFrom ({ xForFreq (pts[i].freq), yForGain (pts[i].gainDb) });
                if (d < bestD) { bestD = d; best = (int) i; }
            }
            return best;
        }

        void showMenu (int i)
        {
            juce::PopupMenu m;
            m.addItem (1, juce::String::fromUTF8 ("Reset this band to the analysis"), i >= 0 && onPointReset != nullptr);
            m.addItem (2, juce::String::fromUTF8 ("Reset all EQ edits"), onResetAll != nullptr);
            juce::Component::SafePointer<SpectrumView> safe (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe, i] (int r)
            {
                if (safe == nullptr) return;
                if (r == 1 && safe->onPointReset) safe->onPointReset (i);
                if (r == 2 && safe->onResetAll)   safe->onResetAll();
                safe->repaint();
            });
        }

        static juce::String freqText (float f)
        {
            return f >= 1000.0f ? juce::String (f / 1000.0f, f >= 10000.0f ? 1 : 2) + " kHz" : juce::String (juce::roundToInt (f)) + " Hz";
        }

        void drawPoints (juce::Graphics& g)
        {
            const auto pts = currentPoints();
            if (pts.empty()) return;
            const int active = dragging >= 0 ? dragging : hover;
            for (size_t i = 0; i < pts.size(); ++i)
            {
                const juce::Point<float> c { xForFreq (pts[i].freq), yForGain (juce::jlimit (-12.0f, 12.0f, pts[i].gainDb)) };
                const bool hot = (int) i == active;
                const float r = hot ? 6.5f : 5.0f;
                g.setColour (col::panel);
                g.fillEllipse (juce::Rectangle<float> (r * 2 + 3, r * 2 + 3).withCentre (c));
                g.setColour (pts[i].edited ? col::warn : col::accent2);
                if (hot) g.fillEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (c));
                else     g.drawEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (c), 2.0f);
            }
            if (active >= 0 && active < (int) pts.size())
            {
                const auto& p = pts[(size_t) active];
                const juce::String txt = freqText (p.freq) + "   " + (p.gainDb >= 0 ? "+" : "") + juce::String (p.gainDb, 1) + " dB";
                const auto b = getLocalBounds().toFloat();
                const float x = xForFreq (p.freq), y = yForGain (juce::jlimit (-12.0f, 12.0f, p.gainDb));
                auto r = juce::Rectangle<float> (112.0f, 20.0f).withCentre ({ x, y < b.getCentreY() ? y + 22.0f : y - 22.0f });
                r = r.constrainedWithin (b.reduced (4));
                g.setColour (col::panelHi.withAlpha (0.95f));
                g.fillRoundedRectangle (r, 4.0f);
                g.setColour (col::border);
                g.drawRoundedRectangle (r, 4.0f, 1.0f);
                g.setColour (col::text);
                g.setFont (font (11.0f, true));
                g.drawText (txt, r, juce::Justification::centred);
            }
        }

        float yForSpec (float db) const
        {
            const auto b = getLocalBounds().toFloat();
            return b.getY() + 6 + (1.0f - (db + 90.0f) / 90.0f) * (b.getHeight() - 22);
        }

        void drawSpectrum (juce::Graphics& g, const std::vector<float>& s, juce::Colour c, bool filled)
        {
            if (s.size() < 4) return;
            const auto b = getLocalBounds().toFloat();
            juce::Path p; bool started = false;
            float lastX = -10;
            for (size_t k = 1; k < s.size(); ++k)
            {
                const float f = (float) (k * sampleRate / fftSize);
                if (f < 20 || f > 20000) continue;
                const float x = xForFreq (f);
                if (x - lastX < 1.5f && k + 1 < s.size()) continue;
                // pente +4,5 dB/oct pour un affichage "plat" sur un mix équilibré
                const float tilt = 4.5f * std::log2 (f / 1000.0f);
                const float y = yForSpec (juce::jlimit (-90.0f, 0.0f, s[k] + tilt));
                if (! started) { p.startNewSubPath (x, y); started = true; } else p.lineTo (x, y);
                lastX = x;
            }
            if (! started) return;
            if (filled)
            {
                juce::Path f (p); f.lineTo (lastX, b.getBottom() - 14); f.lineTo (xForFreq (20), b.getBottom() - 14); f.closeSubPath();
                g.setColour (c); g.fillPath (f);
            }
            else
            {
                g.setColour (c); g.strokePath (p, juce::PathStrokeType (1.5f));
            }
        }

        void drawBands (juce::Graphics& g, const Bands& v, juce::Colour c, bool dashed)
        {
            // affichage relatif centré (forme seulement)
            float mean = 0; for (auto x : v) mean += x; mean /= (float) kBands;
            const auto b = getLocalBounds().toFloat();
            juce::Path p;
            for (int i = 0; i < kBands; ++i)
            {
                const float x = xForFreq (kBandFreqs[(size_t) i]);
                const float y = b.getCentreY() - (v[(size_t) i] - mean) / 24.0f * (b.getHeight() * 0.42f);
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            g.setColour (c);
            if (dashed)
            {
                juce::Path d; const float dash[] = { 5.0f, 4.0f };
                juce::PathStrokeType (1.5f).createDashedStroke (d, p, dash, 2);
                g.fillPath (d);
            }
            else g.strokePath (p, juce::PathStrokeType (1.5f));
        }
    };

    // -------------------------------------------------------------------------
    //  Vumètre vertical
    // -------------------------------------------------------------------------
    class Meter : public juce::Component
    {
    public:
        float valueDb = -100, peakDb = -100, minDb = -60, maxDb = 0;
        bool inverted = false;             // pour la réduction de gain
        juce::String label;
        juce::Colour colour = col::accent;

        void setValue (float db)
        {
            valueDb = db;
            peakDb = db > peakDb ? db : peakDb - 0.3f;
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat();
            auto lab = b.removeFromBottom (14);
            g.setColour (col::textDim); g.setFont (font (10.0f, true));
            g.drawText (label, lab, juce::Justification::centred);

            g.setColour (col::panelHi); g.fillRoundedRectangle (b, 2.0f);
            auto norm = [this] (float db) { return juce::jlimit (0.0f, 1.0f, (db - minDb) / (maxDb - minDb)); };
            const float v = norm (valueDb);
            if (inverted)
            {
                const float h = b.getHeight() * juce::jlimit (0.0f, 1.0f, valueDb / maxDb);
                g.setColour (col::warn); g.fillRect (b.withHeight (h));
            }
            else
            {
                const float h = b.getHeight() * v;
                g.setGradientFill (juce::ColourGradient (colour, 0, b.getBottom(), valueDb > -3 ? col::bad : colour.brighter (0.4f), 0, b.getY(), false));
                g.fillRect (b.withTop (b.getBottom() - h));
                const float py = b.getBottom() - b.getHeight() * norm (peakDb);
                g.setColour (col::text); g.drawHorizontalLine ((int) py, b.getX(), b.getRight());
            }
        }
    };

    // Étiquette de section
    inline void drawSectionTitle (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& t, bool active = true)
    {
        g.setColour (active ? col::text : col::textDim);
        g.setFont (font (11.0f, true));
        g.drawText (t, r, juce::Justification::centredLeft);
    }

    inline void drawPanel (juce::Graphics& g, juce::Rectangle<int> r)
    {
        g.setColour (col::panel);
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (col::border);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 6.0f, 1.0f);
    }

    // Slider rotatif standard avec valeur sous le knob
    inline void setupKnob (juce::Slider& s, juce::Colour c = col::accent)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
        s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        s.setColour (juce::Slider::rotarySliderFillColourId, c);
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        s.setColour (juce::Slider::textBoxTextColourId, col::text);
    }
    // -------------------------------------------------------------------------
    //  Panneau d'aide "?" (sections défilantes)
    // -------------------------------------------------------------------------
    class HelpOverlay : public juce::Component
    {
    public:
        struct Section { juce::String title, body; };

        HelpOverlay (const juce::String& heading, std::vector<Section> s) : title (heading), content (std::move (s))
        {
            addAndMakeVisible (viewport);
            viewport.setViewedComponent (&page, false);
            viewport.setScrollBarsShown (true, false);
            viewport.setScrollBarThickness (8);
            closeBtn.setButtonText ("CLOSE");
            closeBtn.onClick = [this] { setVisible (false); };
            addAndMakeVisible (closeBtn);
            page.owner = this;
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colours::black.withAlpha (0.55f));
            auto r = panelBounds().toFloat();
            g.setColour (col::panel); g.fillRoundedRectangle (r, 10.0f);
            g.setColour (col::border); g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
            g.setColour (col::text); g.setFont (font (18.0f, true));
            g.drawText (title, r.reduced (24, 14).removeFromTop (28).toNearestInt(), juce::Justification::centredLeft);
        }

        void resized() override
        {
            auto r = panelBounds();
            closeBtn.setBounds (r.getRight() - 104, r.getY() + 14, 84, 28);
            auto inner = r.reduced (24, 0).withTrimmedTop (56).withTrimmedBottom (16);
            viewport.setBounds (inner);
            page.setSize (inner.getWidth() - 12, page.computeHeight (inner.getWidth() - 12));
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (! panelBounds().contains (e.getPosition())) setVisible (false);
        }

    private:
        juce::Rectangle<int> panelBounds() const { return getLocalBounds().reduced (60, 40); }

        struct Page : public juce::Component
        {
            HelpOverlay* owner = nullptr;

            juce::TextLayout layoutFor (const Section& s, float w) const
            {
                juce::AttributedString a;
                a.setLineSpacing (3.0f);
                a.append (s.title + "\n", font (14.5f, true), col::accent);
                a.append (s.body, font (13.0f), col::text);
                juce::TextLayout tl; tl.createLayout (a, w);
                return tl;
            }

            int computeHeight (int w) const
            {
                float h = 0;
                for (auto& s : owner->content) h += layoutFor (s, (float) w).getHeight() + 18.0f;
                return (int) std::ceil (h) + 10;
            }

            void paint (juce::Graphics& g) override
            {
                float y = 0;
                for (auto& s : owner->content)
                {
                    auto tl = layoutFor (s, (float) getWidth());
                    tl.draw (g, { 0.0f, y, (float) getWidth(), tl.getHeight() });
                    y += tl.getHeight() + 9.0f;
                    g.setColour (col::border); g.drawHorizontalLine ((int) y, 0.0f, (float) getWidth());
                    y += 9.0f;
                }
            }
        };

        juce::String title;
        std::vector<Section> content;
        juce::Viewport viewport;
        Page page;
        juce::TextButton closeBtn;
    };

    // -------------------------------------------------------------------------
    //  Barre de conseils automatiques (défile toutes les 8 s, clic = suivant)
    // -------------------------------------------------------------------------
    class TipsBar : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        TipsBar() { setTooltip (juce::String::fromUTF8 ("Automatic tips based on what is happening. Click to show the next tip.")); }

        void setTips (const juce::StringArray& t)
        {
            if (t != tips) { tips = t; if (index >= tips.size()) index = 0; repaint(); }
            if (++ticks > 240) { ticks = 0; next(); }   // ~8 s à 30 images/s
        }

        void next() { if (tips.size() > 1) { index = (index + 1) % tips.size(); repaint(); } }

        void mouseDown (const juce::MouseEvent&) override { ticks = 0; next(); }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (col::accent.withAlpha (0.08f)); g.fillRoundedRectangle (r, 4.0f);
            g.setColour (col::accent.withAlpha (0.35f)); g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
            auto left = r.removeFromLeft (110);
            g.setColour (col::accent); g.setFont (font (11.0f, true));
            g.drawText (juce::String::fromUTF8 ("TIP ") + (tips.size() > 1 ? juce::String (index + 1) + "/" + juce::String (tips.size()) : juce::String()),
                        left.reduced (12, 0), juce::Justification::centredLeft);
            g.setColour (col::text); g.setFont (font (12.0f));
            if (index < tips.size())
                g.drawFittedText (tips[index], r.reduced (6, 2).toNearestInt(), juce::Justification::centredLeft, 2, 0.9f);
        }

    private:
        juce::StringArray tips;
        int index = 0, ticks = 0;
    };

    // -------------------------------------------------------------------------
    //  Panneau de révision du mix (note + constats colorés par gravité)
    // -------------------------------------------------------------------------
    class ReviewOverlay : public juce::Component
    {
    public:
        ReviewOverlay()
        {
            addAndMakeVisible (viewport);
            viewport.setViewedComponent (&page, false);
            viewport.setScrollBarsShown (true, false);
            viewport.setScrollBarThickness (8);
            closeBtn.setButtonText ("CLOSE");
            closeBtn.onClick = [this] { setVisible (false); };
            addAndMakeVisible (closeBtn);
            page.owner = this;
        }

        void setReview (const MixReview& r)
        {
            review = r;
            page.setSize (viewport.getWidth() - 12, page.computeHeight (viewport.getWidth() - 12));
            repaint(); page.repaint();
        }

        static juce::Colour sevColour (int sev)
        {
            return sev == SevProblem ? col::bad : sev == SevWarn ? col::warn : sev == SevInfo ? col::accent : col::good;
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colours::black.withAlpha (0.55f));
            auto r = panelBounds().toFloat();
            g.setColour (col::panel); g.fillRoundedRectangle (r, 10.0f);
            g.setColour (col::border); g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);

            auto head = panelBounds().reduced (24, 14).removeFromTop (54);
            // note
            auto scoreBox = head.removeFromLeft (64).toFloat();
            const auto sc = review.score >= 90 ? col::good : review.score >= 70 ? col::accent : review.score >= 50 ? col::warn : col::bad;
            g.setColour (sc.withAlpha (0.15f)); g.fillRoundedRectangle (scoreBox, 8.0f);
            g.setColour (sc); g.drawRoundedRectangle (scoreBox.reduced (0.5f), 8.0f, 1.5f);
            g.setFont (font (24.0f, true)); g.drawText (juce::String (review.score), scoreBox.withTrimmedBottom (12), juce::Justification::centred);
            g.setFont (font (9.5f, true)); g.drawText ("/ 100", scoreBox.withTrimmedTop (34), juce::Justification::centred);
            head.removeFromLeft (14);
            g.setColour (col::text); g.setFont (font (18.0f, true));
            g.drawText (juce::String::fromUTF8 ("Mix review"), head.removeFromTop (26), juce::Justification::centredLeft);
            g.setFont (font (12.5f)); g.setColour (col::textDim);
            g.drawText (review.verdict + "   " + juce::String (review.problems) + juce::String::fromUTF8 (" problem(s) · ")
                        + juce::String (review.warnings) + juce::String::fromUTF8 (" point(s) to check"), head, juce::Justification::centredLeft);
        }

        void resized() override
        {
            auto r = panelBounds();
            closeBtn.setBounds (r.getRight() - 104, r.getY() + 14, 84, 28);
            auto inner = r.reduced (24, 0).withTrimmedTop (78).withTrimmedBottom (16);
            viewport.setBounds (inner);
            page.setSize (inner.getWidth() - 12, page.computeHeight (inner.getWidth() - 12));
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (! panelBounds().contains (e.getPosition())) setVisible (false);
        }

    private:
        juce::Rectangle<int> panelBounds() const { return getLocalBounds().reduced (50, 30); }

        struct Page : public juce::Component
        {
            ReviewOverlay* owner = nullptr;

            juce::TextLayout layoutFor (const ReviewItem& it, float w) const
            {
                juce::AttributedString a;
                a.setLineSpacing (2.0f);
                a.append (it.title + "  ", font (13.0f, true), col::text);
                a.append (it.text, font (12.5f), col::textDim.brighter (0.35f));
                juce::TextLayout tl; tl.createLayout (a, w);
                return tl;
            }

            int computeHeight (int w) const
            {
                float h = 0;
                for (auto& sec : owner->review.sections)
                {
                    h += 30;
                    for (auto& it : sec.items) h += layoutFor (it, (float) w - 22).getHeight() + 10;
                }
                return (int) std::ceil (h) + 10;
            }

            void paint (juce::Graphics& g) override
            {
                float y = 0;
                for (auto& sec : owner->review.sections)
                {
                    g.setColour (col::accent); g.setFont (font (13.5f, true));
                    g.drawText (sec.name.toUpperCase(), 0, (int) y + 6, getWidth(), 18, juce::Justification::centredLeft);
                    g.setColour (col::border); g.drawHorizontalLine ((int) y + 26, 0.0f, (float) getWidth());
                    y += 30;
                    for (auto& it : sec.items)
                    {
                        auto tl = layoutFor (it, (float) getWidth() - 22);
                        g.setColour (sevColour (it.severity));
                        g.fillEllipse (2.0f, y + 5.0f, 9.0f, 9.0f);
                        tl.draw (g, { 20.0f, y, (float) getWidth() - 22, tl.getHeight() });
                        y += tl.getHeight() + 10;
                    }
                }
            }
        };

        MixReview review;
        juce::Viewport viewport;
        Page page;
        juce::TextButton closeBtn;
    };

    // Petit bouton "?"
    inline void styleHelpButton (juce::TextButton& b)
    {
        b.setButtonText ("?");
        b.setColour (juce::TextButton::buttonColourId, col::panelHi);
        b.setTooltip (juce::String::fromUTF8 ("Opens the full plugin guide (every section explained)."));
    }
}

