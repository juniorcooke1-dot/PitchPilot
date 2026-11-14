#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>

#include "Engine.h"
#include "FX.h"
#include "Params.h"

class PitchPilotProcessor : public juce::AudioProcessor,
                            private juce::AsyncUpdater
{
public:
    PitchPilotProcessor();
    ~PitchPilotProcessor() override;

    // AudioProcessor
    const juce::String getName() const override { return "PitchPilot"; }
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- helpers for the editor (message thread) ----
    float getParamValue (const juce::String& id) const;
    void  setParamValue (const juce::String& id, float realValue);
    void  setNoteState (int pitchClass, int state);                    // 0 target, 1 remove, 2 bypass
    void  applyScale (int key, int scale);                             // notes in the scale -> target, others -> remove
    void  setNotesFromMask (unsigned pitchClassMask);                  // used by Learn Scale
    void  applyPreset (const pp::PresetValue* values, std::size_t count);

    juce::AudioProcessorValueTreeState apvts;

    // ---- audio thread -> editor ----
    static constexpr int traceSize = 600;                              // ~6 s at one point per 10 ms
    std::array<std::atomic<float>, traceSize> traceDet, traceOut, traceTarget;   // MIDI note numbers, 0 = unvoiced
    std::atomic<int> traceWrite { 0 };
    std::atomic<int> currentNote { -1 };                               // pitch class being targeted, -1 = none

    // ---- editor -> audio thread / MIDI ----
    std::atomic<unsigned> virtualHeldMask { 0 };                       // on-screen keyboard notes
    std::atomic<unsigned> midiHeldMask { 0 };                          // notes held on the MIDI input
    std::atomic<unsigned> learnMask { 0 };
    std::atomic<bool> learning { false };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void handleAsyncUpdate() override;

    pp::Engine engine;
    pp::FxChain fx;

    std::array<std::atomic<float>*, 12> notePtrs {}, centsPtrs {};
    std::array<bool, 128> heldNotes {};
    double traceAcc = 0.0;
    std::atomic<int> pendingLatency { 0 };
    int reportedLatency = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchPilotProcessor)
};
