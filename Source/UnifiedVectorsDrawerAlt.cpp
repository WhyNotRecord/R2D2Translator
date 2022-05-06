/*
  ==============================================================================

    UnifiedVectorsDrawerAlt.cpp
    Created: 5 May 2022 1:28:32pm
    Author:  whyno

  ==============================================================================
*/

#include <JuceHeader.h>
#include "UnifiedVectorsDrawerAlt.h"

//==============================================================================
UnifiedVectorsDrawerAlt::UnifiedVectorsDrawerAlt(int range, int averageLenth) : 
    UnifiedVectorsDrawer(range), averageLength(averageLenth) {
    counter = 0;
}

UnifiedVectorsDrawerAlt::~UnifiedVectorsDrawerAlt()
{
}

void UnifiedVectorsDrawerAlt::pushValueAt(int curveIndex, float value) {
    //std::list<float>* curveQueue;
    if (!queues.contains(curveIndex)) {
        //std::list<float>* newQueue = new std::list<float>();
        queues.set(curveIndex, std::list<float>());
        //curveQueue = newQueue;
    }
    if (!curveColours.contains(curveIndex)) {
        generateCurveColor(curveIndex);
    }

    std::list<float> *curveQueue = &queues.getReference(curveIndex);
    curveQueue->push_back(value);
    counter++;
    if (curveQueue->size() == averageLength) {
        float avg = std::accumulate(curveQueue->begin(), curveQueue->end(), 0.f);
        avg /= averageLength;
        drawingImage.setPixelAt(rightHandEdge, (1.f - avg - curveIndex / 5.f) * drawingImage.getHeight(), curveColours[curveIndex]);
        curveQueue->pop_front();
    }
}

/*void UnifiedVectorsDrawerAlt::setCurveColor(int curveIndex, juce::Colour col) {
}*/

void UnifiedVectorsDrawerAlt::moveToNextLine() {
    UnifiedVectorsDrawer::moveToNextLine();
    //prevents from storing last values in new series
    auto it = queues.begin();
    do {
        std::list<float>* curveQueue = &queues.getReference(it.getKey());
        if (counter > 0) {
            counter--;
        }
        else if (curveQueue->size() > 0) {
            curveQueue->pop_front();
        }
    } while (it.next());

}

void UnifiedVectorsDrawerAlt::paint (juce::Graphics& g)
{
    UnifiedVectorsDrawer::paint(g);
}

void UnifiedVectorsDrawerAlt::resized()
{
    // This method is where you should set the bounds of any child
    // components that your component contains..

}
