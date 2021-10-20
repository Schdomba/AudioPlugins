/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//Slope setting
enum Slope
{
  Slope_12,
  Slope_24,
  Slope_36,
  Slope_48
};

//All Parameters of the Chain
struct ChainSettings
{
  float peakFreq{0}, peakGainInDecibels{0}, peakQuality{1.f};
  float lowCutFreq{0}, highCutFreq{0};
  Slope lowCutSlope{Slope::Slope_12}, highCutSlope{Slope::Slope_12};
};
//getter function for this struct
ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts);

//==============================================================================
/**
*/
class _3BandEQAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    _3BandEQAudioProcessor();
    ~_3BandEQAudioProcessor() override;

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
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts {*this, nullptr, "Parameters", createParameterLayout()};

private:
    //Filter with float
    using Filter = juce::dsp::IIR::Filter<float>;
    //CutFilter is 4 filters in a row
    using CutFilter = juce::dsp::ProcessorChain<Filter, Filter, Filter, Filter>;
    //MonoChain is Lowcut, Peak, Highcut in a row
    using MonoChain = juce::dsp::ProcessorChain<CutFilter, Filter, CutFilter>;
    //two MonoChains make the Stereo Chain
    MonoChain leftChain, rightChain;
    //position of elements in the chain
    enum ChainPositions
    {
      LowCut,
      Peak,
      HighCut
    };
    //function to update peak filter
    void updatePeakFilter(const ChainSettings& chainSettings);
    //declare alias for coefficients
    using Coefficients = Filter::CoefficientsPtr;
    //helper function to update coefficients
    static void updateCoefficients(Coefficients& old, const Coefficients& replacements);

    //helper function to update cut filter coefficients
    template<int Index, typename ChainType, typename CoefficientType>
    void update(ChainType& chain, const CoefficientType& coefficients)
    {
      updateCoefficients(chain.template get<Index>().coefficients, coefficients[Index]);
      chain.template setBypassed<Index>(false);
    }
    //template helper function to update cut filters
    template<typename ChainType, typename CoefficientType>
    void updateCutFilter(ChainType& chain,
        const CoefficientType& coefficients, 
        const Slope& slope)
    {
      //bypass all of the links in the chain
      chain.template setBypassed<0>(true);
      chain.template setBypassed<1>(true);
      chain.template setBypassed<2>(true);
      chain.template setBypassed<3>(true);

      //because the number of coefficients varies based on the filter order, this switch statement is needed
      switch(slope)
      {
          case Slope_48:
          {
            update<3>(chain, coefficients);
          }
          case Slope_36:
          {
            update<2>(chain, coefficients);
          }
          case Slope_24:
          {
            update<1>(chain, coefficients);
          }
          case Slope_12:
          {
            update<0>(chain, coefficients);
          }
      }
    }

    //update low cut filters
    void updateLowCutFilters(const ChainSettings& chainSettings);
    //update high cut filters
    void updateHighCutFilters(const ChainSettings& chainSettings);
    //update all the filters
    void updateFilters();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (_3BandEQAudioProcessor)
};
