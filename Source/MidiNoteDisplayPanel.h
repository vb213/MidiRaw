/*
 ==============================================================================
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
 ==============================================================================
 */

#pragma once

#include <JuceHeader.h>
#include "MidiTranslator.h"

//==============================================================================
/**
 A text panel that displays the MIDI notes that the plugin is currently putting
 out (i.e. the notes currently flagged as active by the MidiTranslator).

 It owns a Timer that periodically polls the translator's active-notes list on
 the message thread (every per-note flag is an atomic, so this is thread-safe)
 and repaints whenever the set of held notes changes. The notes are rendered as
 human readable names (e.g. "C4", "F#3") in a small bordered panel.

 The panel takes a reference to the translator's Ptr so it always sees the
 current translator instance, even when the plugin rebuilds it. It keeps a
 retained copy while polling to guarantee the object isn't deleted mid-use.
 */
class MidiNoteDisplayPanel : public juce::Component, private juce::Timer
{
public:
    //==============================================================================
    /** Creates the panel and starts its polling timer.
        @param translator a reference to the plugin's current translator instance.
     */
    explicit MidiNoteDisplayPanel (MidiTranslator::Ptr& translator)
        : midiTranslator (translator)
    {
        startTimer (50);
    }

    //==============================================================================
    void paint (juce::Graphics& g) override
    {
        juce::Rectangle<int> area = getLocalBounds();

        // Panel background + thin accent border (matches the DebugPanel styling)
        const juce::Colour panelColour (0xFF2D2D2D);
        const juce::Colour accentColour (0xFF4FFF00);

        g.setColour (panelColour.withAlpha (0.92f));
        g.fillRect (area);
        g.setColour (accentColour.withAlpha (0.5f));
        g.drawRect (area, 1);

        // Header line
        g.setColour (accentColour);
        g.setFont (juce::Font (12.0f).boldened());
        g.drawText ("Active MIDI Notes", area.removeFromTop (18),
                    juce::Justification::centredLeft, true);
        area.removeFromTop (2);

        // Body: one note name per line.
        if (activeNotes.isEmpty())
        {
            g.setColour (juce::Colours::grey);
            g.setFont (juce::Font (12.0f));
            g.drawText ("-", area, juce::Justification::centredLeft, true);
            return;
        }

        g.setColour (juce::Colours::lightgrey);

        for (const auto note : activeNotes)
        {
            const juce::String noteName = juce::MidiMessage::getMidiNoteName (note, true, true, 4);
            g.drawText (noteName, area.removeFromTop (lineHeight),
                        juce::Justification::centredLeft, true);
        }
    }

private:
    //==============================================================================
    void timerCallback() override
    {
        // Keep a retained copy while polling so the translator can't be deleted
        // out from under us if it is being rebuilt on another thread.
        auto retained = midiTranslator;
        if (retained == nullptr)
            return;

        auto newActiveNotes = retained->getActiveNotes();

        // Only repaint when the set of active notes actually changed.
        if (newActiveNotes != activeNotes)
        {
            activeNotes = std::move (newActiveNotes);
            repaint();
        }
    }

    //==============================================================================
    static constexpr int lineHeight = 16;

    // Reference to the plugin's current translator instance.
    MidiTranslator::Ptr& midiTranslator;

    // Cached list of active note numbers, displayed as text.
    juce::Array<int> activeNotes;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiNoteDisplayPanel)
};
