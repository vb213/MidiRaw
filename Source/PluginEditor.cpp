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
CqtanalyzerAudioProcessorEditor::CqtanalyzerAudioProcessorEditor(CqtanalyzerAudioProcessor &p, juce::AudioProcessorValueTreeState &vts)
    : AudioProcessorEditor(&p), audioProcessor(p), valueTreeState(vts), footer(p.getOSCParameterInterface()), cqtVisualizer(audioProcessor.getCQT(), vts), midiNotePanel(audioProcessor.getMidiTranslator())
{
    // ============== BEGIN: essentials ======================
    // set GUI size and lookAndFeel
    // setSize(500, 300); // use this to create a fixed-size GUI
    setResizeLimits(650, 500, 1200, 1000); // use this to create a resizable GUI
    setLookAndFeel(&globalLaF);

    samplerate = p.getSampleRate();

    // make title and footer visible, and set the PluginName
    addAndMakeVisible(&title);
    title.setTitle(juce::String("CQT"), juce::String("Analyzer"));
    title.setFont(globalLaF.robotoBold, globalLaF.robotoLight);
    addAndMakeVisible(&footer);
    // ============= END: essentials ========================

    // create the connection between title component's comboBoxes and valueTreeState

    // Sliders
    addAndMakeVisible(slFMin);
    slFMin.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slFMin.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
    slFMin.setColour(juce::Slider::rotarySliderOutlineColourId, globalLaF.ClWidgetColours[0]);
    slFMin.setTextValueSuffix(" Hz");
    slFMinAttachment.reset(new SliderAttachment(valueTreeState, "fMin", slFMin));
    slFMin.addListener(this);

    addAndMakeVisible(slNOctaves);
    slNOctaves.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slNOctaves.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
    slNOctaves.setColour(juce::Slider::rotarySliderOutlineColourId, globalLaF.ClWidgetColours[1]);
    slNOctavesAttachment.reset(new SliderAttachment(valueTreeState, "nOctaves", slNOctaves));
    slNOctaves.addListener(this);

    addAndMakeVisible(slBPerOct);
    slBPerOct.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slBPerOct.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
    slBPerOct.setColour(juce::Slider::rotarySliderOutlineColourId, globalLaF.ClWidgetColours[1]);
    slBPerOctAttachment.reset(new SliderAttachment(valueTreeState, "bPerOct", slBPerOct));
    slBPerOct.addListener(this);

    addAndMakeVisible(slGamma);
    slGamma.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slGamma.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
    slGamma.setColour(juce::Slider::rotarySliderOutlineColourId, globalLaF.ClWidgetColours[2]);
    slGammaAttachment.reset(new SliderAttachment(valueTreeState, "gamma", slGamma));
    slGamma.addListener(this);

    addAndMakeVisible(slGain);
    slGain.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slGain.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
    slGain.setColour(juce::Slider::thumbColourId, globalLaF.ClWidgetColours[3]);
    slGainAttachment.reset(new SliderAttachment(valueTreeState, "gain", slGain));
    slGain.setTextValueSuffix(" dB");

    addAndMakeVisible(slDBScale);
    slDBScale.setSliderStyle(juce::Slider::LinearHorizontal);
    slDBScale.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
    slDBScale.setColour(juce::Slider::thumbColourId, juce::Colours::grey);
    slDBScaleAttachment.reset(new SliderAttachment(valueTreeState, "dBScale", slDBScale));
    slDBScale.addListener(this);

    addChildComponent(slDynamicRange);
    slDynamicRange.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slDynamicRange.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 70, 20);
    slDynamicRange.setColour(juce::Slider::thumbColourId, globalLaF.ClWidgetColours[3]);
    slDynamicRangeAttachment.reset(new SliderAttachment(valueTreeState, "dynamicRange", slDynamicRange));
    slDynamicRange.setTextValueSuffix(" dB");
    slDynamicRange.addListener(this);

    // Labels

    addAndMakeVisible(lbFMin);
    lbFMin.setJustification(juce::Justification::centred);
    lbFMin.setText("fMin", juce::dontSendNotification);

    addAndMakeVisible(lbNOctaves);
    lbNOctaves.setJustification(juce::Justification::centred);
    lbNOctaves.setText("Octaves", juce::dontSendNotification);

    addAndMakeVisible(lbBPerOct);
    lbBPerOct.setJustification(juce::Justification::centred);
    lbBPerOct.setText("Bins per Oct", juce::dontSendNotification);

    addAndMakeVisible(lbGamma);
    lbGamma.setJustification(juce::Justification::centred);
    lbGamma.setText("Gamma", juce::dontSendNotification);

    addAndMakeVisible(lbGain);
    lbGain.setJustification(juce::Justification::centred);
    lbGain.setText("Gain", juce::dontSendNotification);

    addChildComponent(lbDynamicRange);
    lbDynamicRange.setJustification(juce::Justification::centred);
    lbDynamicRange.setText("Range", juce::dontSendNotification);

    // This is needed for correct scale when reopening UI
    generateScale(slNOctaves.getValue(), slBPerOct.getValue(), slFMin.getValue());

    // Visualizer
    addAndMakeVisible(cqtVisualizer);

    // Debug message panel
    addAndMakeVisible(debugPanel);

    // Active MIDI notes text panel
    addAndMakeVisible(midiNotePanel);

    // start timer after everything is set up properly
    startTimer(25);

    // log a startup message so the debug panel shows some content
    debugPrint("CQT Analyzer GUI initialised at " + juce::String(samplerate) + " Hz");
}

CqtanalyzerAudioProcessorEditor::~CqtanalyzerAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

//==============================================================================
void CqtanalyzerAudioProcessorEditor::paint(juce::Graphics &g)
{
    g.fillAll(globalLaF.ClBackground);
}

void CqtanalyzerAudioProcessorEditor::resized()
{
    // ============ BEGIN: header and footer ============
    const int leftRightMargin = 30;
    const int headerHeight = 60;
    const int footerHeight = 25;
    juce::Rectangle<int> area(getLocalBounds());

    juce::Rectangle<int> footerArea(area.removeFromBottom(footerHeight));
    footer.setBounds(footerArea);

    area.removeFromLeft(leftRightMargin);
    area.removeFromRight(leftRightMargin);
    juce::Rectangle<int> headerArea = area.removeFromTop(headerHeight);
    title.setBounds(headerArea);
    area.removeFromTop(10);
    area.removeFromBottom(5);
    // =========== END: header and footer =================

    // Reserve a fixed strip at the bottom (above the footer) for the "active
    // MIDI notes" text panel, and below that the debug message panel.
    juce::Rectangle<int> midiNoteArea = area.removeFromBottom(midiNotePanelHeight);
    midiNotePanel.setBounds(midiNoteArea);
    area.removeFromBottom(5);

    juce::Rectangle<int> debugArea = area.removeFromBottom(debugPanelHeight);
    debugPanel.setBounds(debugArea);
    area.removeFromBottom(5);

    // Sorting all Sliders and Labels in flexboxes for responsive design
    juce::FlexBox flexboxUI;

    juce::Array<juce::FlexItem> uiItems;
    juce::Array<juce::FlexItem> lbItems;

    flexboxUI.flexDirection = juce::FlexBox::Direction::row;
    flexboxUI.flexWrap = juce::FlexBox::Wrap::noWrap;
    flexboxUI.alignContent = juce::FlexBox::AlignContent::flexEnd;
    flexboxUI.justifyContent = juce::FlexBox::JustifyContent::spaceBetween;
    flexboxUI.alignItems = juce::FlexBox::AlignItems::flexEnd;

    juce::FlexBox flexboxLb = flexboxUI;

    uiItems.add(juce::FlexItem(sliderSize, sliderSize, slFMin));
    uiItems.add(juce::FlexItem(sliderSize, sliderSize, slNOctaves));
    uiItems.add(juce::FlexItem(sliderSize, sliderSize, slBPerOct));
    uiItems.add(juce::FlexItem(sliderSize, sliderSize, slGamma));
    flexboxUI.items = uiItems;

    lbItems.add(juce::FlexItem(sliderSize, labelOffset, lbFMin));
    lbItems.add(juce::FlexItem(sliderSize, labelOffset, lbNOctaves));
    lbItems.add(juce::FlexItem(sliderSize, labelOffset, lbBPerOct));
    lbItems.add(juce::FlexItem(sliderSize, labelOffset, lbGamma));
    flexboxLb.items = lbItems;

    flexboxLb.performLayout(area);
    area.removeFromBottom(labelOffset + 5);
    flexboxUI.performLayout(area);

    area.removeFromBottom(sliderSize + labelOffset + 5);

    // UI control
    juce::Rectangle<int> GainArea = area.removeFromRight(juce::roundToInt(sliderSize * 0.7f));
    slDBScale.setBounds(GainArea.removeFromTop(40));

    lbGain.setBounds(GainArea.removeFromBottom(labelOffset + 5));
    slGain.setBounds(GainArea.removeFromBottom(sliderSize));

    GainArea.removeFromBottom(labelOffset);
    lbDynamicRange.setBounds(GainArea.removeFromBottom(labelOffset + 5));
    slDynamicRange.setBounds(GainArea.removeFromBottom(sliderSize));

    area.removeFromRight(10);

    // Labels for frequency scale
    flexboxScale.flexDirection = juce::FlexBox::Direction::columnReverse;
    flexboxScale.flexWrap = juce::FlexBox::Wrap::noWrap;
    flexboxScale.alignContent = juce::FlexBox::AlignContent::flexEnd;
    flexboxScale.justifyContent = juce::FlexBox::JustifyContent::spaceBetween;
    flexboxScale.alignItems = juce::FlexBox::AlignItems::flexEnd;

    freqIdxArea = area.removeFromRight(freqScaleWidth);

    // necessary for resizeable window
    if (slNOctaves.getValue() > 0)
        generateScale(slNOctaves.getValue(), slBPerOct.getValue(), slFMin.getValue());

    area.removeFromRight(5);

    cqtVisualizer.setBounds(area);
}

void CqtanalyzerAudioProcessorEditor::timerCallback()
{

    // Make slider for range only in dB-Scale visible
    if ((slDBScale.getValue() >= 0.5f) && (slDynamicRange.isVisible() != true))
    {
        slDynamicRange.setVisible(true);
        lbDynamicRange.setVisible(true);
    }
    else if ((slDBScale.getValue() < 0.5f) && (slDynamicRange.isVisible() == true))
    {
        slDynamicRange.setVisible(false);
        lbDynamicRange.setVisible(false);
    }
}

void CqtanalyzerAudioProcessorEditor::sliderValueChanged(juce::Slider *slider)
{
    if (slider == &slDynamicRange)
        cqtVisualizer.setDynamicRange((float)slider->getValue());
    else if (slider == &slDBScale)
        cqtVisualizer.setDBScale((float)slider->getValue());

    if ((slider == &slNOctaves) || (slider == &slBPerOct) || (slider == &slFMin))
        generateScale(slNOctaves.getValue(), slBPerOct.getValue(), slFMin.getValue());
}

void CqtanalyzerAudioProcessorEditor::sliderDragStarted(juce::Slider *slider)
{
    DBG("Slider drag started");
    debugPrint("Slider drag started: " + slider->getName());
    audioProcessor.setSliderDrag(true);
}

void CqtanalyzerAudioProcessorEditor::sliderDragEnded(juce::Slider *slider)
{
    DBG("Slider drag ended");
    debugPrint("Slider drag ended: " + slider->getName());
    audioProcessor.setSliderDrag(false);

    if (slider == &slFMin)
        cqtVisualizer.reallocateImage();
}

void CqtanalyzerAudioProcessorEditor::generateScale(const double nOctaves, const double nBPerOct, const double fMin)
{
    lbFreq.clear();
    scItems.clear();

    const double fMax = juce::jmin(samplerate / 2 / exp2(1.0 / nBPerOct), fMin * pow(2, nOctaves));
    const int numLabels = juce::roundToInt(std::floor(std::log2(fMax / fMin))) + 1;

    for (int ii = 0; ii < numLabels; ++ii)
    {
        std::string freq = std::to_string(int(round(fMin * pow(2, ii))));

        lbFreq.add(new SimpleLabel(freq));
        lbFreq[ii]->setJustification(juce::Justification::left);
        addAndMakeVisible(lbFreq.getLast());
        scItems.add(juce::FlexItem(freqScaleWidth, freqScaleHeight, *lbFreq[ii]));
    }
    flexboxScale.items = scItems;
    flexboxScale.performLayout(freqIdxArea);
}
