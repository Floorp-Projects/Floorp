/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/ 
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License 
 * for the specific language governing rights and limitations under the 
 * License. 
 * 
 * The Original Code is Mozilla Communicator client code, released March
 * 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation. Portions created by Netscape are Copyright (C) 1998-1999
 * Netscape Communications Corporation. All Rights Reserved.  
 * 
 * Contributors:
 *     Samir Gehani <sgehani@netscape.com>
 */


#ifndef _MIW_H_
#define _MIW_H_

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <Navigation.h>
#include <MacTypes.h>
#include <PLStringFuncs.h>

#include "FullPath.h"
#include "MoreFilesExtras.h"
#include "Threads.h"


/*-----------------------------------------------------------*
 *   SBI: SmartDownload administration 
 *-----------------------------------------------------------*/
#define WANT_WINTYPES 1
#define MACINTOSH 1
#ifndef _sdinst_h
#include "sdinst.h"
#endif
typedef HRESULT (_cdecl *SDI_NETINSTALL) (LPSDISTRUCT);
typedef Boolean (*EventProc)(const EventRecord*);

/*-----------------------------------------------------------*
 *   compile time switches [on=1; off=0]
 *-----------------------------------------------------------*/
#define CORRECT_DL_LOCATION 0	/* if on, download to "Temporary Items" */
#define CFG_IS_REMOTE 		0	/* if on, download remote config.ini file */	
#define SDINST_IS_DLL 		1	/* if on, load SDInstLib as code fragment */
#define	MOZILLA				0	/* if on, draws the Mozilla logo, not NS */
 
/*-----------------------------------------------------------*
 *   defines 
 *-----------------------------------------------------------*/
 
/* macros */
#define ERR_CHECK(_funcCall)	\
err = _funcCall;				\
if (err) 						\
{								\
	ErrorHandler();				\
	return;						\
}
										
#define ERR_CHECK_RET(_funcCall, _rv)	\
err = _funcCall;						\
if (err) 								\
{										\
	ErrorHandler();						\
	return _rv;							\
}
											
									
#define NUM_WINS 		5
#define kLicenseID		0		/* window object ids */
#define kWelcomeID		1
#define kSetupTypeID	2
#define kComponentsID	3
#define	kTerminalID		4

#define kMIWMagic		0x0F00BAA0

#define	NGINST			1		/* event handling modes */
#define	SDI				2

#define	MY_EOF			'\0'	/* parser constants */
#define	MAC_EOL			'\r'
#define WIN_EOL			'\n'
#define	START_SECTION	'['
#define END_SECTION		']'
#define KV_DELIM		'='

#define kScrollBarPad	3		/* constants */	
#define kTxtRectPad		5
#define	kInterWidgetPad	12
#define kScrollBarWidth 16 
#define kScrollAmount	11

#define kNumLicScrns 	4	
#define kNumWelcMsgs	3
#define	kNumWelcScrns	4

#define	kMaxSetupTypes	4
#define	kMaxComponents	64
#define kMaxNumKeys		16
#define kKeyMaxLen		128
#define kValueMaxLen	512
#define kSNameMaxLen	128	  /*    v--- for KV_DELIM char   */
#define	kSectionMaxLen	(kKeyMaxLen+1+kValueMaxLen)*kMaxNumKeys
#define kMaxURLPerComp	8
#define kURLMaxLen		255
#define kArchiveMaxLen	64
#define kGenIDIFileSize	0x2000
#define kEnableControl	0
#define kDisableControl	255
#define kNotSelected	0
#define	kSelected		1
#define kNotInSetupType 0	
#define kInSetupType	1	
#define kMaxCoreFiles	256		/* end constants */


#define rRootWin 		128		/* widget rsrc ids */
#define rBackBtn 		129
#define rNextBtn		130
#if MOZILLA == 1
#define rNSLogo			141
#else
#define rNSLogo			140
#endif
#define rNSLogoBox		130

#define	rLicBox			131
#define rLicScrollBar	132

#define rInstType		140
#define rInstDescBox	141
#define rDestLocBox		142
#define	rDestLoc		143

#define rCompListBox	150		/* note: overriden use for list and rect */
#define rCompDescBox	151

#define rStartMsgBox	160

#define rMBar			128		/* menu rsrc ids */
#define mApple			150
#define   iAbout		1
#define	mFile			151
#define   iQuit			1
#define mEdit			152
#define   iUndo			1
#define   iCut			3
#define   iCopy			4
#define   iPaste		5
#define   iClear		6


#define	rStringList 	140		/* i18n strings */
#define sBackBtn 		1 
#define sNextBtn 		2
#define sDeclineBtn 	3
#define sAcceptBtn	 	4
#if MOZILLA == 1
#define sNSInstTitle	22
#else
#define sNSInstTitle 	5
#endif
#define sInstallBtn		6
#define sLicenseFName	7
#define sInstLocTitle	8
#define sSelectFolder	9
#define	sOnDisk			10
#define	sInFolder		11
#define sCompDescTitle	12
#define sConfigFName	14		
#define sSDLib			15
#define sTempIDIName	16		
#define	sConfigIDIName	17		
#define sFolderDlgMsg	18
#define sDiskSpcAvail	19
#define sDiskSpcNeeded	20		
#define sKilobytes		21 		/* end i18n strings */


#define rParseKeys		141		/* parse keys in config.ini */
#define sGeneral		1
#define	sRunMode		2
#define sProductName	3
#define sProgFolderName 4
#define sProgFoldPath	5
#define sDefaultType	6
#define sShowDlg		7
#define sTitle			8
#define sMsg0			9
#define sMsg1			10
#define sMsg2			11


#define rIndices		142		/* integer indices in string form (0-9) */


#define rIDIKeys		143
#define sFile			1
#define	sDesc			2
#define sSDNSInstall	3		/* used when pasring INI */
#define	sCoreFile		4
#define sCoreDir		5
#define sNoAds			6
#define sSilent			7
#define sExecution		8
#define sConfirmInstall	9
#define sTrue			10
#define sFalse			11
#define	sNSInstall		12		/* used when generating IDI */

#define sWelcDlg		12

#define sLicDlg			14
#define sLicFile		15

#define sSetupTypeDlg	16
#define sSetupType		17
#define sDescShort		18
#define sDescLong		19
#define sC				20

#define sCompDlg		21
#define sComponent		22
#define sArchive		23
#define sInstSize		24
#define sAttr			25
#define	sURL			26

#define sTermDlg		27		/* end parse keys */

#define	eParseFailed	-501	/* errors */


/*-----------------------------------------------------------*
 *   structs 
 *-----------------------------------------------------------*/
typedef struct InstComp {
	Handle	shortDesc;
	Handle	longDesc;
	Handle	archive;
	long	size;
	Handle 	url[kMaxURLPerComp];
	short	numURLs;
	
	// TO DO: attributes

} InstComp; 

typedef struct SetupType {
	Handle	shortDesc;
	Handle	longDesc;
	short	comp[kMaxComponents];
	short	numComps;
} SetupType;
		
typedef struct Config {

	/* LicenseWin */
	Handle	licFileName;
	
	/* WelcomeWin */
	Handle	welcMsg[ kNumWelcMsgs ];
	
	/* SetupTypeWin */
	SetupType	st[kMaxSetupTypes];
	short   	numSetupTypes;
	
	/* ComponentsWin */
	short		numComps;
	Handle		selCompMsg;
	InstComp	comp[kMaxComponents];
	
	/* TerminalWin */
	Handle	startMsg;
	
	/* "Tunneled" IDI keys */
	Handle	coreFile;
	Handle	coreDir;
	Handle	noAds;
	Handle	silent;
	Handle	execution;
	Handle	confirmInstall;
	
} Config;

typedef struct Options {

	/* from SetupTypeWin */
	short			instChoice;
	short 			vRefNum;
	long			dirID;
	unsigned char*	folder;
		
	/* from ComponentsWin */
	short			compSelected[ kMaxComponents ];
	short			numCompSelected;
		/* NOTE: if instChoice is not last (i.e. not Custom) then populate
				 the compSelected list with the SetupType.comps associated */
} Options;

typedef struct LicWin {
	ControlHandle 	licBox;
	ControlHandle 	scrollBar;
	TEHandle		licTxt;
} LicWin;

typedef struct WelcWin {
	ControlHandle	welcBox;
	ControlHandle	scrollBar;
	TEHandle		welcTxt;
} WelcWin;

typedef struct SetupTypeWin {
	ControlHandle 	instType;		/* pull down */
	ControlHandle	instDescBox;
	TEHandle		instDescTxt;
	ControlHandle 	destLocBox; 	/* destLoc = destination location */
	ControlHandle	destLoc;		/* Select Folder... */
} SetupTypeWin;

typedef struct CompWin {
	ControlHandle	selCompMsg;
	Rect			compListBox; /* from 'RECT' resource */
	ListHandle		compList;
	ControlHandle	compDescBox;
	TEHandle		compDescTxt;
} CompWin;

typedef struct TermWin {
	TEHandle	startMsg;		
	Rect		startMsgBox;
} TermWin;

typedef struct InstWiz {
	
	/* config.ini options parsed */
	Config		*cfg;
	
	/* user selected Install Wizard-wide persistent options */
	Options		*opt;
	
	/* Window control-holding abstractions */
	LicWin			*lw;
	WelcWin			*ww;
	SetupTypeWin	*stw;
	CompWin			*cw;
	TermWin 		*tw;
			
	/* General wizard controls */
	ControlHandle backB;
	ControlHandle nextB;
	
} InstWiz;


/*-----------------------------------------------------------*
 *   globals 
 *-----------------------------------------------------------*/
extern WindowPtr 	gWPtr;
extern short 		gCurrWin;
extern InstWiz		*gControls;
extern Boolean 		gDone;
extern Boolean		gSDDlg;

extern EventProc		 gSDIEvtHandler;
extern SDI_NETINSTALL 	 gInstFunc;
extern CFragConnectionID gConnID;


/*-----------------------------------------------------------*
 *   prototypes 
 *-----------------------------------------------------------*/
#ifdef __cplusplus
extern "C" {
#endif

void 		main(void);
void		Init(void);
void		InitControlsObject(void);
OSErr		GetCWD(long *outDirID, short *outVRefNum);
void		InitOptObject(void);
void		InitManagers(void);
void		MakeMenus(void);
void 		MainEventLoop(void);
void		ErrorHandler(void);
void		Shutdown(void);

/*-----------------------------------------------------------*
 *   Parser 
 *-----------------------------------------------------------*/
pascal void *PullDownConfig(void*);
void		ParseConfig(void);
Boolean		ReadConfigFile(char **);
OSErr		PopulateLicWinKeys(char *);
OSErr		PopulateWelcWinKeys(char *);
OSErr		PopulateSetupTypeWinKeys(char *);
OSErr		PopulateCompWinKeys(char *);
OSErr		PopulateTermWinKeys(char *);
OSErr		PopulateIDIKeys(char *);
Boolean 	FillKeyValueForIDIKey(short, Handle, char *);
Boolean		FillKeyValueUsingResID(short, short, Handle, char *);
Boolean		FillKeyValueUsingSLID(short, short, short, Handle, char *);
Boolean		FillKeyValueUsingName(char *, char *, Handle, char *);
Boolean 	FindKeyValue(const char *, const char *, const char *, char *);
Boolean		GetNextSection(char **, char *, char *);
Boolean		GetNextKeyVal(char **, char *, char*);
unsigned char *CToPascal(char *);
char *		PascalToC(unsigned char *);

/*-----------------------------------------------------------*
 *   EvtHandlers
 *-----------------------------------------------------------*/
void 		HandleNextEvent(EventRecord *);
void		HandleMouseDown(EventRecord *);
void		HandleKeyDown(EventRecord *);
void		HandleUpdateEvt(EventRecord *);
void 		HandleActivateEvt(EventRecord *);
void 		HandleOSEvt(EventRecord *);
void		React2InContent(EventRecord *, WindowPtr);

/*-----------------------------------------------------------*
 *   LicenseWin
 *-----------------------------------------------------------*/
void 		ShowLicenseWin(void);
void		InLicenseContent(EventRecord*, WindowPtr);
void		EnableLicenseWin(void);
void		DisableLicenseWin(void);
void		InitLicTxt(void);
void		ShowTxt(void);
void		ShowLogo(void);
void		InitScrollBar(ControlHandle);
pascal void	DoScrollProc(ControlHandle, short);
void		CalcChange(ControlHandle, short *);
void 		ShowNavButtons(unsigned char*, unsigned char*);
void 		EnableNavButtons(void);
void		DisableNavButtons(void);

/*-----------------------------------------------------------*
 *   WelcomeWin
 *-----------------------------------------------------------*/
void		ShowWelcomeWin(void);
void		InitWelcTxt(void);
void 		InWelcomeContent(EventRecord*, WindowPtr);
void		EnableWelcomeWin(void);
void 		DisableWelcomeWin(void);

/*-----------------------------------------------------------*
 *   SetupTypeWin
 *-----------------------------------------------------------*/
void		ShowSetupTypeWin(void);
void		ShowSetupDescTxt(void);
void		GetAllVInfo(unsigned char **, short *);
pascal void OurNavEventFunction(NavEventCallbackMessage callBackSelector, 
			NavCBRecPtr callBackParms, NavCallBackUserData callBackUD);
void		DrawDiskNFolder(short, unsigned char *);
void		DrawDiskSpaceMsgs(short);
char*		DiskSpaceNeeded(void);
void		ClearDiskSpaceMsgs(void);
char*		ltoa(long);
short		pstrcmp(unsigned char*, unsigned char*);
unsigned char* pstrcpy(unsigned char*, unsigned char*);
void		InSetupTypeContent(EventRecord *, WindowPtr);
void		EnableSetupTypeWin(void);
void		DisableSetupTypeWin(void);

/*-----------------------------------------------------------*
 *   ComponentsWin
 *-----------------------------------------------------------*/
void		ShowComponentsWin(void);
Boolean		PopulateCompInfo(void);
void		UpdateCompWin(void);
void		InComponentsContent(EventRecord*, WindowPtr);
void		SetOptInfo(void);
void		EnableComponentsWin(void);
void		DisableComponentsWin(void);

/*-----------------------------------------------------------*
 *   TerminalWin
 *-----------------------------------------------------------*/
void		ShowTerminalWin(void);
void		InTerminalContent(EventRecord*, WindowPtr);
void		UpdateTerminalWin(void);
Boolean		SpawnSDThread(ThreadEntryProcPtr, ThreadID *);
void		EnableTerminalWin(void);
void		DisableTerminalWin(void);

/*-----------------------------------------------------------*
 *   InstAction
 *-----------------------------------------------------------*/
pascal void *Install(void*);
Boolean 	GenerateIDIFromOpt(Str255, long, short, FSSpec *);
void		AddKeyToIDI(short, Handle, char *);
Boolean		InitSDLib(void);
Boolean		LoadSDLib(FSSpec, SDI_NETINSTALL *, EventProc *, CFragConnectionID *);
Boolean		UnloadSDLib(CFragConnectionID *);

/*-----------------------------------------------------------*
 *   Deflation
 *-----------------------------------------------------------*/
OSErr		ExtractCoreFile(void);
OSErr		AppleSingleDecode(FSSpecPtr, FSSpecPtr);
void		ResolveDirs(char *, char*);
OSErr		ForceMoveFile(short, long, ConstStr255Param, long);
OSErr		CleanupExtractedFiles(void);

/*-----------------------------------------------------------*
 *   XPInstallGlue
 *-----------------------------------------------------------*/
OSErr		RunAllXPIs(short vRefNum, long dirID);
OSErr		RunXPI(FSSpec&, FSSpec&);
/* NB:
** See XPInstallGlue.c for rest of prototypes
*/

/*-----------------------------------------------------------*
 *   GreyButton
 *-----------------------------------------------------------*/
pascal void 	FrameGreyButton( Rect *buttonFrame );
void 			SetFrameGreyButtonColor( short color );

#ifdef __cplusplus
}
#endif

#endif /* _MIW_H_ */