/*
  ==============================================================================

    UnifiedVectorsDrawerAlt.h
    Created: 5 May 2022 1:28:32pm
    Author:  whyno

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "UnifiedVectorsDrawer.h"
#include <queue>

//==============================================================================
/*
*/
class UnifiedVectorsDrawerAlt  : public UnifiedVectorsDrawer
{
public:
    UnifiedVectorsDrawerAlt(int range, int averageLenth);
    ~UnifiedVectorsDrawerAlt() override;

    //==============================================================================
    void pushValueAt(int curveIndex, float value) override;
    //void setCurveColor(int curveIndex, juce::Colour col) override;
    //void generateCurveColor(int curveIndex);
    void moveToNextLine() override;
    //void setNewRange(int value);

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    int averageLength;
    int counter;
    juce::HashMap<int, std::list<float>> queues;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UnifiedVectorsDrawerAlt)
};
