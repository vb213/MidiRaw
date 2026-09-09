/*
 ==============================================================================
 This file is part of the IEM plug-in suite.
 Author: Felix Holzmüller
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

#pragma once
#include <JuceHeader.h>
#include "../../Source/Utilities/DebugLogger.h"

//==============================================================================
/*
 Small panel component that displays the most recent debug messages from the
 globally accessible DebugLogger singleton. It subscribes as a Listener and
 repaints whenever new messages arrive (delivered on the message thread).

 Widgets: show/hide toggle + clear button.
 */
class DebugPanel   : public juce::Component, public DebugLogger::Listener
{
public:
    //==============================================================================
    DebugPanel()
    {
        DebugLogger::getInstance().addListener (this);

        addAndMakeVisible (showDebugButton);
        showDebugButton.setButtonText ("Show Debug");
        showDebugButton.setClickingTogglesState (true);
        showDebugButton.setColour (juce::TextButton::buttonColourId,
                                   juce::Colour (0xFF2D2D2D));
        showDebugButton.setColour (juce::TextButton::textColourOffId,
                                   juce::Colours::grey);
        showDebugButton.setColour (juce::TextButton::textColourOnId,
                                   juce::Colour (0xFF4FFF00));
        showDebugButton.setToggleState (true, juce::dontSendNotification);
        showDebugButton.onClick = [this]
        {
            repaint();
        };

        addAndMakeVisible (clearButton);
        clearButton.setButtonText ("Clear");
        clearButton.setColour (juce::TextButton::buttonColourId,
                               juce::Colour (0xFF2D2D2D));
        clearButton.setColour (juce::TextButton::textColourOffId,
                               juce::Colours::grey);
        clearButton.onClick = [this]
        {
            DebugLogger::getInstance().clear();
        };
    }

    ~DebugPanel() override
    {
        DebugLogger::getInstance().removeListener (this);
    }

    //==============================================================================
    void debugMessagesChanged() override
    {
        currentMessages = DebugLogger::getInstance().getMessages();

        // keep the message thread safe: snapshot already gathered above
        repaint();
    }

    //==============================================================================
    void paint (juce::Graphics& g) override
    {
        if (! showDebugButton.getToggleState())
            return; // panel hidden; nothing to draw

        juce::Rectangle<int> area = getLocalBounds();
        const juce::Colour panelColour (0xFF2D2D2D);
        const juce::Colour accentColour (0xFF4FFF00);

        g.setColour (panelColour.withAlpha (0.92f));
        g.fillRect (area);
        g.setColour (accentColour.withAlpha (0.5f));
        g.drawRect (area, 1);

        // header line
        g.setColour (accentColour);
        g.setFont (juce::Font (12.0f).boldened());
        g.drawText ("Debug Log", area.removeFromTop (18),
                    juce::Justification::centredLeft, true);
        area.removeFromTop (2);

        g.setColour (juce::Colours::lightgrey);

        // draw most recent messages, newest at the bottom, clipped to the panel
        const int n = currentMessages.size();
        const int rows = juce::jmax (1, area.getHeight() / lineHeight);
        const int startRow = juce::jmax (0, n - rows);
        for (int i = startRow; i < n; ++i)
        {
            const juce::Rectangle<int> lineArea (area.removeFromTop (lineHeight));
            g.setFont (juce::Font (11.0f).withTypefaceStyle ("Regular"));
            g.drawText (currentMessages[i], lineArea,
                        juce::Justification::left, true);
        }
    }

    //==============================================================================
    void resized() override
    {
        juce::Rectangle<int> area = getLocalBounds();

        // top row: toggle + clear buttons on the right
        juce::Rectangle<int> buttonRow = area.removeFromTop (18);
        clearButton.setBounds (buttonRow.removeFromRight (45).reduced (2));
        buttonRow.removeFromRight (6);
        showDebugButton.setBounds (buttonRow.removeFromRight (90).reduced (2));

        // remaining space is used as the scrolling message area in paint()
    }

private:
    //==============================================================================
    static constexpr int lineHeight = 14;

    juce::TextButton showDebugButton;
    juce::TextButton clearButton;
    juce::StringArray currentMessages;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DebugPanel)
};
