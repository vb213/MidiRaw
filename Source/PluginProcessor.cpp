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
    : AudioProcessorBase(
#ifndef JucePlugin_PreferredChannelConfigurations
          BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)
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
    dBScale = parameters.getRawParameterValue("dBScale");
    dynamicRange = parameters.getRawParameterValue("dynamicRange");
    midiThreshold = parameters.getRawParameterValue("midiThreshold");
    midiChannel = parameters.getRawParameterValue("midiChannel");

    // add listeners to parameter changes
    parameters.addParameterListener("fMin", this);
    parameters.addParameterListener("nOctaves", this);
    parameters.addParameterListener("bPerOct", this);
    parameters.addParameterListener("gamma", this);
    parameters.addParameterListener("midiThreshold", this);
    parameters.addParameterListener("midiChannel", this);
}

CqtanalyzerAudioProcessor::~CqtanalyzerAudioProcessor()
{
}

//==============================================================================
int CqtanalyzerAudioProcessor::getNumPrograms()
{
    return 1; // NB: some hosts don't cope very well if you tell them there are 0 programs,
              // so this should be at least 1, even if you're not really implementing programs.
}

int CqtanalyzerAudioProcessor::getCurrentProgram()
{
    return 0;
}

void CqtanalyzerAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String CqtanalyzerAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void CqtanalyzerAudioProcessor::changeProgramName(int index, const juce::String &newName)
{
    juce::ignoreUnused(index, newName);
}

//==============================================================================
void CqtanalyzerAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Initialize CQTThread
    cqt = new CQTThread(sampleRate, *fMin, *nOctaves, *bPerOct, *gamma);

    // Initialize the audio-to-MIDI translator on the CQT thread's dedicated MIDI
    // spectrum queue. Bin spacing (bins per semitone) derives from bPerOct.
    auto retainedCqt = cqt;
    midiTranslator = new MidiTranslator(retainedCqt->getMidiFifo(),
                                        sampleRate,
                                        *midiThreshold,
                                        static_cast<int>(*midiChannel),
                                        *fMin,
                                        static_cast<unsigned int>(*bPerOct / 12.0f));

    copyBuffer.setSize(numberOfInputChannels, samplesPerBlock);
    copyBuffer.clear();

    sr = sampleRate;
}

void CqtanalyzerAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool CqtanalyzerAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
#if JucePlugin_IsMidiEffect
    ignoreUnused(layouts);
    return true;
#else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
#if !JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}
#endif

void CqtanalyzerAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
{
    // Only restart CQT Thread wen params changed AND slider is not dragged anymore
    if ((CqtParamChanged == true) && (sliderDrag == false))
    {
        CqtParamChanged = false;
        startTimer(20); // CQT is resetted in timer to avoid dropouts
    }

    copyBuffer.clear();

    // Copy and add buffer from all channels for a 'mono' spectrogram
    for (int ii = 0; ii < numberOfInputChannels; ++ii)
        copyBuffer.addFrom(0, 0, buffer, ii, 0, buffer.getNumSamples());

    copyBuffer.applyGain(0, 0, copyBuffer.getNumSamples(), float(pow(10, *gain / 20.0) / numberOfInputChannels));

    // Here are the samples pushed into the buffer for QCT analysis
    auto retainedCqt = cqt;
    if (retainedCqt != nullptr)
        retainedCqt->pushSamples(copyBuffer.getReadPointer(0), copyBuffer.getNumSamples());

    // Hand over the accumulated note-on / note-off messages to the MIDI output.
    auto retainedMidi = midiTranslator;
    if (retainedMidi != nullptr)
        retainedMidi->drainMidiEvents(midiMessages);

    juce::String notes;
    for (const auto metadata : midiMessages)
    {
        const auto &message = metadata.getMessage();

        if (message.isNoteOn())
            notes += juce::MidiMessage::getMidiNoteName(
                         message.getNoteNumber(), true, true, 4) +
                     " ";
    }

    debugPrint("Notes: " + notes);
}

//==============================================================================
bool CqtanalyzerAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor *CqtanalyzerAudioProcessor::createEditor()
{
    return new CqtanalyzerAudioProcessorEditor(*this, parameters);
}

//==============================================================================
void CqtanalyzerAudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    auto state = parameters.copyState();

    auto oscConfig = state.getOrCreateChildWithName("OSCConfig", nullptr);
    oscConfig.copyPropertiesFrom(oscParameterInterface.getConfig(), nullptr);

    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void CqtanalyzerAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(parameters.state.getType()))
        {
            parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
            if (parameters.state.hasProperty("OSCPort")) // legacy
            {
                oscParameterInterface.getOSCReceiver().connect(parameters.state.getProperty("OSCPort", juce::var(-1)));
                parameters.state.removeProperty("OSCPort", nullptr);
            }

            auto oscConfig = parameters.state.getChildWithName("OSCConfig");
            if (oscConfig.isValid())
                oscParameterInterface.setConfig(oscConfig);
        }
}

//==============================================================================
void CqtanalyzerAudioProcessor::parameterChanged(const juce::String &parameterID, float newValue)
{
    DBG("Parameter with ID " << parameterID << " has changed. New value: " << newValue);

    auto retainedMidi = midiTranslator;

    if ((parameterID == "midiThreshold") && (retainedMidi != nullptr))
        retainedMidi->setThreshold(newValue);
    else if ((parameterID == "midiChannel") && (retainedMidi != nullptr))
        retainedMidi->setMidiChannel(static_cast<int>(newValue));
    else
        CqtParamChanged = true;
}

void CqtanalyzerAudioProcessor::updateBuffers()
{
    DBG("IOHelper:  input size: " << input.getSize());
    DBG("IOHelper: output size: " << output.getSize());
}

void CqtanalyzerAudioProcessor::timerCallback()
{
    // The CQT thread is rebuilt, so a fresh reference to its dedicated MIDI FIFO
    // is needed for the translator as well.
    cqt = new CQTThread(getSamplerate(), *fMin, *nOctaves, *bPerOct, *gamma);

    auto retainedCqt = cqt;
    midiTranslator = new MidiTranslator(retainedCqt->getMidiFifo(),
                                        getSamplerate(),
                                        *midiThreshold,
                                        static_cast<int>(*midiChannel),
                                        *fMin,
                                        static_cast<unsigned int>(*bPerOct / 12.0f));

    stopTimer();
}

//==============================================================================
std::vector<std::unique_ptr<juce::RangedAudioParameter>> CqtanalyzerAudioProcessor::createParameterLayout()
{
    // add your audio parameters here
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(OSCParameterInterface::createParameterTheOldWay("fMin", "Minimum Analysis Frequency ", "Hz", juce::NormalisableRange<float>(55.0f, 300.0f, 0.1f), 110.0f, [](float value)
                                                                     { return juce::String(value); }, nullptr));

    params.push_back(OSCParameterInterface::createParameterTheOldWay("nOctaves", "Number of Analyzed Octaves ", "", juce::NormalisableRange<float>(1.0f, 8.0f, 1.0f), 5.0f, [](float value)
                                                                     { return juce::String(value); }, nullptr));

    params.push_back(OSCParameterInterface::createParameterTheOldWay("bPerOct", "Bins per Octave ", "", juce::NormalisableRange<float>(12.0f, 72.0f, 12.0f), 12.0f, [](float value)
                                                                     { return juce::String(value); }, nullptr));

    params.push_back(OSCParameterInterface::createParameterTheOldWay("gamma", "Gamma (for better Time-Resolution at low frequencies) ", "", juce::NormalisableRange<float>(0.0f, 30.0f, 0.1f), 0.0f, [](float value)
                                                                     { return juce::String(value); }, nullptr));

    params.push_back(OSCParameterInterface::createParameterTheOldWay("gain", "Gain for Visualization ", "dB", juce::NormalisableRange<float>(-35.0f, 35.0f, 0.1f), 0.0f, [](float value)
                                                                     { return juce::String(value); }, nullptr));

    params.push_back(OSCParameterInterface::createParameterTheOldWay("dBScale", "dB Scale ", "", juce::NormalisableRange<float>(0.0f, 1.0f, 1.0f), 0.0f, [](float value)
                                                                     {
                                                                           if (value >= 0.5f ) return "dB";
                                                                           else return "lin"; }, nullptr));

    params.push_back(OSCParameterInterface::createParameterTheOldWay("dynamicRange", "Dynamic Range", "dB", juce::NormalisableRange<float>(10.0f, 80.0f, 1.f), 40.0, [](float value)
                                                                     { return juce::String(value, 0); }, nullptr));

    params.push_back(OSCParameterInterface::createParameterTheOldWay("midiThreshold", "MIDI Threshold ", "", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.05f, [](float value)
                                                                     { return juce::String(value, 3); }, nullptr));

    params.push_back(OSCParameterInterface::createParameterTheOldWay("midiChannel", "MIDI Channel ", "", juce::NormalisableRange<float>(1.0f, 16.0f, 1.0f), 1.0f, [](float value)
                                                                     { return juce::String(static_cast<int>(value)); }, nullptr));

    return params;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new CqtanalyzerAudioProcessor();
}
