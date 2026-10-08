//==============================================================================
// ShiftFxProcessor — 23 presets over 15 DSP engines.
//==============================================================================

#include "PluginProcessor.h"
#ifndef SHIFTFX_HEADLESS
#include "PluginEditor.h"
#endif

//==============================================================================
// The 23 factory presets. Order matches the knob labels clockwise from the top.
// beatDiv indexes presetDivBeats {1/16,1/8,1/4,1/2,3/4,1,2,4}.
//==============================================================================
const ShiftFxProcessor::PresetDef ShiftFxProcessor::presets[numPresets] =
{
    // name                description                                          eng  depth div  brakeT eSty sSty
    { "CLEAN ECHO OUT",  "Smooth tempo-synced echo with a clean tail.",          0,  0.35f, 3,  0.8f,  0,   0  },
    { "VERSION ECHO",    "Dub-style echo with a dark, rolling tail.",            0,  0.70f, 3,  0.8f,  1,   0  },
    { "PING PONG DELAY", "Stereo echo bouncing left to right.",                  0,  0.55f, 2,  0.8f,  2,   0  },
    { "BRAKE ECHO",      "Echo whose tail slows to a stop.",                     0,  0.65f, 3,  0.8f,  3,   0  },
    { "REPEATER",        "Tight 1/8 beat repeat for builds.",                    2,  0.90f, 1,  0.8f,  0,   0  },
    { "ROLL",            "Classic 1/4 loop roll.",                               2,  0.90f, 2,  0.8f,  0,   0  },
    { "STUTTER ROLL",    "Frantic 1/16 stutter roll.",                           2,  0.95f, 0,  0.8f,  0,   0  },
    { "TURNTABLE BRAKE", "Full vinyl stop. Instant drama.",                      4,  1.00f, 3,  0.8f,  0,   0  },
    { "FILTER SWEEP",    "Resonant filter sweep for transitions.",               1,  0.85f, 3,  0.8f,  0,   0  },
    { "VINYL BRAKE",     "Quick pitch-drop brake, vinyl style.",                 4,  1.00f, 3,  0.35f, 0,   0  },
    { "LO-FI",           "Worn vinyl: wow, dust and crackle.",                   14, 0.70f, 3,  0.8f,  0,   0  },
    { "PITCH",           "Tape-style pitch drop, down an octave.",               12, 1.00f, 3,  0.8f,  0,   0  },
    { "TRANSFORM",       "Transformer chop gate. Rhythmic cuts.",                8,  0.90f, 1,  0.8f,  0,   0  },
    { "GATE",            "Beat-synced trance gate.",                             8,  0.70f, 2,  0.8f,  0,   0  },
    { "RISER",           "Noise riser that lifts into the drop.",                11, 0.90f, 3,  0.8f,  0,   0  },
    { "REVERB WASH",     "Huge reverb wash for breakdowns.",                     3,  0.90f, 3,  0.8f,  0,   0  },
    { "SPACE",           "Cavernous space reverb.",                              3,  1.00f, 3,  0.8f,  0,   0  },
    { "DUB ECHO",        "Dark, heavy-feedback dub echo.",                       0,  0.80f, 4,  0.8f,  1,   0  },
    { "SWEEP",           "Filtered noise sweep.",                                11, 0.70f, 3,  0.8f,  0,   0  },
    { "NOISE",           "Raw noise burst for impact.",                          11, 1.00f, 3,  0.8f,  0,   1  },
    { "CRUSH",           "Bit-crushed digital grit.",                            5,  0.75f, 3,  0.8f,  0,   0  },
    { "FILTER",          "DJ-style resonant filter.",                            1,  0.60f, 3,  0.8f,  0,   0  },
    { "ECHO OUT",        "The classic echo-out transition.",                     0,  0.90f, 3,  0.8f,  0,   0  },
};

//==============================================================================
ShiftFxProcessor::ShiftFxProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    pPreset = apvts.getRawParameterValue (pidPreset);
    pOn     = apvts.getRawParameterValue (pidOn);
    pDryWet = apvts.getRawParameterValue (pidDryWet);
    pOutMix = apvts.getRawParameterValue (pidOutMix);
    pBpmSrc = apvts.getRawParameterValue (pidBpmSrc);
    pBpm    = apvts.getRawParameterValue (pidBpm);

    for (int i = 0; i < 128; ++i)
        ccToParam[i].store (-1);
}

ShiftFxProcessor::~ShiftFxProcessor() = default;

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout ShiftFxProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    juce::StringArray presetChoices;
    for (int i = 0; i < numPresets; ++i)
        presetChoices.add (presets[i].name);

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        pidPreset, "Preset", presetChoices, 0));

    layout.add (std::make_unique<juce::AudioParameterBool> (pidOn, "FX On", false));

    auto twoDecimals = [] (float v, int) { return juce::String (v, 2); };

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        pidDryWet, "Dry/Wet",
        juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (twoDecimals)));

    // 0..1 mapped to -inf..+6 dB; default 0.9091 = 0 dB.
    auto dbText = [] (float v, int)
    {
        if (v <= 0.001f) return juce::String ("-inf");
        return juce::String (-60.0f + v * 66.0f, 1) + " dB";
    };
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        pidOutMix, "Output Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.9091f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (dbText)));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        pidBpmSrc, "BPM Source",
        juce::StringArray { "Host (DAW)", "Manual" }, 0));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        pidBpm, "Manual BPM",
        juce::NormalisableRange<float> (60.0f, 200.0f, 0.1f), 128.0f));

    // Momentary UI/automation trigger for tap tempo. The tap-to-BPM math runs
    // in the editor (message thread); the DSP does not use this value.
    layout.add (std::make_unique<juce::AudioParameterBool> (pidTap, "Tap Tempo", false));

    return layout;
}

//==============================================================================
void ShiftFxProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };

    const int echoMax = (int) (4.1 * sampleRate) + 64;
    for (int c = 0; c < 2; ++c)
    {
        echoDelay[c].setMaximumDelayInSamples (echoMax);
        echoDelay[c].prepare (spec);
        echoDelay[c].reset();
        echoFbMem[c] = 0.0f;
        echoCurDelay[c] = 0.0f;
    }

    for (int c = 0; c < 2; ++c)
    {
        svf[c].prepare (spec);
        svf[c].reset();
    }

    rollRing.setSize (2, (int) (4.2 * sampleRate) + 128, false, true, true);
    rollWritePos = 0;
    rollReadPos  = 0.0f;
    rollEngaged  = false;

    reverb.prepare (spec);
    reverb.reset();

    brakeRing.setSize (2, (int) (2.3 * sampleRate) + 128, false, true, true);
    brakeWritePos    = 0;
    brakeEngaged     = false;
    brakeReleaseLeft = 0;

    dryCopy.setSize (2, samplesPerBlock, false, true, true);

    crushHeld[0] = crushHeld[1] = 0.0f;
    crushCount[0] = crushCount[1] = 0;
    prevFxOn = false;

    const int flangerMax = (int) (0.02 * sampleRate) + 64;
    for (int c = 0; c < 2; ++c)
    {
        flangerDelay[c].setMaximumDelayInSamples (flangerMax);
        flangerDelay[c].prepare (spec);
        flangerDelay[c].reset();
    }
    flangerPhase = 0.0f;

    phaser.prepare (spec);
    phaser.reset();

    gatePhase = 0.0f;
    gateState[0] = gateState[1] = 0.0f;

    reverseRing.setSize (2, (int) (4.2 * sampleRate) + 128, false, true, true);
    reverseWritePos = 0;
    reverseReadPos  = 0.0f;
    reverseEngaged  = false;

    backspinRing.setSize (2, (int) (2.3 * sampleRate) + 128, false, true, true);
    backspinWritePos   = 0;
    backspinEngaged    = false;
    backspinReadPos[0] = backspinReadPos[1] = 0.0f;
    backspinElapsed    = 0.0;

    for (int c = 0; c < 2; ++c)
    {
        sweepFilter[c].prepare (spec);
        sweepFilter[c].reset();
        sweepFilter[c].setType (juce::dsp::StateVariableTPTFilterType::bandpass);
        sweepFilter[c].setResonance (2.0f);
    }
    sweepRand = 0x12345678u;

    pitchBuf.setSize (2, (int) (0.12 * sampleRate) + 128, false, true, true);
    pitchWritePos = 0;
    pitchReadPos[0] = pitchReadPos[1] = 0.0f;
    pitchRateSm = 1.0f;
    pitchFadeLeft[0] = pitchFadeLeft[1] = 0;

    const int wowMax = (int) (0.03 * sampleRate) + 64;
    for (int c = 0; c < 2; ++c)
    {
        wowDelay[c].setMaximumDelayInSamples (wowMax);
        wowDelay[c].prepare (spec);
        wowDelay[c].reset();
        lofiLp[c].prepare (spec);
        lofiLp[c].reset();
        lofiLp[c].setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        lofiLp[c].setCutoffFrequency (6000.0f);
        lofiLp[c].setResonance (0.7f);
    }
    lofiPhase = 0.0f;
    lofiRand  = 0x87654321u;

    // Hardening: force a clean fade-in on the first block after (re-)init.
    currentEngine = -1;
    engineFadePos = 0;
    engineFadeLen = juce::jmax (1, (int) (0.015 * sampleRate));
    wetSmooth = 0.0f;

    effectiveBpm.store (juce::jlimit (60.0f, 200.0f, pBpm->load()));
}

void ShiftFxProcessor::releaseResources()
{
    echoDelay[0].reset(); echoDelay[1].reset();
    reverb.reset();
    phaser.reset();
    flangerDelay[0].reset(); flangerDelay[1].reset();
    wowDelay[0].reset(); wowDelay[1].reset();
}

// Reset every engine's DSP state when the preset's engine changes, so no
// stale delay/reverb content leaks into the new engine. Brake/backspin are
// drained through a short release instead of being cut.
void ShiftFxProcessor::clearEngineState()
{
    for (int c = 0; c < 2; ++c)
    {
        echoDelay[c].reset();
        echoFbMem[c] = 0.0f;
        echoCurDelay[c] = 0.0f;
        svf[c].reset();
        flangerDelay[c].reset();
        wowDelay[c].reset();
        lofiLp[c].reset();
        sweepFilter[c].reset();
    }
    phaser.reset();
    reverb.reset();

    rollEngaged = false;
    reverseEngaged = false;

    if (brakeEngaged || backspinEngaged)
    {
        brakeEngaged = false;
        backspinEngaged = false;
        brakeReleaseTotal = juce::jmax (1, (int) (0.02 * currentSampleRate));
        brakeReleaseLeft  = brakeReleaseTotal;
    }

    crushHeld[0] = crushHeld[1] = 0.0f;
    crushCount[0] = crushCount[1] = 0;
    flangerPhase = 0.0f;
    gatePhase = 0.0f;
    lofiPhase = 0.0f;
    gateState[0] = gateState[1] = 0.0f;
    pitchRateSm = 1.0f;
    pitchFadeLeft[0] = pitchFadeLeft[1] = 0;

    prevFxOn = false; // force a rising-edge engage for the new engine
}

// Bypass: dry pass-through with meters kept alive.
void ShiftFxProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer&)
{
    const int numSamples = buffer.getNumSamples();
    const int ch = juce::jmin (getTotalNumInputChannels(),
                               getTotalNumOutputChannels(), 2);
    float pkL = 0.0f, pkR = 0.0f;
    if (ch >= 1) pkL = buffer.getMagnitude (0, 0, numSamples);
    if (ch >= 2) pkR = buffer.getMagnitude (1, 0, numSamples);
    else         pkR = pkL;

    inLevelL.store  (juce::jmax (pkL, inLevelL.load()  * 0.92f));
    inLevelR.store  (juce::jmax (pkR, inLevelR.load()  * 0.92f));
    outLevelL.store (juce::jmax (pkL, outLevelL.load() * 0.92f));
    outLevelR.store (juce::jmax (pkR, outLevelR.load() * 0.92f));
}

// Fast xorshift white noise in [-1, 1]; no locks, no heap.
float ShiftFxProcessor::xorshiftNoise (juce::uint32& s)
{
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return ((float) (s >> 8) / (float) 0xFFFFFFu) * 2.0f - 1.0f;
}

//==============================================================================
float ShiftFxProcessor::beatsToSamples (float beats, float bpm) const
{
    const float safeBpm = juce::jlimit (30.0f, 300.0f, bpm);
    return beats * (60.0f / safeBpm) * (float) currentSampleRate;
}

float ShiftFxProcessor::readRingLinear (const juce::AudioBuffer<float>& ring,
                                        int ch, float pos) const
{
    const int size = ring.getNumSamples();
    float p = std::fmod (pos, (float) size);
    if (p < 0.0f) p += (float) size;

    const int i0 = (int) p;
    const int i1 = (i0 + 1) % size;
    const float frac = p - (float) i0;

    const float* d = ring.getReadPointer (ch);
    return d[i0] + frac * (d[i1] - d[i0]);
}

//==============================================================================
bool ShiftFxProcessor::isFavorite (int presetIdx) const
{
    return favorites.contains (presetIdx);
}

void ShiftFxProcessor::setFavorite (int presetIdx, bool fav)
{
    if (fav && ! favorites.contains (presetIdx))
        favorites.add (presetIdx);
    else if (! fav)
        favorites.removeAllInstancesOf (presetIdx);
}

//==============================================================================
void ShiftFxProcessor::applyMidiMessage (const juce::MidiMessage& msg)
{
    if (! msg.isController())
        return;

    const int cc  = msg.getControllerNumber();
    const int val = msg.getControllerValue();
    if (cc < 0 || cc > 127)
        return;

    // Learn mode: bind this CC to the armed parameter.
    const int armed = learnArmed.load();
    if (armed >= 0 && armed < NumLearnable)
    {
        ccToParam[cc].store (armed);
        learnArmed.store (-1);
        learnLastCc.store (cc);
        learnLastParam.store (armed);
        learnEventFlag.store (true);
        return;
    }

    // Normal mode: a bound CC drives its parameter.
    const int target = ccToParam[cc].load();
    if (target < 0 || target >= NumLearnable)
        return;

    if (auto* param = apvts.getParameter (learnParamIds[target]))
    {
        const float norm = (float) val / 127.0f;
        if (target == LearnPreset)
        {
            const int idx = juce::jlimit (0, numPresets - 1, (int) (norm * (float) numPresets));
            param->setValueNotifyingHost (param->convertTo0to1 ((float) idx));
        }
        else if (target == LearnOn)
        {
            param->setValueNotifyingHost (val >= 64 ? 1.0f : 0.0f);
        }
        else
        {
            param->setValueNotifyingHost (norm);
        }
    }
}

//==============================================================================
void ShiftFxProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                     juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    if (numIn == 0 || numOut == 0)
        return;

    const int procCh = juce::jmin (numIn, numOut, 2);

    //---- MIDI learn / control --------------------------------------------------
    for (const auto meta : midiMessages)
        applyMidiMessage (meta.getMessage());

    //---- Resolve the active preset into engine + settings -----------------------
    const int presetIdx = juce::jlimit (0, numPresets - 1, (int) std::lround (pPreset->load()));
    const PresetDef& pr = presets[presetIdx];

    const int   sel       = pr.engine;
    const float depth     = pr.depth;
    const float beats     = presetDivBeats[juce::jlimit (0, numDivs - 1, pr.beatDiv)];
    const float brakeT    = juce::jlimit (0.2f, 2.0f, pr.brakeTime);
    const int   echoStyle = pr.echoStyle;
    const int   sweepStyle = pr.sweepStyle;

    const bool  fxOn    = pOn->load() > 0.5f;
    const float dryWet  = juce::jlimit (0.0f, 1.0f, pDryWet->load());
    const float outMixV = juce::jlimit (0.0f, 1.0f, pOutMix->load());
    const float outGain = outMixV <= 0.001f ? 0.0f
                        : juce::Decibels::decibelsToGain (-60.0f + outMixV * 66.0f);
    const float manualB = pBpm->load();
    const bool  useHost = pBpmSrc->load() < 0.5f;

    //---- Engine change: drain old state, fade the new engine in (~15 ms) --------
    if (sel != currentEngine)
    {
        currentEngine = sel;
        engineFadePos = 0;
        engineFadeLen = juce::jmax (1, (int) (0.015 * currentSampleRate));
        clearEngineState();
    }

    //---- Click-free on/off: fast exponential fade of the wet path (~8 ms) -------
    const float wetTarget = fxOn ? 1.0f : 0.0f;
    wetSmooth += (wetTarget - wetSmooth)
               * (1.0f - std::exp (-(float) numSamples / (0.008f * (float) currentSampleRate)));
    const bool runFx = fxOn || wetSmooth > 0.0005f;

    //---- Tempo: host transport first (when selected), manual fallback ------------
    float bpm = manualB;
    bool fromHost = false;
    if (useHost)
    {
        if (auto* playHead = getPlayHead())
        {
            if (auto pos = playHead->getPosition(); pos.hasValue())
            {
                if (auto hostBpm = pos->getBpm(); hostBpm.hasValue() && *hostBpm > 0.0)
                {
                    bpm = (float) *hostBpm;
                    fromHost = true;
                }
            }
        }
    }
    effectiveBpm.store (bpm);
    tempoFromHost.store (fromHost);

    // Keep a dry copy for the final mix, then clear any extra output channels.
    dryCopy.makeCopyOf (buffer, true);
    for (int i = numIn; i < numOut; ++i)
        buffer.clear (i, 0, numSamples);

    //---- Input meters (pre-FX) ----------------------------------------------------
    {
        float pkL = 0.0f, pkR = 0.0f;
        if (procCh >= 1) pkL = dryCopy.getMagnitude (0, 0, numSamples);
        if (procCh >= 2) pkR = dryCopy.getMagnitude (1, 0, numSamples);
        else             pkR = pkL;
        inLevelL.store (juce::jmax (pkL, inLevelL.load() * 0.92f));
        inLevelR.store (juce::jmax (pkR, inLevelR.load() * 0.92f));
    }

    // Capture rings kept fresh while their engine's preset is selected.
    if (sel == 4) // brake
    {
        const int ringSize = brakeRing.getNumSamples();
        for (int c = 0; c < procCh; ++c)
        {
            const float* d = buffer.getReadPointer (c);
            for (int n = 0; n < numSamples; ++n)
                brakeRing.setSample (c, (brakeWritePos + n) % ringSize, d[n]);
        }
        brakeWritePos = (brakeWritePos + numSamples) % ringSize;
    }
    if (sel == 9) // reverse
    {
        const int ringSize = reverseRing.getNumSamples();
        for (int c = 0; c < procCh; ++c)
        {
            const float* d = buffer.getReadPointer (c);
            for (int n = 0; n < numSamples; ++n)
                reverseRing.setSample (c, (reverseWritePos + n) % ringSize, d[n]);
        }
        reverseWritePos = (reverseWritePos + numSamples) % ringSize;
    }
    if (sel == 10) // backspin
    {
        const int ringSize = backspinRing.getNumSamples();
        for (int c = 0; c < procCh; ++c)
        {
            const float* d = buffer.getReadPointer (c);
            for (int n = 0; n < numSamples; ++n)
                backspinRing.setSample (c, (backspinWritePos + n) % ringSize, d[n]);
        }
        backspinWritePos = (backspinWritePos + numSamples) % ringSize;
    }

    //==== FX processing (in place on `buffer`, up to stereo) ====================
    // Skipped entirely once the wet path has fully faded out (saves CPU and
    // keeps state frozen while bypassed).
    if (runFx)
    {
        switch (sel)
        {
            //---------------- ECHO ----------------
            case 0:
            {
                // echoStyle: 0 clean, 1 dub (dark), 2 ping-pong, 3 brake (slowing tail)
                float feedback = depth * 0.85f;
                float lpHz     = 3200.0f;
                float crossA   = 0.75f, crossB = 0.25f;
                float delaySmooth = 0.002f;
                float delayMul    = 1.0f;
                if (echoStyle == 1)      { feedback = depth * 0.95f; lpHz = 1200.0f; crossA = 0.85f; crossB = 0.15f; }
                else if (echoStyle == 2) { crossA = 0.5f; crossB = 0.5f; }
                else if (echoStyle == 3) { lpHz = 1800.0f; delayMul = 1.5f; delaySmooth = 0.0008f; }

                const float lpCoeff = std::exp (-2.0f * juce::MathConstants<float>::pi * lpHz
                                                / (float) currentSampleRate);
                const float targetDelay = juce::jmin (beatsToSamples (beats, bpm) * delayMul,
                                                     (float) echoDelay[0].getMaximumDelayInSamples() - 2.0f);
                for (int c = 0; c < procCh; ++c)
                {
                    auto* d = buffer.getWritePointer (c);
                    for (int n = 0; n < numSamples; ++n)
                    {
                        echoCurDelay[c] += (targetDelay - echoCurDelay[c]) * delaySmooth;
                        echoDelay[c].setDelay (echoCurDelay[c]);

                        const float delayed = echoDelay[c].popSample (c);
                        echoFbMem[c] = lpCoeff * echoFbMem[c] + (1.0f - lpCoeff) * delayed;

                        const float other = echoFbMem[1 - c];
                        const float fbSum = crossA * echoFbMem[c] + crossB * other;

                        echoDelay[c].pushSample (c, d[n] + fbSum * feedback);
                        d[n] = delayed; // wet only; dry/wet blends the dry back in
                    }
                }
                break;
            }

            //---------------- FILTER ----------------
            case 1:
            {
                const float morph = depth * 2.0f - 1.0f; // -1..1, 0 = bypass
                if (std::abs (morph) > 0.02f)
                {
                    for (int c = 0; c < procCh; ++c)
                    {
                        if (morph < 0.0f)
                        {
                            svf[c].setType (juce::dsp::StateVariableTPTFilterType::lowpass);
                            svf[c].setCutoffFrequency (18000.0f * std::pow (70.0f / 18000.0f, -morph));
                        }
                        else
                        {
                            svf[c].setType (juce::dsp::StateVariableTPTFilterType::highpass);
                            svf[c].setCutoffFrequency (20.0f * std::pow (16000.0f / 20.0f, morph));
                        }
                        svf[c].setResonance (1.0f + 1.6f * std::abs (morph));

                        auto* d = buffer.getWritePointer (c);
                        for (int n = 0; n < numSamples; ++n)
                            d[n] = svf[c].processSample (c, d[n]);
                    }
                }
                break;
            }

            //---------------- ROLL ----------------
            case 2:
            {
                const int ringSize = rollRing.getNumSamples();

                for (int c = 0; c < procCh; ++c)
                {
                    const float* d = buffer.getReadPointer (c);
                    for (int n = 0; n < numSamples; ++n)
                        rollRing.setSample (c, (rollWritePos + n) % ringSize, d[n]);
                }
                const int newWritePos = (rollWritePos + numSamples) % ringSize;

                float sliceLen = beatsToSamples (beats, bpm);
                sliceLen = juce::jlimit (64.0f, (float) ringSize - 64.0f, sliceLen);

                if (! rollEngaged)
                {
                    rollEngaged = true;
                    rollReadPos = (float) newWritePos - sliceLen;
                }

                const float fadeLen = juce::jmin (256.0f, sliceLen * 0.125f);
                const float sliceStart = (float) newWritePos - sliceLen;

                for (int c = 0; c < procCh; ++c)
                {
                    auto* d = buffer.getWritePointer (c);
                    for (int n = 0; n < numSamples; ++n)
                    {
                        const float in = d[n];

                        if (rollReadPos >= (float) newWritePos || rollReadPos < sliceStart)
                            rollReadPos = sliceStart;

                        float rolled = readRingLinear (rollRing, c, rollReadPos);

                        const float distToEnd = (float) newWritePos - rollReadPos;
                        if (distToEnd < fadeLen)
                        {
                            const float t = distToEnd / fadeLen;
                            const float wrapped = readRingLinear (rollRing, c, rollReadPos - sliceLen);
                            rolled = t * rolled + (1.0f - t) * wrapped;
                        }

                        rollReadPos += 1.0f;
                        d[n] = in + depth * (rolled - in);
                    }
                }

                rollWritePos = newWritePos;
                break;
            }

            //---------------- REVERB ----------------
            case 3:
            {
                juce::dsp::Reverb::Parameters rp;
                rp.roomSize   = 0.15f + 0.85f * depth;
                rp.damping    = 0.6f;
                rp.wetLevel   = depth;
                rp.dryLevel   = 0.0f;
                rp.width      = 1.0f;
                rp.freezeMode = 0.0f;
                reverb.setParameters (rp);

                juce::dsp::AudioBlock<float> block (buffer);
                auto sub = block.getSubsetChannelBlock (0, (size_t) procCh);
                juce::dsp::ProcessContextReplacing<float> ctx (sub);
                reverb.process (ctx);
                break;
            }

            //---------------- BRAKE ----------------
            case 4:
            {
                if (! prevFxOn)
                {
                    brakeEngaged      = true;
                    brakeReadPosCh[0] = brakeReadPosCh[1] = (float) brakeWritePos;
                    brakeElapsed      = 0.0;
                    brakeReleaseLeft  = 0;
                }

                const double stopSamples = (double) brakeT * currentSampleRate;
                const double progress = juce::jmin (1.0, brakeElapsed / stopSamples);
                const float rate = juce::jmax (0.0f, (float) (1.0 - progress * (double) depth));

                for (int c = 0; c < procCh; ++c)
                {
                    auto* d = buffer.getWritePointer (c);
                    for (int n = 0; n < numSamples; ++n)
                    {
                        brakeReadPosCh[c] += rate;
                        const float out = readRingLinear (brakeRing, c, brakeReadPosCh[c]);
                        brakeLastOut[c] = out;
                        d[n] = out;
                    }
                }

                brakeElapsed += numSamples;
                break;
            }

            //---------------- CRUSH ----------------
            case 5:
            {
                if (depth > 0.001f)
                {
                    const int   bits  = 16 - (int) std::lround (depth * 13.0f);
                    const int   decim = 1 + (int) std::lround (depth * 11.0f);
                    const float levels = std::pow (2.0f, (float) bits);

                    for (int c = 0; c < procCh; ++c)
                    {
                        auto* d = buffer.getWritePointer (c);
                        for (int n = 0; n < numSamples; ++n)
                        {
                            if (crushCount[c] <= 0)
                            {
                                crushHeld[c]  = d[n];
                                crushCount[c] = decim;
                            }
                            crushCount[c]--;

                            d[n] = std::floor (crushHeld[c] * levels + 0.5f) / levels;
                        }
                    }
                }
                break;
            }

            //---------------- FLANGER ----------------
            case 6:
            {
                const float lfoHz    = 1.0f / juce::jmax (0.01f, beats * 60.0f / bpm);
                const float feedback = depth * 0.65f;
                const float wetMix   = 0.25f + 0.75f * depth;
                const float sr       = (float) currentSampleRate;

                for (int n = 0; n < numSamples; ++n)
                {
                    flangerPhase += lfoHz / sr;
                    if (flangerPhase >= 1.0f) flangerPhase -= 1.0f;
                    const float lfo = 0.5f + 0.5f * std::sin (flangerPhase * juce::MathConstants<float>::twoPi);
                    const float delaySamps = sr * (0.0005f + 0.0075f * lfo);

                    for (int c = 0; c < procCh; ++c)
                    {
                        auto* d = buffer.getWritePointer (c);
                        const float in = d[n];
                        flangerDelay[c].setDelay (delaySamps);
                        const float dl = flangerDelay[c].popSample (c);
                        flangerDelay[c].pushSample (c, in + dl * feedback);
                        d[n] = in * (1.0f - wetMix) + dl * wetMix;
                    }
                }
                break;
            }

            //---------------- PHASER ----------------
            case 7:
            {
                const float lfoHz = 1.0f / juce::jmax (0.01f, beats * 60.0f / bpm);
                phaser.setRate (lfoHz);
                phaser.setDepth (0.3f + 0.7f * depth);
                phaser.setCentreFrequency (1200.0f);
                phaser.setFeedback (0.4f * depth);
                phaser.setMix (0.25f + 0.75f * depth);

                juce::dsp::AudioBlock<float> block (buffer);
                auto sub = block.getSubsetChannelBlock (0, (size_t) procCh);
                juce::dsp::ProcessContextReplacing<float> ctx (sub);
                phaser.process (ctx);
                break;
            }

            //---------------- GATE ----------------
            case 8:
            {
                const float gateHz = 1.0f / juce::jmax (0.01f, beats * 60.0f / bpm);
                const float sr = (float) currentSampleRate;
                const float edgeFc = 30.0f + 370.0f * depth;
                const float coeff = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * edgeFc / sr);

                for (int n = 0; n < numSamples; ++n)
                {
                    gatePhase += gateHz / sr;
                    if (gatePhase >= 1.0f) gatePhase -= 1.0f;
                    const float raw = gatePhase < 0.5f ? 1.0f : 0.0f;

                    for (int c = 0; c < procCh; ++c)
                    {
                        gateState[c] += (raw - gateState[c]) * coeff;
                        buffer.getWritePointer (c)[n] *= gateState[c];
                    }
                }
                break;
            }

            //---------------- REVERSE ----------------
            case 9:
            {
                const int ringSize = reverseRing.getNumSamples();
                const float sliceLen = juce::jlimit (64.0f, (float) ringSize - 64.0f,
                                                     beatsToSamples (beats, bpm));
                const float sliceStart = (float) reverseWritePos - sliceLen;
                const float fadeLen = juce::jmin (256.0f, sliceLen * 0.125f);

                if (! reverseEngaged)
                {
                    reverseEngaged = true;
                    reverseReadPos = (float) reverseWritePos;
                }

                for (int n = 0; n < numSamples; ++n)
                {
                    if (reverseReadPos <= sliceStart || reverseReadPos > (float) reverseWritePos)
                        reverseReadPos = (float) reverseWritePos;

                    const float distToStart = reverseReadPos - sliceStart;

                    for (int c = 0; c < procCh; ++c)
                    {
                        auto* d = buffer.getWritePointer (c);
                        const float in = d[n];

                        float rev = readRingLinear (reverseRing, c, reverseReadPos);
                        if (distToStart < fadeLen)
                        {
                            const float t = juce::jlimit (0.0f, 1.0f, distToStart / fadeLen);
                            const float wrapped = readRingLinear (reverseRing, c, reverseReadPos + sliceLen);
                            rev = t * rev + (1.0f - t) * wrapped;
                        }

                        d[n] = in + depth * (rev - in);
                    }

                    reverseReadPos -= 1.0f;
                }
                break;
            }

            //---------------- BACKSPIN ----------------
            case 10:
            {
                if (! prevFxOn)
                {
                    backspinEngaged = true;
                    backspinReadPos[0] = backspinReadPos[1] = (float) backspinWritePos;
                    backspinElapsed = 0.0;
                }

                for (int n = 0; n < numSamples; ++n)
                {
                    const double k = juce::jmin (1.0, backspinElapsed / 1.0);
                    const float rate = -(1.0f + 2.0f * (float) k);
                    const float env = std::exp (-(float) backspinElapsed / 0.8f);

                    for (int c = 0; c < procCh; ++c)
                    {
                        auto* d = buffer.getWritePointer (c);
                        backspinReadPos[c] += rate;
                        const float out = readRingLinear (backspinRing, c, backspinReadPos[c]) * env;
                        backspinLastOut[c] = out;
                        d[n] = out;
                    }

                    backspinElapsed += 1.0 / currentSampleRate;
                }
                break;
            }

            //---------------- SWEEP ----------------
            case 11:
            {
                // sweepStyle: 0 riser (depth sweeps bandpass 200 Hz -> 8 kHz),
                //             1 noise (broadband burst at fixed 2.5 kHz band)
                if (! prevFxOn)
                    for (int c = 0; c < 2; ++c)
                        sweepFilter[c].reset();

                if (sweepStyle == 1)
                {
                    for (int c = 0; c < 2; ++c)
                    {
                        sweepFilter[c].setCutoffFrequency (2500.0f);
                        sweepFilter[c].setResonance (1.0f);
                    }
                }
                else
                {
                    const float cutoff = 200.0f * std::pow (8000.0f / 200.0f, depth);
                    for (int c = 0; c < 2; ++c)
                        sweepFilter[c].setCutoffFrequency (cutoff);
                }

                const float noiseLevel = sweepStyle == 1 ? 0.55f : 0.4f;
                for (int c = 0; c < procCh; ++c)
                {
                    auto* d = buffer.getWritePointer (c);
                    for (int n = 0; n < numSamples; ++n)
                        d[n] = sweepFilter[c].processSample (c, xorshiftNoise (sweepRand) * noiseLevel);
                }
                break;
            }

            //---------------- PITCH ----------------
            case 12:
            {
                const float sr = (float) currentSampleRate;
                const int   bufSize = pitchBuf.getNumSamples();
                const float maxD = 0.08f * sr;
                const float minD = 8.0f;
                const int   fadeLen = 64;

                const float targetRate = 1.0f - 0.5f * depth;

                for (int n = 0; n < numSamples; ++n)
                {
                    pitchRateSm += (targetRate - pitchRateSm) * 0.002f;

                    for (int c = 0; c < procCh; ++c)
                    {
                        auto* d = buffer.getWritePointer (c);
                        const float in = d[n];

                        pitchBuf.setSample (c, pitchWritePos, in);
                        float out = readRingLinear (pitchBuf, c, pitchReadPos[c]);

                        if (pitchFadeLeft[c] > 0)
                        {
                            const float t = 1.0f - (float) pitchFadeLeft[c] / (float) fadeLen;
                            out = pitchFadeFrom[c] * (1.0f - t) + out * t;
                            --pitchFadeLeft[c];
                        }

                        pitchReadPos[c] += pitchRateSm;

                        float delaySamps = (float) pitchWritePos - pitchReadPos[c];
                        if (delaySamps < 0.0f) delaySamps += (float) bufSize;
                        if (delaySamps > maxD)
                        {
                            pitchFadeFrom[c] = out;
                            pitchFadeLeft[c] = fadeLen;
                            pitchReadPos[c] += (maxD - minD);
                        }

                        d[n] = out;
                    }

                    pitchWritePos = (pitchWritePos + 1) % bufSize;
                }
                break;
            }

            //---------------- WIDE ----------------
            case 13:
            {
                if (procCh >= 2 && depth > 0.001f)
                {
                    const float g = std::pow (10.0f, (9.0f * depth) / 20.0f);
                    auto* dl = buffer.getWritePointer (0);
                    auto* dr = buffer.getWritePointer (1);
                    for (int n = 0; n < numSamples; ++n)
                    {
                        const float mid  = 0.5f * (dl[n] + dr[n]);
                        const float side = 0.5f * (dl[n] - dr[n]);
                        dl[n] = mid + side * g;
                        dr[n] = mid - side * g;
                    }
                }
                break;
            }

            //---------------- LOFI ----------------
            case 14:
            {
                const float sr = (float) currentSampleRate;
                const float baseD = 0.005f * sr;
                const float wowAmt = depth * 0.006f;
                const juce::uint32 crackleThresh = (juce::uint32) (depth * 858993.0f);

                for (int n = 0; n < numSamples; ++n)
                {
                    lofiPhase += 0.5f / sr;
                    if (lofiPhase >= 1.0f) lofiPhase -= 1.0f;
                    const float wob = 1.0f + wowAmt * std::sin (lofiPhase * juce::MathConstants<float>::twoPi);
                    const float delaySamps = baseD * wob;

                    float crackle = 0.0f;
                    lofiRand ^= lofiRand << 13; lofiRand ^= lofiRand >> 17; lofiRand ^= lofiRand << 5;
                    if (lofiRand < crackleThresh)
                    {
                        lofiRand ^= lofiRand << 13; lofiRand ^= lofiRand >> 17; lofiRand ^= lofiRand << 5;
                        crackle = ((float) (lofiRand % 2000u) / 1000.0f - 1.0f) * 0.25f * depth;
                    }

                    for (int c = 0; c < procCh; ++c)
                    {
                        auto* d = buffer.getWritePointer (c);
                        const float in = d[n];
                        wowDelay[c].setDelay (delaySamps);
                        const float wowed = wowDelay[c].popSample (c);
                        wowDelay[c].pushSample (c, in);
                        const float dusty = lofiLp[c].processSample (c, wowed) + crackle;
                        d[n] = in + depth * (dusty - in);
                    }
                }
                break;
            }
        }
    }
    else
    {
        rollEngaged = false;
        reverseEngaged = false;
    }

    //---- Brake disengage: short crossfade back to live input ------------------
    if (brakeEngaged && (! fxOn || sel != 4))
    {
        brakeEngaged = false;
        if (sel == 4)
        {
            brakeReleaseTotal = juce::jmax (1, (int) (0.05 * currentSampleRate));
            brakeReleaseLeft  = brakeReleaseTotal;
        }
        else
        {
            brakeReleaseLeft = 0;
        }
    }

    //---- Backspin disengage: crossfade back to live input ----------------------
    if (backspinEngaged && (! fxOn || sel != 10))
    {
        backspinEngaged = false;
        for (int c = 0; c < 2; ++c)
            brakeLastOut[c] = backspinLastOut[c];
        brakeReleaseTotal = juce::jmax (1, (int) (0.05 * currentSampleRate));
        brakeReleaseLeft  = brakeReleaseTotal;
    }

    if (brakeReleaseLeft > 0)
    {
        for (int c = 0; c < procCh; ++c)
        {
            auto* d = buffer.getWritePointer (c);
            for (int n = 0; n < numSamples; ++n)
            {
                const float t = (float) brakeReleaseLeft / (float) brakeReleaseTotal;
                d[n] = brakeLastOut[c] * t + d[n] * (1.0f - t);
                if (c == 0)
                    --brakeReleaseLeft;
            }
        }
        brakeReleaseLeft = juce::jmax (0, brakeReleaseLeft);
    }

    // Mono in -> stereo out: duplicate the processed channel.
    if (procCh == 1 && numOut >= 2)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

    //==== Output: click-free on/off + engine-change fade, dry/wet, out gain ======
    // wetK = 1 -> normal effect mix; wetK = 0 -> pure dry. Both wetSmooth
    // (on/off) and the engine-change fade drive wetK, so neither can click.
    const int outCh = juce::jmin (numOut, 2);
    const int dryCh = juce::jmin (dryCopy.getNumChannels(), 2);
    const float wetGain = dryWet;
    const float dryGain = 1.0f - dryWet;
    const float invFadeLen = engineFadeLen > 0 ? 1.0f / (float) engineFadeLen : 1.0f;

    for (int c = 0; c < outCh; ++c)
    {
        auto*        w = buffer.getWritePointer (c);
        const float* dr = dryCopy.getReadPointer (juce::jmin (c, dryCh - 1));
        int fadePos = engineFadePos; // local copy: identical envelope on every channel
        for (int n = 0; n < numSamples; ++n)
        {
            float ef = 1.0f;
            if (fadePos < engineFadeLen)
            {
                ef = (float) fadePos * invFadeLen;
                ++fadePos;
            }
            const float wetK = wetSmooth * ef;
            const float mixed = (dr[n] * (dryGain + wetGain * (1.0f - wetK))
                                 + w[n] * wetGain * wetK) * outGain;
            w[n] = std::tanh (mixed);
        }
        if (c == 0)
            engineFadePos = juce::jmin (engineFadeLen, engineFadePos + numSamples);
    }

    //---- Output meters (post everything) -----------------------------------------
    {
        float pkL = 0.0f, pkR = 0.0f;
        if (outCh >= 1) pkL = buffer.getMagnitude (0, 0, numSamples);
        if (outCh >= 2) pkR = buffer.getMagnitude (1, 0, numSamples);
        else            pkR = pkL;
        outLevelL.store (juce::jmax (pkL, outLevelL.load() * 0.92f));
        outLevelR.store (juce::jmax (pkR, outLevelR.load() * 0.92f));
    }

    prevFxOn = fxOn;
}

//==============================================================================
void ShiftFxProcessor::writeExtraState (juce::ValueTree& state)
{
    juce::ValueTree extra ("ShiftFxExtra");

    juce::String favStr;
    for (int i = 0; i < favorites.size(); ++i)
        favStr += (i > 0 ? "," : "") + juce::String (favorites[i]);
    extra.setProperty ("favorites", favStr, nullptr);

    juce::String ccStr;
    bool first = true;
    for (int cc = 0; cc < 128; ++cc)
    {
        const int p = ccToParam[cc].load();
        if (p >= 0)
        {
            ccStr += (first ? "" : ";") + juce::String (cc) + "=" + juce::String (p);
            first = false;
        }
    }
    extra.setProperty ("ccMap", ccStr, nullptr);

    state.appendChild (extra, nullptr);
}

void ShiftFxProcessor::readExtraState (const juce::ValueTree& state)
{
    favorites.clear();
    for (int i = 0; i < 128; ++i)
        ccToParam[i].store (-1);

    juce::ValueTree extra = state.getChildWithName ("ShiftFxExtra");
    if (! extra.isValid())
        return;

    const juce::String favStr = extra.getProperty ("favorites", "").toString();
    for (auto tok : juce::StringArray::fromTokens (favStr, ",", ""))
    {
        const int v = tok.getIntValue();
        if (v >= 0 && v < numPresets)
            favorites.addIfNotAlreadyThere (v);
    }

    const juce::String ccStr = extra.getProperty ("ccMap", "").toString();
    for (auto tok : juce::StringArray::fromTokens (ccStr, ";", ""))
    {
        auto kv = juce::StringArray::fromTokens (tok, "=", "");
        if (kv.size() == 2)
        {
            const int cc = kv[0].getIntValue();
            const int p  = kv[1].getIntValue();
            if (cc >= 0 && cc < 128 && p >= 0 && p < NumLearnable)
                ccToParam[cc].store (p);
        }
    }
}

void ShiftFxProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (state.isValid())
    {
        writeExtraState (state);
        std::unique_ptr<juce::XmlElement> xml (state.createXml());
        copyXmlToBinary (*xml, destData);
    }
}

void ShiftFxProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml && xml->hasTagName (apvts.state.getType()))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        readExtraState (state);
        apvts.replaceState (state);
    }
}

//==============================================================================
juce::AudioProcessorEditor* ShiftFxProcessor::createEditor()
{
#ifdef SHIFTFX_HEADLESS
    return nullptr;
#else
    return new ShiftFxEditor (*this);
#endif
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ShiftFxProcessor();
}
