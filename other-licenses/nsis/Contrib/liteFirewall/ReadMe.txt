liteFirewall 1.0 -- based on nsisFirewall 1.2

http://liangsun.info/portfolio/nsis-plugin-litefirewall/
http://nsis.sourceforge.net/LiteFirewall_Plugin

---------------------------------------------------------
liteFirewall resolved the issue nsisFirewall exists on Vista/Windows 7 platforms. 
It support the profiles (private, domain, public) of firewall rules.
It support Unicode NSIS, while another firewall plugin SimpleFC not.
------------------------------------------------------------

Usage
----------------------------------------------------------
liteFirewall::AddRule "<application path>" "<rule name>"
liteFirewall::RemoveRule "<application path>" "<rule name>"

<application path> is the full path to the application you want to be authorized to
	access the network (or accept incoming connections)

<rule name> is the title that will be given to this exception entry in the firewall
	control panel list


Notes
-----
1) Your installer must be run with administrator rights for liteFirewall to work
2) When compiling with more recent compiler than VC60, you need to choose the compilation
option to use static MFC library.

Sample scripts
--------------

	; Add NOTEPAD to the authorized list
	liteFirewall::AddRule "$WINDIR\Notepad.exe" "liteFirewall Test"
	Pop $0

	; Remove NOTEPAD from the authorized list
	liteFirewall::RemoveRule "$WINDIR\Notepad.exe" "liteFirewall Test"
	Pop $0