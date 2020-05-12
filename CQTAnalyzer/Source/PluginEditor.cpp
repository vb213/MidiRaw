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
CqtanalyzerAudioProcessorEditor::CqtanalyzerAudioProcessorEditor (CqtanalyzerAudioProcessor& p, AudioProcessorValueTreeState& vts)
: AudioProcessorEditor (&p), audioProcessor (p), valueTreeState (vts), footer (p.getOSCParameterInterface()), cqtVisualizer (audioProcessor.getCQT(), vts)
{
    // ============== BEGIN: essentials ======================
    // set GUI size and lookAndFeel
    //setSize(500, 300); // use this to create a fixed-size GUI
    setResizeLimits (650, 500, 1200, 1000); // use this to create a resizable GUI
    setLookAndFeel (&globalLaF);
    
    samplerate = p.getSampleRate();

    // make title and footer visible, and set the PluginName
    addAndMakeVisible (&title);
    title.setTitle (String ("CQT"), String ("Analyzer"));
    title.setFont (globalLaF.robotoBold, globalLaF.robotoLight);
    addAndMakeVisible (&footer);
    // ============= END: essentials ========================


    // create the connection between title component's comboBoxes and valueTreeState
    addAndMakeVisible (gcTuning);
    gcTuning.setText ("Tuner");
    
    
    // Sliders
    addAndMakeVisible (slFMin);
    slFMin.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slFMin.setTextBoxStyle (Slider::TextBoxBelow, false, 70, 20);
    slFMin.setColour (Slider::rotarySliderOutlineColourId, globalLaF.ClWidgetColours[0]);
    slFMin.setTextValueSuffix (" Hz");
    slFMinAttachment.reset (new SliderAttachment (valueTreeState, "fMin", slFMin));
    slFMin.addListener (this);
    
    addAndMakeVisible (slNOctaves);
    slNOctaves.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slNOctaves.setTextBoxStyle (Slider::TextBoxBelow, false, 70, 20);
    slNOctaves.setColour (Slider::rotarySliderOutlineColourId, globalLaF.ClWidgetColours[1]);
    slNOctavesAttachment.reset (new SliderAttachment (valueTreeState, "nOctaves", slNOctaves));
    slNOctaves.addListener (this);
    
    addAndMakeVisible (slBPerOct);
    slBPerOct.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slBPerOct.setTextBoxStyle (Slider::TextBoxBelow, false, 70, 20);
    slBPerOct.setColour (Slider::rotarySliderOutlineColourId, globalLaF.ClWidgetColours[1]);
    slBPerOctAttachment.reset (new SliderAttachment (valueTreeState, "bPerOct", slBPerOct));
    
    addAndMakeVisible (slGamma);
    slGamma.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slGamma.setTextBoxStyle (Slider::TextBoxBelow, false, 70, 20);
    slGamma.setColour (Slider::rotarySliderOutlineColourId, globalLaF.ClWidgetColours[2]);
    slGammaAttachment.reset (new SliderAttachment (valueTreeState, "gamma", slGamma));
    
    addAndMakeVisible (slGain);
    slGain.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slGain.setTextBoxStyle (Slider::TextBoxBelow, false, 70, 20);
    slGain.setColour (Slider::thumbColourId, globalLaF.ClWidgetColours[3]);
    slGainAttachment.reset (new SliderAttachment (valueTreeState, "gain", slGain));
    slGain.setTextValueSuffix (" dB");
    
    addAndMakeVisible (slTuning);
    slTuning.setSliderStyle (Slider::IncDecButtons);
    slTuning.setTextBoxStyle (Slider::TextBoxBelow, false, 70, 20);
    slTuning.setColour (Slider::thumbColourId, Colours::grey);
    slTuningAttachment.reset (new SliderAttachment (valueTreeState, "tuningFreq", slTuning));
    slTuning.setTextValueSuffix (" Hz");
    
    addAndMakeVisible (slDBScale);
    slDBScale.setSliderStyle (Slider::LinearHorizontal);
    slDBScale.setTextBoxStyle (Slider::TextBoxBelow, false, 70, 20);
    slDBScale.setColour (Slider::thumbColourId, Colours::grey);
    slDBScaleAttachment.reset (new SliderAttachment (valueTreeState, "dBScale", slDBScale));
    slDBScale.addListener (this);
    
    addChildComponent (slDynamicRange);
    slDynamicRange.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slDynamicRange.setTextBoxStyle (Slider::TextBoxBelow, false, 70, 20);
    slDynamicRange.setColour (Slider::thumbColourId, globalLaF.ClWidgetColours[3]);
    slDynamicRangeAttachment.reset (new SliderAttachment (valueTreeState, "dynamicRange", slDynamicRange));
    slDynamicRange.setTextValueSuffix (" dB");
    slDynamicRange.addListener (this);
    
    
    // Labels
    addAndMakeVisible (lbDetuning);
    lbDetuning.setJustificationType (Justification::centred);
    
    addAndMakeVisible (lbFMin);
    lbFMin.setJustification (Justification::centred);
    lbFMin.setText ("fMin", dontSendNotification);
    
    addAndMakeVisible (lbNOctaves);
    lbNOctaves.setJustification (Justification::centred);
    lbNOctaves.setText ("Octaves", dontSendNotification);
    
    addAndMakeVisible (lbBPerOct);
    lbBPerOct.setJustification (Justification::centred);
    lbBPerOct.setText ("Bins per Oct", dontSendNotification);
    
    addAndMakeVisible (lbGamma);
    lbGamma.setJustification (Justification::centred);
    lbGamma.setText ("Gamma", dontSendNotification);
    
    addAndMakeVisible (lbGain);
    lbGain.setJustification (Justification::centred);
    lbGain.setText ("Gain", dontSendNotification);
    
    addAndMakeVisible (lbTuning);
    lbTuning.setJustification (Justification::centred);
    lbTuning.setText ("", dontSendNotification);
    
    addChildComponent (lbDynamicRange);
    lbDynamicRange.setJustification (Justification::centred);
    lbDynamicRange.setText ("Range", dontSendNotification);
    
    // This is needed for correct scale when reopening UI
    generateScale (slNOctaves.getValue(), slBPerOct.getValue(), slFMin.getValue());
    
    // Visualizer
    addAndMakeVisible (cqtVisualizer);

    // start timer after everything is set up properly
    startTimer (25);
}

CqtanalyzerAudioProcessorEditor::~CqtanalyzerAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void CqtanalyzerAudioProcessorEditor::paint (Graphics& g)
{
    g.fillAll (globalLaF.ClBackground);
}

void CqtanalyzerAudioProcessorEditor::resized()
{
    // ============ BEGIN: header and footer ============
    const int leftRightMargin = 30;
    const int headerHeight = 60;
    const int footerHeight = 25;
    Rectangle<int> area (getLocalBounds());

    Rectangle<int> footerArea (area.removeFromBottom (footerHeight));
    footer.setBounds (footerArea);

    area.removeFromLeft (leftRightMargin);
    area.removeFromRight (leftRightMargin);
    Rectangle<int> headerArea = area.removeFromTop (headerHeight);
    title.setBounds (headerArea);
    area.removeFromTop (10);
    area.removeFromBottom (5);
    // =========== END: header and footer =================
    Rectangle<int> gcArea (sliderSize, 2 * sliderSize);
    gcTuning.setBounds (gcArea);
    
    // Sorting all Silders, Labels and Tuner-GroupComponent in flexboxes for responsive design
    FlexBox flexboxUI;
    
    Array<FlexItem> uiItems;
    Array<FlexItem> lbItems;
    
    flexboxUI.flexDirection = FlexBox::Direction::row;
    flexboxUI.flexWrap = FlexBox::Wrap::noWrap;
    flexboxUI.alignContent = FlexBox::AlignContent::flexEnd;
    flexboxUI.justifyContent = FlexBox::JustifyContent::spaceBetween;
    flexboxUI.alignItems = FlexBox::AlignItems::flexEnd;
    
    FlexBox flexboxLb = flexboxUI;
    
    uiItems.add (FlexItem (sliderSize, sliderSize, slFMin));
    uiItems.add (FlexItem (sliderSize, sliderSize, slNOctaves));
    uiItems.add (FlexItem (sliderSize, sliderSize, slBPerOct));
    uiItems.add (FlexItem (sliderSize, sliderSize, slGamma));
    uiItems.add (FlexItem (2 * sliderSize, sliderSize, gcTuning));
    flexboxUI.items = uiItems;
    
    lbItems.add (FlexItem (sliderSize, labelOffset, lbFMin));
    lbItems.add (FlexItem (sliderSize, labelOffset, lbNOctaves));
    lbItems.add (FlexItem (sliderSize, labelOffset, lbBPerOct));
    lbItems.add (FlexItem (sliderSize, labelOffset, lbGamma));
    lbItems.add (FlexItem (2 * sliderSize, labelOffset, lbTuning));
    flexboxLb.items = lbItems;

    flexboxLb.performLayout (area);
    area.removeFromBottom (labelOffset + 5);
    flexboxUI.performLayout (area);
    
    
    area.removeFromBottom (sliderSize + labelOffset + 5);

    // UI control
    Rectangle<int> GainArea = area.removeFromRight (sliderSize * 0.7f);
    slDBScale.setBounds(GainArea.removeFromTop(40));
    lbGain.setBounds (GainArea.removeFromBottom (labelOffset + 5));
    slGain.setBounds (GainArea.removeFromBottom (sliderSize));
    
    lbDynamicRange.setBounds (GainArea.removeFromBottom (labelOffset + 5));
    slDynamicRange.setBounds (GainArea.removeFromBottom (sliderSize));
    
    area.removeFromRight (10);
    
    
    // Labels for frequency scale
    flexboxScale.flexDirection = FlexBox::Direction::columnReverse;
    flexboxScale.flexWrap = FlexBox::Wrap::noWrap;
    flexboxScale.alignContent = FlexBox::AlignContent::flexEnd;
    flexboxScale.justifyContent = FlexBox::JustifyContent::spaceBetween;
    flexboxScale.alignItems = FlexBox::AlignItems::flexEnd;

    freqIdxArea = area.removeFromRight(freqScaleWidth);
    
    // necessary for resizeable window
    if (slNOctaves.getValue() > 0)
        generateScale (slNOctaves.getValue(), slBPerOct.getValue(), slFMin.getValue());

    area.removeFromRight(5);
    
    cqtVisualizer.setBounds (area);

}

void CqtanalyzerAudioProcessorEditor::timerCallback()
{
    // Update detuning-value
    auto& cqt = audioProcessor.getCQT();
    float detuning = 0.0f;

    if (cqt != nullptr)
        detuning = cqt->getTuning();
    else
        detuning = 0.0f;



    std::string detuningString;

    if (slBPerOct.getValue() < 36)
        detuningString = "NaN";
    else{
        std::stringstream s;
        
        if (round (detuning) > 0)
            s << "+" << std::to_string (int (round (detuning))) << " cent";
        else
            s << std::to_string (int (round (detuning))) << " cent";
        
        detuningString = s.str();
    }
    
    lbDetuning.setText (detuningString, dontSendNotification);
    
    
    // Components for tuning are defined here for bigger height
    auto currentGcArea = gcTuning.getBounds();
    currentGcArea.removeFromTop (25);
    currentGcArea.setHeight (currentGcArea.getHeight() + 20);
    
    slTuning.setBounds (currentGcArea.removeFromLeft (sliderSize - 5));
    lbDetuning.setBounds (currentGcArea.removeFromRight (sliderSize - 5));
    
    
    if ((slDBScale.getValue() >= 0.5f) && (slDynamicRange.isVisible() != true))
    {
        slDynamicRange.setVisible (true);
        lbDynamicRange.setVisible (true);
    }
    else if ((slDBScale.getValue() < 0.5f) && (slDynamicRange.isVisible() == true))
    {
        slDynamicRange.setVisible (false);
        lbDynamicRange.setVisible (false);
    }
}


void CqtanalyzerAudioProcessorEditor::sliderValueChanged (Slider *slider)
{
    if (slider == &slDynamicRange)
        cqtVisualizer.setDynamicRange ((float) slider->getValue());
    else if (slider == &slDBScale)
        cqtVisualizer.setDBScale ((float) slider->getValue());
    
    else if ((slider == &slNOctaves) || (slider == &slBPerOct) || (slider == &slFMin))
        generateScale (slNOctaves.getValue(), slBPerOct.getValue(), slFMin.getValue());
}


void CqtanalyzerAudioProcessorEditor::generateScale (const float nOctaves, const float nBPerOct, const float fMin)
{
    lbFreq.clear();
    scItems.clear();
    
    const float fMax = jmin (samplerate / 2 / exp2 (1.0/nBPerOct), fMin * pow (2, nOctaves));
    const int numLabels = roundToInt (std::floor (std::log2 (fMax / fMin))) + 1;
    
    for (int ii = 0; ii < numLabels; ++ii)
    {
        std::string freq = std::to_string (int (round (fMin * pow (2, ii))));
        
        lbFreq.add (new SimpleLabel(freq));
        lbFreq[ii]->setJustification (Justification::left);
        addAndMakeVisible (lbFreq.getLast());
        scItems.add (FlexItem (freqScaleWidth, freqScaleHeight, *lbFreq[ii]));
    }
    flexboxScale.items = scItems;
    flexboxScale.performLayout (freqIdxArea);
}
