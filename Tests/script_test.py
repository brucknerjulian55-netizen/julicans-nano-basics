# Simuliert FL Studio und prueft das Skript: Verknuepfungen je Preset, Lernen, feste Regler, Noten ans Plugin, LEDs.
import importlib.util, os, sys, types

out, notes, rec = [], [], []
links = {}            # controlId -> eventId, wie FL es speichert
lastTweaked = [None]
mods = {}
for name in ("channels", "device", "general", "midi", "plugins", "ui"):
    mods[name] = types.ModuleType(name); sys.modules[name] = mods[name]
md = mods["midi"]
md.FromMIDI_Max = 1073741824; md.REC_MIDIController = 1005; md.REC_UpdateControl = 16; md.REC_InvalidID = 2147483647
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
mods["general"].processRECEvent = lambda e, v, f: rec.append((e, v, f))
mods["ui"].setHintMsg = lambda t: hints.append(t)
hints = []
ch = mods["channels"]
ch.channelCount = lambda globalCount=False: 5
ch.midiNoteOn = lambda idx, note, vel, channel=-1: notes.append((idx, note, vel))

here = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("dev", os.path.join(here, "..", "Script", "device_julicans_nano_basics.py"))
dev = importlib.util.module_from_spec(spec); spec.loader.exec_module(dev)

names = ["Preset"] + ["Fest " + dev.LABELS[k] for k in dev.LABELS] + ["Lernen"]
values = {i: 0.0 for i in range(len(names))}
for i, n in enumerate(names):
    if n[5:] in [dev.LABELS[k] for k in dev.DEFAULT_LOCKED]: values[i] = 1.0
pl = mods["plugins"]
pl.isValid = lambda i, s=-1, useGlobalIndex=False: (i, s) == (3, -1)
pl.getPluginName = lambda i, s=-1, userName=False, useGlobalIndex=False: dev.PLUGIN_NAME
pl.getParamCount = lambda i, s=-1, useGlobalIndex=False: len(names)
pl.getParamName = lambda p, i, s=-1, useGlobalIndex=False: names[p]
pl.getParamValue = lambda p, i, s=-1, useGlobalIndex=False: values[p]
def setp(v, p, i, s=-1, pickupMode=0, useGlobalIndex=False): values[p] = v
pl.setParamValue = setp
LEARN = names.index("Lernen")

class Ev:
    def __init__(self, cc, val, chn=0):
        self.status = 0xB0 + chn; self.data1 = cc; self.data2 = val; self.midiChan = chn; self.handled = False

fails = 0
def check(ok, what):
    global fails
    if not ok: fails += 1; print("FEHLER:", what)

dev.OnInit()
check(dev.S.chan == 3, "Plugin im Channel Rack gefunden")
check(len(dev.S.lockParams) == 49 and dev.S.learnParam == LEARN, "Parameter gefunden")
check((0xB0, 32, 127) in out, "S1 leuchtet")

# Ein alter globaler Link auf Kanal 1 (so wie im ersten Test) darf freie Regler nicht mehr steuern
links[md.EncodeRemoteControlID(10, 0, 16)] = 999
e = Ev(16, 100); dev.OnMidiMsg(e)
check(e.handled and not any(r[0] == 999 for r in rec), "alter Kanal-1-Link wirkt nicht auf freien Regler")
check((3, 70 + dev.INDEX["knob1"], 1 + 100 * 126 // 127) in notes, "Wert geht als Note ans Plugin")

# Lernen in Preset 1: Kick-Lautstaerke (Event 501) mit Drehregler 1
lastTweaked[0] = 501; values[LEARN] = 1.0
e = Ev(16, 64); dev.OnMidiMsg(e)
check(links.get(md.EncodeRemoteControlID(10, 1, 16)) == 501 and values[LEARN] == 0.0, "Lernen legt Link auf Kanal 2 an und schaltet aus")
rec.clear(); dev.OnMidiMsg(Ev(16, 127))
check(rec and rec[-1][0] == 501 and rec[-1][1] == md.FromMIDI_Max, "Preset 1 steuert Event 501 mit vollem Wert")

# Preset 2: derselbe Regler ist frei, dann mit anderem Ziel lernen
dev.OnMidiMsg(Ev(59, 127)); check(dev.S.preset == 1 and abs(values[0] - 1 / 7.0) < 1e-6, "Track vor -> Preset 2")
rec.clear(); dev.OnMidiMsg(Ev(16, 90)); check(not rec, "in Preset 2 frei")
lastTweaked[0] = 777; values[LEARN] = 1.0; dev.OnMidiMsg(Ev(16, 10))
rec.clear(); dev.OnMidiMsg(Ev(16, 127)); check(rec and rec[-1][0] == 777, "Preset 2 steuert Event 777")
dev.OnMidiMsg(Ev(58, 127)); rec.clear(); dev.OnMidiMsg(Ev(16, 127)); check(rec and rec[-1][0] == 501, "zurueck in Preset 1 wieder 501")

# Feste Regler: FL macht es selbst
e = Ev(41, 127); dev.OnMidiMsg(e); check(not e.handled, "Play fest, FL verarbeitet selbst")
values[names.index("Fest Fader 1")] = 1.0
e = Ev(0, 50); dev.OnMidiMsg(e); check(not e.handled, "Fader 1 im Plugin fest gemacht")

# Preset per Maus im Plugin aendern
values[0] = 4 / 7.0
for _ in range(10): dev.OnIdle()
check(dev.S.preset == 4 and (0xB0, 36, 127) in out, "Preset 5 aus dem Plugin, LED S5")
for _ in range(5): dev.OnMidiMsg(Ev(58, 127))
check(dev.S.preset == 7, "zurueck wickelt um")
e = Ev(99, 1); dev.OnMidiMsg(e); check(not e.handled, "fremde CC bleibt unveraendert")
n = Ev(0, 1); n.status = 0x90; dev.OnMidiMsg(n); check(not n.handled, "Noten bleiben unveraendert")

# Lernen ohne bewegten FL-Regler: Hinweis, kein Link
lastTweaked[0] = None; values[LEARN] = 1.0; before = len(links); dev.OnMidiMsg(Ev(17, 5))
check(len(links) == before and "Maus" in hints[-1], "Hinweis, wenn nichts bewegt wurde")
values[LEARN] = 0.0

# Plugin entfernt: Skript laeuft weiter
pl.isValid = lambda i, s=-1, useGlobalIndex=False: False
dev.S.chan = None
for _ in range(60): dev.OnIdle()
dev.OnMidiMsg(Ev(59, 127)); check(dev.S.preset == 0 and dev.S.chan is None, "ohne Plugin weiter umschaltbar")
check(len(dev.ORDER) == 51 and dev.INDEX["fader8"] == 50, "Reihenfolge wie im Plugin")
print("alles ok" if fails == 0 else "%d Fehler" % fails)
sys.exit(1 if fails else 0)
