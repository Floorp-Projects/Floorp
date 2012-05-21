# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

[ ] 
[ ] use 'property.inc'
[ ] 
[-] testcase LoadURL () appstate none
	[ ] // The first few constants are to be changed according to how your machine is setup
	[ ] //  - strPerfToolsDrive is the drive letter corresponding to the script directory
	[ ] //  - strViewerDrive is the drive letter where the Viewer.exe is installed
	[ ] //  - strURLPath is the full path to the file that has the URLs to process
	[ ] //  - strPerfToolsDir is the directory where the perf tools are installed 
	[ ] //    (no drive, no trailing '\')
	[ ] //  - strBuildBaseDir is the directory which contains the bin-directory where viewer.exe is installed 
	[ ] //    (no drive, no trailing '\') i.e. \Mozilla\dist\Win32_o.obj for a build-tree
	[ ] //  - strMilestoneOrPull is the milestone or date of source pull if not a milestone
	[ ] //    (this is the text put into the header of the HTML table: i.e. 'Milestone M13' or 'Pulled: 1/31/00')
	[ ] //  - numSlashes is the number of '/' characters in the URL up to the site name 
	[ ] //    (should not change unless you move the WebSites folder)
	[ ] //
	[ ] STRING strPerfToolsDrive = "s:"
	[ ] STRING strViewerDrive = "d:"
	[ ] STRING strURLPath = "s:\Mozilla\Tools\performance\layout\40-url.txt"
	[ ] STRING strPerfToolsDir = "\Mozilla\Tools\Performance\layout"
	[ ] STRING strBuildBaseDir = "\Moz-0225"
	[ ] STRING strMilestoneOrPull = "Daily:022500 "
	[ ] INTEGER numSlashes = 10
	[ ] INTEGER sleepBetweenSites = 5
	[ ] 
	[ ] STRING strViewerDir = strBuildBaseDir + "\bin"
	[ ] HFILE inputFile
	[ ] STRING sLine
	[ ] STRING quotedURL
	[ ] STRING logFile = ""
	[ ] STRING site
	[ ] INTEGER NumOfSites = 0
	[ ] STRING strPerfToolsDriveCmd = strPerfToolsDrive + " <ENTER>"
	[ ] STRING strViewerDriveCmd = strViewerDrive + " <ENTER>"
	[ ] STRING strBuildIDBase = strViewerDrive + strBuildBaseDir
	[ ] STRING strLogPath = strPerfToolsDrive + strPerfToolsDir + "\logs"
	[ ] 
	[ ] // make the logs directory (in case it does note exist already)
	[+] if ( !SYS_DirExists( strLogPath ) )
		[ ] SYS_MakeDir( strLogPath )
	[ ] // remove any ABORT.TXT file that may be around
	[+] if ( SYS_FileExists( strPerfToolsDrive + strPerfToolsDir + "\abort.txt" ) )
			[ ] SYS_RemoveFile( strPerfToolsDrive + strPerfToolsDir + "\abort.txt" )
	[ ] // Copy the history file in case there are problems
	[-] if(  SYS_FileExists( strPerfToolsDrive + strPerfToolsDir + "\history.txt" ) )
		[ ] SYS_CopyFile( strPerfToolsDrive + strPerfToolsDir + "\History.txt", strPerfToolsDrive + strPerfToolsDir + "\History_prev.txt" )
	[ ] //
	[ ] // Start processing
	[ ] //
	[ ] inputFile = FileOpen (strURLPath, FM_READ)
	[ ] CommandPrompt.Invoke ()
	[ ] sleep (2)
	[ ] // move to the PerfTools directory
	[ ] CommandPrompt.TypeKeys (strPerfToolsDriveCmd)
	[ ] CommandPrompt.TypeKeys ("cd " + strPerfToolsDir + " <Enter>")
	[ ] // process the page-header
	[ ] CommandPrompt.TypeKeys ("perl " + strPerfToolsDrive + strPerfToolsDir +"\Header.pl {strBuildIDBase} {strMilestoneOrPull} <Enter>")
	[ ] sleep (1)
	[ ] // go through each URI and process it
	[-] while (FileReadLine (inputFile, sLine))
		[ ] CommandPrompt.SetActive ()
		[ ] site = GetField (sLine, '/', numSlashes) 
		[ ] logFile = site + "-log.txt"
		[ ] logFile = strLogPath + "\" + logFile
		[ ] CommandPrompt.TypeKeys (strViewerDrive + strViewerDir + "\viewer > {logFile}<Enter>")
		[ ] sleep (5)
		[ ] Raptor.SetActive ()
		[ ] Raptor.TextField1.SetText (sLine)
		[ ] Raptor.TextField1.TypeKeys ("<Enter>")
		[ ] sleep (sleepBetweenSites)
		[-] if Raptor.Exists ()
			[ ] Raptor.SetActive ()
			[ ] Raptor.Close ()
		[+] if RaptorSecondWindow.Exists ()
			[ ] RaptorSecondWindow.SetActive ()
			[ ] RaptorSecondWindow.Close ()
		[ ] CommandPrompt.SetActive ()
		[ ] CommandPrompt.TypeKeys( "<Ctrl-C><Enter>" )
		[ ] NumOfSites = NumOfSites + 1
		[ ] quotedURL = """" + sLine + """"
		[ ] CommandPrompt.TypeKeys ("perl "+ strPerfToolsDrive + strPerfToolsDir + "\AverageTable2.pl {site} {logFile} {NumOfSites} {strBuildIDBase} {quotedURL} <Enter>")
		[ ] sleep (2)
		[-] if ( SYS_FileExists( strPerfToolsDrive + strPerfToolsDir + "\abort.txt" ) )
			[ ] break
	[ ] CommandPrompt.TypeKeys ("perl "+ strPerfToolsDrive + strPerfToolsDir + "\Footer.pl {strBuildIDBase} {strMilestoneOrPull} <Enter>")
	[ ] sleep (1)
	[ ] CommandPrompt.Close()
