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

//==============================================================================
/**
 Globally accessible, thread-safe debug message logger.

 Any part of the plugin (audio thread, editor thread, worker threads, ...) can
 push a debug message to the logger:

     DebugLogger::getInstance().log ("some message");
     // or, more conveniently:
     debugPrint ("some message");

 UI components may subscribe as a Listener to be notified whenever new messages
 arrive. Notifications are delivered on the message (GUI) thread via an
 internal AsyncUpdater, so it is safe to call log() from any thread.

 The logger keeps a bounded list of the most recent messages.
 */
class DebugLogger : public juce::AsyncUpdater
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        /** Called on the message thread whenever messages are added or cleared. */
        virtual void debugMessagesChanged() = 0;
    };

    /** Returns the single, globally accessible instance. */
    static DebugLogger& getInstance()
    {
        static DebugLogger instance;
        return instance;
    }

    /** Appends a message to the log. Thread-safe. */
    void log (const juce::String& message)
    {
        {
            const juce::ScopedLock sl (lock);
            messages.add (message);

            // keep only the most recent messages
            const int excess = messages.size() - maxMessages;
            if (excess > 0)
                messages.removeRange (0, excess);
        }
        triggerAsyncUpdate();
    }

    /** Clears all stored messages. Thread-safe. */
    void clear()
    {
        {
            const juce::ScopedLock sl (lock);
            messages.clear();
        }
        triggerAsyncUpdate();
    }

    /** Returns a snapshot of the current messages (newest last). Thread-safe. */
    juce::StringArray getMessages() const
    {
        const juce::ScopedLock sl (lock);
        return messages;
    }

    /** Registers a listener to be notified (on the message thread) of changes. */
    void addListener (Listener* listener)
    {
        const juce::ScopedLock sl (lock);
        listeners.addIfNotAlreadyThere (listener);
    }

    /** Removes a previously registered listener. */
    void removeListener (Listener* listener)
    {
        const juce::ScopedLock sl (lock);
        listeners.removeAllInstancesOf (listener);
    }

    /** Maximum number of messages kept in the log. Older ones are dropped. */
    static constexpr int maxMessages = 200;

private:
    DebugLogger() = default;
    ~DebugLogger() override = default;

    // AsyncUpdater
    void handleAsyncUpdate() override
    {
        // notify a snapshot of the listeners so new ones added during
        // iteration are not an issue
        juce::Array<Listener*> listenersToNotify;
        {
            const juce::ScopedLock sl (lock);
            listenersToNotify = listeners;
        }

        for (auto* l : listenersToNotify)
            l->debugMessagesChanged();
    }

    mutable juce::CriticalSection lock;
    juce::StringArray messages;
    juce::Array<Listener*> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DebugLogger)
};


//==============================================================================
/** Convenience wrapper: logs a debug message via the global DebugLogger. */
inline void debugPrint (const juce::String& message)
{
    DebugLogger::getInstance().log (message);
}
