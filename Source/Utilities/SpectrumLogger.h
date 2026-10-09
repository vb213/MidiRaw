/*
 ==============================================================================
 This file holds the SpectrumLogger used by the CQT analyzer plugin to asynchronously
 write the per-frame processing artifacts to a log file.
 Copyright (c) 2021
 ==============================================================================
 */

#pragma once

#include <JuceHeader.h>

#include <string>
#include <vector>

//==============================================================================
/**
    Asynchronously persists the three artifacts of a processed spectrum frame
    (active base notes, active harmonics and the raw spectrum) to a text file.

    The class implements a classic single-producer / single-consumer pattern:
      * The producer is the MIDI worker thread (MidiTranslator::processSpectrum()).
        It calls logFrame(...), which only formats the line and pushes it into an
        internal lock-free ring buffer. It never does any file I/O, so it never
        blocks the processing path.
      * The consumer is the dedicated thread owned by this class. It pops the
        queued lines one by one and appends them to the file.

    Each frame produces one line in the format:
        frameActiveBaseNote#frameActiveHarmonic#spectrum
    where the elements inside each of the single vectors are separated by commas.
    Booleans are written as 0 or 1, floats with 3 decimal places.

    Frames may be dropped if the logging queue runs full, so that the processing
    thread is never blocked behind a slow disk.
 */
class SpectrumLogger : private juce::Thread
{
public:
    /** Number of frames that can be queued before new ones are dropped. */
    static constexpr int maxEnqueuedFrames = 256;

    /** Hard-coded destination file, appended to on the logging thread. */
    static constexpr const char *logFilePath = "C:\\Users\\Valentin\\Documents\\CPP\\audio-to-midi\\fft\\cqt-analyzer\\testfiles\\testlogs\\testlog.txt";

    //==============================================================================
    /** Starts the logging thread.
        @param filePath the text file to append each logged line to. */
    explicit SpectrumLogger(const juce::String &filePath = juce::String(logFilePath))
        : juce::Thread("Spectrum Logger"),
          logFile(filePath),
          fifo(maxEnqueuedFrames)
    {
        // Pre-allocate one string slot per ring-buffer position so the producer
        // never allocates after construction.
        entries.resize(static_cast<size_t>(maxEnqueuedFrames));
        startThread(4);
    }

    ~SpectrumLogger() override
    {
        signalThreadShouldExit();
        stopThread(2000);
    }

    //==============================================================================
    /** Queues a single frame's artifacts for logging (producer side).

        Must be called from exactly one producer thread. Formats the frame into a
        line and pushes it into the ring buffer. Never performs file I/O.
        If the queue is full the frame is dropped and the method returns false.
    */
    bool logFrame(const std::vector<bool> &frameActiveBaseNote,
                  const std::vector<bool> &frameActiveHarmonic,
                  const float *spectrum,
                  int numBins)
    {
        // Pre-format the line off the logging thread, then copy it into the ring.
        lineBuffer.clear();

        appendBoolVector(lineBuffer, frameActiveBaseNote);
        lineBuffer += '#';
        appendBoolVector(lineBuffer, frameActiveHarmonic);
        lineBuffer += '#';
        appendFloatArray(lineBuffer, spectrum, numBins);
        lineBuffer += '\n';

        const auto write = fifo.write(1);
        if (write.blockSize1 == 0)
            return false; // queue full - drop the frame to avoid blocking

        entries[static_cast<size_t>(write.startIndex1)] = lineBuffer;
        return true;
    }

private:
    //==============================================================================
    void run() override
    {
        while (!threadShouldExit())
        {
            if (fifo.getNumReady() > 0)
            {
                const auto read = fifo.read(1);
                writeLine(entries[static_cast<size_t>(read.startIndex1)]);
            }
            else
            {
                wait(5);
            }
        }
    }

    //==============================================================================
    static void appendBoolVector(juce::String &out, const std::vector<bool> &values)
    {
        for (auto it = values.begin(); it != values.end(); ++it)
        {
            if (it != values.begin())
                out += ',';

            // vector<bool> proxies convert to bool; write an explicit 0/1.
            out += (*it) ? '1' : '0';
        }
    }

    static void appendFloatArray(juce::String &out, const float *values, int numBins)
    {
        for (int i = 0; i < numBins; ++i)
        {
            if (i > 0)
                out += ',';

            out += juce::String(values[i], 3);
        }
    }

    void writeLine(const juce::String &line)
    {
        logFile.appendText(line, false, nullptr);
    }

    //==============================================================================
    juce::File logFile;
    juce::AbstractFifo fifo;

    // Reusable string slot rewritten and read on their respective threads. The
    // slot at index i is only ever written by the producer (via startIndex1) and
    // read by the consumer (via startIndex1), so no locking is required beyond
    // the fifo itself.
    std::vector<juce::String> entries;

    // Scratch buffer used solely by the producer while formatting a frame.
    juce::String lineBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumLogger)
};
