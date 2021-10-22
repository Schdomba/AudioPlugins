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
    using namespace juce;
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    //bounding box
    auto bounds = getLocalBounds();
    //top third of the window is for response curve
    auto responseArea = bounds.removeFromTop(bounds.getHeight() * 0.33);
    //width of response curve area
    auto w = responseArea.getWidth();

    //chain elements
    auto& lowCut = monoChain.get<ChainPositions::LowCut>();
    auto& peak = monoChain.get<ChainPositions::Peak>();
    auto& highCut = monoChain.get<ChainPositions::HighCut>();

    //the sample rate form audioProcessor
    auto sampleRate = audioProcessor.getSampleRate();

    //vector to store the magnitudes
    std::vector<double> mags;
    //we need as many magnitues as there are pixels in width
    mags.resize(w);

    //compute magnitude for each pixel (or frequency)
    for( int i = 0; i < w; ++i)
    {
      //set magnitude to 1 (not 0 because it will be multiplied later)
      double mag = 1.f;
      //map pixel coordinates to hearable range of 20 Hz to 20 kHz
      auto freq = mapToLog10(double(i) / double(w), 20.0, 20000.0);

      //check if band is bypassed
      if(! monoChain.isBypassed<ChainPositions::Peak>())
        mag *= peak.coefficients->getMagnitudeForFrequency(freq, sampleRate);
      //check for bypassed in all the low and highcuts
      if(! lowCut.isBypassed<0>())
        mag *= lowCut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
      if(! lowCut.isBypassed<1>())
        mag *= lowCut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
      if(! lowCut.isBypassed<2>())
        mag *= lowCut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
      if(! lowCut.isBypassed<3>())
        mag *= lowCut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);

      if(! highCut.isBypassed<0>())
        mag *= highCut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
      if(! highCut.isBypassed<1>())
        mag *= highCut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
      if(! highCut.isBypassed<2>())
        mag *= highCut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
      if(! highCut.isBypassed<3>())
        mag *= highCut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);

      //convert magnitude into db and store
      mags[i] = Decibels::gainToDecibels(mag);
    }
    //convert magnitude vector into Path
    Path responseCurve;
    //response area bottom and top
    const double outputMin = responseArea.getBottom();
    const double outputMax = responseArea.getY();
    //helper function to map magnitudes to response area
    auto map = [outputMin, outputMax](double input)
    {
      return jmap(input, -24.0, 24.0, outputMin, outputMax);
    };

    //start path at first magnitude entry
    responseCurve.startNewSubPath(responseArea.getX(), map(mags.front()));
    //loop through all magnitude entries and connect them
    for( size_t i = 1; i < mags.size(); ++i )
    {
      responseCurve.lineTo(responseArea.getX() + i, map(mags[i]));
    }

    //draw boundary box
    g.setColour(Colours::orange);
    g.drawRoundedRectangle(responseArea.toFloat(), 4.f, 1.f);

    //draw response curve
    g.setColour(Colours::white);
    g.strokePath(responseCurve, PathStrokeType(2.f));
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

void _3BandEQAudioProcessorEditor::parameterValueChanged (int parameterIndex, float newValue)
{
  //set atomic flag parametersChanged true if parameter value changed
  parametersChanged.set(true);
}

void _3BandEQAudioProcessorEditor::timerCallback()
{
  //check if parametersChanged is true, reset it to false and do stuff
  if( parametersChanged.compareAndSetBool(false, true))
  {
    //update the monoChain
    //signal a repaint to draw new response curve
  }
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
