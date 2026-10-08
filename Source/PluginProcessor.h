#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

//==============================================================================
// SHIFT/FX PRO — Universal DJ Transition Engine, by DJ Toolbox.
//
// Architecture: 23 factory presets, each a fixed recipe over 15 real DSP
// engines (echo, filter, roll, reverb, brake, crush, flanger, phaser, gate,
// reverse, backspin, sweep, pitch, wide, lofi). Only the selected preset's
// engine processes audio. Insert chain:
//   selected engine -> dry/wet -> output mix -> tanh soft clipper.
// Tempo follows the DAW host transport when bpmSource = Host and the host
// provides a valid tempo, otherwise the manual/tap BPM.
//
class ShiftFxProcessor : public juce::AudioProcessor
{
public:
    //---- Parameter IDs -------------------------------------------------------
    static constexpr const char* pidPreset = "preset";     // Choice: 23 presets
    static constexpr const char* pidOn     = "fxOn";       // Bool: effect engaged
    static constexpr const char* pidDryWet = "dryWet";     // Float 0..1
    static constexpr const char* pidOutMix = "outputMix";  // Float 0..1 -> -inf..+6 dB
    static constexpr const char* pidBpmSrc = "bpmSource";  // Choice: Host (DAW) / Manual
    static constexpr const char* pidBpm    = "bpmManual";  // Float 60..200
    static constexpr const char* pidTap    = "tapTempo";   // Bool momentary (UI trigger)

    // Parameters that can be MIDI-learned (indices into learnParamIds).
    enum Learnable { LearnPreset = 0, LearnDryWet = 1, LearnOutMix = 2, LearnOn = 3,
                     NumLearnable = 4 };
    static constexpr const char* learnParamIds[NumLearnable] =
        { pidPreset, pidDryWet, pidOutMix, pidOn };
    static constexpr const char* learnParamNames[NumLearnable] =
        { "Preset", "Dry/Wet", "Output Mix", "FX On/Off" };

    //---- Preset recipe --------------------------------------------------------
    // engine: 0 echo, 1 filter, 2 roll, 3 reverb, 4 brake, 5 crush, 6 flanger,
    //         7 phaser, 8 gate, 9 reverse, 10 backspin, 11 sweep, 12 pitch,
    //         13 wide, 14 lofi
    // beatDiv: index into presetDivBeats
    // echoStyle: 0 clean, 1 dub (dark), 2 ping-pong, 3 brake (slowing tail)
    // sweepStyle: 0 riser (swept bandpass), 1 noise (broadband burst)
    struct PresetDef
    {
        const char* name;
        const char* desc;
        int   engine;
        float depth;
        int   beatDiv;
        float brakeTime;
        int   echoStyle;
        int   sweepStyle;
    };

    static constexpr int numPresets = 23;
    static const PresetDef presets[numPresets];

    static constexpr int numDivs = 8;
    static constexpr float presetDivBeats[numDivs] =
        { 0.0625f, 0.125f, 0.25f, 0.5f, 0.75f, 1.0f, 2.0f, 4.0f };
    static constexpr const char* presetDivNames[numDivs] =
        { "1/16", "1/8", "1/4", "1/2", "3/4", "1", "2", "4" };

    ShiftFxProcessor();
    ~ShiftFxProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
#ifdef SHIFTFX_HEADLESS
    bool hasEditor() const override { return false; }
#else
    bool hasEditor() const override { return true; }
#endif

    const juce::String getName() const override { return "SHIFT FX PRO"; }
    bool acceptsMidi() const override { return true; }   // MIDI learn needs input
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

    //---- Cross-thread state (audio thread writes, editor reads) ---------------
    std::atomic<float> effectiveBpm { 128.0f };
    std::atomic<bool>  tempoFromHost { false };
    std::atomic<float> inLevelL { 0.0f }, inLevelR { 0.0f };
    std::atomic<float> outLevelL { 0.0f }, outLevelR { 0.0f };

    //---- MIDI learn ------------------------------------------------------------
    // learnArmed: which learnable param index is waiting for a CC (-1 = none).
    std::atomic<int>  learnArmed { -1 };
    std::atomic<int>  ccToParam[128];       // CC number -> Learnable index (-1 unbound)
    std::atomic<bool> learnEventFlag { false };
    std::atomic<int>  learnLastCc { -1 };
    std::atomic<int>  learnLastParam { -1 };

    bool isFavorite (int presetIdx) const;
    void setFavorite (int presetIdx, bool fav);

private:
    // Cached raw parameter values (audio-thread safe)
    std::atomic<float>* pPreset = nullptr;
    std::atomic<float>* pOn     = nullptr;
    std::atomic<float>* pDryWet = nullptr;
    std::atomic<float>* pOutMix = nullptr;
    std::atomic<float>* pBpmSrc = nullptr;
    std::atomic<float>* pBpm    = nullptr;

    double currentSampleRate = 44100.0;
    juce::Array<int> favorites; // persisted in state

    void applyMidiMessage (const juce::MidiMessage& msg);
    void writeExtraState (juce::ValueTree& state);
    void readExtraState (const juce::ValueTree& state);

    //---- Echo: tempo-synced stereo delay --------------------------------------
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> echoDelay[2];
    float echoFbMem[2]    = { 0.0f, 0.0f };
    float echoCurDelay[2] = { 0.0f, 0.0f };

    //---- Filter: resonant lowpass <-> highpass morph --------------------------
    juce::dsp::StateVariableTPTFilter<float> svf[2];

    //---- Roll: beat-repeat from a ring of the last 4 beats -------------------
    juce::AudioBuffer<float> rollRing;
    int   rollWritePos = 0;
    float rollReadPos  = 0.0f;
    bool  rollEngaged  = false;

    //---- Reverb ---------------------------------------------------------------
    juce::dsp::Reverb reverb;

    //---- Brake: tape-stop -----------------------------------------------------
    juce::AudioBuffer<float> brakeRing;
    int    brakeWritePos  = 0;
    bool   brakeEngaged   = false;
    float  brakeReadPosCh[2] = { 0.0f, 0.0f };
    double brakeElapsed   = 0.0;
    int    brakeReleaseLeft  = 0;
    int    brakeReleaseTotal = 1;
    float  brakeLastOut[2]   = { 0.0f, 0.0f };
    bool   prevFxOn = false;

    //---- Crusher ---------------------------------------------------------------
    float crushHeld[2]  = { 0.0f, 0.0f };
    int   crushCount[2] = { 0, 0 };

    //---- Flanger ----------------------------------------------------------------
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> flangerDelay[2];
    float flangerPhase = 0.0f;

    //---- Phaser ------------------------------------------------------------------
    juce::dsp::Phaser<float> phaser;

    //---- Gate ---------------------------------------------------------------------
    float gatePhase = 0.0f;
    float gateState[2] = { 0.0f, 0.0f };

    //---- Reverse --------------------------------------------------------------------
    juce::AudioBuffer<float> reverseRing;
    int   reverseWritePos = 0;
    float reverseReadPos  = 0.0f;
    bool  reverseEngaged  = false;

    //---- Backspin ----------------------------------------------------------------------
    juce::AudioBuffer<float> backspinRing;
    int   backspinWritePos = 0;
    bool  backspinEngaged  = false;
    float backspinReadPos[2] = { 0.0f, 0.0f };
    double backspinElapsed = 0.0;
    float backspinLastOut[2] = { 0.0f, 0.0f };

    //---- Sweep ------------------------------------------------------------------------------
    juce::dsp::StateVariableTPTFilter<float> sweepFilter[2];
    juce::uint32 sweepRand = 0x12345678u;

    //---- Pitch -----------------------------------------------------------------------------------
    juce::AudioBuffer<float> pitchBuf;
    int   pitchWritePos = 0;
    float pitchReadPos[2] = { 0.0f, 0.0f };
    float pitchRateSm = 1.0f;
    float pitchFadeFrom[2] = { 0.0f, 0.0f };
    int   pitchFadeLeft[2] = { 0, 0 };

    //---- Lofi -----------------------------------------------------------------------------------------
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> wowDelay[2];
    juce::dsp::StateVariableTPTFilter<float> lofiLp[2];
    float lofiPhase = 0.0f;
    juce::uint32 lofiRand = 0x87654321u;

    juce::AudioBuffer<float> dryCopy;

    //---- Hardening state ---------------------------------------------------------
    int   currentEngine = -1;  // engine index of the last processed block
    int   engineFadePos = 0;   // fade-in position after an engine change
    int   engineFadeLen = 1;   // ~15 ms in samples
    float wetSmooth     = 0.0f; // smoothed on/off (click-free engage/disengage)

    void clearEngineState();   // reset all DSP state on engine change

    float beatsToSamples (float beats, float bpm) const;
    float readRingLinear (const juce::AudioBuffer<float>& ring, int ch, float pos) const;
    static float xorshiftNoise (juce::uint32& state);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ShiftFxProcessor)
};
