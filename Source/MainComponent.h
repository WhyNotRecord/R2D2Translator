#pragma once

#include <JuceHeader.h>

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class MainComponent  : public juce::AudioAppComponent
{
public:
    //==============================================================================
    MainComponent();
    ~MainComponent() override;

    //==============================================================================
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    //==============================================================================
    void paint (juce::Graphics& g) override;
    void resized() override;

    void updateAngleDelta();

private:
    juce::Slider frequencySlider;
    juce::Slider volumeSlider;
    juce::Slider gateSlider;
    juce::Random random;
    double currentSampleRate = 0.0, currentAngle = 0.0, angleDelta = 0.0; // [1]
    double currentFrequency = 440.0, targetFrequency = 440.0;
    //float volume = 0;
    bool gate = false;
    float maxSample = 0;
    int gateCounter = 0;
    int GATE_LENGTH = 10000;
    int lowFreq = 200, highFreq = 3200;


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
