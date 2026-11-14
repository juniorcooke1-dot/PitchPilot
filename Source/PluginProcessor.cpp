#include "PluginProcessor.h"
#include "PluginEditor.h"

using juce::ParameterID;

juce::AudioProcessorValueTreeState::ParameterLayout PitchPilotProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (const auto& d : pp::kFloats)
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            ParameterID { d.id, 1 }, d.name,
            juce::NormalisableRange<float> (d.lo, d.hi, d.step, d.skew), d.def,
            juce::AudioParameterFloatAttributes().withLabel (juce::String (d.unit).trim())));

    for (int i = 0; i < 12; ++i)
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            ParameterID { "cents" + juce::String (i), 1 }, juce::String (pp::kKeyNames[i]) + " Cents",
            juce::NormalisableRange<float> (-50.f, 50.f, 1.f), 0.f));

    auto toArray = [] (const char* const* names, int n)
    {
        juce::StringArray a;
        for (int i = 0; i < n; ++i) a.add (names[i]);
        return a;
    };

    layout.add (std::make_unique<juce::AudioParameterChoice> (ParameterID { "inputtype", 1 }, "Input Type", toArray (pp::kInputTypes, 4), 1));
    layout.add (std::make_unique<juce::AudioParameterChoice> (ParameterID { "key", 1 }, "Key", toArray (pp::kKeyNames, 12), 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (ParameterID { "scale", 1 }, "Scale", toArray (pp::kScaleNames, pp::kNumScales), 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (ParameterID { "vibshape", 1 }, "Vibrato Shape", toArray (pp::kVibShapes, 5), 0));

    for (int i = 0; i < 12; ++i)
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            ParameterID { "note" + juce::String (i), 1 }, juce::String (pp::kKeyNames[i]) + " Note", toArray (pp::kNoteStates, 3), 0));

    for (int i = 0; i < 5; ++i)
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            ParameterID { "slot" + juce::String (i), 1 }, "FX Slot " + juce::String (i + 1), toArray (pp::kStageNames, 5), i));
    layout.add (std::make_unique<juce::AudioParameterBool> (ParameterID { "fxauto", 1 }, "Auto FX Order", true));

    layout.add (std::make_unique<juce::AudioParameterBool> (ParameterID { "classic", 1 },   "Classic", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (ParameterID { "formant", 1 },   "Formant", true));
    layout.add (std::make_unique<juce::AudioParameterBool> (ParameterID { "hold", 1 },      "Hold", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (ParameterID { "ignorevib", 1 }, "Ignore Vibrato", true));
    layout.add (std::make_unique<juce::AudioParameterBool> (ParameterID { "midimode", 1 },  "Target Notes", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (ParameterID { "polish", 1 },    "Auto Polish", false));

    return layout;
}

//==============================================================================
PitchPilotProcessor::PitchPilotProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PitchPilotState", createLayout())
{
    for (auto& a : traceDet)    a.store (0.f);
    for (auto& a : traceOut)    a.store (0.f);
    for (auto& a : traceTarget) a.store (0.f);

    for (int i = 0; i < 12; ++i)
    {
        notePtrs[(size_t) i]  = apvts.getRawParameterValue ("note" + juce::String (i));
        centsPtrs[(size_t) i] = apvts.getRawParameterValue ("cents" + juce::String (i));
    }
}

PitchPilotProcessor::~PitchPilotProcessor()
{
    cancelPendingUpdate();
}

bool PitchPilotProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet();
}

void PitchPilotProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    engine.setInputType ((int) apvts.getRawParameterValue ("inputtype")->load());
    fx.prepare (sampleRate);
    heldNotes.fill (false);
    traceAcc = 0.0;
    reportedLatency = engine.latencySamples();
    setLatencySamples (reportedLatency);
}

void PitchPilotProcessor::handleAsyncUpdate()
{
    setLatencySamples (pendingLatency.load());
}

//==============================================================================
void PitchPilotProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || n == 0)
        return;

    // ---- MIDI: held notes (for Target Notes) and Learn Scale ----
    for (const auto metadata : midi)
    {
        const auto m = metadata.getMessage();
        if (m.isNoteOn())
        {
            heldNotes[(size_t) m.getNoteNumber()] = true;
            if (learning.load())
                learnMask.fetch_or (1u << (m.getNoteNumber() % 12));
        }
        else if (m.isNoteOff())
            heldNotes[(size_t) m.getNoteNumber()] = false;
        else if (m.isAllNotesOff() || m.isAllSoundOff())
            heldNotes.fill (false);
    }

    unsigned midiMask = 0;
    for (int pc = 0; pc < 12; ++pc)
        for (int k = pc; k < 128; k += 12)
            if (heldNotes[(size_t) k]) { midiMask |= 1u << pc; break; }
    midiHeldMask.store (midiMask);

    // ---- parameters -> engine settings ----
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    pp::Settings s;
    s.inputType       = (int) get ("inputtype");
    s.refA            = get ("detune");
    s.retuneMs        = get ("retune");
    s.flex            = get ("flex");
    s.humanize        = get ("humanize");
    s.naturalVibrato  = get ("natvib");
    s.throat          = get ("throat");
    s.tracking        = get ("tracking");
    s.transpose       = (int) std::lround (get ("transpose"));
    s.classic         = get ("classic") > 0.5f;
    s.formantPreserve = get ("formant") > 0.5f;
    s.hold            = get ("hold") > 0.5f;
    s.ignoreVibrato   = get ("ignorevib") > 0.5f;
    s.midiMode        = get ("midimode") > 0.5f;
    s.mix             = get ("mix") * 0.01f;
    s.outputGain      = juce::Decibels::decibelsToGain (get ("outgain"));
    s.vibShape        = (int) get ("vibshape");
    s.vibRate         = get ("vibrate");
    s.vibDelayMs      = get ("vibdelay");
    s.vibOnsetMs      = get ("vibonset");
    s.vibVariation    = get ("vibvar");
    s.vibPitchCents   = get ("vibpitch");
    s.vibAmpPct       = get ("vibamp");
    s.vibFormantPct   = get ("vibformant");
    for (size_t i = 0; i < 12; ++i)
    {
        s.notes[i]     = (int) notePtrs[i]->load();
        s.noteCents[i] = centsPtrs[i]->load();
    }
    s.midiHeldMask = midiMask | virtualHeldMask.load();

    pp::FxSettings f;
    f.lowCutHz   = get ("lowcut");
    f.bodyDb     = get ("body");
    f.presenceDb = get ("presence");
    f.airDb      = get ("air");
    f.compress   = get ("compress");
    f.drive      = get ("drive");
    f.bits       = get ("bits");
    f.downsample = get ("downsample");
    f.crushMix   = get ("crushmix");
    f.space      = get ("space");
    f.spaceSize  = get ("spacesize");
    f.polish     = get ("polish") > 0.5f;
    f.polishAmt  = get ("polishamt");
    static const char* const slotIds[5] = { "slot0", "slot1", "slot2", "slot3", "slot4" };
    if (get ("fxauto") < 0.5f)
        for (int i = 0; i < 5; ++i)
            f.order[(size_t) i] = (int) get (slotIds[i]);

    // ---- audio ----
    float* chans[2] = { buffer.getWritePointer (0), buffer.getWritePointer (1) };
    engine.process (chans, 2, n, s);
    fx.process (chans[0], chans[1], n, f);

    // ---- latency report (input type changes the analysis window) ----
    if (engine.latencySamples() != reportedLatency)
    {
        reportedLatency = engine.latencySamples();
        pendingLatency.store (reportedLatency);
        triggerAsyncUpdate();
    }

    // ---- pitch trace for the display: one point every ~10 ms ----
    const auto& st = engine.state();
    currentNote.store (st.voiced ? st.targetNote : -1);

    traceAcc += n;
    const double step = std::max (1.0, getSampleRate() * 0.01);
    while (traceAcc >= step)
    {
        traceAcc -= step;
        const int w = traceWrite.load();
        const size_t i = (size_t) (w % traceSize);
        traceDet[i].store (st.voiced ? st.detMidi : 0.f);
        traceOut[i].store (st.voiced ? st.outMidi : 0.f);
        traceTarget[i].store (st.voiced ? st.targetMidi : 0.f);
        traceWrite.store (w + 1);
    }
}

//==============================================================================
juce::AudioProcessorEditor* PitchPilotProcessor::createEditor()
{
    return new PitchPilotEditor (*this);
}

void PitchPilotProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void PitchPilotProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
float PitchPilotProcessor::getParamValue (const juce::String& id) const
{
    if (auto* p = apvts.getRawParameterValue (id))
        return p->load();
    return 0.f;
}

void PitchPilotProcessor::setParamValue (const juce::String& id, float realValue)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (realValue));
        p->endChangeGesture();
    }
}

void PitchPilotProcessor::setNoteState (int pitchClass, int state)
{
    setParamValue ("note" + juce::String (pitchClass), (float) state);
}

void PitchPilotProcessor::applyScale (int key, int scale)
{
    for (int pc = 0; pc < 12; ++pc)
        setNoteState (pc, pp::noteInScale (key, scale, pc) ? 0 : 1);
}

void PitchPilotProcessor::setNotesFromMask (unsigned mask)
{
    for (int pc = 0; pc < 12; ++pc)
        setNoteState (pc, ((mask >> pc) & 1u) ? 0 : 1);
}

void PitchPilotProcessor::applyPreset (const pp::PresetValue* values, std::size_t count)
{
    for (std::size_t i = 0; i < count; ++i)
        setParamValue (values[i].id, values[i].value);
}
