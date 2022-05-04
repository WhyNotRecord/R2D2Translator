#pragma once

#include <JuceHeader.h>
#include "SpectrumLineDrawer.h"

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
    bool gate = false;
    juce::Random random;
    double currentSampleRate = 0.0, currentAngle = 0.0, angleDelta = 0.0; // [1]
    double currentFrequency = 440.0, targetFrequency = 440.0;
    //float volume = 0;
    float maxSample = 0;
    int gateCounter = 0;
    const int GATE_LENGTH = 1;//gate release length in seconds
    int gateSampleLength;//gate release length in samples
    int lowFreq = 200, highFreq = 6400;

    juce::dsp::FFT forwardFFT;

    std::array<float, fftSize> fifo;                    // [4]
    std::array<float, fftSize * 2> fftData;             // [5]
    int fifoIndex = 0;                                  // [6]
    bool nextFFTBlockReady = false;                     // [7]
    float maxFFTPower = 1.f;
    float maxFrequencyIndex = fftHalf;



    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
