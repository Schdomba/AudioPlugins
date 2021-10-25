/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
//===============================ResponseCurveComponent===============================================
ResponseCurveComponent::ResponseCurveComponent(_3BandEQAudioProcessor& p) : audioProcessor(p)
{
  //get all the parameters
  const auto& params = audioProcessor.getParameters();
  //set up listener to parameter changes
  for( auto param : params )
  {
    param->addListener(this);
  }

  //start Timer for repainting (60 Hz refresh rate)
  startTimerHz(60);
}
ResponseCurveComponent::~ResponseCurveComponent()
{
  //get all the parameters
  const auto& params = audioProcessor.getParameters();
  //deregister listeners
  for( auto param : params )
  {
    param->removeListener(this);
  }
}

//==============================================================================

void ResponseCurveComponent::parameterValueChanged (int parameterIndex, float newValue)
{
  //set atomic flag parametersChanged true if parameter value changed
  parametersChanged.set(true);
}

void ResponseCurveComponent::timerCallback()
{
  //check if parametersChanged is true, reset it to false and do stuff
  if( parametersChanged.compareAndSetBool(false, true))
  {
    //update the monoChain
    auto chainSettings = getChainSettings(audioProcessor.apvts);
    auto peakCoefficients = makePeakFilter(chainSettings, audioProcessor.getSampleRate());
    auto lowCutCoefficients = makeLowCutFilter(chainSettings, audioProcessor.getSampleRate());
    auto highCutCoefficients = makeHighCutFilter(chainSettings, audioProcessor.getSampleRate());

    updateCoefficients(monoChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);
    updateCutFilter(monoChain.get<ChainPositions::LowCut>(), lowCutCoefficients, chainSettings.lowCutSlope);
    updateCutFilter(monoChain.get<ChainPositions::HighCut>(), highCutCoefficients, chainSettings.highCutSlope);


    //signal a repaint to draw new response curve
    repaint();
  }
}

void ResponseCurveComponent::paint (juce::Graphics& g)
{
    using namespace juce;
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.drawImage(background, getLocalBounds().toFloat());

    //bounding box
    auto responseArea = getAnalysisArea();
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
    g.setColour(getLookAndFeel().findColour (juce::Slider::thumbColourId));
    g.drawRoundedRectangle(getRenderArea().toFloat(), 4.f, 1.f);

    //draw response curve
    g.setColour(Colours::white);
    g.strokePath(responseCurve, PathStrokeType(2.f));
}

void ResponseCurveComponent::resized()
{
  using namespace juce;
  //make background image
  background = Image(Image::PixelFormat::RGB, getWidth(), getHeight(), true);
  //get graphics
  Graphics g(background);
  //set colur to dimgrey
  g.setColour(Colours::dimgrey);

  //array with all frequencies to draw lines at
  Array<float> freqs 
  {
    20, 50, 100,
    200, 500, 1000,
    2000, 5000, 10000,
    20000
  };

  //get render area information
  auto renderArea = getAnalysisArea();
  auto left = renderArea.getX();
  auto right = renderArea.getRight();
  auto top = renderArea.getY();
  auto bottom = renderArea.getBottom();
  auto width = renderArea.getWidth();

  //array of x positions
  Array<float> xs;
  for( auto f : freqs )
  {
    //map frequency to normalized position
    auto normX = mapFromLog10(f, 20.f, 20000.f);
    //compute that to window x position
    xs.add(left + width * normX);
  }

  //draw vertical lines (frequencies)
  for( auto x : xs )
  {
    //draw vertical line for each x
    g.drawVerticalLine(x, top, bottom);
  }
  
  //array with all gain values to draw lines at
  Array<float> gain 
  {
    -24, -12, 0, 12, 24
  };
  //draw horizontal lines (gain values)
  for( auto gdB : gain )
  {
    //map gain to corresponding pixel
    auto y = jmap(gdB, -24.f, 24.f, float(bottom), float(top));
    //set color to white for 0dB line
    g.setColour( gdB == 0.f ? Colours::white : Colours::dimgrey );
    //draw line
    g.drawHorizontalLine(y, left, right);
  }
  //draw frequency labels
  //basic setup
  g.setColour(Colours::dimgrey);
  const int fontHeight = 10;
  g.setFont(fontHeight);

  //loop thorugh frequencies and xs to draw texts
  for( int i = 0; i < freqs.size(); ++i)
  {
    auto f = freqs[i];
    auto x = xs[i];
    //add k (before Hz) to frequencies >= 1000 Hz and divide them by 1000 for the string
    bool addK = false;
    String str;
    if( f > 999.f )
    {
      addK = true;
      f /= 1000.f;
    }
    str << f;
    if( addK )
      str << "k";
    str << "Hz";

    //get string with to build rectangle around it
    auto textWidth = g.getCurrentFont().getStringWidth(str);
    Rectangle<int> r;
    r.setSize(textWidth, fontHeight);
    r.setCentre(x, 0);
    r.setY(1);
    //draw text
    g.drawFittedText(str, r, juce::Justification::centred, 1);

  }
}

juce::Rectangle<int> ResponseCurveComponent::getRenderArea()
{
  //get bounds
  auto bounds = getLocalBounds();

  //reduce it
  bounds.removeFromTop(12);
  bounds.removeFromBottom(2);
  bounds.removeFromLeft(20);
  bounds.removeFromRight(20);

  return bounds;
}

juce::Rectangle<int> ResponseCurveComponent::getAnalysisArea()
{
  auto bounds = getRenderArea();

  bounds.removeFromTop(4);
  bounds.removeFromBottom(4);

  return bounds;
}

//===================================_3BandEQAudioProcessorEditor===========================================
_3BandEQAudioProcessorEditor::_3BandEQAudioProcessorEditor (_3BandEQAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
    responseCurveComponent(audioProcessor),
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
}

void _3BandEQAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..

    //bounding box
    auto bounds = getLocalBounds();
    //top third of the window is for response curve
    auto responseArea = bounds.removeFromTop(bounds.getHeight() * 0.33);
    //make responseCurveComponent inside of this area
    responseCurveComponent.setBounds(responseArea);

    //chop 0.33 from the top (of the remaining two thirds) for frequency controls
    auto freqArea = bounds.removeFromTop(bounds.getHeight() * 0.33);
    //chop 0.2 from the left of this area to display texts
    freqArea.removeFromLeft(freqArea.getWidth() * 0.1);
    //low cut freq slider is on top
    lowCutFreqSlider.setBounds(freqArea.removeFromTop(freqArea.getHeight() * 0.33));
    setupSlider(lowCutFreqSlider, lowCutFreqLabel, " Hz", "low cut f", true);
    //peak freq slider in the middle
    peakFreqSlider.setBounds(freqArea.removeFromTop(freqArea.getHeight() * 0.5));
    setupSlider(peakFreqSlider, peakFreqLabel, " Hz", "peak f", true);
    //high cut freq slider on the bottom
    highCutFreqSlider.setBounds(freqArea);
    setupSlider(highCutFreqSlider, highCutFreqLabel, " Hz", "high cut f", true);

    //chop 0.33 from the left of bounds for low Cut controls
    auto lowCutArea = bounds.removeFromLeft(bounds.getWidth() * 0.33);
    //chop 0.5 from the right of the remaining 0.66 of bounds for high cut controls
    auto highCutArea = bounds.removeFromRight(bounds.getWidth() * 0.5);

    //set bounds for low and high cut controls inside of highCutArea and lowCutArea
    //gap on top and bottom
    lowCutArea.removeFromTop(lowCutArea.getHeight() * 0.2);
    //cut the sides so the label is right on top of the control
    lowCutArea.removeFromLeft(lowCutArea.getWidth() * 0.3);
    lowCutArea.removeFromRight(lowCutArea.getWidth() * 0.33);
    lowCutSlopeSlider.setBounds(lowCutArea);
    setupSlider(lowCutSlopeSlider, lowCutSlopeLabel, " dB/Oct", "low cut slope", false);
    //middle 0.33 are for slope slider
    highCutArea.removeFromTop(highCutArea.getHeight() * 0.2);
    highCutArea.removeFromBottom(highCutArea.getHeight() * 0.1);
    //cut the sides so the label is right on top of the control
    highCutArea.removeFromLeft(highCutArea.getWidth() * 0.3);
    highCutArea.removeFromRight(highCutArea.getWidth() * 0.33);
    highCutSlopeSlider.setBounds(highCutArea);
    setupSlider(highCutSlopeSlider, highCutSlopeLabel, " dB/Oct", "high cut slope", false);

    //set bounds for peak filter controls (all inside the remaining bounds, which is basically the middle column)
    //gap on top
    bounds.removeFromTop(bounds.getHeight() * 0.2);
    //peakGainSlider is left
    peakGainSlider.setBounds(bounds.removeFromLeft(bounds.getWidth() * 0.5));
    setupSlider(peakGainSlider, peakGainLabel, " dB", "peak gain", false);
    //peakQualitySlider right
    peakQualitySlider.setBounds(bounds);
    setupSlider(peakQualitySlider, peakQualityLabel, " ", "peak Q", false);
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
    &highCutSlopeSlider,
    &responseCurveComponent,
    &peakFreqLabel,
    &peakGainLabel,
    &peakQualityLabel,
    &lowCutFreqLabel,
    &highCutFreqLabel,
    &lowCutSlopeLabel,
    &highCutSlopeLabel
  };
}

void _3BandEQAudioProcessorEditor::setupSlider(juce::Slider& slider, juce::Label& label, juce::String valueSuffix, juce::String text, bool onLeft)
{
    slider.setTextValueSuffix(valueSuffix);
    label.setText(text, juce::dontSendNotification);
    label.attachToComponent (&slider, onLeft);
}
