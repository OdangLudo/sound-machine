#include "ViewState.h"

ViewState::ViewState(UndoManager &undoManager) : undoManager(undoManager) {
    viewState = ValueTree(IDs::VIEW_STATE);
}

void ViewState::initializeDefault() {
    setNoteMode();
    focusOnEditorPane();
    focusOnProcessorSlot({}, -1);
    viewState.setProperty(IDs::numProcessorSlots, NUM_VISIBLE_NON_MASTER_TRACK_SLOTS, nullptr);
    // the number of processors in the master track aligns with the number of tracks
    viewState.setProperty(IDs::numMasterProcessorSlots, NUM_VISIBLE_MASTER_TRACK_SLOTS, nullptr);
    setGridViewTrackOffset(0);
    setGridViewSlotOffset(0);
    setMasterViewSlotOffset(0);
}

void ViewState::updateViewTrackOffsetToInclude(int trackIndex, int numNonMasterTracks) {
    if (trackIndex < 0)
        return; // invalid
    auto viewTrackOffset = getGridViewTrackOffset();
    if (trackIndex >= viewTrackOffset + NUM_VISIBLE_TRACKS)
        setGridViewTrackOffset(trackIndex - NUM_VISIBLE_TRACKS + 1);
    else if (trackIndex < viewTrackOffset)
        setGridViewTrackOffset(trackIndex);
    else if (numNonMasterTracks - viewTrackOffset < NUM_VISIBLE_TRACKS && numNonMasterTracks >= NUM_VISIBLE_TRACKS)
        // always show last N tracks if available
        setGridViewTrackOffset(numNonMasterTracks - NUM_VISIBLE_TRACKS);
}

void ViewState::updateViewSlotOffsetToInclude(int processorSlot, bool isMasterTrack) {
    if (processorSlot < 0)
        return; // invalid
    if (isMasterTrack) {
        auto viewSlotOffset = getMasterViewSlotOffset();
        if (processorSlot >= viewSlotOffset + NUM_VISIBLE_MASTER_TRACK_SLOTS)
            setMasterViewSlotOffset(processorSlot - NUM_VISIBLE_MASTER_TRACK_SLOTS + 1);
        else if (processorSlot < viewSlotOffset)
            setMasterViewSlotOffset(processorSlot);
    } else {
        auto viewSlotOffset = getGridViewSlotOffset();
        if (processorSlot >= viewSlotOffset + NUM_VISIBLE_NON_MASTER_TRACK_SLOTS)
            setGridViewSlotOffset(processorSlot - NUM_VISIBLE_NON_MASTER_TRACK_SLOTS + 1);
        else if (processorSlot < viewSlotOffset)
            setGridViewSlotOffset(processorSlot);
    }
}

juce::Point<int> ViewState::getFocusedTrackAndSlot() const {
    int trackIndex = viewState.hasProperty(IDs::focusedTrackIndex) ? getFocusedTrackIndex() : 0;
    int processorSlot = viewState.hasProperty(IDs::focusedProcessorSlot) ? getFocusedProcessorSlot() : -1;
    return {trackIndex, processorSlot};
}

int ViewState::findSlotAt(const juce::Point<int> position, const ValueTree &track) const {
    bool isMaster = isMasterTrack(track);
    int length = isMaster ? position.x : (position.y - TRACK_LABEL_HEIGHT);
    if (length < 0)
        return -1;

    int processorSlotSize = isMaster ? getTrackWidth() : getProcessorHeight();
    int slot = getSlotOffsetForTrack(track) + length / processorSlotSize;
    return std::clamp(slot, 0, getNumSlotsForTrack(track) - 1);
}