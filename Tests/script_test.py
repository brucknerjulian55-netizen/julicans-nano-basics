# Simuliert FL Studio und prueft das Skript: Kanalwechsel, feste Regler, Preset-Tasten, LEDs.
import importlib.util, os, sys, types

out = []
class P:
    params = ["Preset"] + ["Fest " + n for n in []]
mods = {}
for name in ("device", "mixer", "plugins", "ui"):
    mods[name] = types.ModuleType(name); sys.modules[name] = mods[name]
mods["device"].isAssigned = lambda: True
mods["device"].midiOutMsg = lambda m: out.append((m & 0xFF, (m >> 8) & 0xFF, (m >> 16) & 0xFF))
mods["mixer"].trackCount = lambda: 127
mods["ui"].setHintMsg = lambda t: None

here = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("dev", os.path.join(here, "..", "Script", "device_julicans_nano_basics.py"))
dev = importlib.util.module_from_spec(spec); spec.loader.exec_module(dev)

names = ["Preset"] + ["Fest " + dev.LABELS[k] for k in dev.LABELS]
values = {i: 0.0 for i in range(len(names))}
for i, n in enumerate(names):
    if n[5:] in [dev.LABELS[k] for k in dev.DEFAULT_LOCKED]: values[i] = 1.0
pl = mods["plugins"]
pl.isValid = lambda t, s=-1: (t, s) == (0, 3)
pl.getPluginName = lambda t, s=-1: dev.PLUGIN_NAME
pl.getParamCount = lambda t, s=-1: len(names)
pl.getParamName = lambda i, t, s=-1: names[i]
pl.getParamValue = lambda i, t, s=-1: values[i]
def setp(v, i, t, s=-1): values[i] = v
pl.setParamValue = setp

class Ev:
    def __init__(self, cc, val, ch=0):
        self.status = 0xB0 + ch; self.data1 = cc; self.data2 = val; self.midiChan = ch; self.handled = False

fails = 0
def check(ok, what):
    global fails
    if not ok: fails += 1; print("FEHLER:", what)

dev.OnInit()
check(dev.S.where == (0, 3), "Plugin gefunden")
check(len(dev.S.lockParams) == 49, "49 Fest-Parameter gefunden, nicht %d" % len(dev.S.lockParams))
check((0xB0, 32, 127) in out, "S1 leuchtet")
e = Ev(0, 100); dev.OnMidiMsg(e)
check(e.midiChan == 1 and e.status == 0xB1 and not e.handled, "Fader 1 in Preset 1 auf Kanal 2")
e = Ev(41, 127); dev.OnMidiMsg(e)
check(e.midiChan == 0 and not e.handled, "Play fest auf Kanal 1")
out.clear()
e = Ev(59, 127); dev.OnMidiMsg(e)
check(e.handled and abs(values[0] - 1 / 7.0) < 1e-6, "Track vor schaltet auf Preset 2")
check((0xB0, 33, 127) in out and (0xB0, 32, 0) in out, "LED wandert zu S2")
e = Ev(59, 0); dev.OnMidiMsg(e); check(e.handled and dev.S.preset == 1, "Loslassen schaltet nicht")
e = Ev(16, 64); dev.OnMidiMsg(e); check(e.midiChan == 2, "Drehregler 1 in Preset 2 auf Kanal 3")
# Im Plugin festmachen und Preset per Maus aendern
values[names.index("Fest Drehregler 1")] = 1.0; values[0] = 4 / 7.0
for _ in range(10): dev.OnIdle()
e = Ev(16, 64); dev.OnMidiMsg(e); check(e.midiChan == 0, "fester Drehregler bleibt auf Kanal 1")
e = Ev(17, 64); dev.OnMidiMsg(e); check(e.midiChan == 5, "Preset 5 aus dem Plugin gelesen")
e = Ev(58, 127)
for _ in range(5): dev.OnMidiMsg(Ev(58, 127))
check(dev.S.preset == 7, "Zurueck wickelt um: %d" % dev.S.preset)
e = Ev(99, 1); dev.OnMidiMsg(e); check(e.midiChan == 0 and not e.handled, "fremde CC bleibt unveraendert")
n = Ev(0, 1); n.status = 0x90; dev.OnMidiMsg(n); check(n.midiChan == 0, "Noten bleiben unveraendert")
# Plugin entfernt: Skript laeuft weiter
pl.isValid = lambda t, s=-1: False
for _ in range(10): dev.OnIdle()
e = Ev(59, 127); dev.OnMidiMsg(e); check(dev.S.preset == 0 and dev.S.where is None, "ohne Plugin weiter umschaltbar")
print("alles ok" if fails == 0 else "%d Fehler" % fails)
sys.exit(1 if fails else 0)
