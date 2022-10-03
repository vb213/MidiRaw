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

template <typename SampleType>
class BufferQueue
{
public:
    BufferQueue (const int samplesPerBuffer, const int numBuffers) : fifo (numBuffers), bufferSize (samplesPerBuffer)
    {
        jassert (samplesPerBuffer > 0);
        jassert (numBuffers > 0);

        buffers.resize (numBuffers);
        for (int i = 0; i < numBuffers; ++i)
            buffers[i].resize (samplesPerBuffer);

        fifo.setTotalSize (numBuffers);
    }

    const int getBufferSize() const
    {
        return bufferSize;
    }

    const bool spaceAvailable() const
    {
        return fifo.getFreeSpace() > 0;
    }

    const bool dataAvailable() const
    {
        return fifo.getNumReady() > 0;
    }

    const int howMuchDataAvailable() const
    {
        return fifo.getNumReady();
    }


    const bool push (const SampleType* dataToPush, int numSamples)
    {
        if (! spaceAvailable())
        {
            DBG ("Dropped samples!");
            return false;
        }

        auto scopedWrite = fifo.write (1);

        jassert (numSamples <= bufferSize);
        FloatVectorOperations::copy (buffers[scopedWrite.startIndex1].data(), dataToPush, jmin (bufferSize, numSamples));

        if (numSamples < bufferSize)
            FloatVectorOperations::clear (buffers[scopedWrite.startIndex1].data() + numSamples, bufferSize - numSamples);

        return true;
    }


    const bool pop (SampleType* outputBuffer)
    {
        if (! dataAvailable())
            return false;

        auto scopedRead = fifo.read (1);

        FloatVectorOperations::copy (outputBuffer, buffers[scopedRead.startIndex1].data(), bufferSize);

        return true;
    }

private:
    AbstractFifo fifo;
    std::vector<std::vector<SampleType>> buffers;
    const int bufferSize;
};
