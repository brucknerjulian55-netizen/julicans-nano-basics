# name=julicans nano basics
# url=https://github.com/brucknerjulian55-netizen/julicans-nano-basics
#
# Bruecke zwischen Korg nanoKONTROL2 und dem Plugin "julicans nano basics" im Channel Rack.
#
# Tasten schalten Kanaele: In jedem Preset kann jede Taste einen Kanal aus dem Channel Rack
# oder eine Mixer-Spur stumm und wieder laut schalten, auf den naechsten Schlag genau.
# Zuweisen: Kanal anklicken (oder Mixer-Spur waehlen), SET halten, Taste druecken.
# Nochmal SET + Taste mit demselben Kanal loescht die Zuweisung.
#
# Regler: Feste Regler laufen normal durch FL (Link to controller). Alle anderen fuehrt das Skript
# selbst aus, je Preset auf einem eigenen Kanal. Verknuepft wird mit "Lernen" im Plugin.
#
# Track < und > schalten die Presets. Jede Bewegung geht als Note an das Plugin, damit es alles anzeigt.

import channels
import device
import general
import midi
import mixer
import plugins
import transport
import ui

PLUGIN_NAME = "julicans nano basics"
NUM_PRESETS = 8
TRACK_LEFT, TRACK_RIGHT, SET_CC = 58, 59, 60
VALUE_NOTE_BASE = 0            # Note 0 + Reglernummer traegt den Wert als Anschlag
STATE_NOTE_BASE = 93           # Note 93 + Tastennummer: 1 = aus, 64 = wartet, 127 = an
TARGET_MAX, TARGET_MIXER = 2047, 1024

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
RESERVED = {"trackL", "trackR", "set"}
IS_BUTTON = lambda n: not (n.startswith("knob") or n.startswith("fader"))
BUTTONS = [n for n in ORDER if IS_BUTTON(n) and n not in RESERVED]       # 32 Tasten, Reihenfolge wie im Plugin
BUTTON_INDEX = {n: i for i, n in enumerate(BUTTONS)}
LABELS = {"cycle": "Cycle", "markL": "Marker zurück", "markR": "Marker vor", "rew": "Zurückspulen",
          "ff": "Vorspulen", "stop": "Stop", "play": "Play", "rec": "Aufnahme"}
for _ch in range(1, 9):
    LABELS["knob%d" % _ch] = "Drehregler %d" % _ch
    LABELS["fader%d" % _ch] = "Fader %d" % _ch
    LABELS["s%d" % _ch] = "S %d" % _ch
    LABELS["m%d" % _ch] = "M %d" % _ch
    LABELS["r%d" % _ch] = "R %d" % _ch
DEFAULT_LOCKED = {"cycle", "markL", "markR", "rew", "ff", "stop", "play", "rec"}
LED_CCS = set([46, 43, 44, 42, 41, 45] + [32 + c for c in range(8)] + [48 + c for c in range(8)] + [64 + c for c in range(8)])

FROM_MIDI_MAX = getattr(midi, "FromMIDI_Max", 1073741824)
REC_FLAGS = getattr(midi, "REC_MIDIController", 1005) | getattr(midi, "REC_UpdateControl", 16)
REC_INVALID = getattr(midi, "REC_InvalidID", 2147483647)
ABSTICKS = getattr(midi, "SONGLENGTH_ABSTICKS", 2)


class State:
    def __init__(self):
        self.chan = None           # globaler Kanal-Index des Plugins im Channel Rack
        self.presetParam = self.learnParam = self.quantParam = -1
        self.lockParams = {}
        self.targetParams = {}     # (preset, Taste) -> Parameterindex
        self.preset = 0
        self.locked = set(DEFAULT_LOCKED)
        self.learning = False
        self.quant = 1             # 0 sofort, 1 Schlag, 2 Takt
        self.setHeld = False
        self.pending = {}          # Ziel -> (stumm, Zielticks, Startticks, Taste)
        self.shown = {}            # Taste -> zuletzt ans Plugin gesendeter Zustand
        self.ticks = 0
        self.lastLed = -1
        self.chanOffset = 0


S = State()


def _g(fn, *a, **k):
    """Mit globalem Index aufrufen, auf alten FL-Versionen ohne."""
    try:
        return fn(*a, useGlobalIndex=True, **k)
    except TypeError:
        return fn(*a, **k)


def hint(text):
    try:
        ui.setHintMsg(text)
    except Exception:
        pass


# ---------------- Plugin finden und lesen ------------------------------------
def findPlugin():
    S.chan = None
    try:
        try:
            count = channels.channelCount(True)
        except TypeError:
            count = channels.channelCount()
        for c in range(count):
            if not _g(plugins.isValid, c, -1) or _g(plugins.getPluginName, c, -1) != PLUGIN_NAME:
                continue
            S.chan = c
            S.presetParam = S.learnParam = S.quantParam = -1
            S.lockParams, S.targetParams = {}, {}
            byLabel = {"Fest " + LABELS[k]: k for k in LABELS}
            byTarget = {}
            for p in range(NUM_PRESETS):
                for b in BUTTONS:
                    byTarget["Ziel %s P%d" % (LABELS[b], p + 1)] = (p, b)
            for i in range(_g(plugins.getParamCount, c, -1)):
                name = _g(plugins.getParamName, i, c, -1)
                if name == "Preset":
                    S.presetParam = i
                elif name == "Lernen":
                    S.learnParam = i
                elif name == "Einsetzen":
                    S.quantParam = i
                elif name in byLabel:
                    S.lockParams[byLabel[name]] = i
                elif name in byTarget:
                    S.targetParams[byTarget[name]] = i
            return True
    except Exception:
        S.chan = None
    return False


def param(index):
    if S.chan is None or index < 0:
        return 0.0
    return _g(plugins.getParamValue, index, S.chan, -1)


def setParam(index, value):
    if S.chan is None or index < 0:
        return
    try:
        _g(plugins.setParamValue, value, index, S.chan, -1)
    except Exception:
        pass


def readPlugin():
    if S.chan is None:
        return
    try:
        if S.presetParam >= 0:
            S.preset = max(0, min(NUM_PRESETS - 1, int(round(param(S.presetParam) * (NUM_PRESETS - 1)))))
        if S.lockParams:
            S.locked = set(k for k, i in S.lockParams.items() if param(i) > 0.5)
        S.learning = S.learnParam >= 0 and param(S.learnParam) > 0.5
        if S.quantParam >= 0:
            S.quant = max(0, min(2, int(round(param(S.quantParam) * 2))))
    except Exception:
        S.chan = None


def targetOf(preset, button):
    i = S.targetParams.get((preset, button), -1)
    return int(round(param(i) * TARGET_MAX)) if i >= 0 else 0


def setTarget(preset, button, code):
    setParam(S.targetParams.get((preset, button), -1), code / float(TARGET_MAX))


# ---------------- Kanaele stumm und laut -------------------------------------
def isMuted(code):
    try:
        if code >= TARGET_MIXER:
            return bool(mixer.isTrackMuted(code - TARGET_MIXER))
        return bool(_g(channels.isChannelMuted, code - 1))
    except Exception:
        return False


def applyMute(code, mute):
    try:
        if code >= TARGET_MIXER:
            mixer.muteTrack(code - TARGET_MIXER, 1 if mute else 0)
        else:
            _g(channels.muteChannel, code - 1, 1 if mute else 0)
    except Exception:
        pass


def targetName(code):
    try:
        if code >= TARGET_MIXER:
            return "Mixer-Spur %d %s" % (code - TARGET_MIXER, mixer.getTrackName(code - TARGET_MIXER))
        return "Kanal %s" % _g(channels.getChannelName, code - 1)
    except Exception:
        return "Ziel %d" % code


def selectedTarget():
    """Fokus im Mixer: gewaehlte Spur, sonst gewaehlter Channel-Rack-Kanal (nicht das Plugin selbst)."""
    try:
        if ui.getFocused(getattr(midi, "widMixer", 0)):
            return TARGET_MIXER + mixer.trackNumber()
    except Exception:
        pass
    try:
        c = channels.selectedChannel(False, 0, True)
    except TypeError:
        c = channels.selectedChannel()
    except Exception:
        return 0
    if c is None or c < 0 or c == S.chan:
        return 0
    return c + 1


def unitTicks():
    try:
        return general.getRecPPB() if S.quant == 2 else general.getRecPPQ()
    except Exception:
        return 96


def songPos():
    try:
        return transport.getSongPos(ABSTICKS)
    except Exception:
        return 0


def isPlaying():
    try:
        return bool(transport.isPlaying())
    except Exception:
        return False


def toggleTarget(button, code):
    if code in S.pending:                      # nochmal gedrueckt, bevor es eingesetzt hat: abbrechen
        del S.pending[code]
        sendState(button, not isMuted(code))
        return
    mute = not isMuted(code)
    if S.quant == 0 or not isPlaying():
        applyMute(code, mute)
        sendState(button, not mute)
        return
    unit = max(1, unitTicks())
    pos = songPos()
    S.pending[code] = (mute, (pos // unit + 1) * unit, pos, button)
    sendState(button, None)


def runPending():
    if not S.pending:
        return
    pos, playing = songPos(), isPlaying()
    lead = max(1, unitTicks() // 8) if S.quant == 1 else max(1, unitTicks() // 32)
    for code in list(S.pending):
        mute, target, start, button = S.pending[code]
        if not playing or pos >= target - lead or pos < start:     # pos < start: Pattern ist von vorn gestartet
            applyMute(code, mute)
            del S.pending[code]
            sendState(button, not mute)


# ---------------- ans Plugin melden -------------------------------------------
def note(n, vel):
    if S.chan is None:
        return
    try:
        channels.midiNoteOn(S.chan, n, vel)
        channels.midiNoteOn(S.chan, n, 0)
    except Exception:
        pass


def sendValue(name, value):
    note(VALUE_NOTE_BASE + INDEX[name], 1 + (value * 126) // 127)


def sendState(button, on, force=False):
    """on: True an, False aus, None wartet auf den Schlag."""
    if button not in BUTTON_INDEX:
        return
    vel = 64 if on is None else (127 if on else 1)
    if force or S.shown.get(button) != vel:
        S.shown[button] = vel
        note(STATE_NOTE_BASE + BUTTON_INDEX[button], vel)


def refreshStates(force=False):
    for b in BUTTONS:
        code = targetOf(S.preset, b) if b not in S.locked else 0
        if code == 0:
            if force or S.shown.get(b, 1) != 1:
                sendState(b, False, True)
            continue
        if code in S.pending:
            continue
        sendState(b, not isMuted(code), force)


# ---------------- Presets und LEDs ---------------------------------------------
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


def setPreset(p):
    S.preset = p % NUM_PRESETS
    setParam(S.presetParam, S.preset / float(NUM_PRESETS - 1))
    showLeds()
    refreshStates(True)
    hint("nano basics: Preset %d" % (S.preset + 1))


# ---------------- Regler verknuepfen ------------------------------------------
def controlId(chan, cc):
    return midi.EncodeRemoteControlID(device.getPortNumber(), chan, cc)


def linkedEvent(chan, cc):
    try:
        return device.findEventID(controlId(chan, cc), 0)
    except Exception:
        return REC_INVALID


def learn(name, cc):
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
    if linkedEvent(chan, cc) == REC_INVALID:
        for off in (-1, 1):
            if linkedEvent(chan + off, cc) != REC_INVALID:
                S.chanOffset = -off
                break
    S.learning = False
    setParam(S.learnParam, 0.0)
    hint("nano basics: %s in Preset %d verknüpft" % (LABELS.get(name, name), S.preset + 1))


def assign(button):
    code = selectedTarget()
    if code == 0:
        hint("nano basics: Erst einen Kanal im Channel Rack anklicken, dann SET halten und die Taste drücken")
        return
    if targetOf(S.preset, button) == code:
        setTarget(S.preset, button, 0)
        sendState(button, False, True)
        hint("nano basics: %s in Preset %d freigegeben" % (LABELS[button], S.preset + 1))
        return
    setTarget(S.preset, button, code)
    sendState(button, not isMuted(code), True)
    hint("nano basics: %s schaltet in Preset %d %s" % (LABELS[button], S.preset + 1, targetName(code)))


# ---------------- FL-Ereignisse ------------------------------------------------
def OnInit():
    findPlugin()
    readPlugin()
    S.lastLed = -1
    showLeds()
    refreshStates(True)


def OnDeInit():
    for cc in LED_CCS:
        led(cc, False)


def OnIdle():
    S.ticks += 1
    runPending()
    if S.chan is None:
        if S.ticks % 60 == 0 and findPlugin():
            readPlugin()
            S.lastLed = -1
            showLeds()
            refreshStates(True)
        return
    if S.ticks % 10 == 0:
        before = S.preset
        readPlugin()
        if S.preset != before:
            showLeds()
            refreshStates(True)
        else:
            refreshStates()


def OnMidiMsg(event):
    if (event.status & 0xF0) != 0xB0:
        return
    cc, val = event.data1, event.data2
    if cc in (TRACK_LEFT, TRACK_RIGHT):
        if val > 0:
            readPlugin()
            setPreset(S.preset + (1 if cc == TRACK_RIGHT else -1))
        event.handled = True
        return
    if cc == SET_CC:
        S.setHeld = val > 0
        event.handled = True
        return
    name = BY_CC.get(cc)
    if name is None:
        return
    readPlugin()
    sendValue(name, val)
    if name in S.locked:
        return                      # feste Regler: FL macht es selbst, Link to controller wie gewohnt
    event.handled = True
    if IS_BUTTON(name):
        if val > 0:
            if S.setHeld:
                assign(name)
                return
            code = targetOf(S.preset, name)
            if code:
                toggleTarget(name, code)
                return
        elif targetOf(S.preset, name):
            return                  # Loslassen einer Kanal-Taste aendert nichts
    if S.learning:
        learn(name, cc)
        return
    e = linkedEvent(S.preset + 1, cc)
    if e != REC_INVALID:
        try:
            general.processRECEvent(e, (val * FROM_MIDI_MAX) // 127, REC_FLAGS)
        except Exception:
            pass
