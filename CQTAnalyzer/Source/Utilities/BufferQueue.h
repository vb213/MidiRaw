

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
