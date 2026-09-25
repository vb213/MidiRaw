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
class MidiTranslator : public juce::ReferenceCountedObject, public juce::Thread
{
public:
    static constexpr int numMidiNotes = 128;
    static constexpr int maxEventsInQueue = 512;

    /** Number of strongest harmonic fits that are forwarded to the MIDI output.
        All other active notes are treated as overtones of some fundamental and
        are suppressed (i.e. they do not produce their own note-on / note-off). */
    static constexpr int maxDetectedPitches = 6;

    /** Default number of overtones examined per fundamental (the "N" in the
        harmonic-fit sum). The fundamental's own frequency is never counted; only
        the harmonics 2f .. (N+1)f contribute to its fit. */
    static constexpr int defaultMaxOvertone = 5;

    /** Number of per-overtone weights in the overtone "sound profile" tuned by
        the GUI. Each entry weights the strength of one harmonic of a candidate
        fundamental (see applyPitchDetectionFilter()). */
    static constexpr int numOvertoneProfileEntries = 10;

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
    MidiTranslator(BufferQueue<float> &midiFifo,
                   const double sampleRate,
                   const float threshold,
                   const float threshholdHarmonic,
                   const int midiChannel,
                   const float fMin,
                   const unsigned int binsPerSemitone);
    ~MidiTranslator() override;

    //==============================================================================
    /** Runtime-adjustable configuration. Can be called from any thread. */
    void setThreshold(float newThreshold) { threshold.store(newThreshold); }
    float getThreshold() const { return threshold.load(); }

    void setThresholdHarmonic(float newThreshold) { thresholdHarmonic.store(newThreshold); }
    float getThresholdHarmonic() const { return thresholdHarmonic.load(); }

    void setMidiChannel(int newChannel)
    {
        const int clamped = juce::jlimit(1, 16, newChannel);
        midiChannel.store(clamped);
    }
    int getMidiChannel() const { return midiChannel.load(); }

    /** Optional active-note range limiting (inclusive, 0 .. 127). */
    void setMidiNoteRange(int newMinNote, int newMaxNote)
    {
        minNote.store(juce::jlimit(0, numMidiNotes - 1, newMinNote));
        maxNote.store(juce::jlimit(0, numMidiNotes - 1, newMaxNote));
    }
    int getMinMidiNote() const { return minNote.load(); }
    int getMaxMidiNote() const { return maxNote.load(); }

    /** Sets the number of overtones N examined per fundamental in the harmonic
        fit. Only the harmonics 2f .. (N+1)f contribute; the fundamental's own
        frequency is never counted. Newly added weights default to 1.0.
        @param n the overtone order (must be >= 1). */
    void setMaxOvertone(int n);

    /** Replaces the overtone weight ("sound profile") used in the harmonic fit.
        The i-th entry is the weight w_i applied to the measured strength of the
        (i+1)-th harmonic of a candidate fundamental. The vector length is kept in
        sync with the number of overtones; entries are not normalised.
        @param weights the per-overtone weights, starting at the 2nd harmonic. */
    void setOvertoneWeights(const std::vector<float> &weights);

    int getMaxOvertone() const { return maxOvertone; }

    /** Sets the score threshold used by applyPitchDetectionFilter(). A candidate
        fundamental is only forwarded if its (normalised) harmonic-fit score
        exceeds this value.
        @param newScoreThreshold the threshold in [0.0, 1.0]. */
    void setScoreThreshold(float newScoreThreshold)
    {
        scoreThreshold.store(juce::jlimit(0.0f, 1.0f, newScoreThreshold));
    }
    float getScoreThreshold() const { return scoreThreshold.load(); }

    /** Sets one entry of the overtone "sound profile" used by
        applyPitchDetectionFilter(). The weights are normalised to sum to one at
        use time, so any non-negative value is meaningful.
        @param index which overtone (2nd .. (N+1)-th harmonic) to weight.
        @param value the raw weight in [0.0, 1.0]. */
    void setOvertoneProfileEntry(int index, float value)
    {
        if (index < 0 || index >= numOvertoneProfileEntries)
            return;
        overtoneProfile[static_cast<size_t>(index)].store(juce::jlimit(0.0f, 1.0f, value));
    }
    float getOvertoneProfileEntry(int index) const
    {
        if (index < 0 || index >= numOvertoneProfileEntries)
            return 0.0f;
        return overtoneProfile[static_cast<size_t>(index)].load();
    }

    //==============================================================================
    /** Returns the MIDI note numbers that are currently held (i.e. the notes
        whose note-on was sent but whose note-off has not been sent yet).

        This is exactly the set of notes that the plugin is currently putting
        out, and is safe to call from the GUI/message thread because every
        per-note active flag is an atomic. It is meant to be polled periodically
        by the UI to display the active notes as text.
     */
    juce::Array<int> getActiveNotes() const;

    //==============================================================================
    /** Called from the audio thread (processBlock). Drains all pending note
        messages into the given MidiBuffer so they are sent out of the plugin.
     */
    void drainMidiEvents(juce::MidiBuffer &midiMessages);

    /** Let the worker loop know that the spectrum producer has (re)started, so
        any notes that are still flagged as active are send a note-off. */
    void reset() { forceAllNotesOff = true; }

private:
    //==============================================================================
    void run() override;

    /** Turns a popped CQT buffer into note-on / note-off events. */
    void processSpectrum(const float *spectrum, int numBins);

    /** Computes the harmonic fit of a candidate fundamental using the measured
        strength of its overtones 2f .. (N+1)f. The fundamental's own strength is
        intentionally not part of the fit (it only gates whether the note is a
        candidate at all). Out-of-range overtones contribute a presence of zero.
        @param note the candidate's MIDI note.
        @param presence per-note measured strength (frame peaks).
        @param numOvertones the overtone order N (>= 1, 2f .. (N+1)f).
        @param weights the per-overtone weights, starting at the 2nd harmonic. */
    float computeHarmonicFit(int note,
                             const std::vector<float> &presence,
                             int numOvertones,
                             const std::vector<float> &weights) const;

    /** Enqueues a MIDI message for delivery on the audio thread. */
    void enqueueMessage(const juce::MidiMessage &message);

    /** Sends note-offs for every note that is currently flagged as active. */
    void sendAllNotesOff();

    std::vector<bool> applyPitchDetectionFilter(std::vector<bool> frameActiveBaseNote, std::vector<bool> frameActiveHarmonic);

    //==============================================================================
    BufferQueue<float> &midiFifo;

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
    std::atomic<float> thresholdHarmonic;
    std::atomic<int> midiChannel;
    std::atomic<int> minNote;
    std::atomic<int> maxNote;

    // Pitch-detection tuning, adjustable from the GUI / message thread while the
    // worker reads them in applyPitchDetectionFilter(). Both are atomic so they
    // can be written on the GUI thread and read on the MIDI worker thread without
    // a lock. The overtone weights are normalised to sum to one at use time.
    std::atomic<float> scoreThreshold;
    std::array<std::atomic<float>, numOvertoneProfileEntries> overtoneProfile;

    // Pitch-detection configuration (the "sound profile"). maxOvertone and the
    // overtoneWeights vector are mutated by the setters on the GUI thread and read
    // by processSpectrum() on the MIDI worker thread, so they are guarded by the
    // same lock. The worker takes a snapshot under the lock before using them.
    mutable juce::CriticalSection configLock;
    int maxOvertone;
    std::vector<float> overtoneWeights;

    // Event ring buffer handed to the audio thread on demand.
    juce::AbstractFifo eventFifo;
    std::vector<juce::MidiMessage *> eventBuffer;

    std::atomic<bool> forceAllNotesOff;

    double sampleRate;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiTranslator)
};
