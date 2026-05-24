; ============================================================================
; StudioLog NDI MIDI Bridge — Inno Setup 6 installer script
;
; Build:
;   1. cmake --build build/windows-release --config Release
;   2. "C:\Users\Admin\AppData\Local\Programs\Inno Setup 6\ISCC.exe" installer\StudioLogNDIMIDIBridge.iss
;
; Output: dist\StudioLogNDIMIDIBridge-Setup-0.1.0.exe
; ============================================================================

#define AppName      "StudioLog NDI MIDI Bridge"
#define AppVersion   "0.1.0"
#define AppPublisher "Texas Music Cafe"
#define AppURL       "https://texasmusiccafe.org"
#define AppExeName   "StudioLogNDIMIDIBridge.exe"
#define BuildDir     "..\build\windows-release\Release"
#define ResDir       "..\resources"

[Setup]
AppId={{F3A8C2D1-4B7E-4F9A-8C3D-2E1F5A6B7C8D}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}
AppUpdatesURL={#AppURL}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
AllowNoIcons=yes
; Output
OutputDir=..\dist
OutputBaseFilename=StudioLogNDIMIDIBridge-Setup-{#AppVersion}
; Appearance
SetupIconFile={#ResDir}\NDI_BRIDGE.ico
WizardStyle=modern
; Compression
Compression=lzma2/ultra64
SolidCompression=yes
; Privileges — installs to Program Files; writes no per-user state during setup
PrivilegesRequired=admin
; Windows 10 or later (build 10240)
MinVersion=10.0.10240
; Architecture
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

; ── Files ────────────────────────────────────────────────────────────────────

[Files]
; Main executable
Source: "{#BuildDir}\{#AppExeName}";          DestDir: "{app}";              Flags: ignoreversion

; NDI runtime
Source: "{#BuildDir}\Processing.NDI.Lib.x64.dll"; DestDir: "{app}";         Flags: ignoreversion

; Qt runtime DLLs
Source: "{#BuildDir}\Qt6Core.dll";            DestDir: "{app}";              Flags: ignoreversion
Source: "{#BuildDir}\Qt6Gui.dll";             DestDir: "{app}";              Flags: ignoreversion
Source: "{#BuildDir}\Qt6Network.dll";         DestDir: "{app}";              Flags: ignoreversion
Source: "{#BuildDir}\Qt6Svg.dll";             DestDir: "{app}";              Flags: ignoreversion
Source: "{#BuildDir}\Qt6Widgets.dll";         DestDir: "{app}";              Flags: ignoreversion

; DirectX / OpenGL shims (deployed by windeployqt)
Source: "{#BuildDir}\D3Dcompiler_47.dll";     DestDir: "{app}";              Flags: ignoreversion
Source: "{#BuildDir}\dxcompiler.dll";         DestDir: "{app}";              Flags: ignoreversion
Source: "{#BuildDir}\dxil.dll";              DestDir: "{app}";              Flags: ignoreversion
Source: "{#BuildDir}\opengl32sw.dll";         DestDir: "{app}";              Flags: ignoreversion

; Qt platform plugin (required — must live in platforms\)
Source: "{#BuildDir}\platforms\qwindows.dll"; DestDir: "{app}\platforms";    Flags: ignoreversion

; Qt style plugin
Source: "{#BuildDir}\styles\qmodernwindowsstyle.dll"; DestDir: "{app}\styles"; Flags: ignoreversion

; Qt image format plugins
Source: "{#BuildDir}\imageformats\qgif.dll";  DestDir: "{app}\imageformats"; Flags: ignoreversion
Source: "{#BuildDir}\imageformats\qico.dll";  DestDir: "{app}\imageformats"; Flags: ignoreversion
Source: "{#BuildDir}\imageformats\qjpeg.dll"; DestDir: "{app}\imageformats"; Flags: ignoreversion
Source: "{#BuildDir}\imageformats\qsvg.dll";  DestDir: "{app}\imageformats"; Flags: ignoreversion

; Qt icon engine plugin
Source: "{#BuildDir}\iconengines\qsvgicon.dll"; DestDir: "{app}\iconengines"; Flags: ignoreversion

; Qt generic input plugin
Source: "{#BuildDir}\generic\qtuiotouchplugin.dll"; DestDir: "{app}\generic"; Flags: ignoreversion

; Qt network information plugin
Source: "{#BuildDir}\networkinformation\qnetworklistmanager.dll"; DestDir: "{app}\networkinformation"; Flags: ignoreversion

; Qt TLS backends
Source: "{#BuildDir}\tls\qcertonlybackend.dll";  DestDir: "{app}\tls";      Flags: ignoreversion
Source: "{#BuildDir}\tls\qschannelbackend.dll";  DestDir: "{app}\tls";      Flags: ignoreversion

; ── Shortcuts ────────────────────────────────────────────────────────────────

[Icons]
; Start Menu
Name: "{group}\{#AppName}";         Filename: "{app}\{#AppExeName}"; IconFilename: "{app}\{#AppExeName}"
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"

; Optional desktop shortcut (user can uncheck during install)
Name: "{autodesktop}\{#AppName}";   Filename: "{app}\{#AppExeName}"; IconFilename: "{app}\{#AppExeName}"; Tasks: desktopicon

; ── Tasks (checkboxes on the install wizard) ─────────────────────────────────

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional icons:"; Flags: unchecked

; ── Run after install ────────────────────────────────────────────────────────

[Run]
Filename: "{app}\{#AppExeName}"; \
  Description: "Launch {#AppName}"; \
  Flags: nowait postinstall skipifsilent

; ── Registry — app auto-start (optional, disabled by default) ────────────────
; Uncomment if you want a "Start with Windows" option:
; [Registry]
; Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; \
;   ValueType: string; ValueName: "{#AppName}"; \
;   ValueData: """{app}\{#AppExeName}"""; Flags: uninsdeletevalue

; ── Uninstall cleanup ────────────────────────────────────────────────────────

[UninstallDelete]
Type: filesandordirs; Name: "{app}"
