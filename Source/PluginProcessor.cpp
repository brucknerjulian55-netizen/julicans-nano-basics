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
    // Lernen: Das Skript verknuepft die naechste Bewegung am Korg mit dem zuletzt bewegten FL-Regler
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "learn", 1 }, "Lernen", false));
    // Wann eine geschaltete Taste einsetzt
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "quant", 1 }, "Einsetzen",
                                                         juce::StringArray { "Sofort", juce::String::fromUTF8 ("Nächster Schlag"), juce::String::fromUTF8 ("Nächster Takt") }, 1));
    // Welche Taste in welchem Preset welchen Kanal schaltet. Das Skript traegt das ein, das Projekt speichert es.
    for (int p = 0; p < NUM_PRESETS; ++p)
        for (int k = 0; k < NUM_SOUND_BUTTONS; ++k)
        {
            const auto& c = controls()[(size_t) soundButtonControl (k)];
            l.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "t" + juce::String (p) + "_" + c.id, 1 },
                                                              juce::String::fromUTF8 ("Ziel ") + juce::String::fromUTF8 (c.label) + " P" + juce::String (p + 1), 0, TARGET_MAX, 0));
        }
    return l;
}

juce::File NanoProcessor::appFolder()
{
    auto f = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("julicans nano basics");
    f.createDirectory();
    return f;
}

NanoProcessor::NanoProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Ausgang", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "NANOBASICS", makeLayout())
{
    formats.registerBasicFormats();
    presetParam = apvts.getRawParameterValue ("preset");
    learnParam = apvts.getRawParameterValue ("learn");
    quantParam = apvts.getRawParameterValue ("quant");
    for (int p = 0; p < NUM_PRESETS; ++p)
        for (int k = 0; k < NUM_SOUND_BUTTONS; ++k)
            targetParams[(size_t) p][(size_t) k] = apvts.getRawParameterValue ("t" + juce::String (p) + "_" + controls()[(size_t) soundButtonControl (k)].id);
    for (auto& c : channelState) c.store (0);
    for (int i = 0; i < NUM_CTL; ++i)
        lockParams[(size_t) i] = controls()[(size_t) i].reserved ? nullptr : apvts.getRawParameterValue (juce::String ("lock_") + controls()[(size_t) i].id);
    for (int b = 0; b < NUM_BANKS; ++b)
        for (int i = 0; i < NUM_CTL; ++i)
        {
            values[(size_t) b][(size_t) i].store (controls()[(size_t) i].kind == FADER ? 0.78f : controls()[(size_t) i].kind == KNOB ? 0.5f : 0.0f);
            slots[(size_t) b][(size_t) i].store (nullptr);
        }
    for (auto& h : held) h.store (false);
    static const char* defNames[NUM_PRESETS] = { "Kicks A", "Kicks B", "Kick-Bass", "Drops", "Live FX", "Melo", "Frei", "Frei" };
    for (int i = 0; i < NUM_PRESETS; ++i) presetNames[(size_t) i] = defNames[i];
    // Suchordner gelten fuer alle Projekte
    if (auto xml = juce::XmlDocument::parse (appFolder().getChildFile ("settings.xml")))
    {
        if (xml->hasAttribute ("sampleRoot")) searchFolders.add (juce::File (xml->getStringAttribute ("sampleRoot")));
        for (auto* f : xml->getChildWithTagNameIterator ("Folder"))
        {
            const juce::File d (f->getStringAttribute ("path"));
            if (! searchFolders.contains (d)) searchFolders.add (d);
        }
    }
    // Neues, leeres Plugin: nach dem Laden den Standard uebernehmen, falls FL keinen Projektzustand liefert
    triggerAsyncUpdate();
}

void NanoProcessor::handleAsyncUpdate()
{
    if (! stateWasSet && defaultSetFile().existsAsFile()) loadSet (defaultSetFile());
}
NanoProcessor::~NanoProcessor()
{
    for (auto& bank : slots) for (auto& s : bank) s.store (nullptr);
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
void NanoProcessor::setTarget (int presetIndex, int ctl, int code)
{
    const int k = soundButtonIndex (ctl); if (k < 0) return;
    auto* x = apvts.getParameter ("t" + juce::String (presetIndex) + "_" + controls()[(size_t) ctl].id);
    x->beginChangeGesture(); x->setValueNotifyingHost (x->convertTo0to1 ((float) code)); x->endChangeGesture();
    if (code == 0) channelState[(size_t) ctl].store (0);
}
void NanoProcessor::setQuantise (int q)
{
    auto* x = apvts.getParameter ("quant");
    x->beginChangeGesture(); x->setValueNotifyingHost (x->convertTo0to1 ((float) juce::jlimit (0, 2, q))); x->endChangeGesture();
}
void NanoProcessor::setLearning (bool on)
{
    auto* x = apvts.getParameter ("learn");
    x->beginChangeGesture(); x->setValueNotifyingHost (on ? 1.0f : 0.0f); x->endChangeGesture();
}

bool NanoProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void NanoProcessor::prepareToPlay (double rate, int)
{
    sr = rate;
    const auto co = [rate] (double ms) { return (float) std::exp (-1.0 / (ms * 0.001 * rate)); };
    lowCoef = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * 140.0 / rate);
    fastAtt = co (1.0); fastRel = co (35.0); slowCoef = co (280.0); levelRel = co (120.0);
    kickGap = (int) (0.09 * rate);
    lowState = fastEnv = slowEnv = levelEnv = 0; sinceKick = kickGap;
    for (auto& v : voices) v.s = nullptr;
}

// ---------------- Samples ----------------------------------------------------
void NanoProcessor::saveFolders() const
{
    juce::XmlElement x ("NanoBasics");
    for (const auto& f : searchFolders) x.createNewChildElement ("Folder")->setAttribute ("path", f.getFullPathName());
    x.writeTo (appFolder().getChildFile ("settings.xml"));
}
void NanoProcessor::addSearchFolder (const juce::File& f)
{
    if (f.isDirectory() && ! searchFolders.contains (f)) { searchFolders.add (f); indexBuilt = false; saveFolders(); }
}
void NanoProcessor::removeSearchFolder (int i)
{
    if (i >= 0 && i < searchFolders.size()) { searchFolders.remove (i); indexBuilt = false; saveFolders(); }
}
juce::String NanoProcessor::storePath (const juce::File& f) const
{
    for (const auto& d : searchFolders)
        if (d.isDirectory() && f.isAChildOf (d)) return f.getRelativePathFrom (d).replaceCharacter ('\\', '/');
    return f.getFullPathName();
}
juce::File NanoProcessor::resolvePath (const juce::String& stored)
{
    if (stored.isEmpty()) return {};
    if (juce::File::isAbsolutePath (stored) && juce::File (stored).existsAsFile()) return juce::File (stored);
    if (! juce::File::isAbsolutePath (stored))
        for (const auto& d : searchFolders)
            if (const auto f = d.getChildFile (stored); f.existsAsFile()) return f;
    // Verschoben oder anderer Rechner: in allen Suchordnern nach dem Dateinamen suchen. Das Verzeichnis wird einmal aufgebaut.
    if (! indexBuilt)
    {
        nameIndex.clear();
        for (const auto& d : searchFolders)
            if (d.isDirectory())
                for (const auto& f : d.findChildFiles (juce::File::findFiles, true, "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3"))
                    nameIndex.emplace (f.getFileName().toLowerCase(), f);
        indexBuilt = true;
    }
    const auto name = stored.replaceCharacter ('\\', '/').fromLastOccurrenceOf ("/", false, false).toLowerCase();
    const auto it = nameIndex.find (name);
    return it != nameIndex.end() && it->second.existsAsFile() ? it->second : juce::File();
}
int NanoProcessor::findMissing()
{
    indexBuilt = false;
    int found = 0;
    std::vector<Missing> still;
    for (const auto& m : missing)
    {
        const auto f = resolvePath (m.path);
        juce::String err;
        if (f.existsAsFile() && loadSample (m.bank, m.ctl, f, err)) ++found; else still.push_back (m);
    }
    missing = still;
    return found;
}

void NanoProcessor::installSample (int bank, int ctl, std::unique_ptr<Sample> s)
{
    const auto now = juce::Time::getMillisecondCounter();
    retired.erase (std::remove_if (retired.begin(), retired.end(), [now] (const auto& r) { return now - r.second > 60000; }), retired.end());
    const Sample* old = slots[(size_t) bank][(size_t) ctl].exchange (s.get());
    if (s != nullptr) owned.push_back (std::move (s));
    if (old != nullptr)
        for (auto it = owned.begin(); it != owned.end(); ++it)
            if (it->get() == old) { retired.push_back ({ std::move (*it), now }); owned.erase (it); break; }
}

bool NanoProcessor::loadSample (int bank, int ctl, const juce::File& file, juce::String& error)
{
    std::unique_ptr<juce::AudioFormatReader> r (formats.createReaderFor (file));
    if (r == nullptr) { error = juce::String::fromUTF8 ("Diese Datei lässt sich nicht lesen: ") + file.getFileName(); return false; }
    const auto len = (int) juce::jmin<juce::int64> (r->lengthInSamples, (juce::int64) (r->sampleRate * 20.0));
    if (len <= 0) { error = juce::String::fromUTF8 ("Die Datei ist leer."); return false; }
    auto s = std::make_unique<Sample>();
    s->data.setSize (2, len);
    r->read (&s->data, 0, len, 0, true, true);
    if (r->numChannels == 1) s->data.copyFrom (1, 0, s->data, 0, 0, len);
    s->rate = r->sampleRate; s->path = storePath (file); s->name = file.getFileNameWithoutExtension();
    installSample (bank, ctl, std::move (s));
    return true;
}
void NanoProcessor::clearSample (int bank, int ctl) { installSample (bank, ctl, nullptr); }

void NanoProcessor::trigger (int bank, int ctl, float gain)
{
    const Sample* s = slots[(size_t) bank][(size_t) ctl].load();
    if (s == nullptr) return;
    Voice* v = nullptr;
    for (auto& x : voices) if (x.s != nullptr && x.bank == bank && x.ctl == ctl) { v = &x; break; }     // gleicher Platz: neu starten
    if (v == nullptr) for (auto& x : voices) if (x.s == nullptr) { v = &x; break; }
    if (v == nullptr) { v = &voices[0]; for (auto& x : voices) if (x.age < v->age) v = &x; }
    *v = { s, 0.0, s->rate / sr, gain, bank, ctl, ++voiceAge };
    playCount.fetch_add (1); lastPlayed.store (ctl);
}

void NanoProcessor::render (juce::AudioBuffer<float>& out, int from, int to)
{
    const int chs = out.getNumChannels();
    for (auto& v : voices)
    {
        if (v.s == nullptr) continue;
        const int n = v.s->data.getNumSamples();
        const float* l = v.s->data.getReadPointer (0); const float* r = v.s->data.getReadPointer (1);
        for (int i = from; i < to; ++i)
        {
            const int a = (int) v.pos;
            if (a + 1 >= n) { v.s = nullptr; break; }
            const float f = (float) (v.pos - a);
            const float x = (l[a] + (l[a + 1] - l[a]) * f) * v.gain, y = (r[a] + (r[a + 1] - r[a]) * f) * v.gain;
            if (chs > 1) { out.addSample (0, i, x); out.addSample (1, i, y); } else out.addSample (0, i, 0.5f * (x + y));
            v.pos += v.step;
        }
    }
}

void NanoProcessor::handleControl (int ctl, float v, bool fromScript)
{
    const auto& c = controls()[(size_t) ctl];
    const bool wasDown = held[(size_t) ctl].load();
    held[(size_t) ctl].store (v > 0.5f);
    if (c.reserved) return;
    values[(size_t) bankFor (ctl)][(size_t) ctl].store (v);
    lastMoved.store (ctl); moveCount.fetch_add (1);
    lastMidiTime.store (juce::Time::getMillisecondCounterHiRes());
    // Taste gedrueckt: Sound des aktuellen Presets spielen
    if (fromScript && c.kind == BUTTON && v > 0.5f && ! wasDown) trigger (bankFor (ctl), ctl, 1.0f);
}

void NanoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    buffer.clear();
    const int n = buffer.getNumSamples();
    if (const int a = pendingAudition.exchange (-1); a >= 0) trigger (a / 64, a % 64, 1.0f);

    int pos = 0;
    for (const auto m : midi)
    {
        const auto msg = m.getMessage();
        const int at = juce::jlimit (0, n, m.samplePosition);
        if (at > pos) { render (buffer, pos, at); pos = at; }
        if (msg.isNoteOn())
        {
            const int note = msg.getNoteNumber();
            if (note >= VALUE_NOTE_BASE && note < VALUE_NOTE_BASE + NUM_CTL)
                handleControl (note - VALUE_NOTE_BASE, valueFromVelocity (msg.getVelocity()), true);
            else if (note >= STATE_NOTE_BASE && note < STATE_NOTE_BASE + NUM_SOUND_BUTTONS)
            {
                const int v = msg.getVelocity();
                channelState[(size_t) soundButtonControl (note - STATE_NOTE_BASE)].store (v >= 100 ? 2 : v >= 32 ? 1 : 0);
            }
            else if (const int ctl = controlForNote (note); ctl >= 0)
                trigger (bankFor (ctl), ctl, msg.getFloatVelocity());
        }
        else if (msg.isController())
        {
            // Direkt vom Korg ueber den MIDI-Port: nur anzeigen, Sounds kommen ueber das Skript
            if (const int ctl = controlForCc (msg.getControllerNumber()); ctl >= 0)
                handleControl (ctl, (float) msg.getControllerValue() / 127.0f, false);
        }
    }
    if (pos < n) render (buffer, pos, n);
    midi.clear();

    if (auto* ph = getPlayHead())
        if (auto p = ph->getPosition())
        {
            playing.store (p->getIsPlaying());
            if (auto b = p->getBpm()) bpm.store (*b);
            if (auto q = p->getPpqPosition()) ppq.store (*q);
        }

    // Kick erkennen im eigenen Ausgang: Tiefpass, dann schnelle gegen langsame Huellkurve
    const int chs = buffer.getNumChannels();
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

// ---------------- Speichern ----------------------------------------------------
juce::ValueTree NanoProcessor::settingsTree (bool withValues) const
{
    juce::ValueTree s ("SETTINGS");
    for (int i = 0; i < NUM_PRESETS; ++i) s.setProperty ("name" + juce::String (i), presetNames[(size_t) i], nullptr);
    s.setProperty ("skin", skin, nullptr); s.setProperty ("led", ledColour, nullptr); s.setProperty ("soundLed", soundColour, nullptr);
    s.setProperty ("reactive", reactive, nullptr); s.setProperty ("eco", ecoMode, nullptr);
    juce::StringArray locks;
    for (int i = 0; i < NUM_CTL; ++i) if (isLocked (i)) locks.add (controls()[(size_t) i].id);
    s.setProperty ("locks", locks.joinIntoString (","), nullptr);
    for (int b = 0; b < NUM_BANKS; ++b)
        for (int i = 0; i < NUM_CTL; ++i)
            if (const auto* smp = sampleAt (b, i))
            {
                juce::ValueTree x ("SLOT");
                x.setProperty ("bank", b, nullptr); x.setProperty ("ctl", controls()[(size_t) i].id, nullptr); x.setProperty ("path", smp->path, nullptr);
                s.addChild (x, -1, nullptr);
            }
    if (withValues)
        for (int b = 0; b < NUM_BANKS; ++b)
        {
            juce::StringArray v;
            for (int i = 0; i < NUM_CTL; ++i) v.add (juce::String (values[(size_t) b][(size_t) i].load(), 3));
            s.setProperty ("bank" + juce::String (b), v.joinIntoString (","), nullptr);
        }
    return s;
}

void NanoProcessor::applySettings (const juce::ValueTree& s, bool withValues)
{
    for (int i = 0; i < NUM_PRESETS; ++i) presetNames[(size_t) i] = s.getProperty ("name" + juce::String (i), presetNames[(size_t) i]).toString();
    skin = s.getProperty ("skin", skin).toString(); ledColour = (int) s.getProperty ("led", ledColour); soundColour = (int) s.getProperty ("soundLed", soundColour);
    reactive = (bool) s.getProperty ("reactive", reactive); ecoMode = (bool) s.getProperty ("eco", ecoMode);
    if (withValues)
        for (int b = 0; b < NUM_BANKS; ++b)
        {
            const auto v = juce::StringArray::fromTokens (s.getProperty ("bank" + juce::String (b)).toString(), ",", "");
            for (int i = 0; i < juce::jmin (NUM_CTL, v.size()); ++i) values[(size_t) b][(size_t) i].store (juce::jlimit (0.0f, 1.0f, v[i].getFloatValue()));
        }
    missing.clear(); indexBuilt = false;
    for (int b = 0; b < NUM_BANKS; ++b) for (int i = 0; i < NUM_CTL; ++i) if (sampleAt (b, i) != nullptr) clearSample (b, i);
    for (auto x : s)
    {
        if (! x.hasType ("SLOT")) continue;
        const int b = juce::jlimit (0, NUM_BANKS - 1, (int) x.getProperty ("bank"));
        int ctl = -1;
        for (int i = 0; i < NUM_CTL; ++i) if (x.getProperty ("ctl").toString() == controls()[(size_t) i].id) ctl = i;
        if (ctl < 0) continue;
        const auto stored = x.getProperty ("path").toString();
        const auto f = resolvePath (stored);
        juce::String err;
        if (! f.existsAsFile() || ! loadSample (b, ctl, f, err)) missing.push_back ({ b, ctl, stored });
    }
}

void NanoProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.removeChild (state.getChildWithName ("SETTINGS"), nullptr);
    state.addChild (settingsTree (true), -1, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void NanoProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    stateWasSet = true;
    auto state = juce::ValueTree::fromXml (*xml);
    const auto s = state.getChildWithName ("SETTINGS");
    if (s.isValid()) state.removeChild (s, nullptr);
    apvts.replaceState (state);
    if (s.isValid()) applySettings (s, true);
    setLearning (false);
}

juce::File NanoProcessor::setFolder() const { auto f = appFolder().getChildFile ("Presets"); f.createDirectory(); return f; }

bool NanoProcessor::saveSet (const juce::File& file)
{
    auto s = settingsTree (false);
    s.setProperty ("format", 1, nullptr);
    auto xml = s.createXml();
    return xml != nullptr && xml->writeTo (file);
}
bool NanoProcessor::loadSet (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("SETTINGS")) return false;
    const auto s = juce::ValueTree::fromXml (*xml);
    const auto locks = juce::StringArray::fromTokens (s.getProperty ("locks").toString(), ",", "");
    for (int i = 0; i < NUM_CTL; ++i) if (! controls()[(size_t) i].reserved) setLocked (i, locks.contains (controls()[(size_t) i].id));
    applySettings (s, false);
    return true;
}

juce::AudioProcessorEditor* NanoProcessor::createEditor() { return new NanoEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NanoProcessor(); }
