#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent()
{
    // Make sure you set the size of the component after
    // you add any child components.
    addAndMakeVisible(frequencySlider);
    addAndMakeVisible(volumeSlider);
    addAndMakeVisible(gateSlider);
    //addAndMakeVisible (oscilator);
    frequencySlider.setRange(lowFreq, highFreq);
    volumeSlider.setRange(0, 0.5);
    gateSlider.setRange(0, 0.20);
    frequencySlider.setSkewFactorFromMidPoint(880.0); // [4]
    volumeSlider.setSkewFactorFromMidPoint(0.1); // [4]
    frequencySlider.onValueChange = [this]
    {
        targetFrequency = frequencySlider.getValue();
        std::cout << targetFrequency;
    };
    frequencySlider.setValue(currentFrequency, juce::NotificationType::dontSendNotification);
    volumeSlider.setValue(0.1, juce::NotificationType::dontSendNotification);
    gateSlider.setValue(0.075, juce::NotificationType::dontSendNotification);
    setSize(600, 100);

    // Some platforms require permissions to open input channels so request that here
    if (juce::RuntimePermissions::isRequired (juce::RuntimePermissions::recordAudio)
        && ! juce::RuntimePermissions::isGranted (juce::RuntimePermissions::recordAudio))
    {
        juce::RuntimePermissions::request (juce::RuntimePermissions::recordAudio,
                                           [&] (bool granted) { setAudioChannels (granted ? 1 : 0, 2); });
    }
    else
    {
        // Specify the number of input and output channels that we want to open
        setAudioChannels (1, 2);
    }
}

MainComponent::~MainComponent()
{
    // This shuts down the audio device and clears the audio source.
    shutdownAudio();
}

//==============================================================================
void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    // This function will be called when the audio device is started, or when
    // its settings (i.e. sample rate, block size, etc) are changed.

    // You can use this function to initialise any resources you might need,
    // but be careful - it will be called on the audio thread, not the GUI thread.

    // For more details, see the help for AudioProcessor::prepareToPlay()
    currentSampleRate = sampleRate;
    updateAngleDelta();

}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{

    /*auto* device = deviceManager.getCurrentAudioDevice();
    juce::BigInteger activeInputChannels = device->getActiveInputChannels();
    juce::BigInteger activeOutputChannels = device->getActiveOutputChannels();
    auto maxInputChannels = activeInputChannels.getHighestBit() + 1;
    auto maxOutputChannels = activeOutputChannels.getHighestBit() + 1;
    juce::Logger::writeToLog(juce::String(maxInputChannels) + " " + juce::String(maxOutputChannels));*/

    //TODO: нормализовать
    //TODO: реализовать вычисление Ѕѕ‘ с небольшим разрешением (скажем 2^8)
    //TODO: по нескольким точкам получившейс€ последовательности определ€ть поведение синтезатора
    //TODO: например, значение в одной (нескольких) точке определ€ет переход на новую частоту,
    //TODO: в другой - скорость перехода

    // ” програмы три состо€ни€: калибровка (адаптирует гейт под текущий уровень шума), слушание (записывает и анализирует звук)
    // и воспроизведение (озвучивает записанную фразу). ѕонадобитс€ соответствующий енум
    // ¬о врем€ прослушивани€ программа анализирует каждый фрейм и составл€ет кривые по отсчЄтам мошности на определЄнных частотах
    // (скажем, 600, 1200, 4800 √ц).  ак только ввод закончен гейт закрываетс€ и программа переходит в режим воспроизведени€.
    // ¬ нЄм программа синтезирует звук, а составленные при прослушивании кривые выступают в качестве автоматизации параметров синтеза
    // ƒолгие переходы между частотами синтезируемого звука должны раст€гиватьс€ на несколько вызовов обработки буфера

    float level = (float)volumeSlider.getValue();
    auto* leftBuffer = bufferToFill.buffer->getWritePointer(0, bufferToFill.startSample);
    auto* rightBuffer = bufferToFill.buffer->getWritePointer(1, bufferToFill.startSample);
    auto* inputBuffer = bufferToFill.buffer->getReadPointer(0, bufferToFill.startSample);

    //primitive gate
    float avg = 0;
    int avgCounter = 0;
    for (int i = 0; i < bufferToFill.numSamples; i++) {
        float curSample = inputBuffer[i];
        float curSampleAbs = std::abs(curSample);
        if (curSampleAbs >= gateSlider.getValue()) {
            gate = true;
            gateCounter = 0;
            avg += curSampleAbs;
            maxSample = juce::jmax(maxSample, curSampleAbs);
            avgCounter++;
        }
        else
        {
            if (gateCounter >= GATE_LENGTH) {
                gate = false;
            }
            else {
                gateCounter++;
            }
        }
        /*if (gate) {
            leftBuffer[i] = curSample * level;
            rightBuffer[i] = curSample * level;
        }*/
    }
    if (avgCounter != 0) {
        avg /= avgCounter;
        //juce::Logger::writeToLog(juce::String(gate ? "open" : "closed"));
        //Array<float> bufferCopy;
        //auto localTargetFrequency = targetFrequency;
        juce::Logger::writeToLog("New average sample value is " + juce::String(avg) + 
            ". New max sample value is " + juce::String(maxSample));
        auto localTargetFrequency = (avg / maxSample) * (highFreq - lowFreq) + lowFreq;
        juce::Logger::writeToLog("New target frequency is " + juce::String(localTargetFrequency));
        float curSampleNorm = 0;


        if (localTargetFrequency != currentFrequency) {
            auto frequencyIncrement = (localTargetFrequency - currentFrequency) / bufferToFill.numSamples;
            //Logger::writeToLog("Change branch");
            for (auto sample = 0; sample < bufferToFill.numSamples; ++sample)
            {
                auto currentSample = (float)std::sin(currentAngle);
                currentFrequency += frequencyIncrement;                                                       // [9]
                updateAngleDelta();                                                                           // [10]
                currentAngle += angleDelta;
                curSampleNorm = currentSample * level;
                leftBuffer[sample] = curSampleNorm;
                rightBuffer[sample] = curSampleNorm;
            }

            currentFrequency = localTargetFrequency;
        }
        else {
            //Logger::writeToLog("Const branch");
            for (auto sample = 0; sample < bufferToFill.numSamples; ++sample)
            {
                auto currentSample = (float)std::sin(currentAngle);
                currentAngle += angleDelta;
                curSampleNorm = currentSample * level;
                leftBuffer[sample] = curSampleNorm;
                rightBuffer[sample] = curSampleNorm;
                //bufferCopy.add(curSample);
            }
        }
    }
    /*bufferToFill.buffer->clear(0, bufferToFill.startSample, bufferToFill.numSamples);
    bufferToFill.buffer->clear(1, bufferToFill.startSample, bufferToFill.numSamples);*/
    /*for (int i = 0; i < bufferToFill.numSamples; i++) {
        float curSample = inputBuffer[i];
        if (avg >= gateSlider.getValue()) {
            leftBuffer[i] = curSample * level;
            rightBuffer[i] = curSample * level;
        }
        else {
            avg = juce::jmax<float>(curSample, avg);
            leftBuffer[i] = 0;
            rightBuffer[i] = 0;
        }
    }*/
    //avg /= bufferToFill.numSamples;

    /*if (random.nextFloat() < 0.10f) {
        targetFrequency = random.nextInt(juce::Range<int>(lowFreq, highFreq));
        juce::Logger::writeToLog(juce::String(currentFrequency) + " " + juce::String(localTargetFrequency) + " " + juce::String(targetFrequency));
    }*/

    // Right now we are not producing any data, in which case we need to clear the buffer
    // (to prevent the output of random noise)
    /*if (!gate) {
        bufferToFill.clearActiveBufferRegion();
    }*/
}

void MainComponent::releaseResources()
{
    // This will be called when the audio device stops, or when it is being
    // restarted due to a setting change.

    // For more details, see the help for AudioProcessor::releaseResources()
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    // You can add your drawing code here!
}

void MainComponent::resized()
{
    juce::Rectangle<int> r = getLocalBounds();
    frequencySlider.setBounds(r.removeFromTop(30).reduced(5, 0));
    volumeSlider.setBounds(r.removeFromTop(30).reduced(5, 0));
    gateSlider.setBounds(r.removeFromTop(30).reduced(5, 0));
    //oscilator.setBounds(r.reduced(5, 5));
}

void MainComponent::updateAngleDelta()
{
    auto cyclesPerSample = currentFrequency / currentSampleRate;         // [2]
    angleDelta = cyclesPerSample * 2.0 * juce::MathConstants<double>::pi;          // [3]
}
