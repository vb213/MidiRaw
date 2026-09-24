/*
==============================================================================
This file is part of the IEM plug-in suite.
Author: Daniel Rudrich
Copyright (c) 2020 - Institute of Electronic Music and Acoustics (IEM)
https://iem.at

The IEM plug-in suite is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

The IEM plug-in suite is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this software.  If not, see <https://www.gnu.org/licenses/>.
==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include "CQTThread.h"
#include "MidiTranslator.h"

class CQTVisualizer : public juce::Component, private juce::Timer
{
    static constexpr int imageWidth = 2000;

public:
    CQTVisualizer (CQTThread::Ptr& cqt,
                   juce::AudioProcessorValueTreeState& vts,
                   MidiTranslator::Ptr& midiTranslator);

    void paint (juce::Graphics& g) override;
    
    void setDBScale (float sc) { dBScale = bool (sc); }
    void setDynamicRange (float dr) { dynamicRange = dr; }
    
    void reallocateImage ();
private:
    void timerCallback() override;

    void reallocateImage (const int bufferSize);

    void updateData();

    /** Writes the note-overlay column at the given horizontal pixel position.
        The column is first cleared to transparent, then every note that is
        currently held is stamped as a light-blue (50% opacity) one-bin-high
        rectangle at the row matching its frequency. */
    void drawNoteOverlayColumn (int column,
                                const juce::Array<int>& activeNotes,
                                float fMinValue,
                                float binsPerOctave);

    // The audio/CQT thread and the MIDI translator the visualizer renders.
    CQTThread::Ptr& cqt;

    // Reference to the plugin's current translator instance, so the visualizer
    // always polls the most recent one (same pattern as MidiNoteDisplayPanel).
    MidiTranslator::Ptr& midiTranslator;

    // Raw pointers to the GUI parameters that determine the CQT bin grid. They
    // are read fresh on every update, so the note->bin mapping stays valid even
    // while the CQT thread is being rebuilt.
    std::atomic<float>* fMinParam;
    std::atomic<float>* bPerOctParam;

    juce::Image image;
    juce::Image noteOverlay;
    int imageOffset;
    
    bool dBScale;
    float peakLevel;
    float dynamicRange;
    

    std::vector<float> poppedData;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CQTVisualizer)
};
