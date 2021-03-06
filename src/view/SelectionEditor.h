#pragma once

#include "model/Project.h"
#include "PluginManager.h"
#include "processor_editor/ProcessorEditor.h"
#include "view/graph_editor/TooltipBar.h"
#include "view/context_pane/ContextPane.h"

class SelectionEditor : public Component,
                        public DragAndDropContainer,
                        private Button::Listener,
                        private ValueTree::Listener,
                        private StatefulList<Track>::Listener,
                        private StatefulList<Processor>::Listener {
public:
    SelectionEditor(Project &project, View &view, Tracks &tracks, StatefulAudioProcessorWrappers &processorWrappers);
    ~SelectionEditor() override;

    void mouseDown(const MouseEvent &event) override;
    void paint(Graphics &g) override;
    void resized() override;
    void buttonClicked(Button *b) override;

private:
    static const int PROCESSOR_EDITOR_HEIGHT = 160;

    TextButton addProcessorButton{"Add Processor"};

    Project &project;
    View &view;
    Tracks &tracks;
    PluginManager &pluginManager;
    StatefulAudioProcessorWrappers &processorWrappers;

    Viewport contextPaneViewport, processorEditorsViewport;
    ContextPane contextPane;
    TooltipBar statusBar;

    OwnedArray<ProcessorEditor> processorEditors;
    Component processorEditorsComponent;
    DrawableRectangle unfocusOverlay;
    std::unique_ptr<PopupMenu> addProcessorMenu;

    void refreshProcessors(const Processor *singleProcessorToRefresh = nullptr, bool onlyUpdateFocusState = false);
    void assignProcessorToEditor(const Processor *processor, int processorSlot = -1, bool onlyUpdateFocusState = false) const;

    void onChildRemoved(Track *, int oldIndex) override { refreshProcessors(); }
    void onChildChanged(Track *, const Identifier &i) override {}
    void onChildRemoved(Processor *, int oldIndex) override { refreshProcessors(); }
    void onChildChanged(Processor *processor, const Identifier &i) override {
        if (i == ProcessorIDs::initialized || i == ProcessorIDs::slot) {
            refreshProcessors();
        }
    }
    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override;
};
