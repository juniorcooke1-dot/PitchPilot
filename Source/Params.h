// PitchPilot parameter table, scales and style presets (JUCE-free so it is easy to read and test).
#pragma once

#include <cstddef>

namespace pp
{
struct FloatDef
{
    const char* id;
    const char* name;
    float lo, hi, def, step, skew;
    const char* unit;    // shown after the value, e.g. " ms"
    int decimals;
};

inline constexpr FloatDef kFloats[] =
{
    // ---- pitch correction ----
    { "retune",     "Retune Speed",    0.f,   400.f,  15.f,  0.1f, 0.4f, " ms", 0 },
    { "flex",       "Flex-Tune",       0.f,   100.f,  0.f,   1.f,  1.f,  "",    0 },
    { "humanize",   "Humanize",        0.f,   100.f,  0.f,   1.f,  1.f,  "",    0 },
    { "natvib",     "Natural Vibrato", -4.f,  4.f,    0.f,   0.1f, 1.f,  "",    1 },
    { "throat",     "Throat",          50.f,  150.f,  100.f, 1.f,  1.f,  "",    0 },
    { "transpose",  "Transpose",       -12.f, 12.f,   0.f,   1.f,  1.f,  " st", 0 },
    { "detune",     "Detune",          415.f, 466.f,  440.f, 0.1f, 1.f,  " Hz", 1 },
    { "tracking",   "Tracking",        0.f,   100.f,  50.f,  1.f,  1.f,  "",    0 },

    // ---- create vibrato ----
    { "vibrate",    "Rate",            0.5f,  10.f,   5.5f,  0.1f, 1.f,  " Hz", 1 },
    { "vibdelay",   "Onset Delay",     0.f,   2000.f, 500.f, 1.f,  0.6f, " ms", 0 },
    { "vibonset",   "Onset Rate",      0.f,   2000.f, 500.f, 1.f,  0.6f, " ms", 0 },
    { "vibvar",     "Variation",       0.f,   100.f,  20.f,  1.f,  1.f,  "",    0 },
    { "vibpitch",   "Pitch Amt",       0.f,   100.f,  18.f,  1.f,  1.f,  "",    0 },
    { "vibamp",     "Amplitude Amt",   0.f,   100.f,  10.f,  1.f,  1.f,  "",    0 },
    { "vibformant", "Formant Amt",     0.f,   100.f,  70.f,  1.f,  1.f,  "",    0 },

    // ---- output ----
    { "mix",        "Tune Mix",        0.f,   100.f,  100.f, 1.f,  1.f,  " %",  0 },
    { "outgain",    "Output",          -24.f, 12.f,   0.f,   0.1f, 1.f,  " dB", 1 },

    // ---- tone ----
    { "lowcut",     "Low Cut",         20.f,  400.f,  20.f,  1.f,  0.45f, " Hz", 0 },
    { "body",       "Body",            -12.f, 12.f,   0.f,   0.1f, 1.f,  " dB", 1 },
    { "presence",   "Presence",        -12.f, 12.f,   0.f,   0.1f, 1.f,  " dB", 1 },
    { "air",        "Air",             -12.f, 12.f,   0.f,   0.1f, 1.f,  " dB", 1 },
    { "compress",   "Compress",        0.f,   100.f,  0.f,   1.f,  1.f,  "",    0 },

    // ---- distortion / crush ----
    { "drive",      "Drive",           0.f,   100.f,  0.f,   1.f,  1.f,  "",    0 },
    { "bits",       "Bit Depth",       1.f,   16.f,   16.f,  1.f,  1.f,  " bit", 0 },
    { "downsample", "Downsample",      1.f,   32.f,   1.f,   0.1f, 0.5f, "x",   1 },
    { "crushmix",   "Crush Mix",       0.f,   100.f,  100.f, 1.f,  1.f,  " %",  0 },

    // ---- space + polish ----
    { "space",      "Space",           0.f,   100.f,  0.f,   1.f,  1.f,  " %",  0 },
    { "spacesize",  "Size",            0.f,   100.f,  50.f,  1.f,  1.f,  "",    0 },
    { "polishamt",  "Amount",          0.f,   100.f,  60.f,  1.f,  1.f,  "",    0 },
};
inline constexpr std::size_t kNumFloats = sizeof (kFloats) / sizeof (kFloats[0]);

// Scale membership as a 12-bit mask relative to the root (bit 0 = root).
inline constexpr const char* kScaleNames[] =
    { "Chromatic", "Major", "Minor", "Harmonic Minor", "Dorian", "Mixolydian", "Pentatonic Major", "Pentatonic Minor", "Blues" };
inline constexpr unsigned kScaleMasks[] =
    { 0xFFFu, 0xAB5u, 0x5ADu, 0x9ADu, 0x6ADu, 0x6B5u, 0x295u, 0x4A9u, 0x4E9u };
inline constexpr int kNumScales = 9;

inline constexpr const char* kKeyNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
inline constexpr const char* kInputTypes[] = { "Soprano", "Alto/Tenor", "Low Male", "Instrument" };
inline constexpr const char* kVibShapes[]  = { "(No Vibrato)", "Sine", "Triangle", "Square", "Saw" };
inline constexpr const char* kStageNames[] = { "Tone EQ", "Compressor", "Distortion", "Space", "Polish" };
inline constexpr const char* kNoteStates[] = { "Target", "Remove", "Bypass" };   // matches pp::NoteState

inline bool noteInScale (int key, int scale, int pitchClass)
{
    const int rel = ((pitchClass - key) % 12 + 12) % 12;
    return ((kScaleMasks[scale] >> rel) & 1u) != 0u;
}

//==============================================================================
// Style chains. Each value is written to the parameter with that id (real units,
// choices by index, buttons 0/1). Key, scale and note settings are left alone.
//
// These are my own approximations of the *kind* of processing associated with each
// sound (hard tune + brightness/air + crush + space for one; darker, dirtier and
// heavier compression for the other). They are starting points to tweak by ear,
// not measured copies of anyone's real chain, and they cannot change your voice itself.
struct PresetValue { const char* id; float value; };

inline constexpr PresetValue kPresetFakeMink[] =
{
    { "retune", 5.f },   { "flex", 0.f },   { "humanize", 0.f }, { "natvib", 0.f },
    { "throat", 100.f }, { "transpose", 0.f }, { "classic", 0.f }, { "formant", 1.f },
    { "vibshape", 0.f }, { "mix", 100.f },  { "outgain", 0.f },
    { "lowcut", 100.f }, { "body", -1.f },  { "presence", 3.5f }, { "air", 5.f }, { "compress", 55.f },
    { "drive", 15.f },   { "bits", 12.f },  { "downsample", 1.5f }, { "crushmix", 30.f },
    { "space", 35.f },   { "spacesize", 55.f }, { "polish", 1.f }, { "polishamt", 55.f },
};

inline constexpr PresetValue kPresetEsDeeKid[] =
{
    { "retune", 0.f },   { "flex", 0.f },   { "humanize", 0.f }, { "natvib", 0.f },
    { "throat", 110.f }, { "transpose", 0.f }, { "classic", 1.f }, { "formant", 1.f },
    { "vibshape", 0.f }, { "mix", 100.f },  { "outgain", 0.f },
    { "lowcut", 80.f },  { "body", 3.f },   { "presence", 2.f },  { "air", -1.f }, { "compress", 75.f },
    { "drive", 50.f },   { "bits", 9.f },   { "downsample", 3.f }, { "crushmix", 35.f },
    { "space", 18.f },   { "spacesize", 35.f }, { "polish", 1.f }, { "polishamt", 70.f },
};

inline constexpr std::size_t kNumPresetValues = sizeof (kPresetFakeMink) / sizeof (kPresetFakeMink[0]);
static_assert (sizeof (kPresetEsDeeKid) / sizeof (kPresetEsDeeKid[0]) == kNumPresetValues, "presets must set the same parameters");

} // namespace pp
