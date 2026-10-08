# SHIFT/FX PRO — Universal DJ Transition Engine, by DJ Toolbox

A DJ-style **transition FX box** as a pure insert effect plugin. Drop it on any
DAW channel, turn the giant 360° knob to pick one of **23 presets**, hit the
**FX ON/OFF** power button, and blend with **DRY/WET**. One preset is active at
a time — mixer beat-FX style.

- **Formats:** VST3 (Windows/macOS/Linux) + Audio Unit (macOS, for Logic Pro).
  AAX is **planned** (requires Avid developer approval) — not built yet.
- **Framework:** JUCE 8.0.7, C++17, CMake.
- Every parameter is exposed for **DAW automation**: preset, on/off, dry/wet,
  output mix, BPM source, manual BPM, tap tempo.
- **MIDI learn** is built in: click LEARN, click a control (preset knob,
  on/off, dry/wet, output mix), then send any MIDI CC to bind it. Bindings are
  saved with the plugin state.

## Architecture: 23 presets over 15 DSP engines

The library lists 23 presets, but they are *recipes* over 15 real DSP engines —
the same way the big commercial plugins do it. Each preset fixes an engine
plus its settings (intensity, beat division, character). Only the selected
preset's engine processes audio. Signal chain:

```
selected engine -> dry/wet -> output mix (-inf..+6 dB) -> tanh soft clipper
```

Tempo follows the **DAW host transport** when BPM Source is "Host (DAW)" and
the host provides a valid tempo; otherwise it uses the manual BPM (tap it in
with TAP or double-click the readout — 60–200 BPM, default 128).

| # | Preset | Engine | What it does |
|---|--------|--------|--------------|
| 00 | CLEAN ECHO OUT | echo | Smooth tempo-synced echo with a clean tail. |
| 01 | VERSION ECHO | echo | Dub-style echo with a dark, rolling tail. |
| 02 | PING PONG DELAY | echo | Stereo echo bouncing left to right. |
| 03 | BRAKE ECHO | echo | Echo whose tail slows to a stop. |
| 04 | REPEATER | roll | Tight 1/8 beat repeat for builds. |
| 05 | ROLL | roll | Classic 1/4 loop roll. |
| 06 | STUTTER ROLL | roll | Frantic 1/16 stutter roll. |
| 07 | TURNTABLE BRAKE | brake | Full vinyl stop. Instant drama. |
| 08 | FILTER SWEEP | filter | Resonant filter sweep for transitions. |
| 09 | VINYL BRAKE | brake | Quick pitch-drop brake, vinyl style. |
| 10 | LO-FI | lofi | Worn vinyl: wow, dust and crackle. |
| 11 | PITCH | pitch | Tape-style pitch drop, down an octave. |
| 12 | TRANSFORM | gate | Transformer chop gate. Rhythmic cuts. |
| 13 | GATE | gate | Beat-synced trance gate. |
| 14 | RISER | sweep | Noise riser that lifts into the drop. |
| 15 | REVERB WASH | reverb | Huge reverb wash for breakdowns. |
| 16 | SPACE | reverb | Cavernous space reverb. |
| 17 | DUB ECHO | echo | Dark, heavy-feedback dub echo. |
| 18 | SWEEP | sweep | Filtered noise sweep. |
| 19 | NOISE | sweep | Raw noise burst for impact. |
| 20 | CRUSH | crush | Bit-crushed digital grit. |
| 21 | FILTER | filter | DJ-style resonant filter. |
| 22 | ECHO OUT | echo | The classic echo-out transition. |

The 15 engines: echo (4 characters: clean/dub/ping-pong/brake), resonant
LP/HP morph filter, beat-repeat roll, algorithmic reverb, tape-stop brake,
bit crusher, flanger, phaser, beat gate, reverse roll, backspin, noise
sweep/riser, tape pitch drop, M/S widener, vinyl lofi.

### Click-free by design

- ON/OFF fades the wet path in ~8 ms — never a hard switch.
- Switching presets crossfades the new engine in over ~15 ms and drains the
  old engine's delay/reverb buffers instead of cutting them.
- Zero allocations in the audio path; `ScopedNoDenormals` throughout.
- `processBlockBypassed` is a true dry pass-through (meters keep running).
- Reported tail length is 4 s so hosts don't cut echo/reverb tails.

## Building

You need CMake 3.22+, a C++17 compiler, and JUCE 8.0.7 checked out as `JUCE/`
next to this CMakeLists (the GitHub workflow clones it automatically):

```sh
git clone --depth 1 --branch 8.0.7 https://github.com/juce-framework/JUCE.git JUCE
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

- **Linux** also needs the JUCE GUI dependencies, e.g. on Ubuntu/Debian:
  ```sh
  sudo apt install libx11-dev libfreetype6-dev libasound2-dev libgl1-mesa-dev \
      libxrandr-dev libxinerama-dev libxcursor-dev libcurl4-openssl-dev
  ```
- **macOS:** Xcode command-line tools are enough (`xcode-select --install`).
  Builds both VST3 and AU.
- **Windows:** Visual Studio 2022 (any edition) + CMake. Builds VST3.

The plugin bundles land in `build/ShiftFxPro_artefacts/Release/`.

### Automated builds (GitHub Actions)

`.github/workflows/build.yml` builds on every push: **macOS** produces the
VST3 + AU bundles, **Windows** produces the VST3 — all uploaded as workflow
artifacts. No Apple Developer account needed to *build*; see below for the
Gatekeeper note on *running* unsigned builds.

## Installing

- **macOS VST3:** copy `SHIFT FX PRO.vst3` to `~/Library/Audio/Plug-Ins/VST3/`
- **macOS AU (Logic Pro):** copy `SHIFT FX PRO.component` to
  `~/Library/Audio/Plug-Ins/Components/`, then restart Logic and rescan in
  Preferences → Plug-in Manager if needed.
- **Windows VST3:** copy `SHIFT FX PRO.vst3` to `C:\Program Files\Common Files\VST3\`
- **Linux VST3:** copy `SHIFT FX PRO.vst3` to `~/.vst3/`

Then load it as an insert effect on any audio channel (Ableton Live, FL
Studio, Cubase, Reaper, Studio One, Bitwig, Logic Pro via AU).

> **macOS Gatekeeper note:** these CI builds are unsigned. On first load macOS
> may refuse to open the plugin. Fix: right-click the `.vst3`/`.component`
> bundle → **Open** to approve it once, or run
> `xattr -cr "SHIFT FX PRO.vst3"` in Terminal. Proper code signing and
> notarization require a paid Apple Developer account and are a later step.

## How to use it

1. Insert SHIFT/FX PRO on the channel you want to mangle (a common trick: put
   it on the master or on the outgoing track's channel right before a
   transition).
2. Turn the giant **preset knob** (drag vertically, or scroll over it — it
   snaps through all 23 presets), or step with the **< >** arrows.
3. Hit the orange **FX ON/OFF** power button.
4. **DRY/WET** blends the effect; **OUTPUT MIX** trims the level (-inf..+6 dB).
5. Star your favorites; SAVE commits state; the gear button shows the about box.
6. **MIDI LEARN**: click LEARN, click a control, send a MIDI CC — bound, and
   the binding is stored with your session.
7. Automate anything — every control is a DAW-automatable parameter.

## Offline QA

`qa/` contains a headless test harness (`ShiftFxQA`, built by CMake) that
reuses the exact DSP code from the plugin. It renders 3 s of test audio
(sine sweep + transients + noise) through all 23 presets at 44.1/48 kHz and
block sizes 64/512/2048, then checks: no crash, no NaN/inf, peak ≤ 1.0,
non-silent output, click-free preset switching and on/off toggling, rapid
switching through all presets, sample-rate re-init mid-session, bypass
pass-through, MIDI-learn binding, and state save/restore round-trip.
See `qa/RESULTS.md` for the latest full table. `qa/shot_main.cpp` (CMake target
`ShiftFxShot`) renders the real editor UI to `~/workspace/shiftfx-screenshot.png`
offscreen — no display needed — for visual review.

## Limitations / honest notes

- Tempo is host-follow with tap/manual fallback; there is no beat *detection*
  from the audio itself.
- No timestretching — Roll, Reverse and Brake are sample-rate tricks.
- macOS builds from CI are unsigned (see Gatekeeper note above).
- AAX is planned, pending Avid developer approval.
- All branding is original ("SHIFT/FX PRO" by "DJ Toolbox"). It is styled
  like pro DJ gear but contains no third-party trademarks, names, or logos.
