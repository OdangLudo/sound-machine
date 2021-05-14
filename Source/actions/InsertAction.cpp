#include "InsertAction.h"

#include "CreateTrackAction.h"
#include "CreateProcessorAction.h"

static int getIndexOfFirstCopiedTrackWithSelections(const ValueTree &copiedState) {
    for (const auto &track : copiedState)
        if (track[IDs::selected] || TracksState::trackHasAnySlotSelected(track))
            return copiedState.indexOf(track);

    assert(false); // Copied state, by definition, must have a selection.
}

static bool anyCopiedTrackSelected(const ValueTree &copiedState) {
    for (const ValueTree &track : copiedState)
        if (track[IDs::selected])
            return true;
    return false;
}

static juce::Point<int> limitToTrackAndSlot(juce::Point<int> toTrackAndSlot, const ValueTree &copiedState) {
    return {toTrackAndSlot.x, anyCopiedTrackSelected(copiedState) ? 0 : toTrackAndSlot.y};
}

static std::vector<int> findSelectedNonMasterTrackIndices(const ValueTree &copiedState) {
    std::vector<int> selectedTrackIndices;
    for (const auto &track : copiedState)
        if (track[IDs::selected] && !TracksState::isMasterTrack(track))
            selectedTrackIndices.push_back(copiedState.indexOf(track));
    return selectedTrackIndices;
}

static juce::Point<int> findFromTrackAndSlot(const ValueTree &copiedState) {
    int fromTrackIndex = getIndexOfFirstCopiedTrackWithSelections(copiedState);
    if (anyCopiedTrackSelected(copiedState)) return {fromTrackIndex, 0};

    int fromSlot = INT_MAX;
    for (const auto &track : copiedState) {
        int lowestSelectedSlotForTrack = TracksState::getSlotMask(track).findNextSetBit(0);
        if (lowestSelectedSlotForTrack != -1)
            fromSlot = std::min(fromSlot, lowestSelectedSlotForTrack);
    }

    assert(fromSlot != INT_MAX);
    return {fromTrackIndex, fromSlot};
}

static std::vector<int> findDuplicationIndices(std::vector<int> currentIndices) {
    auto duplicationIndices = currentIndices;
    int previousIndex = -1;
    unsigned long endOfContiguousRange = 0;
    for (unsigned long i = 0; i < duplicationIndices.size(); i++) {
        int currentIndex = currentIndices[i];
        if (previousIndex != -1 && currentIndex - previousIndex > 1)
            endOfContiguousRange = i;
        for (unsigned long j = endOfContiguousRange; j < duplicationIndices.size(); j++)
            duplicationIndices[j] += 1;
        previousIndex = currentIndex;
    }

    return duplicationIndices;
}

InsertAction::InsertAction(bool duplicate, const ValueTree &copiedState, const juce::Point<int> toTrackAndSlot,
                           TracksState &tracks, ConnectionsState &connections, ViewState &view, InputState &input, ProcessorGraph &processorGraph)
        : tracks(tracks), view(view), processorGraph(processorGraph),
          fromTrackAndSlot(findFromTrackAndSlot(copiedState)), toTrackAndSlot(limitToTrackAndSlot(toTrackAndSlot, copiedState)),
          oldFocusedTrackAndSlot(view.getFocusedTrackAndSlot()), newFocusedTrackAndSlot(oldFocusedTrackAndSlot) {
    auto trackAndSlotDiff = this->toTrackAndSlot - this->fromTrackAndSlot;

    if (!duplicate && tracks.getMasterTrack().isValid() && toTrackAndSlot.x == tracks.getNumNonMasterTracks()) {
        // When inserting into master track, only insert the processors of the first track with selections
        copyProcessorsFromTrack(copiedState.getChild(fromTrackAndSlot.x), fromTrackAndSlot.x, tracks.getNumNonMasterTracks(), trackAndSlotDiff.y);
    } else {
        // First pass: insert processors that are selected without their parent track also selected.
        // This is done because adding new tracks changes the track indices relative to their current position.
        for (const auto &copiedTrack : copiedState) {
            if (!copiedTrack[IDs::selected]) {
                int fromTrackIndex = copiedState.indexOf(copiedTrack);
                if (duplicate) {
                    duplicateSelectedProcessors(copiedTrack, copiedState);
                } else if (TracksState::isMasterTrack(copiedTrack)) {
                    // Processors copied from master track can only get inserted into master track.
                    const auto &masterTrack = tracks.getMasterTrack();
                    if (masterTrack.isValid())
                        copyProcessorsFromTrack(copiedTrack, fromTrackIndex, tracks.indexOf(masterTrack), trackAndSlotDiff.y);
                } else {
                    int toTrackIndex = copiedState.indexOf(copiedTrack) + trackAndSlotDiff.x;
                    const auto &lane = TracksState::getProcessorLaneForTrack(copiedTrack);
                    if (lane.getNumChildren() > 0) { // create tracks to make room
                        while (toTrackIndex >= tracks.getNumNonMasterTracks()) {
                            addAndPerformAction(new CreateTrackAction(false, {}, tracks, view));
                        }
                    }
                    if (toTrackIndex < tracks.getNumNonMasterTracks())
                        copyProcessorsFromTrack(copiedTrack, fromTrackIndex, toTrackIndex, trackAndSlotDiff.y);
                }
            }
        }
        // Second pass: insert selected tracks (along with their processors)
        const auto selectedTrackIndices = findSelectedNonMasterTrackIndices(copiedState);
        const auto duplicatedTrackIndices = findDuplicationIndices(selectedTrackIndices);
        for (unsigned long i = 0; i < selectedTrackIndices.size(); i++) {
            int fromTrackIndex = selectedTrackIndices[i];
            const ValueTree &fromTrack = copiedState.getChild(fromTrackIndex);
            if (duplicate) {
                addAndPerformCreateTrackAction(fromTrack, fromTrackIndex, duplicatedTrackIndices[i]);
            } else {
                addAndPerformCreateTrackAction(fromTrack, fromTrackIndex, fromTrackIndex + trackAndSlotDiff.x + 1);
            }
        }
    }

    selectAction = std::make_unique<MoveSelectionsAction>(createActions, tracks, connections, view, input, processorGraph);
    selectAction->setNewFocusedSlot(newFocusedTrackAndSlot);

    // Cleanup
    for (int i = createActions.size() - 1; i >= 0; i--) {
        auto *action = createActions.getUnchecked(i);
        if (auto *createProcessorAction = dynamic_cast<CreateProcessorAction *>(action))
            createProcessorAction->undoTemporary();
        else
            action->undo();
    }
}

bool InsertAction::perform() {
    if (createActions.isEmpty()) return false;

    for (auto *createAction : createActions) {
        createAction->perform();
    }
    selectAction->perform();
    return true;
}

bool InsertAction::undo() {
    if (createActions.isEmpty()) return false;

    selectAction->undo();
    for (int i = createActions.size() - 1; i >= 0; i--) {
        createActions.getUnchecked(i)->undo();
    }
    return true;
}

void InsertAction::duplicateSelectedProcessors(const ValueTree &track, const ValueTree &copiedState) {
    const BigInteger slotsMask = TracksState::getSlotMask(track);
    std::vector<int> selectedSlots;
    for (int slot = 0; slot <= std::min(view.getNumSlotsForTrack(track) - 1, slotsMask.getHighestBit()); slot++)
        if (slotsMask[slot])
            selectedSlots.push_back(slot);

    auto duplicatedSlots = findDuplicationIndices(selectedSlots);
    int trackIndex = copiedState.indexOf(track);
    for (unsigned long i = 0; i < selectedSlots.size(); i++) {
        const ValueTree &processor = TracksState::getProcessorAtSlot(track, selectedSlots[i]);
        addAndPerformCreateProcessorAction(processor, trackIndex, selectedSlots[i], trackIndex, duplicatedSlots[i]);
    }
}

void InsertAction::copyProcessorsFromTrack(const ValueTree &fromTrack, int fromTrackIndex, int toTrackIndex, int slotDiff) {
    const BigInteger slotsMask = TracksState::getSlotMask(fromTrack);
    for (int fromSlot = 0; fromSlot <= slotsMask.getHighestBit(); fromSlot++) {
        if (slotsMask[fromSlot])
            addAndPerformCreateProcessorAction(TracksState::getProcessorAtSlot(fromTrack, fromSlot), fromTrackIndex, fromSlot,
                                               toTrackIndex, fromSlot + slotDiff);
    }
}

void InsertAction::addAndPerformAction(UndoableAction *action) {
    if (auto *createProcessorAction = dynamic_cast<CreateProcessorAction *>(action))
        createProcessorAction->performTemporary();
    else
        action->perform();
    createActions.add(action);
}

void InsertAction::addAndPerformCreateProcessorAction(const ValueTree &processor, int fromTrackIndex, int fromSlot, int toTrackIndex, int toSlot) {
    addAndPerformAction(new CreateProcessorAction(processor.createCopy(), toTrackIndex, toSlot, tracks, view, processorGraph));
    if (oldFocusedTrackAndSlot.x == fromTrackIndex && oldFocusedTrackAndSlot.y == fromSlot)
        newFocusedTrackAndSlot = {toTrackIndex, toSlot};
}

void InsertAction::addAndPerformCreateTrackAction(const ValueTree &track, int fromTrackIndex, int toTrackIndex) {
    addAndPerformAction(new CreateTrackAction(toTrackIndex, false, track, tracks, view));
    // Create track-level processors
    for (const auto &processor : track)
        if (processor.hasType(IDs::PROCESSOR))
            addAndPerformAction(new CreateProcessorAction(processor.createCopy(), toTrackIndex, -1, tracks, view, processorGraph));

    // Create in-lane processors
    for (const auto &processor : TracksState::getProcessorLaneForTrack(track)) {
        int slot = processor[IDs::processorSlot];
        addAndPerformCreateProcessorAction(processor, fromTrackIndex, slot, toTrackIndex, slot);
    }
}

InsertAction::MoveSelectionsAction::MoveSelectionsAction(const OwnedArray<UndoableAction> &createActions, TracksState &tracks, ConnectionsState &connections,
                                                         ViewState &view, InputState &input, ProcessorGraph &processorGraph)
        : SelectAction(tracks, connections, view, input, processorGraph) {
    for (int i = 0; i < newTrackSelections.size(); i++) {
        newTrackSelections.setUnchecked(i, false);
        newSelectedSlotsMasks.setUnchecked(i, BigInteger().toString(2));
    }

    for (auto *createAction : createActions) {
        if (auto *createProcessorAction = dynamic_cast<CreateProcessorAction *>(createAction)) {
            String maskString = newSelectedSlotsMasks.getUnchecked(createProcessorAction->trackIndex);
            BigInteger mask;
            mask.parseString(maskString, 2);
            mask.setBit(createProcessorAction->slot, true);
            newSelectedSlotsMasks.setUnchecked(createProcessorAction->trackIndex, mask.toString(2));
        } else if (auto *createTrackAction = dynamic_cast<CreateTrackAction *>(createAction)) {
            newTrackSelections.setUnchecked(createTrackAction->insertIndex, true);
            const auto &track = tracks.getTrack(createTrackAction->insertIndex);
            newSelectedSlotsMasks.setUnchecked(createTrackAction->insertIndex, tracks.createFullSelectionBitmask(track));
        }
    }
}
