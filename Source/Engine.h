// PitchPilot DSP engine - no JUCE dependency, header only.
//
//   PitchAnalyzer : YIN pitch detector (runs on a decimated copy of the input)
//   Corrector     : note targeting, retune / flex / humanize, vibrato, formant logic
//   PsolaShifter  : pitch-synchronous grain shifter (keeps formants unless told otherwise)
//   Engine        : glues the three together for 1-2 channels
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace pp
{
enum NoteState : int { NoteTarget = 0, NoteRemove = 1, NoteBypass = 2 };

struct InputRange { float minHz; float maxHz; };

// 0 Soprano, 1 Alto/Tenor, 2 Low Male, 3 Instrument
inline InputRange rangeForInputType (int t)
{
    static const InputRange ranges[] = { { 180.f, 1400.f }, { 100.f, 800.f }, { 65.f, 500.f }, { 50.f, 1500.f } };
    return ranges[std::clamp (t, 0, 3)];
}

struct Settings
{
    int   inputType = 1;
    float refA = 440.f;            // detune / reference pitch in Hz
    float retuneMs = 15.f;         // 0 = instant
    float flex = 0.f;              // 0..100
    float humanize = 0.f;          // 0..100
    float naturalVibrato = 0.f;    // -4..4, 0 = leave as is
    float throat = 100.f;          // 50..150, 100 = neutral
    float tracking = 50.f;         // 0..100
    int   transpose = 0;           // semitones
    bool  classic = false;
    bool  formantPreserve = true;
    bool  hold = false;
    bool  ignoreVibrato = true;    // pick target note from the vibrato-free centre pitch
    bool  midiMode = false;
    float mix = 1.f;               // 0..1
    float outputGain = 1.f;

    // create vibrato
    int   vibShape = 0;            // 0 none, 1 sine, 2 triangle, 3 square, 4 saw
    float vibRate = 5.5f;
    float vibDelayMs = 500.f;
    float vibOnsetMs = 500.f;
    float vibVariation = 20.f;
    float vibPitchCents = 18.f;
    float vibAmpPct = 10.f;
    float vibFormantPct = 70.f;

    std::array<int, 12> notes {};  // NoteState per pitch class, C = 0
    std::array<float, 12> noteCents {}; // per pitch class detune of the target note, in cents
    unsigned midiHeldMask = 0;     // bit per pitch class currently held on MIDI input
};

//==============================================================================
class PitchAnalyzer
{
public:
    struct Result
    {
        bool voiced = false;
        double period = 0.0;   // in input samples
        double freq = 0.0;
        float confidence = 0.f;
    };

    void prepare (double sr)
    {
        sampleRate = sr;
        decim = std::max (1, (int) std::lround (sr / 11025.0));
        fsd = sr / decim;
        hop = std::max (32, (int) (sr * 0.003));
        ring.assign ((size_t) ringSize, 0.f);
        tmp.assign (1024, 0.f);
        d.assign (512, 0.f);
        dp.assign (512, 0.f);
        setRange (rangeForInputType (1));
    }

    void setRange (InputRange r)
    {
        tauMin = std::max (4, (int) std::floor (fsd / r.maxHz));
        tauMax = std::min (400, (int) std::ceil (fsd / r.minHz));
        win = tauMax;
        need = win + tauMax + 2;
        reset();
    }

    void reset()
    {
        std::fill (ring.begin(), ring.end(), 0.f);
        wpos = 0; accum = 0.f; accCount = 0; hopCount = 0; medCount = 0;
    }

    int hopSamples() const { return hop; }

    // Feed one input sample. Returns true when a new analysis should run.
    bool push (float x)
    {
        accum += x;
        if (++accCount >= decim)
        {
            ring[(size_t) (wpos & ringMask)] = accum / (float) decim;
            ++wpos;
            accum = 0.f;
            accCount = 0;
        }
        if (++hopCount >= hop)
        {
            hopCount = 0;
            return wpos >= (int64_t) need;
        }
        return false;
    }

    Result analyze (float threshold)
    {
        Result res;
        const int N = need;
        for (int k = 0; k < N; ++k)
            tmp[(size_t) k] = ring[(size_t) ((wpos - N + k) & ringMask)];

        double energy = 0.0;
        for (int k = N - win; k < N; ++k)
            energy += (double) tmp[(size_t) k] * tmp[(size_t) k];
        if (std::sqrt (energy / win) < 0.002)
        {
            medCount = 0;
            return res;
        }

        for (int tau = 1; tau <= tauMax; ++tau)
        {
            double sum = 0.0;
            for (int j = 0; j < win; ++j)
            {
                const double diff = (double) tmp[(size_t) j] - (double) tmp[(size_t) (j + tau)];
                sum += diff * diff;
            }
            d[(size_t) tau] = (float) sum;
        }

        dp[0] = 1.f;
        double running = 0.0;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            running += d[(size_t) tau];
            dp[(size_t) tau] = running > 0.0 ? (float) (d[(size_t) tau] * tau / running) : 1.f;
        }

        int best = -1;
        for (int tau = tauMin; tau < tauMax; ++tau)
        {
            if (dp[(size_t) tau] < threshold)
            {
                while (tau + 1 < tauMax && dp[(size_t) (tau + 1)] < dp[(size_t) tau])
                    ++tau;
                best = tau;
                break;
            }
        }

        if (best < 0)
        {
            int m = tauMin;
            for (int tau = tauMin; tau < tauMax; ++tau)
                if (dp[(size_t) tau] < dp[(size_t) m]) m = tau;
            if (dp[(size_t) m] < threshold + 0.1f) best = m;
        }

        if (best < 1 || best >= tauMax)
        {
            medCount = 0;
            return res;
        }

        const double a = dp[(size_t) (best - 1)], b = dp[(size_t) best], c = dp[(size_t) (best + 1)];
        const double denom = a - 2.0 * b + c;
        double shift = std::abs (denom) > 1e-12 ? 0.5 * (a - c) / denom : 0.0;
        shift = std::clamp (shift, -1.0, 1.0);
        const double period = (best + shift) * decim;

        // median of the last three periods for stability
        medHist[(size_t) (medCount % 3)] = period;
        ++medCount;
        double p = period;
        if (medCount >= 3)
        {
            double v[3] = { medHist[0], medHist[1], medHist[2] };
            std::sort (v, v + 3);
            p = v[1];
        }

        res.voiced = true;
        res.period = p;
        res.freq = sampleRate / p;
        res.confidence = 1.f - dp[(size_t) best];
        return res;
    }

private:
    static constexpr int ringSize = 4096;
    static constexpr int64_t ringMask = ringSize - 1;

    double sampleRate = 44100.0, fsd = 11025.0;
    int decim = 4, hop = 132, tauMin = 8, tauMax = 110, win = 110, need = 222;
    std::vector<float> ring, tmp, d, dp;
    int64_t wpos = 0;
    float accum = 0.f;
    int accCount = 0, hopCount = 0;
    double medHist[3] { 0, 0, 0 };
    int medCount = 0;
};

//==============================================================================
class Corrector
{
public:
    struct Output
    {
        bool   voiced = false;
        double period = 0.0;
        double ratio = 1.0;      // pitch ratio to apply
        double formant = 1.0;    // grain read-rate factor (spectral envelope shift)
        float  gain = 1.f;       // amplitude from created vibrato
        float  detMidi = 0.f;    // detected pitch as (fractional) MIDI note
        float  targetMidi = 0.f;
        float  outMidi = 0.f;
        int    targetNote = 0;
    };

    void reset()
    {
        out = Output();
        hasSlow = false; lockedValid = false; lastTarget = -1000;
        noteAge = 0.0; vibAge = 0.0; corr = 0.0; unvoicedHops = 0;
    }

    const Output& output() const { return out; }

    const Output& update (const PitchAnalyzer::Result& r, const Settings& s, double dt)
    {
        if (! r.voiced)
        {
            if (++unvoicedHops > 4)
            {
                out.voiced = false;
                hasSlow = false; lockedValid = false; lastTarget = -1000;
                noteAge = 0.0; vibAge = 0.0; corr = 0.0;
            }
            return out;
        }

        unvoicedHops = 0;
        out.voiced = true;
        out.period = r.period;

        const double midi = 69.0 + 12.0 * std::log2 (r.freq / (double) s.refA);

        // vibrato-free centre pitch
        if (! hasSlow || std::abs (midi - slow) > 0.9)
        {
            slow = midi;
            hasSlow = true;
        }
        else
        {
            slow += (1.0 - std::exp (-dt / 0.15)) * (midi - slow);
        }

        const double pitchForTarget = s.ignoreVibrato ? slow : midi;
        const bool midiActive = s.midiMode && s.midiHeldMask != 0;
        const int target = chooseTarget (pitchForTarget, s, midiActive);

        if (target != lastTarget)
        {
            noteAge = 0.0;
            vibAge = 0.0;
            lastTarget = target;
        }
        noteAge += dt;
        vibAge += dt;

        bool bypass = (target == noTarget);
        if (! bypass && ! midiActive)
            bypass = s.notes[(size_t) pitchClass (target)] == NoteBypass;

        const double off = bypass ? 0.0 : (double) s.noteCents[(size_t) pitchClass (target)] * 0.01;
        double e = bypass ? 0.0 : (double) target + off - midi;

        if (! s.classic && s.flex > 0.f)
        {
            const double tol = s.flex / 100.0 * 0.6;
            const double ae = std::abs (e);
            if (ae < tol)
                e *= (ae / tol) * (ae / tol);
        }

        double tau = s.retuneMs / 1000.0;
        if (! s.classic)
            tau += s.humanize / 100.0 * 0.25 * std::clamp (noteAge / 0.4, 0.0, 1.0);

        if (tau <= 0.0005)
            corr = e;
        else
            corr += (1.0 - std::exp (-dt / tau)) * (e - corr);

        // natural vibrato scaling (only meaningful while retune is slow enough to leave vibrato)
        const double vib = midi - slow;
        const double w = std::clamp (tau / 0.05, 0.0, 1.0);
        const double nvK = s.classic ? 0.0 : std::clamp ((double) s.naturalVibrato * 0.5, -1.0, 2.0);
        const double nvTerm = nvK * vib * w;

        // created vibrato
        double vibSemis = 0.0, formantMod = 1.0;
        float ampGain = 1.f;
        if (s.vibShape > 0)
        {
            const double delay = s.vibDelayMs / 1000.0, onset = s.vibOnsetMs / 1000.0;
            double depth = 0.0;
            if (vibAge >= delay)
                depth = onset <= 0.001 ? 1.0 : std::clamp ((vibAge - delay) / onset, 0.0, 1.0);

            noiseTimer += dt;
            if (noiseTimer > 0.25)
            {
                noiseTimer = 0.0;
                nTarget1 = rnd();
                nTarget2 = rnd();
            }
            const double k = 1.0 - std::exp (-dt / 0.3);
            n1 += (nTarget1 - n1) * k;
            n2 += (nTarget2 - n2) * k;

            const double var = s.vibVariation / 100.0;
            const double rate = std::max (0.1, (double) s.vibRate * (1.0 + var * 0.5 * n1));
            const double depthEff = depth * std::max (0.0, 1.0 + var * 0.5 * n2);

            phase += rate * dt;
            phase -= std::floor (phase);

            double wave = 0.0;
            switch (s.vibShape)
            {
                case 1: wave = std::sin (6.283185307179586 * phase); break;
                case 2: wave = 1.0 - 4.0 * std::abs (phase - 0.5) ; wave = -wave; break;
                case 3: wave = std::tanh (6.0 * std::sin (6.283185307179586 * phase)); break;
                case 4: wave = 2.0 * phase - 1.0; break;
                default: break;
            }

            vibSemis = depthEff * s.vibPitchCents / 100.0 * wave;
            ampGain = (float) (1.0 + depthEff * s.vibAmpPct / 100.0 * 0.5 * wave);
            formantMod = 1.0 + depthEff * s.vibFormantPct / 100.0 * 0.08 * wave;
        }

        const double shift = corr + nvTerm + vibSemis + (double) s.transpose;
        const double ratio = std::clamp (std::pow (2.0, shift / 12.0), 0.5, 2.0);

        double fs = (100.0 / std::clamp ((double) s.throat, 25.0, 400.0)) * formantMod;
        if (! s.formantPreserve)
            fs *= ratio;               // formants follow the pitch (classic "chipmunk" shift)

        out.ratio = ratio;
        out.formant = std::clamp (fs, 0.5, 2.0);
        out.gain = ampGain;
        out.detMidi = (float) midi;
        out.targetMidi = bypass ? (float) midi : (float) ((double) target + off);
        out.outMidi = (float) (midi + shift);
        out.targetNote = bypass ? 0 : pitchClass (target);
        return out;
    }

private:
    static constexpr int noTarget = -1000;

    static int pitchClass (int n) { return ((n % 12) + 12) % 12; }

    int chooseTarget (double p, const Settings& s, bool midiActive)
    {
        auto pos = [&] (int n) { return (double) n + (double) s.noteCents[(size_t) pitchClass (n)] * 0.01; };
        auto allowed = [&] (int n)
        {
            const int pc = pitchClass (n);
            if (midiActive) return ((s.midiHeldMask >> pc) & 1u) != 0u;
            return s.notes[(size_t) pc] != NoteRemove;
        };

        int best = noTarget;
        double bestDist = 1e9;
        const int base = (int) std::floor (p);
        for (int n = base - 7; n <= base + 8; ++n)
        {
            if (! allowed (n)) continue;
            const double dist = std::abs (p - pos (n));
            if (dist < bestDist) { bestDist = dist; best = n; }
        }

        if (lockedValid)
        {
            if (s.hold)
                return locked;
            if (allowed (locked) && best != noTarget && std::abs (p - pos (locked)) <= bestDist + 0.15)
                return locked;
        }

        if (best != noTarget)
        {
            locked = best;
            lockedValid = true;
        }
        return best;
    }

    double rnd()
    {
        rngState ^= rngState << 13;
        rngState ^= rngState >> 17;
        rngState ^= rngState << 5;
        return (double) (rngState & 0xffffff) / (double) 0x7fffff - 1.0;
    }

    Output out;
    double slow = 0.0, noteAge = 0.0, vibAge = 0.0, corr = 0.0;
    double phase = 0.0, noiseTimer = 0.0, n1 = 0.0, n2 = 0.0, nTarget1 = 0.0, nTarget2 = 0.0;
    bool hasSlow = false, lockedValid = false;
    int locked = 0, lastTarget = -1000, unvoicedHops = 0;
    uint32_t rngState = 2463534242u;
};

//==============================================================================
class PsolaShifter
{
public:
    void prepare (int bufferSize)
    {
        N = bufferSize;
        mask = (int64_t) N - 1;
        in.assign ((size_t) N, 0.f);
        ola.assign ((size_t) N, 0.f);
        reset();
    }

    void setLatency (int latencySamples) { L = latencySamples; reset(); }

    void reset()
    {
        std::fill (in.begin(), in.end(), 0.f);
        std::fill (ola.begin(), ola.end(), 0.f);
        wp = 0; haveC = false; nextGrain = 0; cPrev = 0.0; vSm = 0.f; wasVoiced = false;
    }

    float process (float x, bool voiced, double period, double ratio, double formant, float vCoef, float mix)
    {
        in[(size_t) (wp & mask)] = x;
        const int64_t n = wp++;

        if (voiced)
        {
            if (! wasVoiced) { haveC = false; nextGrain = n; }
            if (n >= nextGrain && period > 4.0)
                scheduleGrain (n, period, ratio, formant);
        }
        wasVoiced = voiced;

        vSm += ((voiced ? 1.f : 0.f) - vSm) * vCoef;

        const size_t oi = (size_t) (n & mask);
        const float wet = ola[oi];
        ola[oi] = 0.f;
        const float dry = n >= L ? in[(size_t) ((n - L) & mask)] : 0.f;

        const float shifted = vSm * wet + (1.f - vSm) * dry;
        return mix * shifted + (1.f - mix) * dry;
    }

private:
    float readIn (double pos) const
    {
        const int64_t i0 = (int64_t) std::floor (pos);
        const float frac = (float) (pos - (double) i0);
        const float a = in[(size_t) (i0 & mask)];
        const float b = in[(size_t) ((i0 + 1) & mask)];
        return a + frac * (b - a);
    }

    void scheduleGrain (int64_t n, double P, double ratio, double fs)
    {
        const double Tout = P / ratio;
        const double T = std::clamp (Tout, 16.0, (double) N / 8.0);
        const double mc = nextGrain + T;                       // output-time centre of this grain
        const double cStar = mc - (double) L;         // wanted source centre

        double c;
        if (! haveC || std::abs (cStar - cPrev) > 6.0 * P)
        {
            c = cStar;
        }
        else
        {
            double k = std::round ((cStar - cPrev) / P);
            if (k < 0.0) k = 0.0;
            c = cPrev + k * P;                         // whole periods only -> phase coherent
        }

        // never read samples that have not arrived yet
        int guard = 0;
        while (c + (double) T * fs + 2.0 > (double) n && guard++ < 8)
            c -= P;
        const double lowest = (double) (n - (int64_t) N + 16) + (double) T * fs;
        while (c < lowest && guard++ < 16)
            c += P;

        const int64_t j1 = (int64_t) std::floor (mc + T);
        for (int64_t j = std::max<int64_t> (n, (int64_t) std::ceil (mc - T)); j <= j1; ++j)
        {
            const double dd = (double) j - mc;
            const float w = 0.5f + 0.5f * (float) std::cos (3.14159265358979 * dd / T);
            ola[(size_t) (j & mask)] += w * readIn (c + dd * fs);
        }

        cPrev = c;
        haveC = true;
        nextGrain = mc;
    }

    std::vector<float> in, ola;
    int N = 32768;
    int64_t mask = 32767, wp = 0;
    double nextGrain = 0.0;
    int L = 2048;
    double cPrev = 0.0;
    bool haveC = false, wasVoiced = false;
    float vSm = 0.f;
};

//==============================================================================
class Engine
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        int size = 1024;
        while ((double) size < sr * 0.5) size <<= 1;
        analyzer.prepare (sr);
        for (auto& s : shifters) s.prepare (size);
        vCoef = (float) (1.0 - std::exp (-1.0 / (0.003 * sr)));
        gCoef = (float) (1.0 - std::exp (-1.0 / (0.005 * sr)));
        inputType = -1;
        setInputType (1);
    }

    void setInputType (int t)
    {
        t = std::clamp (t, 0, 3);
        inputType = t;
        const auto range = rangeForInputType (t);
        analyzer.setRange (range);
        latency = (int) std::ceil (4.0 * sampleRate / range.minHz);
        for (auto& s : shifters) s.setLatency (latency);
        corrector.reset();
        gainSm = 1.f;
    }

    int latencySamples() const { return latency; }
    const Corrector::Output& state() const { return corrector.output(); }

    void process (float* const* ch, int numCh, int numSamples, const Settings& s)
    {
        numCh = std::clamp (numCh, 1, 2);
        if (s.inputType != inputType)
            setInputType (s.inputType);

        const float thr = 0.30f - 0.0022f * s.tracking;
        const double hopSec = (double) analyzer.hopSamples() / sampleRate;

        for (int i = 0; i < numSamples; ++i)
        {
            float mono = 0.f;
            for (int c = 0; c < numCh; ++c) mono += ch[c][i];
            mono /= (float) numCh;

            if (analyzer.push (mono))
                corrector.update (analyzer.analyze (thr), s, hopSec);

            const auto& o = corrector.output();
            gainSm += ((o.voiced ? o.gain : 1.f) - gainSm) * gCoef;

            for (int c = 0; c < numCh; ++c)
                ch[c][i] = shifters[(size_t) c].process (ch[c][i], o.voiced, o.period, o.ratio, o.formant, vCoef, s.mix)
                           * gainSm * s.outputGain;
        }
    }

private:
    double sampleRate = 44100.0;
    int inputType = 1, latency = 1764;
    float vCoef = 0.01f, gCoef = 0.005f, gainSm = 1.f;
    PitchAnalyzer analyzer;
    Corrector corrector;
    std::array<PsolaShifter, 2> shifters;
};

} // namespace pp
