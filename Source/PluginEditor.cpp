#include "PluginEditor.h"

namespace ui
{
juce::Font fontOf (float height, bool bold)
{
    auto o = juce::FontOptions (height);
    if (bold) o = o.withStyle ("Bold");
    return juce::Font (o);
}

//==============================================================================
PilotLookAndFeel::PilotLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, col::bg);
    setColour (juce::TextButton::buttonColourId, col::panelHi);
    setColour (juce::TextButton::buttonOnColourId, col::mint);
    setColour (juce::TextButton::textColourOffId, col::text);
    setColour (juce::TextButton::textColourOnId, col::bg);
    setColour (juce::ComboBox::backgroundColourId, col::panelHi);
    setColour (juce::ComboBox::outlineColourId, col::line);
    setColour (juce::ComboBox::textColourId, col::text);
    setColour (juce::ComboBox::arrowColourId, col::mint);
    setColour (juce::PopupMenu::backgroundColourId, col::panel);
    setColour (juce::PopupMenu::textColourId, col::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::mint);
    setColour (juce::PopupMenu::highlightedTextColourId, col::bg);
    setColour (juce::Slider::textBoxTextColourId, col::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, col::text);
}

void PilotLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, juce::Slider& s)
{
    auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (5.f);
    const float radius = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;
    const auto c = r.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);

    g.setColour (col::panelHi);
    g.fillEllipse (c.x - radius * 0.72f, c.y - radius * 0.72f, radius * 1.44f, radius * 1.44f);
    g.setColour (col::line);
    g.drawEllipse (c.x - radius * 0.72f, c.y - radius * 0.72f, radius * 1.44f, radius * 1.44f, 1.f);

    const float arcR = radius - 2.f;
    juce::Path track, value;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.f, startAngle, endAngle, true);
    value.addCentredArc (c.x, c.y, arcR, arcR, 0.f, startAngle, angle, true);
    g.setColour (col::line);
    g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (accent);
    g.strokePath (value, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.6f, -radius * 0.66f, 3.2f, radius * 0.34f, 1.6f);
    g.setColour (col::text);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
}

void PilotLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos,
                                         float, float, juce::Slider::SliderStyle, juce::Slider& s)
{
    const float cy = (float) y + (float) h * 0.5f;
    const juce::Rectangle<float> track ((float) x + 6.f, cy - 3.f, (float) w - 12.f, 6.f);
    g.setColour (col::bg);
    g.fillRoundedRectangle (track, 3.f);
    g.setColour (s.findColour (juce::Slider::rotarySliderFillColourId));
    g.fillRoundedRectangle (track.withRight (juce::jlimit (track.getX(), track.getRight(), sliderPos)), 3.f);
    g.setColour (col::text);
    g.fillEllipse (sliderPos - 8.f, cy - 8.f, 16.f, 16.f);
}

void PilotLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto base = b.findColour (b.getToggleState() ? juce::TextButton::buttonOnColourId : juce::TextButton::buttonColourId);
    if (highlighted) base = base.brighter (0.12f);
    if (down) base = base.darker (0.15f);
    g.setColour (base);
    g.fillRoundedRectangle (r, 7.f);
    g.setColour (b.getToggleState() ? base.brighter (0.25f) : col::line);
    g.drawRoundedRectangle (r, 7.f, 1.f);
}

juce::Font PilotLookAndFeel::getTextButtonFont (juce::TextButton&, int) { return fontOf (12.5f, true); }
juce::Font PilotLookAndFeel::getComboBoxFont (juce::ComboBox&)          { return fontOf (13.f); }
juce::Font PilotLookAndFeel::getPopupMenuFont()                          { return fontOf (13.f); }

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& apvts, const char* paramId, juce::Colour accent, const juce::String& labelOverride)
{
    const pp::FloatDef* def = nullptr;
    for (const auto& d : pp::kFloats)
        if (juce::String (d.id) == paramId) { def = &d; break; }

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 15);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    slider.setColour (juce::Slider::textBoxTextColourId, col::text);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    addAndMakeVisible (slider);

    label.setText (labelOverride.isNotEmpty() ? labelOverride : juce::String (def != nullptr ? def->name : paramId), juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (fontOf (11.5f));
    label.setColour (juce::Label::textColourId, col::dim);
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, slider);

    if (def != nullptr)
    {
        const juce::String unit (def->unit);
        const int decimals = def->decimals;
        slider.setDoubleClickReturnValue (true, def->def);
        slider.textFromValueFunction = [unit, decimals] (double v) { return juce::String (v, decimals) + unit; };
        slider.valueFromTextFunction = [] (const juce::String& t) { return (double) t.retainCharacters ("-0123456789.").getDoubleValue(); };
        slider.updateText();
    }
}

void Knob::resized()
{
    auto b = getLocalBounds();
    label.setBounds (b.removeFromBottom (14));
    slider.setBounds (b);
}

//==============================================================================
TraceView::TraceView (PitchPilotProcessor& p) : proc (p)
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (30);
}

void TraceView::timerCallback()
{
    const int W = proc.traceWrite.load(), N = PitchPilotProcessor::traceSize;
    double sum = 0.0;
    int cnt = 0;
    for (int i = 1; i <= 100 && W - i >= 0; ++i)
    {
        const float v = proc.traceOut[(size_t) ((W - i) % N)].load();
        if (v > 0.f) { sum += v; ++cnt; }
    }
    if (cnt > 10)
        centre += 0.08f * ((float) (sum / cnt) - centre);
    centre = juce::jlimit (38.f, 82.f, centre);
    repaint();
}

void TraceView::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (col::panel);
    g.fillRoundedRectangle (b, 10.f);
    g.setColour (col::line);
    g.drawRoundedRectangle (b.reduced (0.5f), 10.f, 1.f);

    const auto area = b.reduced (10.f, 10.f);
    const float span = 15.f, lo = centre - span * 0.5f;
    auto yOf = [&] (float m) { return area.getBottom() - (m - lo) / span * area.getHeight(); };

    g.saveState();
    g.reduceClipRegion (area.toNearestInt());

    // note lanes
    for (int k = (int) std::floor (lo) - 1; k <= (int) std::ceil (lo + span) + 1; ++k)
    {
        const int pc = ((k % 12) + 12) % 12;
        const int st = (int) proc.getParamValue ("note" + juce::String (pc));
        const float y = yOf ((float) k);
        const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;

        const juce::Rectangle<float> band (area.getX(), yOf ((float) k + 0.5f), area.getWidth(), yOf ((float) k - 0.5f) - yOf ((float) k + 0.5f));
        if (st == 1)      g.setColour (col::bg.withAlpha (0.6f));
        else if (st == 2) g.setColour (col::amber.withAlpha (0.07f));
        else              g.setColour (col::mint.withAlpha (black ? 0.045f : 0.08f));
        g.fillRect (band);

        g.setColour (st == 1 ? col::line.withAlpha (0.4f) : col::line);
        g.drawHorizontalLine ((int) y, area.getX(), area.getRight());
        g.setColour (st == 1 ? col::dim.withAlpha (0.35f) : col::dim);
        g.setFont (fontOf (10.f));
        g.drawText (juce::String (pp::kKeyNames[pc]), (int) area.getRight() - 34, (int) y - 7, 30, 14, juce::Justification::right);
    }

    // pitch lines
    const int W = proc.traceWrite.load(), N = PitchPilotProcessor::traceSize;
    auto drawLine = [&] (const std::array<std::atomic<float>, PitchPilotProcessor::traceSize>& arr, juce::Colour c, float thickness)
    {
        juce::Path path;
        bool pen = false;
        for (int i = 0; i < N; ++i)
        {
            const int idx = W - N + i;
            if (idx < 0) continue;
            const float v = arr[(size_t) (idx % N)].load();
            const float x = area.getX() + area.getWidth() * (float) i / (float) (N - 1);
            if (v <= 0.f) { pen = false; continue; }
            if (! pen) { path.startNewSubPath (x, yOf (v)); pen = true; }
            else         path.lineTo (x, yOf (v));
        }
        g.setColour (c);
        g.strokePath (path, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    drawLine (proc.traceDet, col::text.withAlpha (0.4f), 1.4f);
    drawLine (proc.traceOut, col::mint, 2.6f);
    g.restoreState();

    // big note readout + cents meter
    const int cn = proc.currentNote.load();
    g.setColour (cn >= 0 ? col::mint : col::dim);
    g.setFont (fontOf (46.f, true));
    g.drawText (cn >= 0 ? juce::String (pp::kKeyNames[cn]) : juce::String ("--"), (int) area.getX() + 8, (int) area.getY() + 2, 120, 54, juce::Justification::left);

    float cents = 0.f;
    bool have = false;
    if (W > 0)
    {
        const size_t li = (size_t) ((W - 1) % N);
        const float d = proc.traceDet[li].load(), t = proc.traceTarget[li].load();
        if (d > 0.f && t > 0.f) { cents = juce::jlimit (-99.f, 99.f, (d - t) * 100.f); have = true; }
    }
    const float mx = area.getX() + 12.f, my = area.getY() + 66.f, mw = 140.f;
    g.setColour (col::line);
    g.fillRoundedRectangle (mx, my, mw, 4.f, 2.f);
    g.setColour (col::dim);
    g.fillRect (mx + mw * 0.5f - 1.f, my - 3.f, 2.f, 10.f);
    if (have)
    {
        const float px = mx + mw * 0.5f + juce::jlimit (-50.f, 50.f, cents) / 50.f * (mw * 0.5f);
        g.setColour (std::abs (cents) < 10.f ? col::mint : col::amber);
        g.fillEllipse (px - 5.f, my - 3.f, 10.f, 10.f);
        g.setFont (fontOf (11.f));
        g.setColour (col::dim);
        g.drawText (juce::String ((int) std::round (cents)) + " cents", (int) mx, (int) my + 10, 140, 14, juce::Justification::centred);
    }
}

//==============================================================================
NoteStrip::NoteStrip (PitchPilotProcessor& p) : proc (p)
{
    startTimerHz (15);
}

juce::Rectangle<int> NoteStrip::cell (int pc, int row) const
{
    static const int ys[4] = { 0, 20, 44, 66 }, hs[4] = { 18, 22, 20, 18 };
    return { labelW + pc * colW + 2, ys[row], colW - 4, hs[row] };
}

bool NoteStrip::hit (juce::Point<int> pt, int& pc, int& row) const
{
    if (pt.x < labelW) return false;
    pc = juce::jlimit (0, 11, (pt.x - labelW) / colW);
    for (int r = 0; r < 4; ++r)
        if (cell (pc, r).contains (pt)) { row = r; return true; }
    return false;
}

void NoteStrip::paint (juce::Graphics& g)
{
    g.setFont (fontOf (10.f, true));
    g.setColour (col::dim);
    g.drawText ("CENTS",  4, cell (0, cents).getY(),  labelW - 6, cell (0, cents).getHeight(),  juce::Justification::left);
    g.drawText ("BYPASS", 4, cell (0, bypass).getY(), labelW - 6, cell (0, bypass).getHeight(), juce::Justification::left);
    g.drawText ("REMOVE", 4, cell (0, remove).getY(), labelW - 6, cell (0, remove).getHeight(), juce::Justification::left);

    const int cn = proc.currentNote.load();
    for (int pc = 0; pc < 12; ++pc)
    {
        const int st = (int) proc.getParamValue ("note" + juce::String (pc));
        const float cts = proc.getParamValue ("cents" + juce::String (pc));

        g.setFont (fontOf (12.f, true));
        g.setColour (pc == cn ? col::mint : col::text);
        g.drawText (juce::String (pp::kKeyNames[pc]), cell (pc, header), juce::Justification::centred);

        auto c = cell (pc, cents).toFloat();
        g.setColour (col::panelHi);
        g.fillRoundedRectangle (c, 5.f);
        g.setColour (std::abs (cts) > 0.5f ? col::mint : col::dim);
        g.setFont (fontOf (11.5f));
        g.drawText ((cts > 0.5f ? "+" : "") + juce::String ((int) std::round (cts)), c.toNearestInt(), juce::Justification::centred);

        auto drawCell = [&] (int row, bool on, juce::Colour onCol)
        {
            auto r = cell (pc, row).toFloat();
            g.setColour (on ? onCol : col::panelHi);
            g.fillRoundedRectangle (r, 4.f);
        };
        drawCell (bypass, st == 2, col::amber);
        drawCell (remove, st == 1, col::coral);
    }
}

void NoteStrip::mouseDown (const juce::MouseEvent& e)
{
    int pc = 0, row = 0;
    if (! hit (e.getPosition(), pc, row)) return;

    const int st = (int) proc.getParamValue ("note" + juce::String (pc));
    if (row == bypass)      proc.setNoteState (pc, st == 2 ? 0 : 2);
    else if (row == remove) proc.setNoteState (pc, st == 1 ? 0 : 1);
    else if (row == cents)
    {
        dragParam = proc.apvts.getParameter ("cents" + juce::String (pc));
        if (dragParam != nullptr)
        {
            dragStartValue = proc.getParamValue ("cents" + juce::String (pc));
            dragStartY = e.y;
            dragParam->beginChangeGesture();
        }
    }
}

void NoteStrip::mouseDrag (const juce::MouseEvent& e)
{
    if (dragParam == nullptr) return;
    const float v = juce::jlimit (-50.f, 50.f, std::round (dragStartValue + (float) (dragStartY - e.y) * 0.5f));
    dragParam->setValueNotifyingHost (dragParam->convertTo0to1 (v));
}

void NoteStrip::mouseUp (const juce::MouseEvent&)
{
    if (dragParam != nullptr)
    {
        dragParam->endChangeGesture();
        dragParam = nullptr;
    }
}

void NoteStrip::mouseDoubleClick (const juce::MouseEvent& e)
{
    int pc = 0, row = 0;
    if (hit (e.getPosition(), pc, row) && row == cents)
        proc.setParamValue ("cents" + juce::String (pc), 0.f);
}

//==============================================================================
KeyboardView::KeyboardView (PitchPilotProcessor& p) : proc (p)
{
    startTimerHz (20);
}

juce::Rectangle<float> KeyboardView::keyRect (int key) const
{
    static const int whiteIdx[12] = { 0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6 };
    const int oct = key / 12, pc = key % 12;
    const float whiteW = (float) getWidth() / 14.f, h = (float) getHeight();
    if (! isBlack (pc))
        return { (float) (oct * 7 + whiteIdx[pc]) * whiteW, 0.f, whiteW, h };
    const float bw = whiteW * 0.6f;
    return { (float) (oct * 7 + whiteIdx[pc] + 1) * whiteW - bw * 0.5f, 0.f, bw, h * 0.62f };
}

int KeyboardView::keyAt (juce::Point<int> pt) const
{
    const auto fp = pt.toFloat();
    for (int k = 0; k < 24; ++k)
        if (isBlack (k % 12) && keyRect (k).contains (fp)) return k;
    for (int k = 0; k < 24; ++k)
        if (! isBlack (k % 12) && keyRect (k).contains (fp)) return k;
    return -1;
}

void KeyboardView::paint (juce::Graphics& g)
{
    const int cn = proc.currentNote.load();
    const unsigned held = proc.virtualHeldMask.load() | proc.midiHeldMask.load();

    auto drawKey = [&] (int k)
    {
        const int pc = k % 12;
        const bool black = isBlack (pc);
        const int st = (int) proc.getParamValue ("note" + juce::String (pc));
        auto r = keyRect (k).reduced (black ? 0.f : 1.f, 0.f);

        juce::Colour c = black ? juce::Colour (0xff0b0d11) : juce::Colour (0xffe9edf3);
        if (st == 1) c = black ? juce::Colour (0xff23262e) : juce::Colour (0xff3a3f4a);
        if (st == 2) c = black ? juce::Colour (0xff6b4a14) : juce::Colour (0xffe7c27a);
        if (pc == cn) c = c.interpolatedWith (col::mint, 0.7f);
        if ((held >> pc) & 1u) c = col::violet;

        g.setColour (c);
        g.fillRoundedRectangle (r.withTrimmedTop (-4.f), 4.f);
        g.setColour (col::line);
        g.drawRoundedRectangle (r.withTrimmedTop (-4.f), 4.f, 1.f);

        if (st == 1)   // removed notes get a coral marker so they read clearly on black keys too
        {
            g.setColour (col::coral);
            g.fillEllipse (r.getCentreX() - 3.5f, r.getBottom() - (black ? 16.f : 32.f), 7.f, 7.f);
        }

        if (! black)
        {
            g.setColour (st == 1 ? col::dim.withAlpha (0.5f) : juce::Colour (0xff555b68));
            g.setFont (fontOf (10.f));
            g.drawText (juce::String (pp::kKeyNames[pc]), r.withTrimmedTop (r.getHeight() - 20.f).toNearestInt(), juce::Justification::centred);
        }
    };

    for (int k = 0; k < 24; ++k) if (! isBlack (k % 12)) drawKey (k);
    for (int k = 0; k < 24; ++k) if (isBlack (k % 12))   drawKey (k);
}

void KeyboardView::mouseDown (const juce::MouseEvent& e)
{
    const int key = keyAt (e.getPosition());
    if (key < 0) return;
    const int pc = key % 12;
    const unsigned bit = 1u << pc;

    if (proc.learning.load())
    {
        proc.learnMask.fetch_or (bit);
        proc.virtualHeldMask.fetch_or (bit);
        heldPc = pc;
        return;
    }

    if (proc.getParamValue ("midimode") > 0.5f)
    {
        if (isLatch && isLatch())
        {
            if (proc.virtualHeldMask.load() & bit) proc.virtualHeldMask.fetch_and (~bit);
            else                                   proc.virtualHeldMask.fetch_or (bit);
        }
        else
        {
            proc.virtualHeldMask.fetch_or (bit);
            heldPc = pc;
        }
        return;
    }

    const int target = (getEditMode && getEditMode() == 1) ? 2 : 1;
    const int st = (int) proc.getParamValue ("note" + juce::String (pc));
    proc.setNoteState (pc, st == target ? 0 : target);
}

void KeyboardView::mouseUp (const juce::MouseEvent&)
{
    if (heldPc >= 0)
    {
        proc.virtualHeldMask.fetch_and (~(1u << heldPc));
        heldPc = -1;
    }
}

} // namespace ui

//==============================================================================
void PitchPilotEditor::Content::paint (juce::Graphics& g)
{
    using namespace ui;
    g.fillAll (col::bg);

    juce::AttributedString title;
    title.append ("PITCH", fontOf (26.f, true), col::text);
    title.append ("PILOT", fontOf (26.f, true), col::mint);
    title.draw (g, juce::Rectangle<float> (20.f, 14.f, 200.f, 34.f));
    g.setColour (col::dim);
    g.setFont (fontOf (11.f));
    g.drawText ("pitch correction + vocal fx", 22, 50, 200, 14, juce::Justification::left);

    for (const auto& p : panels)
    {
        g.setColour (col::panel);
        g.fillRoundedRectangle (p.r.toFloat(), 10.f);
        g.setColour (col::line);
        g.drawRoundedRectangle (p.r.toFloat().reduced (0.5f), 10.f, 1.f);
        if (p.title.isNotEmpty())
        {
            g.setColour (col::dim);
            g.setFont (fontOf (10.5f, true));
            g.drawText (p.title, p.r.getX() + 14, p.r.getY() + 6, p.r.getWidth() - 28, 14, juce::Justification::left);
        }
    }

    for (const auto& c : captions)
    {
        g.setColour (col::dim);
        g.setFont (fontOf (c.size, c.size < 11.5f));
        g.drawText (c.text, c.r, c.just);
    }
}

//==============================================================================
PitchPilotEditor::PitchPilotEditor (PitchPilotProcessor& p)
    : AudioProcessorEditor (&p), proc (p), trace (p), strip (p), keyboard (p)
{
    using namespace ui;
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    auto panel   = [this] (juce::Rectangle<int> r, const juce::String& t) { content.panels.push_back ({ r, t }); };
    auto caption = [this] (juce::Rectangle<int> r, const juce::String& t, juce::Justification j = juce::Justification::left, float size = 10.f)
                   { content.captions.push_back ({ r, t, j, size }); };

    auto addKnob = [&] (const char* id, juce::Rectangle<int> r, juce::Colour c, const juce::String& label = {})
    {
        auto k = std::make_unique<Knob> (p.apvts, id, c, label);
        k->setBounds (r);
        content.addAndMakeVisible (*k);
        knobs.push_back (std::move (k));
    };

    auto addButton = [&] (juce::TextButton& b, const juce::String& text, juce::Rectangle<int> r)
    {
        b.setButtonText (text);
        b.setBounds (r);
        content.addAndMakeVisible (b);
    };

    auto addToggle = [&] (juce::TextButton& b, const char* id, const juce::String& text, juce::Rectangle<int> r, juce::Colour on)
    {
        addButton (b, text, r);
        b.setClickingTogglesState (true);
        b.setColour (juce::TextButton::buttonOnColourId, on);
        buttonAtts.push_back (std::make_unique<ButtonAtt> (p.apvts, id, b));
    };

    auto addCombo = [&] (juce::ComboBox& c, const char* id, juce::Rectangle<int> r)
    {
        if (auto* cp = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter (id)))
            c.addItemList (cp->choices, 1);
        c.setBounds (r);
        content.addAndMakeVisible (c);
        comboAtts.push_back (std::make_unique<ComboAtt> (p.apvts, id, c));
    };

    // ---------------- header ----------------
    caption ({ 230, 10, 132, 14 }, "INPUT TYPE");
    caption ({ 370, 10, 72, 14 },  "KEY");
    caption ({ 450, 10, 160, 14 }, "SCALE");
    addCombo (inputBox, "inputtype", { 230, 28, 132, 28 });
    addCombo (keyBox,   "key",       { 370, 28, 72, 28 });
    addCombo (scaleBox, "scale",     { 450, 28, 160, 28 });
    keyBox.onChange   = [this] { proc.applyScale (keyBox.getSelectedItemIndex(), scaleBox.getSelectedItemIndex()); };
    scaleBox.onChange = [this] { proc.applyScale (keyBox.getSelectedItemIndex(), scaleBox.getSelectedItemIndex()); };

    addToggle (classicBtn,    "classic",   "Classic",    { 622, 28, 72, 28 }, col::amber);
    addToggle (formantBtn,    "formant",   "Formant",    { 700, 28, 80, 28 }, col::mint);
    addToggle (ignoreVibBtn,  "ignorevib", "Ignore Vib", { 786, 28, 88, 28 }, col::mint);

    addKnob ("throat",    { 880, 2, 66, 82 }, col::mint);
    addKnob ("transpose", { 946, 2, 66, 82 }, col::mint);
    addKnob ("detune",    { 1012, 2, 66, 82 }, col::mint);
    addKnob ("tracking",  { 1078, 2, 66, 82 }, col::mint);

    // ---------------- main row ----------------
    panel ({ 16, 92, 140, 268 }, {});
    panel ({ 1004, 92, 140, 268 }, {});
    addKnob ("retune",   { 16, 98, 140, 128 }, col::mint);
    addKnob ("flex",     { 16, 228, 140, 128 }, col::mint);
    addKnob ("natvib",   { 1004, 98, 140, 128 }, col::amber);
    addKnob ("humanize", { 1004, 228, 140, 128 }, col::amber);

    trace.setBounds (168, 92, 824, 268);
    content.addAndMakeVisible (trace);
    addToggle (holdBtn, "hold", "HOLD", { 168 + 824 - 88, 102, 76, 28 }, col::amber);

    // ---------------- create vibrato ----------------
    panel ({ 16, 372, 724, 120 }, "CREATE VIBRATO");
    caption ({ 28, 402, 134, 12 }, "SHAPE");
    addCombo (vibShapeBox, "vibshape", { 28, 420, 134, 28 });
    const char* vibIds[] = { "vibrate", "vibdelay", "vibonset", "vibvar", "vibpitch", "vibamp", "vibformant" };
    const char* vibLabels[] = { "Rate", "Onset Delay", "Onset Rate", "Variation", "Pitch Amt", "Amp Amt", "Formant Amt" };
    for (int i = 0; i < 7; ++i)
        addKnob (vibIds[i], { 176 + 78 * i, 396, 78, 92 }, col::violet, vibLabels[i]);

    // ---------------- style chains ----------------
    panel ({ 752, 372, 392, 120 }, "STYLE CHAINS");
    addButton (fakeMinkBtn, "FAKEMINK STYLE", { 768, 402, 172, 38 });
    addButton (esDeeKidBtn, "ESDEEKID STYLE", { 952, 402, 176, 38 });
    for (auto* b : { &fakeMinkBtn, &esDeeKidBtn })
    {
        b->setColour (juce::TextButton::buttonColourId, b == &fakeMinkBtn ? col::coral : col::violet);
        b->setColour (juce::TextButton::textColourOffId, col::bg);
    }
    fakeMinkBtn.onClick = [this] { proc.applyPreset (pp::kPresetFakeMink, pp::kNumPresetValues); resetOrder(); };
    esDeeKidBtn.onClick = [this] { proc.applyPreset (pp::kPresetEsDeeKid, pp::kNumPresetValues); resetOrder(); };
    caption ({ 768, 448, 360, 32 }, "One-click starting points - not exact copies. Tweak by ear.", juce::Justification::centred, 11.f);

    // ---------------- note targeting ----------------
    addButton (setMajorBtn,  "Set Major",  { 16, 504, 88, 32 });
    addButton (setMinorBtn,  "Set Minor",  { 110, 504, 88, 32 });
    addButton (setAllBtn,    "Set All",    { 204, 504, 88, 32 });
    addButton (bypassAllBtn, "Bypass All", { 298, 504, 88, 32 });
    addButton (removeAllBtn, "Remove All", { 392, 504, 88, 32 });
    setMajorBtn.onClick = [this] { proc.setParamValue ("scale", 1.f); proc.applyScale ((int) proc.getParamValue ("key"), 1); };
    setMinorBtn.onClick = [this] { proc.setParamValue ("scale", 2.f); proc.applyScale ((int) proc.getParamValue ("key"), 2); };
    setAllBtn.onClick   = [this] { proc.setParamValue ("scale", 0.f); for (int i = 0; i < 12; ++i) proc.setNoteState (i, 0); };
    bypassAllBtn.onClick = [this] { for (int i = 0; i < 12; ++i) proc.setNoteState (i, 2); };
    removeAllBtn.onClick = [this] { for (int i = 0; i < 12; ++i) proc.setNoteState (i, 1); };

    addToggle (targetNotesBtn, "midimode", "Target Notes", { 498, 504, 100, 32 }, col::violet);
    addButton (learnBtn, "Learn Scale", { 604, 504, 100, 32 });
    learnBtn.setClickingTogglesState (true);
    learnBtn.setColour (juce::TextButton::buttonOnColourId, col::coral);
    learnBtn.onClick = [this]
    {
        if (learnBtn.getToggleState())
        {
            proc.learnMask = 0;
            proc.virtualHeldMask = 0;
            proc.learning = true;
        }
        else
        {
            proc.learning = false;
            proc.virtualHeldMask = 0;
            const unsigned m = proc.learnMask.load();
            if (m != 0) proc.setNotesFromMask (m);
        }
    };

    addButton (editRemoveBtn, "Edit: Remove", { 722, 504, 92, 32 });
    addButton (editBypassBtn, "Edit: Bypass", { 820, 504, 92, 32 });
    addButton (latchBtn,      "Keys: Latch",  { 930, 504, 92, 32 });
    addButton (momentaryBtn,  "Keys: Momentary", { 1028, 504, 116, 32 });
    for (auto* b : { &editRemoveBtn, &editBypassBtn, &latchBtn, &momentaryBtn })
    {
        b->setClickingTogglesState (true);
        b->setColour (juce::TextButton::buttonOnColourId, col::amber);
    }
    editRemoveBtn.setRadioGroupId (101);
    editBypassBtn.setRadioGroupId (101);
    latchBtn.setRadioGroupId (102);
    momentaryBtn.setRadioGroupId (102);
    editRemoveBtn.setToggleState (true, juce::dontSendNotification);
    latchBtn.setToggleState (true, juce::dontSendNotification);

    strip.setBounds (16, 544, 1128, 84);
    content.addAndMakeVisible (strip);

    keyboard.setBounds (20, 638, 1120, 114);
    keyboard.getEditMode = [this] { return editBypassBtn.getToggleState() ? 1 : 0; };
    keyboard.isLatch     = [this] { return latchBtn.getToggleState(); };
    content.addAndMakeVisible (keyboard);

    // ---------------- effects rack ----------------
    panel ({ 16, 762, 316, 126 }, "TONE");
    const char* toneIds[] = { "lowcut", "body", "presence", "air", "compress" };
    for (int i = 0; i < 5; ++i)
        addKnob (toneIds[i], { 24 + 60 * i, 782, 60, 100 }, col::mint);

    panel ({ 338, 762, 256, 126 }, "DISTORTION / CRUSH");
    const char* crushIds[] = { "drive", "bits", "downsample", "crushmix" };
    const char* crushLabels[] = { "Drive", "Bits", "Downsample", "Crush Mix" };
    for (int i = 0; i < 4; ++i)
        addKnob (crushIds[i], { 346 + 60 * i, 782, 60, 100 }, col::coral, crushLabels[i]);

    panel ({ 600, 762, 218, 126 }, "SPACE");
    spaceSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    spaceSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    spaceSlider.setColour (juce::Slider::rotarySliderFillColourId, col::violet);
    spaceSlider.setBounds (612, 812, 130, 30);
    spaceSlider.setDoubleClickReturnValue (true, 0.0);
    content.addAndMakeVisible (spaceSlider);
    spaceAtt = std::make_unique<SliderAtt> (p.apvts, "space", spaceSlider);
    spaceValue.setBounds (612, 786, 130, 22);
    spaceValue.setFont (fontOf (15.f, true));
    spaceValue.setColour (juce::Label::textColourId, col::violet);
    spaceValue.setJustificationType (juce::Justification::left);
    spaceValue.setInterceptsMouseClicks (false, false);
    content.addAndMakeVisible (spaceValue);
    spaceSlider.onValueChange = [this] { spaceValue.setText (juce::String ((int) std::round (spaceSlider.getValue())) + " %", juce::dontSendNotification); };
    spaceSlider.onValueChange();
    caption ({ 612, 846, 130, 14 }, "reverb amount", juce::Justification::left, 10.f);
    addKnob ("spacesize", { 750, 782, 60, 100 }, col::violet);

    panel ({ 824, 762, 148, 126 }, "AUTO POLISH");
    addToggle (autoPolishBtn, "polish", "AUTO", { 834, 806, 62, 34 }, col::mint);
    addKnob ("polishamt", { 902, 782, 60, 100 }, col::mint);

    panel ({ 978, 762, 166, 126 }, "OUTPUT");
    addKnob ("mix",     { 986, 782, 60, 100 }, col::mint);
    addKnob ("outgain", { 1054, 782, 60, 100 }, col::mint);

    // ---------------- fx order ----------------
    panel ({ 16, 896, 1128, 58 }, "FX ORDER   -   AUTO uses the best-sounding order. Change any slot to take over (picking a stage swaps it with its current slot).");
    addToggle (autoOrderBtn, "fxauto", "AUTO ORDER", { 28, 918, 112, 28 }, col::mint);
    autoOrderBtn.onClick = [this] { if (autoOrderBtn.getToggleState()) resetOrder(); };
    for (int i = 0; i < 5; ++i)
    {
        const int x = 160 + i * 176;
        addCombo (slotBox[i], ("slot" + juce::String (i)).toRawUTF8(), { x, 918, 140, 28 });
        slotBox[i].onChange = [this, i] { slotChanged (i); };
        if (i < 4)
            caption ({ x + 140, 918, 36, 28 }, juce::String (">"), juce::Justification::centred, 16.f);
    }
    refreshSlots();

    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) kW / (double) kH);
    setResizeLimits (kW / 2, kH / 2, kW * 3 / 2, kH * 3 / 2);
    setSize (kW, kH);
}

PitchPilotEditor::~PitchPilotEditor()
{
    setLookAndFeel (nullptr);
}

void PitchPilotEditor::resized()
{
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) kW));
    content.setBounds (0, 0, kW, kH);
}

//==============================================================================
void PitchPilotEditor::refreshSlots()
{
    for (int i = 0; i < 5; ++i)
        slotShown[i] = (int) proc.getParamValue ("slot" + juce::String (i));
}

void PitchPilotEditor::resetOrder()
{
    for (int i = 0; i < 5; ++i)
        proc.setParamValue ("slot" + juce::String (i), (float) i);
    proc.setParamValue ("fxauto", 1.f);
    refreshSlots();
}

void PitchPilotEditor::slotChanged (int k)
{
    const int chosen = slotBox[k].getSelectedItemIndex();
    const int previous = slotShown[k];

    for (int j = 0; j < 5; ++j)                  // choosing a stage that is used elsewhere swaps the two slots
        if (j != k && (int) proc.getParamValue ("slot" + juce::String (j)) == chosen)
            proc.setParamValue ("slot" + juce::String (j), (float) previous);

    proc.setParamValue ("fxauto", 0.f);          // manual order from here on
    refreshSlots();
}
