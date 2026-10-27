#pragma once

// =============================================================================
//  MixPilot — interface commune (thème sombre "studio")
// =============================================================================

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "Profiles.h"
#include "Review.h"
#include "ProEqModel.h"

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
    //  Couleur de chaque bande de l'EQ libre
    // -------------------------------------------------------------------------
    inline juce::Colour proBandColour (int k)
    {
        static const juce::uint32 c[] = { 0xffffc93f, 0xffe04ad8, 0xffff9f43, 0xffff4d6d, 0xff3fd0ff, 0xff5fe08a,
                                          0xff7c8cff, 0xffb48cff, 0xfff2ea6b, 0xff4dffd2, 0xffff7ab8, 0xff9be15d };
        return juce::Colour (c[(size_t) (((k % 12) + 12) % 12)]);
    }

    inline juce::String freqText (float f)
    {
        return f >= 1000.0f ? juce::String (f / 1000.0f, f >= 10000.0f ? 1 : 2) + " kHz" : juce::String (juce::roundToInt (f)) + " Hz";
    }

    // -------------------------------------------------------------------------
    //  Analyseur de spectre (log 20 Hz – 20 kHz) + courbes + EQ libre
    // -------------------------------------------------------------------------
    class SpectrumView : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        enum AnaMode { AnaPre, AnaPost, AnaBoth, AnaDelta };

        SpectrumView() { setWantsKeyboardFocus (true); }

        std::vector<float> specIn, specOut;     // dB par bin FFT
        double sampleRate = 48000.0;
        int fftSize = 2048;
        std::function<float (float)> eqCurve;   // dB de correction (analyse)
        bool showTarget = false;
        Bands target {};                         // forme cible (dB relatifs)
        Bands measured {};                       // spectre appris (dB)
        bool showMeasured = false;
        Bands refBands {};                       // spectre de la référence
        bool showRef = false;
        juce::String caption;

        // Points d'EQ de l'analyse déplaçables à la souris : vertical = gain, horizontal = fréquence.
        // eqPoints vide (ou non défini) = courbe en lecture seule.
        struct EqPoint { float freq = 1000.0f, gainDb = 0.0f; bool edited = false; };
        std::function<std::vector<EqPoint>()> eqPoints;
        std::function<void (int, float freq, float gainDb)> onPointMoved;
        std::function<void (int, bool starting)> onPointGesture;   // début / fin de glissement (automation)
        std::function<void (int)> onPointReset;                    // revient à la valeur de l'analyse
        std::function<void()> onResetAll;
        float pointMinDb = -12.0f, pointMaxDb = 12.0f;

        // EQ libre : bandes ajoutées par double-clic
        ProEqModel* pro = nullptr;
        juce::Component* bandPanel = nullptr;      // panneau de la bande sélectionnée (enfant, posé en bas)
        std::function<void (int)> onSelectBand;     // -1 = aucune
        int  getSelected() const { return selected; }
        void select (int k)
        {
            selected = k;
            if (onSelectBand) onSelectBand (k);
            layoutPanel();
            repaint();
        }

        // Affichage (mémorisé par le plugin via onViewChanged)
        int  anaMode = AnaBoth;
        bool frozen = false, piano = false;
        std::function<void()> onViewChanged;
        static constexpr float rangeDb = 18.0f;    // échelle des courbes d'EQ (±)

        void setSpectra (std::vector<float> in, std::vector<float> out)
        {
            if (frozen) return;
            specIn = std::move (in); specOut = std::move (out);
        }

        float xForFreq (float f) const
        {
            const auto b = getLocalBounds().toFloat();
            return b.getX() + b.getWidth() * (std::log10 (f / 20.0f) / 3.0f);
        }
        float freqForX (float x) const { return 20.0f * std::pow (1000.0f, x / (float) juce::jmax (1, getWidth())); }

        void resized() override { layoutPanel(); }

        void paint (juce::Graphics& g) override
        {
            const auto b = getLocalBounds().toFloat();
            const auto pl = plot();
            g.setColour (col::panel);
            g.fillRoundedRectangle (b, 6.0f);

            drawGlows (g);

            // Grille
            g.setFont (font (10.0f));
            for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
            {
                const float x = xForFreq (f);
                g.setColour (col::border.withAlpha (0.6f));
                g.drawVerticalLine ((int) x, b.getY() + 4, pl.getBottom());
                g.setColour (col::textDim);
                g.drawText (f >= 1000 ? juce::String (f / 1000.0f, 0) + "k" : juce::String ((int) f),
                            juce::Rectangle<float> (x - 20, pl.getBottom(), 40, 12), juce::Justification::centred);
            }
            for (int db = -80; db <= 0; db += 20)
            {
                const float y = yForSpec ((float) db);
                g.setColour (col::border.withAlpha (0.35f));
                g.drawHorizontalLine ((int) y, b.getX(), b.getRight());
            }
            if (pro != nullptr)
            {
                g.setFont (font (9.0f));
                for (int db : { -12, -6, 6, 12 })
                {
                    const float y = yForGain ((float) db);
                    g.setColour (col::border.withAlpha (0.25f));
                    g.drawHorizontalLine ((int) y, b.getX(), b.getRight());
                    g.setColour (col::textDim.withAlpha (0.7f));
                    g.drawText ((db > 0 ? "+" : "") + juce::String (db), juce::Rectangle<float> (b.getRight() - 26, y - 6, 22, 12), juce::Justification::centredRight);
                }
            }

            switch (anaMode)
            {
                case AnaPre:   drawSpectrum (g, specIn, col::textDim.withAlpha (0.35f), true); break;
                case AnaPost:  drawSpectrum (g, specOut, col::accent.withAlpha (0.16f), true);
                               drawSpectrum (g, specOut, col::accent.withAlpha (0.9f), false); break;
                case AnaDelta: drawDelta (g); break;
                default:       drawSpectrum (g, specIn,  col::textDim.withAlpha (0.25f), true);
                               drawSpectrum (g, specOut, col::accent.withAlpha (0.9f),   false); break;
            }

            if (showRef) drawBands (g, refBands, col::purple, false);
            if (showTarget) drawBands (g, target, col::warn, true);
            if (showMeasured) drawBands (g, measured, col::accent2.withAlpha (0.8f), false);

            // Fréquences d'affichage des courbes (une tous les 2 px)
            std::vector<float> xs, fs;
            for (float x = b.getX(); x <= b.getRight(); x += 2.0f) { xs.push_back (x); fs.push_back (freqForX (x)); }
            const int n = (int) xs.size();
            std::vector<float> sum ((size_t) n, 0.0f), tmp ((size_t) n, 0.0f);
            bool anyPro = false;

            // Bandes de l'EQ libre : chacune sa forme colorée
            if (pro != nullptr)
            {
                const float mid = yForGain (0.0f);
                for (int k = 0; k < kProBands; ++k)
                {
                    const auto band = pro->band (k);
                    if (! band.on || ! band.active) continue;
                    anyPro = true;
                    pro->bandCurve (k, fs.data(), tmp.data(), n);
                    juce::Path p;
                    for (int i = 0; i < n; ++i)
                    {
                        sum[(size_t) i] += tmp[(size_t) i];
                        const float y = yForGain (juce::jlimit (-rangeDb - 2, rangeDb + 2, tmp[(size_t) i]));
                        if (i == 0) p.startNewSubPath (xs[0], y); else p.lineTo (xs[(size_t) i], y);
                    }
                    const auto c = proBandColour (k);
                    juce::Path fill (p); fill.lineTo (xs.back(), mid); fill.lineTo (xs.front(), mid); fill.closeSubPath();
                    g.setGradientFill (juce::ColourGradient (c.withAlpha (k == selected ? 0.42f : 0.28f), xForFreq (band.freq), yForGain (band.gainDb),
                                                             c.withAlpha (0.04f), xForFreq (band.freq), mid, false));
                    g.fillPath (fill);
                    g.setColour (c.withAlpha (k == selected ? 0.95f : 0.7f));
                    g.strokePath (p, juce::PathStrokeType (k == selected ? 1.6f : 1.1f));
                }
            }

            // Courbe de correction de l'analyse
            if (eqCurve)
            {
                juce::Path p;
                const float mid = yForGain (0.0f);
                for (int i = 0; i < n; ++i)
                {
                    const float db = eqCurve (fs[(size_t) i]);
                    sum[(size_t) i] += db;
                    const float y = yForGain (juce::jlimit (-rangeDb, rangeDb, db));
                    if (i == 0) p.startNewSubPath (xs[0], y); else p.lineTo (xs[(size_t) i], y);
                }
                g.setColour (col::accent2.withAlpha (0.12f));
                juce::Path fill (p); fill.lineTo (b.getRight(), mid); fill.lineTo (b.getX(), mid); fill.closeSubPath();
                g.fillPath (fill);
                g.setColour (anyPro ? col::accent2.withAlpha (0.8f) : col::accent2);
                g.strokePath (p, juce::PathStrokeType (anyPro ? 1.5f : 2.0f));
                g.setColour (col::accent2.withAlpha (0.3f));
                g.drawHorizontalLine ((int) mid, b.getX(), b.getRight());
            }

            // Courbe totale (analyse + bandes ajoutées)
            if (anyPro)
            {
                juce::Path p;
                for (int i = 0; i < n; ++i)
                {
                    const float y = yForGain (juce::jlimit (-rangeDb - 2, rangeDb + 2, sum[(size_t) i]));
                    if (i == 0) p.startNewSubPath (xs[0], y); else p.lineTo (xs[(size_t) i], y);
                }
                g.setColour (juce::Colours::white.withAlpha (0.9f));
                g.strokePath (p, juce::PathStrokeType (2.0f));
            }

            const auto chips = chipRects();
            if (caption.isNotEmpty())
            {
                g.setColour (col::textDim);
                g.setFont (font (11.0f, true));
                const float right = chips.empty() ? b.getRight() - 10 : chips.front().first.getX() - 10;
                g.drawFittedText (caption, juce::Rectangle<float> (b.getX() + 10, b.getY() + 6, right - b.getX() - 10, 14).toNearestInt(),
                                  juce::Justification::topLeft, 1, 0.7f);
            }

            drawPoints (g);
            drawProPoints (g);
            drawChips (g, chips);
            if (piano) drawPiano (g);

            juce::String tag;
            if (anaMode == AnaDelta) tag = "ANALYZER: DELTA (after - before)";
            if (frozen) tag = tag.isEmpty() ? juce::String ("FROZEN") : tag + "  |  FROZEN";
            if (pro != nullptr && pro->getSolo() >= 0) tag = (tag.isEmpty() ? juce::String() : tag + "  |  ") + "SOLO: BAND " + juce::String (pro->getSolo() + 1);
            if (tag.isNotEmpty())
            {
                g.setFont (font (10.5f, true));
                auto r = juce::Rectangle<float> (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), tag) + 20.0f, 18.0f)
                             .withCentre ({ b.getCentreX(), b.getY() + 34.0f });
                g.setColour (col::panelHi.withAlpha (0.92f)); g.fillRoundedRectangle (r, 4.0f);
                g.setColour (col::warn.withAlpha (0.8f));     g.drawRoundedRectangle (r, 4.0f, 1.0f);
                g.setColour (col::warn);                      g.drawText (tag, r, juce::Justification::centred);
            }
        }

        // ---- souris -----------------------------------------------------------
        void mouseMove (const juce::MouseEvent& e) override
        {
            const int k = proAt (e.position);
            const int h = k >= 0 ? -1 : pointAt (e.position);
            if (h != hover || k != hoverPro) { hover = h; hoverPro = k; repaint(); }
            setMouseCursor (h >= 0 || k >= 0 ? juce::MouseCursor::DraggingHandCursor
                                             : (chipAt (e.position) >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor));
        }

        void mouseExit (const juce::MouseEvent&) override
        {
            if ((hover >= 0 || hoverPro >= 0) && dragging < 0 && dragPro < 0) { hover = hoverPro = -1; repaint(); }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (isShowing()) grabKeyboardFocus();
            if (const int c = chipAt (e.position); c >= 0) { clickChip (c); return; }

            const int k = proAt (e.position);
            if (e.mods.isPopupMenu())
            {
                if (k >= 0) { if (selected != k) select (k); showProMenu (k); }
                else showMenu (pointAt (e.position), e.position);
                return;
            }

            if (k >= 0)
            {
                if (e.mods.isAltDown()) { deleteBand (k); return; }
                if (selected != k) select (k);
                const auto band = pro->band (k);
                dragPro = hoverPro = k;
                dragStartFreq = band.freq;
                dragStartGain = band.gainDb;
                dragProGain = proHasGain (band.type);
                pro->beginGesture (k, FFreq);
                if (dragProGain) pro->beginGesture (k, FGain);
                setMouseCursor (juce::MouseCursor::DraggingHandCursor);
                return;
            }

            const auto pts = currentPoints();
            const int i = pts.empty() ? -1 : pointAt (e.position);
            if (i < 0)
            {
                if (selected >= 0) select (-1);
                return;
            }
            dragging = hover = i;
            dragStartFreq = pts[(size_t) i].freq;
            dragStartGain = pts[(size_t) i].gainDb;
            if (onPointGesture) onPointGesture (i, true);
            setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            // Maj : réglage fin
            const float k = e.mods.isShiftDown() ? 0.25f : 1.0f;
            const auto d = e.position - e.mouseDownPosition;
            const float x = juce::jlimit (0.0f, (float) getWidth(), xForFreq (dragStartFreq) + d.x * k);

            if (dragPro >= 0 && pro != nullptr)
            {
                pro->setField (dragPro, FFreq, juce::jlimit (20.0f, 20000.0f, freqForX (x)));
                if (dragProGain)
                    pro->setField (dragPro, FGain, juce::jlimit (-rangeDb, rangeDb, gainForY (yForGain (dragStartGain) + d.y * k)));
                repaint();
                return;
            }

            if (dragging < 0 || ! onPointMoved) return;
            const float db = juce::jlimit (pointMinDb, pointMaxDb, gainForY (yForGain (dragStartGain) + d.y * k));
            onPointMoved (dragging, freqForX (x), db);
            repaint();
        }

        void mouseUp (const juce::MouseEvent&) override
        {
            if (dragPro >= 0 && pro != nullptr)
            {
                pro->endGesture (dragPro, FFreq);
                if (dragProGain) pro->endGesture (dragPro, FGain);
                dragPro = -1;
                repaint();
                return;
            }
            if (dragging < 0) return;
            if (onPointGesture) onPointGesture (dragging, false);
            dragging = -1;
            repaint();
        }

        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            if (chipAt (e.position) >= 0) return;
            if (const int k = proAt (e.position); k >= 0)
            {
                if (proHasGain (pro->band (k).type)) pro->setNow (k, FGain, 0.0f);
                repaint();
                return;
            }
            const int i = pointAt (e.position);
            if (i >= 0 && onPointReset) { onPointReset (i); repaint(); return; }
            addBandAt (e.position, PBell);
        }

        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
        {
            const int k = proAt (e.position);
            if (k < 0 || pro == nullptr) { Component::mouseWheelMove (e, w); return; }
            // molette = Q (Maj : réglage fin)
            const float dir = w.isReversed ? -1.0f : 1.0f;
            const float q = pro->band (k).q * std::exp (w.deltaY * dir * (e.mods.isShiftDown() ? 0.6f : 2.4f));
            pro->setNow (k, FQ, juce::jlimit (0.1f, 18.0f, q));
            repaint();
        }

        bool keyPressed (const juce::KeyPress& key) override
        {
            if (selected >= 0 && pro != nullptr
                && (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey))
            {
                deleteBand (selected);
                return true;
            }
            return false;
        }

        juce::String getTooltip() override
        {
            // pas d'infobulle pendant qu'on manipule un point : l'étiquette du point suffit
            return hover >= 0 || dragging >= 0 || hoverPro >= 0 || dragPro >= 0 ? juce::String() : SettableTooltipClient::getTooltip();
        }

    private:
        int hover = -1, dragging = -1, hoverPro = -1, dragPro = -1, selected = -1;
        bool dragProGain = false;
        float dragStartFreq = 1000.0f, dragStartGain = 0.0f;

        float bottomPad() const { return piano ? 30.0f : 14.0f; }
        juce::Rectangle<float> plot() const { return getLocalBounds().toFloat().withTrimmedBottom (bottomPad()); }

        void layoutPanel()
        {
            if (bandPanel == nullptr) return;
            // en bas, ou en haut si la bande sélectionnée est dans la moitié basse (pour ne pas la cacher)
            const int w = juce::jmin (640, getWidth() - 12), h = 112;
            const bool top = pro != nullptr && selected >= 0 && proPos (pro->band (selected)).y > plot().getCentreY();
            bandPanel->setBounds ((getWidth() - w) / 2, top ? 26 : (int) plot().getBottom() - h - 6, w, h);
        }

        std::vector<EqPoint> currentPoints() const { return eqPoints ? eqPoints() : std::vector<EqPoint> {}; }

    public:
        float yForGain (float db) const
        {
            const auto b = plot();
            return b.getCentreY() - db / rangeDb * (b.getHeight() * 0.44f);
        }
        float gainForY (float y) const
        {
            const auto b = plot();
            return (b.getCentreY() - y) / juce::jmax (1.0f, b.getHeight() * 0.44f) * rangeDb;
        }

    private:

        int pointAt (juce::Point<float> p) const
        {
            const auto pts = currentPoints();
            int best = -1; float bestD = 11.0f;
            for (size_t i = 0; i < pts.size(); ++i)
            {
                const float d = p.getDistanceFrom ({ xForFreq (pts[i].freq), yForGain (juce::jlimit (-rangeDb, rangeDb, pts[i].gainDb)) });
                if (d < bestD) { bestD = d; best = (int) i; }
            }
            return best;
        }

        juce::Point<float> proPos (const ProBand& b) const
        {
            return { xForFreq (b.freq), yForGain (proHasGain (b.type) ? juce::jlimit (-rangeDb, rangeDb, b.gainDb) : 0.0f) };
        }

        int proAt (juce::Point<float> p) const
        {
            if (pro == nullptr) return -1;
            int best = -1; float bestD = 11.0f;
            for (int k = 0; k < kProBands; ++k)
            {
                const auto b = pro->band (k);
                if (! b.on) continue;
                const float d = p.getDistanceFrom (proPos (b));
                if (d < bestD) { bestD = d; best = k; }
            }
            return best;
        }

        void addBandAt (juce::Point<float> p, int type)
        {
            if (pro == nullptr) return;
            const int k = pro->createBand (juce::jlimit (20.0f, 20000.0f, freqForX (p.x)),
                                           juce::jlimit (-rangeDb, rangeDb, gainForY (p.y)), type);
            if (k >= 0) select (k);
            repaint();
        }

        void deleteBand (int k)
        {
            pro->deleteBand (k);
            hoverPro = -1;
            if (selected == k) select (-1);
            repaint();
        }

        // ---- boutons de l'analyseur (en haut à droite) ------------------------
        std::vector<std::pair<juce::Rectangle<float>, int>> chipRects() const
        {
            static const std::pair<const char*, int> items[] = { { "PRE", 0 }, { "POST", 1 }, { "BOTH", 2 }, { "DELTA", 3 }, { "FREEZE", 10 }, { "PIANO", 11 } };
            static const float widths[] = { 36, 40, 42, 46, 52, 46 };
            float total = 8;
            for (auto w : widths) total += w + 3;
            float x = (float) getWidth() - total;
            std::vector<std::pair<juce::Rectangle<float>, int>> r;
            for (size_t i = 0; i < 6; ++i)
            {
                if (items[i].second == 10) x += 8;
                r.push_back ({ { x, 5.0f, widths[i], 17.0f }, items[i].second });
                x += widths[i] + 3;
            }
            return r;
        }

        int chipAt (juce::Point<float> p) const
        {
            for (auto& c : chipRects()) if (c.first.contains (p)) return c.second;
            return -1;
        }

        void clickChip (int id)
        {
            if (id < 10) anaMode = id;
            if (id == 10) frozen = ! frozen;
            if (id == 11) { piano = ! piano; layoutPanel(); }
            if (onViewChanged) onViewChanged();
            repaint();
        }

        void drawChips (juce::Graphics& g, const std::vector<std::pair<juce::Rectangle<float>, int>>& chips)
        {
            static const char* labels[] = { "PRE", "POST", "BOTH", "DELTA" };
            g.setFont (font (9.5f, true));
            for (auto& [r, id] : chips)
            {
                const bool on = id < 10 ? anaMode == id : (id == 10 ? frozen : piano);
                g.setColour (on ? col::accent.withAlpha (0.85f) : col::panelHi.withAlpha (0.9f));
                g.fillRoundedRectangle (r, 3.0f);
                g.setColour (on ? col::accent.brighter (0.3f) : col::border);
                g.drawRoundedRectangle (r, 3.0f, 1.0f);
                g.setColour (on ? juce::Colours::white : col::textDim);
                g.drawText (id < 10 ? labels[id] : (id == 10 ? "FREEZE" : "PIANO"), r, juce::Justification::centred);
            }
        }

        // ---- menus --------------------------------------------------------------
        void showMenu (int i, juce::Point<float> pos)
        {
            juce::PopupMenu m;
            if (pro != nullptr)
            {
                m.addItem (10, "Add a band here (or double-click)");
                m.addItem (11, "Add a low cut here");
                m.addItem (12, "Add a high cut here");
                m.addSeparator();
            }
            m.addItem (1, "Reset this band to the analysis", i >= 0 && onPointReset != nullptr);
            m.addItem (2, "Reset all analysis EQ edits", onResetAll != nullptr);
            if (pro != nullptr) m.addItem (3, "Delete all added bands", pro->anyOn());
            juce::Component::SafePointer<SpectrumView> safe (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe, i, pos] (int r)
            {
                if (safe == nullptr) return;
                if (r == 1 && safe->onPointReset) safe->onPointReset (i);
                if (r == 2 && safe->onResetAll)   safe->onResetAll();
                if (r == 3) { for (int k = 0; k < kProBands; ++k) if (safe->pro->band (k).on) safe->pro->deleteBand (k); safe->select (-1); }
                if (r == 10) safe->addBandAt (pos, PBell);
                if (r == 11) safe->addBandAt (pos, PLowCut);
                if (r == 12) safe->addBandAt (pos, PHighCut);
                safe->repaint();
            });
        }

        void showProMenu (int k)
        {
            const auto b = pro->band (k);
            juce::PopupMenu m, types, chans, slopes;
            for (int t = 0; t < NumProTypes; ++t)    types.addItem (100 + t, proTypeName (t), true, b.type == t);
            for (int c = 0; c < NumProChannels; ++c) chans.addItem (200 + c, proChannelName (c), true, b.ch == c);
            for (int s = 0; s < 4; ++s)              slopes.addItem (300 + s, proSlopeName (s), true, b.slope == s);
            m.addSectionHeader ("Band " + juce::String (k + 1));
            m.addSubMenu ("Type", types);
            m.addSubMenu ("Channel", chans);
            if (proIsCut (b.type)) m.addSubMenu ("Slope", slopes);
            m.addSeparator();
            m.addItem (1, "Dynamic EQ", proHasGain (b.type), b.dyn);
            m.addItem (2, "Listen to this band (solo)", true, pro->getSolo() == k);
            m.addItem (3, "Invert gain", proHasGain (b.type));
            m.addItem (4, b.active ? "Disable band" : "Enable band");
            m.addSeparator();
            m.addItem (5, "Delete band");
            juce::Component::SafePointer<SpectrumView> safe (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe, k, b] (int r)
            {
                if (safe == nullptr || r == 0) return;
                auto* p = safe->pro;
                if (r >= 100 && r < 100 + NumProTypes)    p->setNow (k, FType, (float) (r - 100));
                if (r >= 200 && r < 200 + NumProChannels) p->setNow (k, FCh, (float) (r - 200));
                if (r >= 300 && r < 304)                  p->setNow (k, FSlope, (float) (r - 300));
                if (r == 1) p->setNow (k, FDyn, b.dyn ? 0.0f : 1.0f);
                if (r == 2) p->setSolo (p->getSolo() == k ? -1 : k);
                if (r == 3) p->setNow (k, FGain, -b.gainDb);
                if (r == 4) p->setNow (k, FActive, b.active ? 0.0f : 1.0f);
                if (r == 5) safe->deleteBand (k);
                if (safe != nullptr) safe->select (safe->selected);   // rafraîchit le panneau
            });
        }

        // ---- dessin -------------------------------------------------------------
        void drawGlows (juce::Graphics& g)
        {
            if (pro == nullptr) return;
            const auto pl = plot();
            for (int k = 0; k < kProBands; ++k)
            {
                const auto b = pro->band (k);
                if (! b.on || ! b.active) continue;
                const float x = xForFreq (b.freq), w = k == selected ? 46.0f : 32.0f;
                const auto c = proBandColour (k);
                juce::ColourGradient grad (c.withAlpha (0.0f), x - w, 0, c.withAlpha (0.0f), x + w, 0, false);
                grad.addColour (0.5, c.withAlpha (k == selected ? 0.20f : 0.11f));
                g.setGradientFill (grad);
                g.fillRect (juce::Rectangle<float> (x - w, pl.getY() + 2, w * 2, pl.getHeight() - 2));
            }
        }

        void drawPoints (juce::Graphics& g)
        {
            const auto pts = currentPoints();
            if (pts.empty()) return;
            const int active = dragging >= 0 ? dragging : hover;
            for (size_t i = 0; i < pts.size(); ++i)
            {
                const juce::Point<float> c { xForFreq (pts[i].freq), yForGain (juce::jlimit (-rangeDb, rangeDb, pts[i].gainDb)) };
                const bool hot = (int) i == active;
                const float r = hot ? 6.0f : 4.5f;
                g.setColour (col::panel);
                g.fillEllipse (juce::Rectangle<float> (r * 2 + 3, r * 2 + 3).withCentre (c));
                g.setColour (pts[i].edited ? col::warn : col::accent2);
                if (hot) g.fillEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (c));
                else     g.drawEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (c), 2.0f);
            }
            if (active >= 0 && active < (int) pts.size())
            {
                const auto& p = pts[(size_t) active];
                drawLabel (g, { xForFreq (p.freq), yForGain (juce::jlimit (-rangeDb, rangeDb, p.gainDb)) },
                           "Analysis  " + freqText (p.freq) + "  " + (p.gainDb >= 0 ? "+" : "") + juce::String (p.gainDb, 1) + " dB", col::accent2);
            }
        }

        void drawProPoints (juce::Graphics& g)
        {
            if (pro == nullptr) return;
            const int active = dragPro >= 0 ? dragPro : hoverPro;
            for (int k = 0; k < kProBands; ++k)
            {
                const auto b = pro->band (k);
                if (! b.on) continue;
                auto c = proBandColour (k);
                if (! b.active) c = c.withSaturation (0.0f).withAlpha (0.6f);
                const auto p = proPos (b);

                // EQ dynamique : plage visée (anneau) et gain ajouté en ce moment (point)
                if (b.dyn && proHasGain (b.type))
                {
                    const float yT = yForGain (juce::jlimit (-rangeDb, rangeDb, b.gainDb + b.range));
                    juce::Path l; l.startNewSubPath (p); l.lineTo (p.x, yT);
                    juce::Path dashed; const float dash[] = { 3.0f, 3.0f };
                    juce::PathStrokeType (1.2f).createDashedStroke (dashed, l, dash, 2);
                    g.setColour (c.withAlpha (0.7f)); g.fillPath (dashed);
                    g.drawEllipse (juce::Rectangle<float> (9, 9).withCentre ({ p.x, yT }), 1.5f);
                    const float yN = yForGain (juce::jlimit (-rangeDb, rangeDb, b.gainDb + b.dynDb));
                    g.fillRoundedRectangle (juce::Rectangle<float> (14, 3).withCentre ({ p.x, yN }), 1.5f);
                }

                const bool hot = k == active || k == selected;
                const float r = hot ? 8.5f : 7.0f;
                g.setColour (col::bg.withAlpha (0.8f));
                g.fillEllipse (juce::Rectangle<float> (r * 2 + 4, r * 2 + 4).withCentre (p));
                g.setColour (c);
                g.fillEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (p));
                if (k == selected) { g.setColour (juce::Colours::white); g.drawEllipse (juce::Rectangle<float> (r * 2 + 3, r * 2 + 3).withCentre (p), 1.5f); }
                g.setColour (col::bg);
                g.setFont (font (9.5f, true));
                g.drawText (juce::String (k + 1), juce::Rectangle<float> (r * 2, r * 2).withCentre (p), juce::Justification::centred);
                if (pro->getSolo() == k)
                {
                    g.setColour (col::warn);
                    g.drawEllipse (juce::Rectangle<float> (r * 2 + 8, r * 2 + 8).withCentre (p), 1.5f);
                }
            }
            if (active >= 0)
            {
                const auto b = pro->band (active);
                juce::String txt = juce::String (proTypeName (b.type)) + "  " + freqText (b.freq);
                if (proHasGain (b.type)) txt << "  " << (b.gainDb >= 0 ? "+" : "") << juce::String (b.gainDb, 1) << " dB";
                txt << "  Q " << juce::String (b.q, 2);
                if (b.ch != PStereo) txt << "  " << proChannelName (b.ch);
                if (b.dyn && proHasGain (b.type)) txt << "  DYN";
                drawLabel (g, proPos (b), txt, proBandColour (active));
            }
        }

        void drawLabel (juce::Graphics& g, juce::Point<float> at, const juce::String& txt, juce::Colour c)
        {
            const auto b = getLocalBounds().toFloat();
            g.setFont (font (11.0f, true));
            const float w = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), txt) + 18.0f;
            auto r = juce::Rectangle<float> (w, 20.0f).withCentre ({ at.x, at.y < b.getCentreY() ? at.y + 24.0f : at.y - 24.0f });
            r = r.constrainedWithin (b.reduced (4));
            g.setColour (col::panelHi.withAlpha (0.95f));
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (c.withAlpha (0.8f));
            g.drawRoundedRectangle (r, 4.0f, 1.0f);
            g.setColour (col::text);
            g.drawText (txt, r, juce::Justification::centred);
        }

        void drawPiano (juce::Graphics& g)
        {
            const auto b = getLocalBounds().toFloat();
            const auto strip = juce::Rectangle<float> (b.getX() + 2, b.getBottom() - 16, b.getWidth() - 4, 14);
            int hotNote = -1;
            juce::Colour hotCol = col::accent;
            if (pro != nullptr)
            {
                const int k = dragPro >= 0 ? dragPro : (hoverPro >= 0 ? hoverPro : selected);
                if (k >= 0) { hotNote = juce::roundToInt (69.0f + 12.0f * std::log2 (pro->band (k).freq / 440.0f)); hotCol = proBandColour (k); }
            }
            auto noteX = [this] (float midi) { return xForFreq (440.0f * std::pow (2.0f, (midi - 69.0f) / 12.0f)); };
            auto isBlack = [] (int m) { const int n = ((m % 12) + 12) % 12; return n == 1 || n == 3 || n == 6 || n == 8 || n == 10; };

            g.saveState();
            g.reduceClipRegion (strip.toNearestInt());
            g.setColour (juce::Colour (0xffcfd4dc));
            g.fillRect (strip);
            for (int pass = 0; pass < 2; ++pass)
                for (int m = 15; m <= 136; ++m)
                {
                    if (isBlack (m) != (pass == 1)) continue;
                    const float x0 = noteX ((float) m - 0.5f), x1 = noteX ((float) m + 0.5f);
                    if (x1 < strip.getX() || x0 > strip.getRight()) continue;
                    const bool hot = m == hotNote;
                    if (pass == 0)
                    {
                        if (hot) { g.setColour (hotCol); g.fillRect (juce::Rectangle<float> (x0, strip.getY(), x1 - x0, strip.getHeight())); }
                        const int n = ((m % 12) + 12) % 12;
                        if (n == 0 || n == 5) { g.setColour (juce::Colour (0xff6b717c)); g.drawVerticalLine ((int) x0, strip.getY(), strip.getBottom()); }
                        if (n == 0 && x1 - x0 > 3.0f)
                        {
                            g.setColour (juce::Colour (0xff3a3f48));
                            g.setFont (font (7.5f, true));
                            g.drawText ("C" + juce::String (m / 12 - 1), juce::Rectangle<float> (x0 + 1, strip.getBottom() - 8, 16, 8), juce::Justification::centredLeft);
                        }
                    }
                    else
                    {
                        g.setColour (hot ? hotCol : juce::Colour (0xff1a1d22));
                        g.fillRect (juce::Rectangle<float> (x0, strip.getY(), x1 - x0, strip.getHeight() * 0.58f));
                    }
                }
            g.restoreState();
        }

        float yForSpec (float db) const
        {
            const auto b = getLocalBounds().toFloat();
            return b.getY() + 6 + (1.0f - (db + 90.0f) / 90.0f) * (b.getHeight() - 8 - bottomPad());
        }

        void drawSpectrum (juce::Graphics& g, const std::vector<float>& s, juce::Colour c, bool filled)
        {
            if (s.size() < 4) return;
            const auto pl = plot();
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
                juce::Path f (p); f.lineTo (lastX, pl.getBottom()); f.lineTo (xForFreq (20), pl.getBottom()); f.closeSubPath();
                g.setColour (c); g.fillPath (f);
            }
            else
            {
                g.setColour (c); g.strokePath (p, juce::PathStrokeType (1.5f));
            }
        }

        // Différence après - avant (ce que le traitement change), sur l'échelle de l'EQ
        void drawDelta (juce::Graphics& g)
        {
            if (specIn.size() < 4 || specIn.size() != specOut.size()) return;
            const auto b = getLocalBounds().toFloat();
            juce::Path p; bool started = false;
            for (float x = b.getX(); x <= b.getRight(); x += 3.0f)
            {
                const float f0 = freqForX (x - 1.5f), f1 = freqForX (x + 1.5f);
                int k0 = juce::jmax (1, (int) std::floor (f0 * fftSize / sampleRate));
                int k1 = juce::jmin ((int) specIn.size() - 1, (int) std::ceil (f1 * fftSize / sampleRate));
                if (k1 < k0) k1 = k0;
                if (k0 >= (int) specIn.size()) break;
                float d = 0; int cnt = 0;
                for (int k = k0; k <= k1; ++k) { d += specOut[(size_t) k] - specIn[(size_t) k]; ++cnt; }
                d = cnt > 0 ? d / (float) cnt : 0.0f;
                const float y = yForGain (juce::jlimit (-rangeDb, rangeDb, d));
                if (! started) { p.startNewSubPath (x, y); started = true; } else p.lineTo (x, y);
            }
            if (! started) return;
            g.setColour (col::warn.withAlpha (0.9f));
            g.strokePath (p, juce::PathStrokeType (1.4f));
        }

        void drawBands (juce::Graphics& g, const Bands& v, juce::Colour c, bool dashed)
        {
            // affichage relatif centré (forme seulement)
            float mean = 0; for (auto x : v) mean += x; mean /= (float) kBands;
            const auto b = plot();
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

