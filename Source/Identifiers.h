#pragma once

#include "JuceHeader.h"

namespace IDs
{
    #define DECLARE_ID(name) const juce::Identifier name (#name);

    DECLARE_ID (TREE)
    DECLARE_ID (pi)

    DECLARE_ID(PARAM)
    DECLARE_ID(value)
    DECLARE_ID(id)

    DECLARE_ID(TRACKS)
    DECLARE_ID(INPUT)
    DECLARE_ID(OUTPUT)

    DECLARE_ID(MASTER_TRACK)

    DECLARE_ID(freq_1)

    DECLARE_ID (PROJECT)
    DECLARE_ID (uuid)
    DECLARE_ID (mediaId)

    DECLARE_ID (TRACK)
    DECLARE_ID (colour)
    DECLARE_ID (name)
    DECLARE_ID (image)
    DECLARE_ID (selected)

    DECLARE_ID(PROCESSOR)
    DECLARE_ID(PROCESSOR_SLOT)
    DECLARE_ID(NODE_ID)

    DECLARE_ID (CLIP)
    DECLARE_ID (start)
    DECLARE_ID (length)
    DECLARE_ID (timestretchOptions)
    DECLARE_ID (NOTE)

    DECLARE_ID(PARAM_NA)

    DECLARE_ID(CONNECTIONS)
    DECLARE_ID(CONNECTION)
    DECLARE_ID(SOURCE)
    DECLARE_ID(DESTINATION)
    DECLARE_ID(CHANNEL)

    #undef DECLARE_ID
}
