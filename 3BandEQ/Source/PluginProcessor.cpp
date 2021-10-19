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

    //get chain Settings
    auto chainSettings = getChainSettings(apvts);

    //get coefficients for peak Filter
    auto peakCoefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, 
            chainSettings.peakFreq, 
            chainSettings.peakQuality, 
            juce::Decibels::decibelsToGain(chainSettings.peakGainInDecibels));
    //apply coefficients to filter chain
    *leftChain.get<ChainPositions::Peak>().coefficients = *peakCoefficients;
    *rightChain.get<ChainPositions::Peak>().coefficients = *peakCoefficients;

    
    //get coefficients for low cut filter
    //because the filter can be realized with different steepness levels (different orders), the designIIRHighpass... method has to be used
    //this function returns multiple coefficients for higher order filters (1 coefficient for 2nd order, 2 coefficients for 4th order, ...)
    auto lowCutCoefficients = juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(chainSettings.lowCutFreq,
            sampleRate, 
            2*(chainSettings.lowCutSlope+1));

    //get left low cut filter chain
    auto& leftLowCut = leftChain.get<ChainPositions::LowCut>();
    //bypass all of the links in the chain
    leftLowCut.setBypassed<0>(true);
    leftLowCut.setBypassed<1>(true);
    leftLowCut.setBypassed<2>(true);
    leftLowCut.setBypassed<3>(true);

    //because the number of coefficients varies based on the filter order, this switch statement is needed
    switch(chainSettings.lowCutSlope)
    {
        //Slope 12 corresponds to 2nd order filter, so 1 set of coefficients
        case Slope_12:
            *leftLowCut.get<0>().coefficients = *lowCutCoefficients[0];
            leftLowCut.setBypassed<0>(false);
            break;
        //Slope 24 corresponds to 4th order filter, so 2 sets of coefficients
        case Slope_24:
            *leftLowCut.get<0>().coefficients = *lowCutCoefficients[0];
            *leftLowCut.get<1>().coefficients = *lowCutCoefficients[1];
            leftLowCut.setBypassed<0>(false);
            leftLowCut.setBypassed<1>(false);
            break;
        //Slope 36 corresponds to 6th order filter, so 3 sets of coefficients
        case Slope_36:
            *leftLowCut.get<0>().coefficients = *lowCutCoefficients[0];
            *leftLowCut.get<1>().coefficients = *lowCutCoefficients[1];
            *leftLowCut.get<2>().coefficients = *lowCutCoefficients[2];
            leftLowCut.setBypassed<0>(false);
            leftLowCut.setBypassed<1>(false);
            leftLowCut.setBypassed<2>(false);
            break;
        //Slope 48 corresponds to 8th order filter, so 4 sets of coefficients
        case Slope_48:
            *leftLowCut.get<0>().coefficients = *lowCutCoefficients[0];
            *leftLowCut.get<1>().coefficients = *lowCutCoefficients[1];
            *leftLowCut.get<2>().coefficients = *lowCutCoefficients[2];
            *leftLowCut.get<3>().coefficients = *lowCutCoefficients[3];
            leftLowCut.setBypassed<0>(false);
            leftLowCut.setBypassed<1>(false);
            leftLowCut.setBypassed<2>(false);
            leftLowCut.setBypassed<3>(false);
            break;
    }

    
    //get right low cut filter chain
    auto& rightLowCut = rightChain.get<ChainPositions::LowCut>();
    //bypass all of the links in the chain
    rightLowCut.setBypassed<0>(true);
    rightLowCut.setBypassed<1>(true);
    rightLowCut.setBypassed<2>(true);
    rightLowCut.setBypassed<3>(true);

    //because the number of coefficients varies based on the filter order, this switch statement is needed
    switch(chainSettings.lowCutSlope)
    {
        //Slope 12 corresponds to 2nd order filter, so 1 set of coefficients
        case Slope_12:
            *rightLowCut.get<0>().coefficients = *lowCutCoefficients[0];
            rightLowCut.setBypassed<0>(false);
            break;
        //Slope 24 corresponds to 4th order filter, so 2 sets of coefficients
        case Slope_24:
            *rightLowCut.get<0>().coefficients = *lowCutCoefficients[0];
            *rightLowCut.get<1>().coefficients = *lowCutCoefficients[1];
            rightLowCut.setBypassed<0>(false);
            rightLowCut.setBypassed<1>(false);
            break;
        //Slope 36 corresponds to 6th order filter, so 3 sets of coefficients
        case Slope_36:
            *rightLowCut.get<0>().coefficients = *lowCutCoefficients[0];
            *rightLowCut.get<1>().coefficients = *lowCutCoefficients[1];
            *rightLowCut.get<2>().coefficients = *lowCutCoefficients[2];
            rightLowCut.setBypassed<0>(false);
            rightLowCut.setBypassed<1>(false);
            rightLowCut.setBypassed<2>(false);
            break;
        //Slope 48 corresponds to 8th order filter, so 4 sets of coefficients
        case Slope_48:
            *rightLowCut.get<0>().coefficients = *lowCutCoefficients[0];
            *rightLowCut.get<1>().coefficients = *lowCutCoefficients[1];
            *rightLowCut.get<2>().coefficients = *lowCutCoefficients[2];
            *rightLowCut.get<3>().coefficients = *lowCutCoefficients[3];
            rightLowCut.setBypassed<0>(false);
            rightLowCut.setBypassed<1>(false);
            rightLowCut.setBypassed<2>(false);
            rightLowCut.setBypassed<3>(false);
            break;
    }

    //get coefficients for high cut filter
    
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

    //---------update parameters before running audio through the chain
    //get chain Settings
    auto chainSettings = getChainSettings(apvts);

    //get coefficients for peak Filter
    auto peakCoefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(getSampleRate(), 
            chainSettings.peakFreq, 
            chainSettings.peakQuality, 
            juce::Decibels::decibelsToGain(chainSettings.peakGainInDecibels));
    //apply coefficients to filter chain
    *leftChain.get<ChainPositions::Peak>().coefficients = *peakCoefficients;
    *rightChain.get<ChainPositions::Peak>().coefficients = *peakCoefficients;


    
    //get coefficients for low cut filter
    //because the filter can be realized with different steepness levels (different orders), the designIIRHighpass... method has to be used
    //this function returns multiple coefficients for higher order filters (1 coefficient for 2nd order, 2 coefficients for 4th order, ...)
    auto lowCutCoefficients = juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(chainSettings.lowCutFreq,
            getSampleRate(), 
            2*(chainSettings.lowCutSlope+1));

    //get left low cut filter chain
    auto& leftLowCut = leftChain.get<ChainPositions::LowCut>();
    //bypass all of the links in the chain
    leftLowCut.setBypassed<0>(true);
    leftLowCut.setBypassed<1>(true);
    leftLowCut.setBypassed<2>(true);
    leftLowCut.setBypassed<3>(true);

    //because the number of coefficients varies based on the filter order, this switch statement is needed
    switch(chainSettings.lowCutSlope)
    {
        //Slope 12 corresponds to 2nd order filter, so 1 set of coefficients
        case Slope_12:
            *leftLowCut.get<0>().coefficients = *lowCutCoefficients[0];
            leftLowCut.setBypassed<0>(false);
            break;
        //Slope 24 corresponds to 4th order filter, so 2 sets of coefficients
        case Slope_24:
            *leftLowCut.get<0>().coefficients = *lowCutCoefficients[0];
            *leftLowCut.get<1>().coefficients = *lowCutCoefficients[1];
            leftLowCut.setBypassed<0>(false);
            leftLowCut.setBypassed<1>(false);
            break;
        //Slope 36 corresponds to 6th order filter, so 3 sets of coefficients
        case Slope_36:
            *leftLowCut.get<0>().coefficients = *lowCutCoefficients[0];
            *leftLowCut.get<1>().coefficients = *lowCutCoefficients[1];
            *leftLowCut.get<2>().coefficients = *lowCutCoefficients[2];
            leftLowCut.setBypassed<0>(false);
            leftLowCut.setBypassed<1>(false);
            leftLowCut.setBypassed<2>(false);
            break;
        //Slope 48 corresponds to 8th order filter, so 4 sets of coefficients
        case Slope_48:
            *leftLowCut.get<0>().coefficients = *lowCutCoefficients[0];
            *leftLowCut.get<1>().coefficients = *lowCutCoefficients[1];
            *leftLowCut.get<2>().coefficients = *lowCutCoefficients[2];
            *leftLowCut.get<3>().coefficients = *lowCutCoefficients[3];
            leftLowCut.setBypassed<0>(false);
            leftLowCut.setBypassed<1>(false);
            leftLowCut.setBypassed<2>(false);
            leftLowCut.setBypassed<3>(false);
            break;
    }

    
    //get right low cut filter chain
    auto& rightLowCut = rightChain.get<ChainPositions::LowCut>();
    //bypass all of the links in the chain
    rightLowCut.setBypassed<0>(true);
    rightLowCut.setBypassed<1>(true);
    rightLowCut.setBypassed<2>(true);
    rightLowCut.setBypassed<3>(true);

    //because the number of coefficients varies based on the filter order, this switch statement is needed
    switch(chainSettings.lowCutSlope)
    {
        //Slope 12 corresponds to 2nd order filter, so 1 set of coefficients
        case Slope_12:
            *rightLowCut.get<0>().coefficients = *lowCutCoefficients[0];
            rightLowCut.setBypassed<0>(false);
            break;
        //Slope 24 corresponds to 4th order filter, so 2 sets of coefficients
        case Slope_24:
            *rightLowCut.get<0>().coefficients = *lowCutCoefficients[0];
            *rightLowCut.get<1>().coefficients = *lowCutCoefficients[1];
            rightLowCut.setBypassed<0>(false);
            rightLowCut.setBypassed<1>(false);
            break;
        //Slope 36 corresponds to 6th order filter, so 3 sets of coefficients
        case Slope_36:
            *rightLowCut.get<0>().coefficients = *lowCutCoefficients[0];
            *rightLowCut.get<1>().coefficients = *lowCutCoefficients[1];
            *rightLowCut.get<2>().coefficients = *lowCutCoefficients[2];
            rightLowCut.setBypassed<0>(false);
            rightLowCut.setBypassed<1>(false);
            rightLowCut.setBypassed<2>(false);
            break;
        //Slope 48 corresponds to 8th order filter, so 4 sets of coefficients
        case Slope_48:
            *rightLowCut.get<0>().coefficients = *lowCutCoefficients[0];
            *rightLowCut.get<1>().coefficients = *lowCutCoefficients[1];
            *rightLowCut.get<2>().coefficients = *lowCutCoefficients[2];
            *rightLowCut.get<3>().coefficients = *lowCutCoefficients[3];
            rightLowCut.setBypassed<0>(false);
            rightLowCut.setBypassed<1>(false);
            rightLowCut.setBypassed<2>(false);
            rightLowCut.setBypassed<3>(false);
            break;
    }


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
//    return new _3BandEQAudioProcessorEditor (*this);
    return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void _3BandEQAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void _3BandEQAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
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
