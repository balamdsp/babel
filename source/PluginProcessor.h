#pragma once

#include <JuceHeader.h>
#include "Core/ScaleQuantizer.h"
#include "Core/MidiRecorder.h"

namespace BabelIds
{
    inline constexpr const char* ROOT_ID = "root";
    inline constexpr const char* SCALE_ID = "scale";
    inline constexpr const char* OCTAVE_ID = "octave";
    inline constexpr const char* ROW_OFFSET_ID = "row_offset";
    inline constexpr const char* SHOW_NON_SCALE_ID = "show_non_scale";
    inline constexpr const char* LAYOUT_ID = "layout";
    inline constexpr const char* GRID_COLS_ID = "grid_cols";
    inline constexpr const char* GRID_ROWS_ID = "grid_rows";
    inline constexpr const char* VELOCITY_ID = "velocity";
    inline constexpr const char* VEL_FROM_Y_ID = "vel_from_y";
    inline constexpr const char* LEGATO_ID = "legato";
    inline constexpr const char* CHANNEL_ID = "channel";
    inline constexpr const char* X_CC_ENABLED_ID = "x_cc_enabled";
    inline constexpr const char* X_CC_NUM_ID = "x_cc_num";
    inline constexpr const char* Y_CC_ENABLED_ID = "y_cc_enabled";
    inline constexpr const char* Y_CC_NUM_ID = "y_cc_num";
    inline constexpr const char* UI_SCALE_ID = "ui_scale";

    inline const juce::StringArray& rootChoices()
    {
        static const juce::StringArray c { "C", "C#", "D", "D#", "E", "F",
                                            "F#", "G", "G#", "A", "A#", "B" };
        return c;
    }
    inline juce::StringArray scaleChoices()
    {
        juce::StringArray c;
        for (auto& s : babel::scales())
            c.add (s.name);
        return c;
    }
    inline const juce::StringArray& rowOffsetChoices()
    {
        static const juce::StringArray c { "3rd", "4th", "5th", "6th", "7th", "Octave" };
        return c;
    }
    inline const juce::StringArray& layoutChoices()
    {
        static const juce::StringArray c { "Scale Steps", "Chromatic" };
        return c;
    }
    inline constexpr std::array<float, 6> ZOOM_PERCENTS { 75.0f, 100.0f, 125.0f,
                                                          150.0f, 200.0f, 300.0f };
    inline constexpr int UI_SCALE_DEFAULT = 1;
}

// UI pushes, processBlock drains.
struct BabelGesture
{
    enum class Type { NoteOn, NoteOff, PitchCCs, AllOff };
    Type type = Type::NoteOn;
    int note = -1;       // 0..127 for NoteOn
    int velocity = 100;  // 1..127
    int channel = 1;     // 1..16
    float xNorm = 0.5f;  // 0..1 across pad (for X CC)
    float yNorm = 0.5f;  // 0..1 bottom->top (for Y CC)
};

class BabelAudioProcessor : public juce::AudioProcessor
{
public:
    BabelAudioProcessor();
    ~BabelAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    using AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

    // Called from the message thread (PadGrid).
    void pushGesture (const BabelGesture& g);

    babel::GridConfig getGridConfig() const;
    int getVelocityForRow (int row, int rows) const;

    // Host-locked takes.
    void armMidiTake();
    void stopMidiTake();
    bool isRecording() const noexcept { return recording.load(); }
    bool isArmed() const noexcept { return recordArmed.load(); }
    bool hasMidiTake() const;
    bool writeMidiTake (const juce::File& file);

private:
    juce::AudioProcessorValueTreeState apvts;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::CriticalSection gestureLock;
    std::vector<BabelGesture> gestureQueue; // guarded by gestureLock

    std::atomic<bool> recordArmed { false };
    std::atomic<bool> recordStopRequested { false };
    std::atomic<bool> recording { false };
    babel::MidiTake liveTake; // audio thread only, while recording
    // PPQ unwrap state (audio thread only, while recording). Host ppq jumps
    // backwards on loop wraps / backward seeks; we accumulate an offset so the
    // stored ppq stays monotonic and the export lays passes end-to-end.
    double takePpqOffset = 0.0;
    double takeLastRawPpq = 0.0;
    bool takeHaveLastPpq = false;
    mutable juce::CriticalSection takeLock;
    babel::MidiTake finishedTake; // guarded by takeLock
    bool hasFinishedTake = false; // guarded by takeLock

    void updateTakeState (const juce::AudioPlayHead::PositionInfo* pos);
    void recordEmitted (const juce::MidiBuffer& midi, double unwrappedPpq);
    double unwrapRecordingPpq (double rawPpq, const juce::AudioPlayHead::PositionInfo* pos);
    void finalizeTake();

    // Audio-thread sounding state. Legato holds overlapping notes.
    static constexpr int maxHeldNotes = 16;
    std::vector<std::pair<int, int>> heldNotes; // (note, channel)
    int soundingNote = -1;
    int soundingChannel = 1;

    bool isLegato() const;
    void emitNoteOff (juce::MidiBuffer& midi);
    void emitAllOff (juce::MidiBuffer& midi);
    void emitNoteOn (juce::MidiBuffer& midi, int note, int velocity, int channel);
    void emitCCs (juce::MidiBuffer& midi, float xNorm, float yNorm);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BabelAudioProcessor)
};
