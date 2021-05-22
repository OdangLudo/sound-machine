#pragma once

#include "DefaultAudioProcessor.h"

class SineSynth : public DefaultAudioProcessor {
public:
    explicit SineSynth() : DefaultAudioProcessor(getPluginDescription()) {
        const int numVoices = 8;
        for (int i = numVoices; --i >= 0;)
            synth.addVoice(new SineWaveVoice());

        synth.addSound(new SineWaveSound());
    }

    ~SineSynth() override = default;

    static String name() { return "Sine Synth"; }

    static PluginDescription getPluginDescription() {
        return DefaultAudioProcessor::getPluginDescription(name(), true, true);
    }

    void prepareToPlay(double newSampleRate, int) override {
        synth.setCurrentPlaybackSampleRate(newSampleRate);
    }

    void processBlock(AudioBuffer<float> &buffer, MidiBuffer &midiMessages) override {
        const int numSamples = buffer.getNumSamples();

        buffer.clear();
        synth.renderNextBlock(buffer, midiMessages, 0, numSamples);
        buffer.applyGain(0.8f);
    }

private:
    struct SineWaveSound : public SynthesiserSound {
        SineWaveSound() = default;

        bool appliesToNote(int /*midiNoteNumber*/) override { return true; }
        bool appliesToChannel(int /*midiChannel*/) override { return true; }
    };

    struct SineWaveVoice : public SynthesiserVoice {
        SineWaveVoice() : currentAngle(0), angleDelta(0), level(0), tailOff(0) {}

        bool canPlaySound(SynthesiserSound *sound) override {
            return dynamic_cast<SineWaveSound *> (sound) != nullptr;
        }

        void startNote(int midiNoteNumber, float velocity,
                       SynthesiserSound * /*sound*/,
                       int /*currentPitchWheelPosition*/) override {
            currentAngle = 0.0;
            level = velocity * 0.15;
            tailOff = 0.0;

            double cyclesPerSecond = MidiMessage::getMidiNoteInHertz(midiNoteNumber);
            double cyclesPerSample = cyclesPerSecond / getSampleRate();

            angleDelta = cyclesPerSample * 2.0 * double_Pi;
        }

        void stopNote(float /*velocity*/, bool allowTailOff) override {
            if (allowTailOff) {
                // start a tail-off by setting this flag. The render callback will pick up on
                // this and do a fade out, calling clearCurrentNote() when it's finished.
                if (tailOff == 0.0) // we only need to begin a tail-off if it's not already doing so - the
                    // stopNote method could be called more than once.
                    tailOff = 1.0;
            } else {
                // we're being told to stop playing immediately, so reset everything..
                clearCurrentNote();
                angleDelta = 0.0;
            }
        }

        void pitchWheelMoved(int /*newValue*/) override {
            // not implemented for the purposes of this demo!
        }

        void controllerMoved(int /*controllerNumber*/, int /*newValue*/) override {
            // not implemented for the purposes of this demo!
        }

        void renderNextBlock(AudioBuffer<float> &outputBuffer, int startSample, int numSamples) override {
            if (angleDelta != 0.0) {
                if (tailOff > 0) {
                    while (--numSamples >= 0) {
                        const auto currentSample = (float) (sin(currentAngle) * level * tailOff);
                        for (int i = outputBuffer.getNumChannels(); --i >= 0;)
                            outputBuffer.addSample(i, startSample, currentSample);

                        currentAngle += angleDelta;
                        ++startSample;
                        tailOff *= 0.99;
                        if (tailOff <= 0.005) {
                            // tells the synth that this voice has stopped
                            clearCurrentNote();
                            angleDelta = 0.0;
                            break;
                        }
                    }
                } else {
                    while (--numSamples >= 0) {
                        const auto currentSample = (float) (sin(currentAngle) * level);
                        for (int i = outputBuffer.getNumChannels(); --i >= 0;)
                            outputBuffer.addSample(i, startSample, currentSample);

                        currentAngle += angleDelta;
                        ++startSample;
                    }
                }
            }
        }

    private:
        double currentAngle = 0, angleDelta = 0, level = 0, tailOff = 0;
    };

    Synthesiser synth;
};
