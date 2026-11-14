# PitchPilot

Pitch correction + vocal FX plugin (VST3) for FL Studio and any other DAW. Built with JUCE.

## What it does

**Pitch correction**
- Real-time pitch detection (YIN) and pitch-synchronous shifting (PSOLA), with formants preserved
- Input type (Soprano / Alto-Tenor / Low Male / Instrument), Key, Scale (9 scales), Detune (reference pitch), Transpose
- Retune Speed, Flex-Tune, Humanize, Natural Vibrato, Throat (formant), Tracking, Classic mode, Formant on/off
- Create Vibrato: shape, rate, onset delay, onset rate, variation, pitch / amplitude / formant amount
- Per-note Target / Remove / Bypass, per-note cents detune (drag the numbers, double-click to reset)
- Set Major, Set Minor, Set All, Bypass All, Remove All
- Hold, Ignore Vibrato (targets the note you meant, not the wobble)
- MIDI targeting: **Target Notes** (held MIDI or on-screen keys become the only targets), **Learn Scale** (play notes, switch it off, and those notes become the scale), keyboard Latch / Momentary
- Live pitch display (grey = input, mint = corrected), current note and cents meter

**Vocal FX**
- **Tone**: Low Cut, Body, Presence, Air, Compress (auto-threshold compressor)
- **Distortion / Crush**: Drive, Bit Depth, Downsample, Crush Mix
- **Space**: reverb amount slider + size
- **Auto Polish**: one switch for EQ shaping, auto compression and a soft limiter (a Soundgoodizer-style finisher)
- **Style chains**: FAKEMINK STYLE and ESDEEKID STYLE buttons set every parameter above in one click
- **FX order**: AUTO ORDER runs the effects in the best-sounding order (Tone > Compressor > Distortion > Space > Polish). Pick a different stage in any slot to take over manually; picking a stage swaps it with the slot it was in. Press AUTO ORDER to go back.

The style chains are my own approximations of the *kind* of processing associated with those sounds. They are starting points, not exact copies, and no plugin can change the character of your voice itself. Tweak by ear.

## Build (Windows)

**Easiest:** open "x64 Native Tools Command Prompt for VS 2022" **as Administrator**, `cd` into this folder and run `build_windows.bat`. It builds and installs the plugin into the VST3 folder in one go.

**No Visual Studio?** Push this folder to a GitHub repo; the included workflow (`.github/workflows/build.yml`) builds the Windows and Mac VST3 in the cloud, and you download it from the Actions tab (untested workflow - check the log if it fails).

Manual steps:

1. Install **Visual Studio 2022 Community** (tick "Desktop development with C++") and **CMake** (3.22+).
2. Open "x64 Native Tools Command Prompt for VS 2022" and run:

```
cd PitchPilot
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

The first build downloads JUCE (needs internet). The plugin ends up in
`build\PitchPilot_artefacts\Release\VST3\PitchPilot.vst3`.

3. Copy the whole `PitchPilot.vst3` folder to `C:\Program Files\Common Files\VST3\` (needs admin).

## Build (Mac)

```
cd PitchPilot
cmake -B build -G Xcode
cmake --build build --config Release
```

Copy `PitchPilot.vst3` to `~/Library/Audio/Plug-Ins/VST3/`.

A `Standalone` app is built next to it, handy for quick testing without FL Studio.

## Use in FL Studio

1. Options > Manage plugins > **Find installed plugins** (make sure the VST3 folder is scanned), then refresh.
2. Put PitchPilot on a mixer insert as an effect. Latency is reported to FL Studio, so it stays in time with the rest of the project.
3. Set the Key and Scale to your song, then raise or lower Retune Speed (0 ms = hard robotic tune, ~30+ ms = natural).
4. For Target Notes / Learn Scale with a MIDI keyboard, route MIDI to the plugin in FL Studio's plugin wrapper settings (MIDI input port). The on-screen keys work without any routing.

## Files

| File | What |
|---|---|
| `Source/Engine.h` | pitch detector, note targeting, retune/flex/humanize, vibrato, PSOLA shifter |
| `Source/FX.h` | EQ, compressor, distortion/crush, reverb, auto polish |
| `Source/Params.h` | every parameter, the scales, and the two style presets |
| `Source/PluginProcessor.*` | JUCE plugin, parameters, MIDI, state |
| `Source/PluginEditor.*` | the interface |
