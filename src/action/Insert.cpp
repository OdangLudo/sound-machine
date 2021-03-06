#include "Insert.h"

#include "CreateTrack.h"
#include "CreateProcessor.h"

static int getIndexOfFirstCopiedTrackWithSelections(const OwnedArray<Track> &copiedTracks) {
    for (const auto *track : copiedTracks)
        if (track->isSelected() || track->hasAnySlotSelected())
            return copiedTracks.indexOf(track);
    assert(false); // Copied state must have a selection.
}

static bool anyCopiedTrackSelected(const OwnedArray<Track> &copiedTracks) {
    for (const auto *track : copiedTracks)
        if (track->isSelected())
            return true;
    return false;
}

static juce::Point<int> limitToTrackAndSlot(juce::Point<int> toTrackAndSlot, const OwnedArray<Track> &copiedTracks) {
    return {toTrackAndSlot.x, anyCopiedTrackSelected(copiedTracks) ? 0 : toTrackAndSlot.y};
}

static std::vector<int> findSelectedNonMasterTrackIndices(const OwnedArray<Track> &copiedTracks) {
    std::vector<int> selectedTrackIndices;
    for (auto *track : copiedTracks)
        if (track->isSelected() && !track->isMaster())
            selectedTrackIndices.push_back(copiedTracks.indexOf(track));
    return selectedTrackIndices;
}

static juce::Point<int> findFromTrackAndSlot(const OwnedArray<Track> &copiedTracks) {
    int fromTrackIndex = getIndexOfFirstCopiedTrackWithSelections(copiedTracks);
    if (anyCopiedTrackSelected(copiedTracks)) return {fromTrackIndex, 0};

    int fromSlot = INT_MAX;
    for (const auto *track : copiedTracks) {
        int lowestSelectedSlotForTrack = track->getSlotMask().findNextSetBit(0);
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

Insert::Insert(bool duplicate, const OwnedArray<Track> &copiedTracks, const juce::Point<int> toTrackAndSlot,
               Tracks &tracks, Connections &connections, View &view, Input &input, AllProcessors &allProcessors, ProcessorGraph &processorGraph)
        : tracks(tracks), view(view), allProcessors(allProcessors), processorGraph(processorGraph),
          fromTrackAndSlot(findFromTrackAndSlot(copiedTracks)), toTrackAndSlot(limitToTrackAndSlot(toTrackAndSlot, copiedTracks)),
          oldFocusedTrackAndSlot(view.getFocusedTrackAndSlot()), newFocusedTrackAndSlot(oldFocusedTrackAndSlot) {
    auto trackAndSlotDiff = this->toTrackAndSlot - this->fromTrackAndSlot;

    if (!duplicate && tracks.getMasterTrack() != nullptr && toTrackAndSlot.x == tracks.getNumNonMasterTracks()) {
        // When inserting into master track, only insert the processors of the first track with selections
        copyProcessorsFromTrack(copiedTracks[fromTrackAndSlot.x], fromTrackAndSlot.x, tracks.getNumNonMasterTracks(), trackAndSlotDiff.y);
    } else {
        // First pass: insert processors that are selected without their parent track also selected.
        // This is done because adding new tracks changes the track indices relative to their current position.
        for (const auto *copiedTrack : copiedTracks) {
            if (copiedTrack->isSelected()) continue;

            int fromTrackIndex = copiedTracks.indexOf(copiedTrack);
            if (duplicate) {
                duplicateSelectedProcessors(copiedTrack, copiedTracks);
            } else if (copiedTrack->isMaster()) {
                // Processors copied from master track can only get inserted into master track.
                auto *masterTrack = tracks.getMasterTrack();
                if (masterTrack != nullptr)
                    copyProcessorsFromTrack(copiedTrack, fromTrackIndex, tracks.indexOf(masterTrack), trackAndSlotDiff.y);
            } else {
                int toTrackIndex = copiedTracks.indexOf(copiedTrack) + trackAndSlotDiff.x;
                const auto *lane = copiedTrack->getProcessorLane();
                if (lane->size() > 0) { // create tracks to make room
                    while (toTrackIndex >= tracks.getNumNonMasterTracks()) {
                        addAndPerformAction(new CreateTrack(false, {}, tracks, view));
                    }
                }
                if (toTrackIndex < tracks.getNumNonMasterTracks())
                    copyProcessorsFromTrack(copiedTrack, fromTrackIndex, toTrackIndex, trackAndSlotDiff.y);
            }
        }
        // Second pass: insert selected tracks (along with their processors)
        const auto selectedTrackIndices = findSelectedNonMasterTrackIndices(copiedTracks);
        const auto duplicatedTrackIndices = findDuplicationIndices(selectedTrackIndices);
        for (unsigned long i = 0; i < selectedTrackIndices.size(); i++) {
            int fromTrackIndex = selectedTrackIndices[i];
            addAndPerformCreateTrackAction(fromTrackIndex, duplicate ? duplicatedTrackIndices[i] : fromTrackIndex + trackAndSlotDiff.x + 1);
        }
    }

    selectAction = std::make_unique<MoveSelections>(createActions, tracks, connections, view, input, allProcessors, processorGraph);
    selectAction->setNewFocusedSlot(newFocusedTrackAndSlot);

    // Cleanup
    for (int i = createActions.size() - 1; i >= 0; i--) {
        auto *action = createActions.getUnchecked(i);
        if (auto *createProcessorAction = dynamic_cast<CreateProcessor *>(action))
            createProcessorAction->undoTemporary();
        else
            action->undo();
    }
}

bool Insert::perform() {
    if (createActions.isEmpty()) return false;

    for (auto *createAction : createActions) {
        createAction->perform();
    }
    selectAction->perform();
    return true;
}

bool Insert::undo() {
    if (createActions.isEmpty()) return false;

    selectAction->undo();
    for (int i = createActions.size() - 1; i >= 0; i--) {
        createActions.getUnchecked(i)->undo();
    }
    return true;
}

void Insert::duplicateSelectedProcessors(const Track *track, const OwnedArray<Track> &copiedTracks) {
    const BigInteger slotsMask = track->getSlotMask();
    std::vector<int> selectedSlots;
    for (int slot = 0; slot <= std::min(tracks.getNumSlotsForTrack(track) - 1, slotsMask.getHighestBit()); slot++)
        if (slotsMask[slot])
            selectedSlots.push_back(slot);

    auto duplicatedSlots = findDuplicationIndices(selectedSlots);
    int trackIndex = copiedTracks.indexOf(track);
    for (unsigned long i = 0; i < selectedSlots.size(); i++) {
        addAndPerformCreateProcessorAction(trackIndex, selectedSlots[i], trackIndex, duplicatedSlots[i]);
    }
}

void Insert::copyProcessorsFromTrack(const Track *fromTrack, int fromTrackIndex, int toTrackIndex, int slotDiff) {
    const BigInteger slotsMask = fromTrack->getSlotMask();
    for (int fromSlot = 0; fromSlot <= slotsMask.getHighestBit(); fromSlot++) {
        if (slotsMask[fromSlot]) {
            addAndPerformCreateProcessorAction(fromTrackIndex, fromSlot, toTrackIndex, fromSlot + slotDiff);
        }
    }
}

void Insert::addAndPerformAction(UndoableAction *action) {
    if (auto *createProcessorAction = dynamic_cast<CreateProcessor *>(action))
        createProcessorAction->performTemporary();
    else
        action->perform();
    createActions.add(action);
}

void Insert::addAndPerformCreateProcessorAction(int fromTrackIndex, int fromSlot, int toTrackIndex, int toSlot) {
    addAndPerformAction(new CreateProcessor(juce::Point(fromTrackIndex, fromSlot), toTrackIndex, toSlot, tracks, view, allProcessors, processorGraph));
    if (oldFocusedTrackAndSlot.x == fromTrackIndex && oldFocusedTrackAndSlot.y == fromSlot)
        newFocusedTrackAndSlot = {toTrackIndex, toSlot};
}

void Insert::addAndPerformCreateTrackAction(int fromTrackIndex, int toTrackIndex) {
    addAndPerformAction(new CreateTrack(toTrackIndex, false, fromTrackIndex, tracks, view));
    const auto *track = tracks.getMostRecentlyCreatedTrack();
    if (track == nullptr) return;

    // Create track-level processors
    for (auto *processor : track->getAllProcessors())
        if (processor->isIoProcessor())
            addAndPerformAction(new CreateProcessor(juce::Point(fromTrackIndex, -1), toTrackIndex, -1, tracks, view, allProcessors, processorGraph));

    // Create in-lane processors
    for (auto *processor : track->getProcessorLane()->getChildren()) {
        int slot = processor->getSlot();
        addAndPerformCreateProcessorAction(fromTrackIndex, slot, toTrackIndex, slot);
    }
}

Insert::MoveSelections::MoveSelections(const OwnedArray<UndoableAction> &createActions, Tracks &tracks, Connections &connections,
                                       View &view, Input &input, AllProcessors &allProcessors, ProcessorGraph &processorGraph)
        : Select(tracks, connections, view, input, allProcessors, processorGraph) {
    for (int i = 0; i < newTrackSelections.size(); i++) {
        newTrackSelections.setUnchecked(i, false);
        newSelectedSlotsMasks.setUnchecked(i, BigInteger());
    }

    for (auto *createAction : createActions) {
        if (auto *createProcessorAction = dynamic_cast<CreateProcessor *>(createAction)) {
            BigInteger mask = newSelectedSlotsMasks.getUnchecked(createProcessorAction->trackIndex);
            mask.setBit(createProcessorAction->slot, true);
            newSelectedSlotsMasks.setUnchecked(createProcessorAction->trackIndex, mask);
        } else if (auto *createTrackAction = dynamic_cast<CreateTrack *>(createAction)) {
            newTrackSelections.setUnchecked(createTrackAction->insertIndex, true);
            const auto *track = tracks.get(createTrackAction->insertIndex);
            const auto fullSelectionBitmask = Track::createFullSelectionBitmask(view.getNumProcessorSlots(track != nullptr && track->isMaster()));
            newSelectedSlotsMasks.setUnchecked(createTrackAction->insertIndex, fullSelectionBitmask);
        }
    }
}
