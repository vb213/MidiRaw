 /*
 ==============================================================================
 This file is part of the IEM plug-in suite.
 Author: Daniel Rudrich
 Copyright (c) 2018 - Institute of Electronic Music and Acoustics (IEM)
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
#include "BufferQueue.h"

template <typename SampleType>
class OverlappingSampleCollector
{
public:
    //==============================================================================
    OverlappingSampleCollector (BufferQueue<SampleType>& queueToUse, const int numberOfOverlapSamples)
    : bufferQueue (queueToUse), bufferSize (queueToUse.getBufferSize()), overlap (numberOfOverlapSamples)
    {
        jassert (overlap < bufferSize);

        buffer.resize (static_cast<unsigned int>(bufferSize));
        numCollected = 0;

        clearSkippedCounter();
    }


    void clearSkippedCounter()
    {
        skippedSamples = 0;
    }

    const int getAndClearSkippedCounter()
    {
        return skippedSamples.exchange (0);
    }

    const int getSkippedCounter()
    {
        return skippedSamples.get();
    }

    //==============================================================================
    void process (const SampleType* data, int numSamples)
    {
        int index = 0;

        while (index < numSamples) // there are input samples left to process
        {
            int samplesLeft = numSamples - index; // how many samples are left?

            if (state == State::waitingForFreeSpace) // in case we are currently waiting for free space...
            {
                if (bufferQueue.spaceAvailable()) // ... we check if the queue has some free space now
                    state = State::collecting; // it does, let's collect some samples
                else // apparently, there's no free space... too bad!
                {
                    skippedSamples += samplesLeft;
                    return; // bye bye
                }
            }


            if (state == State::collecting) // free space in the queue, let's collect some samples
            {
                while (index < numSamples) // as long as there are new samples...
                {
                    buffer[numCollected++] = data[index++]; // ...we write them into our buffer.

                    if (numCollected == buffer.size()) // once the buffer is full...
                    {
                        bufferQueue.push (buffer.data(), static_cast<int> (buffer.size())); // ... copy it into the queue

                        const unsigned int overlapOffset = static_cast<unsigned int>(bufferSize - overlap); // where does the overlap start?
                        for (unsigned int i = 0; i < static_cast<unsigned int>(overlap); ++i) // move the last overlap samples to the beginning
                            buffer[i] = buffer[overlapOffset + i];

                        numCollected = static_cast<unsigned int>(overlap); // nice, we alreay have overlap many samples!

                        state = State::waitingForFreeSpace; // let's wait for new space
                        break;
                    }
                }
            }
        }
    }

private:
    //==============================================================================
    BufferQueue<SampleType>& bufferQueue;
    std::vector<SampleType> buffer;

    const int bufferSize;
    const int overlap;

    juce::Atomic<int> skippedSamples;

    size_t numCollected;

    enum class State
    {
        waitingForFreeSpace,
        collecting
    } state { State::waitingForFreeSpace };
};
