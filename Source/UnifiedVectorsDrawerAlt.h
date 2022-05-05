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

//==============================================================================
/*
*/
class UnifiedVectorsDrawerAlt  : public UnifiedVectorsDrawer
{
public:
    UnifiedVectorsDrawerAlt(int range);
    ~UnifiedVectorsDrawerAlt() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UnifiedVectorsDrawerAlt)
};
