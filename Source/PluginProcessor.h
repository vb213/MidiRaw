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
#include "../resources/AudioProcessorBase.h"
#include "CQTThread.h"
#include "MidiTranslator.h"

#define ProcessorClass CqtanalyzerAudioProcessor

//==============================================================================
class CqtanalyzerAudioProcessor  :  public AudioProcessorBase<IOTypes::AudioChannels<2>, IOTypes::AudioChannels<2>>, private juce::Timer
{
public:
    constexpr static int numberOfInputChannels = 2;
    constexpr static int numberOfOutputChannels = 2;
    //==============================================================================
    CqtanalyzerAudioProcessor();
    ~CqtanalyzerAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    
    #ifndef JucePlugin_PreferredChannelConfigurations
     bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;


    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    void parameterChanged (const juce::String &parameterID, float newValue) override;
    void updateBuffers() override; // use this to implement a buffer update method
    void timerCallback() override;

    CQTThread::Ptr& getCQT() { return cqt; }
    double& getSamplerate() { return sr; }
    void setSliderDrag ( bool newStatus ) { sliderDrag = newStatus; }
    
    //======= Parameters ===========================================================
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> createParameterLayout();
    //==============================================================================


private:
    //==============================================================================
    // list of used audio parameters
    std::atomic<float>* fMin;
    std::atomic<float>* nOctaves;
    std::atomic<float>* bPerOct;
    std::atomic<float>* gamma;
    std::atomic<float>* gain;
    std::atomic<float>* tuningFreq;
    std::atomic<float>* dBScale;
    std::atomic<float>* dynamicRange;
    std::atomic<float>* tunerStatus;
    std::atomic<float>* midiThreshold;
    std::atomic<float>* midiChannel;
    
    double sr;
    bool sliderDrag = false;

    
    juce::AudioBuffer<float> copyBuffer;
    
    CQTThread::Ptr cqt;
    MidiTranslator::Ptr midiTranslator;
    
    bool CqtParamChanged = false;


    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CqtanalyzerAudioProcessor)
};
