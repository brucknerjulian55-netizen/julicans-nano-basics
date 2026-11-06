# name=julicans nano basics
# url=https://github.com/brucknerjulian55-netizen/julicans-nano-basics
#
# Bruecke zwischen Korg nanoKONTROL2 und dem Plugin "julicans nano basics" im Channel Rack.
# - Feste Regler laufen ganz normal durch FL: Link to controller wie immer.
# - Alle anderen Regler fuehrt das Skript selbst aus, je Preset auf einem eigenen Kanal
#   (Preset 1 = Kanal 2 ... Preset 8 = Kanal 9). Verknuepft wird mit "Lernen" im Plugin.
# - Jede Bewegung geht als Note an das Plugin, damit es die Regler zeigt und Samples abspielt.
# - Track < und > schalten die Presets, die S-Taste des Presets leuchtet.

import channels
import device
import general
import midi
import plugins
import ui

PLUGIN_NAME = "julicans nano basics"
NUM_PRESETS = 8
TRACK_LEFT, TRACK_RIGHT = 58, 59
VALUE_NOTE_BASE = 0            # Note 0 + Reglernummer traegt den Wert als Anschlag

# Reihenfolge wie im Plugin (Source/core/Layout.h), Index = Reglernummer
ORDER = ["trackL", "trackR", "cycle", "set", "markL", "markR", "rew", "ff", "stop", "play", "rec"]
for _ch in range(1, 9):
    ORDER += ["knob%d" % _ch, "s%d" % _ch, "m%d" % _ch, "r%d" % _ch, "fader%d" % _ch]
CCS = {"trackL": 58, "trackR": 59, "cycle": 46, "set": 60, "markL": 61, "markR": 62,
       "rew": 43, "ff": 44, "stop": 42, "play": 41, "rec": 45}
for _ch in range(8):
    CCS["knob%d" % (_ch + 1)] = 16 + _ch
    CCS["fader%d" % (_ch + 1)] = _ch
    CCS["s%d" % (_ch + 1)] = 32 + _ch
    CCS["m%d" % (_ch + 1)] = 48 + _ch
    CCS["r%d" % (_ch + 1)] = 64 + _ch
BY_CC = {cc: name for name, cc in CCS.items()}
INDEX = {name: i for i, name in enumerate(ORDER)}
LABELS = {"cycle": "Cycle", "set": "Marker Set", "markL": "Marker zurück", "markR": "Marker vor", "rew": "Zurückspulen",
          "ff": "Vorspulen", "stop": "Stop", "play": "Play", "rec": "Aufnahme"}
for _ch in range(1, 9):
    LABELS["knob%d" % _ch] = "Drehregler %d" % _ch
    LABELS["fader%d" % _ch] = "Fader %d" % _ch
    LABELS["s%d" % _ch] = "S %d" % _ch
    LABELS["m%d" % _ch] = "M %d" % _ch
    LABELS["r%d" % _ch] = "R %d" % _ch
DEFAULT_LOCKED = {"cycle", "set", "markL", "markR", "rew", "ff", "stop", "play", "rec"}
LED_CCS = set([46, 43, 44, 42, 41, 45] + [32 + c for c in range(8)] + [48 + c for c in range(8)] + [64 + c for c in range(8)])

FROM_MIDI_MAX = getattr(midi, "FromMIDI_Max", 1073741824)
REC_FLAGS = getattr(midi, "REC_MIDIController", 1005) | getattr(midi, "REC_UpdateControl", 16)
REC_INVALID = getattr(midi, "REC_InvalidID", 2147483647)


class State:
    def __init__(self):
        self.chan = None           # globaler Kanal-Index des Plugins im Channel Rack
        self.presetParam = -1
        self.learnParam = -1
        self.lockParams = {}
        self.preset = 0
        self.locked = set(DEFAULT_LOCKED)
        self.learning = False
        self.ticks = 0
        self.lastLed = -1
        self.chanOffset = 0        # falls linkToLastTweaked Kanaele anders zaehlt


S = State()


def _p(fn, *a):
    """Ruft Plugin-Funktionen mit globalem Index auf, faellt auf alte FL-Versionen zurueck."""
    try:
        return fn(*a, useGlobalIndex=True)
    except TypeError:
        return fn(*a)


def findPlugin():
    S.chan = None
    try:
        count = channels.channelCount(True)
    except TypeError:
        count = channels.channelCount()
    except Exception:
        return False
    for c in range(count):
        try:
            if not _p(plugins.isValid, c, -1):
                continue
            if _p(plugins.getPluginName, c, -1) != PLUGIN_NAME:
                continue
            S.chan = c
            S.presetParam, S.learnParam, S.lockParams = -1, -1, {}
            byLabel = {"Fest " + LABELS[k]: k for k in LABELS}
            for i in range(_p(plugins.getParamCount, c, -1)):
                name = _p(plugins.getParamName, i, c, -1)
                if name == "Preset":
                    S.presetParam = i
                elif name == "Lernen":
                    S.learnParam = i
                elif name in byLabel:
                    S.lockParams[byLabel[name]] = i
            return True
        except Exception:
            continue
    return False


def readPlugin():
    if S.chan is None:
        return
    try:
        if S.presetParam >= 0:
            v = _p(plugins.getParamValue, S.presetParam, S.chan, -1)
            S.preset = max(0, min(NUM_PRESETS - 1, int(round(v * (NUM_PRESETS - 1)))))
        if S.lockParams:
            S.locked = set(k for k, i in S.lockParams.items() if _p(plugins.getParamValue, i, S.chan, -1) > 0.5)
        S.learning = S.learnParam >= 0 and _p(plugins.getParamValue, S.learnParam, S.chan, -1) > 0.5
    except Exception:
        S.chan = None


def setParam(index, value):
    if S.chan is None or index < 0:
        return
    try:
        _p(plugins.setParamValue, value, index, S.chan, -1)
    except Exception:
        pass


def hint(text):
    try:
        ui.setHintMsg(text)
    except Exception:
        pass


def setPreset(p):
    S.preset = p % NUM_PRESETS
    setParam(S.presetParam, S.preset / float(NUM_PRESETS - 1))
    showLeds()
    hint("nano basics: Preset %d" % (S.preset + 1))


def channelFor(name):
    """Feste Regler: Kanal 1 (Index 0). Preset n: Kanal n + 1."""
    return 0 if name in S.locked else S.preset + 1


def controlId(chan, cc):
    return midi.EncodeRemoteControlID(device.getPortNumber(), chan, cc)


def linkedEvent(chan, cc):
    try:
        e = device.findEventID(controlId(chan, cc), 0)
    except Exception:
        return REC_INVALID
    return e


def learn(name, cc):
    """Verknuepft diesen Regler im aktuellen Preset mit dem zuletzt bewegten FL-Regler."""
    chan = S.preset + 1
    try:
        r = device.linkToLastTweaked(cc, chan + S.chanOffset)
    except Exception:
        hint("nano basics: Lernen braucht FL Studio 21 oder neuer")
        return
    if r == 1:
        hint("nano basics: Erst in FL einen Regler mit der Maus bewegen")
        return
    if r == 2:
        hint("nano basics: Dieser Regler ist in diesem Preset schon belegt")
    # pruefen, auf welchem Kanal FL die Verknuepfung abgelegt hat
    if linkedEvent(chan, cc) == REC_INVALID:
        for off in (-1, 1):
            if linkedEvent(chan + off, cc) != REC_INVALID:
                S.chanOffset = -off
                break
    S.learning = False
    setParam(S.learnParam, 0.0)
    hint("nano basics: %s in Preset %d verknüpft" % (LABELS.get(name, name), S.preset + 1))


def sendToPlugin(name, value):
    if S.chan is None:
        return
    note = VALUE_NOTE_BASE + INDEX[name]
    try:
        channels.midiNoteOn(S.chan, note, 1 + (value * 126) // 127)
        channels.midiNoteOn(S.chan, note, 0)
    except Exception:
        pass


def led(cc, on):
    try:
        if device.isAssigned():
            device.midiOutMsg(0xB0 + (cc << 8) + ((127 if on else 0) << 16))
    except Exception:
        pass


def showLeds():
    if S.lastLed == S.preset:
        return
    for ch in range(8):
        led(32 + ch, ch == S.preset)
    S.lastLed = S.preset


def OnInit():
    findPlugin()
    readPlugin()
    S.lastLed = -1
    showLeds()


def OnDeInit():
    for cc in LED_CCS:
        led(cc, False)


def OnIdle():
    S.ticks += 1
    if S.chan is None:
        if S.ticks % 60 == 0 and findPlugin():
            readPlugin()
            S.lastLed = -1
            showLeds()
        return
    if S.ticks % 10 == 0:
        before = S.preset
        readPlugin()
        if S.preset != before:
            showLeds()


def OnMidiMsg(event):
    if (event.status & 0xF0) != 0xB0:
        return
    cc = event.data1
    if cc in (TRACK_LEFT, TRACK_RIGHT):
        if event.data2 > 0:
            readPlugin()
            setPreset(S.preset + (1 if cc == TRACK_RIGHT else -1))
        event.handled = True
        return
    name = BY_CC.get(cc)
    if name is None:
        return
    readPlugin()
    sendToPlugin(name, event.data2)
    if cc in LED_CCS and not (32 <= cc < 40 and cc - 32 == S.preset):
        led(cc, event.data2 > 0)
    if name in S.locked:
        return                      # FL verarbeitet feste Regler selbst, Link to controller wie gewohnt
    event.handled = True            # globale Verknuepfungen auf Kanal 1 wirken nur fuer feste Regler
    if S.learning:
        learn(name, cc)
        return
    e = linkedEvent(channelFor(name), cc)
    if e != REC_INVALID:
        try:
            general.processRECEvent(e, (event.data2 * FROM_MIDI_MAX) // 127, REC_FLAGS)
        except Exception:
            pass
