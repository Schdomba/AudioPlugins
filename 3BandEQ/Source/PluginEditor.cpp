/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
_3BandEQAudioProcessorEditor::_3BandEQAudioProcessorEditor (_3BandEQAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
    peakFreqSliderAttachment(audioProcessor.apvts, "Peak Freq", peakFreqSlider),
    peakGainSliderAttachment(audioProcessor.apvts, "Peak Gain", peakGainSlider),
    peakQualitySliderAttachment(audioProcessor.apvts, "Peak Quality", peakQualitySlider),
    lowCutFreqSliderAttachment(audioProcessor.apvts, "LowCut Freq", lowCutFreqSlider),
    highCutFreqSliderAttachment(audioProcessor.apvts, "HighCut Freq", highCutFreqSlider),
    lowCutSlopeSliderAttachment(audioProcessor.apvts, "LowCut Slope", lowCutSlopeSlider),
    highCutSlopeSliderAttachment(audioProcessor.apvts, "HighCut Slope", highCutSlopeSlider)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.

    //make all components visible by iterating through them
    for(auto* comp : getComps())
    {
      addAndMakeVisible(comp);
    }

    //plugin window size
    setSize (600, 400);
}

_3BandEQAudioProcessorEditor::~_3BandEQAudioProcessorEditor()
{
}

//==============================================================================
void _3BandEQAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    //g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    // g.setColour (juce::Colours::white);
    // g.setFont (15.0f);
    // g.drawFittedText ("Hello World!", getLocalBounds(), juce::Justification::centred, 1);
}

void _3BandEQAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..

    //bounding box
    auto bounds = getLocalBounds();
    //top third of the window is for response curve
    auto responseArea = bounds.removeFromTop(bounds.getHeight() * 0.33);

    //chop 0.33 from the left of bounds for low Cut controls
    auto lowCutArea = bounds.removeFromLeft(bounds.getWidth() * 0.33);
    //chop 0.5 from the right of the remaining 0.66 of bounds for high cut controls
    auto highCutArea = bounds.removeFromRight(bounds.getWidth() * 0.5);

    //set bounds for low and high cut controls inside of highCutArea and lowCutArea
    //top 0.66 are for freq slider
    lowCutFreqSlider.setBounds(lowCutArea.removeFromTop(bounds.getHeight() * 0.66));
    //bottom 0.33 are for slope slider
    lowCutSlopeSlider.setBounds(lowCutArea);
    //top 0.66 are for freq slider
    highCutFreqSlider.setBounds(highCutArea.removeFromTop(bounds.getHeight() * 0.66));
    //bottom 0.33 are for slope slider
    highCutSlopeSlider.setBounds(highCutArea);

    //set bounds for peak filter controls (all inside the remaining bounds, which is basically the middle column)
    //peakFreqSlilder is in the top third
    peakFreqSlider.setBounds(bounds.removeFromTop(bounds.getHeight() * 0.33));
    //peakGainSlider is in the middle third (removing 0.5 from the remaining 0.66)
    peakGainSlider.setBounds(bounds.removeFromTop(bounds.getHeight() * 0.5));
    //peakQualitySlider is on the bottom third (the remaining 0.33)
    peakQualitySlider.setBounds(bounds);
}

std::vector<juce::Component*> _3BandEQAudioProcessorEditor::getComps()
{
  return
  {
    &peakFreqSlider,
    &peakGainSlider,
    &peakQualitySlider,
    &lowCutFreqSlider,
    &highCutFreqSlider,
    &lowCutSlopeSlider,
    &highCutSlopeSlider
  };
}
