/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
_3BandEQAudioProcessor::_3BandEQAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

_3BandEQAudioProcessor::~_3BandEQAudioProcessor()
{
}

//==============================================================================
const juce::String _3BandEQAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool _3BandEQAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool _3BandEQAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool _3BandEQAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double _3BandEQAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int _3BandEQAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int _3BandEQAudioProcessor::getCurrentProgram()
{
    return 0;
}

void _3BandEQAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String _3BandEQAudioProcessor::getProgramName (int index)
{
    return {};
}

void _3BandEQAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void _3BandEQAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..

    juce::dsp::ProcessSpec spec;

    spec.maximumBlockSize = samplesPerBlock;

    spec.numChannels = 1;

    spec.sampleRate = sampleRate;

    //prepare both mono chains with the spec
    leftChain.prepare(spec);
    rightChain.prepare(spec);
    
    updateFilters();
}

void _3BandEQAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool _3BandEQAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
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

void _3BandEQAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    //---------update filters before running audio through the chain
    updateFilters();

    //----------run audio through the chain
    //make AudioBlock from buffer
    juce::dsp::AudioBlock<float> block(buffer);
    //get left channel
    auto leftBlock = block.getSingleChannelBlock(0);
    //get right channel
    auto rightBlock = block.getSingleChannelBlock(1);

    //context is sort of a wrapper around the block that the chain can use
    juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock);
    juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);

    //these contexts can then be passed to the mono filter chains to be processed
    leftChain.process(leftContext);
    rightChain.process(rightContext);
}

//==============================================================================
bool _3BandEQAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* _3BandEQAudioProcessor::createEditor()
{
    //editor with customizable GUI
    return new _3BandEQAudioProcessorEditor (*this);
    //generic editor shows sliders but has no customizable GUI
//    return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void _3BandEQAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    juce::MemoryOutputStream mos(destData, true);
    apvts.state.writeToStream(mos);
}

void _3BandEQAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    auto tree = juce::ValueTree::readFromData(data, sizeInBytes);
    if(tree.isValid())
    {
        apvts.replaceState(tree);
        updateFilters();
    }
}

//getter function for chain settings
ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts)
{
    ChainSettings settings;

    settings.lowCutFreq = apvts.getRawParameterValue("LowCut Freq")->load();
    settings.highCutFreq = apvts.getRawParameterValue("HighCut Freq")->load();
    settings.peakFreq = apvts.getRawParameterValue("Peak Freq")->load();
    settings.peakGainInDecibels = apvts.getRawParameterValue("Peak Gain")->load();
    settings.peakQuality = apvts.getRawParameterValue("Peak Quality")->load();
    settings.lowCutSlope = static_cast<Slope>(apvts.getRawParameterValue("LowCut Slope")->load());
    settings.highCutSlope = static_cast<Slope>(apvts.getRawParameterValue("HighCut Slope")->load());

    return settings;
}

//function to update peak filter
void _3BandEQAudioProcessor::updatePeakFilter(const ChainSettings &chainSettings)
{
    //get coefficients for peak Filter
    auto peakCoefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(getSampleRate(), 
            chainSettings.peakFreq, 
            chainSettings.peakQuality, 
            juce::Decibels::decibelsToGain(chainSettings.peakGainInDecibels));
    //apply coefficients to filter chain
    updateCoefficients(leftChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);
    updateCoefficients(rightChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);

}

void _3BandEQAudioProcessor::updateCoefficients(Coefficients &old, const Coefficients &replacements)
{
    *old = *replacements;
}


//update low cut filters
void _3BandEQAudioProcessor::updateLowCutFilters(const ChainSettings& chainSettings)
{
    //get coefficients for low cut filter
    //because the filter can be realized with different steepness levels (different orders), the designIIRHighpass... method has to be used
    //this function returns multiple coefficients for higher order filters (1 coefficient for 2nd order, 2 coefficients for 4th order, ...)
    auto cutCoefficients = juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(chainSettings.lowCutFreq,
            getSampleRate(), 
            2*(chainSettings.lowCutSlope+1));

    //get left low cut filter chain
    auto& leftLowCut = leftChain.get<ChainPositions::LowCut>();
    //get right low cut filter chain
    auto& rightLowCut = rightChain.get<ChainPositions::LowCut>();
    //update left lowcut filter chain
    updateCutFilter(leftLowCut, cutCoefficients, chainSettings.lowCutSlope);
    //update right lowcut filter chain
    updateCutFilter(rightLowCut, cutCoefficients, chainSettings.lowCutSlope);
}
//update high cut filters
void _3BandEQAudioProcessor::updateHighCutFilters(const ChainSettings& chainSettings)
{
    //get coefficients for high cut filter
    auto cutCoefficients = juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod(chainSettings.highCutFreq,
            getSampleRate(), 
            2*(chainSettings.highCutSlope+1));

    //get left high cut filter chain
    auto& leftHighCut = leftChain.get<ChainPositions::HighCut>();
    //get right high cut filter chain
    auto& rightHighCut = rightChain.get<ChainPositions::HighCut>();
    //update left high cut filter chain
    updateCutFilter(leftHighCut, cutCoefficients, chainSettings.highCutSlope);
    //update right high cut filter chain
    updateCutFilter(rightHighCut, cutCoefficients, chainSettings.highCutSlope);
}
//update all the filters
void _3BandEQAudioProcessor::updateFilters()
{
    //get chain Settings
    auto chainSettings = getChainSettings(apvts);
    updateLowCutFilters(chainSettings);
    updatePeakFilter(chainSettings);
    updateHighCutFilters(chainSettings);
}

juce::AudioProcessorValueTreeState::ParameterLayout _3BandEQAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    //slider (AudioParameterFloat) for low cut frequency, standard: 20 Hz
    //skew value of 0.25 distributes the values unevenly over the range from 20 to 20000
    layout.add(std::make_unique<juce::AudioParameterFloat>("LowCut Freq",
            "LowCut Freq",
            juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f),
            20.f));

    //slider (AudioParameterFloat) for high cut frequency, standard: 20 kHz
    layout.add(std::make_unique<juce::AudioParameterFloat>("HighCut Freq",
            "HighCut Freq",
            juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f),
            20000.f));
    
    //slider (AudioParameterFloat) for peak frequency, standard: 750 Hz
    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak Freq",
            "Peak Freq",
            juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f),
            750.f));
            
    //slider (AudioParameterFloat) for peak gain, standard: 0 dB
    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak Gain",
            "Peak Gain",
            juce::NormalisableRange<float>(-24.f, 24.f, 0.5f, 1.f),
            0.0f));
            
    //slider (AudioParameterFloat) for peak quality (how narrow or wide the peak is), standard: 1
    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak Quality",
            "Peak Quality",
            juce::NormalisableRange<float>(0.1f, 10.f, 0.05f, 1.f),
            1.f));

    //choices for filter cut slope in dB per Octave are always multiples of 12
    juce::StringArray stringArray;
    for(int i = 0; i<4; ++i)
    {
        juce::String str;
        str << (12 + i*12);
        str << " db/Oct";
        stringArray.add(str);
    }

    //choice (AudioParameterChoice) for different low cut slopes
    layout.add(std::make_unique<juce::AudioParameterChoice>("LowCut Slope", "LowCut Slope", stringArray, 0));
    //choice (AudioParameterChoice) for different high cut slopes
    layout.add(std::make_unique<juce::AudioParameterChoice>("HighCut Slope", "HighCut Slope", stringArray, 0));


    return layout;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new _3BandEQAudioProcessor();
}
