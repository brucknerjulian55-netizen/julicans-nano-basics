#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "core/Layout.h"
#include <array>
#include <atomic>

/** Liegt als Effekt im Mixer. Das Audio laeuft unveraendert durch und dient nur dazu, die Oberflaeche reagieren zu lassen.
    Die eigentliche Umschaltung der Regler macht das FL-Skript. Es liest dafuer die Parameter Preset und Fest. */
class NanoProcessor : public juce::AudioProcessor
{
public:
    NanoProcessor();
    ~NanoProcessor() override = default;

    void prepareToPlay (double sampleRate, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    int preset() const { return juce::jlimit (0, nb::NUM_PRESETS - 1, (int) presetParam->load()); }
    void setPreset (int p);
    bool isLocked (int ctl) const { return lockParams[(size_t) ctl] != nullptr && lockParams[(size_t) ctl]->load() > 0.5f; }
    void setLocked (int ctl, bool on);
    juce::RangedAudioParameter* lockParameter (int ctl) const { return apvts.getParameter (juce::String ("lock_") + nb::controls()[(size_t) ctl].id); }

    // Live-Werte vom Korg, je Preset und eine Bank fuer feste Regler
    std::array<std::array<std::atomic<float>, nb::NUM_CTL>, nb::NUM_BANKS> values;
    std::array<std::atomic<bool>, nb::NUM_CTL> held;
    float valueFor (int ctl) const { return values[(size_t) (isLocked (ctl) ? nb::FIXED_BANK : preset())][(size_t) ctl].load(); }
    std::atomic<int> lastMoved { -1 };
    std::atomic<juce::uint32> moveCount { 0 };
    std::atomic<double> lastMidiTime { 0 };

    // Fuer die Reaktion der Oberflaeche
    std::atomic<float> level { 0 };
    std::atomic<juce::uint32> kicks { 0 };
    std::atomic<double> ppq { 0 }, bpm { 0 };
    std::atomic<bool> playing { false };

    // Einstellungen, nur im Nachrichten-Thread benutzt
    std::array<juce::String, nb::NUM_PRESETS> presetNames;
    juce::String skin { "dm_klassisch" };
    int ledColour = 0;
    bool reactive = true, ecoMode = false;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    std::atomic<float>* presetParam = nullptr;
    std::array<std::atomic<float>*, nb::NUM_CTL> lockParams {};
    float lowState = 0, fastEnv = 0, slowEnv = 0, lowCoef = 0, fastAtt = 0, fastRel = 0, slowCoef = 0, levelEnv = 0, levelRel = 0;
    int sinceKick = 0, kickGap = 4410;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NanoProcessor)
};
