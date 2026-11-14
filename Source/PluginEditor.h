#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include <memory>
#include <vector>

#include "PluginProcessor.h"

namespace ui
{
namespace col
{
    inline const juce::Colour bg      { 0xff111318 };
    inline const juce::Colour panel   { 0xff1a1d23 };
    inline const juce::Colour panelHi { 0xff262a33 };
    inline const juce::Colour line    { 0xff333845 };
    inline const juce::Colour text    { 0xffe9edf3 };
    inline const juce::Colour dim     { 0xff8a93a3 };
    inline const juce::Colour mint    { 0xff4de3b5 };
    inline const juce::Colour amber   { 0xffffb547 };
    inline const juce::Colour coral   { 0xffff6b7a };
    inline const juce::Colour violet  { 0xff9b8cff };
}

juce::Font fontOf (float height, bool bold = false);

//==============================================================================
class PilotLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PilotLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float minSliderPos, float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
};

//==============================================================================
// Rotary knob bound to a float parameter from pp::kFloats, with name label and value readout.
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const char* paramId, juce::Colour accent, const juce::String& labelOverride = {});
    void resized() override;

private:
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

//==============================================================================
// Scrolling pitch display: grey = what you sang, mint = what comes out, lanes = note targets.
class TraceView : public juce::Component, private juce::Timer
{
public:
    explicit TraceView (PitchPilotProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    PitchPilotProcessor& proc;
    float centre = 57.f;
};

//==============================================================================
// Per-note cents (drag), Bypass and Remove cells.
class NoteStrip : public juce::Component, private juce::Timer
{
public:
    explicit NoteStrip (PitchPilotProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    enum Row { header = 0, cents, bypass, remove };
    static constexpr int labelW = 64, colW = 88;
    juce::Rectangle<int> cell (int pc, int row) const;
    bool hit (juce::Point<int>, int& pc, int& row) const;
    void timerCallback() override { repaint(); }

    PitchPilotProcessor& proc;
    juce::RangedAudioParameter* dragParam = nullptr;
    float dragStartValue = 0.f;
    int dragStartY = 0;
};

//==============================================================================
// Two-octave on-screen keyboard. Click edits Remove/Bypass, or plays target notes when "Target Notes" is on.
class KeyboardView : public juce::Component, private juce::Timer
{
public:
    explicit KeyboardView (PitchPilotProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    std::function<int()> getEditMode;     // 0 = Remove, 1 = Bypass
    std::function<bool()> isLatch;

private:
    static bool isBlack (int pc) { return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; }
    juce::Rectangle<float> keyRect (int key) const;
    int keyAt (juce::Point<int>) const;
    void timerCallback() override { repaint(); }

    PitchPilotProcessor& proc;
    int heldPc = -1;
};

//==============================================================================
} // namespace ui

class PitchPilotEditor : public juce::AudioProcessorEditor
{
public:
    explicit PitchPilotEditor (PitchPilotProcessor&);
    ~PitchPilotEditor() override;
    void resized() override;

private:
    static constexpr int kW = 1160, kH = 960;

    class Content : public juce::Component
    {
    public:
        struct Panel   { juce::Rectangle<int> r; juce::String title; };
        struct Caption { juce::Rectangle<int> r; juce::String text; juce::Justification just; float size; };
        std::vector<Panel> panels;
        std::vector<Caption> captions;
        void paint (juce::Graphics&) override;
    };

    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;

    PitchPilotProcessor& proc;
    ui::PilotLookAndFeel lnf;
    Content content;
    ui::TraceView trace;
    ui::NoteStrip strip;
    ui::KeyboardView keyboard;

    juce::ComboBox inputBox, keyBox, scaleBox, vibShapeBox;
    juce::TextButton classicBtn, formantBtn, ignoreVibBtn, holdBtn, targetNotesBtn, autoPolishBtn;
    juce::TextButton setMajorBtn, setMinorBtn, setAllBtn, bypassAllBtn, removeAllBtn, learnBtn;
    juce::TextButton editRemoveBtn, editBypassBtn, latchBtn, momentaryBtn;
    juce::TextButton fakeMinkBtn, esDeeKidBtn, autoOrderBtn;
    juce::ComboBox slotBox[5];
    int slotShown[5] { 0, 1, 2, 3, 4 };
    juce::Slider spaceSlider;
    juce::Label spaceValue;

    std::vector<std::unique_ptr<ui::Knob>> knobs;
    std::vector<std::unique_ptr<ButtonAtt>> buttonAtts;
    std::vector<std::unique_ptr<ComboAtt>> comboAtts;
    std::unique_ptr<SliderAtt> spaceAtt;

    void refreshSlots();
    void resetOrder();          // back to the automatic best-sounding order
    void slotChanged (int slot);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchPilotEditor)
};
