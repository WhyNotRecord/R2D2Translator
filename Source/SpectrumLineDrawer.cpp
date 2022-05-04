/*
  ==============================================================================

    SpectrumLineDrawer.cpp
    Created: 4 May 2022 3:45:23pm
    Author:  whyno

  ==============================================================================
*/

#include "SpectrumLineDrawer.h"

SpectrumLineDrawer::SpectrumLineDrawer(int range) :
    spectrogramImage(juce::Image::RGB, 1024, range, true) {
    rightHandEdge = spectrogramImage.getWidth() - 1;
}

void SpectrumLineDrawer::pushValueAt(int index, float value) {
    spectrogramImage.setPixelAt(rightHandEdge, index, juce::Colour::fromHSV(0.0f, 1.0f, value, 1.0f)); // [5]
}

void SpectrumLineDrawer::moveToNextLine() {
    spectrogramImage.moveImageSection(0, 0, 1, 0, rightHandEdge, spectrogramImage.getHeight());
}

void SpectrumLineDrawer::setNewRange(int value) {
    spectrogramImage = juce::Image(juce::Image::RGB, 1024, value, true);
}

void SpectrumLineDrawer::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::black);

    g.setOpacity(1.0f);
    g.drawImage(spectrogramImage, getLocalBounds().toFloat());
}

void SpectrumLineDrawer::resized()
{
}