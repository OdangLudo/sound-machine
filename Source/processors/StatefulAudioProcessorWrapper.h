#pragma once

#include <Utilities.h>
#include "Identifiers.h"
#include "view/processor_editor/SwitchParameterComponent.h"

class StatefulAudioProcessorWrapper : private AudioProcessorListener, private Timer {
public:
    struct Parameter
            : public AudioProcessorParameterWithID,
              private Utilities::ValueTreePropertyChangeListener,
              public AudioProcessorParameter::Listener, public Slider::Listener, public Button::Listener,
              public ComboBox::Listener, public SwitchParameterComponent::Listener {
        explicit Parameter(AudioProcessorParameter *parameter)
                : AudioProcessorParameterWithID(parameter->getName(32), parameter->getName(32),
                                                parameter->getLabel(), parameter->getCategory()),
                  sourceParameter(parameter),
                  defaultValue(parameter->getDefaultValue()), value(parameter->getDefaultValue()),
                  valueToTextFunction([this](float value) { return sourceParameter->getCurrentValueAsText(); }),
                  textToValueFunction([this](const String &text) { return sourceParameter->getValueForText(text); }) {
            if (auto *p = dynamic_cast<AudioParameterFloat *>(sourceParameter)) {
                range = p->range;
            } else {
                if (sourceParameter->getNumSteps() != AudioProcessor::getDefaultNumParameterSteps())
                    range = NormalisableRange<float>(0.0, 1.0, 1.0f / (sourceParameter->getNumSteps() - 1.0f));
                else if (!sourceParameter->getAllValueStrings().isEmpty())
                    range = NormalisableRange<float>(0.0, 1.0, 1.0f / (sourceParameter->getAllValueStrings().size() - 1.0f));
                else
                    range = NormalisableRange<float>(0.0, 1.0);

            }
            value = defaultValue = convertNormalizedToUnnormalized(parameter->getDefaultValue());
            sourceParameter->addListener(this);
        }

        ~Parameter() override {
            if (state.isValid())
                state.removeListener(this);
            sourceParameter->removeListener(this);
            for (auto* label : attachedLabels) {
                label->onTextChange = nullptr;
            }
            attachedLabels.clear(false);
            for (auto* slider : attachedSliders) {
                slider->removeListener(this);
            }
            attachedSliders.clear(false);
            for (auto* button : attachedButtons) {
                button->removeListener(this);
            }
            attachedButtons.clear(false);
            for (auto* comboBox : attachedComboBoxes) {
                comboBox->removeListener(this);
            }
            attachedComboBoxes.clear(false);
            for (auto* parameterSwitch : attachedSwitches) {
                parameterSwitch->removeListener(this);
            }
            attachedSwitches.clear(false);
        }

        String getText(float v, int length) const override {
            return valueToTextFunction != nullptr ?
                   valueToTextFunction(convertNormalizedToUnnormalized(v)) :
                   AudioProcessorParameter::getText(v, length);
        }

        void parameterValueChanged(int parameterIndex, float newValue) override {
            if (!ignoreCallbacks) { setValue(newValue); }
        }

        void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override {}

        float convertNormalizedToUnnormalized(float value) const {
            return range.snapToLegalValue(range.convertFrom0to1(value));
        }

        float getValue() const override {
            return range.convertTo0to1(value);
        }

        void setValue(float newValue) override {
            ScopedValueSetter<bool> svs(ignoreCallbacks, true);
            newValue = range.convertFrom0to1(newValue);

            if (value != newValue || listenersNeedCalling) {
                value = newValue;
                postUnnormalisedValue(value);
                setAttachedComponentValues(value);
                listenersNeedCalling = false;
                needsUpdate = true;
            }
        }

        void setUnnormalizedValue(float unnormalisedValue) {
            setValue(range.convertTo0to1(unnormalisedValue));
        }

        void setAttachedComponentValues(float newValue) {
            const ScopedLock selfCallbackLock(selfCallbackMutex);
            {
                ScopedValueSetter<bool> svs (ignoreCallbacks, true);
                for (auto* label : attachedLabels) {
                    label->setText(valueToTextFunction(newValue), dontSendNotification);
                }
                for (auto* slider : attachedSliders) {
                    slider->setValue(newValue, sendNotificationSync);
                }
                for (auto* button : attachedButtons) {
                    button->setToggleState(newValue >= 0.5f, sendNotificationSync);
                }
                for (auto* comboBox : attachedComboBoxes) {
                    auto index = roundToInt(newValue * (comboBox->getNumItems() - 1));
                    comboBox->setSelectedItemIndex(index, sendNotificationSync);
                }
                for (auto* parameterSwitch : attachedSwitches) {
                    auto index = roundToInt(newValue * (parameterSwitch->getNumItems() - 1));
                    parameterSwitch->setSelectedItemIndex(index, sendNotificationSync);
                }
            }
        }

        void postUnnormalisedValue(float unnormalisedValue) {
            ScopedValueSetter<bool> svs(ignoreCallbacks, true);
            if (convertNormalizedToUnnormalized(sourceParameter->getValue()) != unnormalisedValue) {
                sourceParameter->setValueNotifyingHost(range.convertTo0to1(unnormalisedValue));
            }
        }

        void updateFromValueTree() {
            const float unnormalisedValue = float(state.getProperty(IDs::value, defaultValue));
            setUnnormalizedValue(unnormalisedValue);
        }

        void setNewState(const ValueTree &v, UndoManager *undoManager) {
            state = v;
            this->undoManager = undoManager;
            this->state.addListener(this);
            updateFromValueTree();
            copyValueToValueTree();
        }

        void copyValueToValueTree() {
            if (auto *valueProperty = state.getPropertyPointer(IDs::value)) {
                if ((float) *valueProperty != value) {
                    ScopedValueSetter<bool> svs(ignoreParameterChangedCallbacks, true);
                    state.setProperty(IDs::value, value, undoManager);
                }
            } else {
                state.setProperty(IDs::value, value, nullptr);
            }
        }

        void attachLabel(Label *valueLabel) {
            if (valueLabel != nullptr) {
                attachedLabels.add(valueLabel);
                valueLabel->onTextChange = [this, valueLabel] { textChanged(valueLabel); };
            }
            setAttachedComponentValues(value);
        }

        void detachLabel(Label *valueLabel) {
            if (valueLabel != nullptr)
                attachedLabels.removeObject(valueLabel, false);
        }

        void attachSlider(Slider *slider) {
            if (slider != nullptr) {
                slider->textFromValueFunction = nullptr;
                slider->valueFromTextFunction = nullptr;
                slider->setNormalisableRange(doubleRangeFromFloatRange(range));
                slider->textFromValueFunction = valueToTextFunction;
                slider->valueFromTextFunction = textToValueFunction;
                attachedSliders.add(slider);
                slider->addListener(this);
            }
            setAttachedComponentValues(value);
        }

        void detachSlider(Slider *slider) {
            if (slider != nullptr) {
                slider->removeListener(this);
                slider->textFromValueFunction = nullptr;
                slider->valueFromTextFunction = nullptr;
                attachedSliders.removeObject(slider, false);
            }
        }

        void attachButton(Button *button) {
            if (button != nullptr) {
                attachedButtons.add(button);
                button->addListener(this);
            }
            setAttachedComponentValues(value);
        }

        void detachButton(Button *button) {
            if (button != nullptr) {
                button->removeListener(this);
                attachedButtons.removeObject(button, false);
            }
        }

        void attachComboBox(ComboBox *comboBox) {
            if (comboBox != nullptr) {
                attachedComboBoxes.add(comboBox);
                comboBox->addListener(this);
            }
            setAttachedComponentValues(value);
        }

        void detachComboBox(ComboBox *comboBox) {
            if (comboBox != nullptr) {
                comboBox->removeListener(this);
                attachedComboBoxes.removeObject(comboBox, false);
            }
        }

        void attachSwitch(SwitchParameterComponent *parameterSwitch) {
            if (parameterSwitch != nullptr) {
                attachedSwitches.add(parameterSwitch);
                parameterSwitch->addListener(this);
            }
            setAttachedComponentValues(value);
        }

        void detachSwitch(SwitchParameterComponent *parameterSwitch) {
            if (parameterSwitch != nullptr) {
                parameterSwitch->removeListener(this);
                attachedSwitches.removeObject(parameterSwitch, false);
            }
        }

        float getDefaultValue() const override {
            return range.convertTo0to1(defaultValue);
        }

        float getValueForText(const String &text) const override {
            return range.convertTo0to1(
                    textToValueFunction != nullptr ? textToValueFunction(text) : text.getFloatValue());
        }

        static NormalisableRange<double> doubleRangeFromFloatRange(NormalisableRange<float> &floatRange) {
            return NormalisableRange<double>(floatRange.start, floatRange.end, floatRange.interval, floatRange.skew,
                                             floatRange.symmetricSkew);
        }

        AudioProcessorParameter *sourceParameter{nullptr};
        float defaultValue, value;
        std::function<String(const float)> valueToTextFunction;
        std::function<float(const String &)> textToValueFunction;
        NormalisableRange<float> range;

        std::atomic<bool> needsUpdate{true};
        ValueTree state;
        UndoManager *undoManager{nullptr};

    private:
        bool listenersNeedCalling{true};
        bool ignoreParameterChangedCallbacks = false;

        bool ignoreCallbacks { false };
        CriticalSection selfCallbackMutex;

        OwnedArray<Label> attachedLabels {};
        OwnedArray<Slider> attachedSliders {};
        OwnedArray<Button> attachedButtons {};
        OwnedArray<ComboBox> attachedComboBoxes {};
        OwnedArray<SwitchParameterComponent> attachedSwitches {};

        void valueTreePropertyChanged(ValueTree &tree, const Identifier &p) override {
            if (ignoreParameterChangedCallbacks)
                return;

            if (p == IDs::value) {
                updateFromValueTree();
            }
        }

        void textChanged(Label *valueLabel) {
            const ScopedLock selfCallbackLock(selfCallbackMutex);

            auto newValue = convertNormalizedToUnnormalized(textToValueFunction(valueLabel->getText()));
            if (!ignoreCallbacks) {
                beginParameterChange();
                setUnnormalizedValue(newValue);
                endParameterChange();
            }
        }

        void sliderValueChanged(Slider* slider) override {
            const ScopedLock selfCallbackLock(selfCallbackMutex);

            if (!ignoreCallbacks)
                setUnnormalizedValue((float) slider->getValue());
        }

        void buttonClicked(Button* button) override {
            const ScopedLock selfCallbackLock (selfCallbackMutex);

            if (!ignoreCallbacks) {
                beginParameterChange();
                setUnnormalizedValue(button->getToggleState() ? 1.0f : 0.0f);
                endParameterChange();
            }
        }

        void comboBoxChanged(ComboBox* comboBox) override {
            const ScopedLock selfCallbackLock(selfCallbackMutex);

            if (!ignoreCallbacks) {
                if (sourceParameter->getCurrentValueAsText() != comboBox->getText()) {
                    beginParameterChange();
                    setUnnormalizedValue(sourceParameter->getValueForText(comboBox->getText()));
                    endParameterChange();
                }
            }
        }

        void switchChanged(SwitchParameterComponent* parameterSwitch) override {
            const ScopedLock selfCallbackLock(selfCallbackMutex);

            if (!ignoreCallbacks) {
                beginParameterChange();
                float newValue = float(parameterSwitch->getSelectedItemIndex()) / float((parameterSwitch->getNumItems() - 1));
                setUnnormalizedValue(newValue);
                endParameterChange();
            }
        }

        void beginParameterChange() {
            if (undoManager != nullptr)
                undoManager->beginNewTransaction();
            sourceParameter->beginChangeGesture();
        }

        void endParameterChange() {
            sourceParameter->endChangeGesture();
        }

        void sliderDragStarted (Slider* slider) override { beginParameterChange(); }
        void sliderDragEnded   (Slider* slider) override { endParameterChange();   }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Parameter)
    };

    StatefulAudioProcessorWrapper(AudioPluginInstance *processor, ValueTree state, UndoManager &undoManager) :
            processor(processor), state(std::move(state)), undoManager(undoManager) {
        processor->addListener(this);
        processor->enableAllBuses();
        audioProcessorChanged(processor);
        updateValueTree();
        startTimerHz(10);
    }

    Parameter *getParameter(int parameterIndex) {
        return parameters[parameterIndex];
    }

    void updateValueTree() {
        for (auto parameter : processor->getParameters()) {
            auto *parameterWrapper = new Parameter(parameter);
            parameterWrapper->setNewState(getOrCreateChildValueTree(parameterWrapper->paramID), &undoManager);
            parameters.add(parameterWrapper);
        }

        state.setProperty(IDs::numInputChannels, processor->getTotalNumInputChannels(), &undoManager);
        state.setProperty(IDs::numOutputChannels, processor->getTotalNumOutputChannels(), &undoManager);
    }

    ValueTree getOrCreateChildValueTree(const String &paramID) {
        ValueTree v(state.getChildWithProperty(IDs::id, paramID));

        if (!v.isValid()) {
            v = ValueTree(IDs::PARAM);
            v.setProperty(IDs::id, paramID, nullptr);
            state.appendChild(v, nullptr);
        }

        return v;
    }

    bool flushParameterValuesToValueTree() {
        ScopedLock lock(valueTreeChanging);

        bool anythingUpdated = false;

        for (auto *ap : parameters) {
            bool needsUpdateTestValue = true;

            if (ap->needsUpdate.compare_exchange_strong(needsUpdateTestValue, false)) {
                ap->copyValueToValueTree();
                anythingUpdated = true;
            }
        }

        return anythingUpdated;
    }

    // TODO only one timer callback for all processors
    void timerCallback() override {
        auto anythingUpdated = flushParameterValuesToValueTree();
        startTimer(anythingUpdated ? 1000 / 50 : jlimit(50, 500, getTimerInterval() + 20));
    }

    AudioPluginInstance *processor;
    ValueTree state;
private:
    UndoManager &undoManager;
    OwnedArray<Parameter> parameters;

    CriticalSection valueTreeChanging;

    void audioProcessorParameterChanged(AudioProcessor *processor, int parameterIndex, float newValue) override {}

    void audioProcessorChanged(AudioProcessor *processor) override {
        MessageManager::callAsync([this, processor] {
            if (processor != nullptr) {
                // TODO should we use UndoManager and also support _setting_ playConfigDetails on state change?
                state.setProperty(IDs::numInputChannels, processor->getTotalNumInputChannels(), nullptr);
                state.setProperty(IDs::numOutputChannels, processor->getTotalNumOutputChannels(), nullptr);
                state.setProperty(IDs::acceptsMidi, processor->acceptsMidi(), nullptr);
                state.setProperty(IDs::producesMidi, processor->producesMidi(), nullptr);
            }
        });
    }

    void audioProcessorParameterChangeGestureBegin(AudioProcessor *processor, int parameterIndex) override {}

    void audioProcessorParameterChangeGestureEnd(AudioProcessor *processor, int parameterIndex) override {}
};
