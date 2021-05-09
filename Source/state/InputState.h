#pragma once

#include <PluginManager.h>
#include "StatefulAudioProcessorContainer.h"
#include "Stateful.h"
#include "TracksState.h"
#include "ConnectionsState.h"

class InputState : public Stateful, private ChangeListener, private ValueTree::Listener {
public:
    InputState(TracksState &tracks, ConnectionsState &connections, StatefulAudioProcessorContainer &audioProcessorContainer,
               PluginManager &pluginManager, UndoManager &undoManager, AudioDeviceManager &deviceManager);

    ~InputState() override;

    ValueTree &getState() override { return input; }

    void loadFromState(const ValueTree &state) override;

    void initializeDefault();

    ValueTree getAudioInputProcessorState() const {
        return input.getChildWithProperty(IDs::name, pluginManager.getAudioInputDescription().name);
    }

    ValueTree getPush2MidiInputProcessor() const {
        return input.getChildWithProperty(IDs::deviceName, push2MidiDeviceName);
    }

    AudioProcessorGraph::NodeID getDefaultInputNodeIdForConnectionType(ConnectionType connectionType) const {
        if (connectionType == audio) {
            return StatefulAudioProcessorContainer::getNodeIdForState(getAudioInputProcessorState());
        } else if (connectionType == midi) {
            return StatefulAudioProcessorContainer::getNodeIdForState(getPush2MidiInputProcessor());
        } else {
            return {};
        }
    }

private:
    ValueTree input;

    TracksState &tracks;
    ConnectionsState &connections;
    StatefulAudioProcessorContainer &audioProcessorContainer;
    PluginManager &pluginManager;

    UndoManager &undoManager;
    AudioDeviceManager &deviceManager;

    void syncInputDevicesWithDeviceManager();

    void changeListenerCallback(ChangeBroadcaster *source) override;

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override {
        if (child[IDs::name] == MidiInputProcessor::name() && !deviceManager.isMidiInputEnabled(child[IDs::deviceName]))
            deviceManager.setMidiInputEnabled(child[IDs::deviceName], true);
    }

    void valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int) override {
        if (child[IDs::name] == MidiInputProcessor::name() && deviceManager.isMidiInputEnabled(child[IDs::deviceName]))
            deviceManager.setMidiInputEnabled(child[IDs::deviceName], false);
    }

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override {
        if (i == IDs::deviceName && tree == input) {
            AudioDeviceManager::AudioDeviceSetup config;
            deviceManager.getAudioDeviceSetup(config);
            config.inputDeviceName = tree[IDs::deviceName];
            deviceManager.setAudioDeviceSetup(config, true);
        }
    }
};
