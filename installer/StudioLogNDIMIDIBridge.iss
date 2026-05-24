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

; ── Prerequisite detection + silent download/install ─────────────────────────
;
; Two prerequisites are checked after files are copied (ssPostInstall):
;
;   1. Visual C++ Redistributable 2015–2022 x64  (MSVCP140 / VCRUNTIME140)
;      Built with MSVC 14.50 (VS 18); uses the VS 18 permalink (~18 MB).
;      Update VCRedistURL if a newer VS ships a new permalink.
;
;   2. loopMIDI (Tobias Erichsen) — virtual MIDI port driver (~7.5 MB zip)
;      Update LoopMIDIZipURL + LoopMIDIExe when a new version is released at
;      https://www.tobias-erichsen.de/software/loopmidi.html

[Code]

const
  VCRedistURL    = 'https://aka.ms/vs/18/release/vc_redist.x64.exe';
  LoopMIDIZipURL = 'https://www.tobias-erichsen.de/wp-content/uploads/2020/01/loopMIDISetup_1_0_16_27.zip';
  LoopMIDIExe    = 'loopMIDISetup_1_0_16_27.exe';

var
  DownloadPage: TDownloadWizardPage;

{ ── VC++ Redistributable detection ─────────────────────────────────────────── }

function IsVCRedistInstalled: Boolean;
var
  Installed: Cardinal;
begin
  { Registry key written by the VC++ 2015-2022 x64 redistributable installer }
  Result :=
    RegQueryDWordValue(HKLM,
      'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64',
      'Installed', Installed) and (Installed = 1);
  if Result then Exit;

  { Fallback: check System32 directly }
  Result :=
    FileExists(ExpandConstant('{sys}\MSVCP140.dll')) and
    FileExists(ExpandConstant('{sys}\VCRUNTIME140_1.dll'));
end;

{ ── VC++ Redistributable download + silent install ───────────────────────────── }

procedure TryInstallVCRedist;
var
  ResultCode: Integer;
begin
  DownloadPage.Clear;
  DownloadPage.Add(VCRedistURL, 'vc_redist.x64.exe', '');
  DownloadPage.Show;
  try
    try
      DownloadPage.Download;
    except
      if DownloadPage.AbortedByUser then
        Log('VC++ Redist download cancelled by user')
      else
        SuppressibleMsgBox(
          'Could not download the Visual C++ Redistributable.' + #13#10 +
          'Install it manually from:' + #13#10 +
          'https://aka.ms/vs/18/release/vc_redist.x64.exe',
          mbError, MB_OK, IDOK);
      Exit;
    end;
  finally
    DownloadPage.Hide;
  end;

  if not Exec(ExpandConstant('{tmp}\vc_redist.x64.exe'),
              '/install /quiet /norestart', '',
              SW_HIDE, ewWaitUntilTerminated, ResultCode) then
    SuppressibleMsgBox(
      'Visual C++ Redistributable installation failed. Install it manually.',
      mbError, MB_OK, IDOK)
  else if (ResultCode <> 0) and (ResultCode <> 3010) then
    { 3010 = success but reboot required — acceptable }
    SuppressibleMsgBox(
      'VC++ Redistributable installer returned exit code ' + IntToStr(ResultCode) + '.' + #13#10 +
      'You may need to install it manually.',
      mbInformation, MB_OK, IDOK);
end;

{ ── loopMIDI detection ──────────────────────────────────────────────────────── }

function IsLoopMIDIInstalled: Boolean;
var
  SubKeys:     TArrayOfString;
  DisplayName: String;
  i:           Integer;
begin
  { Fast path: check the known filesystem location (loopMIDI is an x86 app) }
  Result :=
    FileExists(ExpandConstant('{pf32}\Tobias Erichsen\loopMIDI\loopMIDI.exe')) or
    FileExists(ExpandConstant('{pf}\Tobias Erichsen\loopMIDI\loopMIDI.exe'));
  if Result then Exit;

  { Fallback: scan WOW6432Node uninstall keys for DisplayName = "loopMIDI" }
  if RegGetSubkeyNames(HKLM,
       'SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall',
       SubKeys) then
    for i := 0 to GetArrayLength(SubKeys) - 1 do
      if RegQueryStringValue(HKLM,
           'SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\' + SubKeys[i],
           'DisplayName', DisplayName) and
         (CompareText(DisplayName, 'loopMIDI') = 0) then begin
        Result := True;
        Exit;
      end;
end;

{ ── Download progress callback ───────────────────────────────────────────────── }

function OnDownloadProgress(const Url, Filename: String;
                             const Progress, ProgressMax: Int64): Boolean;
begin
  if ProgressMax <> 0 then
    DownloadPage.SetProgress(Progress, ProgressMax);
  Result := True;
end;

{ ── Wizard init ──────────────────────────────────────────────────────────────── }

procedure InitializeWizard;
begin
  DownloadPage := CreateDownloadPage(
    'Downloading prerequisite',
    'Please wait while a required component is downloaded…',
    @OnDownloadProgress);
end;

{ ── Download + silent install loopMIDI ──────────────────────────────────────── }

procedure TryInstallLoopMIDI;
var
  ZipPath, ExtractDir, SetupExe, PSArgs: String;
  ResultCode: Integer;
begin
  { ── Download ── }
  DownloadPage.Clear;
  DownloadPage.Add(LoopMIDIZipURL, 'loopMIDISetup.zip', '');
  DownloadPage.Show;
  try
    try
      DownloadPage.Download;
    except
      if DownloadPage.AbortedByUser then
        Log('loopMIDI download cancelled by user')
      else
        SuppressibleMsgBox(
          'Could not download loopMIDI.' + #13#10 +
          'Install it manually from:' + #13#10 +
          'https://www.tobias-erichsen.de/software/loopmidi.html',
          mbError, MB_OK, IDOK);
      Exit;
    end;
  finally
    DownloadPage.Hide;
  end;

  { ── Extract ZIP via PowerShell Expand-Archive ── }
  ZipPath    := ExpandConstant('{tmp}\loopMIDISetup.zip');
  ExtractDir := ExpandConstant('{tmp}\loopMIDI');

  { Wrap paths in single-quotes so spaces in temp dir are handled by PowerShell }
  PSArgs := '-NonInteractive -Command "Expand-Archive -LiteralPath ' +
            '''' + ZipPath + '''' + ' -DestinationPath ' +
            '''' + ExtractDir + '''' + ' -Force"';

  if not Exec('powershell.exe', PSArgs, '', SW_HIDE, ewWaitUntilTerminated, ResultCode)
     or (ResultCode <> 0) then begin
    SuppressibleMsgBox(
      'Could not extract loopMIDI archive. Install it manually.',
      mbError, MB_OK, IDOK);
    Exit;
  end;

  { ── Run the loopMIDI installer silently ── }
  SetupExe := ExtractDir + '\' + LoopMIDIExe;
  if not FileExists(SetupExe) then begin
    SuppressibleMsgBox(
      'loopMIDI installer not found in archive (' + LoopMIDIExe + ').' + #13#10 +
      'Install it manually from tobias-erichsen.de/software/loopmidi.html',
      mbError, MB_OK, IDOK);
    Exit;
  end;

  Log('Running loopMIDI installer: ' + SetupExe);
  if not Exec(SetupExe, '/VERYSILENT /NORESTART', '', SW_SHOW,
              ewWaitUntilTerminated, ResultCode) then
    SuppressibleMsgBox('loopMIDI installation failed. Install it manually.', mbError, MB_OK, IDOK)
  else if ResultCode <> 0 then
    SuppressibleMsgBox(
      'loopMIDI installer returned exit code ' + IntToStr(ResultCode) + '.' + #13#10 +
      'You may need to install it manually.',
      mbInformation, MB_OK, IDOK);
end;

{ ── Post-install hook: install prereqs if missing ───────────────────────────── }

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep <> ssPostInstall then Exit;

  { 1. Visual C++ Redistributable (must come before the app can run) }
  if not IsVCRedistInstalled then begin
    if SuppressibleMsgBox(
        'The Visual C++ 2015-2022 Redistributable (x64) is not installed.' + #13#10 +
        'This runtime is required by StudioLog NDI MIDI Bridge.' + #13#10 + #13#10 +
        'Download and install it now? (~18 MB)',
        mbConfirmation, MB_YESNO, IDYES) = IDYES then
      TryInstallVCRedist;
  end;

  { 2. loopMIDI virtual MIDI driver }
  if not IsLoopMIDIInstalled then begin
    if SuppressibleMsgBox(
        'StudioLog NDI MIDI Bridge sends MIDI Timecode to a virtual MIDI port.' + #13#10 +
        'loopMIDI (by Tobias Erichsen) was not found on this system.' + #13#10 + #13#10 +
        'Download and install loopMIDI now? (~7.5 MB)',
        mbConfirmation, MB_YESNO, IDNO) = IDYES then
      TryInstallLoopMIDI;
  end;
end;
