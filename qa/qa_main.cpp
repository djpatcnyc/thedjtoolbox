//==============================================================================
// SHIFT/FX PRO — offline QA harness (headless).
//
// Renders test audio through every one of the 23 presets at multiple sample
// rates and block sizes, checking for crashes, NaN/inf, blowups, silence,
// and click discontinuities at preset-switch and on/off boundaries.
// Also covers rapid preset switching, sample-rate re-init, no-host-tempo,
// bypass pass-through, MIDI-learn binding, and state save/restore.
//
// Build: part of the CMake project (target ShiftFxQA). Run: ./ShiftFxQA
// Exit code 0 = all green, 1 = any failure.
//==============================================================================

#include "../Source/PluginProcessor.h"

#include <cstdio>
#include <cmath>
#include <vector>

namespace
{
int gFailures = 0;
int gPasses = 0;

void check (bool ok, const juce::String& name)
{
    if (ok) { ++gPasses; std::printf ("  [PASS] %s\n", name.toRawUTF8()); }
    else    { ++gFailures; std::printf ("  [FAIL] %s\n", name.toRawUTF8()); }
}

bool hasNanInf (const juce::AudioBuffer<float>& b)
{
    for (int c = 0; c < b.getNumChannels(); ++c)
    {
        const float* d = b.getReadPointer (c);
        for (int n = 0; n < b.getNumSamples(); ++n)
            if (! std::isfinite (d[n]))
                return true;
    }
    return false;
}

float peakOf (const juce::AudioBuffer<float>& b)
{
    float pk = 0.0f;
    for (int c = 0; c < b.getNumChannels(); ++c)
        pk = juce::jmax (pk, b.getMagnitude (c, 0, b.getNumSamples()));
    return pk;
}

// Max sample-to-sample jump; a hard click across a switch is ~O(1).
float maxJump (const juce::AudioBuffer<float>& b)
{
    float mj = 0.0f;
    for (int c = 0; c < b.getNumChannels(); ++c)
    {
        const float* d = b.getReadPointer (c);
        for (int n = 1; n < b.getNumSamples(); ++n)
            mj = juce::jmax (mj, std::abs (d[n] - d[n - 1]));
    }
    return mj;
}

juce::uint32 rngState = 0xC0FFEEu;
float frand()
{
    rngState ^= rngState << 13; rngState ^= rngState >> 17; rngState ^= rngState << 5;
    return ((float) (rngState >> 8) / (float) 0xFFFFFFu) * 2.0f - 1.0f;
}

// 3 s test signal: sine sweep 100 Hz -> 8 kHz + transient bursts every 0.5 s
// + low noise bed. Peak-normalized to 0.5.
juce::AudioBuffer<float> makeTestSignal (double sr, double seconds)
{
    const int n = (int) (sr * seconds);
    juce::AudioBuffer<float> b (2, n);
    float peak = 0.0f;
    for (int c = 0; c < 2; ++c)
    {
        auto* d = b.getWritePointer (c);
        float phase = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const double t = (double) i / sr;
            const float freq = 100.0f * std::pow (80.0f, (float) (t / seconds));
            phase += juce::MathConstants<float>::twoPi * freq / (float) sr;
            float s = 0.45f * std::sin (phase);

            // transient burst every 0.5 s (10 ms decaying noise tick)
            const double cyc = std::fmod (t, 0.5);
            if (cyc < 0.01)
                s += 0.5f * frand() * std::exp ((float) (-cyc * 400.0));

            s += 0.03f * frand(); // noise bed
            d[i] = s;
            peak = juce::jmax (peak, std::abs (s));
        }
    }
    const float g = 0.5f / juce::jmax (peak, 1e-6f);
    b.applyGain (g);
    return b;
}

void setParam01 (ShiftFxProcessor& p, const char* pid, float v)
{
    if (auto* par = p.apvts.getParameter (pid))
        par->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v));
}

void setPreset (ShiftFxProcessor& p, int idx)
{
    if (auto* par = p.apvts.getParameter (ShiftFxProcessor::pidPreset))
        par->setValueNotifyingHost (par->convertTo0to1 ((float) idx));
}

// Render `in` through the processor in blocks; returns the output.
juce::AudioBuffer<float> renderThrough (ShiftFxProcessor& proc,
                                        const juce::AudioBuffer<float>& in,
                                        int blockSize)
{
    const int total = in.getNumSamples();
    juce::AudioBuffer<float> out (2, total);
    juce::MidiBuffer midi;
    int pos = 0;
    while (pos < total)
    {
        const int n = juce::jmin (blockSize, total - pos);
        juce::AudioBuffer<float> seg (2, n);
        for (int c = 0; c < 2; ++c)
            seg.copyFrom (c, 0, in, c, pos, n);
        proc.processBlock (seg, midi);
        for (int c = 0; c < 2; ++c)
            out.copyFrom (c, pos, seg, c, 0, n);
        pos += n;
    }
    return out;
}

struct PresetResult { bool nanInf = false; float peak = 0.0f; };

PresetResult runPresetOnce (int presetIdx, double sr, int blockSize, double seconds)
{
    ShiftFxProcessor proc;
    proc.prepareToPlay (sr, blockSize);
    setPreset (proc, presetIdx);
    setParam01 (proc, ShiftFxProcessor::pidOn, 1.0f);
    setParam01 (proc, ShiftFxProcessor::pidBpmSrc, 1.0f); // Manual (no host in harness)
    auto in = makeTestSignal (sr, seconds);
    auto out = renderThrough (proc, in, blockSize);
    return { hasNanInf (out), peakOf (out) };
}

} // namespace

//==============================================================================
int main()
{
    // NB: no JUCE GUI initialisation needed — this harness uses DSP only.
    std::printf ("SHIFT/FX PRO offline QA — 23 presets x {44.1k, 48k} x {64, 512, 2048}\n\n");

    //---- 1. Every preset, every rate, every block size ----------------------------
    std::printf ("== Preset render matrix ==\n");
    const double rates[2] = { 44100.0, 48000.0 };
    const int blocks[3] = { 64, 512, 2048 };
    for (int pi = 0; pi < ShiftFxProcessor::numPresets; ++pi)
    {
        bool ok = true;
        int runs = 0;
        for (double sr : rates)
            for (int bs : blocks)
            {
                const auto r = runPresetOnce (pi, sr, bs, 3.0);
                ++runs;
                // tanh caps output at 1.0; allow tiny overshoot margin
                if (r.nanInf || r.peak > 1.01f || r.peak < 1e-5f)
                    ok = false;
            }
        check (ok, juce::String::formatted ("preset %02d %-15s (%d runs: no NaN/inf, peak<=1, non-silent)",
                                            pi, ShiftFxProcessor::presets[pi].name, runs));
    }

    //---- 2. Click-free preset switching ---------------------------------------------
    std::printf ("\n== Switch / toggle click tests ==\n");
    {
        ShiftFxProcessor proc;
        proc.prepareToPlay (48000.0, 512);
        setParam01 (proc, ShiftFxProcessor::pidBpmSrc, 1.0f);
        setPreset (proc, 0);
        setParam01 (proc, ShiftFxProcessor::pidOn, 1.0f);
        auto in = makeTestSignal (48000.0, 4.0);

        // 1 s on preset 0, switch to preset 12 mid-stream, 1 s more
        juce::AudioBuffer<float> a (2, 48000), b (2, 48000);
        for (int c = 0; c < 2; ++c) { a.copyFrom (c, 0, in, c, 0, 48000); b.copyFrom (c, 0, in, c, 48000, 48000); }
        auto outA = renderThrough (proc, a, 512);
        setPreset (proc, 12);
        auto outB = renderThrough (proc, b, 512);
        juce::AudioBuffer<float> joined (2, 96000);
        for (int c = 0; c < 2; ++c) { joined.copyFrom (c, 0, outA, c, 0, 48000); joined.copyFrom (c, 48000, outB, c, 0, 48000); }
        // measure the jump only in a window around the switch point
        juce::AudioBuffer<float> window (2, 2000);
        for (int c = 0; c < 2; ++c) window.copyFrom (c, 0, joined, c, 47000, 2000);
        const float j = maxJump (window);
        check (j < 0.6f, juce::String::formatted ("preset switch 0->12 mid-stream: max jump %.3f < 0.6", j));
    }
    {
        ShiftFxProcessor proc;
        proc.prepareToPlay (48000.0, 256);
        setParam01 (proc, ShiftFxProcessor::pidBpmSrc, 1.0f);
        setPreset (proc, 5); // ROLL
        setParam01 (proc, ShiftFxProcessor::pidOn, 1.0f);
        auto in = makeTestSignal (48000.0, 3.0);
        juce::MidiBuffer midi;
        // render 1 s on, toggle off mid-stream, render 1 s; then on again, 1 s
        juce::AudioBuffer<float> out (2, 144000);
        int pos = 0;
        auto renderSeg = [&](int n, bool toggle, float v)
        {
            if (toggle) setParam01 (proc, ShiftFxProcessor::pidOn, v);
            int done = 0;
            while (done < n)
            {
                const int bs = juce::jmin (256, n - done);
                juce::AudioBuffer<float> seg (2, bs);
                for (int c = 0; c < 2; ++c) seg.copyFrom (c, 0, in, c, pos + done, bs);
                proc.processBlock (seg, midi);
                for (int c = 0; c < 2; ++c) out.copyFrom (c, pos + done, seg, c, 0, bs);
                done += bs;
            }
            pos += n;
        };
        renderSeg (48000, false, 0.0f);
        renderSeg (48000, true, 0.0f);   // off
        renderSeg (48000, true, 1.0f);   // on again
        juce::AudioBuffer<float> w1 (2, 2000), w2 (2, 2000);
        for (int c = 0; c < 2; ++c)
        {
            w1.copyFrom (c, 0, out, c, 47000, 2000);
            w2.copyFrom (c, 0, out, c, 95000, 2000);
        }
        check (maxJump (w1) < 0.6f, juce::String::formatted ("on->off toggle: max jump %.3f < 0.6", maxJump (w1)));
        check (maxJump (w2) < 0.6f, juce::String::formatted ("off->on toggle: max jump %.3f < 0.6", maxJump (w2)));
        check (! hasNanInf (out), "toggle sequence: no NaN/inf");
    }

    //---- 3. Rapid switching through all 23 presets ------------------------------------
    std::printf ("\n== Rapid switching ==\n");
    {
        ShiftFxProcessor proc;
        proc.prepareToPlay (44100.0, 128);
        setParam01 (proc, ShiftFxProcessor::pidBpmSrc, 1.0f);
        setParam01 (proc, ShiftFxProcessor::pidOn, 1.0f);
        auto in = makeTestSignal (44100.0, 0.15 * 23 + 0.5);
        juce::AudioBuffer<float> out (2, in.getNumSamples());
        juce::MidiBuffer midi;
        int pos = 0;
        for (int pi = 0; pi < 23; ++pi)
        {
            setPreset (proc, pi);
            const int n = (int) (44100.0 * 0.15);
            int done = 0;
            while (done < n && pos + done < in.getNumSamples())
            {
                const int bs = juce::jmin (128, n - done);
                juce::AudioBuffer<float> seg (2, bs);
                for (int c = 0; c < 2; ++c) seg.copyFrom (c, 0, in, c, pos + done, bs);
                proc.processBlock (seg, midi);
                for (int c = 0; c < 2; ++c) out.copyFrom (c, pos + done, seg, c, 0, bs);
                done += bs;
            }
            pos += n;
        }
        check (! hasNanInf (out), "23 presets x 0.15 s: no NaN/inf");
        check (peakOf (out) <= 1.01f, juce::String::formatted ("23 presets x 0.15 s: peak %.3f <= 1.01", peakOf (out)));
    }

    //---- 4. Sample-rate / block-size re-init mid-session --------------------------------
    std::printf ("\n== Re-init ==\n");
    {
        ShiftFxProcessor proc;
        proc.prepareToPlay (44100.0, 512);
        setPreset (proc, 3);
        setParam01 (proc, ShiftFxProcessor::pidOn, 1.0f);
        auto in1 = makeTestSignal (44100.0, 1.0);
        auto o1 = renderThrough (proc, in1, 512);
        proc.prepareToPlay (48000.0, 2048); // re-init: new rate + block size
        auto in2 = makeTestSignal (48000.0, 1.0);
        auto o2 = renderThrough (proc, in2, 2048);
        check (! hasNanInf (o1) && ! hasNanInf (o2), "re-init 44.1k/512 -> 48k/2048: no NaN/inf");
        check (peakOf (o2) > 1e-5f && peakOf (o2) <= 1.01f, "re-init: output sane after rate change");
    }

    //---- 5. Bypass pass-through ----------------------------------------------------------
    std::printf ("\n== Bypass ==\n");
    {
        ShiftFxProcessor proc;
        proc.prepareToPlay (48000.0, 512);
        auto in = makeTestSignal (48000.0, 1.0);
        juce::AudioBuffer<float> out (2, in.getNumSamples());
        juce::MidiBuffer midi;
        int pos = 0;
        while (pos < in.getNumSamples())
        {
            const int bs = juce::jmin (512, in.getNumSamples() - pos);
            juce::AudioBuffer<float> seg (2, bs);
            for (int c = 0; c < 2; ++c) seg.copyFrom (c, 0, in, c, pos, bs);
            proc.processBlockBypassed (seg, midi);
            for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, seg, c, 0, bs);
            pos += bs;
        }
        float maxDiff = 0.0f;
        for (int c = 0; c < 2; ++c)
        {
            const float* a = in.getReadPointer (c);
            const float* b = out.getReadPointer (c);
            for (int n = 0; n < in.getNumSamples(); ++n)
                maxDiff = juce::jmax (maxDiff, std::abs (a[n] - b[n]));
        }
        check (maxDiff < 1e-6f, juce::String::formatted ("bypass: dry pass-through, max diff %.2e", maxDiff));
        check (proc.inLevelL.load() > 0.01f, "bypass: meters keep running");
    }

    //---- 6. MIDI learn binding ----------------------------------------------------------------
    std::printf ("\n== MIDI learn ==\n");
    {
        ShiftFxProcessor proc;
        proc.prepareToPlay (48000.0, 512);
        proc.learnArmed.store (ShiftFxProcessor::LearnDryWet);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::controllerEvent (1, 74, 100), 0);
        juce::AudioBuffer<float> seg (2, 64);
        proc.processBlock (seg, midi);
        check (proc.ccToParam[74].load() == ShiftFxProcessor::LearnDryWet, "learn: CC 74 bound to Dry/Wet");
        check (proc.learnEventFlag.load(), "learn: completion flag raised");
        // now the CC drives the parameter
        juce::MidiBuffer midi2;
        midi2.addEvent (juce::MidiMessage::controllerEvent (1, 74, 64), 0);
        juce::AudioBuffer<float> seg2 (2, 64);
        proc.processBlock (seg2, midi2);
        auto* par = proc.apvts.getParameter (ShiftFxProcessor::pidDryWet);
        check (std::abs (par->getValue() - 64.0f / 127.0f) < 0.01f, "learn: CC 74 drives Dry/Wet");
    }

    //---- 7. State save/restore round-trip ---------------------------------------------------------
    std::printf ("\n== State round-trip ==\n");
    {
        ShiftFxProcessor a, b;
        a.prepareToPlay (48000.0, 512);
        b.prepareToPlay (48000.0, 512);
        setPreset (a, 7);
        setParam01 (a, ShiftFxProcessor::pidDryWet, 0.3f);
        setParam01 (a, ShiftFxProcessor::pidOutMix, 0.5f);
        setParam01 (a, ShiftFxProcessor::pidOn, 1.0f);
        a.setFavorite (3, true);
        a.ccToParam[12].store (ShiftFxProcessor::LearnOn);
        juce::MemoryBlock mb;
        a.getStateInformation (mb);
        b.setStateInformation (mb.getData(), (int) mb.getSize());
        bool same = true;
        for (auto* pid : { ShiftFxProcessor::pidPreset, ShiftFxProcessor::pidDryWet,
                           ShiftFxProcessor::pidOutMix, ShiftFxProcessor::pidOn,
                           ShiftFxProcessor::pidBpmSrc, ShiftFxProcessor::pidBpm })
        {
            const float va = a.apvts.getParameter (pid)->getValue();
            const float vb = b.apvts.getParameter (pid)->getValue();
            if (std::abs (va - vb) > 1e-6f) same = false;
        }
        check (same, "state: all parameter values identical after restore");
        check (b.isFavorite (3), "state: favorite flag restored");
        check (b.ccToParam[12].load() == ShiftFxProcessor::LearnOn, "state: MIDI CC binding restored");
    }

    //---- 8. Tail length ------------------------------------------------------------------------------
    std::printf ("\n== Tail ==\n");
    check (ShiftFxProcessor ().getTailLengthSeconds() >= 4.0, "getTailLengthSeconds >= 4 s (echo/reverb tails)");

    std::printf ("\n==============================\n");
    std::printf ("RESULT: %d passed, %d failed\n", gPasses, gFailures);
    std::printf ("==============================\n");
    return gFailures == 0 ? 0 : 1;
}
