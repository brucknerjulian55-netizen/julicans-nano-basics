; Installer fuer julicans nano basics: Plugin plus FL-Skript. Wird von GitHub beim Veroeffentlichen gebaut.
#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif

[Setup]
AppId={{3B6F2D8E-9A41-4C7E-B0D5-6E21A9C4F713}
AppName=julicans nano basics
AppVersion={#AppVersion}
AppPublisher=Tekkbox
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
Uninstallable=yes
UninstallDisplayName=julicans nano basics
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=dist
OutputBaseFilename=NanoBasics-Setup
Compression=lzma2
SolidCompression=yes
CloseApplications=yes
CloseApplicationsFilter=*.vst3,*.dll
RestartApplications=no

[Languages]
Name: "de"; MessagesFile: "compiler:Languages\German.isl"

[Files]
Source: "build\NanoBasics_artefacts\Release\VST3\julicans nano basics.vst3\*"; DestDir: "{commoncf64}\VST3\julicans nano basics.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "Script\device_julicans_nano_basics.py"; DestDir: "{userdocs}\Image-Line\FL Studio\Settings\Hardware\julicans nano basics"; Flags: ignoreversion

[Messages]
FinishedLabel=[name] ist installiert. Starte FL Studio neu, such im Plugin-Manager nach neuen Plugins und lade es als Instrument in den Channel Rack. In den MIDI-Einstellungen beim nanoKONTROL2 den Controller-Typ "julicans nano basics" wählen.
