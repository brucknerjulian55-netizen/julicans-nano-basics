#include "PluginEditor.h"

using namespace nb;
using juce::Colour; using juce::Graphics; using juce::Rectangle; using juce::String;

namespace col
{
const Colour bg (0xff0b0b0e), panel (0xff121216), line (0xff222228), well (0xff15151a), ink (0xfff2f2f5), soft (0xffb8b8c2), dim (0xff9a9aa6),
             acc (0xffc9a6ff), gold (0xfff5d76e);
}
static String U (const char* s) { return String::fromUTF8 (s); }
static juce::Font font (float size, bool bold = false) { return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain)); }

struct LedDef { const char* name; juce::uint32 c; };
static const LedDef leds[8] = { { "Rot (Original)", 0xffff2a1a }, { "Lila", 0xffb04dff }, { "Cyan", 0xff00e8ff }, { "Grün", 0xff4dff6a },
                                { "Orange", 0xffff8a1a }, { "Pink", 0xffff2e9a }, { "Weiß", 0xfff2f2f5 }, { "Gold", 0xffffc23d } };
static Colour ledColour (const NanoProcessor& p) { return Colour (leds[juce::jlimit (0, 7, p.ledColour)].c); }

static void drawPanel (Graphics& g, Rectangle<float> r)
{
    g.setColour (col::panel); g.fillRoundedRectangle (r, 14.0f);
    g.setColour (col::line); g.drawRoundedRectangle (r.reduced (0.5f), 14.0f, 1.0f);
}
static void pill (Graphics& g, Rectangle<float> r, const String& t, bool on, float size = 13.0f)
{
    g.setColour (on ? col::ink : col::well); g.fillRoundedRectangle (r, 8.0f);
    if (! on) { g.setColour (Colour (0xff2c2c33)); g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f); }
    g.setColour (on ? col::bg : Colour (0xffc9c9d1)); g.setFont (font (size, on));
    g.drawFittedText (t, r.toNearestInt().reduced (6, 0), juce::Justification::centred, 1, 0.8f);
}

// ---------------- Der Controller -------------------------------------------
class KorgView : public juce::Component, public juce::SettableTooltipClient
{
public:
    static constexpr float S = 3.2f;
    static constexpr int BW = 520, BH = 133;

    explicit KorgView (NanoEditor::Ui& u) : ui (u), skinImg (juce::Image::ARGB, BW, BH, true)
    {
        juce::Random r (77);
        for (int i = 0; i < 90; ++i) stars.push_back ({ r.nextFloat() * 320.0f - 160.0f, r.nextFloat() * 79.0f - 39.5f, r.nextFloat() < 0.15f ? 2.2f : 1.3f, r.nextFloat() * 6.28f });
        setSkin();
    }
    std::function<void (int)> onClick;

    juce::Point<float> at (float xmm, float ymm) const { return { body.getX() + (xmm + 162.5f) * S, body.getY() + (41.5f - ymm) * S }; }
    Rectangle<float> rectOf (int i) const
    {
        const auto& c = controls()[(size_t) i];
        const auto m = at (c.x, c.y);
        return Rectangle<float> (c.w * S, c.h * S).withCentre (m);
    }
    const Skin& skin() const { return skins()[(size_t) skinIndex (ui.p.skin.toStdString())]; }

    void setSkin()
    {
        const auto& s = skin();
        fieldA = makeField (s, BW, BH, 0);
        fieldB = s.reactive ? makeField (s, BW, BH, 1) : fieldA;
        renderNow();
    }
    void renderNow()
    {
        const auto& s = skin();
        std::vector<uint32_t> px ((size_t) (BW * BH));
        Motion still; still.time = ui.motion.time;
        renderSkin (s, fieldA, fieldB, s.reactive && ui.p.reactive && ! ui.p.ecoMode ? ui.motion : still, px.data());
        juce::Image::BitmapData bd (skinImg, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < BH; ++y)
        {
            auto* line = bd.getLinePointer (y);
            for (int x = 0; x < BW; ++x) reinterpret_cast<juce::PixelARGB*> (line + x * bd.pixelStride)->set (juce::PixelARGB (px[(size_t) (y * BW + x)] >> 24, (px[(size_t) (y * BW + x)] >> 16) & 255, (px[(size_t) (y * BW + x)] >> 8) & 255, px[(size_t) (y * BW + x)] & 255));
        }
    }
    bool animated() const { return skin().reactive && ui.p.reactive && ! ui.p.ecoMode; }

    void resized() override { body = Rectangle<float> (325.0f * S, 83.0f * S).withCentre (getLocalBounds().toFloat().getCentre().translated (0, -6.0f)); }

    void paint (Graphics& g) override
    {
        auto& p = ui.p;
        const Colour led = ledColour (p);
        // Gehaeuse mit Kante und Schatten
        {
            juce::Path sh; sh.addRoundedRectangle (body.translated (0, 14.0f), 18.0f);
            juce::DropShadow (Colour (0x99000000), 34, { 0, 18 }).drawForPath (g, sh);
            g.setColour (Colour (0xff060607)); g.fillRoundedRectangle (body.withHeight (body.getHeight() + 12.0f), 18.0f);
            g.setColour (Colour (0xff1a1a1e)); g.drawRoundedRectangle (body.withHeight (body.getHeight() + 12.0f).reduced (0.5f), 18.0f, 1.0f);
        }
        {
            Graphics::ScopedSaveState ss (g);
            juce::Path clip; clip.addRoundedRectangle (body, 16.0f); g.reduceClipRegion (clip);
            g.setImageResamplingQuality (Graphics::highResamplingQuality);
            g.drawImage (skinImg, body);
            if (skin().stars)
            {
                const float t = ui.motion.time, lv = ui.motion.level;
                for (const auto& s : stars)
                {
                    const auto c = at (s.x, s.y);
                    const float a = juce::jlimit (0.0f, 1.0f, 0.35f + 0.35f * std::sin (t * 1.7f + s.phase) + 0.5f * lv + 0.6f * ui.motion.pulse);
                    g.setColour (Colour (0xffffffff).withAlpha (a)); g.fillEllipse (c.x - s.r * 0.5f, c.y - s.r * 0.5f, s.r, s.r);
                }
            }
            // Licht von oben, Schatten unten: macht die flache Haut plastisch
            g.setGradientFill (juce::ColourGradient (Colour (0x16ffffff), body.getX(), body.getY(), Colour (0x2e000000), body.getX(), body.getBottom(), false));
            g.fillRect (body);
            g.setColour (Colour (0x22ffffff)); g.drawRoundedRectangle (body.reduced (1.0f), 16.0f, 1.5f);
        }
        // Beschriftung
        for (const auto& t : prints())
        {
            const auto c = at (t.x, t.y);
            g.setColour (Colour (0xd9f2f2f5)); g.setFont (font (t.size * S, t.bold));
            const Rectangle<float> r = t.centred ? Rectangle<float> (200.0f, t.size * S * 1.4f).withCentre (c) : Rectangle<float> (c.x, c.y - t.size * S * 0.7f, 300.0f, t.size * S * 1.4f);
            g.drawText (U (t.text), r, t.centred ? juce::Justification::centred : juce::Justification::centredLeft, false);
        }
        // Regler
        const int pre = p.preset();
        for (int i = 0; i < NUM_CTL; ++i)
        {
            const auto& c = controls()[(size_t) i];
            const auto r = rectOf (i);
            const float v = p.valueFor (i);
            if (c.kind == KNOB)
            {
                g.setColour (Colour (0xff050506)); g.fillEllipse (r.translated (0, 4.0f));
                g.setGradientFill (juce::ColourGradient (Colour (0xff3a3a40), r.getX() + r.getWidth() * 0.4f, r.getY() + r.getHeight() * 0.35f, Colour (0xff16161a), r.getRight(), r.getBottom(), true));
                g.fillEllipse (r);
                g.setColour (Colour (0x1fffffff)); g.drawEllipse (r.reduced (0.5f), 1.0f);
                juce::Path arc; arc.addCentredArc (r.getCentreX(), r.getCentreY(), r.getWidth() * 0.5f + 5.0f, r.getWidth() * 0.5f + 5.0f, 0, -2.36f, -2.36f + 4.71f * v, true);
                g.setColour (led.withAlpha (0.85f)); g.strokePath (arc, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                const float ang = -2.36f + 4.71f * v;
                const auto m = r.getCentre(); const float len = r.getWidth() * 0.42f;
                g.setColour (col::ink);
                g.drawLine (m.x + std::sin (ang) * len * 0.2f, m.y - std::cos (ang) * len * 0.2f, m.x + std::sin (ang) * len, m.y - std::cos (ang) * len, 3.0f);
            }
            else if (c.kind == FADER)
            {
                const auto slot = r.withSizeKeepingCentre (8.0f, r.getHeight());
                g.setColour (Colour (0xff020203)); g.fillRoundedRectangle (slot, 4.0f);
                const float capH = 19.0f, top = r.getY() + (1.0f - v) * (r.getHeight() - capH);
                const auto cap = Rectangle<float> (r.getX(), top, r.getWidth(), capH);
                g.setColour (Colour (0xff050506)); g.fillRoundedRectangle (cap.translated (0, 3.0f), 4.0f);
                g.setGradientFill (juce::ColourGradient (Colour (0xff3a3a40), cap.getX(), cap.getY(), Colour (0xff1a1a1e), cap.getX(), cap.getBottom(), false));
                g.fillRoundedRectangle (cap, 4.0f);
                g.setColour (col::ink); g.fillRect (cap.withSizeKeepingCentre (cap.getWidth() - 2.0f, 2.0f));
            }
            else
            {
                const bool lit = c.led && ((i >= 0 && String (c.id) == "s" + String (pre + 1)) || p.held[(size_t) i].load() || (String (c.id) == "play" && p.playing.load()));
                g.setColour (Colour (0xff050506)); g.fillRoundedRectangle (r.translated (0, 3.0f), 5.0f);
                if (lit)
                {
                    for (int k = 4; k >= 1; --k) { g.setColour (led.withAlpha (0.07f * (float) k)); g.fillRoundedRectangle (r.expanded ((float) (5 - k) * 3.5f), 8.0f); }
                    g.setColour (led); g.fillRoundedRectangle (r, 5.0f);
                    g.setColour (Colour (0x80ffffff)); g.fillRoundedRectangle (r.withHeight (2.0f).reduced (3.0f, 0).translated (0, 1.0f), 1.0f);
                }
                else
                {
                    g.setGradientFill (juce::ColourGradient (Colour (0xff2d2d33), r.getX(), r.getY(), Colour (0xff1d1d22), r.getX(), r.getBottom(), false));
                    g.fillRoundedRectangle (r, 5.0f);
                    g.setColour (Colour (0x1affffff)); g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
                }
            }
            // Markierungen
            const bool locked = p.isLocked (i);
            if (c.reserved && ui.mode == 1)
            {
                g.setColour (col::dim.withAlpha (0.8f)); g.drawRoundedRectangle (r.expanded (4.0f), 6.0f, 1.0f);
            }
            if (locked)
            {
                if (ui.mode == 1)
                {
                    juce::Path d; d.addRoundedRectangle (r.expanded (4.0f), 6.0f);
                    juce::Path dashed; const float dash[] = { 4.0f, 3.0f };
                    juce::PathStrokeType (2.0f).createDashedStroke (dashed, d, dash, 2);
                    g.setColour (col::gold); g.fillPath (dashed);
                }
                const auto b = Rectangle<float> (16.0f, 16.0f).withCentre ({ r.getRight() + 2.0f, r.getY() - 2.0f });
                g.setColour (col::gold); g.fillEllipse (b);
                g.setColour (col::bg);
                g.fillRoundedRectangle (b.withSizeKeepingCentre (8.0f, 6.0f).translated (0, 1.5f), 1.0f);
                g.drawEllipse (b.withSizeKeepingCentre (5.0f, 6.0f).translated (0, -1.5f), 1.4f);
            }
            if (c.kind == BUTTON && p.sampleAt (p.bankFor (i), i) != nullptr)
            {
                const auto d = Rectangle<float> (7.0f, 7.0f).withCentre ({ r.getX() + 1.0f, r.getY() + 1.0f });
                g.setColour (col::bg); g.fillEllipse (d.expanded (1.5f));
                g.setColour (col::acc); g.fillEllipse (d);
            }
            if (dropTarget == i) { g.setColour (col::acc); g.drawRoundedRectangle (r.expanded (6.0f), 7.0f, 2.5f); }
            if (ui.selected == i) { g.setColour (col::ink); g.drawRoundedRectangle (r.expanded (5.0f), 7.0f, 2.0f); }
        }
        if (p.learning())
        {
            const auto bar = Rectangle<float> (body.getCentreX() - 300.0f, body.getBottom() + 18.0f, 600.0f, 26.0f);
            g.setColour (col::acc); g.fillRoundedRectangle (bar, 8.0f);
            g.setColour (col::bg); g.setFont (font (13.0f, true));
            g.drawText (U ("Lernen: Erst in FL einen Regler mit der Maus bewegen, dann den Regler am Korg"), bar, juce::Justification::centred, false);
        }
        // Preset-Wechsel: kurzer Blitz
        if (ui.motion.flash > 0.01f) { g.setColour (led.withAlpha (0.18f * ui.motion.flash)); g.fillRoundedRectangle (body, 16.0f); }
    }

    int hit (juce::Point<float> pt) const
    {
        for (int i = 0; i < NUM_CTL; ++i) if (rectOf (i).expanded (5.0f).contains (pt)) return i;
        return -1;
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const int i = hit (e.position);
        if (i >= 0 && onClick) onClick (i);
    }
    void mouseMove (const juce::MouseEvent& e) override
    {
        const int i = hit (e.position);
        setMouseCursor (i >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        setTooltip (i >= 0 ? U (controls()[(size_t) i].label) : String());
    }

    NanoEditor::Ui& ui;
    Rectangle<float> body;
    int dropTarget = -1;
private:
    struct Star { float x, y, r, phase; };
    juce::Image skinImg; Field fieldA, fieldB; std::vector<Star> stars;
};

// ---------------- Presetleiste ---------------------------------------------
class PresetBar : public juce::Component
{
public:
    explicit PresetBar (NanoEditor::Ui& u) : ui (u) {}
    Rectangle<float> cell (int i) const
    {
        const float w = ((float) getWidth() - 7.0f * 8.0f) / 8.0f;
        return { (float) i * (w + 8.0f), 0.0f, w, (float) getHeight() };
    }
    void paint (Graphics& g) override
    {
        const int cur = ui.p.preset();
        for (int i = 0; i < NUM_PRESETS; ++i)
        {
            const auto r = cell (i); const bool on = i == cur;
            g.setColour (on ? col::acc : col::well); g.fillRoundedRectangle (r, 10.0f);
            if (! on) { g.setColour (Colour (0xff2c2c33)); g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f); }
            g.setColour (on ? col::bg.withAlpha (0.7f) : col::dim); g.setFont (font (11.5f));
            g.drawText ("Preset " + String (i + 1), r.reduced (12.0f, 6.0f).removeFromTop (16.0f), juce::Justification::centredLeft, false);
            g.setColour (on ? col::bg : Colour (0xffd8d8de)); g.setFont (font (14.5f, true));
            g.drawFittedText (ui.p.presetNames[(size_t) i], r.reduced (12.0f, 6.0f).withTrimmedTop (17.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
        }
    }
    int at (juce::Point<int> pt) const { for (int i = 0; i < NUM_PRESETS; ++i) if (cell (i).contains (pt.toFloat())) return i; return -1; }
    void mouseDown (const juce::MouseEvent& e) override { const int i = at (e.getPosition()); if (i >= 0) ui.p.setPreset (i); }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int i = at (e.getPosition()); if (i < 0) return;
        auto* w = new juce::AlertWindow (U ("Preset umbenennen"), U ("Neuer Name für Preset ") + String (i + 1), juce::MessageBoxIconType::NoIcon, this);
        w->addTextEditor ("name", ui.p.presetNames[(size_t) i]);
        w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey)); w->addButton ("Abbrechen", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        w->enterModalState (true, juce::ModalCallbackFunction::create ([sp = SafePointer<PresetBar> (this), w, i] (int r)
        {
            std::unique_ptr<juce::AlertWindow> owner (w);
            if (sp == nullptr || r != 1) return;
            const auto t = w->getTextEditorContents ("name").trim().substring (0, 24);
            if (t.isNotEmpty()) sp->ui.p.presetNames[(size_t) i] = t;
            sp->repaint();
        }), false);
    }
    NanoEditor::Ui& ui;
};

// ---------------- Design-Auswahl -------------------------------------------
class DesignPanel : public juce::Component
{
public:
    explicit DesignPanel (NanoEditor::Ui& u) : ui (u)
    {
        for (const auto& s : skins())
        {
            const auto f = makeField (s, 130, 33, 0);
            juce::Image img (juce::Image::RGB, 130, 33, false);
            for (int y = 0; y < 33; ++y) for (int x = 0; x < 130; ++x) img.setPixelAt (x, y, Colour (mapColour (s, f.v[(size_t) (y * 130 + x)])));
            thumbs.push_back (img);
        }
    }
    std::vector<int> visible() const { std::vector<int> v; for (int i = 0; i < (int) skins().size(); ++i) if (skins()[(size_t) i].group == ui.category) v.push_back (i); return v; }
    Rectangle<float> tab (int i) const { return { 16.0f + (float) i * 128.0f, 48.0f, 120.0f, 34.0f }; }
    Rectangle<float> tile (int k) const
    {
        const float w = ((float) getWidth() - 32.0f - 3.0f * 10.0f) / 4.0f;
        return { 16.0f + (float) (k % 4) * (w + 10.0f), 96.0f + (float) (k / 4) * 62.0f, w, 56.0f };
    }
    void paint (Graphics& g) override
    {
        drawPanel (g, getLocalBounds().toFloat());
        g.setColour (col::ink); g.setFont (font (15.0f, true)); g.drawText ("Design", 16, 14, 200, 22, juce::Justification::centredLeft, false);
        const auto& cur = skins()[(size_t) skinIndex (ui.p.skin.toStdString())];
        g.setColour (cur.reactive ? col::acc : col::dim); g.setFont (font (12.0f));
        g.drawText (cur.reactive ? U ("Reaktiv: bewegt sich mit der Musik") : U ("Statisch: kostet keine Leistung"), getWidth() - 316, 14, 300, 22, juce::Justification::centredRight, false);
        for (int i = 0; i < NUM_GROUPS; ++i) pill (g, tab (i), U (groupName (i)), i == ui.category);
        const auto v = visible();
        for (int k = 0; k < (int) v.size(); ++k)
        {
            const auto r = tile (k); const auto& s = skins()[(size_t) v[(size_t) k]];
            const auto pic = r.withHeight (r.getWidth() * 33.0f / 130.0f > 34.0f ? 34.0f : r.getWidth() * 33.0f / 130.0f);
            {
                Graphics::ScopedSaveState ss (g);
                juce::Path cl; cl.addRoundedRectangle (pic, 6.0f); g.reduceClipRegion (cl);
                g.drawImage (thumbs[(size_t) v[(size_t) k]], pic, juce::RectanglePlacement::fillDestination);
            }
            if (String (s.key) == ui.p.skin) { g.setColour (col::ink); g.drawRoundedRectangle (pic.expanded (2.0f), 7.0f, 2.0f); }
            g.setColour (col::soft); g.setFont (font (11.5f));
            g.drawText (U (s.name) + (s.reactive ? U ("  ·  reaktiv") : String()), r.withTrimmedTop (pic.getHeight() + 3.0f).toNearestInt(), juce::Justification::topLeft, true);
        }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int i = 0; i < NUM_GROUPS; ++i) if (tab (i).contains (e.position)) { ui.category = i; repaint(); return; }
        const auto v = visible();
        for (int k = 0; k < (int) v.size(); ++k)
            if (tile (k).contains (e.position)) { ui.p.skin = skins()[(size_t) v[(size_t) k]].key; if (ui.skinChanged) ui.skinChanged(); repaint(); return; }
    }
    NanoEditor::Ui& ui;
    std::vector<juce::Image> thumbs;
};

// ---------------- LEDs und Bewegung ----------------------------------------
class LedPanel : public juce::Component
{
public:
    explicit LedPanel (NanoEditor::Ui& u) : ui (u) {}
    Rectangle<float> swatch (int i) const { return { 16.0f + (float) (i % 4) * 44.0f, 48.0f + (float) (i / 4) * 44.0f, 32.0f, 32.0f }; }
    Rectangle<float> toggle (int i) const { return { 16.0f, 202.0f + (float) i * 32.0f, (float) getWidth() - 32.0f, 26.0f }; }
    void paint (Graphics& g) override
    {
        drawPanel (g, getLocalBounds().toFloat());
        g.setColour (col::ink); g.setFont (font (15.0f, true)); g.drawText ("LED-Farbe", 16, 14, 200, 22, juce::Justification::centredLeft, false);
        for (int i = 0; i < 8; ++i)
        {
            const auto r = swatch (i);
            g.setColour (Colour (leds[i].c)); g.fillEllipse (r);
            g.setColour (i == ui.p.ledColour ? col::ink : Colour (0xff2c2c33)); g.drawEllipse (r.expanded (2.0f), 2.0f);
        }
        g.setColour (col::dim); g.setFont (font (12.0f));
        g.drawFittedText (U (leds[juce::jlimit (0, 7, ui.p.ledColour)].name) + U (" gilt nur im Plugin. Die LEDs am Korg bleiben rot, das ist fest verbaut."),
                          16, 140, getWidth() - 32, 54, juce::Justification::topLeft, 3, 0.9f);
        const String names[3] = { U ("Reagiert auf Musik"), U ("Takt-Vorschau 180 BPM"), U ("Sparmodus") };
        const bool on[3] = { ui.p.reactive, ui.preview, ui.p.ecoMode };
        for (int i = 0; i < 3; ++i)
        {
            const auto r = toggle (i);
            const auto box = r.withWidth (18.0f).withSizeKeepingCentre (18.0f, 18.0f);
            g.setColour (on[i] ? col::ink : col::well); g.fillRoundedRectangle (box, 4.0f);
            g.setColour (Colour (0xff3a3a44)); g.drawRoundedRectangle (box, 4.0f, 1.0f);
            if (on[i]) { g.setColour (col::bg); juce::Path t; t.startNewSubPath (box.getX() + 4, box.getCentreY()); t.lineTo (box.getX() + 8, box.getBottom() - 5); t.lineTo (box.getRight() - 4, box.getY() + 5); g.strokePath (t, juce::PathStrokeType (2.0f)); }
            g.setColour (Colour (0xffd8d8de)); g.setFont (font (13.0f));
            g.drawText (names[i], r.withTrimmedLeft (28.0f), juce::Justification::centredLeft, false);
        }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int i = 0; i < 8; ++i) if (swatch (i).expanded (4.0f).contains (e.position)) { ui.p.ledColour = i; if (ui.refresh) ui.refresh(); return; }
        if (toggle (0).contains (e.position)) ui.p.reactive = ! ui.p.reactive;
        else if (toggle (1).contains (e.position)) ui.preview = ! ui.preview;
        else if (toggle (2).contains (e.position)) ui.p.ecoMode = ! ui.p.ecoMode;
        else return;
        if (ui.skinChanged) ui.skinChanged();
    }
    NanoEditor::Ui& ui;
};

// ---------------- Bearbeiten ------------------------------------------------
class EditPanel : public juce::Component
{
public:
    explicit EditPanel (NanoEditor::Ui& u) : ui (u) {}
    std::function<void (int)> chooseSample;
    Rectangle<float> seg (int i) const { return { 16.0f + (float) i * 112.0f, 48.0f, 104.0f + (i == 1 ? 20.0f : 0.0f), 34.0f }; }
    Rectangle<float> button (int row, int i) const { const float w = ((float) getWidth() - 32.0f - 8.0f) / 2.0f; return { 16.0f + (float) i * (w + 8.0f), (float) getHeight() - (row == 0 ? 92.0f : 50.0f), w, 34.0f }; }
    bool isButton() const { return ui.selected >= 0 && controls()[(size_t) ui.selected].kind == BUTTON && ! controls()[(size_t) ui.selected].reserved; }
    void paint (Graphics& g) override
    {
        drawPanel (g, getLocalBounds().toFloat());
        auto& p = ui.p;
        g.setColour (col::ink); g.setFont (font (15.0f, true)); g.drawText ("Bearbeiten", 16, 14, 200, 22, juce::Justification::centredLeft, false);
        pill (g, seg (0), "Belegen", ui.mode == 0); pill (g, seg (1), "Fest markieren", ui.mode == 1);
        auto box = Rectangle<float> (16.0f, 94.0f, (float) getWidth() - 32.0f, (float) getHeight() - 200.0f);
        g.setColour (col::bg); g.fillRoundedRectangle (box, 10.0f);
        box.reduce (12.0f, 10.0f);
        String head, text;
        const int i = ui.selected;
        if (ui.mode == 1)
        {
            head = U ("Fest markieren");
            text = U ("Klick auf einen Regler schaltet fest an oder aus. Feste Regler machen in allen Presets dasselbe, zum Beispiel deine Effekte. Sie verknüpfst du in FL ganz normal mit Link to controller.");
        }
        else if (i < 0) { head = U ("Kein Regler gewählt"); text = U ("Klick einen Regler an. Auf Tasten kannst du ein Sample aus dem FL-Browser ziehen. Doppelklick auf ein Preset benennt es um."); }
        else
        {
            const auto& c = controls()[(size_t) i];
            const bool locked = p.isLocked (i);
            head = U (c.label) + (c.reserved ? U (" · schaltet Presets") : locked ? U (" · fest") : U (" · Preset ") + String (p.preset() + 1));
            if (c.reserved) text = U ("Track zurück und vor wechseln das Preset. Die S-Taste mit der Nummer des Presets leuchtet am Korg.");
            else if (const auto* smp = p.sampleAt (p.bankFor (i), i))
            {
                const int note = triggerNote (i);
                text = U ("Sound: ") + smp->name + U ("\n") + smp->path + U ("\nIm Piano Roll auf ") + juce::MidiMessage::getMidiNoteName (note, true, true, 3) + U (" spielbar.");
            }
            else if (locked) text = U ("Fest: In FL Rechtsklick auf den Zielregler, Link to controller, dann diesen Regler bewegen.");
            else text = U ("In diesem Preset frei. Verknüpfen: Lernen drücken, in FL den Zielregler mit der Maus bewegen, dann diesen Regler am Korg.")
                        + (c.kind == BUTTON ? U (" Oder ein Sample auf die Taste ziehen.") : String());
        }
        g.setColour (col::ink); g.setFont (font (14.0f, true)); g.drawText (head, box.removeFromTop (22.0f), juce::Justification::centredLeft, true);
        g.setColour (col::soft); g.setFont (font (12.5f)); g.drawFittedText (text, box.toNearestInt(), juce::Justification::topLeft, 6, 0.9f);

        if (ui.mode == 1) { pill (g, button (0, 0), U ("Alle lösen"), false, 12.5f); }
        else if (isButton())
        {
            const bool has = p.sampleAt (p.bankFor (i), i) != nullptr;
            pill (g, button (0, 0), has ? U ("Sample tauschen") : U ("Sample wählen"), false, 12.5f);
            if (has) pill (g, button (0, 1), U ("Entfernen"), false, 12.5f);
        }
        pill (g, button (1, 0), p.learning() ? U ("Lernen läuft") : U ("Lernen"), p.learning(), 12.5f);
        pill (g, button (1, 1), U ("Einrichtung"), false, 12.5f);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        auto& p = ui.p;
        if (seg (0).contains (e.position)) ui.mode = 0;
        else if (seg (1).contains (e.position)) ui.mode = 1;
        else if (button (1, 0).contains (e.position)) p.setLearning (! p.learning());
        else if (button (1, 1).contains (e.position)) { showSetup(); return; }
        else if (button (0, 0).contains (e.position))
        {
            if (ui.mode == 1) { for (int i = 0; i < NUM_CTL; ++i) if (! controls()[(size_t) i].reserved) p.setLocked (i, false); }
            else if (isButton() && chooseSample) { chooseSample (ui.selected); return; }
            else return;
        }
        else if (button (0, 1).contains (e.position) && ui.mode == 0 && isButton()) p.clearSample (p.bankFor (ui.selected), ui.selected);
        else return;
        if (ui.refresh) ui.refresh();
    }
    void showSetup()
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, U ("Einrichtung in FL Studio"),
            U ("1. Korg Kontrol Editor: CC-Modus, LED Mode External.\n"
               "2. FL: Optionen > MIDI. Beim nanoKONTROL2 unter Eingang den Controller-Typ \"julicans nano basics\" wählen und einen Port setzen, zum Beispiel 10. Unter Ausgang den nanoKONTROL2 auf denselben Port.\n"
               "3. Dieses Plugin als Instrument in den Channel Rack laden. Das Skript findet es von selbst.\n\n"
               "Freie Regler verknüpfen: Lernen drücken, in FL den Zielregler mit der Maus bewegen, dann den Regler am Korg. Das gilt nur für das aktuelle Preset.\n"
               "Feste Regler: wie gewohnt mit Rechtsklick > Link to controller.\n"
               "Sounds: Sample aus dem FL-Browser auf eine Taste ziehen. Gespeichert wird nur der Pfad im Sample-Ordner."), "OK", this);
    }
    NanoEditor::Ui& ui;
};

// ---------------- Kopfzeile -------------------------------------------------
class Header : public juce::Component
{
public:
    explicit Header (NanoEditor::Ui& u) : ui (u) {}
    std::function<void (int)> onButton;
    Rectangle<float> button (int i) const { static const float w[3] = { 104.0f, 92.0f, 132.0f }; float x = (float) getWidth(); for (int k = 2; k >= i; --k) x -= w[k] + 8.0f; return { x + 8.0f, 6.0f, w[i], 34.0f }; }
    void mouseDown (const juce::MouseEvent& e) override { for (int i = 0; i < 3; ++i) if (button (i).contains (e.position) && onButton) onButton (i); }
    void paint (Graphics& g) override
    {
        pill (g, button (0), U ("Speichern"), false, 12.5f); pill (g, button (1), U ("Laden"), false, 12.5f); pill (g, button (2), U ("Sample-Ordner"), false, 12.5f);
        const double since = juce::Time::getMillisecondCounterHiRes() - ui.p.lastMidiTime.load();
        const bool live = ui.p.lastMidiTime.load() > 0 && since < 4000.0;
        g.setColour (live ? Colour (0xff4dff6a) : col::dim); g.fillEllipse (button (0).getX() - 150.0f, 18.0f, 9.0f, 9.0f);
        g.setColour (col::dim); g.setFont (font (12.0f));
        g.drawText (live ? U ("Korg sendet") : ui.p.lastMidiTime.load() > 0 ? U ("Korg verbunden") : U ("Noch keine Daten"), (int) button (0).getX() - 136, 12, 128, 20, juce::Justification::centredLeft, false);
        g.setColour (col::ink); g.setFont (font (26.0f, true));
        g.drawText ("julicans nano basics", 0, 0, 400, 32, juce::Justification::centredLeft, false);
        int fixed = 0, free = 0;
        for (int i = 0; i < NUM_CTL; ++i) { if (controls()[(size_t) i].reserved) continue; (ui.p.isLocked (i) ? fixed : free)++; }
        const int p = ui.p.preset();
        g.setColour (col::dim); g.setFont (font (13.0f));
        g.drawText (U ("Preset ") + String (p + 1) + U (" von 8 · ") + ui.p.presetNames[(size_t) p] + U (" · ") + String (free) + U (" Regler wechseln, ") + String (fixed) + U (" fest"),
                    0, 34, 480, 18, juce::Justification::centredLeft, false);
    }
    NanoEditor::Ui& ui;
};

// ---------------- Gesamtfenster ---------------------------------------------
class Root : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    explicit Root (NanoEditor::Ui& u) : ui (u), header (u), presets (u), korg (u), design (u), led (u), edit (u),
        update ("brucknerjulian55-netizen/julicans-nano-basics", JucePlugin_VersionString)
    {
        for (auto* c : std::initializer_list<juce::Component*> { &header, &presets, &korg, &design, &led, &edit }) addAndMakeVisible (c);
        update.fill = [] { return col::acc; }; update.text = [] { return col::bg; };
        addChildComponent (update);
        header.onButton = [this] (int b) { b == 0 ? saveSet() : b == 1 ? loadSet() : chooseRoot(); };
        edit.chooseSample = [this] (int ctl) { chooseSample (ctl); };
        korg.onClick = [this] (int i)
        {
            const auto& c = controls()[(size_t) i];
            if (ui.mode == 1 && ! c.reserved) ui.p.setLocked (i, ! ui.p.isLocked (i));
            ui.selected = i;
            repaintAll();
        };
        setSize (1280, 820);
    }
    void paint (Graphics& g) override { g.fillAll (col::bg); }
    static bool isAudio (const String& f) { return f.endsWithIgnoreCase (".wav") || f.endsWithIgnoreCase (".aif") || f.endsWithIgnoreCase (".aiff") || f.endsWithIgnoreCase (".flac") || f.endsWithIgnoreCase (".ogg") || f.endsWithIgnoreCase (".mp3"); }
    bool isInterestedInFileDrag (const juce::StringArray& f) override { return f.size() > 0 && isAudio (f[0]); }
    int buttonAt (int x, int y) const
    {
        const int i = korg.hit (korg.getLocalPoint (this, juce::Point<int> (x, y)).toFloat());
        return i >= 0 && controls()[(size_t) i].kind == BUTTON && ! controls()[(size_t) i].reserved ? i : -1;
    }
    void fileDragMove (const juce::StringArray&, int x, int y) override { const int i = buttonAt (x, y); if (i != korg.dropTarget) { korg.dropTarget = i; korg.repaint(); } }
    void fileDragExit (const juce::StringArray&) override { korg.dropTarget = -1; korg.repaint(); }
    void filesDropped (const juce::StringArray& files, int x, int y) override
    {
        korg.dropTarget = -1;
        const int i = buttonAt (x, y);
        if (i < 0) { message (U ("Zieh das Sample direkt auf eine Taste: S, M, R, Cycle, Marker oder Transport.")); return; }
        assign (i, juce::File (files[0]));
    }
    void assign (int ctl, const juce::File& f)
    {
        String err;
        if (! ui.p.loadSample (ui.p.bankFor (ctl), ctl, f, err)) message (err);
        else if (! ui.p.sampleRoot.isDirectory()) message (U ("Gespeichert wird der volle Pfad. Wähle oben den Sample-Ordner, dann findet das Plugin die Sounds auch, wenn du sie innerhalb des Ordners verschiebst."));
        ui.selected = ctl; ui.mode = 0;
        repaintAll();
    }
    void message (const String& t) { juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "julicans nano basics", t, "OK", this); }
    void chooseSample (int ctl)
    {
        chooser = std::make_unique<juce::FileChooser> (U ("Sample für ") + U (controls()[(size_t) ctl].label), ui.p.sampleRoot, "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [sp = SafePointer<Root> (this), ctl] (const juce::FileChooser& c)
        { if (sp != nullptr && c.getResult().existsAsFile()) sp->assign (ctl, c.getResult()); });
    }
    void chooseRoot()
    {
        chooser = std::make_unique<juce::FileChooser> (U ("Sample-Ordner wählen, zum Beispiel deinen FL-Browser-Ordner"), ui.p.sampleRoot);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [sp = SafePointer<Root> (this)] (const juce::FileChooser& c)
        {
            if (sp == nullptr || ! c.getResult().isDirectory()) return;
            sp->ui.p.setSampleRoot (c.getResult());
            sp->message (U ("Sample-Ordner: ") + c.getResult().getFullPathName() + U ("\nNeue Sounds werden ab jetzt relativ zu diesem Ordner gespeichert."));
        });
    }
    void saveSet()
    {
        chooser = std::make_unique<juce::FileChooser> (U ("Presets speichern"), ui.p.setFolder().getChildFile (ui.p.presetNames[0] + ".nano"), "*.nano");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [sp = SafePointer<Root> (this)] (const juce::FileChooser& c)
        {
            if (sp == nullptr || c.getResult() == juce::File()) return;
            const auto f = c.getResult().withFileExtension ("nano");
            sp->message (sp->ui.p.saveSet (f) ? U ("Gespeichert: ") + f.getFileName() + U ("\nAlle 8 Presets mit Namen, festen Reglern und den Pfaden der Sounds.") : U ("Speichern ist fehlgeschlagen."));
        });
    }
    void loadSet()
    {
        chooser = std::make_unique<juce::FileChooser> (U ("Presets laden"), ui.p.setFolder(), "*.nano");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [sp = SafePointer<Root> (this)] (const juce::FileChooser& c)
        {
            if (sp == nullptr || ! c.getResult().existsAsFile()) return;
            if (! sp->ui.p.loadSet (c.getResult())) { sp->message (U ("Das ist keine Preset-Datei von julicans nano basics.")); return; }
            if (sp->ui.skinChanged) sp->ui.skinChanged();
            sp->reportMissing();
        });
    }
    void reportMissing()
    {
        if (ui.p.missing.isEmpty()) return;
        juce::StringArray m (ui.p.missing); while (m.size() > 8) m.remove (8);
        message (String (ui.p.missing.size()) + U (" Sound(s) nicht gefunden. Prüf den Sample-Ordner:\n") + m.joinIntoString ("\n"));
    }
    void resized() override
    {
        header.setBounds (32, 24, 1216, 56);
        update.setBounds (540, 30, 200, 26);
        presets.setBounds (32, 90, 1216, 50);
        korg.setBounds (32, 150, 1216, 312);
        design.setBounds (32, 474, 600, 322);
        led.setBounds (646, 474, 248, 322);
        edit.setBounds (908, 474, 340, 322);
    }
    void repaintAll() { for (auto* c : getChildren()) c->repaint(); }
    NanoEditor::Ui& ui;
    Header header; PresetBar presets; KorgView korg; DesignPanel design; LedPanel led; EditPanel edit; tk::UpdateBanner update;
    std::unique_ptr<juce::FileChooser> chooser;
};

// ---------------- Editor ------------------------------------------------------
NanoEditor::NanoEditor (NanoProcessor& p) : AudioProcessorEditor (p), proc (p), ui (p)
{
    auto r = std::make_unique<Root> (ui);
    auto* rp = r.get();
    ui.skinChanged = [rp] { rp->korg.setSkin(); rp->repaintAll(); };
    ui.refresh = [rp] { rp->repaintAll(); };
    for (int i = 0; i < NUM_GROUPS; ++i) if (skins()[(size_t) skinIndex (p.skin.toStdString())].group == i) ui.category = i;
    root = std::move (r);
    addAndMakeVisible (*root);
    tips = std::make_unique<juce::TooltipWindow> (this, 500);
    setResizable (true, true);
    setResizeLimits (800, 513, 1920, 1230);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio (1280.0 / 820.0);
    setSize (1280, 820);
    lastTick = juce::Time::getMillisecondCounterHiRes();
    seenPreset = p.preset(); seenMoves = p.moveCount.load(); seenKicks = p.kicks.load();
    startTimerHz (30);
    juce::MessageManager::callAsync ([sp = SafePointer<NanoEditor> (this)] { if (sp != nullptr) static_cast<Root*> (sp->root.get())->reportMissing(); });
}
NanoEditor::~NanoEditor() { stopTimer(); }

void NanoEditor::paint (juce::Graphics& g) { g.fillAll (col::bg); }
void NanoEditor::resized()
{
    if (root == nullptr) return;
    const float s = (float) getWidth() / 1280.0f;
    root->setTransform (juce::AffineTransform::scale (s));
    root->setBounds (0, 0, 1280, 820);
}

void NanoEditor::timerCallback()
{
    auto* r = static_cast<Root*> (root.get());
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dt = (float) juce::jlimit (0.0, 0.1, (now - lastTick) * 0.001); lastTick = now;
    auto& m = ui.motion;
    const bool anim = r->korg.animated();
    if (anim) m.time += dt;

    // Kick aus dem Audio, sonst Takt aus FL, sonst Vorschau
    const auto k = proc.kicks.load();
    if (k != seenKicks) { seenKicks = k; m.pulse = 1.0f; lastKickTime = now * 0.001; }
    else m.pulse *= std::exp (-dt * 7.0f);
    if (now * 0.001 - lastKickTime > 2.0)
    {
        double phase = -1;
        if (ui.preview) { previewClock += dt * 3.0; phase = previewClock - std::floor (previewClock); }
        else if (proc.playing.load() && proc.bpm.load() > 0) phase = proc.ppq.load() - std::floor (proc.ppq.load());
        if (phase >= 0) m.pulse = std::max (m.pulse, (float) std::exp (-phase * 6.0) * 0.8f);
    }
    m.level = proc.level.load();

    const int p = proc.preset();
    if (p != seenPreset) { seenPreset = p; m.flash = 1.0f; r->repaintAll(); }
    m.flash *= std::exp (-dt * 5.0f);

    const auto mv = proc.moveCount.load();
    if (mv != seenMoves)
    {
        seenMoves = mv;
        const int c = proc.lastMoved.load();
        if (c >= 0)
        {
            bool fresh = true;
            for (const auto& w : m.waves) if (w.age < 0.12f && std::fabs (w.x - controls()[(size_t) c].x) < 1.0f && std::fabs (w.y - controls()[(size_t) c].y) < 1.0f) fresh = false;
            if (fresh)
            {
                auto* oldest = &m.waves[0];
                for (auto& w : m.waves) if (w.age > oldest->age) oldest = &w;
                *oldest = { controls()[(size_t) c].x, controls()[(size_t) c].y, 0.0f };
            }
        }
        r->edit.repaint(); r->header.repaint();
    }
    for (auto& w : m.waves) w.age = std::min (1.0f, w.age + dt / 1.3f);

    if (anim) r->korg.renderNow();
    r->korg.repaint();
    static int slow = 0;
    if (++slow % 15 == 0) { r->edit.repaint(); r->header.repaint(); r->presets.repaint(); }
    const bool learn = proc.learning();
    if (learn != seenLearn) { seenLearn = learn; r->edit.repaint(); }
}
