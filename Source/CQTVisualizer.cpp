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

#include "CQTVisualizer.h"
#include "Utilities/Parula.h"

CQTVisualizer::CQTVisualizer (CQTThread::Ptr& cqtThread, juce::AudioProcessorValueTreeState& vts) : cqt (cqtThread)
{
    
    dBScale = bool (*vts.getRawParameterValue ("dBScale"));
    dynamicRange = *vts.getRawParameterValue ("dynamicRange");
    
    startTimer (20);
}

void CQTVisualizer::paint (juce::Graphics& g)
{
    updateData();

    auto bounds = getLocalBounds();
    const float prop = 1.0f - static_cast<float> (imageOffset) / image.getWidth();

    const int mid = bounds.getWidth() * prop;

    g.drawImage (image, 0, 0, mid, bounds.getHeight(),
                 imageOffset, 0, image.getWidth() - imageOffset, image.getHeight());

    g.drawImage (image, mid, 0, bounds.getWidth() - mid, bounds.getHeight(),
    0, 0, imageOffset, image.getHeight());

}


void CQTVisualizer::timerCallback()
{
    auto retainedPtr = cqt;
    if (retainedPtr != nullptr && retainedPtr->getCqtFifo().dataAvailable())
        repaint();
}

void CQTVisualizer::reallocateImage()
{
    reallocateImage (image.getHeight());
}

void CQTVisualizer::reallocateImage (const int imageHeight)
{
    image = juce::Image (juce::Image::RGB, imageWidth, imageHeight, true);
    DBG("IS THIS IMAGE REALLOCATION");
    imageOffset = 0;
    poppedData.resize (imageHeight);
}

void CQTVisualizer::updateData()
{
    auto retainedPtr = cqt;
    if (retainedPtr != nullptr)
    {
        auto& fifo = retainedPtr->getCqtFifo();

        if (image.getHeight() != fifo.getBufferSize())
            reallocateImage (fifo.getBufferSize());

        const int numColsAvailable = fifo.howMuchDataAvailable();

        for (int i = 0; i < numColsAvailable; ++i)
        {
            fifo.pop (poppedData.data());
            const float kFactor = ((127)/dynamicRange);
            
            for (int h = 0; h < image.getHeight(); ++h)
            {
                float val;
                
                if (dBScale == false)
                {
                    val = poppedData[h] * 127;
                }
                else
                {
                    val = 20*log10 (poppedData[h]) * kFactor + 127;
                }
                const int colourIndex = juce::jlimit (0, 127, juce::roundToInt (val));
                const auto colour = juce::Colour (parula[colourIndex][0], parula[colourIndex][1], parula[colourIndex][2]);
                image.setPixelAt (imageOffset, h, colour);
            }

            ++imageOffset;
            while (imageOffset > image.getWidth())
                imageOffset -= image.getWidth();
        }
    }
}
