/*
==============================================================================
This file is part of the IEM plug-in suite.
Author: Felix Holzmüller
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

CQTVisualizer::CQTVisualizer (CQTThread::Ptr& cqtThread) : cqt (cqtThread)
{
    startTimer (20);
}

void CQTVisualizer::paint (Graphics& g)
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

void CQTVisualizer::reallocateImage (const int imageHeight)
{
    image = Image (Image::RGB, imageWidth, imageHeight, true);
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

            for (int h = 0; h < image.getHeight(); ++h)
            {
                const float val = poppedData[h];
                const int colourIndex = jlimit (0, 127, roundToInt (val * 127));
                const auto colour = Colour (parula[colourIndex][0], parula[colourIndex][1], parula[colourIndex][2]);
                image.setPixelAt (imageOffset, h, colour);
            }

            ++imageOffset;
            while (imageOffset > image.getWidth())
                imageOffset -= image.getWidth();
        }
    }
}
