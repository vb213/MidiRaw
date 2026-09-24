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

#include "CQTVisualizer.h"
#include "Utilities/Parula.h"

#include <cmath>

CQTVisualizer::CQTVisualizer(CQTThread::Ptr &cqtThread, juce::AudioProcessorValueTreeState &vts, MidiTranslator::Ptr &midiTranslatorRef)
    : cqt(cqtThread), midiTranslator(midiTranslatorRef), fMinParam(nullptr), bPerOctParam(nullptr)
{

    dBScale = bool(*vts.getRawParameterValue("dBScale"));
    dynamicRange = *vts.getRawParameterValue("dynamicRange");

    // Parameters that define the CQT bin grid. Raw pointers stay valid for the
    // lifetime of the parameter tree (i.e. for the lifetime of this component).
    fMinParam = vts.getRawParameterValue("fMin");
    bPerOctParam = vts.getRawParameterValue("bPerOct");

    startTimer(20);
}

void CQTVisualizer::paint(juce::Graphics &g)
{
    updateData();

    auto bounds = getLocalBounds();
    const float prop = 1.0f - static_cast<float>(imageOffset) / image.getWidth();

    const int mid = juce::roundToInt(bounds.getWidth() * prop);

    g.drawImage(image, 0, 0, mid, bounds.getHeight(),
                imageOffset, 0, image.getWidth() - imageOffset, image.getHeight());

    g.drawImage(image, mid, 0, bounds.getWidth() - mid, bounds.getHeight(),
                0, 0, imageOffset, image.getHeight());

    // Note overlay scrolls exactly like the spectrum, drawn on top of it. The
    // overlay uses per-pixel alpha, so transparent regions leave the spectrum
    // untouched while held notes show as light-blue rectangles at 50% opacity.
    if (noteOverlay.isValid())
    {
        g.drawImage(noteOverlay, 0, 0, mid, bounds.getHeight(),
                    imageOffset, 0, noteOverlay.getWidth() - imageOffset, noteOverlay.getHeight());

        g.drawImage(noteOverlay, mid, 0, bounds.getWidth() - mid, bounds.getHeight(),
                    0, 0, imageOffset, noteOverlay.getHeight());
    }
}

void CQTVisualizer::timerCallback()
{
    auto retainedPtr = cqt;
    if (retainedPtr != nullptr && retainedPtr->getCqtFifo().dataAvailable())
        repaint();
}

void CQTVisualizer::reallocateImage()
{
    reallocateImage(image.getHeight());
}

void CQTVisualizer::reallocateImage(const int imageHeight)
{
    image = juce::Image(juce::Image::RGB, imageWidth, imageHeight, true);
    // Note overlay: ARGB so it can carry per-pixel alpha (light-blue rectangles
    // at 50% opacity) over a fully transparent background.
    noteOverlay = juce::Image(juce::Image::ARGB, imageWidth, imageHeight, true);
    DBG("IS THIS IMAGE REALLOCATION");
    imageOffset = 0;
    poppedData.resize(static_cast<unsigned int>(imageHeight));
}

void CQTVisualizer::drawNoteOverlayColumn(int column, const juce::Array<int> &activeNotes, float fMinValue, float binsPerOctave)
{
    if (column < 0 || column >= noteOverlay.getWidth())
        return;

    const int height = noteOverlay.getHeight();
    if (height <= 0 || fMinValue <= 0.0f || binsPerOctave <= 0.0f)
        return;

    // Start with a fully transparent column so released notes leave no trace in
    // it (previously written columns of their trail keep scrolling to the left).
    juce::Image::BitmapData overlayData(noteOverlay, juce::Image::BitmapData::readWrite);
    for (int y = 0; y < height; ++y)
        overlayData.setPixelColour(column, y, juce::Colours::transparentBlack);

    // The CQT bins are geometrically spaced with ratio 2^(1/binsPerOctave),
    // bin k has centre frequency fMin * 2^(k/binsPerOctave) - exactly the same
    // grid the MidiTranslator uses building its bin-frequency table. A MIDI
    // note therefore maps to CQT bin k = round(log2(f) * binsPerOctave) with
    // f the note's frequency f = 440 * 2^((note-69)/12).
    //
    // Image row h stores buffer index h = CQT bin (height-1-k), so row 0 is the
    // top (highest frequency) and row height-1 the bottom (fMin), matching the
    // spectrum and the frequency scale drawn next to it.
    const float log2fMin = std::log2(fMinValue);
    static const juce::Colour noteColour = juce::Colours::purple.withAlpha(1.0f);

    for (const auto note : activeNotes)
    {
        if (note < 0 || note > MidiTranslator::numMidiNotes - 1)
            continue;

        const float freq = 440.0f * std::pow(2.0f, static_cast<float>(note - 69) / 12.0f);
        const int bin = juce::roundToInt((std::log2(freq) - log2fMin) * binsPerOctave);

        const int row = height - 1 - bin;
        if (row < 0 || row >= height)
            continue;

        overlayData.setPixelColour(column, row, noteColour);
    }
}

void CQTVisualizer::updateData()
{
    auto retainedPtr = cqt;
    if (retainedPtr != nullptr)
    {
        auto &fifo = retainedPtr->getCqtFifo();

        if (image.getHeight() != fifo.getBufferSize())
            reallocateImage(fifo.getBufferSize());

        const int numColsAvailable = fifo.howMuchDataAvailable();

        // Snapshot of the notes the plugin is currently putting out, taken once
        // per update pass and applied to every column written in this pass.
        // Same thread-safe polling pattern as MidiNoteDisplayPanel; exact
        // time-sync precision with the CQT frames is not critical.
        auto retainedMidi = midiTranslator;
        juce::Array<int> activeNotes;
        if (retainedMidi != nullptr)
            activeNotes = retainedMidi->getActiveNotes();

        // Read the bin-grid parameters fresh so the note->bin mapping stays
        // consistent even while the CQT thread is being rebuilt.
        const float fMinValue = (fMinParam != nullptr) ? fMinParam->load() : 0.0f;
        const float binsPerOctave = (bPerOctParam != nullptr) ? bPerOctParam->load() : 12.0f;

        for (int i = 0; i < numColsAvailable; ++i)
        {
            fifo.pop(poppedData.data());
            const float kFactor = ((127) / dynamicRange);

            for (unsigned int h = 0; h < static_cast<unsigned int>(image.getHeight()); ++h)
            {
                float val;

                if (dBScale == false)
                {
                    val = poppedData[h] * 127;
                }
                else
                {
                    val = 20 * log10(poppedData[h]) * kFactor + 127;
                }
                const unsigned int colourIndex = static_cast<unsigned int>(juce::jlimit(0, 127, juce::roundToInt(val)));
                const auto colour = juce::Colour(parula[colourIndex][0], parula[colourIndex][1], parula[colourIndex][2]);
                image.setPixelAt(imageOffset, int(h), colour);
            }

            // Stamp the held notes into the overlay at the same horizontal
            // position as this spectrum column.
            drawNoteOverlayColumn(imageOffset, activeNotes, fMinValue, binsPerOctave);

            ++imageOffset;
            while (imageOffset > image.getWidth())
                imageOffset -= image.getWidth();
        }
    }
}
