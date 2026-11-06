#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "core/Layout.h"
#include <array>
#include <atomic>
#include <map>

/** Instrument im Channel Rack. Zeigt den Korg, haelt die Presets und spielt pro Taste und Preset ein Sample.
    Das FL-Skript schickt jede Bewegung als Note, liest Preset, Fest und Lernen und fuehrt die Verknuepfungen aus. */
class NanoProcessor : public juce::AudioProcessor, private juce::AsyncUpdater
{
public:
    NanoProcessor();
    ~NanoProcessor() override;

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
    bool learning() const { return learnParam->load() > 0.5f; }
    void setLearning (bool on);
    int bankFor (int ctl) const { return isLocked (ctl) ? nb::FIXED_BANK : preset(); }
    /** Kanal oder Mixer-Spur, die eine Taste im Preset schaltet. 0 = keine. Das Skript schreibt diese Parameter. */
    int targetOf (int presetIndex, int ctl) const
    {
        const int k = nb::soundButtonIndex (ctl);
        return k < 0 ? 0 : (int) targetParams[(size_t) presetIndex][(size_t) k]->load();
    }
    void setTarget (int presetIndex, int ctl, int code);
    int quantise() const { return (int) quantParam->load(); }
    void setQuantise (int q);
    // Zustand der geschalteten Kanaele im aktuellen Preset: 0 aus, 1 wartet auf den Schlag, 2 an
    std::array<std::atomic<int>, nb::NUM_CTL> channelState;

    // Live-Werte vom Korg, je Preset und eine Bank fuer feste Regler
    std::array<std::array<std::atomic<float>, nb::NUM_CTL>, nb::NUM_BANKS> values;
    std::array<std::atomic<bool>, nb::NUM_CTL> held;
    float valueFor (int ctl) const { return values[(size_t) bankFor (ctl)][(size_t) ctl].load(); }
    std::atomic<int> lastMoved { -1 };
    std::atomic<juce::uint32> moveCount { 0 };
    std::atomic<double> lastMidiTime { 0 };

    // Sound-Plaetze: ein Sample je Taste und Preset, gespeichert wird nur der Pfad
    struct Sample { juce::AudioBuffer<float> data; double rate = 44100; juce::String path, name; };
    const Sample* sampleAt (int bank, int ctl) const { return slots[(size_t) bank][(size_t) ctl].load(); }
    bool loadSample (int bank, int ctl, const juce::File& file, juce::String& error);
    void clearSample (int bank, int ctl);
    void audition (int bank, int ctl) { pendingAudition.store (bank * 64 + ctl); }
    std::atomic<juce::uint32> playCount { 0 };
    std::atomic<int> lastPlayed { -1 };

    // Sample-Ordner: Pfade darunter werden relativ gespeichert und beim Laden dort gesucht
    // Suchordner: am besten dieselben wie links im FL-Browser. Fehlende Sounds werden dort nach Namen gesucht.
    juce::Array<juce::File> searchFolders;
    juce::File sampleRoot() const { return searchFolders.isEmpty() ? juce::File() : searchFolders[0]; }
    void addSearchFolder (const juce::File& f);
    void removeSearchFolder (int i);
    juce::String storePath (const juce::File& f) const;
    juce::File resolvePath (const juce::String& stored);
    struct Missing { int bank, ctl; juce::String path; };
    std::vector<Missing> missing;            // Sounds, die beim letzten Laden fehlten
    int findMissing();                       // sucht erneut in allen Ordnern, gibt die Zahl der gefundenen zurueck

    // Standard: wird in jedes neue, leere Plugin geladen
    juce::File defaultSetFile() const { return appFolder().getChildFile ("Standard.nano"); }
    bool saveDefault() { return saveSet (defaultSetFile()); }

    // Sammlung aller 8 Presets als Datei
    juce::File setFolder() const;
    bool saveSet (const juce::File& file);
    bool loadSet (const juce::File& file);

    // Fuer die Reaktion der Oberflaeche
    std::atomic<float> level { 0 };
    std::atomic<juce::uint32> kicks { 0 };
    std::atomic<double> ppq { 0 }, bpm { 0 };
    std::atomic<bool> playing { false };

    // Einstellungen, nur im Nachrichten-Thread benutzt
    std::array<juce::String, nb::NUM_PRESETS> presetNames;
    juce::String skin { "dm_klassisch" };
    int ledColour = 0, soundColour = 2;
    bool reactive = true, ecoMode = false;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    juce::ValueTree settingsTree (bool withValues) const;
    void applySettings (const juce::ValueTree& s, bool withValues);
    void installSample (int bank, int ctl, std::unique_ptr<Sample> s);
    void handleControl (int ctl, float v, bool fromScript);
    void trigger (int bank, int ctl, float gain);
    void render (juce::AudioBuffer<float>& out, int from, int to);
    static juce::File appFolder();
    void handleAsyncUpdate() override;
    void saveFolders() const;
    std::map<juce::String, juce::File> nameIndex; bool indexBuilt = false; bool stateWasSet = false;

    std::atomic<float>* presetParam = nullptr; std::atomic<float>* learnParam = nullptr; std::atomic<float>* quantParam = nullptr;
    std::array<std::array<std::atomic<float>*, nb::NUM_SOUND_BUTTONS>, nb::NUM_PRESETS> targetParams {};
    std::array<std::atomic<float>*, nb::NUM_CTL> lockParams {};
    std::array<std::array<std::atomic<const Sample*>, nb::NUM_CTL>, nb::NUM_BANKS> slots;
    std::vector<std::unique_ptr<Sample>> owned;
    std::vector<std::pair<std::unique_ptr<Sample>, juce::uint32>> retired;
    juce::AudioFormatManager formats;
    std::atomic<int> pendingAudition { -1 };

    struct Voice { const Sample* s = nullptr; double pos = 0, step = 1; float gain = 1; int bank = -1, ctl = -1; juce::uint32 age = 0; };
    std::array<Voice, 24> voices; juce::uint32 voiceAge = 0; double sr = 44100;
    float lowState = 0, fastEnv = 0, slowEnv = 0, lowCoef = 0, fastAtt = 0, fastRel = 0, slowCoef = 0, levelEnv = 0, levelRel = 0;
    int sinceKick = 0, kickGap = 4410;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NanoProcessor)
};
