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

#pragma once

#include <JuceHeader.h>

#include "Utilities/BufferQueue.h"

#include <atomic>
#include <cmath>
#include <vector>
#include <array>

/**
  Translates the frequency spectrum produced by the CQT thread into MIDI notes.

  The translator owns an internal worker thread. It continuously reads the
  dedicated spectrum FIFO (filled by CQTThread::getMidiFifo()), maps every CQT
  bin with an energy above an adjustable threshold to the corresponding MIDI
  note, and keeps a boolean for each note indicating whether it is currently
  played. Whenever a note's state changes, the corresponding note-on / note-off
  message is enqueued and handed over to the audio thread by calling
  drainMidiEvents() from CqtanalyzerAudioProcessor::processBlock().

  A reference-counted class, so the plugin can hold a Ptr. Before using the
  queue member, keep a retained copy of the pointer, so the object won't be
  deleted while the worker thread is still alive.
 */
class MidiTranslator  :  public juce::ReferenceCountedObject, public juce::Thread
{
public:
    static constexpr int numMidiNotes = 128;
    static constexpr int maxEventsInQueue = 512;

    using Ptr = juce::ReferenceCountedObjectPtr<MidiTranslator>;

    /** Creates the translator and starts its thread.
        @param midiFifo the dedicated spectrum FIFO filled by the CQT thread.
        All CQT bins of one pushed buffer are interpreted as one spectrum frame.
        @param sampleRate the audio sample rate (used to timestamp events).
        @param threshold initial energy threshold above which a bin is "on".
        @param midiChannel initial MIDI output channel (1 .. 16).
        @param fMin the lowest analyzed frequency (frequency of CQT bin 0).
        @param binsPerSemitone the number of CQT bins per MIDI semitone
        (equal to CQTThread's B / 12). Used to derive the geometric bin spacing.
     */
    MidiTranslator (BufferQueue<float>& midiFifo,
                    const double sampleRate,
                    const float threshold,
                    const int midiChannel,
                    const float fMin,
                    const unsigned int binsPerSemitone);
    ~MidiTranslator() override;

    //==============================================================================
    /** Runtime-adjustable configuration. Can be called from any thread. */
    void setThreshold (float newThreshold)        { threshold.store (newThreshold); }
    float getThreshold() const                    { return threshold.load(); }

    void setMidiChannel (int newChannel)
    {
        const int clamped = juce::jlimit (1, 16, newChannel);
        midiChannel.store (clamped);
    }
    int getMidiChannel() const                    { return midiChannel.load(); }

    /** Optional active-note range limiting (inclusive, 0 .. 127). */
    void setMidiNoteRange (int newMinNote, int newMaxNote)
    {
        minNote.store (juce::jlimit (0, numMidiNotes - 1, newMinNote));
        maxNote.store (juce::jlimit (0, numMidiNotes - 1, newMaxNote));
    }
    int getMinMidiNote() const                    { return minNote.load(); }
    int getMaxMidiNote() const                    { return maxNote.load(); }

    //==============================================================================
    /** Called from the audio thread (processBlock). Drains all pending note
        messages into the given MidiBuffer so they are sent out of the plugin.
     */
    void drainMidiEvents (juce::MidiBuffer& midiMessages);

    /** Let the worker loop know that the spectrum producer has (re)started, so
        any notes that are still flagged as active are send a note-off. */
    void reset() { forceAllNotesOff = true; }

private:
    //==============================================================================
    void run() override;

    /** Turns a popped CQT buffer into note-on / note-off events. */
    void processSpectrum (const float* spectrum, int numBins);

    /** Enqueues a MIDI message for delivery on the audio thread. */
    void enqueueMessage (const juce::MidiMessage& message);

    /** Sends note-offs for every note that is currently flagged as active. */
    void sendAllNotesOff();

    //==============================================================================
    BufferQueue<float>& midiFifo;

    // Frequency of the spectrum bin at popped-buffer index i. The CQT thread
    // pushes its bins in reversed order, so this vector is pre-reversed from the
    // CQT thread's internal bin-frequency table.
    std::vector<float> binFrequencies;

    // Scratch buffer for popping spectra from midiFifo.
    std::vector<float> poppedBuffer;

    // Per-note state: is this note currently "played"?
    std::array<std::atomic<bool>, numMidiNotes> noteActive;

    // Config, adjustable from the GUI / message thread while run() reads them.
    std::atomic<float> threshold;
    std::atomic<int> midiChannel;
    std::atomic<int> minNote;
    std::atomic<int> maxNote;

    // Event ring buffer handed to the audio thread on demand.
    juce::AbstractFifo eventFifo;
    std::vector<juce::MidiMessage*> eventBuffer;

    std::atomic<bool> forceAllNotesOff;

    double sampleRate;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiTranslator)
};
