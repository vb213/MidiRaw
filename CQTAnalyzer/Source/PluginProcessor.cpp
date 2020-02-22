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

#include "PluginProcessor.h"
#include "PluginEditor.h"


//==============================================================================
CqtanalyzerAudioProcessor::CqtanalyzerAudioProcessor()
: AudioProcessorBase (
                      #ifndef JucePlugin_PreferredChannelConfigurations
                      BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
                      .withInput ("Input",  AudioChannelSet::stereo(), true)
#endif
                      .withOutput ("Output", AudioChannelSet::stereo(), true)
#endif
                       ,
#endif
                       createParameterLayout())
{
    // get pointers to the parameters
    fMin = parameters.getRawParameterValue("fMin");
    nOctaves = parameters.getRawParameterValue("nOctaves");
    bPerOct = parameters.getRawParameterValue("bPerOct");
    gamma = parameters.getRawParameterValue("gamma");
    gain = parameters.getRawParameterValue("gain");
    tuningFreq = parameters.getRawParameterValue("tuningFreq");


    // add listeners to parameter changes
    parameters.addParameterListener("fMin", this);
    parameters.addParameterListener("nOctaves", this);
    parameters.addParameterListener("bPerOct", this);
    parameters.addParameterListener("gamma", this);
    parameters.addParameterListener("tuningFreq", this);
}

CqtanalyzerAudioProcessor::~CqtanalyzerAudioProcessor()
{
}

//==============================================================================
int CqtanalyzerAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int CqtanalyzerAudioProcessor::getCurrentProgram()
{
    return 0;
}

void CqtanalyzerAudioProcessor::setCurrentProgram (int index)
{
    ignoreUnused (index);
}

const String CqtanalyzerAudioProcessor::getProgramName (int index)
{
    ignoreUnused (index);
    return {};
}

void CqtanalyzerAudioProcessor::changeProgramName (int index, const String& newName)
{
    ignoreUnused (index, newName);
}

//==============================================================================
void CqtanalyzerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Initialize CQTThread
    cqt = new CQTThread (sampleRate, *fMin, *nOctaves, *bPerOct, *gamma, *tuningFreq);
}

void CqtanalyzerAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool CqtanalyzerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif


void CqtanalyzerAudioProcessor::processBlock (AudioSampleBuffer& buffer, MidiBuffer&)
{
    double sampleRate = getSampleRate();
       
    float gainLinear = pow (10, *gain/20.0);
    buffer.applyGain (gainLinear);
    
    // Here are the samples pushed into the buffer for QCT analysis
    // TODO: Currently only analyzing left channel --> conversion to momo could be useful
    auto retainedCqt = cqt;
    if (retainedCqt != nullptr)
        retainedCqt->pushSamples (buffer.getReadPointer (0), buffer.getNumSamples());

    buffer.applyGain (1.0/gainLinear);
    
    // TODO: Implement timer-class for a more elegant change of params
    if (paramChanged > 0)
    {
        ++paramChanged;
            
        if (paramChanged > 10){
            paramChanged = 0;
            cqt = new CQTThread (sampleRate, *fMin, *nOctaves, *bPerOct, *gamma, *tuningFreq);
            
            
            retainedCqt = cqt;
            if (retainedCqt != nullptr)
                retainedCqt->pushSamples (buffer.getReadPointer (0), buffer.getNumSamples());
            }
        }
}

//==============================================================================
bool CqtanalyzerAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

AudioProcessorEditor* CqtanalyzerAudioProcessor::createEditor()
{
    return new CqtanalyzerAudioProcessorEditor (*this, parameters);
}

//==============================================================================
void CqtanalyzerAudioProcessor::getStateInformation (MemoryBlock& destData)
{
  auto state = parameters.copyState();

  auto oscConfig = state.getOrCreateChildWithName ("OSCConfig", nullptr);
  oscConfig.copyPropertiesFrom (oscParameterInterface.getConfig(), nullptr);

  std::unique_ptr<XmlElement> xml (state.createXml());
  copyXmlToBinary (*xml, destData);
}


void CqtanalyzerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    std::unique_ptr<XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (parameters.state.getType()))
        {
            parameters.replaceState (ValueTree::fromXml (*xmlState));
            if (parameters.state.hasProperty ("OSCPort")) // legacy
            {
                oscParameterInterface.getOSCReceiver().connect (parameters.state.getProperty ("OSCPort", var (-1)));
                parameters.state.removeProperty ("OSCPort", nullptr);
            }

            auto oscConfig = parameters.state.getChildWithName ("OSCConfig");
            if (oscConfig.isValid())
                oscParameterInterface.setConfig (oscConfig);
        }
}

//==============================================================================
void CqtanalyzerAudioProcessor::parameterChanged (const String &parameterID, float newValue)
{
    DBG ("Parameter with ID " << parameterID << " has changed. New value: " << newValue);

    if (paramChanged == 0)
        paramChanged = 1;
}

void CqtanalyzerAudioProcessor::updateBuffers()
{
    DBG ("IOHelper:  input size: " << input.getSize());
    DBG ("IOHelper: output size: " << output.getSize());
}


//==============================================================================
std::vector<std::unique_ptr<RangedAudioParameter>> CqtanalyzerAudioProcessor::createParameterLayout()
{
    // add your audio parameters here
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    params.push_back (OSCParameterInterface::createParameterTheOldWay ("fMin", "Minimum Analysis Frequency ", "",
                                                       NormalisableRange<float> (55.0f, 300.0f, 0.1f), 110.0f,
                                                       [](float value) {return String (value);}, nullptr));
    
    params.push_back (OSCParameterInterface::createParameterTheOldWay ("nOctaves", "Number of Analyzed Octaves ", "",
                                                       NormalisableRange<float> (1.0f, 8.0f, 1.0f), 5.0f,
                                                       [](float value) {return String (value);}, nullptr));
    
    params.push_back (OSCParameterInterface::createParameterTheOldWay ("bPerOct", "Bins per Octave ", "",
                                                       NormalisableRange<float> (12.0f, 72.0f, 12.0f), 48.0f,
                                                       [](float value) {return String (value);}, nullptr));
    
    params.push_back (OSCParameterInterface::createParameterTheOldWay ("gamma", "Gamma (for better Time-Resolution at low frequencies) ", "",
                                                       NormalisableRange<float> (0.0f, 30.0f, 0.1f), 0.0f,
                                                       [](float value) {return String (value);}, nullptr));
    
    params.push_back (OSCParameterInterface::createParameterTheOldWay ("gain", "Gain for Visualization ", "",
                                                       NormalisableRange<float> (-30.0f, 30.0f, 0.1f), 0.0f,
                                                       [](float value) {return String (value);}, nullptr));
    
    params.push_back (OSCParameterInterface::createParameterTheOldWay ("tuningFreq", "Tuning Frequency ", "",
                                                       NormalisableRange<float> (432.0f, 448.0f, 1.0f), 440.0f,
                                                       [](float value) {return String (value);}, nullptr));
    
    return params;
}


//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CqtanalyzerAudioProcessor();
}
