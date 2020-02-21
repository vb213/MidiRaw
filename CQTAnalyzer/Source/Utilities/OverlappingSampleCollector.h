 /*
 ==============================================================================

 SampleCollector.h
 Created: 22 Oct 2018 8:48:18pm
 Author:  Daniel Rudrich

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

        buffer.resize (bufferSize);
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

                        const int overlapOffset = bufferSize - overlap; // where does the overlap start?
                        for (int i = 0; i < overlap; ++i) // move the last overlap samples to the beginning
                            buffer[i] = buffer[overlapOffset + i];

                        numCollected = overlap; // nice, we alreay have overlap many samples!

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

    Atomic<int> skippedSamples;

    size_t numCollected;

    enum State
    {
        waitingForFreeSpace,
        collecting
    } state { State::waitingForFreeSpace };
};
