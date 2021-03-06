#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

#include "push2/Push2MidiDevice.h"
#include "PluginManager.h"
#include "StatefulList.h"
#include "Processor.h"
#include "ConnectionType.h"

namespace InputIDs {
#define ID(name) const juce::Identifier name(#name);
ID(INPUT)
#undef ID
}

struct Input : public Stateful<Input>, public StatefulList<Processor> {
    Input(PluginManager &pluginManager, UndoManager &undoManager, AudioDeviceManager &deviceManager)
            : StatefulList<Processor>(state), pluginManager(pluginManager), undoManager(undoManager), deviceManager(deviceManager) {
        rebuildObjects();
    }
    ~Input() override {
        freeObjects();
    }

    static Identifier getIdentifier() { return InputIDs::INPUT; }
    bool isChildType(const ValueTree &tree) const override { return Processor::isType(tree); }

    void setDeviceName(const String &deviceName) { state.setProperty(ProcessorIDs::deviceName, deviceName, nullptr); }

    Processor *getProcessorByNodeId(juce::AudioProcessorGraph::NodeID nodeId) const {
        for (auto *processor : children)
            if (processor->getNodeId() == nodeId)
                return processor;
        return nullptr;
    }

    Processor *getDefaultInputProcessorForConnectionType(ConnectionType connectionType) const {
        if (connectionType == audio) return getChildForState(state.getChildWithProperty(ProcessorIDs::name, pluginManager.getAudioInputDescription().name));
        if (connectionType == midi) return getChildForState(state.getChildWithProperty(ProcessorIDs::deviceName, Push2MidiDevice::getDeviceName()));
        return {};
    }

    // Returns input processors to delete
    Array<Processor *> syncInputDevicesWithDeviceManager();

private:
    PluginManager &pluginManager;
    UndoManager &undoManager;
    AudioDeviceManager &deviceManager;

    Processor *createNewObject(const ValueTree &tree) override { return new Processor(tree, undoManager, deviceManager); }
    void onChildAdded(Processor *processor) override {
        if (processor->isMidiInputProcessor() && !deviceManager.isMidiInputEnabled(processor->getDeviceName()))
            deviceManager.setMidiInputEnabled(processor->getDeviceName(), true);
    }
    void onChildRemoved(Processor *processor, int oldIndex) override {
        if (processor->isMidiInputProcessor() && deviceManager.isMidiInputEnabled(processor->getDeviceName()))
            deviceManager.setMidiInputEnabled(processor->getDeviceName(), false);
    }

    void onChildChanged(Processor *processor, const Identifier &i) override {
        if (i == ProcessorIDs::deviceName) {
            AudioDeviceManager::AudioDeviceSetup config;
            deviceManager.getAudioDeviceSetup(config);
            config.inputDeviceName = processor->getDeviceName();
            deviceManager.setAudioDeviceSetup(config, true);
        }
    }
};
