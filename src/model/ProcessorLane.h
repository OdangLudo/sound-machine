#pragma once

#include "Stateful.h"
#include "Processor.h"
#include "StatefulList.h"

namespace ProcessorLaneIDs {
#define ID(name) const juce::Identifier name(#name);
ID(PROCESSOR_LANE)
ID(selectedSlotsMask)
#undef ID
}


struct ProcessorLane : public Stateful<ProcessorLane>, public StatefulList<Processor> {
    struct Listener {
        virtual void processorAdded(Processor *) {}
        virtual void processorRemoved(Processor *, int oldIndex) {}
        virtual void processorOrderChanged() {}
        virtual void processorPropertyChanged(Processor *, const Identifier &) {}
    };

    void addProcessorLaneListener(Listener *listener) {
        listeners.add(listener);
    }
    void removeProcessorLaneListener(Listener *listener) { listeners.remove(listener); }

    ProcessorLane(UndoManager &undoManager, AudioDeviceManager &deviceManager)
            : StatefulList<Processor>(state), undoManager(undoManager), deviceManager(deviceManager) {
        rebuildObjects();
    }

    explicit ProcessorLane(const ValueTree &state, UndoManager &undoManager, AudioDeviceManager &deviceManager)
            : Stateful<ProcessorLane>(state), StatefulList<Processor>(state), undoManager(undoManager), deviceManager(deviceManager) {
        rebuildObjects();
    }

    ~ProcessorLane() override {
        freeObjects();
    }

    static Identifier getIdentifier() { return ProcessorLaneIDs::PROCESSOR_LANE; }
    void loadFromState(const ValueTree &fromState) override;
    bool isChildType(const ValueTree &tree) const override { return Processor::isType(tree); }

    int getIndex() const { return state.getParent().indexOf(state); }

    Processor *getProcessorAtSlot(int slot) const {
        for (auto *processor : children)
            if (processor->getSlot() == slot)
                return processor;
        return nullptr;
    }
    Processor *getProcessorByNodeId(juce::AudioProcessorGraph::NodeID nodeId) const {
        for (auto *processor : children)
            if (processor->getNodeId() == nodeId)
                return processor;
        return nullptr;
    }

    BigInteger getSelectedSlotsMask() const {
        BigInteger selectedSlotsMask;
        selectedSlotsMask.parseString(state[ProcessorLaneIDs::selectedSlotsMask].toString(), 2);
        return selectedSlotsMask;
    }

    void setSelectedSlotsMask(const BigInteger &selectedSlotsMask) { state.setProperty(ProcessorLaneIDs::selectedSlotsMask, selectedSlotsMask.toString(2), nullptr); }

    static void setSelectedSlotsMask(ValueTree &state, const BigInteger &selectedSlotsMask) { state.setProperty(ProcessorLaneIDs::selectedSlotsMask, selectedSlotsMask.toString(2), nullptr); }

protected:
    Processor *createNewObject(const ValueTree &tree) override { return new Processor(tree, undoManager, deviceManager); }
    void deleteObject(Processor *processor) override { delete processor; }
    void newObjectAdded(Processor *processor) override { listeners.call(&Listener::processorAdded, processor); }
    void objectRemoved(Processor *processor, int oldIndex) override { listeners.call(&Listener::processorRemoved, processor, oldIndex); }
    void objectOrderChanged() override { listeners.call(&Listener::processorOrderChanged); }
    void objectChanged(Processor *processor, const Identifier &i) override { listeners.call(&Listener::processorPropertyChanged, processor, i); }

private:
    ListenerList<Listener> listeners;
    UndoManager &undoManager;
    AudioDeviceManager &deviceManager;
};
