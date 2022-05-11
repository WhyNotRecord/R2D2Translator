#include "MainComponent.h"
#include <iterator>
#include "Const.h"

//==============================================================================
MainComponent::MainComponent() : forwardFFT(fftOrder), drawer(fftHalf, 512), drawer2(fftHalf, 512), gateSampleLength(1024)//, specialDrawer(64, 10)
{
    // Make sure you set the size of the component after
    // you add any child components.
    gateLabel.setText("Gate", juce::NotificationType::dontSendNotification);
    //gateLabel.attachToComponent(&gateSlider, true);
    volumeLabel.setText("Volume", juce::NotificationType::dontSendNotification);
    //volumeLabel.attachToComponent(&volumeSlider, true);

    addAndMakeVisible(volumeSlider);
    addAndMakeVisible(volumeLabel);
    addAndMakeVisible(gateSlider);
    addAndMakeVisible(gateLabel);
    addAndMakeVisible(drawer);
    addAndMakeVisible(drawer2);
    addAndMakeVisible(artBox);
    //addAndMakeVisible(specialDrawer);
    //addAndMakeVisible (oscilator);
    volumeSlider.setRange(0, 0.5);
    gateSlider.setRange(0, 0.5);
    volumeSlider.setSkewFactorFromMidPoint(0.1); // [4]
    volumeSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, volumeSlider.getTextBoxWidth(), volumeSlider.getTextBoxHeight());
    gateSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, gateSlider.getTextBoxWidth(), gateSlider.getTextBoxHeight());
    volumeSlider.setValue(0.1, juce::NotificationType::dontSendNotification);
    gateSlider.setValue(0.1, juce::NotificationType::dontSendNotification);
    
    artBox.setMultiLine(true, false);
    artBox.setReadOnly(true);
    juce::MemoryOutputStream decodedStream;
    if (juce::Base64::convertFromBase64(decodedStream, R2D2Art)) {
        artBox.setText(decodedStream.toString());
    }
    artBox.setColour(juce::TextEditor::backgroundColourId, juce::Colours::ghostwhite);
    artBox.setColour(juce::TextEditor::textColourId, juce::Colours::darkblue);
    artBox.applyFontToAllText(juce::Font("Courier New", 4, 0), true);

    setSize(920, 520);

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
        setAudioChannels (1, 2);
    }
    auto setup = deviceManager.getAudioDeviceSetup();
    setup.bufferSize = 1024;
    juce::Logger::writeToLog(juce::String(setup.bufferSize));

    startTimerHz(60);
}

MainComponent::~MainComponent()
{
    // This shuts down the audio device and clears the audio source.
    shutdownAudio();
}

//==============================================================================
void MainComponent::prepareToPlay (int, double sampleRate)
{
    // This function will be called when the audio device is started, or when
    // its settings (i.e. sample rate, block size, etc) are changed.

    // You can use this function to initialise any resources you might need,
    // but be careful - it will be called on the audio thread, not the GUI thread.

    auto* device = deviceManager.getCurrentAudioDevice();
    juce::BigInteger activeInputChannels = device->getActiveInputChannels();
    juce::BigInteger activeOutputChannels = device->getActiveOutputChannels();
    auto maxInputChannels = activeInputChannels.getHighestBit() + 1;
    auto maxOutputChannels = activeOutputChannels.getHighestBit() + 1;
    if (maxInputChannels < 1 || maxOutputChannels < 2) {
        juce::Logger::writeToLog("At least 1 input and 2 output channels required");
        currentState = Error;
    }

    currentSampleRate = sampleRate;
    gateSampleLength = (int) (GATE_LENGTH * sampleRate);

    updateAngleDelta();

    //determine the largest needed index of FFT result (which corresponds to max frequency)
    oneBlockLength = (float)(fftSize / currentSampleRate);
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

    /*juce::Logger::writeToLog(juce::String(maxInputChannels) + " " + juce::String(maxOutputChannels));*/

    // У програмы три основных состояния: калибровка (адаптирует гейт под текущий уровень шума), слушание (записывает и анализирует звук)
    // и воспроизведение (озвучивает записанную фразу).
    // Во время прослушивания программа анализирует каждый фрейм и составляет карты по отсчётам мошности спектра
    // Как только ввод закончен, гейт закрывается, и программа переходит в режим воспроизведения.
    // В нём программа синтезирует звук, а составленные при прослушивании кривые выступают в качестве автоматизации параметров синтеза

    switch (currentState)
    {
    case Calibrating:
        if (autoGateCounter > 0) {
            auto* inputBuffer = bufferToFill.buffer->getReadPointer(0, bufferToFill.startSample);
            for (int i = 0; i < bufferToFill.numSamples; i++) {
                float curSampleAbs = std::abs(inputBuffer[i]);
                if (curSampleAbs >= autoGateMaximum)
                    autoGateMaximum = curSampleAbs;
            }
            autoGateCounter--;
        }
        else {
            autoGateCounter = AUTOGATE_LENGTH / 2;
            currentState = Idling;
            stateChanged = true;
            //autoGateMaximum = 0.f;
        }
        bufferToFill.clearActiveBufferRegion();

        break;
    case Speaking:
    {
        if (!imageReady) {
            bufferToFill.clearActiveBufferRegion();
            break;
        }
        float level = (float)volumeSlider.getValue();
        auto* leftBuffer = bufferToFill.buffer->getWritePointer(0, bufferToFill.startSample);
        auto* rightBuffer = bufferToFill.buffer->getWritePointer(1, bufferToFill.startSample);

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
        //juce::Logger::writeToLog(juce::String(localTargetFrequency));


        if (localTargetFrequency != currentFrequency) {
            auto frequencyIncrement = (localTargetFrequency - currentFrequency) / bufferToFill.numSamples;
            //juce::Logger::writeToLog("Change branch");
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
            //juce::Logger::writeToLog("Const branch");
            for (auto sample = 0; sample < bufferToFill.numSamples; ++sample)
            {
                auto currentSample = (float)std::sin(currentAngle);
                currentAngle += angleDelta;
                curSampleNorm = currentSample * level;
                leftBuffer[sample] = curSampleNorm;
                rightBuffer[sample] = curSampleNorm;
            }
        }

        fftImageSpeechIndex += 0.125f;
        if ((int) fftImageSpeechIndex >= maxFrequencyIndex) {
            fftImageSpeechIndex = 0.f;

            fftImageSpeechCounter++;
            if (fftImageSpeechCounter >= fftImageSeqCounter) {
                resetFftImageSequence();
                currentState = FadingOut;
                imageReady = false;
                stateChanged = true;
            }
        }

        break;
    }
    case FadingOut:
    {
        float level = (float)volumeSlider.getValue();
        auto* leftBuffer = bufferToFill.buffer->getWritePointer(0, bufferToFill.startSample);
        auto* rightBuffer = bufferToFill.buffer->getWritePointer(1, bufferToFill.startSample);
        float curSampleNorm = 0.f;
        for (auto sample = 0; sample < bufferToFill.numSamples; ++sample)
        {
            auto currentSample = (float)std::sin(currentAngle);
            currentAngle += angleDelta;
            curSampleNorm = currentSample * level * std::pow(0.99f, (float) sample);
            //juce::Logger::writeToLog("Fading " + juce::String(curSampleNorm));
            leftBuffer[sample] = curSampleNorm;
            rightBuffer[sample] = curSampleNorm;
        }
        leftBuffer[bufferToFill.numSamples - 1] = 0.f;
        rightBuffer[bufferToFill.numSamples - 1] = 0.f;

        currentState = Calibrating;
        stateChanged = true;

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
        bufferToFill.clearActiveBufferRegion();
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
    case FadingOut:
        getParentComponent()->setName(WindowCaption + " - Speaking");
        break;
    case Error:
        getParentComponent()->setName(WindowCaption + " - Error occured (see log)");
        break;
    case Idling:
        if (autoGateMaximum > 0.f) {
            gateSlider.setValue(juce::jmax(autoGateMaximum * 1.5f, 0.1f));
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
    //frequencySlider.setBounds(r.removeFromTop(30).reduced(5, 0));
    juce::Rectangle<int> firstRow = r.removeFromTop(30);
    volumeLabel.setBounds(firstRow.removeFromLeft(50));
    volumeSlider.setBounds(firstRow.reduced(5, 0));
    juce::Rectangle<int> secondRow = r.removeFromTop(30);
    gateLabel.setBounds(secondRow.removeFromLeft(50));
    gateSlider.setBounds(secondRow.reduced(5, 0));
    artBox.setBounds(r.removeFromLeft(r.getWidth() / 2).reduced(5, 5));
    drawer.setBounds(r.removeFromTop(r.getHeight() / 2).reduced(5, 5));
    drawer2.setBounds(r.reduced(5, 5));
    //specialDrawer.setBounds(r.reduced(5, 5));
    //oscilator.setBounds(r.reduced(5, 5));
}

void MainComponent::timerCallback() {
    if (stateChanged) {
        juce::Logger::writeToLog("State changed: " + juce::String(currentState));
        processCurrentState();
        stateChanged = false;
    }

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
    juce::Logger::writeToLog("fillFifoWithZeros");
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
    juce::Logger::writeToLog("processNextFFTBlock");
    forwardFFT.performFrequencyOnlyForwardTransform(fftData.data());
    auto range = juce::FloatVectorOperations::findMinAndMax(fftData.data(), fftSize / 2);
    //juce::Logger::writeToLog(juce::String(range.getStart()) + " " + juce::String(range.getEnd()));
    maxFFTPower = juce::jmax(maxFFTPower, range.getEnd());

    //float halfMax = range.getEnd() / 2.f;

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

        //juce::Logger::writeToLog("Image preparing");
        float max = 0.f;
        for (auto y = 0; y < maxFrequencyIndex; ++y) {
            fftImageAccumulator[y] = fftImageFilter[y] * juce::jmin(fftImageAccumulator[y] / fftImageCounter, 1.0f);
            drawer2.pushValueAt(y, fftImageAccumulator[y] * 5.f);
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

float MainComponent::evaluateLastBlockMainFrequency() {
    float mainFreqIndex = 0;
    float maxPower = 0.f;
    for (auto y = 0; y < fftHalf; ++y) {
        if (fftData[y] > maxPower) {
            maxPower = fftData[y];
            mainFreqIndex = (float) y;
        }
    }
    if (mainFreqIndex != 0) {
        if (fftData[mainFreqIndex] * 0.95f < fftData[mainFreqIndex - 1]) {
            mainFreqIndex -= 0.5f;
        } else if (fftData[mainFreqIndex] * 0.95f < fftData[mainFreqIndex + 1]) {
            mainFreqIndex += 0.5f;
        }
    }

    float mainFreq = mainFreqIndex / oneBlockLength;
    juce::Logger::writeToLog("Main frequency detected: " + juce::String(mainFreq));
    return mainFreq;
}

float MainComponent::getValueForFrequency(int frequency) {
    float floatIndex = (float) frequency * oneBlockLength;
    int intIndex = (int)floatIndex;
    float remainder = floatIndex - intIndex;
    return fftData[intIndex] * (1.f - remainder) + fftData[intIndex + 1] * remainder;
}

void MainComponent::addNextFftImage(bool fin) {
    juce::Logger::writeToLog("addNextFftImage");
    std::copy(std::begin(fftImageAccumulator), std::end(fftImageAccumulator), fftImageSequence[fftImageSeqCounter].begin());
    fftImageSeqCounter++;
    imageReady = true;
    if (fftImageSeqCounter == FFT_IMAGE_SEQ_LENGTH || fin) {
        fftImageSeqCounter = 0;
        gate = false;
        currentState = Speaking;
    }
}

void MainComponent::resetFftImageSequence() {
    juce::Logger::writeToLog("resetFftImageSequence");
    fftImageSeqCounter = 0;
    fftImageSpeechCounter = 0;
    fftImageSpeechIndex = 0;
}

