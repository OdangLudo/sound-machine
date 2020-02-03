#pragma once

#include "JuceHeader.h"
#include "SelectAction.h"
#include "UpdateAllDefaultConnectionsAction.h"

struct MoveSelectedItemsAction : UndoableAction {
    MoveSelectedItemsAction(juce::Point<int> gridDelta, bool makeInvalidDefaultsIntoCustom,
                            TracksState &tracks, ConnectionsState &connections, ViewState &view,
                            InputState &input, StatefulAudioProcessorContainer &audioProcessorContainer)
            : tracks(tracks), connections(connections), view(view), input(input),
              audioProcessorContainer(audioProcessorContainer),
              gridDelta(gridDelta),
              insertActions(createInsertActions()),
              updateSelectionAction(createUpdateSelectionAction()),
              updateConnectionsAction(createUpdateConnectionsAction(makeInvalidDefaultsIntoCustom)) {
        // cleanup - yeah it's ugly but avoids need for some copy/move madness in createUpdateConnectionsAction
        for (int i = insertActions.size() - 1; i >= 0; i--)
            insertActions.getUnchecked(i)->undo();
    }

    bool perform() override {
        if (insertActions.isEmpty())
            return false;

        for (auto* insertAction : insertActions)
            insertAction->perform();
        updateSelectionAction.perform();
        updateConnectionsAction.perform();
        return true;
    }

    bool undo() override {
        if (insertActions.isEmpty())
            return false;

        for (int i = insertActions.size() - 1; i >= 0; i--)
            insertActions.getUnchecked(i)->undo();
        updateSelectionAction.undo();
        updateConnectionsAction.undo();
        return true;
    }

    int getSizeInUnits() override {
        return (int)sizeof(*this); //xxx should be more accurate
    }

private:

    struct MoveSelectionsAction : public SelectAction {
        MoveSelectionsAction(juce::Point<int> gridDelta,
                             TracksState &tracks, ConnectionsState &connections, ViewState &view,
                             InputState &input, StatefulAudioProcessorContainer &audioProcessorContainer)
                : SelectAction(tracks, connections, view, input, audioProcessorContainer) {
            if (gridDelta.y != 0) {
                for (int i = 0; i < tracks.getNumTracks(); i++) {
                    const auto& track = tracks.getTrack(i);
                    BigInteger selectedSlotsMask;
                    selectedSlotsMask.parseString(track[IDs::selectedSlotsMask].toString(), 2);
                    selectedSlotsMask.shiftBits(gridDelta.y, 0);
                    newSelectedSlotsMasks.setUnchecked(i, selectedSlotsMask.toString(2));
                }
            }
            if (gridDelta.x != 0) {
                auto moveTrackSelections = [&](int fromTrackIndex) {
                    const auto &fromTrack = tracks.getTrack(fromTrackIndex);
                    int toTrackIndex = fromTrackIndex + gridDelta.x;
                    if (toTrackIndex >= 0 && toTrackIndex < newSelectedSlotsMasks.size()) {
                        const auto &toTrack = tracks.getTrack(toTrackIndex);
                        newSelectedSlotsMasks.setUnchecked(toTrackIndex, newSelectedSlotsMasks.getUnchecked(fromTrackIndex));
                        newSelectedSlotsMasks.setUnchecked(fromTrackIndex, BigInteger().toString(2));
                    }
                };
                if (gridDelta.x < 0) {
                    for (int fromTrackIndex = 0; fromTrackIndex < tracks.getNumTracks(); fromTrackIndex++) {
                        moveTrackSelections(fromTrackIndex);
                    }
                } else if (gridDelta.x > 0) {
                    for (int fromTrackIndex = tracks.getNumTracks() - 1; fromTrackIndex >= 0; fromTrackIndex--) {
                        moveTrackSelections(fromTrackIndex);
                    }
                }
            }
            setNewFocusedSlot(oldFocusedSlot + gridDelta, false);
        }
    };

    TracksState &tracks;
    ConnectionsState &connections;
    ViewState &view;
    InputState &input;
    StatefulAudioProcessorContainer &audioProcessorContainer;

    juce::Point<int> gridDelta;
    OwnedArray<InsertProcessorAction> insertActions;
    SelectAction updateSelectionAction;
    UpdateAllDefaultConnectionsAction updateConnectionsAction;

    // TODO limit to borders & mixer channel slots
    // Side effect: this actually does the processor/track moves in preparation for
    // `createUpdateConnectionsAction`, which should be called immediately after this.
    // This avoids an unnecessary `undo` on all insert actions here, followed by
    // a `perform` in `createUpdateConnectionsAction` to find where new default connections will be.
    OwnedArray<InsertProcessorAction> createInsertActions() {
        OwnedArray<InsertProcessorAction> insertActions;
        if (gridDelta.x == 0 && gridDelta.y == 0)
            return insertActions;

        auto addInsertActionsForTrackIndex = [&](int trackIndex) {
            const auto& fromTrack = tracks.getTrack(trackIndex);
            auto toTrack = tracks.getTrack(trackIndex + gridDelta.x);

            auto addInsertActionsForProcessor = [&](const ValueTree &processor) {
                if (tracks.isProcessorSelected(processor)) {
                    auto toSlot = int(processor[IDs::processorSlot]) + gridDelta.y;
                    insertActions.add(new InsertProcessorAction(toTrack, processor, toSlot, tracks, view));
                    // Need to actually _do_ the move for each track, since this could affect the results of
                    // a later track's slot moves. i.e. if gridDelta.x == -1, then we need to move selected processors
                    // out of this track before advancing to the next track (at trackIndex + 1).
                    // (This action is undone later.)
                    insertActions.getLast()->perform();
                }
            };

            if (gridDelta.y <= 0) {
                for (int processorIndex = 0; processorIndex < fromTrack.getNumChildren(); processorIndex++)
                    addInsertActionsForProcessor(fromTrack.getChild(processorIndex));
            } else {
                for (int processorIndex = fromTrack.getNumChildren() - 1; processorIndex >= 0; processorIndex--)
                    addInsertActionsForProcessor(fromTrack.getChild(processorIndex));
            }
        };

        if (gridDelta.x <= 0) {
            for (int trackIndex = 0; trackIndex < tracks.getNumTracks(); trackIndex++)
                addInsertActionsForTrackIndex(trackIndex);
        } else {
            for (int trackIndex = tracks.getNumTracks() - 1; trackIndex >= 0; trackIndex--)
                addInsertActionsForTrackIndex(trackIndex);
        }

        return insertActions;
    }

    // Assumes all insertActions have been performed (see comment above `createInsertActions`)
    // After determining what the new default connections will be, it moves everything back to where it was.
    UpdateAllDefaultConnectionsAction createUpdateConnectionsAction(bool makeInvalidDefaultsIntoCustom) {
        return UpdateAllDefaultConnectionsAction(makeInvalidDefaultsIntoCustom, true, connections, tracks, input,
                                                 audioProcessorContainer, updateSelectionAction.getNewFocusedTrack());
    }

    SelectAction createUpdateSelectionAction() {
        return MoveSelectionsAction(gridDelta, tracks, connections, view, input, audioProcessorContainer);
    }

    JUCE_DECLARE_NON_COPYABLE(MoveSelectedItemsAction)
};
