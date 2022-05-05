/*
  ==============================================================================

    SpectrumLineDrawer.h
    Created: 4 May 2022 3:45:23pm
    Author:  whyno

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class SpectrumLineDrawer : public juce::Component {
public:
    SpectrumLineDrawer(int range);

    //==============================================================================
    void pushValueAt(int index, float value);
    void moveToNextLine();
    void setNewRange(int value);
    //==============================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::Image spectrogramImage;
    int rightHandEdge;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumLineDrawer)
};