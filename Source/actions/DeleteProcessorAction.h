#pragma once

#include <state/ConnectionsState.h>
#include <state/TracksState.h>

#include "JuceHeader.h"
#include "DisconnectProcessorAction.h"

struct DeleteProcessorAction : public UndoableAction {
    DeleteProcessorAction(const ValueTree &processorToDelete, TracksState &tracks, ConnectionsState &connections,
                          StatefulAudioProcessorContainer &audioProcessorContainer)
            : parentTrack(processorToDelete.getParent()), processorToDelete(processorToDelete),
              processorIndex(parentTrack.indexOf(processorToDelete)),
              disconnectProcessorAction(DisconnectProcessorAction(connections, processorToDelete, all, true, true, true, true)),
              audioProcessorContainer(audioProcessorContainer) {}

    bool perform() override {
        disconnectProcessorAction.perform();
        parentTrack.removeChild(processorToDelete, nullptr);
        audioProcessorContainer.onProcessorDestroyed(processorToDelete);
        return true;
    }

    bool undo() override {
        parentTrack.addChild(processorToDelete, processorIndex, nullptr);
        audioProcessorContainer.onProcessorCreated(processorToDelete);
        disconnectProcessorAction.undo();

        return true;
    }

    int getSizeInUnits() override {
        return (int) sizeof(*this); //xxx should be more accurate
    }

private:
    ValueTree parentTrack;
    ValueTree processorToDelete;
    int processorIndex;
    DisconnectProcessorAction disconnectProcessorAction;

    StatefulAudioProcessorContainer &audioProcessorContainer;

    JUCE_DECLARE_NON_COPYABLE(DeleteProcessorAction)
};
