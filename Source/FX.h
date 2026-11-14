// PitchPilot effects (JUCE-free, header only).
//
// Signal order:  Tone EQ -> Compressor -> Distortion / Bit-crush -> Space (reverb) -> Auto Polish
// Every stage is skipped completely when its controls are at their neutral values,
// so a default FxSettings leaves the audio bit-exact.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace pp
{
// The five processing stages. The default order below is the best-sounding one:
// shape the tone first, even out dynamics, then add grit, then space, and finish with the polish.
enum StageId : int { StageTone = 0, StageComp, StageCrush, StageSpace, StagePolish };

struct FxSettings
{
    float lowCutHz   = 20.f;    // <= 25 means off
    float bodyDb     = 0.f;     // low shelf at 150 Hz
    float presenceDb = 0.f;     // bell at 3.5 kHz
    float airDb      = 0.f;     // high shelf at 10 kHz
    float compress   = 0.f;     // 0..100
    float drive      = 0.f;     // 0..100
    float bits       = 16.f;    // 1..16 (16 = off)
    float downsample = 1.f;     // 1..32 (1 = off)
    float crushMix   = 100.f;   // 0..100 : how much of the distorted/crushed signal is used
    float space      = 0.f;     // 0..100 : reverb amount
    float spaceSize  = 50.f;    // 0..100 : reverb decay
    std::array<int, 5> order { StageTone, StageComp, StageCrush, StageSpace, StagePolish };   // slot -> StageId
    bool  polish     = false;   // auto "soundgoodizer"
    float polishAmt  = 60.f;    // 0..100
};

// Transparent soft limiter: linear below 0.6, smoothly bends towards 1.0 above.
inline float softLimit (float x)
{
    constexpr float t = 0.6f;
    const float a = std::abs (x);
    if (a <= t) return x;
    const float y = t + (1.f - t) * std::tanh ((a - t) / (1.f - t));
    return x < 0.f ? -y : y;
}

//==============================================================================
struct Biquad
{
    float b0 = 1.f, b1 = 0.f, b2 = 0.f, a1 = 0.f, a2 = 0.f, z1 = 0.f, z2 = 0.f;

    void reset() { z1 = z2 = 0.f; }
    void copyCoeffs (const Biquad& o) { b0 = o.b0; b1 = o.b1; b2 = o.b2; a1 = o.a1; a2 = o.a2; }

    float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void highPass (double sr, double f, double q = 0.7071)
    {
        const double w = 6.283185307179586 * f / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }

    void peak (double sr, double f, double q, double dB)
    {
        const double A = std::pow (10.0, dB / 40.0), w = 6.283185307179586 * f / sr;
        const double c = std::cos (w), al = std::sin (w) / (2.0 * q);
        set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }

    void lowShelf (double sr, double f, double dB)
    {
        const double A = std::pow (10.0, dB / 40.0), w = 6.283185307179586 * f / sr;
        const double c = std::cos (w), beta = 2.0 * std::sqrt (A) * (std::sin (w) * 0.70710678);
        set (A * ((A + 1) - (A - 1) * c + beta), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - beta),
             (A + 1) + (A - 1) * c + beta, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - beta);
    }

    void highShelf (double sr, double f, double dB)
    {
        const double A = std::pow (10.0, dB / 40.0), w = 6.283185307179586 * f / sr;
        const double c = std::cos (w), beta = 2.0 * std::sqrt (A) * (std::sin (w) * 0.70710678);
        set (A * ((A + 1) + (A - 1) * c + beta), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - beta),
             (A + 1) - (A - 1) * c + beta, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - beta);
    }

private:
    void set (double B0, double B1, double B2, double A0, double A1, double A2)
    {
        b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0);
        a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
    }
};

//==============================================================================
// Stereo-linked compressor whose threshold follows the signal's own running level,
// so it works the same whether the vocal is quiet or hot ("auto" gain staging).
class AutoCompressor
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        trkCoef = (float) (1.0 - std::exp (-1.0 / (sr * 1.5)));
        reset();
    }

    void reset() { env = 0.f; trk = -30.f; }

    // offsetDb: threshold relative to the running level (negative = more compression)
    void set (float offsetDb_, float ratio_, float attackMs, float releaseMs, float makeupFrac_)
    {
        offsetDb = offsetDb_;
        ratio = std::max (1.f, ratio_);
        makeupFrac = makeupFrac_;
        aCoef = (float) (1.0 - std::exp (-1.0 / (sr * attackMs * 0.001)));
        rCoef = (float) (1.0 - std::exp (-1.0 / (sr * releaseMs * 0.001)));
    }

    void process (float& l, float& r)
    {
        const float det = std::max (std::abs (l), std::abs (r));
        env += (det > env ? aCoef : rCoef) * (det - env);
        const float db = 6.0206f * std::log2 (std::max (env, 1e-6f));
        if (db > -60.f) trk += trkCoef * (db - trk);

        const float over = std::max (0.f, db - (trk + offsetDb));
        const float gr = over * (1.f - 1.f / ratio);
        const float makeup = -offsetDb * (1.f - 1.f / ratio) * makeupFrac;
        const float g = std::exp2 ((makeup - gr) * 0.1661f);   // dB -> linear
        l *= g;
        r *= g;
    }

private:
    double sr = 44100.0;
    float env = 0.f, trk = -30.f, trkCoef = 0.f, aCoef = 0.f, rCoef = 0.f;
    float offsetDb = -6.f, ratio = 2.f, makeupFrac = 0.7f;
};

//==============================================================================
struct Crusher
{
    float held = 0.f, phase = 0.f;
    void reset() { held = 0.f; phase = 0.f; }

    // driveGain 1 = clean; levels 0 = no quantising; ds 1 = no sample-rate reduction
    float process (float x, float driveGain, float makeup, float levels, float ds)
    {
        float y = x;
        if (driveGain > 1.0001f) y = std::tanh (y * driveGain) * makeup;
        if (levels > 0.f) y = std::round (y * levels) / levels;
        if (ds > 1.001f)
        {
            phase += 1.f;
            if (phase >= ds) { phase -= ds; held = y; }
            y = held;
        }
        return y;
    }
};

//==============================================================================
// Freeverb-style reverb (8 combs + 4 all-passes per side).
class Reverb
{
public:
    void prepare (double sr)
    {
        static const int combLen[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
        static const int apLen[4]   = { 556, 441, 341, 225 };
        const double k = sr / 44100.0;
        for (int side = 0; side < 2; ++side)
        {
            const int spread = side == 0 ? 0 : 23;
            for (int i = 0; i < 8; ++i) comb[side][i].init ((int) std::lround ((combLen[i] + spread) * k));
            for (int i = 0; i < 4; ++i) ap[side][i].init ((int) std::lround ((apLen[i] + spread) * k));
        }
    }

    void clear()
    {
        for (int s = 0; s < 2; ++s)
        {
            for (auto& c : comb[s]) c.clear();
            for (auto& a : ap[s])   a.clear();
        }
    }

    // size 0..1. Returns the wet signal only.
    void process (float inL, float inR, float size, float& outL, float& outR)
    {
        const float fb = 0.72f + 0.24f * size, damp = 0.35f;
        const float in = (inL + inR) * 0.015f;
        float o[2] = { 0.f, 0.f };
        for (int s = 0; s < 2; ++s)
        {
            for (auto& c : comb[s]) o[s] += c.process (in, fb, damp);
            for (auto& a : ap[s])   o[s] = a.process (o[s]);
        }
        outL = o[0] * wetScale;
        outR = o[1] * wetScale;
    }

    static constexpr float wetScale = 2.0f;

private:
    struct Comb
    {
        std::vector<float> buf; int idx = 0; float store = 0.f;
        void init (int n) { buf.assign ((size_t) std::max (1, n), 0.f); idx = 0; store = 0.f; }
        void clear() { std::fill (buf.begin(), buf.end(), 0.f); store = 0.f; }
        float process (float in, float fb, float damp)
        {
            const float out = buf[(size_t) idx];
            store = out * (1.f - damp) + store * damp;
            buf[(size_t) idx] = in + store * fb;
            if (++idx >= (int) buf.size()) idx = 0;
            return out;
        }
    };

    struct Allpass
    {
        std::vector<float> buf; int idx = 0;
        void init (int n) { buf.assign ((size_t) std::max (1, n), 0.f); idx = 0; }
        void clear() { std::fill (buf.begin(), buf.end(), 0.f); }
        float process (float in)
        {
            const float b = buf[(size_t) idx];
            const float out = -in + b;
            buf[(size_t) idx] = in + b * 0.5f;
            if (++idx >= (int) buf.size()) idx = 0;
            return out;
        }
    };

    Comb comb[2][8];
    Allpass ap[2][4];
};

//==============================================================================
class FxChain
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        reverb.prepare (sr);
        comp.prepare (sr);
        polishComp.prepare (sr);
        wetCoef = (float) (1.0 - std::exp (-1.0 / (0.02 * sr)));
        reset();
        lastLowCut = lastBody = lastPres = lastAir = lastPolishAmt = -1000.f;
    }

    void reset()
    {
        for (int c = 0; c < 2; ++c)
        {
            toneHP[c].reset(); toneLS[c].reset(); tonePK[c].reset(); toneHS[c].reset();
            polHP[c].reset(); polLM[c].reset(); polPK[c].reset(); polHS[c].reset();
            crush[c].reset();
        }
        comp.reset();
        polishComp.reset();
        reverb.clear();
        wet = 0.f;
    }

    void process (float* l, float* r, int n, const FxSettings& s)
    {
        const bool lowCutOn = s.lowCutHz > 25.f;
        const bool bodyOn = std::abs (s.bodyDb) > 0.05f, presOn = std::abs (s.presenceDb) > 0.05f, airOn = std::abs (s.airDb) > 0.05f;
        const bool compOn = s.compress > 0.5f;
        const bool crushOn = s.crushMix > 0.5f && (s.drive > 0.5f || s.bits < 15.99f || s.downsample > 1.001f);
        const bool polishOn = s.polish && s.polishAmt > 0.5f;

        updateTone (s, lowCutOn, bodyOn, presOn, airOn);
        if (compOn)
        {
            const float a = s.compress * 0.01f;
            comp.set (-(2.f + 8.f * a), 1.5f + 4.5f * a, 6.f, 110.f - 40.f * a, 0.75f);
        }
        if (polishOn) updatePolish (s);

        const float g = 1.f + s.drive * 0.12f;
        const float gMakeup = 1.f / std::sqrt (g);
        const float levels = s.bits < 15.99f ? std::exp2 (std::max (1.f, s.bits) - 1.f) : 0.f;
        const float cm = s.crushMix * 0.01f;
        const float wetTarget = s.space * 0.01f;
        const float size = s.spaceSize * 0.01f;
        const float pa = s.polishAmt * 0.01f;
        const float preGain = 1.f + 0.9f * pa;

        const auto ord = sanitiseOrder (s);

        for (int i = 0; i < n; ++i)
        {
            float a = l[i], b = r[i];

            for (int k = 0; k < 5; ++k)
            {
                switch (ord[(size_t) k])
                {
                    case StageTone:
                        if (lowCutOn) { a = toneHP[0].process (a); b = toneHP[1].process (b); }
                        if (bodyOn)   { a = toneLS[0].process (a); b = toneLS[1].process (b); }
                        if (presOn)   { a = tonePK[0].process (a); b = tonePK[1].process (b); }
                        if (airOn)    { a = toneHS[0].process (a); b = toneHS[1].process (b); }
                        break;

                    case StageComp:
                        if (compOn) comp.process (a, b);
                        break;

                    case StageCrush:
                        if (crushOn)
                        {
                            const float ya = crush[0].process (a, g, gMakeup, levels, s.downsample);
                            const float yb = crush[1].process (b, g, gMakeup, levels, s.downsample);
                            a += cm * (ya - a);
                            b += cm * (yb - b);
                        }
                        break;

                    case StageSpace:
                    {
                        wet += (wetTarget - wet) * wetCoef;
                        if (wet > 1e-4f || wetTarget > 0.001f)
                        {
                            float wl, wr;
                            reverb.process (a, b, size, wl, wr);
                            a += wl * wet;
                            b += wr * wet;
                            reverbWasOn = true;
                        }
                        else if (reverbWasOn)
                        {
                            reverb.clear();
                            reverbWasOn = false;
                        }
                        break;
                    }

                    case StagePolish:
                        if (polishOn)
                        {
                            a = polHP[0].process (a); b = polHP[1].process (b);
                            a = polLM[0].process (a); b = polLM[1].process (b);
                            a = polPK[0].process (a); b = polPK[1].process (b);
                            a = polHS[0].process (a); b = polHS[1].process (b);
                            polishComp.process (a, b);
                            a = softLimit (a * preGain);
                            b = softLimit (b * preGain);
                        }
                        break;

                    default: break;
                }
            }

            l[i] = a;
            r[i] = b;
        }
    }

private:
    // Repairs any order: duplicates and out-of-range values are dropped, missing stages are appended in default order.
    static std::array<int, 5> sanitiseOrder (const FxSettings& s)
    {
        std::array<int, 5> o {};
        bool used[5] = { false, false, false, false, false };
        int n = 0;
        for (int k = 0; k < 5; ++k)
        {
            const int v = s.order[(size_t) k];
            if (v >= 0 && v < 5 && ! used[v]) { used[v] = true; o[(size_t) n++] = v; }
        }
        for (int v = 0; v < 5; ++v)
            if (! used[v]) o[(size_t) n++] = v;
        return o;
    }

    void updateTone (const FxSettings& s, bool lowCutOn, bool bodyOn, bool presOn, bool airOn)
    {
        if (lowCutOn && s.lowCutHz != lastLowCut) { toneHP[0].highPass (sr, s.lowCutHz); toneHP[1].copyCoeffs (toneHP[0]); }
        if (bodyOn && s.bodyDb != lastBody)       { toneLS[0].lowShelf (sr, 150.0, s.bodyDb); toneLS[1].copyCoeffs (toneLS[0]); }
        if (presOn && s.presenceDb != lastPres)   { tonePK[0].peak (sr, 3500.0, 0.9, s.presenceDb); tonePK[1].copyCoeffs (tonePK[0]); }
        if (airOn && s.airDb != lastAir)          { toneHS[0].highShelf (sr, 10000.0, s.airDb); toneHS[1].copyCoeffs (toneHS[0]); }
        lastLowCut = s.lowCutHz; lastBody = s.bodyDb; lastPres = s.presenceDb; lastAir = s.airDb;
    }

    void updatePolish (const FxSettings& s)
    {
        const float a = s.polishAmt * 0.01f;
        polishComp.set (-(3.f + 6.f * a), 1.5f + 2.5f * a, 8.f, 90.f, 1.0f);
        if (a == lastPolishAmt) return;
        lastPolishAmt = a;
        polHP[0].highPass (sr, 30.0);                   polHP[1].copyCoeffs (polHP[0]);
        polLM[0].peak (sr, 320.0, 1.0, -2.2 * a);       polLM[1].copyCoeffs (polLM[0]);
        polPK[0].peak (sr, 3800.0, 0.9, 2.5 * a);       polPK[1].copyCoeffs (polPK[0]);
        polHS[0].highShelf (sr, 11000.0, 3.0 * a);      polHS[1].copyCoeffs (polHS[0]);
    }

    double sr = 44100.0;
    float wetCoef = 0.002f, wet = 0.f;
    bool reverbWasOn = false;
    float lastLowCut = -1000.f, lastBody = -1000.f, lastPres = -1000.f, lastAir = -1000.f, lastPolishAmt = -1000.f;

    Biquad toneHP[2], toneLS[2], tonePK[2], toneHS[2];
    Biquad polHP[2], polLM[2], polPK[2], polHS[2];
    AutoCompressor comp, polishComp;
    Crusher crush[2];
    Reverb reverb;
};

} // namespace pp
