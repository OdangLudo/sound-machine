#pragma once

#include "audio_sources/ToneSourceWithParameters.h"
#include "DefaultAudioProcessor.h"

class SineBank : public DefaultAudioProcessor {
public:
    explicit SineBank() : DefaultAudioProcessor(getPluginDescription()),
                          toneSource1("1", defaultStringFromValue, defaultValueFromString, createDefaultGainParameter),
                          toneSource2("2", defaultStringFromValue, defaultValueFromString, createDefaultGainParameter),
                          toneSource3("3", defaultStringFromValue, defaultValueFromString, createDefaultGainParameter),
                          toneSource4("4", defaultStringFromValue, defaultValueFromString, createDefaultGainParameter) {

        addParameter(toneSource1.getAmpParameter());
        addParameter(toneSource1.getFreqParameter());
        addParameter(toneSource2.getAmpParameter());
        addParameter(toneSource2.getFreqParameter());
        addParameter(toneSource3.getAmpParameter());
        addParameter(toneSource3.getFreqParameter());
        addParameter(toneSource4.getAmpParameter());
        addParameter(toneSource4.getFreqParameter());

        toneSource1.getAmpParameter()->addListener(this);
        toneSource1.getFreqParameter()->addListener(this);
        toneSource2.getAmpParameter()->addListener(this);
        toneSource2.getFreqParameter()->addListener(this);
        toneSource3.getAmpParameter()->addListener(this);
        toneSource3.getFreqParameter()->addListener(this);
        toneSource4.getAmpParameter()->addListener(this);
        toneSource4.getFreqParameter()->addListener(this);

        mixerAudioSource.addInputSource(toneSource1.get(), false);
        mixerAudioSource.addInputSource(toneSource2.get(), false);
        mixerAudioSource.addInputSource(toneSource3.get(), false);
        mixerAudioSource.addInputSource(toneSource4.get(), false);

        for (auto *parameter : getParameters()) {
            parameterChanged(parameter, dynamic_cast<AudioParameterFloat *>(parameter)->range.convertFrom0to1(parameter->getDefaultValue()));
        }
    }

    ~SineBank() override {
        toneSource1.getAmpParameter()->removeListener(this);
        toneSource1.getFreqParameter()->removeListener(this);
        toneSource2.getAmpParameter()->removeListener(this);
        toneSource2.getFreqParameter()->removeListener(this);
        toneSource3.getAmpParameter()->removeListener(this);
        toneSource3.getFreqParameter()->removeListener(this);
        toneSource4.getAmpParameter()->removeListener(this);
        toneSource4.getFreqParameter()->removeListener(this);
    }

    static String name() { return "Sine Bank"; }

    static PluginDescription getPluginDescription() {
        return DefaultAudioProcessor::getPluginDescription(name(), true, false);
    }

    void parameterChanged(AudioProcessorParameter *parameter, float newValue) override {
        if (parameter == toneSource1.getAmpParameter()) {
            toneSource1.setAmplitude(newValue);
        } else if (parameter == toneSource1.getFreqParameter()) {
            toneSource1.setFrequency(newValue);
        } else if (parameter == toneSource2.getAmpParameter()) {
            toneSource2.setAmplitude(newValue);
        } else if (parameter == toneSource2.getFreqParameter()) {
            toneSource2.setFrequency(newValue);
        } else if (parameter == toneSource3.getAmpParameter()) {
            toneSource3.setAmplitude(newValue);
        } else if (parameter == toneSource3.getFreqParameter()) {
            toneSource3.setFrequency(newValue);
        } else if (parameter == toneSource4.getAmpParameter()) {
            toneSource4.setAmplitude(newValue);
        } else if (parameter == toneSource4.getFreqParameter()) {
            toneSource4.setFrequency(newValue);
        }
    }

    void processBlock(AudioSampleBuffer &buffer, MidiBuffer &midiMessages) override {
        const AudioSourceChannelInfo &channelInfo = AudioSourceChannelInfo(buffer);
        mixerAudioSource.getNextAudioBlock(channelInfo);
    }

private:
    ToneSourceWithParameters toneSource1;
    ToneSourceWithParameters toneSource2;
    ToneSourceWithParameters toneSource3;
    ToneSourceWithParameters toneSource4;

    MixerAudioSource mixerAudioSource;
};
