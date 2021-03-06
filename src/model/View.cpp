#include "View.h"

void View::initializeDefault() {
    setNoteMode();
    focusOnEditorPane();
    focusOnProcessorSlot({0, -1});
    state.setProperty(ViewIDs::numProcessorSlots, NUM_VISIBLE_NON_MASTER_TRACK_SLOTS, nullptr);
    // the number of processors in the master track aligns with the number of tracks
    state.setProperty(ViewIDs::numMasterProcessorSlots, NUM_VISIBLE_MASTER_TRACK_SLOTS, nullptr);
    setGridViewTrackOffset(0);
    setGridViewSlotOffset(0);
    setMasterViewSlotOffset(0);
}

juce::Point<int> View::getFocusedTrackAndSlot() const {
    int trackIndex = state.hasProperty(ViewIDs::focusedTrackIndex) ? getFocusedTrackIndex() : 0;
    int processorSlot = state.hasProperty(ViewIDs::focusedProcessorSlot) ? getFocusedProcessorSlot() : -1;
    return {trackIndex, processorSlot};
}
void View::focusOnProcessorSlot(const juce::Point<int> slot) {
    auto currentlyFocusedTrackAndSlot = getFocusedTrackAndSlot();
    focusOnTrackIndex(slot.x);
    if (slot.x != currentlyFocusedTrackAndSlot.x && slot.y == currentlyFocusedTrackAndSlot.y) {
        // Different track but same slot selected - still send out the message
        state.sendPropertyChangeMessage(ViewIDs::focusedProcessorSlot);
    } else {
        state.setProperty(ViewIDs::focusedProcessorSlot, slot.y, nullptr);
    }
}

void View::updateViewTrackOffsetToInclude(int trackIndex, int numNonMasterTracks) {
    if (trackIndex < 0) return; // invalid

    auto viewTrackOffset = getGridViewTrackOffset();
    if (trackIndex >= viewTrackOffset + NUM_VISIBLE_TRACKS)
        setGridViewTrackOffset(trackIndex - NUM_VISIBLE_TRACKS + 1);
    else if (trackIndex < viewTrackOffset)
        setGridViewTrackOffset(trackIndex);
    else if (numNonMasterTracks - viewTrackOffset < NUM_VISIBLE_TRACKS && numNonMasterTracks >= NUM_VISIBLE_TRACKS)
        // always show last N tracks if available
        setGridViewTrackOffset(numNonMasterTracks - NUM_VISIBLE_TRACKS);
}
void View::updateViewSlotOffsetToInclude(int processorSlot, bool isMasterTrack) {
    if (processorSlot < 0) return; // invalid

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