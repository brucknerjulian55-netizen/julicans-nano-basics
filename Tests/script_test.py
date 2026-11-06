# Simuliert FL Studio und prueft das Skript: Kanaele schalten je Preset, Einsetzen im Takt,
# Zuweisen mit SET, Regler-Verknuepfungen, feste Regler, Noten ans Plugin, LEDs.
import importlib.util, os, sys, types

out, notes, rec, hints = [], [], [], []
links = {}
lastTweaked = [None]
muted = {}             # globaler Kanal -> stumm
mixMuted = {}
song = {"pos": 0, "playing": False}
focus = {"mixer": False, "sel": 0, "track": 0}
mods = {}
for name in ("channels", "device", "general", "midi", "mixer", "plugins", "transport", "ui"):
    mods[name] = types.ModuleType(name); sys.modules[name] = mods[name]
md = mods["midi"]
md.FromMIDI_Max = 1073741824; md.REC_MIDIController = 1005; md.REC_UpdateControl = 16; md.REC_InvalidID = 2147483647
md.SONGLENGTH_ABSTICKS = 2; md.widMixer = 0
md.EncodeRemoteControlID = lambda port, chan, cc: cc + (chan << 16) + ((port + 1) << 22)
dv = mods["device"]
dv.isAssigned = lambda: True
dv.getPortNumber = lambda: 10
dv.midiOutMsg = lambda m: out.append((m & 0xFF, (m >> 8) & 0xFF, (m >> 16) & 0xFF))
dv.findEventID = lambda cid, flags=0: links.get(cid, md.REC_InvalidID)
def linkToLastTweaked(cc, chan, global_link=False, eventId=None):
    if lastTweaked[0] is None: return 1
    cid = md.EncodeRemoteControlID(10, chan, cc)
    if cid in links: return 2
    links[cid] = lastTweaked[0]; return 0
dv.linkToLastTweaked = linkToLastTweaked
gn = mods["general"]
gn.processRECEvent = lambda e, v, f: rec.append((e, v, f))
gn.getRecPPQ = lambda: 96
gn.getRecPPB = lambda: 384
mods["ui"].setHintMsg = lambda t: hints.append(t)
mods["ui"].getFocused = lambda w: focus["mixer"]
tr = mods["transport"]
tr.getSongPos = lambda mode=-1: song["pos"]
tr.isPlaying = lambda: song["playing"]
mx = mods["mixer"]
mx.isTrackMuted = lambda t: mixMuted.get(t, False)
def muteTrack(t, v=-1): mixMuted[t] = (not mixMuted.get(t, False)) if v == -1 else bool(v)
mx.muteTrack = muteTrack
mx.trackNumber = lambda: focus["track"]
mx.getTrackName = lambda t: "Insert %d" % t
ch = mods["channels"]
ch.channelCount = lambda globalCount=False: 10
ch.midiNoteOn = lambda idx, n, vel, channel=-1: notes.append((idx, n, vel))
ch.isChannelMuted = lambda i, useGlobalIndex=False: muted.get(i, False)
def muteChannel(i, v=-1, useGlobalIndex=False): muted[i] = (not muted.get(i, False)) if v == -1 else bool(v)
ch.muteChannel = muteChannel
ch.getChannelName = lambda i, useGlobalIndex=False: "Kick %d" % i
ch.selectedChannel = lambda canBeNone=False, offset=0, indexGlobal=False: focus["sel"]

here = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("dev", os.path.join(here, "..", "Script", "device_julicans_nano_basics.py"))
dev = importlib.util.module_from_spec(spec); spec.loader.exec_module(dev)

names = ["Preset"] + ["Fest " + dev.LABELS[k] for k in dev.LABELS] + ["Lernen", "Einsetzen"]
for p in range(8):
    for b in dev.BUTTONS:
        names.append("Ziel %s P%d" % (dev.LABELS[b], p + 1))
values = {i: 0.0 for i in range(len(names))}
for i, n in enumerate(names):
    if n[5:] in [dev.LABELS[k] for k in dev.DEFAULT_LOCKED]: values[i] = 1.0
values[names.index("Einsetzen")] = 0.5
PLUGIN = 3
pl = mods["plugins"]
pl.isValid = lambda i, s=-1, useGlobalIndex=False: (i, s) == (PLUGIN, -1)
pl.getPluginName = lambda i, s=-1, userName=False, useGlobalIndex=False: dev.PLUGIN_NAME
pl.getParamCount = lambda i, s=-1, useGlobalIndex=False: len(names)
pl.getParamName = lambda p, i, s=-1, useGlobalIndex=False: names[p]
pl.getParamValue = lambda p, i, s=-1, useGlobalIndex=False: values[p]
def setp(v, p, i, s=-1, pickupMode=0, useGlobalIndex=False): values[p] = v
pl.setParamValue = setp
LEARN = names.index("Lernen")
target = lambda p, b: int(round(values[names.index("Ziel %s P%d" % (dev.LABELS[b], p + 1))] * 2047))

class Ev:
    def __init__(self, cc, val, chn=0):
        self.status = 0xB0 + chn; self.data1 = cc; self.data2 = val; self.midiChan = chn; self.handled = False
def press(cc):
    dev.OnMidiMsg(Ev(cc, 127)); dev.OnMidiMsg(Ev(cc, 0))
def state(b):
    s = [v for (i, n, v) in notes if n == 93 + dev.BUTTON_INDEX[b] and v > 0]
    return s[-1] if s else None

fails = 0
def check(ok, what):
    global fails
    if not ok: fails += 1; print("FEHLER:", what)

dev.OnInit()
check(dev.S.chan == PLUGIN, "Plugin im Channel Rack gefunden")
check(len(dev.S.lockParams) == 48 and len(dev.S.targetParams) == 256 and dev.S.quantParam >= 0, "Parameter gefunden: %d %d" % (len(dev.S.lockParams), len(dev.S.targetParams)))
check(len(dev.BUTTONS) == 32, "32 Tasten")

# --- Kanal zuweisen: Kanal 5 anklicken, SET halten, S1 druecken ---
focus["sel"] = 5
dev.OnMidiMsg(Ev(60, 127)); press(32); dev.OnMidiMsg(Ev(60, 0))
check(target(0, "s1") == 6, "S1 in Preset 1 -> Kanal 5 (Code 6), ist %d" % target(0, "s1"))
check(state("s1") == 127, "Plugin zeigt S1 an")
# Ohne Wiedergabe: sofort stumm und laut
press(32); check(muted.get(5) is True and state("s1") == 1, "S1 schaltet Kanal 5 stumm")
press(32); check(muted.get(5) is False and state("s1") == 127, "S1 schaltet Kanal 5 wieder an")

# --- Im Takt: Wiedergabe laeuft, Einsetzen auf den naechsten Schlag ---
song["playing"] = True; song["pos"] = 130
press(32)
check(muted.get(5) is False and state("s1") == 64, "wartet auf den Schlag")
song["pos"] = 170; dev.OnIdle(); check(muted.get(5) is False, "noch nicht bei 170")
song["pos"] = 185; dev.OnIdle(); check(muted.get(5) is True and state("s1") == 1, "kurz vor Schlag 192 stumm")
press(32); song["pos"] = 200; press(32)
check(5 not in [k - 1 for k in dev.S.pending] and muted.get(5) is True, "zweiter Druck vor dem Einsatz bricht ab")
# Takt-Modus
values[names.index("Einsetzen")] = 1.0; dev.OnIdle(); dev.readPlugin()
song["pos"] = 400; press(32)
song["pos"] = 700; dev.OnIdle(); check(muted.get(5) is True, "Takt: bei 700 noch nicht")
song["pos"] = 760; dev.OnIdle(); check(muted.get(5) is False, "Takt: kurz vor 768 an")
# Pattern springt zurueck an den Anfang: ausstehender Einsatz greift
song["pos"] = 300; press(32); song["pos"] = 10; dev.OnIdle(); check(muted.get(5) is True, "Pattern-Neustart fuehrt aus")
values[names.index("Einsetzen")] = 0.5; dev.readPlugin(); song["playing"] = False

# --- Preset 2: S1 ist frei, eigener Kanal ---
press(59); check(dev.S.preset == 1 and state("s1") == 1, "Preset 2, S1 frei angezeigt")
before = dict(muted); press(32); check(muted == before, "S1 in Preset 2 schaltet nichts")
focus["sel"] = 7; dev.OnMidiMsg(Ev(60, 127)); press(32); dev.OnMidiMsg(Ev(60, 0))
check(target(1, "s1") == 8 and target(0, "s1") == 6, "Preset 2 eigenes Ziel, Preset 1 bleibt")
press(32); check(muted.get(7) is True and muted.get(5) is True, "Preset 2 schaltet Kanal 7")
press(58); check(dev.S.preset == 0 and state("s1") == 1, "zurueck in Preset 1 zeigt Zustand von Kanal 5")

# --- Mixer-Spur zuweisen ---
focus["mixer"] = True; focus["track"] = 4
dev.OnMidiMsg(Ev(60, 127)); press(48); dev.OnMidiMsg(Ev(60, 0)); focus["mixer"] = False
check(target(0, "m1") == 1024 + 4, "M1 -> Mixer-Spur 4")
press(48); check(mixMuted.get(4) is True, "M1 schaltet Mixer-Spur stumm")
# SET + gleiche Taste mit gleichem Ziel loescht
focus["mixer"] = True; dev.OnMidiMsg(Ev(60, 127)); press(48); dev.OnMidiMsg(Ev(60, 0)); focus["mixer"] = False
check(target(0, "m1") == 0, "zweites SET+Taste gibt frei")
# Plugin selbst ist kein Ziel
focus["sel"] = PLUGIN; dev.OnMidiMsg(Ev(60, 127)); press(33); dev.OnMidiMsg(Ev(60, 0))
check(target(0, "s2") == 0 and "anklicken" in hints[-1], "Plugin-Kanal wird nicht zugewiesen")

# --- Mausklick auf Mute in FL wird angezeigt ---
muted[5] = False; notes.clear()
for _ in range(10): dev.OnIdle()
check(state("s1") == 127, "Mute per Maus in FL wird nachgezogen")

# --- Regler wie bisher ---
links[md.EncodeRemoteControlID(10, 0, 16)] = 999
e = Ev(16, 100); dev.OnMidiMsg(e)
check(e.handled and not any(r[0] == 999 for r in rec), "alter Kanal-1-Link wirkt nicht auf freien Regler")
check((PLUGIN, dev.INDEX["knob1"], 1 + 100 * 126 // 127) in notes, "Wert geht als Note ans Plugin")
lastTweaked[0] = 501; values[LEARN] = 1.0
dev.OnMidiMsg(Ev(16, 64))
check(links.get(md.EncodeRemoteControlID(10, 1, 16)) == 501 and values[LEARN] == 0.0, "Lernen legt Link auf Kanal 2 an")
rec.clear(); dev.OnMidiMsg(Ev(16, 127)); check(rec and rec[-1][0] == 501, "Preset 1 steuert Event 501")
e = Ev(41, 127); dev.OnMidiMsg(e); check(not e.handled, "Play fest, FL verarbeitet selbst")
e = Ev(60, 127); dev.OnMidiMsg(e); check(e.handled, "SET wird nie an FL weitergegeben"); dev.OnMidiMsg(Ev(60, 0))

# --- Plugin weg: Skript stuerzt nicht ab ---
pl.isValid = lambda i, s=-1, useGlobalIndex=False: False
dev.S.chan = None
for _ in range(60): dev.OnIdle()
press(59); press(32); check(dev.S.chan is None, "ohne Plugin kein Absturz")
check(len(dev.ORDER) == 51 and dev.INDEX["fader8"] == 50, "Reihenfolge wie im Plugin")
print("alles ok" if fails == 0 else "%d Fehler" % fails)
sys.exit(1 if fails else 0)
