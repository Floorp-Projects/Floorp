/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Samir Gehani <sgehani@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#ifndef _MIW_H_
#define _MIW_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <TextUtils.h>
#include <Resources.h>
#include <LowMem.h>
#include <ToolUtils.h>
#include <Sound.h>
#include <Gestalt.h>
#include <Balloons.h>
#include <Folders.h>
#include <Lists.h>
#include <Navigation.h>
#include <MacTypes.h>
#include <PLStringFuncs.h>
#include <Icons.h>
#include <Appearance.h>
#if TARGET_CARBON || (UNIVERSAL_INTERFACES_VERSION >= 0x0330)
#include <ControlDefinitions.h>
#endif

#include "FullPath.h"
#include "MoreFilesExtras.h"
#include "IterateDirectory.h"
#include "Threads.h"

 
/*-----------------------------------------------------------*
 *   defines 
 *-----------------------------------------------------------*/
 
/* macros */
#define ERR_CHECK(_funcCall)	\
err = _funcCall;				\
if (err) 						\
{								\
    ErrorHandler(err, nil);         \
    return;                     \
}

#define ERR_CHECK_MSG(_funcCall, _rv, _msg) \
err = _funcCall;                \
if (err)                        \
{                               \
    ErrorHandler(err, _msg);            \
	return _rv;						\
}
										
#define ERR_CHECK_RET(_funcCall, _rv)	\
err = _funcCall;						\
if (err) 								\
{										\
    ErrorHandler(err, nil);                 \
	return _rv;							\
}
    
#define UNIFY_CHAR_CODE(_targetUint32, _src1char, _src2char, _src3char, _src4char)  \
    _targetUint32 =                                                                 \
    ( (unsigned long)                                                               \
    (((unsigned long)((_src1char & 0x000000FF) << 24)                               \
    | (unsigned long)((_src2char & 0x000000FF) << 16)                               \
    | (unsigned long)((_src3char & 0x000000FF) << 8)                                \
    | (unsigned long)((_src4char & 0x000000FF)))))
    
#define INVERT_HIGHLIGHT(_rectPtr)          \
            hiliteVal = LMGetHiliteMode();  \
            BitClr(&hiliteVal, pHiliteBit); \
            LMSetHiliteMode(hiliteVal);     \
            InvertRect(_rectPtr);
                                                        
                                    
#define NUM_WINS        5
#define kLicenseID      0       /* window object ids */
#define kWelcomeID      1
#define kSetupTypeID    2
#define kComponentsID   3
#define kAdditionsID    4
#define kTerminalID     5

#define kMIWMagic       0x0F00BAA0

#define NGINST          1       /* event handling modes */
#define SDI             2

#define MY_EOF          '\0'    /* parser constants */
#define MAC_EOL         '\r'
#define WIN_EOL         '\n'
#define START_SECTION   '['
#define END_SECTION     ']'
#define KV_DELIM        '='

#define kTempFolder     "\pTemp NSInstall"  // only used in debugging
#define kViewerFolder   "\p:viewer"

#define kScrollBarPad   3       /* constants */ 
#define kTxtRectPad     5
#define kInterWidgetPad 12
#define kScrollBarWidth 16 
#define kScrollAmount   11

#define kNumLicScrns    4   
#define kNumWelcMsgs    3
#define kNumWelcScrns   4

#define kMaxSetupTypes  4
#define kMaxComponents  64
#define kMaxNumKeys     16
#define kKeyMaxLen      128
#define kValueMaxLen    512
#define kSNameMaxLen    128   /*    v--- for KV_DELIM char   */
#define kSectionMaxLen  (kKeyMaxLen+1+kValueMaxLen)*kMaxNumKeys
#define kArchiveMaxLen  64
#define kGenIDIFileSize 2048
#define kEnableControl  0
#define kDisableControl 255
#define kNotSelected    0
#define kSelected       1
#define kNotInSetupType 0   
#define kInSetupType    1   
#define kDependeeOff    0
#define kDependeeOn     1
#define kInvalidCompIdx -999
#define kMaxCoreFiles   256     
#define kMaxProgUnits   100.0   
#define kMaxRunApps     32
#define kMaxLegacyChecks 32     
#define kMaxSites       32      
#define kMaxGlobalURLs  5       /* end constants */


#define rRootWin        128     /* widget rsrc ids */
#define rBackBtn        129
#define rNextBtn        130
#define rCancelBtn      174
#define rNSLogo         140
#define rNSLogoBox      130
#define rLogoImgWell    170

#define rLicBox         131
#define rLicScrollBar   132

#define rWelcMsgTextbox 171
#define rReadmeBtn      133

#define rInstType       140
#define rInstDescBox    141
#define rDestLocBox     142
#define rDestLoc        143

#define rCompListBox    150     /* note: overriden use for list and rect */
#define rCompDescBox    151

#define rCheckboxLDEF   128

#define rStartMsgBox    160
#define rAllProgBar     161
#define rPerXPIProgBar  162
#define rSiteSelector   163
#define rSiteSelMsg     179
#define rSaveCheckbox   164
#define rSaveBitsMsgBox 165
#define rProxySettgBtn  175
#define rDLSettingsGB   178
#define rDLProgBar      180
#define rLabDloading     181
#define rLabFrom        182
#define rLabTo          183
#define rLabRate        184
#define rLabTimeLeft    185

#define rGrayPixPattern 128

#define rAlrtDelOldInst 150
#define rAlrtOS85Reqd   160
#define rAlrtError      170
#define rWarnLessSpace  180
#define rDlgProxySettg  140

    
#define rMBar           128     /* menu rsrc ids */ 
#define mApple          150
#define   iAbout        1
#define mFile           151
#define   iQuit         1
#define mEdit           152
#define   iUndo         1
#define   iCut          3
#define   iCopy         4
#define   iPaste        5
#define   iClear        6

#define rAboutBox       128

#define rStringList     140     // common strings
#define sConfigFName    1
#define sInstallFName   2       
#define sTempIDIName    3       
#define sConfigIDIName  4       
#define sInstModules    5

#define rTitleStrList   170
#define sNSInstTitle    1


#define rParseKeys      141     /* parse keys in config.ini */
#define sGeneral        1
#define sRunMode        2
#define sProductName    3
#define sProgFolderName 4
#define sProgFoldPath   5
#define sDefaultType    6
#define sShowDlg        7
#define sTitle          8
#define sMsg0           9
#define sMsg1           10
#define sMsg2           11

#define rIndices        142     /* integer indices in string form (0-9) */


#define rIDIKeys        143
#define sFile           1
#define sDesc           2
#define sSDNSInstall    3       /* used when pasring INI */
#define sCoreFile       4
#define sCoreDir        5
#define sNoAds          6
#define sSilent         7
#define sExecution      8
#define sConfirmInstall 9
#define sTrue           10
#define sFalse          11
#define sNSInstall      12      /* used when generating IDI */

#define sWelcDlg        12
#define sReadmeFilename 32
#define sReadmeApp      33

#define sLicDlg         14
#define sLicFile        15

#define sSetupTypeDlg   16
#define sSetupType      17
#define sDescShort      18
#define sDescLong       19
#define sC              20

#define sCompDlg        21
#define sAddDlg         48
#define sComponent      22
#define sArchive        23
#define sInstSize       24
#define sAttributes     25
#define sURL            26
#define sServerPath     45
#define sDependee       31
#define sRandomInstall  34

#define sRunApp         35
#define sTargetApp      36
#define sTargetDoc      37

#define sLegacyCheck    38
#define sFilename       39
#define sSubfolder      49
#define sVersion        40
#define sMessage        41

#define sSiteSelector   42
#define sIdentifier     50
#define sDomain         43
#define sDescription    44
#define sRedirect       46
#define sSubpath        51

#define sTermDlg        27
        
#define sSELECTED       28
#define sINVISIBLE      29
#define sLAUNCHAPP      30  
#define sADDITIONAL     47  
#define sDOWNLOAD_ONLY  52  /* end parse keys */

#define rErrorList      144 /* errors */
#define eErrorMessage   1
#define eErr1           2
#define eErr2           3
#define eErr3           4
#define eParam          5
#define eMem            6
#define eParseFailed    7
#define eLoadLib        8
#define eUnknownDlgID   9   
#define eSpawn          10
#define eMenuHdl        11
#define eCfgRead        12
#define eDownload       13
#define eInstRead       6       // installer.ini read failed
#define eDLFailed       14

#define instErrsNum     13  /* number of the install.ini errors */

#define rInstList       145     // install.ini strings
#define sInstGeneral    1
#define sNsTitle        2
#define sMoTitle        3
#define sBackBtn        4 
#define sNextBtn        5
#define sCancel         6
#define sDeclineBtn     7
#define sAcceptBtn      8
#define sInstallBtn     9
#define sLicenseFName   10
#define sReadme         11
#define sInstLocTitle   12
#define sSelectFolder   13
#define sOnDisk         14
#define sInFolder       15
#define sCompDescTitle  16
#define sFolderDlgMsg   17
#define sDiskSpcAvail   18
#define sDiskSpcNeeded  19      
#define sKilobytes      20  
#define sExtracting     21      
#define sInstalling     22
#define sFileSp         23
#define sSpOfSp         24  
#define sProcessing     25      
#define sProxySettings  26
#define sProxyDlg       27
#define sProxyHost      28
#define sProxyPort      29
#define sProxyUsername  30
#define sProxyPassword  31
#define sOKBtn          32
#define sDLSettings     33
#define sSiteSelMsg     34
#define sLabDloading    35
#define sLabFrom        36
#define sLabTo          37
#define sLabRate        38
#define sTimeLeft       39
#define sDownloadKB     40
#define sDeleteBtn      41
#define sQuitBtn        42
#define sSpaceMsg1      43
#define sSpaceMsg2      44
#define sSpaceMsg3      45
#define sPauseBtn       46
#define sResumeBtn      47
#define sValidating     48
#define sExecuting      49

#define instKeysNum     49  /* number of installer.ini keys */

#define rInstMenuList   146
#define sMenuGeneral    1
#define sMenuAboutNs    2
#define sMenuAboutMo    3
#define sMenuFile       4
#define sMenuQuit       5
#define sMenuQuitHot    6
#define sMenuEdit       7
#define sMenuUndo       8
#define sMenuUndoHot    9
#define sMenuCut        10
#define sMenuCutHot     11
#define sMenuCopy       12
#define sMenuCopyHot    13
#define sMenuPaste      14
#define sMenuPasteHot   15
#define sMenuClear      16
#define sMenuClearHot   17
 
#define instMenuNum     17


/*-----------------------------------------------------------*
 *   structs 
 *-----------------------------------------------------------*/
typedef struct XPISpec {
    FSSpecPtr FSp;
    struct XPISpec *next;
} XPISpec;

typedef struct _errTableEnt {
    short num;
    Str255 msg;
} ErrTableEnt;


typedef struct InstComp {
    /* descriptions */
    Handle  shortDesc;
    Handle  longDesc;
    
    /* archive properties */
    Handle  archive;
    long    size;
    
    /* attributes */
    Boolean selected;
    Boolean invisible;
    Boolean launchapp;
    Boolean additional;
    Boolean download_only;
    
    /* dependees */
    Handle  depName[kMaxComponents];
    short   dep[kMaxComponents];
    short   numDeps;
    short   refcnt;
    
    /* UI highlighting */
    Boolean highlighted;
    
    Boolean dirty;    // dload has not been attempted, or, it failed CRC

} InstComp; 

typedef struct SetupType {
    Handle  shortDesc;
    Handle  longDesc;
    short   comp[kMaxComponents];
    short   numComps;
} SetupType;
        
typedef struct RunApp {
    Handle  targetApp;
    Handle  targetDoc;
} RunApp;

typedef struct LegacyCheck {
    Handle  filename;
    Handle  subfolder;
    Handle  version;
    Handle  message;
} LegacyCheck;

typedef struct SiteSelector {
    Handle  id;
    Handle  desc;
    Handle  domain;
} SiteSelector;

typedef struct Redirect {
    Handle  desc;
    Handle  subpath;
} Redirect; 

typedef struct Config {
    
    /*------------------------------------------------------------*
     *   Dialog Keys 
     *------------------------------------------------------------*/
     
    /* General */
    Handle  targetSubfolder;
    Handle  globalURL[kMaxGlobalURLs];
    short   numGlobalURLs;
    
    /* LicenseWin */
    Handle  licFileName;
    
    /* WelcomeWin */
    Handle  welcMsg[ kNumWelcMsgs ];
    Handle  readmeFile;
    Handle  readmeApp;
    Boolean bReadme;
    
    /* SetupTypeWin */
    SetupType   st[kMaxSetupTypes];
    short       numSetupTypes;
    
    /* ComponentsWin and AdditionsWin */
    short       numComps;
    Handle      selCompMsg;
    Handle      selAddMsg;
    Boolean     bAdditionsExist;
    InstComp    comp[kMaxComponents];
    
    /* TerminalWin */
    Handle       startMsg;
    short        numSites;
    SiteSelector site[kMaxSites];
    Redirect     redirect;
    Handle       saveBitsMsg;
    
    /* "Tunneled" IDI keys */
    Handle  coreFile;
    Handle  coreDir;
    Handle  noAds;
    Handle  silent;
    Handle  execution;
    Handle  confirmInstall;
    
    /*------------------------------------------------------------*
     *   Miscellaneous Keys
     *------------------------------------------------------------*/
    
    /* RunApp instances */
    RunApp      apps[kMaxRunApps];
    long        numRunApps;
    
    /* LegacyCheck instances */
    LegacyCheck checks[kMaxLegacyChecks];
    long        numLegacyChecks;
    
} Config;

typedef struct Options {

    /* from SetupTypeWin */
    short           instChoice;
    short           vRefNum;
    long            dirID;
    unsigned char*  folder;
        
    /* from ComponentsWin and AdditionsWin */
    short           compSelected[ kMaxComponents ];
    short           numCompSelected;
        /* NOTE: if instChoice is not last (i.e. not Custom) then populate
                 the compSelected list with the SetupType.comps associated */
                 
    /* from TerminalWin */
    short           siteChoice;
    Boolean         saveBits;
    char            *proxyHost;
    char            *proxyPort;
    char            *proxyUsername;
    char            *proxyPassword;
    
} Options;

typedef struct LicWin {
    ControlHandle   licBox;
    ControlHandle   scrollBar;
    TEHandle        licTxt;
} LicWin;

typedef struct WelcWin {
    ControlHandle   welcMsgCntl[kNumWelcMsgs];
    ControlHandle   readmeButton;
} WelcWin;

typedef struct SetupTypeWin {
    ControlHandle   instType;       /* pull down */
    ControlHandle   instDescBox;
    TEHandle        instDescTxt;
    ControlHandle   destLocBox;     /* destLoc = destination location */
    ControlHandle   destLoc;        /* Select Folder... */
} SetupTypeWin;

typedef struct CompWin {
    ControlHandle   selCompMsg;
    Rect            compListBox; /* from 'RECT' resource */
    ListHandle      compList;
    ControlHandle   compDescBox;
    TEHandle        compDescTxt;
} CompWin;

typedef struct TermWin {
    TEHandle        startMsg;       
    Rect            startMsgBox;
    
    ControlHandle   dlProgressBar;
    ControlHandle   allProgressBar;
    TEHandle        allProgressMsg;
    ControlHandle   xpiProgressBar;
    TEHandle        xpiProgressMsg;
    
    ControlHandle   dlSettingsGB;
    ControlHandle   dlLabels[5];
    TEHandle        dlProgressMsgs[5];

    ControlHandle   siteSelector;
    ControlHandle   siteSelMsg;
    
    ControlHandle   saveBitsCheckbox;
    TEHandle        saveBitsMsg;
    Rect            saveBitsMsgBox;
    
    ControlHandle   proxySettingsBtn;
} TermWin;

typedef enum {
    eInstallNotStarted = 0,
    eDownloading,
    ePaused,
    eResuming,
    eExtracting,
    eInstalling
} InstallState;

typedef struct InstWiz {
    
    /* config.ini options parsed */
    Config      *cfg;
    
    /* user selected Install Wizard-wide persistent options */
    Options     *opt;
    
    /* Window control-holding abstractions */
    LicWin          *lw;    /* LicenseWin */
    WelcWin         *ww;    /* WelcomeWin */
    SetupTypeWin    *stw;   /* SetupTypeWin */
    CompWin         *cw;    /* ComponentsWin */
    CompWin         *aw;    /* AdditionsWin */
    TermWin         *tw;    /* TerminalWin */
            
    /* General wizard controls */
    ControlHandle backB;
    ControlHandle nextB;
    ControlHandle cancelB;
    
    InstallState  state;
    int           resPos;
    
} InstWiz;

typedef struct InstINIRes {
    Str255  iKey[instKeysNum+1];
    Str255  iErr[instErrsNum+1];
    Str255  iMenu[instMenuNum+1];
} InstINIRes;

/*-----------------------------------------------------------*
 *   globals 
 *-----------------------------------------------------------*/
extern WindowPtr    gWPtr;
extern short        gCurrWin;
extern InstWiz      *gControls;
extern InstINIRes   *gStrings;
extern Boolean      gDone;
extern Boolean      gInstallStarted;


/*-----------------------------------------------------------*
 *   prototypes 
 *-----------------------------------------------------------*/
#ifdef __cplusplus
extern "C" {
#endif

void        main(void);
Boolean     VerifyEnv(void);
void        Init(void);
void        InitControlsObject(void);
OSErr       GetCWD(long *outDirID, short *outVRefNum);
void        InitOptObject(void);
void        InitManagers(void);
void        CleanTemp(void);
pascal void CheckIfXPI(const CInfoPBRec * const, Boolean *, void *);
void        MakeMenus(void);
void        MainEventLoop(void);
void        MainEventLoopPass();
int         BreathFunc();
void        ErrorHandler(short, Str255);
Boolean LookupErrorMsg( short code, Str255 msg );
void        Shutdown(void);

/*-----------------------------------------------------------*
 *   Parser 
 *-----------------------------------------------------------*/
void        ParseConfig(void);
void        ParseInstall(void);
Boolean     ReadINIFile(int, char **);
OSErr       PopulateInstallKeys(char *);
Boolean     GetResourcedString(Str255, int, int);
OSErr       PopulateGeneralKeys(char *);
OSErr       PopulateLicWinKeys(char *);
OSErr       PopulateWelcWinKeys(char *);
OSErr       PopulateSetupTypeWinKeys(char *);
OSErr       PopulateCompWinKeys(char *);
OSErr       PopulateTermWinKeys(char *);
OSErr       PopulateIDIKeys(char *);
OSErr       PopulateMiscKeys(char *);
OSErr       MapDependees(void);
Boolean     RandomSelect(long);
short       GetComponentIndex(Handle);
Boolean     FillKeyValueForIDIKey(short, Handle, char *);
Boolean     FillKeyValueUsingResID(short, short, Handle, char *);
Boolean     FillKeyValueUsingSLID(short, short, short, Handle, char *);
Boolean     FillKeyValueSecNameKeyID(short, char *, short, Handle, char *);
Boolean     FillKeyValueUsingName(char *, char *, Handle, char *);
Boolean     FindKeyValue(const char *, const char *, const char *, char *);
Boolean     GetNextSection(char **, char *, char *);
Boolean     GetNextKeyVal(char **, char *, char*);
unsigned char *CToPascal(char *);
char *      PascalToC(unsigned char *);
void        CopyPascalStrToC(ConstStr255Param srcPString, char* dest);
OSErr ReadSystemErrors(const char *instText);


/*-----------------------------------------------------------*
 *   EvtHandlers
 *-----------------------------------------------------------*/
void        HandleNextEvent(EventRecord *);
void        HandleMouseDown(EventRecord *);
void        HandleKeyDown(EventRecord *);
void        HandleMenuChoice(SInt32);
void        HandleUpdateEvt(EventRecord *);
void        HandleActivateEvt(EventRecord *);
void        HandleOSEvt(EventRecord *);
void        React2InContent(EventRecord *, WindowPtr);
Boolean     DidUserCancel(EventRecord *);

/*-----------------------------------------------------------*
 *   LicenseWin
 *-----------------------------------------------------------*/
void        ShowLicenseWin(void);
void        InLicenseContent(EventRecord*, WindowPtr);
void        EnableLicenseWin(void);
void        DisableLicenseWin(void);
void        InitLicTxt(void);
void        ShowTxt(void);
void        ShowLogo(Boolean);
void        InitScrollBar(ControlHandle);
pascal void DoScrollProc(ControlHandle, short);
void        CalcChange(ControlHandle, short *);
void        ShowNavButtons(unsigned char*, unsigned char*);
void        EnableNavButtons(void);
void        DisableNavButtons(void);

/*-----------------------------------------------------------*
 *   WelcomeWin
 *-----------------------------------------------------------*/
void        ShowWelcomeWin(void);
void        ShowWelcomeMsg(void);
void        InWelcomeContent(EventRecord*, WindowPtr);
void        ShowCancelButton(void);
void        ShowReadmeButton(void);
void        ShowReadme(void);
void        EnableWelcomeWin(void);
void        DisableWelcomeWin(void);
OSErr       LaunchAppOpeningDoc (Boolean, FSSpec *, ProcessSerialNumber *, 
            FSSpec *, unsigned short, unsigned short);
OSErr       FindAppUsingSig (OSType, FSSpec *, Boolean *, ProcessSerialNumber *);
OSErr       FindRunningAppBySignature (OSType, FSSpec *, ProcessSerialNumber *);
static OSErr VolHasDesktopDB (short, Boolean *);
static OSErr FindAppOnVolume (OSType, short, FSSpec *);
OSErr       GetSysVolume (short *);
OSErr       GetIndVolume (short, short *);
OSErr       GetLastModDateTime(const FSSpec *, unsigned long *);
void        InitNewMenu(void);

/*-----------------------------------------------------------*
 *   SetupTypeWin
 *-----------------------------------------------------------*/
void        ShowSetupTypeWin(void);
void        ShowSetupDescTxt(void);
void        GetAllVInfo(unsigned char **, short *);
pascal void OurNavEventFunction(NavEventCallbackMessage callBackSelector, 
            NavCBRecPtr callBackParms, NavCallBackUserData callBackUD);
void        InsertCompList(int instChoice);
void        DrawDiskNFolder(short, unsigned char *);
void        DrawDiskSpaceMsgs(short);
char*       DiskSpaceNeeded(void);
void        ClearDiskSpaceMsgs(void);
char*       ltoa(long);
short       pstrcmp(unsigned char*, unsigned char*);
unsigned char* pstrcpy(unsigned char*, unsigned char*);
unsigned char* pstrcat(unsigned char*, unsigned char*);
void        InSetupTypeContent(EventRecord *, WindowPtr);
Boolean     LegacyFileCheck(short, long);
int         CompareVersion(Handle, FSSpecPtr);
Boolean     VerifyDiskSpace(void);
void        EnableSetupTypeWin(void);
void        DisableSetupTypeWin(void);
void        strtran(char*, const char*, const char*); 

/*-----------------------------------------------------------*
 *   ComponentsWin
 *-----------------------------------------------------------*/
void        ShowComponentsWin(void);
Boolean     PopulateCompInfo(void);
void        UpdateCompWin(void);
void        InComponentsContent(EventRecord*, WindowPtr);
short       GetCompRow(int);
void        SetOptInfo(Boolean);
void        InitRowHighlight(int);
void        UpdateRowHighlight(Point);
void        UpdateLongDesc(int);
void        ResolveDependees(int, int);
void        UpdateRefCount(int, int);
void        EnableComponentsWin(void);
void        DisableComponentsWin(void);

/*-----------------------------------------------------------*
 *   AdditionsWin
 *-----------------------------------------------------------*/
void        ShowAdditionsWin(void); 
Boolean     AddPopulateCompInfo(void);
void        InAdditionsContent(EventRecord*, WindowPtr);
void        UpdateAdditionsWin(void);
short       AddGetCompRow(int);
void        AddSetOptInfo(Boolean);
void        AddInitRowHighlight(int);
void        AddUpdateRowHighlight(Point);
void        AddUpdateLongDesc(int);
void        EnableAdditionsWin(void);
void        DisableAdditionsWin(void);
 
/*-----------------------------------------------------------*
 *   TerminalWin
 *-----------------------------------------------------------*/
void        ShowTerminalWin(void);
short       GetRectFromRes(Rect *, short);
void        BeginInstall(void);
void        my_c2pstrcpy(const char *, Str255);
void        InTerminalContent(EventRecord*, WindowPtr);
void        UpdateTerminalWin(void);
void        OpenProxySettings(void);
Boolean     SpawnSDThread(ThreadEntryProcPtr, ThreadID *);
void        ClearDownloadSettings(void);
void        ClearSaveBitsMsg(void);
void        EnableTerminalWin(void);
void        DisableTerminalWin(void);
void        SetupPauseResumeButtons(void);
void        SetPausedState(void);
void        SetResumedState(void);
void        DisablePauseAndResume();


/*-----------------------------------------------------------*
 *   InstAction
 *-----------------------------------------------------------*/
 
 
#define TYPE_UNDEF 0
#define TYPE_PROXY 1
#define TYPE_HTTP 2
#define TYPE_FTP 3

typedef struct _conn
{
    unsigned char type; // TYPE_UNDEF, etc.
    char *URL;          // URL this connection is for
    void *conn;         // pointer to open connection
} CONN;


pascal void *Install(void*);
long        ComputeTotalDLSize(void);
short       DownloadXPIs(short, long);
short       DownloadFile(Handle, long, Handle, int, int, CONN *);
OSErr       DLMarkerSetCurrent(char *);
OSErr       DLMarkerGetCurrent(int *, int*);
OSErr       DLMarkerDelete(void);
int         GetResPos(InstComp *);
OSErr       GetInstallerModules(short *, long *);
OSErr       GetIndexFromName(char *, int *, int *);
int         ParseFTPURL(char *, char **, char **);
void        CompressToFit(char *, char *, int);
float       ComputeRate(int, time_t, time_t);
int         DLProgressCB(int, int);
void        IfRemoveOldCore(short, long);
Boolean     GenerateIDIFromOpt(Str255, long, short, FSSpec *);
void        AddKeyToIDI(short, Handle, char *);
Boolean     ExistArchives(short, long);
Boolean     CheckConn(char *URL, int type, CONN *myConn, Boolean force);
OSErr       ExistsXPI(int);
void        LaunchApps(short, long);
void        RunApps(void);
void        DeleteXPIs(short, long);
void        InitDLProgControls(Boolean onlyLabels);
void        ClearDLProgControls(Boolean onlyLabels);
void        InitProgressBar(void);
Boolean     CRCCheckDownloadedArchives(Handle dlPath, short dlPathLen, int count);
Boolean     IsArchiveFile( char *path );


/*-----------------------------------------------------------*
 *   Inflation
 *-----------------------------------------------------------*/
OSErr       ExtractCoreFile(short, long, short, long);
void        WhackDirectories(char *);
OSErr       InflateFiles(void*, void*, short, long);
OSErr       AppleSingleDecode(FSSpecPtr, FSSpecPtr);
void        ResolveDirs(char *, char*);
OSErr       DirCreateRecursive(char *);
OSErr       ForceMoveFile(short, long, ConstStr255Param, long);
OSErr       CleanupExtractedFiles(short, long);

/*-----------------------------------------------------------*
 *   XPInstallGlue
 *-----------------------------------------------------------*/
OSErr       RunAllXPIs(short xpiVRefNum, long xpiDirID, short vRefNum, long dirID);
/* NB:
** See XPInstallGlue.c for rest of prototypes
*/

/*-----------------------------------------------------------*
 *   GreyButton
 *-----------------------------------------------------------*/
pascal void     FrameGreyButton( Rect *buttonFrame );
void            SetFrameGreyButtonColor( short color );

#ifdef __cplusplus
}
#endif

#define kNumDLFields 4

#endif /* _MIW_H_ */
