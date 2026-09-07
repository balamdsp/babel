#pragma once

#include <JuceHeader.h>
#include "../Core/ScaleQuantizer.h"

class BabelAudioProcessor;
struct BabelGesture;

// Touch grid, bottom-left origin. Down = note-on, drag = glide, up = off.
class PadGrid : public juce::Component
{
public:
    explicit PadGrid (BabelAudioProcessor* processor);
    ~PadGrid() override = default;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent&) override;

    // Last triggered note for status display (-1 = none).
    int getLastNote() const { return lastNote; }
    std::function<void()> onGesture;

private:
    // Returns {col, row} with row 0 = bottom. -1 pair when outside.
    juce::Point<int> cellAt (juce::Point<int> pos) const;
    juce::Rectangle<float> cellRect (int col, int rowBottom) const;
    void triggerCell (int col, int rowBottom, bool isDrag);
    void sendCCs (juce::Point<int> pos);

    BabelAudioProcessor* processor = nullptr;
    int activeCol = -1, activeRow = -1; // rowBottom origin
    int activeNote = -1;
    int lastNote = -1;
    float lastXNorm = 0.5f, lastYNorm = 0.5f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PadGrid)
};
