#include "MainComponent.h"
#include <iterator>

//==============================================================================
MainComponent::MainComponent() : forwardFFT(fftOrder), drawer(fftHalf), drawer2(fftHalf), specialDrawer(64, 10)
{
    // Make sure you set the size of the component after
    // you add any child components.
    addAndMakeVisible(frequencySlider);
    addAndMakeVisible(volumeSlider);
    addAndMakeVisible(gateSlider);
    addAndMakeVisible(drawer);
    addAndMakeVisible(drawer2);
    //addAndMakeVisible(specialDrawer);
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
    gateSlider.setValue(0.1, juce::NotificationType::dontSendNotification);
    setSize(640, 480);

    specialDrawer.setCurveColor(0, juce::Colour::fromHSV(0.0f, 1.f, 1.f, 1.f));
    specialDrawer.setCurveColor(1, juce::Colour::fromHSV(0.15f, 1.f, 1.f, 1.f));
    specialDrawer.setCurveColor(2, juce::Colour::fromHSV(0.3f, 1.f, 1.f, 1.f));
    specialDrawer.setCurveColor(3, juce::Colour::fromHSV(0.45f, 1.f, 1.f, 1.f));
    specialDrawer.setCurveColor(4, juce::Colour::fromHSV(0.6f, 1.f, 1.f, 1.f));

    fftImageAccumulator.fill(0.f);

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
        /*auto setup = deviceManager.getAudioDeviceSetup();
        setup.bufferSize = 512;*/
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
    maxFrequencyIndex = (int)(highFreq * oneBlockLength + 3);//take some extra indices for showing

    //smooth low-pass filter
    fftImageFilter.clear();
    float step = 1.0f / maxFrequencyIndex;
    float y = 0;
    for (int i = 0; i <= maxFrequencyIndex; i++) {
        fftImageFilter.push_back(std::pow(y, 0.25f));
        y += step;
    }
    drawer.setNewRange(maxFrequencyIndex);
    drawer2.setNewRange(maxFrequencyIndex);
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{

    /*auto* device = deviceManager.getCurrentAudioDevice();
    juce::BigInteger activeInputChannels = device->getActiveInputChannels();
    juce::BigInteger activeOutputChannels = device->getActiveOutputChannels();
    auto maxInputChannels = activeInputChannels.getHighestBit() + 1;
    auto maxOutputChannels = activeOutputChannels.getHighestBit() + 1;
    juce::Logger::writeToLog(juce::String(maxInputChannels) + " " + juce::String(maxOutputChannels));*/

    // У програмы три состояния: калибровка (адаптирует гейт под текущий уровень шума), слушание (записывает и анализирует звук)
    // и воспроизведение (озвучивает записанную фразу).
    // Во время прослушивания программа анализирует каждый фрейм и составляет карты по отсчётам мошности спектра
    // Как только ввод закончен, гейт закрывается, и программа переходит в режим воспроизведения.
    // В нём программа синтезирует звук, а составленные при прослушивании кривые выступают в качестве автоматизации параметров синтеза


    switch (currentState)
    {
    case Calibrating:
        if (autoGateCounter < AUTOGATE_LENGTH) {
            auto* inputBuffer = bufferToFill.buffer->getReadPointer(0, bufferToFill.startSample);
            for (int i = 0; i < bufferToFill.numSamples; i++) {
                float curSampleAbs = std::abs(inputBuffer[i]);
                if (curSampleAbs >= autoGateMaximum)
                    autoGateMaximum = curSampleAbs;
            }
            autoGateCounter++;
        }
        else {
            autoGateCounter = 0;
            currentState = Idling;
            stateChanged = true;
            //autoGateMaximum = 0.f;
        }
        break;
    case Speaking:
    {
        float level = (float)volumeSlider.getValue();
        auto* leftBuffer = bufferToFill.buffer->getWritePointer(0, bufferToFill.startSample);
        auto* rightBuffer = bufferToFill.buffer->getWritePointer(1, bufferToFill.startSample);

        //auto localTargetFrequency = (avg / maxSample) * (highFreq - lowFreq) + lowFreq;
        float value = 0.f;
        float max = 1.f;
        if (maxFrequencyIndex < fftHalf) {
            max = fftImageSequence[fftImageSpeechCounter][fftHalf - 1];
        }
        //filtering weak frequencies
        while ((value = fftImageSequence[fftImageSpeechCounter][(int)fftImageSpeechIndex]) < max / 8.f && fftImageSpeechIndex < maxFrequencyIndex) {
            fftImageSpeechIndex++;
        }
        auto localTargetFrequency = juce::jmap(value, 0.f, max, (float) lowFreq, (float) highFreq);
        float curSampleNorm = 0;
        juce::Logger::writeToLog(juce::String(localTargetFrequency));


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

        //fftImageSpeechIndex += (1.5f - value);
        fftImageSpeechIndex += 0.125f;
        if ((int) fftImageSpeechIndex >= maxFrequencyIndex) {
            fftImageSpeechIndex = 0.f;

            fftImageSpeechCounter++;
            if (fftImageSpeechCounter >= fftImageSeqCounter) {
                resetFftImageSequence();
                currentState = Calibrating;
                stateChanged = true;
            }
        }

        break;
    }
    case Idling:
    case Listening:
    default:
    {
        auto* inputBuffer = bufferToFill.buffer->getReadPointer(0, bufferToFill.startSample);
        for (int i = 0; i < bufferToFill.numSamples; i++) {
            float curSample = inputBuffer[i];
            float curSampleAbs = std::abs(curSample);
            if (curSampleAbs >= gateSlider.getValue()) {
                gate = true;
                gateCounter = 0;
                //avg += curSampleAbs;
                //maxSample = juce::jmax(maxSample, curSampleAbs);
                //avgCounter++;
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
            if (gate) {
                pushNextSampleIntoFifo(inputBuffer[i]);
            }
        }
        if (!gate && fifoIndex > 0)
            fillFifoWithZeros();
        break;
    }
    }
    if (currentState == Idling && gate) {
        currentState = Listening;
        stateChanged = true;
    }
    else if (currentState == Listening && !gate) {
        currentState = Speaking;
        stateChanged = true;
    }
    

    //if (bufferToFill.buffer->getNumChannels() > 0)
    //auto* channelData = bufferToFill.buffer->getReadPointer(0, bufferToFill.startSample);

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
    if (stateChanged) {
        processCurrentState();
    }
}

void MainComponent::processCurrentState() {
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
        if (autoGateMaximum > 0.f) {
            gateSlider.setValue(autoGateMaximum * 2.f);
            autoGateMaximum = 0;
        }
    default:
        getParentComponent()->setName(WindowCaption + " - Idling");
        break;
    }
    repaint();
}

void MainComponent::resized()
{
    juce::Rectangle<int> r = getLocalBounds();
    frequencySlider.setBounds(r.removeFromTop(30).reduced(5, 0));
    volumeSlider.setBounds(r.removeFromTop(30).reduced(5, 0));
    gateSlider.setBounds(r.removeFromTop(30).reduced(5, 0));
    drawer.setBounds(r.removeFromTop(r.getHeight() / 2).reduced(5, 5));
    drawer2.setBounds(r.reduced(5, 5));
    //specialDrawer.setBounds(r.reduced(5, 5));
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

void MainComponent::fillFifoWithZeros() noexcept {
    int rest = fftSize - fifoIndex;
    for (int i = 0; i < rest; i++) {
        pushNextSampleIntoFifo(0.f);
    }
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
}

void MainComponent::processNextFFTBlock() {
    forwardFFT.performFrequencyOnlyForwardTransform(fftData.data());
    auto range = juce::FloatVectorOperations::findMinAndMax(fftData.data(), fftSize / 2);
    juce::Logger::writeToLog(juce::String(range.getStart()) + " " + juce::String(range.getEnd()));
    maxFFTPower = juce::jmax(maxFFTPower, range.getEnd());

    float halfMax = range.getEnd() / 2.f;
    //int analCounter = 0;
    //std::vector<float> vect(maxFrequencyIndex + 1);

    for (auto y = 0; y < maxFrequencyIndex; ++y)                                              // [4]
    {
        //auto skewedProportionY = 1.0f - std::exp(std::log((float)y / (float)imageHeight) * 0.2f);
        //auto fftDataIndex = (size_t)juce::jlimit(0, fftSize / 2, (int)(skewedProportionY * fftSize / 2));
        //auto level = juce::jmap(fftData[fftDataIndex], 0.0f, juce::jmax(maxLevel.getEnd(), 1e-5f), 0.0f, 1.0f);
        //auto level = juce::jmap(fftData[y], 0.0f, range.getEnd(), 0.0f, 1.0f);
        float normValue = juce::jmap(fftData[y], range.getStart(), maxFFTPower, 0.0f, 1.0f);
        drawer.pushValueAt(y, normValue); // [5]
        fftImageAccumulator[y] += normValue;
    }
    fftImageCounter++;
    if (fftImageCounter == FFT_IMAGE_LENGTH || fftImageCounter > 0 && !gate) {
        drawer2.clearRemainder();

        float max = 0.f;
        for (auto y = 0; y < maxFrequencyIndex; ++y) {
            fftImageAccumulator[y] = fftImageFilter[y] * juce::jmin(fftImageAccumulator[y] / fftImageCounter, 1.0f);
            drawer2.pushValueAt(y, fftImageAccumulator[y]);
            if (fftImageAccumulator[y] > max)
                max = fftImageAccumulator[y];
        }
        if (maxFrequencyIndex < fftHalf) {
            fftImageAccumulator[fftHalf - 1] = max;
        }
        addNextFftImage();

        fftImageAccumulator.fill(0.f);
        fftImageCounter = 0;
    }

    /*for (int i = 0; i < vect.size(); i++) {
        drawer2.pushValueAt(i, vect[i]);
    }*/
    drawer.moveToNextLine();
    drawer2.moveToNextLine();
    if (!gate) {
        drawer.clearRemainder();
        drawer.moveToNextLine();
        drawer2.clearRemainder();
        drawer2.moveToNextLine();
    }
    /*if (gate) {
        for (int i = 0; i < mainFrequenciesCount; i++) {
            float pow = getValueForFrequency(mainFrequencies[i]);
            specialDrawer.pushValueAt(i, juce::jmap(pow, range.getStart(), maxFFTPower, 0.0f, 1.0f));
        }
    }
    specialDrawer.moveToNextLine();*/
    //evaluateLastBlockMainFrequency();
    maxFFTPower *= 0.95f;//gradually lowers range and recovers sensibility
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
    float mainFreq = mainFreqIndex / oneBlockLength;
    juce::Logger::writeToLog("Main frequency detected: " + juce::String(mainFreq));

    if (gate) {
       float value = mainFreq / highFreq;
       specialDrawer.pushValueAt(0, value);
    }
    specialDrawer.moveToNextLine();
}

float MainComponent::getValueForFrequency(int frequency) {
    float floatIndex = (float) frequency * oneBlockLength;
    int intIndex = (int)floatIndex;
    float remainder = floatIndex - intIndex;
    return fftData[intIndex] * (1.f - remainder) + fftData[intIndex + 1] * remainder;
}

float MainComponent::getValueForFrequencyWide(int frequency) {
    float floatIndex = (float)frequency * oneBlockLength;
    int intIndex = (int)floatIndex;
    float remainder = floatIndex - intIndex;
    float valueForTwo = fftData[intIndex] * (1.f - remainder) + fftData[intIndex + 1] * remainder;
    float valueForFour = valueForTwo * 0.75f + (fftData[intIndex - 1] + fftData[intIndex + 2]) * 0.25f;
    return valueForFour;
}

void MainComponent::addNextFftImage(bool fin) {
    std::copy(std::begin(fftImageAccumulator), std::end(fftImageAccumulator), fftImageSequence[fftImageSeqCounter].begin());
    fftImageSeqCounter++;
    if (fftImageSeqCounter == FFT_IMAGE_SEQ_LENGTH || fin) {
        fftImageSeqCounter = 0;
        gate = false;
        currentState = Speaking;
    }
}

void MainComponent::resetFftImageSequence() {
    fftImageSeqCounter = 0;
    fftImageSpeechCounter = 0;
    fftImageSpeechIndex = 0;
}

