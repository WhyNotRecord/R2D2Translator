#pragma once

#include <JuceHeader.h>
#include "SpectrumLineDrawer.h"
#include "UnifiedVectorsDrawer.h"
#define mainFrequenciesCount 5

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class MainComponent  : public juce::AudioAppComponent, private juce::Timer
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
    void timerCallback() override;

    void updateAngleDelta();
    void pushNextSampleIntoFifo(float sample) noexcept;
    void processNextFFTBlock();
    void evaluateLastBlockMainFrequency();
    float getValueForFrequency(int frequency);

    enum MainState {
        Calibrating,
        Idling,
        Listening,
        Speaking
    };

    static constexpr auto fftOrder = 10;                // [1]
    static constexpr auto fftSize = 1 << fftOrder;     // [2]
    static constexpr auto fftHalf = fftSize / 2 + 1;

private:
    MainState currentState = Idling;
    const juce::String WindowCaption = "R2D2 Speech Translator";
    juce::Slider frequencySlider;
    juce::Slider volumeSlider;
    juce::Slider gateSlider;
    SpectrumLineDrawer drawer;
    UnifiedVectorsDrawer specialDrawer;
    bool gate = false;
    juce::Random random;
    double currentSampleRate = 0.0, currentAngle = 0.0, angleDelta = 0.0; // [1]
    double currentFrequency = 440.0, targetFrequency = 440.0;
    float oneBlockLength = 1;
    //float volume = 0;
    float maxSample = 0;
    int gateCounter = 0;
    const int GATE_LENGTH = 1;//gate release length in seconds
    int gateSampleLength;//gate release length in samples
    int lowFreq = 200, highFreq = 6400;
    int mainFrequencies[mainFrequenciesCount] = {200, 600, 1200, 3600, 6000};
    //int mainFreq1 = 200, mainFreq2 = 600, mainFreq3 = 1200, mainFreq4 = 3600, mainFreq5 = 6000;

    juce::dsp::FFT forwardFFT;

    std::array<float, fftSize> fifo;                    // [4]
    std::array<float, fftSize * 2> fftData;             // [5]
    int fifoIndex = 0;                                  // [6]
    bool nextFFTBlockReady = false;                     // [7]
    float maxFFTPower = 1.f;
    float maxFrequencyIndex = fftHalf;



    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
