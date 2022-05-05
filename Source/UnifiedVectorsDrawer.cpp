/*
  ==============================================================================

    UnifiedVectorsDrawer.cpp
    Created: 5 May 2022 12:04:22pm
    Author:  whyno

  ==============================================================================
*/

#include <JuceHeader.h>
#include "UnifiedVectorsDrawer.h"

//==============================================================================
UnifiedVectorsDrawer::UnifiedVectorsDrawer(int range) : 
    drawingImage(juce::Image::RGB, 1024, range, true) {
    rightHandEdge = drawingImage.getWidth() - 1;


}

UnifiedVectorsDrawer::~UnifiedVectorsDrawer()
{
}

void UnifiedVectorsDrawer::pushValueAt(int curveIndex, float value) {
    if (!curveColours.contains(curveIndex)) {
        generateCurveColor(curveIndex);
    }
    drawingImage.setPixelAt(rightHandEdge, (1.f - value) * drawingImage.getHeight(), curveColours[curveIndex]); 
}

void UnifiedVectorsDrawer::setCurveColor(int curveIndex, juce::Colour col) {
    curveColours.set(curveIndex, col);
}

void UnifiedVectorsDrawer::generateCurveColor(int curveIndex) {
    juce::Random r = juce::Random::getSystemRandom();
    curveColours.set(curveIndex, juce::Colour::fromHSV(r.nextFloat(), r.nextFloat() / 2.f + 0.5f, 1.f, 1.f));
}

void UnifiedVectorsDrawer::moveToNextLine() {
    drawingImage.moveImageSection(0, 0, 1, 0, rightHandEdge, drawingImage.getHeight());
    drawingImage.clear(juce::Rectangle<int>(rightHandEdge, 0, 1, drawingImage.getHeight()));
}

void UnifiedVectorsDrawer::setNewRange(int value) {
    drawingImage = juce::Image(juce::Image::RGB, 1024, value, true);
}

void UnifiedVectorsDrawer::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));   // clear the background

    g.setOpacity(1.0f);
    g.drawImage(drawingImage, getLocalBounds().toFloat());

}

void UnifiedVectorsDrawer::resized()
{
    // This method is where you should set the bounds of any child
    // components that your component contains..

}
