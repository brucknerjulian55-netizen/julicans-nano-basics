# name=julicans nano basics
# url=https://github.com/brucknerjulian55-netizen/julicans-nano-basics
#
# Brücke zwischen Korg nanoKONTROL2 und dem Plugin "julicans nano basics".
# Jedes Preset bekommt einen eigenen MIDI-Kanal. FL sieht dadurch pro Preset andere Regler,
# und "Link to controller" funktioniert ganz normal. Feste Regler bleiben auf Kanal 1.
# Track < und > schalten die Presets um. Die S-Taste mit der Nummer des Presets leuchtet.

import device
import mixer
import plugins
import ui

PLUGIN_NAME = "julicans nano basics"
NUM_PRESETS = 8
TRACK_LEFT, TRACK_RIGHT = 58, 59

# Standard-CCs des nanoKONTROL2 im CC-Modus, gleiche Namen wie im Plugin
CONTROLS = {46: "cycle", 60: "set", 61: "markL", 62: "markR", 43: "rew", 44: "ff", 42: "stop", 41: "play", 45: "rec"}
for _ch in range(8):
    CONTROLS[16 + _ch] = "knob%d" % (_ch + 1)
    CONTROLS[_ch] = "fader%d" % (_ch + 1)
    CONTROLS[32 + _ch] = "s%d" % (_ch + 1)
    CONTROLS[48 + _ch] = "m%d" % (_ch + 1)
    CONTROLS[64 + _ch] = "r%d" % (_ch + 1)
LABELS = {"cycle": "Cycle", "set": "Marker Set", "markL": "Marker zurück", "markR": "Marker vor", "rew": "Zurückspulen",
          "ff": "Vorspulen", "stop": "Stop", "play": "Play", "rec": "Aufnahme"}
for _ch in range(8):
    n = _ch + 1
    LABELS["knob%d" % n] = "Drehregler %d" % n
    LABELS["fader%d" % n] = "Fader %d" % n
    LABELS["s%d" % n] = "S %d" % n
    LABELS["m%d" % n] = "M %d" % n
    LABELS["r%d" % n] = "R %d" % n
DEFAULT_LOCKED = {"cycle", "set", "markL", "markR", "rew", "ff", "stop", "play", "rec"}
LED_CCS = set([46, 43, 44, 42, 41, 45] + [32 + c for c in range(8)] + [48 + c for c in range(8)] + [64 + c for c in range(8)])


class State:
    def __init__(self):
        self.where = None          # (Mixer-Spur, Slot) des Plugins
        self.presetParam = -1
        self.lockParams = {}       # Reglername -> Parameterindex
        self.preset = 0
        self.locked = set(DEFAULT_LOCKED)
        self.ticks = 0
        self.lastLed = -1


S = State()


def channelFor(name):
    """Feste Regler: Kanal 1 (Index 0). Preset n: Kanal n + 1."""
    return 0 if name in S.locked else S.preset + 1


def findPlugin():
    S.where = None
    try:
        for track in range(mixer.trackCount()):
            for slot in range(10):
                if not plugins.isValid(track, slot):
                    continue
                if plugins.getPluginName(track, slot) != PLUGIN_NAME:
                    continue
                S.where = (track, slot)
                S.presetParam = -1
                S.lockParams = {}
                byLabel = {"Fest " + LABELS[k]: k for k in LABELS}
                for i in range(plugins.getParamCount(track, slot)):
                    name = plugins.getParamName(i, track, slot)
                    if name == "Preset":
                        S.presetParam = i
                    elif name in byLabel:
                        S.lockParams[byLabel[name]] = i
                return True
    except Exception:
        S.where = None
    return False


def readPlugin():
    if S.where is None:
        return
    track, slot = S.where
    try:
        if not plugins.isValid(track, slot) or plugins.getPluginName(track, slot) != PLUGIN_NAME:
            S.where = None
            return
        if S.presetParam >= 0:
            v = plugins.getParamValue(S.presetParam, track, slot)
            S.preset = max(0, min(NUM_PRESETS - 1, int(round(v * (NUM_PRESETS - 1)))))
        if S.lockParams:
            S.locked = set(k for k, i in S.lockParams.items() if plugins.getParamValue(i, track, slot) > 0.5)
    except Exception:
        S.where = None


def setPreset(p):
    S.preset = p % NUM_PRESETS
    if S.where is not None and S.presetParam >= 0:
        try:
            track, slot = S.where
            plugins.setParamValue(S.preset / float(NUM_PRESETS - 1), S.presetParam, track, slot)
        except Exception:
            pass
    showLeds()
    try:
        ui.setHintMsg("nano basics: Preset %d" % (S.preset + 1))
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
    # etwa alle halbe Sekunde mit dem Plugin abgleichen, alle drei Sekunden neu suchen, falls es fehlt
    S.ticks += 1
    if S.where is None:
        if S.ticks % 60 == 0 and findPlugin():
            readPlugin()
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
    name = CONTROLS.get(cc)
    if name is None:
        return
    chan = channelFor(name)
    event.midiChan = chan
    event.status = 0xB0 + chan
    # LED der gedrueckten Taste mitleuchten lassen, ohne die Preset-Anzeige zu loeschen
    if cc in LED_CCS and not (32 <= cc < 40 and cc - 32 == S.preset):
        led(cc, event.data2 > 0)
    # nicht als erledigt markieren: FL verarbeitet die Nachricht mit dem neuen Kanal weiter
