/*
  ==============================================================================

    UnifiedVectorsDrawer.h
    Created: 5 May 2022 12:04:22pm
    Author:  whyno

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/*
*/
class UnifiedVectorsDrawer  : public juce::Component
{
public:
    UnifiedVectorsDrawer(int range);
    ~UnifiedVectorsDrawer() override;

    //==============================================================================
    virtual void pushValueAt(int curveIndex, float value);
    virtual void setCurveColor(int curveIndex, juce::Colour col);
    void generateCurveColor(int curveIndex);
    virtual void moveToNextLine();
    void setNewRange(int value);

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

protected:
    juce::Image drawingImage;
    int rightHandEdge;
    juce::HashMap<int, juce::Colour> curveColours;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UnifiedVectorsDrawer)
};
