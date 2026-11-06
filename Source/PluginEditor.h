#pragma once
#include "PluginProcessor.h"
#include "core/Skins.h"
#include "ui/UpdateCheck.h"

class NanoEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit NanoEditor (NanoProcessor&);
    ~NanoEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    struct Ui
    {
        explicit Ui (NanoProcessor& proc) : p (proc) {}
        NanoProcessor& p;
        int mode = 0;            // 0 Belegen, 1 Fest markieren
        int selected = -1;
        int category = 1;
        bool preview = false;    // Takt-Vorschau ohne Musik
        nb::Motion motion;
        std::function<void()> skinChanged, refresh;
    };

private:
    void timerCallback() override;
    NanoProcessor& proc;
    Ui ui;
    std::unique_ptr<juce::Component> root;
    std::unique_ptr<juce::TooltipWindow> tips;
    juce::uint32 seenMoves = 0, seenKicks = 0; int seenPreset = -1;
    double lastTick = 0, lastKickTime = -10, previewClock = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NanoEditor)
};
