# julicans nano basics

Ein Plugin für FL Studio, das aus einem Korg nanoKONTROL2 acht Controller macht. Oben im Fenster wählst du eins von 8 Presets. In jedem Preset sind alle Regler wieder frei und lassen sich in FL ganz normal mit „Link to controller" verknüpfen. Regler, die du als **fest** markierst (zum Beispiel deine Effekte), machen in jedem Preset dasselbe.

## So funktioniert es

Das mitgelieferte FL-Skript gibt jedem Preset einen eigenen MIDI-Kanal: Feste Regler senden auf Kanal 1, Preset 1 auf Kanal 2, Preset 8 auf Kanal 9. FL sieht dadurch pro Preset andere Regler. Das Skript liest im Plugin, welches Preset aktiv ist und welche Regler fest sind. Track ◄ und ► am Korg schalten die Presets um, die S-Taste mit der Nummer des Presets leuchtet.

Die Verknüpfungen speichert FL im Projekt. Für neue Projekte: einmal alles verknüpfen und das Projekt als Vorlage speichern.

## Einrichtung

1. `NanoBasics-Setup.exe` aus den Releases installieren. Plugin und Skript landen automatisch am richtigen Ort.
2. Korg Kontrol Editor: CC-Modus (Standardbelegung), **LED Mode: External**, schreiben.
3. FL Studio: Optionen › MIDI-Einstellungen. Beim nanoKONTROL2 unter Eingang: aktivieren, Controller-Typ **julicans nano basics**, Port zum Beispiel 10. Unter Ausgang den nanoKONTROL2 ebenfalls auf Port 10.
4. Das Plugin in einen Mixer-Slot laden, am besten auf den Master oder die Kick-Spur. Dort reagiert die Oberfläche auf deine Kick.
5. Im Plugin-Fenster oben links auf das Zahnrad, dann **MIDI › Eingangsport** auf 10. Dann siehst du die Regler im Plugin mitlaufen.

## Designs

23 Oberflächen in drei Gruppen: Camouflage, Dark Matter und Abstrakt. Dark Matter, Neon-Linien und Aurora reagieren auf Kick, Takt, bewegte Regler und Preset-Wechsel. Der Sparmodus friert alles ein. Die LED-Farbe gilt nur im Plugin, die echten LEDs am Korg sind rot.

## Bauen

GitHub baut bei jedem Upload selbst und veröffentlicht einen Installer, sobald die Version in `CMakeLists.txt` neu ist. Lokal: `build.bat`.

Tests: `python Tests/script_test.py` simuliert FL und prüft das Skript, `Tests/core_test.cpp` prüft Layout und Designs.
