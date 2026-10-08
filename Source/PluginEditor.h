#pragma once

#include "PluginProcessor.h"

//==============================================================================
// Small UI building blocks for the SHIFT/FX PRO panel.
//==============================================================================

// Generic rotary knob: vertical drag + mouse wheel.
class TfxKnob : public juce::Component
{
public:
    TfxKnob (float diameterPx, juce::Colour accentColour, int numDetents = 0);

    void setValue (float v, bool notify = true);
    float getValue() const { return value; }

    void setLearnable (int learnIdx) { this->learnIdx = learnIdx; }
    void setLearnModeActive (bool b) { learnMode = b; repaint(); }
    void setLearnHighlight (bool b) { learnHighlight = b; repaint(); }

    std::function<void(float)> onChange;
    std::function<void(int)>   onLearnArm;   // called with learnIdx when armed

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent&,
                         const juce::MouseWheelDetails& wheel) override;

private:
    float value = 0.0f;
    float dragStartVal = 0.0f;
    int   dragStartY = 0;
    float diameter;
    juce::Colour accent;
    int detents; // 0 = continuous
    int learnIdx = -1;
    bool learnMode = false, learnHighlight = false;
};

// The giant 360-degree preset selector with 23 labeled detents.
class PresetWheel : public juce::Component
{
public:
    PresetWheel();

    void setPreset (int idx, bool notify = true);
    int  getPreset() const { return preset; }

    void setLearnable (int learnIdx) { this->learnIdx = learnIdx; }
    void setLearnModeActive (bool b) { learnMode = b; repaint(); }
    void setLearnHighlight (bool b) { learnHighlight = b; repaint(); }

    std::function<void(int)> onChange;
    std::function<void(int)> onLearnArm;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent&,
                         const juce::MouseWheelDetails& wheel) override;

private:
    int preset = 0;
    int dragStartPreset = 0;
    int dragStartY = 0;
    int learnIdx = -1;
    bool learnMode = false, learnHighlight = false;

    static float detentAngle (int idx); // radians, -PI/2 = top
};

// Horizontal stereo level meter (two bars).
class LevelMeter : public juce::Component
{
public:
    void setLevels (float l, float r);
    void paint (juce::Graphics& g) override;
private:
    float lv[2] = { 0.0f, 0.0f };
    float pk[2] = { 0.0f, 0.0f };
};

// Backlit power button.
class PowerButton : public juce::Component
{
public:
    void setOn (bool o, bool notify = true);
    bool isOn() const { return on; }

    void setLearnable (int learnIdx) { this->learnIdx = learnIdx; }
    void setLearnModeActive (bool b) { learnMode = b; repaint(); }
    void setLearnHighlight (bool b) { learnHighlight = b; repaint(); }

    std::function<void(bool)> onChange;
    std::function<void(int)>  onLearnArm;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;

private:
    bool on = false;
    int learnIdx = -1;
    bool learnMode = false, learnHighlight = false;
};

// Small icon buttons: star / save / settings / arrows.
class GlyphButton : public juce::Component
{
public:
    enum Glyph { Star, Save, Settings, ArrowLeft, ArrowRight };

    explicit GlyphButton (Glyph g);

    std::function<void()> onClick;
    void setActive (bool a) { active = a; repaint(); } // star: filled favorite

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { hover = false; repaint(); }

private:
    Glyph glyph;
    bool active = false, hover = false;
};

// Preset name + description display panel.
class PresetDisplay : public juce::Component
{
public:
    void setPreset (const ShiftFxProcessor::PresetDef& p, bool favorite);
    void paint (juce::Graphics& g) override;
private:
    juce::String name, desc;
    bool favorite = false;
};

//==============================================================================
class ShiftFxEditor : public juce::AudioProcessorEditor,
                      private juce::Timer
{
public:
    explicit ShiftFxEditor (ShiftFxProcessor&);
    ~ShiftFxEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    void setParam01 (const char* pid, float v01);
    void setPresetIndex (int idx);
    void stepPreset (int dir);
    void doTapTempo();
    void armLearn (int learnIdx);
    void setLearnMode (bool b);

    ShiftFxProcessor& processor;

    // Top bar
    juce::Label bpmLabel;
    juce::ComboBox bpmSourceBox;
    juce::TextButton tapButton { "TAP" };
    std::unique_ptr<GlyphButton> presetArrowL, presetArrowR;
    std::unique_ptr<PresetDisplay> presetDisplay;
    std::unique_ptr<GlyphButton> starButton, saveButton, settingsButton;
    std::unique_ptr<LevelMeter> inMeterTop, outMeterTop;

    // Center
    std::unique_ptr<PresetWheel> presetWheel;
    std::unique_ptr<GlyphButton> centerArrowL, centerArrowR;
    std::unique_ptr<PresetDisplay> centerDisplay;

    // Bottom bar
    std::unique_ptr<LevelMeter> inMeterBottom;
    std::unique_ptr<TfxKnob> dryWetKnob, outMixKnob;
    std::unique_ptr<PowerButton> powerButton;
    juce::TextButton learnButton { "LEARN" };
    juce::Label learnStatusLabel;

    bool learnMode = false;
    juce::Array<juce::uint32> tapTimes;
    juce::String saveFlash;
    juce::uint32 saveFlashUntil = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ShiftFxEditor)
};
