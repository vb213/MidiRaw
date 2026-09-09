/*
  This file is part of the CQT analyzer plugin suite.
  Copyright (c) 2021

  This Plugin is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This Plugin is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this software.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "MidiTranslator.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <utility>

#include "Utilities/DebugLogger.h"

MidiTranslator::MidiTranslator(BufferQueue<float> &fifo,
                               const double sr,
                               const float initialThreshold,
                               const int initialChannel,
                               const float fMin,
                               const unsigned int binsPerSemitone)
    : Thread("MIDI Translator"),
      midiFifo(fifo),
      threshold(initialThreshold),
      midiChannel(juce::jlimit(1, 16, initialChannel)),
      minNote(0),
      maxNote(numMidiNotes - 1),
      eventFifo(maxEventsInQueue),
      forceAllNotesOff(false),
      maxOvertone(defaultMaxOvertone),
      sampleRate(sr)
{
    // Build the per-index frequency table. The CQT thread pushes its bins in
    // reversed order, so buffer index i holds CQT bin (numBins - 1 - i).
    // The CQT bins are geometrically spaced with ratio 2^(1/B), where
    // B = binsPerSemitone * 12.
    const int numBins = midiFifo.getBufferSize();
    binFrequencies.resize(static_cast<size_t>(numBins));
    for (int i = 0; i < numBins; ++i)
    {
        const int bin = numBins - 1 - i;
        binFrequencies[static_cast<size_t>(i)] =
            static_cast<float>(fMin * std::pow(2.0, bin / (binsPerSemitone * 12.0)));
    }

    // Scratch buffer for popping spectra.
    poppedBuffer.resize(static_cast<size_t>(numBins));

    // One active-flag per MIDI note. The array members are left indeterminate on
    // construction, so explicitly set every flag to the "off" state.
    for (auto &flag : noteActive)
        flag.store(false);

    // Pre-allocate the event-ring slots.
    eventBuffer.resize(static_cast<size_t>(maxEventsInQueue));
    for (auto *&slot : eventBuffer)
        slot = new juce::MidiMessage();

    startThread(6);
}

MidiTranslator::~MidiTranslator()
{
    signalThreadShouldExit();
    stopThread(500);

    for (auto *slot : eventBuffer)
        delete slot;
}

//==============================================================================
void MidiTranslator::run()
{
    while (!threadShouldExit())
    {
        if (midiFifo.dataAvailable())
        {
            // A spectrum frame is available - analyse it. If the producer has been
            // restarted, send note-offs for every held note first.
            if (forceAllNotesOff.exchange(false))
                sendAllNotesOff();

            if (midiFifo.pop(poppedBuffer.data()))
                processSpectrum(poppedBuffer.data(), static_cast<int>(poppedBuffer.size()));
        }
        else
        {
            // No data right now - poll periodically. The hopsize cadence of the
            // CQT thread is far higher than this, so 5 ms is more than enough.
            wait(5);
        }
    }
}

//==============================================================================
void MidiTranslator::processSpectrum(const float *spectrum, int numBins)
{
    // One bit per MIDI note indicating whether ANY of its bins is above threshold
    // in this frame.
    std::vector<bool> frameActive(static_cast<size_t>(numMidiNotes), false);
    std::vector<float> framePeak(static_cast<size_t>(numMidiNotes), 0.0f);

    const float currentThreshold = threshold.load();
    const int channel = midiChannel.load();
    const int lowNote = minNote.load();
    const int highNote = maxNote.load();

    // Map every bin that exceeds the threshold to a MIDI note, keeping the
    // strongest magnitude per note (used as velocity).
    for (int i = 0; i < numBins; ++i)
    {
        const float mag = spectrum[i];
        if (mag < currentThreshold)
            continue;

        const float freq = binFrequencies[static_cast<size_t>(i)];
        const int note = juce::roundToInt(69.0f + 12.0f * std::log2(freq / 440.0f));

        if (note < lowNote || note > highNote)
            continue;

        auto &peak = framePeak[static_cast<size_t>(note)];
        if (mag > peak)
            peak = mag;
        frameActive[static_cast<size_t>(note)] = true;
    }
    // do pitch detection with overtone algorithm
    frameActive = applyPitchDetectionFilter(frameActive);

    // Compare with the persistent per-note flags and emit the transitions.
    for (int note = lowNote; note <= highNote; ++note)
    {
        const bool nowActive = frameActive[static_cast<size_t>(note)];

        if (nowActive && !noteActive[static_cast<size_t>(note)].load())
        {
            // Note turned on.
            noteActive[static_cast<size_t>(note)].store(true);

            const float peak = framePeak[static_cast<size_t>(note)];
            const uint8_t velocity = static_cast<uint8_t>(juce::jlimit(1, 127,
                                                                       juce::roundToInt(std::min(1.0f, peak) * 127.0f)));

            enqueueMessage(juce::MidiMessage::noteOn(channel, note, velocity));
        }
        else if (!nowActive && noteActive[static_cast<size_t>(note)].load())
        {
            // Note turned off.
            noteActive[static_cast<size_t>(note)].store(false);
            enqueueMessage(juce::MidiMessage::noteOff(channel, note, 0.0f));
        }
    }
}

std::vector<bool> MidiTranslator::applyPitchDetectionFilter(std::vector<bool> activePitches)
{
    const int numOvertones = 2;
    std::vector<std::pair<int, float>> scores{};
    const int overtonePattern[numOvertones] = {12, 16};
    const float overtoneProfile[numOvertones] = {0.5, 0.5};

    for (int note = 0; note < activePitches.size(); note++)
    {
        float score = 0.0;
        for (int i = 0; i < numOvertones; i++)
        {
            int overtoneIndex = note + overtonePattern[i];
            if (overtoneIndex < activePitches.size() && activePitches[overtoneIndex])
            {
                score += overtoneProfile[i];
            }
        }
        scores.push_back({note, score});
    }

    // get entries with highest scores
    std::sort(scores.begin(), scores.end(),
              [](const auto &a, const auto &b)
              {
                  return a.second > b.second;
              });

    int k = 6;
    if (scores.size() < k)
    {
        k = scores.size();
    }
    std::vector<bool> filteredActivePitches(activePitches.size());
    for (int i = 0; i < k; i++)
    {
        filteredActivePitches[scores[i].first] = true;
    }
    return filteredActivePitches;
}

//==============================================================================
void MidiTranslator::sendAllNotesOff()
{
    const int channel = midiChannel.load();

    for (int note = 0; note < numMidiNotes; ++note)
    {
        if (noteActive[static_cast<size_t>(note)].load())
        {
            noteActive[static_cast<size_t>(note)].store(false);
            enqueueMessage(juce::MidiMessage::noteOff(channel, note, 0.0f));
        }
    }
}

//==============================================================================
void MidiTranslator::enqueueMessage(const juce::MidiMessage &message)
{
    auto scopedWrite = eventFifo.write(1);

    // Writing a single item always yields exactly one usable slot.
    jassert(scopedWrite.blockSize1 == 1 && scopedWrite.blockSize2 == 0);

    *eventBuffer[static_cast<size_t>(scopedWrite.startIndex1)] = message;
}

//==============================================================================
void MidiTranslator::drainMidiEvents(juce::MidiBuffer &midiMessages)
{
    while (eventFifo.getNumReady() > 0)
    {
        auto scopedRead = eventFifo.read(1);
        midiMessages.addEvent(*eventBuffer[static_cast<size_t>(scopedRead.startIndex1)], 0);
    }
}
