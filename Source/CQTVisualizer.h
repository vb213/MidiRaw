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
#include "JuceHeader.h"
#include "CQTThread.h"

class CQTVisualizer : public juce::Component, private juce::Timer
{
    static constexpr int imageWidth = 2000;

public:
    CQTVisualizer (CQTThread::Ptr& cqt, juce::AudioProcessorValueTreeState& vts);

    void paint (juce::Graphics& g) override;
    
    void setDBScale (float sc) { dBScale = bool (sc); }
    void setDynamicRange (float dr) { dynamicRange = dr; }
    
    void reallocateImage ();
private:
    void timerCallback() override;

    void reallocateImage (const int bufferSize);

    void updateData();


    CQTThread::Ptr& cqt;

    juce::Image image;
    int imageOffset;
    
    bool dBScale;
    float peakLevel;
    float dynamicRange;
    

    std::vector<float> poppedData;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CQTVisualizer)
};
