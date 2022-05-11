#pragma once

#include <JuceHeader.h>
#include "SpectrumLineDrawer.h"
#include "UnifiedVectorsDrawerAlt.h"

#define FFT_IMAGE_SEQ_LENGTH 32
#define FADEOUT_LENGTH 10
#define AUTOGATE_LENGTH 75

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

    //==============================================================================
    void processCurrentState();
    void updateAngleDelta();
    void pushNextSampleIntoFifo(float sample) noexcept;
    void fillFifoWithZeros() noexcept;
    void processNextFFTBlock();
    float evaluateLastBlockMainFrequency();
    float getValueForFrequency(int frequency);
    void addNextFftImage(bool fin = false);
    void resetFftImageSequence();

    enum MainState {
        Calibrating,
        Idling,
        Listening,
        Speaking,
        FadingOut,
        Error
    };

    static constexpr auto fftOrder = 8;                // [1]
    static constexpr auto fftSize = 1 << fftOrder;     // [2]
    static constexpr auto fftHalf = fftSize / 2 + 1;

private:
    MainState currentState = Calibrating;
    const juce::String WindowCaption = "R2D2 Speech Translator";
    juce::Slider volumeSlider;
    juce::Label volumeLabel;
    juce::Slider gateSlider;
    juce::Label gateLabel;
    juce::TextEditor artBox;
    SpectrumLineDrawer drawer;
    SpectrumLineDrawer drawer2;
    //UnifiedVectorsDrawerAlt specialDrawer;
    bool gate = false;
    bool imageReady = false;
    bool stateChanged = true;
    //juce::Random random;
    double currentSampleRate = 0.0, currentAngle = 0.0, angleDelta = 0.0; // [1]
    double currentFrequency = 440.0, targetFrequency = 440.0;
    float oneBlockLength = 1;
    //float volume = 0;
    //float maxSample = 0;
    int gateCounter = 0;
    const float GATE_LENGTH = 0.5f;//gate release length in seconds
    int gateSampleLength;//gate release length in samples
    int lowFreq = 100, highFreq = 4800;

    juce::dsp::FFT forwardFFT;

    std::array<float, fftSize> fifo;                    // [4]
    std::array<float, fftSize * 2> fftData;             // [5]
    std::array<float, fftHalf> fftImageAccumulator;
    std::array<std::array<float, fftHalf>, FFT_IMAGE_SEQ_LENGTH> fftImageSequence;
    const int FFT_IMAGE_LENGTH = 7;
    int fftImageCounter = 0;
    int fftImageSeqCounter = 0;
    int fftImageSpeechCounter = 0;
    float fftImageSpeechIndex = 0;
    int fifoIndex = 0;                                  // [6]
    bool nextFFTBlockReady = false;                     // [7]
    float maxFFTPower = 1.f;
    int maxFrequencyIndex = fftHalf;
    float autoGateMaximum = 0.f;
    int autoGateCounter = AUTOGATE_LENGTH;

    std::vector<float> fftImageFilter;


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
