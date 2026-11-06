#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace nb;

juce::AudioProcessorValueTreeState::ParameterLayout NanoProcessor::makeLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    juce::StringArray names; for (int i = 1; i <= NUM_PRESETS; ++i) names.add ("Preset " + juce::String (i));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "preset", 1 }, "Preset", names, 0));
    // Ein Schalter je Regler: fest heisst, er macht in jedem Preset dasselbe. Das Skript liest diese Werte.
    for (int i = 0; i < NUM_CTL; ++i)
    {
        const auto& c = controls()[(size_t) i];
        if (c.reserved) continue;
        l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { juce::String ("lock_") + c.id, 1 },
                                                            juce::String::fromUTF8 ("Fest ") + juce::String::fromUTF8 (c.label), lockedByDefault (i)));
    }
    return l;
}

NanoProcessor::NanoProcessor()
    : AudioProcessor (BusesProperties().withInput ("Eingang", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Ausgang", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "NANOBASICS", makeLayout())
{
    presetParam = apvts.getRawParameterValue ("preset");
    for (int i = 0; i < NUM_CTL; ++i)
        lockParams[(size_t) i] = controls()[(size_t) i].reserved ? nullptr : apvts.getRawParameterValue (juce::String ("lock_") + controls()[(size_t) i].id);
    for (auto& bank : values) for (auto& v : bank) v.store (0.0f);
    // Startwerte, damit Regler nicht alle auf null stehen, bevor der Korg etwas sendet
    for (int b = 0; b < NUM_BANKS; ++b)
        for (int i = 0; i < NUM_CTL; ++i)
            if (controls()[(size_t) i].kind != BUTTON) values[(size_t) b][(size_t) i].store (controls()[(size_t) i].kind == FADER ? 0.78f : 0.5f);
    for (auto& h : held) h.store (false);
    static const char* defNames[NUM_PRESETS] = { "Kicks A", "Kicks B", "Kick-Bass", "Drops", "Live FX", "Melo", "Frei", "Frei" };
    for (int i = 0; i < NUM_PRESETS; ++i) presetNames[(size_t) i] = defNames[i];
}

void NanoProcessor::setPreset (int p)
{
    auto* x = apvts.getParameter ("preset");
    x->beginChangeGesture(); x->setValueNotifyingHost (x->convertTo0to1 ((float) juce::jlimit (0, NUM_PRESETS - 1, p))); x->endChangeGesture();
}
void NanoProcessor::setLocked (int ctl, bool on)
{
    if (auto* x = lockParameter (ctl)) { x->beginChangeGesture(); x->setValueNotifyingHost (on ? 1.0f : 0.0f); x->endChangeGesture(); }
}

bool NanoProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return l.getMainInputChannelSet() == out;
}

void NanoProcessor::prepareToPlay (double sr, int)
{
    const auto co = [sr] (double ms) { return (float) std::exp (-1.0 / (ms * 0.001 * sr)); };
    lowCoef = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * 140.0 / sr);
    fastAtt = co (1.0); fastRel = co (35.0); slowCoef = co (280.0); levelRel = co (120.0);
    kickGap = (int) (0.09 * sr);
    lowState = fastEnv = slowEnv = levelEnv = 0; sinceKick = kickGap;
}

void NanoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    // Regler vom Korg mitlesen. Der Kanal ist egal, weil das Skript ihn je Preset umschreibt.
    for (const auto m : midi)
    {
        const auto msg = m.getMessage();
        if (! msg.isController()) continue;
        const int ctl = controlForCc (msg.getControllerNumber());
        if (ctl < 0) continue;
        const float v = (float) msg.getControllerValue() / 127.0f;
        held[(size_t) ctl].store (v > 0.5f);
        if (controls()[(size_t) ctl].reserved) continue;
        values[(size_t) (isLocked (ctl) ? FIXED_BANK : preset())][(size_t) ctl].store (v);
        lastMoved.store (ctl); moveCount.fetch_add (1);
        lastMidiTime.store (juce::Time::getMillisecondCounterHiRes());
    }
    midi.clear();

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            playing.store (pos->getIsPlaying());
            if (auto b = pos->getBpm()) bpm.store (*b);
            if (auto q = pos->getPpqPosition()) ppq.store (*q);
        }

    // Kick erkennen: Tiefpass, dann schnelle gegen langsame Huellkurve
    const int n = buffer.getNumSamples(), chs = buffer.getNumChannels();
    if (chs == 0) return;
    const float* l = buffer.getReadPointer (0); const float* r = buffer.getReadPointer (chs > 1 ? 1 : 0);
    for (int i = 0; i < n; ++i)
    {
        const float x = 0.5f * (l[i] + r[i]);
        lowState = x + lowCoef * (lowState - x);
        const float a = std::fabs (lowState);
        fastEnv = a > fastEnv ? fastAtt * fastEnv + (1 - fastAtt) * a : fastRel * fastEnv + (1 - fastRel) * a;
        slowEnv = slowCoef * slowEnv + (1 - slowCoef) * a;
        const float ax = std::fabs (x);
        levelEnv = ax > levelEnv ? ax : levelRel * levelEnv + (1 - levelRel) * ax;
        if (++sinceKick > kickGap && fastEnv > 0.04f && fastEnv > slowEnv * 1.7f) { kicks.fetch_add (1); sinceKick = 0; }
    }
    if (! std::isfinite (lowState)) lowState = fastEnv = slowEnv = levelEnv = 0;
    level.store (juce::jlimit (0.0f, 1.0f, levelEnv));
}

void NanoProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    juce::ValueTree s ("SETTINGS");
    for (int i = 0; i < NUM_PRESETS; ++i) s.setProperty ("name" + juce::String (i), presetNames[(size_t) i], nullptr);
    s.setProperty ("skin", skin, nullptr); s.setProperty ("led", ledColour, nullptr);
    s.setProperty ("reactive", reactive, nullptr); s.setProperty ("eco", ecoMode, nullptr);
    for (int b = 0; b < NUM_BANKS; ++b)
    {
        juce::StringArray v;
        for (int i = 0; i < NUM_CTL; ++i) v.add (juce::String (values[(size_t) b][(size_t) i].load(), 3));
        s.setProperty ("bank" + juce::String (b), v.joinIntoString (","), nullptr);
    }
    state.removeChild (state.getChildWithName ("SETTINGS"), nullptr);
    state.addChild (s, -1, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void NanoProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (auto s = state.getChildWithName ("SETTINGS"); s.isValid())
    {
        for (int i = 0; i < NUM_PRESETS; ++i) presetNames[(size_t) i] = s.getProperty ("name" + juce::String (i), presetNames[(size_t) i]).toString();
        skin = s.getProperty ("skin", skin).toString(); ledColour = (int) s.getProperty ("led", ledColour);
        reactive = (bool) s.getProperty ("reactive", reactive); ecoMode = (bool) s.getProperty ("eco", ecoMode);
        for (int b = 0; b < NUM_BANKS; ++b)
        {
            const auto v = juce::StringArray::fromTokens (s.getProperty ("bank" + juce::String (b)).toString(), ",", "");
            for (int i = 0; i < juce::jmin (NUM_CTL, v.size()); ++i) values[(size_t) b][(size_t) i].store (juce::jlimit (0.0f, 1.0f, v[i].getFloatValue()));
        }
        state.removeChild (s, nullptr);
    }
    apvts.replaceState (state);
}

juce::AudioProcessorEditor* NanoProcessor::createEditor() { return new NanoEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NanoProcessor(); }
