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

#include "JuceHeader.h"
#include "PluginProcessor.h"

//Plugin Design Essentials
#include "../resources/lookAndFeel/IEM_LaF.h"
#include "../resources/customComponents/TitleBar.h"

//Custom Components
#include "../resources/customComponents/ReverseSlider.h"
#include "../resources/customComponents/SimpleLabel.h"
#include "CQTVisualizer.h"


typedef ReverseSlider::SliderAttachment SliderAttachment; // all ReverseSliders will make use of the parameters' valueToText() function
typedef juce::AudioProcessorValueTreeState::ComboBoxAttachment ComboBoxAttachment;
typedef juce::AudioProcessorValueTreeState::ButtonAttachment ButtonAttachment;

//==============================================================================
/**
*/
class CqtanalyzerAudioProcessorEditor  : public juce::AudioProcessorEditor, private juce::Timer, juce::Slider::Listener
{
public:
    CqtanalyzerAudioProcessorEditor (CqtanalyzerAudioProcessor&, juce::AudioProcessorValueTreeState&);
    ~CqtanalyzerAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;


    void timerCallback() override;
    void sliderValueChanged (juce::Slider *slider) override;
    void sliderDragStarted(juce::Slider *slider) override;
    void sliderDragEnded(juce::Slider *slider) override;
    
    CQTVisualizer& getVisualizerComponent () { return cqtVisualizer; }
private:
    // ====================== begin essentials ==================
    // lookAndFeel class with the IEM plug-in suite design
    LaF globalLaF;

    // stored references to the AudioProcessor and ValueTreeState holding all the parameters
    CqtanalyzerAudioProcessor& audioProcessor;
    juce::AudioProcessorValueTreeState& valueTreeState;
    void generateScale(const float nOctaves, const float nBPerOct, const float fMin);


    /* title and footer component
     title component can hold different widgets for in- and output:
        - NoIOWidget (if there's no need for an input or output widget)
        - AudioChannelsIOWidget<maxNumberOfChannels, isChoosable>
        - AmbisonicIOWidget<maxOrder>
        - DirectivitiyIOWidget
     */
    TitleBar<AudioChannelsIOWidget<2, false>, NoIOWidget> title;
    OSCFooter footer;
    // =============== end essentials ============

    // Attachments to create a connection between IOWidgets comboboxes
    // and the associated parameters
    CQTVisualizer cqtVisualizer;
    
    double samplerate;
    
    juce::GroupComponent gcTuning;
    
    juce::Slider slFMin, slNOctaves, slBPerOct, slGamma, slGain, slTuning, slDynamicRange, slDBScale, slTunerStatus;
    
    std::unique_ptr<SliderAttachment> slFMinAttachment, slNOctavesAttachment, slBPerOctAttachment, slGammaAttachment, slGainAttachment, slTuningAttachment, slDynamicRangeAttachment, slDBScaleAttachment, slTunerStatusAttachment;
    
    juce::Label lbDetuning;
    
    SimpleLabel lbFMin, lbNOctaves, lbBPerOct, lbGamma, lbGain, lbTuning, lbDynamicRange;
    juce::OwnedArray<SimpleLabel> lbFreq;
    
    juce::Rectangle<int> freqIdxArea;
    
    const int sliderSize = 70;
    const int labelOffset = 20;
    
    // For frequency labels of visualizer
    const int freqScaleWidth = 30;
    const int freqScaleHeight = 10;
    
    const float gammaTh = 10.0f;
    
    juce::FlexBox flexboxScale;
    juce::Array<juce::FlexItem> scItems;



    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CqtanalyzerAudioProcessorEditor)
};
