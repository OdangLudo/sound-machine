#pragma once

#include <ValueTreeObjectList.h>
#include <state/Project.h>
#include "GraphEditorProcessor.h"
#include "GraphEditorProcessorContainer.h"
#include "ConnectorDragListener.h"
#include "GraphEditorPin.h"
#include "view/CustomColourIds.h"

class GraphEditorProcessors : public Component,
                              public Utilities::ValueTreeObjectList<GraphEditorProcessor>,
                              public GraphEditorProcessorContainer {
public:
    explicit GraphEditorProcessors(Project &project, const ValueTree &state, ConnectorDragListener &connectorDragListener)
            : Utilities::ValueTreeObjectList<GraphEditorProcessor>(state),
              project(project), tracks(project.getTracks()), view(project.getView()),
              connections(project.getConnections()),
              pluginManager(project.getPluginManager()), connectorDragListener(connectorDragListener) {
        rebuildObjects();
        view.addListener(this);
        // TODO shouldn't need to do this
        valueTreePropertyChanged(view.getState(), isMasterTrack() ? IDs::numMasterProcessorSlots : IDs::numProcessorSlots);
    }

    ~GraphEditorProcessors() override {
        view.removeListener(this);
        freeObjects();
    }

    bool isMasterTrack() const { return TracksState::isMasterTrack(parent); }

    int getNumAvailableSlots() const { return tracks.getNumAvailableSlotsForTrack(parent); }

    int getSlotOffset() const { return tracks.getSlotOffsetForTrack(parent); }

    void mouseDown(const MouseEvent &e) override {
        int slot = findSlotAt(e.getEventRelativeTo(this));
        bool isSlotSelected = TracksState::isSlotSelected(parent, slot);
        project.setProcessorSlotSelected(parent, slot, !(isSlotSelected && e.mods.isCommandDown()), !(isSlotSelected || e.mods.isCommandDown()));
        if (e.mods.isPopupMenu() || e.getNumberOfClicks() == 2)
            showPopupMenu(slot);
    }

    void mouseUp(const MouseEvent &e) override {
        if (e.mouseWasClicked() && !e.mods.isCommandDown()) {
            // single click deselects other tracks/processors
            project.setProcessorSlotSelected(parent, findSlotAt(e.getEventRelativeTo(this)), true);
        }
    }

    void resized() override {
        auto slotOffset = getSlotOffset();
        auto processorSlotSize = getProcessorSlotSize();
        auto r = getLocalBounds();
        if (isMasterTrack()) {
            r.setWidth(processorSlotSize);
            r.setX(-slotOffset * processorSlotSize);
        } else {
            r.setHeight(processorSlotSize);
            r.setY(-slotOffset * processorSlotSize);
        }

        for (int slot = 0; slot < processorSlotRectangles.size(); slot++) {
            if (slot == slotOffset) {
                if (isMasterTrack())
                    r.setX(r.getX() + ViewState::TRACK_LABEL_HEIGHT);
                else
                    r.setY(r.getY() + ViewState::TRACK_LABEL_HEIGHT);
            } else if (slot == slotOffset + ViewState::NUM_VISIBLE_NON_MASTER_TRACK_SLOTS + (isMasterTrack() ? 1 : 0)) {
                if (isMasterTrack())
                    r.setX(r.getX() + ViewState::TRACK_OUTPUT_HEIGHT);
                else
                    r.setY(r.getY() + ViewState::TRACK_OUTPUT_HEIGHT);
            }
            processorSlotRectangles.getUnchecked(slot)->setRectangle(r.reduced(1).toFloat());
            if (auto *processor = findProcessorAtSlot(slot))
                processor->setBounds(r);
            if (isMasterTrack())
                r.setX(r.getX() + processorSlotSize);
            else
                r.setY(r.getY() + processorSlotSize);
        }
    }

    void updateProcessorSlotColours() {
        const static auto &baseColour = findColour(ResizableWindow::backgroundColourId).brighter(0.4);

        for (int slot = 0; slot < processorSlotRectangles.size(); slot++) {
            auto fillColour = baseColour;
            if (TracksState::doesTrackHaveSelections(parent))
                fillColour = fillColour.brighter(0.2);
            if (TracksState::isSlotSelected(parent, slot))
                fillColour = TracksState::getTrackColour(parent);
            if (tracks.isSlotFocused(parent, slot))
                fillColour = fillColour.brighter(0.16);
            if (!tracks.isProcessorSlotInView(parent, slot))
                fillColour = fillColour.darker(0.3);
            processorSlotRectangles.getUnchecked(slot)->setFill(fillColour);
        }
    }

    bool isSuitableType(const ValueTree &v) const override {
        return v.hasType(IDs::PROCESSOR);
    }

    GraphEditorProcessor *createNewObject(const ValueTree &tree) override {
        GraphEditorProcessor *processor = currentlyMovingProcessor != nullptr
                                          ? currentlyMovingProcessor
                                          : new GraphEditorProcessor(project, tracks, tree, connectorDragListener);
        addAndMakeVisible(processor);
        return processor;
    }

    void deleteObject(GraphEditorProcessor *processor) override {
        if (currentlyMovingProcessor == nullptr)
            delete processor;
        else
            removeChildComponent(processor);
    }

    void newObjectAdded(GraphEditorProcessor *processor) override {
        processor->addMouseListener(this, true);
        resized();
    }

    void objectRemoved(GraphEditorProcessor *processor) override {
        processor->removeMouseListener(this);
        resized();
    }

    void objectOrderChanged() override { resized(); }

    GraphEditorProcessor *getProcessorForNodeId(AudioProcessorGraph::NodeID nodeId) const override {
        for (auto *processor : objects)
            if (processor->getNodeId() == nodeId)
                return processor;
        return nullptr;
    }

    GraphEditorPin *findPinAt(const MouseEvent &e) const {
        for (auto *processor : objects)
            if (auto *pin = processor->findPinAt(e))
                return pin;
        return nullptr;
    }

    int findSlotAt(const MouseEvent &e) {
        return view.findSlotAt(e.getEventRelativeTo(this).getPosition(), parent);
    }

    GraphEditorProcessor *getCurrentlyMovingProcessor() const {
        return currentlyMovingProcessor;
    }

    void setCurrentlyMovingProcessor(GraphEditorProcessor *currentlyMovingProcessor) {
        this->currentlyMovingProcessor = currentlyMovingProcessor;
    }

private:
    static constexpr int
            DELETE_MENU_ID = 1, TOGGLE_BYPASS_MENU_ID = 2, ENABLE_DEFAULTS_MENU_ID = 3, DISCONNECT_ALL_MENU_ID = 4,
            DISABLE_DEFAULTS_MENU_ID = 5, DISCONNECT_CUSTOM_MENU_ID = 6,
            SHOW_PLUGIN_GUI_MENU_ID = 10, SHOW_ALL_PROGRAMS_MENU_ID = 11, CONFIGURE_AUDIO_MIDI_MENU_ID = 12,
            ADD_MIXER_CHANNEL_MENU_ID = 13;

    Project &project;
    TracksState &tracks;
    ViewState &view;
    ConnectionsState &connections;

    PluginManager &pluginManager;
    ConnectorDragListener &connectorDragListener;
    GraphEditorProcessor *currentlyMovingProcessor{};

    OwnedArray<DrawableRectangle> processorSlotRectangles;

    GraphEditorProcessor *findProcessorAtSlot(int slot) const {
        for (auto *processor : objects)
            if (processor->getSlot() == slot)
                return processor;
        return nullptr;
    }

    void showPopupMenu(int slot) {
        PopupMenu menu;
        auto *processor = findProcessorAtSlot(slot);
        bool isMixerChannel = slot == tracks.getMixerChannelSlotForTrack(parent);

        if (processor != nullptr) {
            if (!isMixerChannel) {
                PopupMenu processorSelectorSubmenu;
                pluginManager.addPluginsToMenu(processorSelectorSubmenu, parent);
                menu.addSubMenu("Insert new processor", processorSelectorSubmenu);
                menu.addSeparator();
            }

            if (processor->isIoProcessor()) {
                menu.addItem(CONFIGURE_AUDIO_MIDI_MENU_ID, "Configure audio/MIDI IO");
            } else {
                menu.addItem(DELETE_MENU_ID, "Delete this processor");
                menu.addItem(TOGGLE_BYPASS_MENU_ID, "Toggle Bypass");
            }

            menu.addSeparator();
            // todo single, stateful, menu item for enable/disable default connections
            menu.addItem(ENABLE_DEFAULTS_MENU_ID, "Enable default connections");
            menu.addItem(DISABLE_DEFAULTS_MENU_ID, "Disable default connections");
            menu.addItem(DISCONNECT_ALL_MENU_ID, "Disconnect all");
            menu.addItem(DISCONNECT_CUSTOM_MENU_ID, "Disconnect all custom");

            if (processor->getAudioProcessor()->hasEditor()) {
                menu.addSeparator();
                menu.addItem(SHOW_PLUGIN_GUI_MENU_ID, "Show plugin GUI");
                menu.addItem(SHOW_ALL_PROGRAMS_MENU_ID, "Show all programs");
            }

            menu.showMenuAsync({}, ModalCallbackFunction::create
                    ([this, processor, slot](int result) {
                        const auto &description = pluginManager.getChosenType(result);
                        if (!description.name.isEmpty()) {
                            project.createProcessor(description, slot);
                            return;
                        }

                        switch (result) {
                            case DELETE_MENU_ID:
                                getCommandManager().invokeDirectly(CommandIDs::deleteSelected, false);
                                break;
                            case TOGGLE_BYPASS_MENU_ID:
                                project.toggleProcessorBypass(processor->getState());
                                break;
                            case ENABLE_DEFAULTS_MENU_ID:
                                project.setDefaultConnectionsAllowed(processor->getState(), true);
                                break;
                            case DISABLE_DEFAULTS_MENU_ID:
                                project.setDefaultConnectionsAllowed(processor->getState(), false);
                                break;
                            case DISCONNECT_ALL_MENU_ID:
                                project.disconnectProcessor(processor->getState());
                                break;
                            case DISCONNECT_CUSTOM_MENU_ID:
                                project.disconnectCustom(processor->getState());
                                break;
                            case SHOW_PLUGIN_GUI_MENU_ID:
                                processor->showWindow(PluginWindow::Type::normal);
                                break;
                            case SHOW_ALL_PROGRAMS_MENU_ID:
                                processor->showWindow(PluginWindow::Type::programs);
                                break;
                            case CONFIGURE_AUDIO_MIDI_MENU_ID:
                                getCommandManager().invokeDirectly(CommandIDs::showAudioMidiSettings, false);
                                break;
                            default:
                                break;
                        }
                    }));
        } else { // no processor in this slot
            if (isMixerChannel)
                menu.addItem(ADD_MIXER_CHANNEL_MENU_ID, "Add mixer channel");
            else
                pluginManager.addPluginsToMenu(menu, parent);

            menu.showMenuAsync({}, ModalCallbackFunction::create([this, slot, isMixerChannel](int result) {
                if (isMixerChannel) {
                    if (result == ADD_MIXER_CHANNEL_MENU_ID) {
                        getCommandManager().invokeDirectly(CommandIDs::addMixerChannel, false);
                    }
                } else {
                    auto &description = pluginManager.getChosenType(result);
                    if (!description.name.isEmpty()) {
                        project.createProcessor(description, slot);
                    }
                }
            }));
        }
    }

    int getProcessorSlotSize() const {
        return isMasterTrack() ? view.getTrackWidth() : view.getProcessorHeight();
    }

    // TODO only instantiate 64 slot rects (and maybe another set for the boundary perimeter)
    //  might be an over-early optimization though
    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override {
        if (isSuitableType(tree) && i == IDs::processorSlot) {
            resized();
        } else if (i == IDs::selected || i == IDs::colour ||
                   i == IDs::selectedSlotsMask || i == IDs::focusedTrackIndex || i == IDs::focusedProcessorSlot ||
                   i == IDs::gridViewTrackOffset) {
            updateProcessorSlotColours();
        } else if (i == IDs::gridViewSlotOffset || (i == IDs::masterViewSlotOffset && isMasterTrack())) {
            resized();
            updateProcessorSlotColours();
        } else if (i == IDs::numProcessorSlots || (i == IDs::numMasterProcessorSlots && isMasterTrack())) {
            auto numSlots = getNumAvailableSlots();
            while (processorSlotRectangles.size() < numSlots) {
                auto *rect = new DrawableRectangle();
                processorSlotRectangles.add(rect);
                addAndMakeVisible(rect);
                rect->setCornerSize({3, 3});
                rect->toBack();
            }
            processorSlotRectangles.removeLast(processorSlotRectangles.size() - numSlots, true);
            resized();
            updateProcessorSlotColours();
        }
        Utilities::ValueTreeObjectList<GraphEditorProcessor>::valueTreePropertyChanged(tree, i);
    }

    void valueTreeChildAdded(ValueTree &parent, ValueTree &tree) override {
        ValueTreeObjectList::valueTreeChildAdded(parent, tree);
        if (this->parent == parent && isSuitableType(tree))
            resized();
    }
};
