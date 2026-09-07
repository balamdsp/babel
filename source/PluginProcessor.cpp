#include "PluginProcessor.h"
#include "PluginEditor.h"

BabelAudioProcessor::BabelAudioProcessor()
    : AudioProcessor (BusesProperties()),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
}

BabelAudioProcessor::~BabelAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout
BabelAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterChoice>(
        BabelIds::ROOT_ID, "Root", BabelIds::rootChoices(), 0));
    params.push_back (std::make_unique<juce::AudioParameterChoice>(
        BabelIds::SCALE_ID, "Scale", BabelIds::scaleChoices(), 0));
    params.push_back (std::make_unique<juce::AudioParameterInt>(
        BabelIds::OCTAVE_ID, "Octave", -2, 4, 0));
    params.push_back (std::make_unique<juce::AudioParameterChoice>(
        BabelIds::ROW_OFFSET_ID, "Row Offset", BabelIds::rowOffsetChoices(), 2));
    params.push_back (std::make_unique<juce::AudioParameterBool>(
        BabelIds::SHOW_NON_SCALE_ID, "Show Non-Scale", true));
    params.push_back (std::make_unique<juce::AudioParameterChoice>(
        BabelIds::LAYOUT_ID, "Layout", BabelIds::layoutChoices(), 0));
    params.push_back (std::make_unique<juce::AudioParameterInt>(
        BabelIds::GRID_COLS_ID, "Grid Cols", 4, 16, 8));
    params.push_back (std::make_unique<juce::AudioParameterInt>(
        BabelIds::GRID_ROWS_ID, "Grid Rows", 4, 16, 8));
    params.push_back (std::make_unique<juce::AudioParameterInt>(
        BabelIds::VELOCITY_ID, "Velocity", 1, 127, 100));
    params.push_back (std::make_unique<juce::AudioParameterBool>(
        BabelIds::VEL_FROM_Y_ID, "Velocity From Y", false));
    params.push_back (std::make_unique<juce::AudioParameterBool>(
        BabelIds::LEGATO_ID, "Legato", false));
    params.push_back (std::make_unique<juce::AudioParameterInt>(
        BabelIds::CHANNEL_ID, "Channel", 1, 16, 1));
    params.push_back (std::make_unique<juce::AudioParameterBool>(
        BabelIds::X_CC_ENABLED_ID, "X CC Enabled", false));
    params.push_back (std::make_unique<juce::AudioParameterInt>(
        BabelIds::X_CC_NUM_ID, "X CC", 0, 127, 1));
    params.push_back (std::make_unique<juce::AudioParameterBool>(
        BabelIds::Y_CC_ENABLED_ID, "Y CC Enabled", false));
    params.push_back (std::make_unique<juce::AudioParameterInt>(
        BabelIds::Y_CC_NUM_ID, "Y CC", 0, 127, 2));

    juce::StringArray zoomChoices;
    for (const auto p : BabelIds::ZOOM_PERCENTS)
        zoomChoices.add (juce::String (p, 0) + "%");
    params.push_back (std::make_unique<juce::AudioParameterChoice>(
        BabelIds::UI_SCALE_ID, "UI Scale", zoomChoices, BabelIds::UI_SCALE_DEFAULT));

    return { params.begin(), params.end() };
}

void BabelAudioProcessor::prepareToPlay (double, int) {}
void BabelAudioProcessor::releaseResources()
{
    soundingNote = -1;
    heldNotes.clear();
}

bool BabelAudioProcessor::isBusesLayoutSupported (const BusesLayout&) const
{
    return true; // MIDI effect: no audio buses to validate
}

void BabelAudioProcessor::pushGesture (const BabelGesture& g)
{
    const juce::ScopedLock lock (gestureLock);
    gestureQueue.push_back (g);
}

void BabelAudioProcessor::armMidiTake()
{
    recordStopRequested.store (false);
    {
        const juce::ScopedLock lock (takeLock);
        hasFinishedTake = false;
        finishedTake = babel::MidiTake();
    }
    takePpqOffset = 0.0;
    takeHaveLastPpq = false;
    recordArmed.store (true);
}

void BabelAudioProcessor::stopMidiTake()
{
    recordArmed.store (false);
    recordStopRequested.store (true);
}

bool BabelAudioProcessor::hasMidiTake() const
{
    const juce::ScopedLock lock (takeLock);
    return hasFinishedTake && ! finishedTake.empty();
}

void BabelAudioProcessor::updateTakeState (const juce::AudioPlayHead::PositionInfo* pos)
{
    const bool playing = pos != nullptr && pos->getIsPlaying();
    const auto ppq = (pos != nullptr) ? pos->getPpqPosition() : juce::Optional<double>();

    if (recordStopRequested.load())
    {
        recordStopRequested.store (false);
        recordArmed.store (false);
        finalizeTake();
        return;
    }

    if (! recording.load())
    {
        if (recordArmed.load() && playing && ppq.hasValue())
        {
            liveTake = babel::MidiTake();
            liveTake.startPpq = *ppq;
            const auto bpm = (pos != nullptr) ? pos->getBpm() : juce::Optional<double>();
            if (bpm.hasValue() && *bpm > 0.0)
                liveTake.bpm = *bpm;
            if (pos != nullptr)
            {
                if (auto ts = pos->getTimeSignature(); ts.hasValue())
                {
                    liveTake.timeSigNum = ts->numerator;
                    liveTake.timeSigDen = ts->denominator;
                }
            }
            // Reset the loop/seek unwrap clock: first block defines the origin.
            takePpqOffset = 0.0;
            takeLastRawPpq = *ppq;
            takeHaveLastPpq = true;
            recordArmed.store (false);
            recording.store (true);
        }
        return;
    }

    if (! playing || ! ppq.hasValue())
        finalizeTake();
}

double BabelAudioProcessor::unwrapRecordingPpq (double rawPpq,
    const juce::AudioPlayHead::PositionInfo* pos)
{
    static constexpr double eps = 1.0e-6; // beats; ignores fp jitter only

    if (! takeHaveLastPpq)
    {
        takeHaveLastPpq = true;
        takeLastRawPpq = rawPpq;
        return rawPpq + takePpqOffset;
    }

    if (rawPpq < takeLastRawPpq - eps)
    {
        // Backward jump: host loop wrap or manual backward seek. Keep the
        // take linear so loop passes lay end-to-end instead of stacking.
        const double lastUnwrapped = takeLastRawPpq + takePpqOffset;
        double loopLen = 0.0;
        if (pos != nullptr && pos->getIsLooping())
        {
            if (auto lp = pos->getLoopPoints(); lp.hasValue())
            {
                const double len = lp->ppqEnd - lp->ppqStart;
                if (len > eps)
                    loopLen = len;
            }
        }
        if (loopLen > 0.0)
        {
            takePpqOffset += loopLen;
            // Not actually a loop wrap (e.g. manual seek while loop is on):
            // fall back to gapless continuity to stay monotonic.
            if (rawPpq + takePpqOffset < lastUnwrapped - eps)
                takePpqOffset = lastUnwrapped - rawPpq;
        }
        else
        {
            takePpqOffset += (takeLastRawPpq - rawPpq);
        }
    }

    takeLastRawPpq = rawPpq;
    return rawPpq + takePpqOffset;
}

void BabelAudioProcessor::recordEmitted (const juce::MidiBuffer& midi, double unwrappedPpq)
{
    for (const auto metadata : midi)
    {
        const auto msg = metadata.getMessage();
        babel::RecordedEvent e;
        e.ppq = unwrappedPpq;
        if (msg.isNoteOn())
        {
            e.isNoteOn = true;
            e.note = msg.getNoteNumber();
            e.channel = msg.getChannel();
            e.velocity = msg.getVelocity();
        }
        else if (msg.isNoteOff())
        {
            e.note = msg.getNoteNumber();
            e.channel = msg.getChannel();
        }
        else if (msg.isController())
        {
            e.isCC = true;
            e.channel = msg.getChannel();
            e.ccNum = msg.getControllerNumber();
            e.ccVal = msg.getControllerValue();
        }
        else
        {
            continue;
        }
        liveTake.events.push_back (e);
    }
}

void BabelAudioProcessor::finalizeTake()
{
    if (! recording.load() && liveTake.empty())
        return;
    liveTake.sortByTime();
    {
        const juce::ScopedLock lock (takeLock);
        finishedTake = liveTake;
        hasFinishedTake = ! finishedTake.empty();
    }
    liveTake = babel::MidiTake();
    takePpqOffset = 0.0;
    takeHaveLastPpq = false;
    recording.store (false);
}

bool BabelAudioProcessor::writeMidiTake (const juce::File& file)
{
    babel::MidiTake take;
    {
        const juce::ScopedLock lock (takeLock);
        if (! hasFinishedTake || finishedTake.empty())
            return false;
        take = finishedTake;
    }

    static constexpr int tpq = 960;
    juce::MidiMessageSequence seq;
    const int mpq = juce::roundToInt (60e6 / juce::jmax (1.0, take.bpm));
    seq.addEvent (juce::MidiMessage::tempoMetaEvent (mpq), 0);
    seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (take.timeSigNum, take.timeSigDen), 0);
    for (auto& e : take.events)
    {
        const double tick = (double) take.ticksFor (e.ppq, tpq);
        juce::MidiMessage msg;
        if (e.isCC)
            msg = juce::MidiMessage::controllerEvent (e.channel, e.ccNum, e.ccVal);
        else if (e.isNoteOn)
            msg = juce::MidiMessage::noteOn (e.channel, e.note, (juce::uint8) e.velocity);
        else
            msg = juce::MidiMessage::noteOff (e.channel, e.note);
        seq.addEvent (msg, tick);
    }
    seq.updateMatchedPairs();

    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote (tpq);
    midiFile.addTrack (seq);
    juce::FileOutputStream out (file);
    if (out.failedToOpen())
        return false;
    midiFile.writeTo (out);
    out.flush();
    return ! out.getStatus().failed();
}

babel::GridConfig BabelAudioProcessor::getGridConfig() const
{
    babel::GridConfig cfg;
    auto getChoice = [this] (const char* id, int fallback)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (id)))
            return p->getIndex();
        return fallback;
    };
    auto getInt = [this] (const char* id, int fallback)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterInt*> (apvts.getParameter (id)))
            return p->get();
        return fallback;
    };
    auto getBool = [this] (const char* id, bool fallback)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (id)))
            return p->get();
        return fallback;
    };
    cfg.root = juce::jlimit (0, 11, getChoice (BabelIds::ROOT_ID, 0));
    cfg.scaleIndex = juce::jlimit (0, (int) babel::scales().size() - 1,
                                   getChoice (BabelIds::SCALE_ID, 0));
    cfg.baseOctave = getInt (BabelIds::OCTAVE_ID, 0);
    cfg.rowOffsetIndex = juce::jlimit (0, 5, getChoice (BabelIds::ROW_OFFSET_ID, 2));
    cfg.cols = juce::jlimit (4, 16, getInt (BabelIds::GRID_COLS_ID, 8));
    cfg.rows = juce::jlimit (4, 16, getInt (BabelIds::GRID_ROWS_ID, 8));
    cfg.showNonScale = getBool (BabelIds::SHOW_NON_SCALE_ID, true);
    cfg.layoutMode = getChoice (BabelIds::LAYOUT_ID, 0) == 1
                     ? babel::LayoutMode::Chromatic
                     : babel::LayoutMode::ScaleSteps;
    return cfg;
}

int BabelAudioProcessor::getVelocityForRow (int row, int rows) const
{
    auto* base = dynamic_cast<juce::AudioParameterInt*> (apvts.getParameter (BabelIds::VELOCITY_ID));
    auto* fromY = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (BabelIds::VEL_FROM_Y_ID));
    const int v = base != nullptr ? base->get() : 100;
    if (fromY == nullptr || ! fromY->get() || rows <= 1)
        return juce::jlimit (1, 127, v);
    // Low rows softer.
    const float t = (float) row / (float) (rows - 1);
    return juce::jlimit (1, 127, juce::roundToInt ((0.4f + 0.6f * t) * (float) v));
}

bool BabelAudioProcessor::isLegato() const
{
    if (auto* p = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (BabelIds::LEGATO_ID)))
        return p->get();
    return false;
}

void BabelAudioProcessor::emitNoteOff (juce::MidiBuffer& midi)
{
    if (soundingNote >= 0)
    {
        midi.addEvent (juce::MidiMessage::noteOff (soundingChannel, soundingNote), 0);
        soundingNote = -1;
    }
}

void BabelAudioProcessor::emitAllOff (juce::MidiBuffer& midi)
{
    for (auto& [note, channel] : heldNotes)
        midi.addEvent (juce::MidiMessage::noteOff (channel, note), 0);
    heldNotes.clear();
    soundingNote = -1;
}

void BabelAudioProcessor::emitNoteOn (juce::MidiBuffer& midi, int note, int velocity, int channel)
{
    note = juce::jlimit (0, 127, note);
    velocity = juce::jlimit (1, 127, velocity);
    channel = juce::jlimit (1, 16, channel);
    if (isLegato())
    {
        for (auto it = heldNotes.begin(); it != heldNotes.end(); ++it)
        {
            if (it->first == note && it->second == channel)
            {
                midi.addEvent (juce::MidiMessage::noteOff (channel, note), 0);
                midi.addEvent (juce::MidiMessage::noteOn (channel, note,
                                                         (juce::uint8) velocity), 0);
                heldNotes.erase (it);
                heldNotes.emplace_back (note, channel);
                soundingNote = note;
                soundingChannel = channel;
                return;
            }
        }
        midi.addEvent (juce::MidiMessage::noteOn (channel, note,
                                                 (juce::uint8) velocity), 0);
        if ((int) heldNotes.size() >= maxHeldNotes)
        {
            auto [oldNote, oldChannel] = heldNotes.front();
            midi.addEvent (juce::MidiMessage::noteOff (oldChannel, oldNote), 0);
            heldNotes.erase (heldNotes.begin());
        }
        heldNotes.emplace_back (note, channel);
        soundingNote = note;
        soundingChannel = channel;
        return;
    }
    if (soundingNote >= 0 && (soundingNote != note || soundingChannel != channel))
        emitNoteOff (midi);
    if (soundingNote != note || soundingChannel != channel)
    {
        midi.addEvent (juce::MidiMessage::noteOn (channel, note,
                                                 (juce::uint8) velocity), 0);
        soundingNote = note;
        soundingChannel = channel;
    }
}

void BabelAudioProcessor::emitCCs (juce::MidiBuffer& midi, float xNorm, float yNorm)
{
    auto* xEn = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (BabelIds::X_CC_ENABLED_ID));
    auto* yEn = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (BabelIds::Y_CC_ENABLED_ID));
    auto* xNum = dynamic_cast<juce::AudioParameterInt*> (apvts.getParameter (BabelIds::X_CC_NUM_ID));
    auto* yNum = dynamic_cast<juce::AudioParameterInt*> (apvts.getParameter (BabelIds::Y_CC_NUM_ID));
    auto* ch = dynamic_cast<juce::AudioParameterInt*> (apvts.getParameter (BabelIds::CHANNEL_ID));
    const int channel = ch != nullptr ? ch->get() : 1;
    if (xEn != nullptr && xEn->get() && xNum != nullptr)
        midi.addEvent (juce::MidiMessage::controllerEvent (channel, xNum->get(),
            juce::jlimit (0, 127, juce::roundToInt (xNorm * 127.0f))), 0);
    if (yEn != nullptr && yEn->get() && yNum != nullptr)
        midi.addEvent (juce::MidiMessage::controllerEvent (channel, yNum->get(),
            juce::jlimit (0, 127, juce::roundToInt (yNorm * 127.0f))), 0);
}

void BabelAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    midiMessages.clear(); // MIDI generator: ignore input, own the output buffer

    std::vector<BabelGesture> gestures;
    {
        const juce::ScopedLock lock (gestureLock);
        gestures.swap (gestureQueue);
    }

    auto* ch = dynamic_cast<juce::AudioParameterInt*> (apvts.getParameter (BabelIds::CHANNEL_ID));
    const int channel = ch != nullptr ? juce::jlimit (1, 16, ch->get()) : 1;

    juce::AudioPlayHead::PositionInfo posInfo;
    const juce::AudioPlayHead::PositionInfo* posPtr = nullptr;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition(); pos.hasValue())
        {
            posInfo = *pos;
            posPtr = &posInfo;
        }
    }
    updateTakeState (posPtr);

    for (auto& g : gestures)
    {
        switch (g.type)
        {
            case BabelGesture::Type::NoteOn:
                emitNoteOn (midiMessages, g.note, g.velocity, channel);
                emitCCs (midiMessages, g.xNorm, g.yNorm);
                break;
            case BabelGesture::Type::PitchCCs:
                emitCCs (midiMessages, g.xNorm, g.yNorm);
                break;
            case BabelGesture::Type::NoteOff:
            case BabelGesture::Type::AllOff:
                if (isLegato() || ! heldNotes.empty())
                    emitAllOff (midiMessages);
                else
                    emitNoteOff (midiMessages);
                break;
        }
    }

    if (recording.load() && posPtr != nullptr)
    {
        if (auto ppq = posPtr->getPpqPosition(); ppq.hasValue())
        {
            // Advance the unwrap clock every block (even silent ones) so loop
            // wraps during silence still accumulate the offset.
            const double unwrapped = unwrapRecordingPpq (*ppq, posPtr);
            recordEmitted (midiMessages, unwrapped);
        }
    }
}

bool BabelAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* BabelAudioProcessor::createEditor()
{
    return new BabelAudioProcessorEditor (*this);
}

void BabelAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void BabelAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

const juce::String BabelAudioProcessor::getName() const { return JucePlugin_Name; }
bool BabelAudioProcessor::acceptsMidi() const { return false; }
bool BabelAudioProcessor::producesMidi() const { return true; }
bool BabelAudioProcessor::isMidiEffect() const { return true; }
double BabelAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int BabelAudioProcessor::getNumPrograms() { return 1; }
int BabelAudioProcessor::getCurrentProgram() { return 0; }
void BabelAudioProcessor::setCurrentProgram (int) {}
const juce::String BabelAudioProcessor::getProgramName (int) { return {}; }
void BabelAudioProcessor::changeProgramName (int, const juce::String&) {}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BabelAudioProcessor();
}
