#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent() : forwardFFT(fftOrder), drawer(fftHalf), specialDrawer(64)
{
    // Make sure you set the size of the component after
    // you add any child components.
    addAndMakeVisible(frequencySlider);
    addAndMakeVisible(volumeSlider);
    addAndMakeVisible(gateSlider);
    addAndMakeVisible(drawer);
    addAndMakeVisible(specialDrawer);
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
    setSize(640, 480);

    specialDrawer.setCurveColor(0, juce::Colour::fromHSV(0.0f, 1.f, 1.f, 1.f));
    specialDrawer.setCurveColor(1, juce::Colour::fromHSV(0.1f, 1.f, 1.f, 1.f));
    specialDrawer.setCurveColor(2, juce::Colour::fromHSV(0.2f, 1.f, 1.f, 1.f));
    specialDrawer.setCurveColor(3, juce::Colour::fromHSV(0.3f, 1.f, 1.f, 1.f));
    specialDrawer.setCurveColor(4, juce::Colour::fromHSV(0.4f, 1.f, 1.f, 1.f));

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
    startTimerHz(60);

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
    currentSampleRate = sampleRate;
    gateSampleLength = GATE_LENGTH * sampleRate;

    updateAngleDelta();

    //determine the largest needed index of FFT result (which corresponds to max frequency)
    oneBlockLength = fftSize / currentSampleRate;
    maxFrequencyIndex = highFreq * oneBlockLength + 3;//take some extra indices for showing
    drawer.setNewRange(maxFrequencyIndex);

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
    //TODO: реализовать вычисление БПФ с небольшим разрешением (скажем 2^8)
    //TODO: по нескольким точкам получившейся последовательности определять поведение синтезатора
    //TODO: например, значение в одной (нескольких) точке определяет переход на новую частоту,
    //TODO: в другой - скорость перехода

    // У програмы три состояния: калибровка (адаптирует гейт под текущий уровень шума), слушание (записывает и анализирует звук)
    // и воспроизведение (озвучивает записанную фразу). Понадобится соответствующий енум
    // Во время прослушивания программа анализирует каждый фрейм и составляет кривые по отсчётам мошности на определённых частотах
    // (скажем, 600, 1200, 3600 Гц). Как только ввод закончен, гейт закрывается, и программа переходит в режим воспроизведения.
    // В нём программа синтезирует звук, а составленные при прослушивании кривые выступают в качестве автоматизации параметров синтеза
    // Долгие переходы между частотами синтезируемого звука должны растягиваться на несколько вызовов обработки буфера
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
            if (gateCounter >= gateSampleLength) {
                gate = false;
            }
            else {
                gateCounter++;
            }
        }
        /*if (gate) {
        }*/
    }

    if (currentState == Idling && gate) {
        currentState = Listening;
    } else if (currentState == Listening && !gate) {
        currentState = Idling;
    }

    switch (currentState)
    {
    case Calibrating:
        break;
    case Listening:
        break;
    case Speaking:
        break;
    case Idling:
    default:
        break;
    }


    float level = (float)volumeSlider.getValue();
    auto* leftBuffer = bufferToFill.buffer->getWritePointer(0, bufferToFill.startSample);
    auto* rightBuffer = bufferToFill.buffer->getWritePointer(1, bufferToFill.startSample);
   
    if (false) {
        //avg /= avgCounter;
        //Array<float> bufferCopy;
        /*juce::Logger::writeToLog("New average sample value is " + juce::String(avg) + 
            ". New max sample value is " + juce::String(maxSample));*/
        //auto localTargetFrequency = (avg / maxSample) * (highFreq - lowFreq) + lowFreq;
        auto localTargetFrequency = targetFrequency;
        //juce::Logger::writeToLog("New target frequency is " + juce::String(localTargetFrequency));
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

    //if (bufferToFill.buffer->getNumChannels() > 0)
    //auto* channelData = bufferToFill.buffer->getReadPointer(0, bufferToFill.startSample);
        for (auto i = 0; i < bufferToFill.numSamples; ++i)
            pushNextSampleIntoFifo(inputBuffer[i]);


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
    switch (currentState)
    {
    case Calibrating:
        getParentComponent()->setName(WindowCaption + " - Calibrating");
        break;
    case Listening:
        getParentComponent()->setName(WindowCaption + " - Listening");
        break;
    case Speaking:
        getParentComponent()->setName(WindowCaption + " - Speaking");
        break;
    case Idling:
    default:
        getParentComponent()->setName(WindowCaption + " - Idling");
        break;
    }
    repaint();//TODO only when the state was changed
    // You can add your drawing code here!
}

void MainComponent::resized()
{
    juce::Rectangle<int> r = getLocalBounds();
    frequencySlider.setBounds(r.removeFromTop(30).reduced(5, 0));
    volumeSlider.setBounds(r.removeFromTop(30).reduced(5, 0));
    gateSlider.setBounds(r.removeFromTop(30).reduced(5, 0));
    drawer.setBounds(r.removeFromTop(r.getHeight() / 2).reduced(5, 5));
    specialDrawer.setBounds(r.reduced(5, 5));
    //oscilator.setBounds(r.reduced(5, 5));
}

void MainComponent::timerCallback() {
    if (nextFFTBlockReady)
    {
        processNextFFTBlock();
        nextFFTBlockReady = false;
        repaint();
    }
}

void MainComponent::updateAngleDelta()
{
    auto cyclesPerSample = currentFrequency / currentSampleRate;         // [2]
    angleDelta = cyclesPerSample * 2.0 * juce::MathConstants<double>::pi;          // [3]
}

void MainComponent::pushNextSampleIntoFifo(float sample) noexcept
{
    // if the fifo contains enough data, set a flag to say
    // that the next line should now be rendered..
    if (fifoIndex == fftSize)       // [8]
    {
        if (!nextFFTBlockReady)    // [9]
        {
            std::fill(fftData.begin(), fftData.end(), 0.0f);
            std::copy(fifo.begin(), fifo.end(), fftData.begin());
            nextFFTBlockReady = true;
        }

        fifoIndex = 0;
    }

    fifo[(size_t)fifoIndex++] = sample; // [9]
}

void MainComponent::processNextFFTBlock() {
    forwardFFT.performFrequencyOnlyForwardTransform(fftData.data());
    auto range = juce::FloatVectorOperations::findMinAndMax(fftData.data(), fftSize / 2);
    juce::Logger::writeToLog(juce::String(range.getStart()) + " " + juce::String(range.getEnd()));
    maxFFTPower = juce::jmax(maxFFTPower, range.getEnd());

    for (auto y = 0; y < maxFrequencyIndex; ++y)                                              // [4]
    {
        //auto skewedProportionY = 1.0f - std::exp(std::log((float)y / (float)imageHeight) * 0.2f);
        //auto fftDataIndex = (size_t)juce::jlimit(0, fftSize / 2, (int)(skewedProportionY * fftSize / 2));
        //auto level = juce::jmap(fftData[fftDataIndex], 0.0f, juce::jmax(maxLevel.getEnd(), 1e-5f), 0.0f, 1.0f);
        //auto level = juce::jmap(fftData[y], 0.0f, range.getEnd(), 0.0f, 1.0f);

        drawer.pushValueAt(y, juce::jmap(fftData[y], range.getStart(), maxFFTPower, 0.0f, 1.0f)); // [5]
    }
    drawer.moveToNextLine();
    if (gate) {
        for (int i = 0; i < mainFrequenciesCount; i++) {
            specialDrawer.pushValueAt(i, getValueForFrequency(mainFrequencies[i]));
        }
    }
    specialDrawer.moveToNextLine();
    evaluateLastBlockMainFrequency();
    maxFFTPower *= 0.99f;
}

void MainComponent::evaluateLastBlockMainFrequency() {
    float mainFreqIndex = 0;
    float maxPower = 0.f;
    for (auto y = 0; y < fftHalf; ++y) {
        if (fftData[y] > maxPower) {
            maxPower = fftData[y];
            mainFreqIndex = y;
        }
    }
    if (mainFreqIndex != 0) {
        if (fftData[mainFreqIndex] * 0.95f < fftData[mainFreqIndex - 1]) {
            mainFreqIndex -= 0.5f;
        } else if (fftData[mainFreqIndex] * 0.95f < fftData[mainFreqIndex + 1]) {
            mainFreqIndex += 0.5f;
        }
    }

    //float T = fftSize / currentSampleRate;
    juce::Logger::writeToLog("Main frequency detected: " + juce::String(mainFreqIndex / oneBlockLength));

}

float MainComponent::getValueForFrequency(int frequency) {
    float floatIndex = (float) frequency * oneBlockLength;
    int intIndex = (int)floatIndex;
    float remainder = floatIndex - intIndex;
    return fftData[intIndex] * (1.f - remainder) + fftData[intIndex + 1] * remainder;
}
