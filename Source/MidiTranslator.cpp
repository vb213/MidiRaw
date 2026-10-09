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
                               const float initialThresholdBasenote,
                               const float initialThresholdHarmonic,
                               const int initialChannel,
                               const float fMin,
                               const unsigned int binsPerSemitone)
    : Thread("MIDI Translator"),
      midiFifo(fifo),
      threshold(initialThresholdBasenote),
      thresholdHarmonic(initialThresholdHarmonic),
      midiChannel(juce::jlimit(1, 16, initialChannel)),
      // The lowest analysed frequency (fMin, the frequency of CQT bin 0) maps
      // to the lowest MIDI note the spectrum can ever produce, so initialise
      // the active-note range there instead of MIDI note 0.
      minNote(juce::jlimit(0, numMidiNotes - 1,
                           juce::roundToInt(69.0f + 12.0f * std::log2(fMin / 440.0f)))),
      maxNote(numMidiNotes - 1),
      eventFifo(maxEventsInQueue),
      forceAllNotesOff(false),
      maxOvertone(defaultMaxOvertone),
      sampleRate(sr),
      spectrumLogger(std::make_unique<SpectrumLogger>())
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

    // Initial pitch-detection tuning. These are read by the worker thread but
    // written from the GUI thread, so they are initialised here before the
    // thread starts.
    scoreThreshold.store(0.5f);
    for (auto &weight : overtoneProfile)
        weight.store(0.2f);

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
    std::vector<bool> frameActiveBaseNote(static_cast<size_t>(numMidiNotes), false);
    std::vector<bool> frameActiveHarmonic(static_cast<size_t>(numMidiNotes), false);
    std::vector<float> framePeak(static_cast<size_t>(numMidiNotes), 0.0f);

    const float currentThresholdBaseNote = threshold.load();
    const float currentThresholdHarmonic = thresholdHarmonic.load();
    const int channel = midiChannel.load();
    const int lowNote = minNote.load();
    const int highNote = maxNote.load();
    debugPrint(std::to_string(minNote) + " - " + std::to_string(maxNote));
    int baseActiveCount = 0;
    int harmActiveCount = 0;

    // Map every bin that exceeds the threshold to a MIDI note, keeping the
    // strongest magnitude per note (used as velocity).
    for (int i = 0; i < numBins; ++i)
    {
        const float mag = spectrum[i];

        if (mag < currentThresholdHarmonic)
            continue;

        const float freq = binFrequencies[static_cast<size_t>(i)];
        const int note = juce::roundToInt(69.0f + 12.0f * std::log2(freq / 440.0f));

        if (note < lowNote || note > highNote)
            continue;

        auto &peak = framePeak[static_cast<size_t>(note)];
        if (mag > peak)
            peak = mag;

        if (mag < currentThresholdBaseNote)
        {
            frameActiveHarmonic[static_cast<size_t>(note)] = true;
            harmActiveCount++;
        }
        else
        {
            baseActiveCount++;
            frameActiveBaseNote[static_cast<size_t>(note)] = true;
        }
    }
    if (harmActiveCount > 0)
        debugPrint("Len base: " + std::to_string(baseActiveCount) + " --- Len harmonics " + std::to_string(harmActiveCount));
    // do pitch detection with overtone algorithm
    frameActiveBaseNote = applyPitchDetectionFilter(frameActiveBaseNote, frameActiveHarmonic);

    // Log the per-frame artifacts (post-filter base notes, harmonics and the raw
    // spectrum). Handed to a dedicated logging thread via an internal queue, so
    // this never blocks the processing loop.
    if (spectrumLogger != nullptr && isLogWorthy(spectrum, numBins))
        spectrumLogger->logFrame(frameActiveBaseNote, frameActiveHarmonic, spectrum, numBins);

    // Compare with the persistent per-note flags and emit the transitions.
    for (int note = lowNote; note <= highNote; ++note)
    {
        const bool nowActive = frameActiveBaseNote[static_cast<size_t>(note)];

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

bool MidiTranslator::isLogWorthy(const float *spectrum, int numBins)
{
    for (int i = 0; i < numBins; i++)
    {
        if (spectrum[i] > 0.0)
        {
            return true;
        }
    }
    return false;
}

std::vector<bool> MidiTranslator::applyPitchDetectionFilter(std::vector<bool> frameActiveBaseNote, std::vector<bool> frameActiveHarmonic)
{
    const int numOvertones = numOvertoneProfileEntries;
    std::vector<std::pair<int, float>> scores{};
    // const int overtonePattern[numOvertones] = {-12, 12, 19, 24, 28, 31, 34, 36, 38, 40};
    const int overtonePattern[numOvertones] = {-42, -38, -34, -28, -19, 19, 28, 34, 38, 42};

    // Read the GUI-tunable sound profile. The per-overtone weights are normalised
    // here so they always sum to one, regardless of the raw slider values.
    const float scoreThreshhold = scoreThreshold.load();
    float rawProfile[numOvertones];
    float profileSum = 0.0f;
    for (int i = 0; i < numOvertones; i++)
    {
        rawProfile[i] = overtoneProfile[static_cast<size_t>(i)].load();
        profileSum += rawProfile[i];
    }
    if (profileSum <= 0.0f)
    {
        // Avoid division by zero: fall back to equal weights.
        profileSum = static_cast<float>(numOvertones);
        for (int i = 0; i < numOvertones; i++)
            rawProfile[i] = 1.0f;
    }
    float overtoneProfile[numOvertones];
    for (int i = 0; i < numOvertones; i++)
        overtoneProfile[i] = rawProfile[i] / profileSum;

    for (int note = 0; note < frameActiveBaseNote.size(); note++)
    {
        if (!frameActiveBaseNote[note])
            continue;
        //  intended range of score: [0.0, 1.0]
        float score = 0.0;
        for (int i = 0; i < numOvertones; i++)
        {
            int overtoneIndex = note + overtonePattern[i];
            if (0 <= overtoneIndex && overtoneIndex < frameActiveHarmonic.size() && frameActiveHarmonic[overtoneIndex])
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
    std::vector<bool> filteredActivePitches(frameActiveBaseNote.size());
    for (int i = 0; i < k; i++)
    {
        if (scores[i].second > scoreThreshhold)
        {
            filteredActivePitches[scores[i].first] = true;
        }
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
juce::Array<int> MidiTranslator::getActiveNotes() const
{
    juce::Array<int> activeNotes;

    const int lowNote = minNote.load();
    const int highNote = maxNote.load();

    for (int note = lowNote; note <= highNote; ++note)
        if (noteActive[static_cast<size_t>(note)].load())
            activeNotes.add(note);

    return activeNotes;
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
