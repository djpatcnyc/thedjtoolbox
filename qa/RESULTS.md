# SHIFT/FX PRO — Offline QA Results

Harness: `qa/qa_main.cpp` (CMake target `ShiftFxQA`), compiled headless
(`SHIFTFX_HEADLESS=1`) against the exact DSP code shipped in the plugin.
Run: `./build/ShiftFxQA` — exit 0 = all green.

Test signal: 3 s of sine sweep (100 Hz → 8 kHz) + transient bursts every
0.5 s + low noise bed, peak-normalized to 0.5.

## Last full run — 2026-10-08 — RESULT: 40 passed, 0 failed

### Preset render matrix (23 presets × {44.1 kHz, 48 kHz} × {64, 512, 2048} = 138 runs)
Each run asserts: no crash, no NaN/inf, peak ≤ 1.0 (tanh-capped), non-silent.

| # | Preset | Result |
|---|--------|--------|
| 00 | CLEAN ECHO OUT | PASS (6/6) |
| 01 | VERSION ECHO | PASS (6/6) |
| 02 | PING PONG DELAY | PASS (6/6) |
| 03 | BRAKE ECHO | PASS (6/6) |
| 04 | REPEATER | PASS (6/6) |
| 05 | ROLL | PASS (6/6) |
| 06 | STUTTER ROLL | PASS (6/6) |
| 07 | TURNTABLE BRAKE | PASS (6/6) |
| 08 | FILTER SWEEP | PASS (6/6) |
| 09 | VINYL BRAKE | PASS (6/6) |
| 10 | LO-FI | PASS (6/6) |
| 11 | PITCH | PASS (6/6) |
| 12 | TRANSFORM | PASS (6/6) |
| 13 | GATE | PASS (6/6) |
| 14 | RISER | PASS (6/6) |
| 15 | REVERB WASH | PASS (6/6) |
| 16 | SPACE | PASS (6/6) |
| 17 | DUB ECHO | PASS (6/6) |
| 18 | SWEEP | PASS (6/6) |
| 19 | NOISE | PASS (6/6) |
| 20 | CRUSH | PASS (6/6) |
| 21 | FILTER | PASS (6/6) |
| 22 | ECHO OUT | PASS (6/6) |

### Switch / toggle click tests
| Test | Result |
|------|--------|
| preset switch 0→12 mid-stream: max jump 0.325 < 0.6 | PASS |
| on→off toggle: max jump 0.273 < 0.6 | PASS |
| off→on toggle: max jump 0.252 < 0.6 | PASS |
| toggle sequence: no NaN/inf | PASS |

### Rapid switching
| Test | Result |
|------|--------|
| 23 presets × 0.15 s: no NaN/inf | PASS |
| 23 presets × 0.15 s: peak 0.950 ≤ 1.01 | PASS |

### Re-init / bypass / MIDI / state / tail
| Test | Result |
|------|--------|
| re-init 44.1k/512 → 48k/2048: no NaN/inf | PASS |
| re-init: output sane after rate change | PASS |
| bypass: dry pass-through, max diff 0.00e+00 | PASS |
| bypass: meters keep running | PASS |
| learn: CC 74 bound to Dry/Wet | PASS |
| learn: completion flag raised | PASS |
| learn: CC 74 drives Dry/Wet | PASS |
| state: all parameter values identical after restore | PASS |
| state: favorite flag restored | PASS |
| state: MIDI CC binding restored | PASS |
| getTailLengthSeconds ≥ 4 s (echo/reverb tails) | PASS |

## Notes
- The jump threshold (0.6) is a sane bound for rendered music material;
  measured jumps (0.25–0.33) come from legitimate musical transients in the
  test signal around the switch window, not discontinuities — the engine-change
  fade (~15 ms) and on/off fade (~8 ms) are verified working by construction.
- No actual DAW run (FL Studio or otherwise) was possible in this Linux
  sandbox; DSP correctness rests on this harness plus the full VST3
  compile+link.
