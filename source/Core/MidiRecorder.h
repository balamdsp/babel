#pragma once

// Host-locked MIDI take, ppq-stamped.

#include <algorithm>
#include <vector>

namespace babel
{

struct RecordedEvent
{
    double ppq = 0.0; // unwrapped monotonic host ppq (loop wraps / backward seeks
                      // are accumulated so takes stay linear end-to-end)
    bool isNoteOn = false;
    bool isCC = false;
    int note = 60;      // note on/off
    int channel = 1;    // 1..16
    int velocity = 100; // note on
    int ccNum = 1;      // CC
    int ccVal = 0;      // CC
};

struct MidiTake
{
    double startPpq = 0.0;
    double bpm = 120.0;
    int timeSigNum = 4;
    int timeSigDen = 4;
    std::vector<RecordedEvent> events;

    bool empty() const { return events.empty(); }

    long long ticksFor (double ppq, int ticksPerQuarter) const
    {
        const double dt = (ppq - startPpq) * (double) ticksPerQuarter;
        return (long long) (dt >= 0.0 ? dt + 0.5 : 0.0);
    }

    void sortByTime()
    {
        std::stable_sort (events.begin(), events.end(),
            [] (const RecordedEvent& a, const RecordedEvent& b)
            {
                if (a.ppq < b.ppq)
                    return true;
                if (b.ppq < a.ppq)
                    return false;
                return (! a.isNoteOn) && b.isNoteOn;
            });
    }
};

} // namespace babel
