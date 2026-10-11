//==============================================================================
// ShiftFxEditor — SHIFT/FX PRO panel. All artwork drawn in code; no
// third-party trademarks anywhere. Layout follows the reference mockup:
// top bar / FX TYPE selector / bottom bar.
//==============================================================================

#include "PluginEditor.h"

//---- palette -------------------------------------------------------------------
static const juce::Colour cBg      (0xff0a0c10);
static const juce::Colour cPanel   (0xff11141b);
static const juce::Colour cPanel2  (0xff161a23);
static const juce::Colour cEdge    (0xff232936);
static const juce::Colour cBlue    (0xff2f9dff);
static const juce::Colour cOrange  (0xffff9e2c);
static const juce::Colour cText    (0xffe9edf4);
static const juce::Colour cDim     (0xff8b93a5);
static const juce::Colour cGreen   (0xff35d06f);

static juce::Font fontBold (float s)   { return juce::Font (juce::FontOptions (s, juce::Font::bold)); }
static juce::Font fontPlain (float s)  { return juce::Font (juce::FontOptions (s)); }

static void drawGlowText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area,
                          const juce::Font& font, juce::Colour colour,
                          juce::Justification just = juce::Justification::centred)
{
    g.setFont (font);
    g.setColour (colour.withAlpha (0.16f));
    for (int dx = -1; dx <= 1; ++dx)
        for (int dy = -1; dy <= 1; ++dy)
            if (dx != 0 || dy != 0)
                g.drawText (text, area.translated ((float) dx, (float) dy), just, true);
    g.setColour (colour);
    g.drawText (text, area, just, true);
}

//==============================================================================
// TfxKnob
//==============================================================================
TfxKnob::TfxKnob (float diameterPx, juce::Colour accentColour, int numDetents)
    : diameter (diameterPx), accent (accentColour), detents (numDetents)
{
    setSize ((int) diameter + 16, (int) diameter + 16);
}

void TfxKnob::setValue (float v, bool notify)
{
    float nv = juce::jlimit (0.0f, 1.0f, v);
    if (detents > 1)
        nv = std::round (nv * (float) (detents - 1)) / (float) (detents - 1);
    if (nv != value)
    {
        value = nv;
        repaint();
        if (notify && onChange)
            onChange (value);
    }
}

void TfxKnob::mouseDown (const juce::MouseEvent& e)
{
    if (learnMode && learnIdx >= 0 && onLearnArm) { onLearnArm (learnIdx); return; }
    dragStartVal = value;
    dragStartY = e.y;
    // NB: a previous version hid the cursor here with NoCursor and never
    // restored it, which made the pointer vanish over the knob after the
    // first drag ("mouse goes behind it"). The cursor now stays visible.
}

void TfxKnob::mouseUp (const juce::MouseEvent&)
{
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

void TfxKnob::mouseEnter (const juce::MouseEvent&)
{
    if (! learnMode)
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

void TfxKnob::mouseExit (const juce::MouseEvent&)
{
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

void TfxKnob::mouseDoubleClick (const juce::MouseEvent&)
{
    if (learnMode && learnIdx >= 0 && onLearnArm) return; // don't reset while arming
    setValue (defaultValue);
}

void TfxKnob::mouseDrag (const juce::MouseEvent& e)
{
    const float range = 220.0f;
    setValue (dragStartVal + (float) (dragStartY - e.y) / range);
}

void TfxKnob::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const float step = detents > 1 ? 1.0f / (float) (detents - 1) : 0.04f;
    setValue (value + (w.deltaY > 0 ? step : -step) * (w.isReversed ? -1.0f : 1.0f));
}

void TfxKnob::paint (juce::Graphics& g)
{
    const float ui = (float) getWidth() / (diameter + 16.0f); // 1.0 at design size
    const float cx = getWidth() * 0.5f, cy = getHeight() * 0.5f;
    const float r = diameter * 0.5f * ui;

    if (learnHighlight)
    {
        g.setColour (cGreen.withAlpha (0.35f));
        g.drawEllipse (cx - r - 7 * ui, cy - r - 7 * ui, (r + 7 * ui) * 2, (r + 7 * ui) * 2, 3.0f * ui);
    }
    else if (learnMode && learnIdx >= 0)
    {
        g.setColour (cGreen.withAlpha (0.15f));
        g.drawEllipse (cx - r - 7 * ui, cy - r - 7 * ui, (r + 7 * ui) * 2, (r + 7 * ui) * 2, 2.0f * ui);
    }

    // value arc
    const float a0 = -2.35f, a1 = 2.35f; // ~ -135..135 deg
    const float va = a0 + value * (a1 - a0);
    g.setColour (accent.withAlpha (0.9f));
    juce::Path arc;
    arc.addCentredArc (cx, cy, r + 5 * ui, r + 5 * ui, 0.0f, a0, va, true);
    g.strokePath (arc, juce::PathStrokeType (3.0f * ui));
    g.setColour (cEdge);
    juce::Path rest;
    rest.addCentredArc (cx, cy, r + 5 * ui, r + 5 * ui, 0.0f, va, a1, true);
    g.strokePath (rest, juce::PathStrokeType (3.0f * ui));

    // metallic body
    juce::ColourGradient grad (juce::Colour (0xff3a4150), cx - r * 0.5f, cy - r * 0.6f,
                               juce::Colour (0xff14171d), cx + r * 0.4f, cy + r * 0.5f, false);
    grad.addColour (0.5, juce::Colour (0xff232936));
    g.setGradientFill (grad);
    g.fillEllipse (cx - r, cy - r, r * 2, r * 2);
    g.setColour (cEdge.brighter (0.15f));
    g.drawEllipse (cx - r, cy - r, r * 2, r * 2, 1.5f * ui);

    // pointer line
    const float pa = -juce::MathConstants<float>::pi * 0.75f + value * juce::MathConstants<float>::pi * 1.5f;
    const float px = cx + std::cos (pa) * r * 0.78f, py = cy + std::sin (pa) * r * 0.78f;
    g.setColour (juce::Colours::white);
    g.drawLine (cx, cy, px, py, 3.0f * ui);
    const float dotR = 3.0f * ui;
    g.fillEllipse (cx - dotR, cy - dotR, dotR * 2, dotR * 2);
}

//==============================================================================
// PresetWheel
//==============================================================================
PresetWheel::PresetWheel()
{
    setSize (560, 420);
}

float PresetWheel::detentAngle (int idx)
{
    // -PI/2 (top), clockwise.
    return -juce::MathConstants<float>::halfPi
           + (float) idx * (juce::MathConstants<float>::twoPi / 23.0f);
}

void PresetWheel::setPreset (int idx, bool notify)
{
    int ni = juce::jlimit (0, 22, idx);
    if (ni != preset)
    {
        preset = ni;
        repaint();
        if (notify && onChange)
            onChange (preset);
    }
}

void PresetWheel::mouseDown (const juce::MouseEvent& e)
{
    if (learnMode && learnIdx >= 0 && onLearnArm) { onLearnArm (learnIdx); return; }
    dragStartPreset = preset;
    dragStartY = e.y;
}

void PresetWheel::mouseDrag (const juce::MouseEvent& e)
{
    const int steps = (dragStartY - e.y) / 14;
    setPreset (dragStartPreset + steps);
}

void PresetWheel::mouseEnter (const juce::MouseEvent&)
{
    if (! learnMode)
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

void PresetWheel::mouseExit (const juce::MouseEvent&)
{
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

void PresetWheel::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const int dir = (w.deltaY > 0 ? -1 : 1) * (w.isReversed ? -1 : 1);
    setPreset ((preset + dir + 23) % 23);
}

void PresetWheel::paint (juce::Graphics& g)
{
    // ui scale: 1.0 when the wheel is at its design width (580px).
    const float ui = (float) getWidth() / 580.0f;
    const float cx = getWidth() * 0.5f, cy = getHeight() * 0.5f + 6.0f * ui;
    const float knobR = 95.0f * ui, dotR = 125.0f * ui, labelR = 150.0f * ui;

    if (learnHighlight)
    {
        g.setColour (cGreen.withAlpha (0.4f));
        g.drawEllipse (cx - knobR - 10 * ui, cy - knobR - 10 * ui,
                       (knobR + 10 * ui) * 2, (knobR + 10 * ui) * 2, 3.0f * ui);
    }
    else if (learnMode && learnIdx >= 0)
    {
        g.setColour (cGreen.withAlpha (0.15f));
        g.drawEllipse (cx - knobR - 10 * ui, cy - knobR - 10 * ui,
                       (knobR + 10 * ui) * 2, (knobR + 10 * ui) * 2, 2.0f * ui);
    }

    // decorative half arcs: blue left, orange right
    // (startAsSubPath=true so no stray line is drawn from the origin)
    g.setColour (cBlue.withAlpha (0.55f));
    juce::Path leftArc;
    leftArc.addCentredArc (cx, cy, knobR + 14 * ui, knobR + 14 * ui, 0.0f,
                           juce::MathConstants<float>::pi * 0.5f,
                           juce::MathConstants<float>::pi * 1.5f, true);
    g.strokePath (leftArc, juce::PathStrokeType (4.0f * ui));
    g.setColour (cOrange.withAlpha (0.55f));
    juce::Path rightArc;
    rightArc.addCentredArc (cx, cy, knobR + 14 * ui, knobR + 14 * ui, 0.0f,
                            -juce::MathConstants<float>::pi * 0.5f,
                            juce::MathConstants<float>::pi * 0.5f, true);
    g.strokePath (rightArc, juce::PathStrokeType (4.0f * ui));

    // knob body
    juce::ColourGradient grad (juce::Colour (0xff454e60), cx - knobR * 0.5f, cy - knobR * 0.6f,
                               juce::Colour (0xff101318), cx + knobR * 0.4f, cy + knobR * 0.5f, false);
    grad.addColour (0.5, juce::Colour (0xff262c38));
    g.setGradientFill (grad);
    g.fillEllipse (cx - knobR, cy - knobR, knobR * 2, knobR * 2);
    g.setColour (cEdge.brighter (0.2f));
    g.drawEllipse (cx - knobR, cy - knobR, knobR * 2, knobR * 2, 2.0f * ui);
    // inner bevel ring
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    const float bev = 10.0f * ui;
    g.drawEllipse (cx - knobR + bev, cy - knobR + bev,
                   (knobR - bev) * 2, (knobR - bev) * 2, 2.0f * ui);

    // pointer to selected detent
    const float pa = detentAngle (preset);
    g.setColour (juce::Colours::white);
    g.drawLine (cx, cy, cx + std::cos (pa) * (knobR - 14 * ui),
                cy + std::sin (pa) * (knobR - 14 * ui), 4.0f * ui);

    // bright marker at the selected detent on the ring
    g.setColour (cBlue);
    const float mk = 4.0f * ui;
    g.fillEllipse (cx + std::cos (pa) * (knobR + 14 * ui) - mk,
                   cy + std::sin (pa) * (knobR + 14 * ui) - mk, mk * 2, mk * 2);

    // dots mark every detent (selected dot is white; the pointer + bright
    // marker drawn above also indicate the selected detent)
    for (int i = 0; i < 23; ++i)
    {
        const float a = detentAngle (i);
        const float ca = std::cos (a), sa = std::sin (a);
        const bool leftSide = ca < -0.2f;
        const juce::Colour dotCol = leftSide ? cBlue : (ca > 0.2f ? cOrange : cBlue);
        const bool selected = (i == preset);

        const float dx = cx + ca * dotR, dy = cy + sa * dotR;
        const float dr = 3.0f * ui;
        g.setColour (selected ? juce::Colours::white : dotCol.withAlpha (0.85f));
        g.fillEllipse (dx - dr, dy - dr, dr * 2, dr * 2);
    }

    // Static label ring. The selected preset's name is already shown glowing in
    // the FX TYPE display, so its ring label is skipped here (it used to sit on
    // top of its neighbours). Labels are visited in angular order starting just
    // clockwise of the selected detent, so each label's predecessor in the loop
    // is its angular neighbour; any label that would overlap its neighbour is
    // nudged radially (in or out, whichever clears first) until the boxes
    // separate. This holds whichever detent is selected.
    auto labelBoxFor = [&] (int idx, float lr, juce::Justification& just) -> juce::Rectangle<float>
    {
        const float a = detentAngle (idx);
        const float ca = std::cos (a), sa = std::sin (a);
        just = juce::Justification::centred;
        float txOff = -80.0f * ui, tw = 160.0f * ui;
        if (ca < -0.35f)      { just = juce::Justification::centredRight; txOff = -150.0f * ui; tw = 145.0f * ui; }
        else if (ca > 0.35f)  { just = juce::Justification::centredLeft;  txOff = 5.0f * ui;    tw = 145.0f * ui; }
        return juce::Rectangle<float> (cx + ca * lr + txOff, cy + sa * lr - 10.0f * ui, tw, 20.0f * ui);
    };

    g.setFont (fontPlain (12.0f * ui));
    g.setColour (cDim);
    juce::Rectangle<float> prevBox;
    bool havePrev = false;
    for (int k = 0; k < 22; ++k)
    {
        const int i = (preset + 1 + k) % 23; // every detent except the selected one
        float lr = labelR + ((i % 2 == 0) ? 0.0f : 20.0f * ui);
        juce::Justification just = juce::Justification::centred;
        juce::Rectangle<float> box = labelBoxFor (i, lr, just);
        const float nudgeStep = 12.0f * ui;

        for (int nudge = 0; nudge < 8 && havePrev
             && box.expanded (3.0f * ui, 2.0f * ui).intersects (prevBox); ++nudge)
        {
            juce::Justification dummy = juce::Justification::centred;
            const bool outClear = ! labelBoxFor (i, lr + nudgeStep, dummy).expanded (3.0f * ui, 2.0f * ui).intersects (prevBox);
            const bool inClear  = ! labelBoxFor (i, lr - nudgeStep, dummy).expanded (3.0f * ui, 2.0f * ui).intersects (prevBox);
            if (outClear && ! inClear)      lr += nudgeStep;
            else if (inClear && ! outClear) lr -= nudgeStep;
            else if (outClear)              lr += nudgeStep; // both clear: prefer outward
            else
            {
                // neither clears on its own: step away from the neighbour's centre
                const float a = detentAngle (i);
                const float away = (box.getCentreX() - prevBox.getCentreX()) * std::cos (a)
                                 + (box.getCentreY() - prevBox.getCentreY()) * std::sin (a);
                lr += (away >= 0.0f ? nudgeStep : -nudgeStep);
            }
            lr = juce::jlimit (labelR - 24.0f * ui, labelR + 84.0f * ui, lr);
            box = labelBoxFor (i, lr, just);
        }

        g.drawText (ShiftFxProcessor::presets[i].name, box, just, true);
        prevBox = box;
        havePrev = true;
    }
}

//==============================================================================
// LevelMeter
//==============================================================================
void LevelMeter::setLevels (float l, float r)
{
    lv[0] = l; lv[1] = r;
    pk[0] = juce::jmax (pk[0] * 0.985f, l);
    pk[1] = juce::jmax (pk[1] * 0.985f, r);
    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    const float rowH = getHeight() * 0.5f;
    for (int ch = 0; ch < 2; ++ch)
    {
        const float y = ch * rowH + 2.0f, h = rowH - 4.0f;
        const float w = getWidth() - 22.0f, x = 20.0f;

        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillRoundedRectangle (x, y, w, h, 2.0f);

        const float v = juce::jlimit (0.0f, 1.0f, lv[ch]);
        const int segs = 24;
        const float segW = w / (float) segs;
        for (int s = 0; s < segs; ++s)
        {
            if ((float) (s + 1) / (float) segs > v) break;
            const float t = (float) s / (float) (segs - 1);
            juce::Colour sc = t < 0.65f ? cGreen : (t < 0.85f ? juce::Colour (0xffe8c33a) : juce::Colour (0xfff04a3a));
            g.setColour (sc);
            g.fillRoundedRectangle (x + s * segW + 1, y + 1, segW - 2, h - 2, 1.0f);
        }
        const float px = x + juce::jlimit (0.0f, 1.0f, pk[ch]) * w;
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.fillRect (px - 1, y, 2.0f, h);

        g.setFont (fontBold (10.0f * uiScale));
        g.setColour (cDim);
        g.drawText (ch == 0 ? "L" : "R", 0.0f, y, 16.0f, h, juce::Justification::centredLeft, true);
    }
}

//==============================================================================
// PowerButton
//==============================================================================
void PowerButton::setOn (bool o, bool notify)
{
    if (o != on)
    {
        on = o;
        repaint();
        if (notify && onChange)
            onChange (on);
    }
}

void PowerButton::mouseDown (const juce::MouseEvent&)
{
    if (learnMode && learnIdx >= 0 && onLearnArm) { onLearnArm (learnIdx); return; }
    setOn (! on);
}

void PowerButton::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2);
    const float cr = 10.0f;

    if (on)
    {
        g.setColour (cOrange.withAlpha (0.25f));
        g.fillRoundedRectangle (r.expanded (6), cr + 4);
    }
    g.setColour (on ? cOrange.withAlpha (0.35f) : cPanel2);
    g.fillRoundedRectangle (r, cr);
    g.setColour (on ? cOrange : cEdge.brighter (0.2f));
    g.drawRoundedRectangle (r, cr, on ? 2.5f : 1.5f);

    if (learnHighlight)
    {
        g.setColour (cGreen);
        g.drawRoundedRectangle (r.expanded (3), cr + 2, 2.0f);
    }

    // power glyph
    const float cx = r.getCentreX(), cy = r.getCentreY() + 2;
    const float gr = juce::jmin (r.getWidth(), r.getHeight()) * 0.26f;
    g.setColour (on ? cOrange : cDim);
    juce::Path p;
    p.addCentredArc (cx, cy, gr, gr, 0.0f, -0.6f, juce::MathConstants<float>::twoPi + 0.6f, true);
    g.strokePath (p, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved));
    g.drawLine (cx, cy - gr - 6, cx, cy - 2, 3.0f);
}

//==============================================================================
// GlyphButton
//==============================================================================
GlyphButton::GlyphButton (Glyph g) : glyph (g) {}

void GlyphButton::mouseDown (const juce::MouseEvent&)
{
    if (onClick) onClick();
}

void GlyphButton::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float ui = (float) juce::jmin (getWidth(), getHeight()) / 30.0f; // 1.0 at design size
    g.setColour (hover ? cPanel2.brighter (0.12f) : juce::Colours::transparentBlack);
    g.fillRoundedRectangle (r, 6.0f * ui);

    const float cx = r.getCentreX(), cy = r.getCentreY();
    g.setColour (active ? juce::Colour (0xffe8c33a) : cDim);

    switch (glyph)
    {
        case Star:
        {
            juce::Path star;
            for (int i = 0; i < 10; ++i)
            {
                const float ang = -juce::MathConstants<float>::halfPi + i * juce::MathConstants<float>::pi / 5.0f;
                const float rad = ((i % 2 == 0) ? 8.0f : 3.6f) * ui;
                const float px = cx + std::cos (ang) * rad, py = cy + std::sin (ang) * rad;
                if (i == 0) star.startNewSubPath (px, py);
                else        star.lineTo (px, py);
            }
            star.closeSubPath();
            if (active) g.fillPath (star);
            else        g.strokePath (star, juce::PathStrokeType (1.6f * ui));
            break;
        }
        case Save:
            g.drawRoundedRectangle (cx - 7 * ui, cy - 8 * ui, 14 * ui, 16 * ui, 2.0f * ui, 1.8f * ui);
            g.fillRect (cx - 3.0f * ui, cy - 8.0f * ui, 6.0f * ui, 5.0f * ui);
            g.drawLine (cx - 7 * ui, cy + 2 * ui, cx + 7 * ui, cy + 2 * ui, 1.4f * ui);
            break;
        case Settings:
        {
            g.drawEllipse (cx - 6 * ui, cy - 6 * ui, 12 * ui, 12 * ui, 1.8f * ui);
            for (int i = 0; i < 8; ++i)
            {
                const float ang = i * juce::MathConstants<float>::pi / 4.0f;
                g.drawLine (cx + std::cos (ang) * 7.5f * ui, cy + std::sin (ang) * 7.5f * ui,
                            cx + std::cos (ang) * 10.0f * ui, cy + std::sin (ang) * 10.0f * ui, 1.8f * ui);
            }
            break;
        }
        case ArrowLeft:
            g.setFont (fontBold (18.0f * ui));
            g.drawText ("<", r, juce::Justification::centred, true);
            break;
        case ArrowRight:
            g.setFont (fontBold (18.0f * ui));
            g.drawText (">", r, juce::Justification::centred, true);
            break;
    }
}

//==============================================================================
// PresetDisplay
//==============================================================================
void PresetDisplay::setPreset (const ShiftFxProcessor::PresetDef& p, bool fav)
{
    name = p.name;
    desc = p.desc;
    favorite = fav;
    repaint();
}

void PresetDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (r, 8.0f * uiScale);
    g.setColour (cEdge);
    g.drawRoundedRectangle (r, 8.0f * uiScale, 1.0f);

    drawGlowText (g, name, { r.getX(), r.getY() + 6 * uiScale, r.getWidth(), 26 * uiScale },
                  fontBold (21.0f * uiScale), cBlue);
    g.setFont (fontPlain (11.5f * uiScale));
    g.setColour (cDim);
    g.drawText (desc, juce::Rectangle<float> (r.getX() + 8 * uiScale, r.getY() + 34 * uiScale,
                                              r.getWidth() - 16 * uiScale, 20.0f * uiScale),
                juce::Justification::centred, true);
}

//==============================================================================
// ShiftFxEditor
//==============================================================================
ShiftFxEditor::ShiftFxEditor (ShiftFxProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    // NB: setSize is called at the END of the constructor — calling it here
    // would trigger resized() before the components below exist.

    //---- top bar ---------------------------------------------------------------
    bpmLabel.setFont (fontBold (24.0f));
    bpmLabel.setColour (juce::Label::textColourId, cBlue);
    bpmLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (bpmLabel);

    bpmSourceBox.addItem ("Host (DAW)", 1);
    bpmSourceBox.addItem ("Manual", 2);
    bpmSourceBox.setSelectedId (1, juce::dontSendNotification);
    bpmSourceBox.setColour (juce::ComboBox::backgroundColourId, cPanel2);
    bpmSourceBox.setColour (juce::ComboBox::textColourId, cText);
    bpmSourceBox.setColour (juce::ComboBox::outlineColourId, cEdge);
    bpmSourceBox.setColour (juce::ComboBox::arrowColourId, cDim);
    bpmSourceBox.onChange = [this]
    {
        const int id = bpmSourceBox.getSelectedId();
        setParam01 (ShiftFxProcessor::pidBpmSrc, id == 2 ? 1.0f : 0.0f);
    };
    addAndMakeVisible (bpmSourceBox);

    tapButton.setColour (juce::TextButton::buttonColourId, cPanel2);
    tapButton.setColour (juce::TextButton::textColourOffId, cBlue);
    tapButton.setColour (juce::TextButton::buttonOnColourId, cBlue);
    tapButton.onClick = [this] { doTapTempo(); };
    addAndMakeVisible (tapButton);

    presetArrowL = std::make_unique<GlyphButton> (GlyphButton::ArrowLeft);
    presetArrowR = std::make_unique<GlyphButton> (GlyphButton::ArrowRight);
    presetArrowL->onClick = [this] { stepPreset (-1); };
    presetArrowR->onClick = [this] { stepPreset (+1); };
    addAndMakeVisible (*presetArrowL);
    addAndMakeVisible (*presetArrowR);

    presetDisplay = std::make_unique<PresetDisplay>();
    addAndMakeVisible (*presetDisplay);

    starButton = std::make_unique<GlyphButton> (GlyphButton::Star);
    saveButton = std::make_unique<GlyphButton> (GlyphButton::Save);
    settingsButton = std::make_unique<GlyphButton> (GlyphButton::Settings);
    starButton->onClick = [this]
    {
        const int idx = presetWheel->getPreset();
        processor.setFavorite (idx, ! processor.isFavorite (idx));
        presetDisplay->setPreset (ShiftFxProcessor::presets[idx], processor.isFavorite (idx));
        centerDisplay->setPreset (ShiftFxProcessor::presets[idx], processor.isFavorite (idx));
        starButton->setActive (processor.isFavorite (idx));
    };
    saveButton->onClick = [this]
    {
        processor.updateHostDisplay (
            juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged (true));
        saveFlash = "Saved";
        saveFlashUntil = juce::Time::getMillisecondCounter() + 1500;
    };
    settingsButton->onClick = []
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::InfoIcon, "SHIFT/FX PRO",
            "Universal DJ Transition Engine v1.0\nby DJ Toolbox\n\n"
            "23 presets over 15 DSP engines.\nFormats: VST3 (Win/Mac), AU (Mac).");
    };
    addAndMakeVisible (*starButton);
    addAndMakeVisible (*saveButton);
    addAndMakeVisible (*settingsButton);

    inMeterTop = std::make_unique<LevelMeter>();
    outMeterTop = std::make_unique<LevelMeter>();
    addAndMakeVisible (*inMeterTop);
    addAndMakeVisible (*outMeterTop);

    //---- center ------------------------------------------------------------------
    presetWheel = std::make_unique<PresetWheel>();
    presetWheel->setLearnable (ShiftFxProcessor::LearnPreset);
    presetWheel->onChange = [this] (int idx) { setPresetIndex (idx); };
    presetWheel->onLearnArm = [this] (int li) { armLearn (li); };
    addAndMakeVisible (*presetWheel);

    centerArrowL = std::make_unique<GlyphButton> (GlyphButton::ArrowLeft);
    centerArrowR = std::make_unique<GlyphButton> (GlyphButton::ArrowRight);
    centerArrowL->onClick = [this] { stepPreset (-1); };
    centerArrowR->onClick = [this] { stepPreset (+1); };
    addAndMakeVisible (*centerArrowL);
    addAndMakeVisible (*centerArrowR);

    centerDisplay = std::make_unique<PresetDisplay>();
    addAndMakeVisible (*centerDisplay);

    //---- bottom bar ----------------------------------------------------------------
    inMeterBottom = std::make_unique<LevelMeter>();
    addAndMakeVisible (*inMeterBottom);

    dryWetKnob = std::make_unique<TfxKnob> (64.0f, cBlue);
    dryWetKnob->setLearnable (ShiftFxProcessor::LearnDryWet);
    dryWetKnob->setDefaultValue (1.0f); // double-click reset = parameter default
    dryWetKnob->onChange = [this] (float v)
        { setParam01 (ShiftFxProcessor::pidDryWet, v); };
    dryWetKnob->onLearnArm = [this] (int li) { armLearn (li); };
    addAndMakeVisible (*dryWetKnob);

    outMixKnob = std::make_unique<TfxKnob> (64.0f, cBlue);
    outMixKnob->setLearnable (ShiftFxProcessor::LearnOutMix);
    outMixKnob->setDefaultValue (0.9091f); // double-click reset = 0 dB
    outMixKnob->onChange = [this] (float v)
        { setParam01 (ShiftFxProcessor::pidOutMix, v); };
    outMixKnob->onLearnArm = [this] (int li) { armLearn (li); };
    addAndMakeVisible (*outMixKnob);

    powerButton = std::make_unique<PowerButton>();
    powerButton->setLearnable (ShiftFxProcessor::LearnOn);
    powerButton->onChange = [this] (bool b)
        { setParam01 (ShiftFxProcessor::pidOn, b ? 1.0f : 0.0f); };
    powerButton->onLearnArm = [this] (int li) { armLearn (li); };
    addAndMakeVisible (*powerButton);

    learnButton.setColour (juce::TextButton::buttonColourId, cPanel2);
    learnButton.setColour (juce::TextButton::textColourOffId, cText);
    learnButton.setColour (juce::TextButton::buttonOnColourId, cGreen);
    learnButton.setClickingTogglesState (true);
    learnButton.onClick = [this] { setLearnMode (learnButton.getToggleState()); };
    addAndMakeVisible (learnButton);

    learnStatusLabel.setFont (fontPlain (10.5f));
    learnStatusLabel.setColour (juce::Label::textColourId, cDim);
    learnStatusLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (learnStatusLabel);

    //---- initial state from parameters ----------------------------------------------
    auto* pPreset = processor.apvts.getParameter (ShiftFxProcessor::pidPreset);
    auto* pOn     = processor.apvts.getParameter (ShiftFxProcessor::pidOn);
    auto* pDryWet = processor.apvts.getParameter (ShiftFxProcessor::pidDryWet);
    auto* pOutMix = processor.apvts.getParameter (ShiftFxProcessor::pidOutMix);
    auto* pBpmSrc = processor.apvts.getParameter (ShiftFxProcessor::pidBpmSrc);
    const int initPreset = (int) pPreset->convertFrom0to1 (pPreset->getValue());
    presetWheel->setPreset (initPreset, false);
    presetDisplay->setPreset (ShiftFxProcessor::presets[initPreset],
                              processor.isFavorite (initPreset));
    centerDisplay->setPreset (ShiftFxProcessor::presets[initPreset],
                              processor.isFavorite (initPreset));
    starButton->setActive (processor.isFavorite (initPreset));
    powerButton->setOn (pOn->getValue() > 0.5f, false);
    dryWetKnob->setValue (pDryWet->getValue(), false);
    outMixKnob->setValue (pOutMix->getValue(), false);
    bpmSourceBox.setSelectedId (pBpmSrc->getValue() > 0.5f ? 2 : 1, juce::dontSendNotification);

    // z-order audit: the preset wheel's bounds overlap the top edge of the
    // bottom-bar knobs, so make sure the interactive controls sit on top and
    // reliably receive mouse events across their full visible area.
    dryWetKnob->toFront (false);
    outMixKnob->toFront (false);
    powerButton->toFront (false);

    // resizable window: 12:7 aspect locked, corner resizer visible.
    setResizable (true, true);
    setResizeLimits (960, 560, 1800, 1050);
    if (auto* boundsConstrainer = getConstrainer())
        boundsConstrainer->setFixedAspectRatio (1200.0 / 700.0);

    setSize (1200, 700);
    startTimerHz (30);
}

ShiftFxEditor::~ShiftFxEditor()
{
    stopTimer();
}

//==============================================================================
void ShiftFxEditor::resized()
{
    // Fully proportional layout: uniform scale s (aspect ratio is locked to
    // 12:7 by the constrainer, so sx == sy). Every bound derives from s.
    const float s = (float) getWidth() / 1200.0f;
    auto B = [s] (float x, float y, float w, float h)
    {
        return juce::Rectangle<int> (juce::roundToInt (x * s), juce::roundToInt (y * s),
                                     juce::roundToInt (w * s), juce::roundToInt (h * s));
    };

    // Top bar (0..96)
    bpmLabel.setBounds (B (296, 44, 130, 34));
    bpmLabel.setFont (fontBold (24.0f * s));
    bpmSourceBox.setBounds (B (440, 46, 132, 26));
    tapButton.setBounds (B (580, 46, 62, 26));

    presetArrowL->setBounds (B (688, 40, 30, 34));
    presetDisplay->setBounds (B (722, 40, 210, 34));
    presetArrowR->setBounds (B (936, 40, 30, 34));
    starButton->setBounds (B (974, 42, 30, 30));
    saveButton->setBounds (B (1008, 42, 30, 30));
    settingsButton->setBounds (B (1042, 42, 30, 30));

    inMeterTop->setBounds (B (1100, 14, 88, 30));
    outMeterTop->setBounds (B (1100, 50, 88, 30));

    // Center (96..560): FX TYPE display + giant wheel
    centerArrowL->setBounds (B (392, 116, 32, 62));
    centerDisplay->setBounds (B (430, 116, 340, 62));
    centerArrowR->setBounds (B (776, 116, 32, 62));
    presetWheel->setBounds (B (310, 149, 580, 450));

    // Bottom bar (560..700)
    inMeterBottom->setBounds (B (44, 586, 220, 52));
    dryWetKnob->setBounds (B (300, 572, 80, 80));
    outMixKnob->setBounds (B (440, 572, 80, 80));
    powerButton->setBounds (B (620, 576, 84, 72));
    learnButton.setBounds (B (1040, 586, 120, 34));
    learnStatusLabel.setBounds (B (1040, 624, 150, 22));
    learnStatusLabel.setFont (fontPlain (10.5f * s));

    // scale-aware sub-component fonts
    presetDisplay->setUiScale (s);
    centerDisplay->setUiScale (s);
    inMeterTop->setUiScale (s);
    outMeterTop->setUiScale (s);
    inMeterBottom->setUiScale (s);
}

void ShiftFxEditor::paint (juce::Graphics& g)
{
    // Scale all of the hand-drawn artwork below with the window size
    // (aspect ratio is locked, so one uniform scale factor is exact).
    const float s = (float) getWidth() / 1200.0f;
    g.addTransform (juce::AffineTransform::scale (s, s));

    // NB: below this point all coordinates are in DESIGN space (1200x700);
    // use literal design dimensions, not getWidth()/getHeight().
    g.fillAll (cBg);

    // subtle top-bar / bottom-bar panels
    g.setColour (cPanel);
    g.fillRect (0, 0, 1200, 96);
    g.fillRect (0, 560, 1200, 140);
    g.setColour (cEdge);
    g.drawLine (0, 96, 1200.0f, 96, 1.0f);
    g.drawLine (0, 560, 1200.0f, 560, 1.0f);

    //---- logo ----------------------------------------------------------------------
    g.setFont (fontBold (26.0f));
    g.setColour (cText);
    g.drawText ("SHIFT", 20, 14, 90, 30, juce::Justification::centredLeft, true);
    g.setColour (cBlue);
    g.drawText ("/ FX PRO", 104, 14, 130, 30, juce::Justification::centredLeft, true);
    g.setFont (fontPlain (10.0f));
    g.setColour (cDim);
    g.drawText ("UNIVERSAL DJ TRANSITION ENGINE", 22, 44, 260, 14,
                juce::Justification::centredLeft, true);

    //---- host sync + bpm labels -------------------------------------------------------
    const bool fromHost = processor.tempoFromHost.load();
    g.setColour (fromHost ? cGreen : cOrange);
    g.fillEllipse (298.0f, 24.0f, 9.0f, 9.0f);
    g.setFont (fontBold (10.5f));
    g.setColour (cDim);
    g.drawText (fromHost ? "HOST SYNC" : "MANUAL", 312, 16, 120, 24,
                juce::Justification::centredLeft, true);
    g.setFont (fontPlain (10.5f));
    g.drawText ("BPM SOURCE", 440, 24, 132, 18, juce::Justification::centredLeft, true);
    g.setFont (fontBold (10.5f));
    g.drawText ("PRESET", 688, 20, 120, 16, juce::Justification::centredLeft, true);

    // IN / OUT labels for top meters
    g.setFont (fontBold (10.0f));
    g.setColour (cDim);
    g.drawText ("IN", 1078, 14, 20, 30, juce::Justification::centredLeft, true);
    g.drawText ("OUT", 1078, 50, 24, 30, juce::Justification::centredLeft, true);

    //---- FX TYPE heading ----------------------------------------------------------------
    g.setFont (fontPlain (13.0f));
    g.setColour (cDim);
    g.drawText ("F X   T Y P E", 0, 96, 1200, 18, juce::Justification::centred, true);

    //---- bottom bar labels -------------------------------------------------------------------
    g.setFont (fontBold (10.5f));
    g.setColour (cDim);
    g.drawText ("LIVE AUDIO INPUT", 20, 564, 200, 16, juce::Justification::centredLeft, true);
    g.drawText ("DRY / WET", 290, 564, 100, 16, juce::Justification::centred, true);
    g.drawText ("OUTPUT MIX", 430, 564, 100, 16, juce::Justification::centred, true);
    g.drawText ("FX ON / OFF", 600, 564, 124, 16, juce::Justification::centred, true);
    g.drawText ("MIDI LEARN", 1040, 564, 120, 16, juce::Justification::centredLeft, true);

    g.setFont (fontPlain (10.5f));
    g.drawText ("DRY", 292, 662, 40, 14, juce::Justification::centredLeft, true);
    g.drawText ("WET", 348, 662, 40, 14, juce::Justification::centredRight, true);
    g.drawText ("-INF", 432, 662, 44, 14, juce::Justification::centredLeft, true);
    g.drawText ("+6dB", 484, 662, 44, 14, juce::Justification::centredRight, true);

    // automation ready
    g.setColour (cGreen);
    g.fillEllipse (762.0f, 572.0f, 9.0f, 9.0f);
    g.setFont (fontBold (11.0f));
    g.setColour (cText);
    g.drawText ("AUTOMATION READY", 776, 564, 220, 24, juce::Justification::centredLeft, true);
    g.setFont (fontPlain (10.5f));
    g.setColour (cDim);
    g.drawText ("All parameters can be automated\nin your DAW (Playlist, Arrangement, etc.)",
                762, 590, 240, 40, juce::Justification::topLeft, true);

    if (saveFlash.isNotEmpty()
        && juce::Time::getMillisecondCounter() < saveFlashUntil)
    {
        g.setFont (fontBold (12.0f));
        g.setColour (cGreen);
        g.drawText (saveFlash, 974, 74, 100, 18, juce::Justification::centredLeft, true);
    }
}

//==============================================================================
void ShiftFxEditor::timerCallback()
{
    // meters
    const float il = processor.inLevelL.load(), ir = processor.inLevelR.load();
    const float ol = processor.outLevelL.load(), orr = processor.outLevelR.load();
    inMeterTop->setLevels (il, ir);
    outMeterTop->setLevels (ol, orr);
    inMeterBottom->setLevels (il, ir);

    // bpm readout
    const float bpm = processor.effectiveBpm.load();
    bpmLabel.setText (juce::String (bpm, 1) + " BPM", juce::dontSendNotification);

    // sync UI from parameters (covers DAW automation playback)
    auto* pPreset = processor.apvts.getParameter (ShiftFxProcessor::pidPreset);
    auto* pOn     = processor.apvts.getParameter (ShiftFxProcessor::pidOn);
    auto* pDryWet = processor.apvts.getParameter (ShiftFxProcessor::pidDryWet);
    auto* pOutMix = processor.apvts.getParameter (ShiftFxProcessor::pidOutMix);
    auto* pBpmSrc = processor.apvts.getParameter (ShiftFxProcessor::pidBpmSrc);

    const int pIdx = (int) pPreset->convertFrom0to1 (pPreset->getValue());
    if (pIdx != presetWheel->getPreset())
    {
        presetWheel->setPreset (pIdx, false);
        presetDisplay->setPreset (ShiftFxProcessor::presets[pIdx],
                                  processor.isFavorite (pIdx));
        centerDisplay->setPreset (ShiftFxProcessor::presets[pIdx],
                                  processor.isFavorite (pIdx));
        starButton->setActive (processor.isFavorite (pIdx));
    }
    powerButton->setOn (pOn->getValue() > 0.5f, false);
    if (std::abs (dryWetKnob->getValue() - pDryWet->getValue()) > 0.0005f)
        dryWetKnob->setValue (pDryWet->getValue(), false);
    if (std::abs (outMixKnob->getValue() - pOutMix->getValue()) > 0.0005f)
        outMixKnob->setValue (pOutMix->getValue(), false);

    const int wantBoxId = pBpmSrc->getValue() > 0.5f ? 2 : 1;
    if (bpmSourceBox.getSelectedId() != wantBoxId)
        bpmSourceBox.setSelectedId (wantBoxId, juce::dontSendNotification);

    // MIDI learn completion event from the audio thread
    if (processor.learnEventFlag.exchange (false))
    {
        const int cc = processor.learnLastCc.load();
        const int lp = processor.learnLastParam.load();
        if (cc >= 0 && lp >= 0)
            learnStatusLabel.setText ("CC " + juce::String (cc) + " -> "
                                     + ShiftFxProcessor::learnParamNames[lp],
                                     juce::dontSendNotification);
        setLearnMode (false);
        learnButton.setToggleState (false, juce::dontSendNotification);
    }

    repaint (0, 0, getWidth(), juce::roundToInt (96.0f * (float) getWidth() / 1200.0f)); // top bar (bpm text)
    repaint (0, juce::roundToInt (560.0f * (float) getWidth() / 1200.0f),
             getWidth(), getHeight()); // bottom bar labels
}

//==============================================================================
void ShiftFxEditor::setParam01 (const char* pid, float v01)
{
    if (auto* p = processor.apvts.getParameter (pid))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v01));
        p->endChangeGesture();
    }
}

void ShiftFxEditor::setPresetIndex (int idx)
{
    const int ni = (idx % 23 + 23) % 23;
    if (auto* p = processor.apvts.getParameter (ShiftFxProcessor::pidPreset))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) ni));
        p->endChangeGesture();
    }
    presetDisplay->setPreset (ShiftFxProcessor::presets[ni], processor.isFavorite (ni));
    centerDisplay->setPreset (ShiftFxProcessor::presets[ni], processor.isFavorite (ni));
    starButton->setActive (processor.isFavorite (ni));
}

void ShiftFxEditor::stepPreset (int dir)
{
    setPresetIndex (presetWheel->getPreset() + dir);
}

void ShiftFxEditor::doTapTempo()
{
    const juce::uint32 now = juce::Time::getMillisecondCounter();
    tapTimes.add (now);
    while (tapTimes.size() > 8)
        tapTimes.remove (0);
    // drop taps older than 2.5 s from the newest
    while (tapTimes.size() > 1 && now - tapTimes.getFirst() > 2500)
        tapTimes.remove (0);

    if (tapTimes.size() >= 2)
    {
        double total = 0.0;
        for (int i = 1; i < tapTimes.size(); ++i)
            total += (double) (tapTimes[i] - tapTimes[i - 1]);
        const double avgMs = total / (double) (tapTimes.size() - 1);
        if (avgMs > 200.0 && avgMs < 2000.0)
        {
            const float bpm = juce::jlimit (60.0f, 200.0f, (float) (60000.0 / avgMs));
            setParam01 (ShiftFxProcessor::pidBpm, (bpm - 60.0f) / 140.0f);
            // tapping implies manual tempo
            setParam01 (ShiftFxProcessor::pidBpmSrc, 1.0f);
            bpmSourceBox.setSelectedId (2, juce::dontSendNotification);
        }
    }

    // also pulse the momentary tap parameter for automation visibility
    setParam01 (ShiftFxProcessor::pidTap, 1.0f);
    juce::Timer::callAfterDelay (120, [this]
    {
        if (auto* p = processor.apvts.getParameter (ShiftFxProcessor::pidTap))
            p->setValueNotifyingHost (0.0f);
    });
}

void ShiftFxEditor::armLearn (int learnIdx)
{
    if (! learnMode)
        return;
    processor.learnArmed.store (learnIdx);
    presetWheel->setLearnHighlight (learnIdx == ShiftFxProcessor::LearnPreset);
    dryWetKnob->setLearnHighlight (learnIdx == ShiftFxProcessor::LearnDryWet);
    outMixKnob->setLearnHighlight (learnIdx == ShiftFxProcessor::LearnOutMix);
    powerButton->setLearnHighlight (learnIdx == ShiftFxProcessor::LearnOn);
    learnStatusLabel.setText ("Waiting for MIDI CC… (click LEARN to cancel)",
                             juce::dontSendNotification);
}

void ShiftFxEditor::setLearnMode (bool b)
{
    learnMode = b;
    if (! b)
    {
        processor.learnArmed.store (-1);
        presetWheel->setLearnHighlight (false);
        dryWetKnob->setLearnHighlight (false);
        outMixKnob->setLearnHighlight (false);
        powerButton->setLearnHighlight (false);
        if (learnStatusLabel.getText().startsWith ("Waiting"))
            learnStatusLabel.setText ("", juce::dontSendNotification);
    }
    else
    {
        learnStatusLabel.setText ("Click a control, then send a MIDI CC",
                                 juce::dontSendNotification);
    }
    presetWheel->setLearnModeActive (b);
    dryWetKnob->setLearnModeActive (b);
    outMixKnob->setLearnModeActive (b);
    powerButton->setLearnModeActive (b);
    learnButton.setColour (juce::TextButton::buttonColourId, b ? cGreen.withAlpha (0.25f) : cPanel2);
}
