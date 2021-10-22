/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
//struct for rotary sliders
struct CustomRotarySlider : juce::Slider
{
  CustomRotarySlider() : juce::Slider(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag,
                                      juce::Slider::TextEntryBoxPosition::NoTextBox)
  {
    
  }
};
//struct for linear horizontal sliders
struct CustomLinearHSlider : juce::Slider
{
  CustomLinearHSlider() : juce::Slider(juce::Slider::SliderStyle::LinearHorizontal,
                                      juce::Slider::TextEntryBoxPosition::NoTextBox)
  {
    
  }
};
//struct for linear vertical sliders
struct CustomLinearVSlider : juce::Slider
{
  CustomLinearVSlider() : juce::Slider(juce::Slider::SliderStyle::LinearVertical,
                                      juce::Slider::TextEntryBoxPosition::NoTextBox)
  {
    
  }
};

//===============================ResponseCurveComponent===============================================
//response curve gets its own component so painter can't draw out of bounds
//this is very similar to the _3BandEQAudioProcessorEditor
struct ResponseCurveComponent: juce::Component,
juce::AudioProcessorParameter::Listener,
juce::Timer
{
  ResponseCurveComponent(_3BandEQAudioProcessor&);
  ~ResponseCurveComponent();
    
  void parameterValueChanged (int parameterIndex, float newValue) override;

  //empty implementation for this function, because we don't use parameterGestures
  void parameterGestureChanged (int parameterIndex, bool gestureIsStarting) override {}

  void timerCallback() override;

  void paint(juce::Graphics& g) override;
private:
  _3BandEQAudioProcessor& audioProcessor;
  juce::Atomic<bool> parametersChanged{false};

  MonoChain monoChain;
};


//===================================_3BandEQAudioProcessorEditor===========================================
class _3BandEQAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    _3BandEQAudioProcessorEditor (_3BandEQAudioProcessor&);
    ~_3BandEQAudioProcessorEditor() override;
    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    _3BandEQAudioProcessor& audioProcessor;

    //all the rotary sliders
    CustomRotarySlider peakGainSlider,
    peakQualitySlider;

    //all the linear sliders
    CustomLinearHSlider peakFreqSlider,
    lowCutFreqSlider,
    highCutFreqSlider;
    CustomLinearVSlider lowCutSlopeSlider,
    highCutSlopeSlider;

    //instance of ResponseCurveComponent
    ResponseCurveComponent responseCurveComponent;

    //alias for readability
    using APVTS = juce::AudioProcessorValueTreeState;
    using Attachment = APVTS::SliderAttachment;

    Attachment peakFreqSliderAttachment,
    peakGainSliderAttachment,
    peakQualitySliderAttachment,
    lowCutFreqSliderAttachment,
    highCutFreqSliderAttachment,
    lowCutSlopeSliderAttachment,
    highCutSlopeSliderAttachment;

    //helper function to get Components as vector
    std::vector<juce::Component*> getComps();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (_3BandEQAudioProcessorEditor)
};
