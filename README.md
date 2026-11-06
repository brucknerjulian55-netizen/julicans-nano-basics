# julicans nano basics

Ein Plugin für FL Studio, das aus einem Korg nanoKONTROL2 acht Controller macht. Oben im Fenster wählst du eins von 8 Presets. In jedem Preset sind alle Regler wieder frei und lassen sich in FL ganz normal mit „Link to controller" verknüpfen. Regler, die du als **fest** markierst (zum Beispiel deine Effekte), machen in jedem Preset dasselbe.

## So funktioniert es

Das Plugin ist ein Instrument im Channel Rack. Das mitgelieferte FL-Skript liest dort, welches Preset aktiv ist und welche Regler fest sind.

- **Feste Regler** laufen ganz normal durch FL. Du verknüpfst sie wie immer mit Rechtsklick › Link to controller.
- **Alle anderen Regler** führt das Skript selbst aus, für jedes Preset getrennt. Verknüpft wird mit **Lernen**: Knopf im Plugin drücken, in FL den Zielregler mit der Maus bewegen, dann den Regler am Korg. Das braucht FL Studio 21 oder neuer.
- **Sounds:** Zieh ein Sample aus dem FL-Browser auf eine Taste (S, M, R, Cycle, Marker, Transport). Sie spielt es in diesem Preset ab. Gespeichert wird nur der Pfad, relativ zu deinem Sample-Ordner. Im Piano Roll spielen C1 bis G1 die S-Tasten, G#1 bis D#2 die M-Tasten, E2 bis B2 die R-Tasten und C3 bis G#3 Cycle bis Rec.
- **Speichern und Laden** sichert alle 8 Presets mit Namen, festen Reglern und Sound-Pfaden als `.nano`-Datei. In einem leeren Projekt lädst du sie, und die Sounds kommen aus deinem Ordner zurück.
- Track ◄ und ► schalten die Presets um, die S-Taste mit der Nummer des Presets leuchtet.

Die Lernen-Verknüpfungen speichert FL im Projekt, nicht in der `.nano`-Datei. Für neue Projekte: einmal verknüpfen und als Vorlage speichern.

## Einrichtung

1. `NanoBasics-Setup.exe` aus den Releases installieren. Plugin und Skript landen automatisch am richtigen Ort.
2. Korg Kontrol Editor: CC-Modus (Standardbelegung), **LED Mode: External**, schreiben.
3. FL Studio neu starten, im Plugin-Manager nach neuen Plugins suchen.
4. Optionen › MIDI-Einstellungen: beim nanoKONTROL2 unter Eingang den Controller-Typ **julicans nano basics** und einen Port, zum Beispiel 10. Unter Ausgang den nanoKONTROL2 ebenfalls auf Port 10.
5. Das Plugin als Instrument in den Channel Rack laden und oben auf **Sample-Ordner** deinen Sound-Ordner wählen.

## Designs

23 Oberflächen in drei Gruppen: Camouflage, Dark Matter und Abstrakt. Dark Matter, Neon-Linien und Aurora reagieren auf Kick, Takt, bewegte Regler und Preset-Wechsel. Der Sparmodus friert alles ein. Die LED-Farbe gilt nur im Plugin, die echten LEDs am Korg sind rot.

## Bauen

GitHub baut bei jedem Upload selbst und veröffentlicht einen Installer, sobald die Version in `CMakeLists.txt` neu ist. Lokal: `build.bat`.

Tests: `python Tests/script_test.py` simuliert FL und prüft das Skript, `Tests/core_test.cpp` prüft Layout und Designs.
