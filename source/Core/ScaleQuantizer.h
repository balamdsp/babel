#pragma once

// Grid -> MIDI. Scale Steps: columns walk degrees; Chromatic: semitones.
// Rows stack by row-offset interval (3rd/4th/5th/6th/7th/octave).

#include <array>
#include <string>
#include <vector>

namespace babel
{

struct ScaleDef
{
    const char* name;
    std::vector<int> intervals; // semitones from root, ascending, without octave
};

inline const std::vector<ScaleDef>& scales()
{
    static const std::vector<ScaleDef> s {
        { "Major",              { 0, 2, 4, 5, 7, 9, 11 } },
        { "Natural Minor",      { 0, 2, 3, 5, 7, 8, 10 } },
        { "Harmonic Minor",     { 0, 2, 3, 5, 7, 8, 11 } },
        { "Melodic Minor",      { 0, 2, 3, 5, 7, 9, 11 } },
        { "Dorian",             { 0, 2, 3, 5, 7, 9, 10 } },
        { "Phrygian",           { 0, 1, 3, 5, 7, 8, 10 } },
        { "Lydian",             { 0, 2, 4, 6, 7, 9, 11 } },
        { "Mixolydian",         { 0, 2, 4, 5, 7, 9, 10 } },
        { "Locrian",            { 0, 1, 3, 5, 6, 8, 10 } },
        { "Major Pentatonic",   { 0, 2, 4, 7, 9 } },
        { "Minor Pentatonic",   { 0, 3, 5, 7, 10 } },
        { "Blues",              { 0, 3, 5, 6, 7, 10 } },
        { "Whole Tone",         { 0, 2, 4, 6, 8, 10 } },
        { "Diminished WH",      { 0, 2, 3, 5, 6, 8, 9, 11 } },
        { "Diminished HW",      { 0, 1, 3, 4, 6, 7, 9, 10 } },
        { "Chromatic",          { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } },
        { "Harmonic Major",     { 0, 2, 4, 5, 7, 8, 11 } },
        { "Hungarian Minor",    { 0, 2, 3, 6, 7, 8, 11 } },
        { "Hijaz",              { 0, 1, 4, 5, 7, 8, 10 } },
        { "Hirajoshi",          { 0, 2, 3, 7, 8 } },
        { "Insen",              { 0, 1, 5, 7, 8 } },
        { "Neapolitan Minor",   { 0, 1, 3, 5, 7, 8, 11 } },
    };
    return s;
}

inline const std::array<const char*, 12> rootNames()
{
    return { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
}

// Row offsets in *scale steps* (not semitones) so exotic scales stay in key.
// 3rd=2 steps, 4th=3, 5th=4, 6th=5, 7th=6, octave=scale length.
inline int rowOffsetSteps (int rowOffsetIndex, int scaleLen)
{
    switch (rowOffsetIndex)
    {
        case 0: return 2;          // 3rd
        case 1: return 3;          // 4th
        case 2: return 4;          // 5th (default)
        case 3: return 5;          // 6th
        case 4: return 6;          // 7th
        case 5: return scaleLen;   // octave
        default: return 4;
    }
}

// Row offsets in semitones for Chromatic layout.
inline int rowOffsetSemitones (int rowOffsetIndex)
{
    switch (rowOffsetIndex)
    {
        case 0: return 4;    // 3rd
        case 1: return 5;    // 4th
        case 2: return 7;    // 5th (default)
        case 3: return 9;    // 6th
        case 4: return 11;   // 7th
        case 5: return 12;   // octave
        default: return 7;
    }
}

enum class LayoutMode { ScaleSteps = 0, Chromatic = 1 };

struct GridConfig
{
    int root = 0;            // 0..11 pitch class
    int scaleIndex = 0;      // index into scales()
    int baseOctave = 0;      // -2..+4, added to middle C area (60 + root)
    int rowOffsetIndex = 2;  // default 5th
    int cols = 8;
    int rows = 8;
    bool showNonScale = true;
    LayoutMode layoutMode = LayoutMode::ScaleSteps;
};

inline int midiForDegree (const ScaleDef& scale, int root, int baseOctave, int degree)
{
    const int len = (int) scale.intervals.size();
    if (len == 0)
        return -1;
    int oct = degree / len;
    int idx = degree % len;
    if (idx < 0) { idx += len; oct -= 1; }
    const int base = 60 + root + baseOctave * 12; // C4 = 60
    return base + oct * 12 + scale.intervals[(size_t) idx];
}

// Bottom-left origin. -1 when out of range.
inline int gridToMidi (const GridConfig& cfg, int col, int row)
{
    const auto& all = scales();
    if (cfg.scaleIndex < 0 || cfg.scaleIndex >= (int) all.size())
        return -1;
    if (col < 0 || col >= cfg.cols || row < 0 || row >= cfg.rows)
        return -1;
    const auto& scale = all[(size_t) cfg.scaleIndex];
    int midi = -1;
    if (cfg.layoutMode == LayoutMode::Chromatic)
    {
        midi = 60 + cfg.root + cfg.baseOctave * 12
             + col + row * rowOffsetSemitones (cfg.rowOffsetIndex);
    }
    else
    {
        const int rowSteps = rowOffsetSteps (cfg.rowOffsetIndex, (int) scale.intervals.size());
        const int degree = col + row * rowSteps;
        midi = midiForDegree (scale, cfg.root, cfg.baseOctave, degree);
    }
    if (midi < 0 || midi > 127)
        return -1;
    return midi;
}

// True when a MIDI note belongs to the scale (pitch-class membership).
inline bool isScaleNote (const ScaleDef& scale, int root, int midi)
{
    if (scale.intervals.empty())
        return false;
    const int pc = ((midi - root) % 12 + 12) % 12;
    for (int iv : scale.intervals)
        if (iv == pc)
            return true;
    return false;
}

inline bool isRootNote (int root, int midi)
{
    return ((midi - root) % 12 + 12) % 12 == 0;
}

inline bool isRootDegree (const ScaleDef& scale, int degree)
{
    const int len = (int) scale.intervals.size();
    if (len == 0) return false;
    int idx = degree % len;
    if (idx < 0) idx += len;
    return scale.intervals[(size_t) idx] == 0;
}

} // namespace babel
