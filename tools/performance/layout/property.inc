[ ] 
[ ] use "c:\program files\segue\qworks\bwcompat.inc"
[ ] 
[+] window DialogBox CommandPrompt
	[ ] tag "c:\winnt\system32\cmd.exe"
	[-] Invoke ()
		[ ] APP_START ("c:\winnt\system32\cmd.exe")
		[ ] 
[ ] 
[+] window MainWin Raptor
	[-] multitag "*Raptor"
		[ ] "*Raptor"
		[ ] "#1"
		[ ] "$D:\VIEWER\mozilla\dist\WIN32_O.OBJ\bin\viewer.exe"
	[+] Menu File
		[+] multitag "File"
			[ ] "#1"
		[+] MenuItem NewWindow
			[+] multitag "New Window"
				[ ] "#1"
				[ ] "$40009"
		[+] MenuItem Open
			[+] multitag "Open"
				[ ] "#2"
				[ ] "$40010"
		[+] MenuItem ViewSource
			[+] multitag "ViewSource"
				[ ] "#3"
				[ ] "$40004"
		[+] Menu Samples
			[+] multitag "Samples"
				[ ] "#4"
			[+] MenuItem Demo0
				[+] multitag "demo #0"
					[ ] "#1"
					[ ] "$40011"
			[+] MenuItem Demo1
				[+] multitag "demo #1"
					[ ] "#2"
					[ ] "$40012"
			[+] MenuItem Demo2
				[+] multitag "demo #2"
					[ ] "#3"
					[ ] "$40013"
			[+] MenuItem Demo3
				[+] multitag "demo #3"
					[ ] "#4"
					[ ] "$40014"
			[+] MenuItem Demo4
				[+] multitag "demo #4"
					[ ] "#5"
					[ ] "$40015"
			[+] MenuItem Demo5
				[+] multitag "demo #5"
					[ ] "#6"
					[ ] "$40016"
			[+] MenuItem Demo6
				[+] multitag "demo #6"
					[ ] "#7"
					[ ] "$40017"
			[+] MenuItem Demo7
				[+] multitag "demo #7"
					[ ] "#8"
					[ ] "$40018"
			[+] MenuItem Demo8
				[+] multitag "demo #8"
					[ ] "#9"
					[ ] "$40019"
			[+] MenuItem Demo9
				[+] multitag "demo #9"
					[ ] "#10"
					[ ] "$40020"
			[+] MenuItem Demo10
				[+] multitag "demo #10"
					[ ] "#11"
					[ ] "$40021"
			[+] MenuItem Demo11
				[+] multitag "demo #11"
					[ ] "#12"
					[ ] "$40022"
			[+] MenuItem Demo12
				[+] multitag "demo #12"
					[ ] "#13"
					[ ] "$40023"
			[+] MenuItem Demo13
				[+] multitag "demo #13"
					[ ] "#14"
					[ ] "$40024"
			[+] MenuItem Demo14
				[+] multitag "demo #14"
					[ ] "#15"
					[ ] "$40025"
			[+] MenuItem Demo15
				[+] multitag "demo #15"
					[ ] "#16"
					[ ] "$40026"
			[+] MenuItem Demo16
				[+] multitag "demo #16"
					[ ] "#17"
					[ ] "$40027"
			[+] MenuItem Demo17
				[+] multitag "demo #17"
					[ ] "#18"
					[ ] "$40028"
		[+] MenuItem TestSites
			[+] multitag "Test Sites"
				[ ] "#5"
				[ ] "$40300"
		[+] Menu XPToolkitTests
			[+] multitag "XPToolkit Tests"
				[ ] "#6"
			[+] MenuItem ToolbarTest1
				[+] multitag "Toolbar Test 1"
					[ ] "#1"
					[ ] "$40900"
			[+] MenuItem TreeTest1
				[+] multitag "Tree Test 1"
					[ ] "#2"
					[ ] "$40901"
		[+] MenuItem PrintPreview
			[+] multitag "Print Preview"
				[ ] "#7"
				[ ] "$40051"
		[+] MenuItem Print
			[+] multitag "Print"
				[ ] "#8"
				[ ] "$40060"
		[+] MenuItem PrintSetup
			[+] multitag "Print Setup"
				[ ] "#9"
				[ ] "$40061"
		[+] MenuItem Exit
			[+] multitag "Exit"
				[ ] "#10"
				[ ] "$40002"
	[+] Menu Edit
		[+] multitag "Edit"
			[ ] "#2"
		[+] MenuItem Cut
			[+] multitag "Cut"
				[ ] "#1"
				[ ] "$40201"
		[+] MenuItem Copy
			[+] multitag "Copy"
				[ ] "#2"
				[ ] "$40202"
		[+] MenuItem Paste
			[+] multitag "Paste"
				[ ] "#3"
				[ ] "$40203"
		[+] MenuItem MenuItem4
			[+] multitag "#4"
				[ ] "$40204"
		[+] MenuItem FindInPage
			[+] multitag "Find in Page"
				[ ] "#5"
				[ ] "$40205"
		[+] MenuItem Prefs
			[+] multitag "Prefs"
				[ ] "#6"
				[ ] "$40130"
	[+] Menu Debug
		[+] multitag "Debug"
			[ ] "#3"
		[+] MenuItem VisualDebugging
			[+] multitag "Visual Debugging"
				[ ] "#1"
				[ ] "$40029"
		[+] Menu EventDebugging
			[+] multitag "Event Debugging"
				[ ] "#2"
			[+] MenuItem TogglePaintFlashing
				[+] multitag "Toggle Paint Flashing"
					[ ] "#1"
					[ ] "$40200"
			[+] MenuItem TogglePaintDumping
				[+] multitag "Toggle Paint Dumping"
					[ ] "#2"
					[ ] "$40210"
			[+] MenuItem ToggleInvalidateDumping
				[+] multitag "Toggle Invalidate Dumping"
					[ ] "#3"
					[ ] "$40220"
			[+] MenuItem ToggleEventDumping
				[+] multitag "Toggle Event Dumping"
					[ ] "#4"
					[ ] "$40230"
			[+] MenuItem ToggleMotionEventDumping
				[+] multitag "Toggle Motion Event Dumping"
					[ ] "#5"
					[ ] "$40240"
			[+] MenuItem ToggleCrossingEventDumping
				[+] multitag "Toggle Crossing Event Dumping"
					[ ] "#6"
					[ ] "$40250"
		[+] MenuItem ReflowTest
			[+] multitag "Reflow Test"
				[ ] "#3"
				[ ] "$40030"
		[+] MenuItem DumpContent
			[+] multitag "Dump Content"
				[ ] "#4"
				[ ] "$40031"
		[+] MenuItem DumpFrames
			[+] multitag "Dump Frames"
				[ ] "#5"
				[ ] "$40032"
		[+] MenuItem DumpViews
			[+] multitag "Dump Views"
				[ ] "#6"
				[ ] "$40033"
		[+] MenuItem DumpStyleSheets
			[+] multitag "Dump Style Sheets"
				[ ] "#7"
				[ ] "$40034"
		[+] MenuItem DumpStyleContexts
			[+] multitag "Dump Style Contexts"
				[ ] "#8"
				[ ] "$40035"
		[+] MenuItem ShowContentSize
			[+] multitag "Show Content Size"
				[ ] "#9"
				[ ] "$40037"
		[+] MenuItem ShowFrameSize
			[+] multitag "Show Frame Size"
				[ ] "#10"
				[ ] "$40038"
		[+] MenuItem ShowStyleSize
			[+] multitag "Show Style Size"
				[ ] "#11"
				[ ] "$40039"
		[+] MenuItem DebugSave
			[+] multitag "Debug Save"
				[ ] "#12"
				[ ] "$40040"
		[+] MenuItem DebugDisplayText
			[+] multitag "Debug Display Text"
				[ ] "#13"
				[ ] "$40049"
		[+] MenuItem DebugDisplayHTML
			[+] multitag "Debug Display HTML"
				[ ] "#14"
				[ ] "$40050"
		[+] MenuItem DebugToggleSelection
			[+] multitag "Debug Toggle Selection"
				[ ] "#15"
				[ ] "$40042"
		[+] MenuItem DebugRobot
			[+] multitag "Debug Robot"
				[ ] "#16"
				[ ] "$40036"
		[+] MenuItem ShowContentQuality
			[+] multitag "Show Content Quality"
				[ ] "#17"
				[ ] "$40041"
	[+] Menu Style
		[-] multitag "Style"
			[ ] "#4"
		[+] Menu SelectStyleSheet
			[+] multitag "Select Style Sheet"
				[ ] "#1"
			[+] MenuItem ListAvailabeSheets
				[+] multitag "List Availabe Sheets"
					[ ] "#1"
					[ ] "$40500"
			[+] MenuItem SelectDefault
				[+] multitag "Select Default"
					[ ] "#2"
					[ ] "$40501"
			[+] MenuItem SelectAlternate1
				[+] multitag "Select Alternate 1"
					[ ] "#3"
					[ ] "$40502"
			[+] MenuItem SelectAlternate2
				[+] multitag "Select Alternate 2"
					[ ] "#4"
					[ ] "$40503"
			[+] MenuItem SelectAlternate3
				[+] multitag "Select Alternate 3"
					[ ] "#5"
					[ ] "$40504"
			[+] MenuItem SelectAlternate4
				[+] multitag "Select Alternate 4"
					[ ] "#6"
					[ ] "$40505"
		[+] Menu CompatibilityModePref
			[+] multitag "Compatibility Mode Pref"
				[ ] "#2"
			[+] MenuItem NavQuirks
				[+] multitag "Nav Quirks"
					[ ] "#1"
					[ ] "$40043"
			[+] MenuItem Standard
				[+] multitag "Standard"
					[ ] "#2"
					[ ] "$40044"
		[+] Menu WidgetRenderingModePref
			[+] multitag "Widget Rendering Mode Pref"
				[ ] "#3"
			[+] MenuItem Native
				[-] multitag "Native"
					[ ] "#1"
					[ ] "$40047"
			[+] MenuItem Gfx
				[+] multitag "Gfx"
					[ ] "#2"
					[ ] "$40048"
	[+] Menu Tools
		[+] multitag "Tools"
			[ ] "#5"
		[+] MenuItem JavaScriptConsole
			[+] multitag "JavaScript Console"
				[ ] "#1"
				[ ] "$40100"
		[+] MenuItem EditorMode
			[+] multitag "Editor Mode"
				[ ] "#2"
				[ ] "$40120"
		[+] Menu PrivacyTools
			[+] multitag "Privacy Tools"
				[ ] "#3"
			[+] MenuItem SafePrefillFromWallet
				[+] multitag "Safe Prefill from Wallet"
					[ ] "#1"
					[ ] "$40290"
			[+] MenuItem QuickPrefillFromWallet
				[+] multitag "Quick Prefill from Wallet"
					[ ] "#2"
					[ ] "$40291"
			[+] MenuItem DisplayWallet
				[+] multitag "Display Wallet"
					[ ] "#3"
					[ ] "$40292"
			[+] MenuItem DisplayCookies
				[+] multitag "Display Cookies"
					[ ] "#4"
					[ ] "$40293"
			[+] MenuItem DisplaySignons
				[+] multitag "Display Signons"
					[ ] "#5"
					[ ] "$40294"
		[+] Menu EditorTests
			[+] multitag "Editor Tests"
				[ ] "#4"
			[+] MenuItem SetBackgroundRED
				[+] multitag "Set Background RED"
					[ ] "#1"
					[ ] "$40548"
			[+] MenuItem SetBackgroundYELLOW
				[+] multitag "Set Background YELLOW"
					[ ] "#2"
					[ ] "$40549"
	[+] TextField TextField2
		[+] multitag "#2"
			[ ] "$0[2]"
			[ ] "@(306,342)"
	[+] CustomWin NetscapeWindowClass1
		[+] msw multitag "[NetscapeWindowClass]#1"
			[ ] "[NetscapeWindowClass]$0[1]"
			[ ] "[NetscapeWindowClass]@(596,16)"
	[+] TextField TextField1
		[+] multitag "#1"
			[ ] "$0[1]"
			[ ] "@(380,16)"
	[+] PushButton Forward
		[+] multitag "Forward"
			[ ] "#2"
			[ ] "$0[2]"
			[ ] "@(135,16)"
	[+] PushButton Back
		[+] multitag "Back"
			[ ] "#1"
			[ ] "$0[1]"
			[ ] "@(45,16)"
	[+] CustomWin NetscapeWindowClass2
		[+] msw multitag "[NetscapeWindowClass]#2"
			[ ] "[NetscapeWindowClass]$0[2]"
			[ ] "[NetscapeWindowClass]@(306,181)"
		[+] CustomWin NetscapeWindowClass1
			[+] msw multitag "[NetscapeWindowClass]#1"
				[ ] "[NetscapeWindowClass]$0"
				[ ] "[NetscapeWindowClass]@(306,149)"
			[+] ScrollBar ScrollBar1
				[+] multitag "#1"
					[ ] "$0[1]"
					[ ] "@(604,141)"
			[+] ScrollBar ScrollBar2
				[+] multitag "#2"
					[ ] "$0[2]"
					[ ] "@(298,290)"
			[+] CustomWin NetscapeWindowClass2
				[+] msw multitag "[NetscapeWindowClass]#2"
					[ ] "[NetscapeWindowClass]$0[2]"
					[ ] "[NetscapeWindowClass]@(604,290)"
			[+] CustomWin NetscapeWindowClass1
				[+] msw multitag "[NetscapeWindowClass]#1"
					[ ] "[NetscapeWindowClass]$0[1]"
					[ ] "[NetscapeWindowClass]@(298,141)"
				[+] PushButton Search
					[+] multitag "Search"
						[ ] "#1"
						[ ] "$0"
						[ ] "@(394,149)"
				[+] TextField TextField1
					[+] multitag "#1"
						[ ] "$0"
						[ ] "@(223,143)"
[ ] 
[+] // window MainWin Raptor
	[ ] // tag "*Raptor"
	[-] // Menu File
		[ ] // tag "File"
		[+] // MenuItem NewWindow
			[+] // multitag "New Window"
				[ ] // "#1"
				[ ] // "$40009"
		[+] // MenuItem Open
			[+] // multitag "Open"
				[ ] // "#2"
				[ ] // "$40010"
		[+] // MenuItem ViewSource
			[+] // multitag "ViewSource"
				[ ] // "#3"
				[ ] // "$40004"
		[+] // Menu Samples
			[+] // multitag "Samples"
				[ ] // "#4"
			[+] // MenuItem Demo0
				[+] // multitag "demo #0"
					[ ] // "#1"
					[ ] // "$40011"
			[+] // MenuItem Demo1
				[+] // multitag "demo #1"
					[ ] // "#2"
					[ ] // "$40012"
			[+] // MenuItem Demo2
				[+] // multitag "demo #2"
					[ ] // "#3"
					[ ] // "$40013"
			[+] // MenuItem Demo3
				[+] // multitag "demo #3"
					[ ] // "#4"
					[ ] // "$40014"
			[+] // MenuItem Demo4
				[+] // multitag "demo #4"
					[ ] // "#5"
					[ ] // "$40015"
			[+] // MenuItem Demo5
				[+] // multitag "demo #5"
					[ ] // "#6"
					[ ] // "$40016"
			[+] // MenuItem Demo6
				[+] // multitag "demo #6"
					[ ] // "#7"
					[ ] // "$40017"
			[+] // MenuItem Demo7
				[+] // multitag "demo #7"
					[ ] // "#8"
					[ ] // "$40018"
			[+] // MenuItem Demo8
				[+] // multitag "demo #8"
					[ ] // "#9"
					[ ] // "$40019"
			[+] // MenuItem Demo9
				[+] // multitag "demo #9"
					[ ] // "#10"
					[ ] // "$40020"
			[+] // MenuItem Demo10
				[+] // multitag "demo #10"
					[ ] // "#11"
					[ ] // "$40021"
			[+] // MenuItem Demo11
				[+] // multitag "demo #11"
					[ ] // "#12"
					[ ] // "$40022"
			[+] // MenuItem Demo12
				[+] // multitag "demo #12"
					[ ] // "#13"
					[ ] // "$40023"
			[+] // MenuItem Demo13
				[+] // multitag "demo #13"
					[ ] // "#14"
					[ ] // "$40024"
			[+] // MenuItem Demo14
				[+] // multitag "demo #14"
					[ ] // "#15"
					[ ] // "$40025"
			[+] // MenuItem Demo15
				[+] // multitag "demo #15"
					[ ] // "#16"
					[ ] // "$40026"
			[+] // MenuItem Demo16
				[+] // multitag "demo #16"
					[ ] // "#17"
					[ ] // "$40027"
			[+] // MenuItem Demo17
				[+] // multitag "demo #17"
					[ ] // "#18"
					[ ] // "$40028"
		[+] // MenuItem TestSites
			[+] // multitag "Test Sites"
				[ ] // "#5"
				[ ] // "$40300"
		[+] // Menu XPToolkitTests
			[+] // multitag "XPToolkit Tests"
				[ ] // "#6"
			[+] // MenuItem ToolbarTest1
				[+] // multitag "Toolbar Test 1"
					[ ] // "#1"
					[ ] // "$40900"
			[+] // MenuItem TreeTest1
				[+] // multitag "Tree Test 1"
					[ ] // "#2"
					[ ] // "$40901"
		[+] // MenuItem PrintPreview
			[+] // multitag "Print Preview"
				[ ] // "#7"
				[ ] // "$40051"
		[+] // MenuItem Print
			[+] // multitag "Print"
				[ ] // "#8"
				[ ] // "$40060"
		[+] // MenuItem PrintSetup
			[+] // multitag "Print Setup"
				[ ] // "#9"
				[ ] // "$40061"
		[-] // MenuItem Exit
			[ ] // tag "Exit"
	[+] // Menu Edit
		[+] // multitag "Edit"
			[ ] // "#2"
		[+] // MenuItem Cut
			[+] // multitag "Cut"
				[ ] // "#1"
				[ ] // "$40201"
		[+] // MenuItem Copy
			[+] // multitag "Copy"
				[ ] // "#2"
				[ ] // "$40202"
		[+] // MenuItem Paste
			[+] // multitag "Paste"
				[ ] // "#3"
				[ ] // "$40203"
		[+] // MenuItem MenuItem4
			[+] // multitag "#4"
				[ ] // "$40204"
		[+] // MenuItem FindInPage
			[+] // multitag "Find in Page"
				[ ] // "#5"
				[ ] // "$40205"
		[+] // MenuItem Prefs
			[+] // multitag "Prefs"
				[ ] // "#6"
				[ ] // "$40130"
	[+] // Menu Debug
		[+] // multitag "Debug"
			[ ] // "#3"
		[+] // MenuItem VisualDebugging
			[+] // multitag "Visual Debugging"
				[ ] // "#1"
				[ ] // "$40029"
		[+] // Menu EventDebugging
			[+] // multitag "Event Debugging"
				[ ] // "#2"
			[+] // MenuItem TogglePaintFlashing
				[+] // multitag "Toggle Paint Flashing"
					[ ] // "#1"
					[ ] // "$40200"
			[+] // MenuItem TogglePaintDumping
				[+] // multitag "Toggle Paint Dumping"
					[ ] // "#2"
					[ ] // "$40210"
			[+] // MenuItem ToggleInvalidateDumping
				[+] // multitag "Toggle Invalidate Dumping"
					[ ] // "#3"
					[ ] // "$40220"
			[+] // MenuItem ToggleEventDumping
				[+] // multitag "Toggle Event Dumping"
					[ ] // "#4"
					[ ] // "$40230"
			[+] // MenuItem ToggleMotionEventDumping
				[+] // multitag "Toggle Motion Event Dumping"
					[ ] // "#5"
					[ ] // "$40240"
			[+] // MenuItem ToggleCrossingEventDumping
				[+] // multitag "Toggle Crossing Event Dumping"
					[ ] // "#6"
					[ ] // "$40250"
		[+] // MenuItem ReflowTest
			[+] // multitag "Reflow Test"
				[ ] // "#3"
				[ ] // "$40030"
		[+] // MenuItem DumpContent
			[+] // multitag "Dump Content"
				[ ] // "#4"
				[ ] // "$40031"
		[+] // MenuItem DumpFrames
			[+] // multitag "Dump Frames"
				[ ] // "#5"
				[ ] // "$40032"
		[+] // MenuItem DumpViews
			[+] // multitag "Dump Views"
				[ ] // "#6"
				[ ] // "$40033"
		[+] // MenuItem DumpStyleSheets
			[+] // multitag "Dump Style Sheets"
				[ ] // "#7"
				[ ] // "$40034"
		[+] // MenuItem DumpStyleContexts
			[+] // multitag "Dump Style Contexts"
				[ ] // "#8"
				[ ] // "$40035"
		[+] // MenuItem ShowContentSize
			[+] // multitag "Show Content Size"
				[ ] // "#9"
				[ ] // "$40037"
		[+] // MenuItem ShowFrameSize
			[+] // multitag "Show Frame Size"
				[ ] // "#10"
				[ ] // "$40038"
		[+] // MenuItem ShowStyleSize
			[+] // multitag "Show Style Size"
				[ ] // "#11"
				[ ] // "$40039"
		[+] // MenuItem DebugSave
			[+] // multitag "Debug Save"
				[ ] // "#12"
				[ ] // "$40040"
		[+] // MenuItem DebugDisplayText
			[+] // multitag "Debug Display Text"
				[ ] // "#13"
				[ ] // "$40049"
		[+] // MenuItem DebugDisplayHTML
			[+] // multitag "Debug Display HTML"
				[ ] // "#14"
				[ ] // "$40050"
		[+] // MenuItem DebugToggleSelection
			[+] // multitag "Debug Toggle Selection"
				[ ] // "#15"
				[ ] // "$40042"
		[+] // MenuItem DebugRobot
			[+] // multitag "Debug Robot"
				[ ] // "#16"
				[ ] // "$40036"
		[+] // MenuItem ShowContentQuality
			[+] // multitag "Show Content Quality"
				[ ] // "#17"
				[ ] // "$40041"
	[-] // Menu Style
		[ ] // tag "Style"
		[+] // Menu SelectStyleSheet
			[+] // multitag "Select Style Sheet"
				[ ] // "#1"
			[+] // MenuItem ListAvailabeSheets
				[+] // multitag "List Availabe Sheets"
					[ ] // "#1"
					[ ] // "$40500"
			[+] // MenuItem SelectDefault
				[+] // multitag "Select Default"
					[ ] // "#2"
					[ ] // "$40501"
			[+] // MenuItem SelectAlternate1
				[+] // multitag "Select Alternate 1"
					[ ] // "#3"
					[ ] // "$40502"
			[+] // MenuItem SelectAlternate2
				[+] // multitag "Select Alternate 2"
					[ ] // "#4"
					[ ] // "$40503"
			[+] // MenuItem SelectAlternate3
				[+] // multitag "Select Alternate 3"
					[ ] // "#5"
					[ ] // "$40504"
			[+] // MenuItem SelectAlternate4
				[+] // multitag "Select Alternate 4"
					[ ] // "#6"
					[ ] // "$40505"
		[+] // Menu CompatibilityModePref
			[+] // multitag "Compatibility Mode Pref"
				[ ] // "#2"
			[+] // MenuItem NavQuirks
				[+] // multitag "Nav Quirks"
					[ ] // "#1"
					[ ] // "$40043"
			[+] // MenuItem Standard
				[+] // multitag "Standard"
					[ ] // "#2"
					[ ] // "$40044"
		[-] // Menu WidgetRenderingModePref
			[ ] // tag "Widget Rendering Mode Pref"
			[-] // MenuItem Native
				[ ] // tag "Native"
			[-] // MenuItem Gfx
				[ ] // tag "Gfx"
	[+] // Menu Tools
		[+] // multitag "Tools"
			[ ] // "#5"
		[+] // MenuItem JavaScriptConsole
			[+] // multitag "JavaScript Console"
				[ ] // "#1"
				[ ] // "$40100"
		[+] // MenuItem EditorMode
			[+] // multitag "Editor Mode"
				[ ] // "#2"
				[ ] // "$40120"
		[+] // Menu PrivacyTools
			[+] // multitag "Privacy Tools"
				[ ] // "#3"
			[+] // MenuItem SafePrefillFromWallet
				[+] // multitag "Safe Prefill from Wallet"
					[ ] // "#1"
					[ ] // "$40290"
			[+] // MenuItem QuickPrefillFromWallet
				[+] // multitag "Quick Prefill from Wallet"
					[ ] // "#2"
					[ ] // "$40291"
			[+] // MenuItem DisplayWallet
				[+] // multitag "Display Wallet"
					[ ] // "#3"
					[ ] // "$40292"
			[+] // MenuItem DisplayCookies
				[+] // multitag "Display Cookies"
					[ ] // "#4"
					[ ] // "$40293"
			[+] // MenuItem DisplaySignons
				[+] // multitag "Display Signons"
					[ ] // "#5"
					[ ] // "$40294"
		[+] // Menu EditorTests
			[+] // multitag "Editor Tests"
				[ ] // "#4"
			[+] // MenuItem SetBackgroundRED
				[+] // multitag "Set Background RED"
					[ ] // "#1"
					[ ] // "$40548"
			[+] // MenuItem SetBackgroundYELLOW
				[+] // multitag "Set Background YELLOW"
					[ ] // "#2"
					[ ] // "$40549"
	[+] // TextField TextField2
		[+] // multitag "#2"
			[ ] // "$0[2]"
			[ ] // "@(306,342)"
	[+] // CustomWin NetscapeWindowClass1
		[+] // msw multitag "[NetscapeWindowClass]#1"
			[ ] // "[NetscapeWindowClass]$0[1]"
			[ ] // "[NetscapeWindowClass]@(596,16)"
	[+] // TextField TextField1
		[+] // multitag "#1"
			[ ] // "$0[1]"
			[ ] // "@(380,16)"
	[+] // PushButton Forward
		[+] // multitag "Forward"
			[ ] // "#2"
			[ ] // "$0[2]"
			[ ] // "@(135,16)"
	[+] // PushButton Back
		[+] // multitag "Back"
			[ ] // "#1"
			[ ] // "$0[1]"
			[ ] // "@(45,16)"
	[+] // CustomWin NetscapeWindowClass2
		[+] // msw multitag "[NetscapeWindowClass]#2"
			[ ] // "[NetscapeWindowClass]$0[2]"
			[ ] // "[NetscapeWindowClass]@(306,181)"
		[+] // CustomWin NetscapeWindowClass1
			[+] // msw multitag "[NetscapeWindowClass]#1"
				[ ] // "[NetscapeWindowClass]$0"
				[ ] // "[NetscapeWindowClass]@(306,149)"
			[+] // ScrollBar ScrollBar1
				[+] // multitag "#1"
					[ ] // "$0[1]"
					[ ] // "@(604,141)"
			[+] // ScrollBar ScrollBar2
				[+] // multitag "#2"
					[ ] // "$0[2]"
					[ ] // "@(298,290)"
			[+] // CustomWin NetscapeWindowClass2
				[+] // msw multitag "[NetscapeWindowClass]#2"
					[ ] // "[NetscapeWindowClass]$0[2]"
					[ ] // "[NetscapeWindowClass]@(604,290)"
			[+] // CustomWin NetscapeWindowClass1
				[+] // msw multitag "[NetscapeWindowClass]#1"
					[ ] // "[NetscapeWindowClass]$0[1]"
					[ ] // "[NetscapeWindowClass]@(298,141)"
				[+] // PushButton Search
					[+] // multitag "Search"
						[ ] // "#1"
						[ ] // "$0"
						[ ] // "@(394,149)"
				[+] // TextField TextField1
					[+] // multitag "#1"
						[ ] // "$0"
						[ ] // "@(223,143)"
[ ] 
[-] window MainWin RaptorSecondWindow
	[ ] tag "*Raptor"
	[-] CustomWin NetscapeWindowClass
		[-] msw multitag "[NetscapeWindowClass]#1"
			[ ] "[NetscapeWindowClass]$0"
			[ ] "[NetscapeWindowClass]@(229,201)"
		[+] CustomWin NetscapeWindowClass
			[+] msw multitag "[NetscapeWindowClass]#1"
				[ ] "[NetscapeWindowClass]$0"
				[ ] "[NetscapeWindowClass]@(229,201)"
			[+] ScrollBar ScrollBar1
				[+] multitag "#1"
					[ ] "$0"
					[ ] "@(450,201)"
			[-] CustomWin NetscapeWindowClass
				[+] msw multitag "[NetscapeWindowClass]#1"
					[ ] "[NetscapeWindowClass]$0"
					[ ] "[NetscapeWindowClass]@(221,201)"
