' This Source Code Form is subject to the terms of the Mozilla Public
' License, v. 2.0. If a copy of the MPL was not distributed with this
' file, You can obtain one at http://mozilla.org/MPL/2.0/.

' This script downloads and install MSYS2 and the preferred terminal emulator ConEmu

Sub Download(uri, path)
    Dim httpRequest, stream

    Set httpRequest = CreateObject("MSXML2.ServerXMLHTTP.6.0")
    Set stream = CreateObject("Adodb.Stream")

    httpRequest.Open "GET", uri, False
    httpRequest.Send

    With stream
        .type = 1
        .open
        .write httpRequest.responseBody
        .savetofile path, 2
    End With
End Sub

Function GetInstallPath()
    Dim message, prompt

    message = "When you click OK, we will download and extract a build environment to the directory specified. You should see various windows appear. Do NOT interact with them until one explicitly prompts you to continue." & vbCrLf & vbCrLf & "Installation Path:"
    title = "Select Installation Location"
    GetInstallPath = InputBox(message, title, "c:\mozdev")
end Function

Dim installPath, msysPath, conemuPath, conemuSettingsPath, conemuExecutable, bashExecutable
Dim conemuSettingsURI, settingsFile, settingsText, fso, shell, msysArchive, appShell, errorCode
Dim mingwExecutable

' Set up OS interaction like filesystem and shell
Set fso = CreateObject("Scripting.FileSystemObject")
Set shell = CreateObject("WScript.Shell")
Set appShell = CreateObject("Shell.Application")

' Get where MSYS2 and ConEmu should be installed, create directories if necessary
installPath = GetInstallPath()
msysPath = fso.BuildPath(installPath, "msys64")
conemuPath = fso.BuildPath(installPath, "ConEmu")
If NOT fso.FolderExists(installPath) Then
    fso.CreateFolder(installPath)
    fso.CreateFolder(msysPath)
End If
If NOT fso.FolderExists(installPath) Then
    MsgBox("Failed to create folder. Do you have permission to install in this directory?")
    WScript.Quit 1
End If

On Error Resume Next
' Download and move MSYS2 into the right place
Download "https://api.pub.build.mozilla.org/tooltool/sha512/f93a685c8a10abbd349cbef5306441ba235c4cbfba1cc000299e11b58f258e9953cbe23463515407925eeca94c3f5d8e5f637c95be387e620845efa43cdcb0c0", "msys2.zip"
Set FilesInZip = appShell.NameSpace(fso.GetAbsolutePathName("msys2.zip")).Items()
appShell.NameSpace(msysPath).CopyHere(FilesInZip)
' MSYS2 archive doesn't have tmp directory...
fso.CreateFolder(fso.BuildPath(msysPath, "tmp"))
fso.DeleteFile("msys2.zip")
If Err.Number <> 0 Then
    MsgBox("Error downloading and installing MSYS2. Make sure you have internet connection. If you think this is a bug, please file one in Bugzilla https://bugzilla.mozilla.org/enter_bug.cgi?product=Core&component=Build%20Config")
    WScript.Quit 1
End If
On Error GoTo 0

' Install ConEmu
' Download installer
On Error Resume Next
Download "https://conemu.github.io/install2.ps1", "install2.ps1"
conemuSettingsURI = "https://api.pub.build.mozilla.org/tooltool/sha512/9aa384ecc8025a974999e913c83064b3b797e05d19806e62ef558c8300e4c3f72967e9464ace59759f76216fc2fc66f338a1e5cdea3b9aa264529487f091d929"
' Run installer
errorCode = shell.Run("powershell.exe -NoProfile -ExecutionPolicy Unrestricted set dst '" & conemuPath & "'; set ver 'stable'; set lnk 'Mozilla Development Shell'; set xml '" & conemuSettingsURI & "'; set run $FALSE; .\install2.ps1", 0, true)
' Delete ConEmu installer
fso.DeleteFile("install2.ps1")
If Err.Number <> 0 Then
    MsgBox("Error downloading and installing ConEmu. Make sure you have internet connection and Powershell installed. If you think this is a bug, please file one in Bugzilla https://bugzilla.mozilla.org/enter_bug.cgi?product=Core&component=Build%20Config")
    WScript.Quit 1
End If
On Error GoTo 0

' Replace paths in ConEmu settings file
conemuSettingsPath = fso.BuildPath(conemuPath, "ConEmu.xml")
Set settingsFile = fso.OpenTextFile(conemuSettingsPath, 1)
settingsText = settingsFile.ReadAll
settingsFile.Close
settingsText = Replace(settingsText, "%MSYS2_PATH", msysPath)
Set settingsFile = fso.OpenTextFile(conemuSettingsPath, 2)
settingsFile.WriteLine settingsText
settingsFile.Close

' Make MSYS2 Mozilla-ready
bashExecutable = fso.BuildPath(msysPath, fso.BuildPath("usr", fso.BuildPath("bin", "bash.exe")))
conemuExecutable = fso.BuildPath(conemuPath, "ConEmu.exe")
' There may be spaces in the paths to the executable, this ensures they're parsed correctly
bashExecutable = """" & bashExecutable & """"
conemuExecutable = """" & conemuExecutable & """"

errorCode = shell.Run(bashExecutable & " -l -c 'logout", 1, true)
If errorCode <> 0 Then
    MsgBox("MSYS2 initial setup failed. Make sure you have full access to the path you specified. If you think this is a bug, please file one in Bugzilla at https://bugzilla.mozilla.org/enter_bug.cgi?product=Core&component=Build%20Config")
    WScript.Quit 1
End If

errorCode = shell.Run(bashExecutable & " -l -c 'pacman -Syu --noconfirm wget mingw-w64-x86_64-python2-pip && logout'", 1, true)
If errorCode <> 0 Then
    MsgBox("Package update failed. Make sure you have internet access. If you think this is a bug, please file one in Bugzilla at https://bugzilla.mozilla.org/enter_bug.cgi?product=Core&component=Build%20Config")
    WScript.Quit 1
End If

errorCode = shell.Run(conemuExecutable & " -run set CHERE_INVOKING=1 & set MSYSTEM=MINGW64 & " & bashExecutable & " -cil 'export MOZ_WINDOWS_BOOTSTRAP=1 && cd """ & installPath & """ && wget -q https://hg.mozilla.org/mozilla-central/raw-file/default/python/mozboot/bin/bootstrap.py -O /tmp/bootstrap.py && python /tmp/bootstrap.py'", 1, true)
If errorCode <> 0 Then
    MsgBox("Bootstrap failed. Make sure you have internet access. If you think this is a bug, please file one in Bugzilla https://bugzilla.mozilla.org/enter_bug.cgi?product=Core&component=Build%20Config")
    WScript.Quit 1
End If
