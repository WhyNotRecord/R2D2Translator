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
UnifiedVectorsDrawerAlt::UnifiedVectorsDrawerAlt(int range) : UnifiedVectorsDrawer(range)
{
    // In your constructor, you should add any child components, and
    // initialise any special settings that your component needs.

}

UnifiedVectorsDrawerAlt::~UnifiedVectorsDrawerAlt()
{
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
