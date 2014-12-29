; liteFirewall - Sample script

!ifdef TARGETDIR
!addplugindir "${TARGETDIR}"
!else
!addplugindir "..\bin"
!endif

Name "Sample liteFirewall"
OutFile "Sample.exe"
ShowInstDetails show	

Section "Main program"
	; Add NOTEPAD to the authorized list
	liteFirewallW::AddRule "$WINDIR\Notepad.exe" "liteFirewall Test"
	Pop $0
	Exec "rundll32.exe shell32.dll,Control_RunDLL firewall.cpl"
	MessageBox MB_OK "Program added to Firewall exception list.$\r$\n(close the control panel before clicking OK)"

	; Remove NOTEPAD from the authorized list
	liteFirewallW::RemoveRule "$WINDIR\Notepad.exe" "liteFirewall Test"
	Pop $0
	Exec "rundll32.exe shell32.dll,Control_RunDLL firewall.cpl"
	MessageBox MB_OK "Program removed to Firewall exception list"
SectionEnd
