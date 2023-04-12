; Script generated by the Inno Setup Script Wizard.
; SEE THE DOCUMENTATION FOR DETAILS ON CREATING INNO SETUP SCRIPT FILES!

#define MyAppName "maxdomeII X2 PlugIn"
#define MyAppVersion "1.35"
#define MyAppPublisher "RTI-Zone"
#define MyAppURL "https://rti-zone.org"

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
; Do not use the same AppId value in installers for other applications.
; (To generate a new GUID, click Tools | Generate GUID inside the IDE.)
AppId={{6D49D7F2-C290-41C0-BD3C-F93913AB7F12}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
;AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={code:TSXInstallDir}\Resources\Common
DefaultGroupName={#MyAppName}

; Need to customise these
; First is where you want the installer to end up
OutputDir=installer
; Next is the name of the installer
OutputBaseFilename=maxdomeII_X2_Installer
; Final one is the icon you would like on the installer. Comment out if not needed.
SetupIconFile=rti_zone_logo.ico
Compression=lzma
SolidCompression=yes
; We don't want users to be able to select the drectory since read by TSXInstallDir below
DisableDirPage=yes
; Uncomment this if you don't want an uninstaller.
;Uninstallable=no
CloseApplications=yes
DirExistsWarning=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Dirs]
Name: "{app}\Plugins\DomePlugIns";
Name: "{app}\Plugins64\DomePlugIns";

[Files]
Source: "domelist MaxDomeII.txt";                       DestDir: "{app}\Miscellaneous Files"; Flags: ignoreversion
Source: "domelist MaxDomeII.txt";                       DestDir: "{app}\Miscellaneous Files"; Flags: ignoreversion; DestName: "domelist64 MaxDomeII.txt"
; 32 bits
Source: "libmaxdomeII\Win32\Release\libmaxdomeII.dll";  DestDir: "{app}\Plugins\DomePlugIns"; Flags: ignoreversion
Source: "maxdomeII.ui";                                 DestDir: "{app}\Plugins\DomePlugIns"; Flags: ignoreversion
; 64 bits
Source: "libmaxdomeII\x64\Release\libmaxdomeII.dll";    DestDir: "{app}\Plugins64\DomePlugIns"; Flags: ignoreversion; Check: DirExists(ExpandConstant('{app}\Plugins64\DomePlugIns'))
Source: "maxdomeII.ui";                                 DestDir: "{app}\Plugins64\DomePlugIns"; Flags: ignoreversion; Check: DirExists(ExpandConstant('{app}\Plugins64\DomePlugIns'))

[Code]
{* Below is a function to read TheSkyXInstallPath.txt and confirm that the directory does exist
   This is then used in the DefaultDirName above
   *}
var
  Location: String;
  LoadResult: Boolean;

function TSXInstallDir(Param: String) : String;
begin
  LoadResult := LoadStringFromFile(ExpandConstant('{userdocs}') + '\Software Bisque\TheSkyX Professional Edition\TheSkyXInstallPath.txt', Location);
  if not LoadResult then
    LoadResult := LoadStringFromFile(ExpandConstant('{userdocs}') + '\Software Bisque\TheSky Professional Edition 64\TheSkyXInstallPath.txt', Location);
    if not LoadResult then
      LoadResult := BrowseForFolder('Please locate the installation path for TheSkyX', Location, False);
      if not LoadResult then
        RaiseException('Unable to find the installation path for TheSkyX');
  if not DirExists(Location) then
    RaiseException('TheSkyX installation directory ' + Location + ' does not exist');
  Result := Location;
end;
