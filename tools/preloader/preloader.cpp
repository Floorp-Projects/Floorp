/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/****************************************************************************

  Module Notes:
  =============

  Overview:
    This little win32 application knows how to load the modules for
    intalled version netscape and mozilla browsers. It preloads these
    modules in order to make overall startup time seem faster to our
    end users. 

  Notes:
    We assume that this preloader is stored with the mozilla/netscape
    install somewhere, and that a link to that file is stored in either 
    the CURRENT_USER startup folder or the ALL_USERS startup folder. 
    If it's get's put somewhere else (like DEFAULT_USERS) then the 
    code we use to remove ourselves from the startup folder needs to 
    be adjusted accordingly.


  Who       When      What Changed
  ==================================================================
  rickg   03.15.01    Version 1.0 of the preloader; proof of concept. 

  rickg   04.16.01    changed code to use system-tray API's directly
  rickg   04.17.01    added menu code to system tray to allow user to disable us
  rickg   04.18.01    added code to auto-remove the preloader from startup folder
  rickg   04.18.01    switched strings to resource files for easier localization.
  rickg   04.23.01    added code to prevent multiple instances
  rickg   04.23.01    added code to prevent preloader operation if browser is already running
  rickg   04.23.01    added code to display unique icon (ugly yellow) if browser is already running.
  rickg   04.23.01    added code to get/set "tuning" config settings from config dialog 
  rickg   04.24.01    added code to get/set "tuning" settings from registry
  rickg   04.24.01    added accelerators to menu, and changed tooltip    
  rickg   04.24.01    moved more strings to resource file
  rickg   04.24.01    hooked up "tuning" config settings for gModulePercent and gFrequencyPercent
  rickg   04.27.01    hooked up "tuning" config settings for gEntryPercent
  rickg   04.27.01    added a new config setting that specifies which browser instance to preload

 ****************************************************************************/


//additional includes
#include "resrc1.h"
#include <atlbase.h>
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

//trayicon notification message
#define WM_TRAY   WM_USER+0x100+5

static  NOTIFYICONDATA gTrayData = {0,0,0,0,0,0,0};
static  HWND gMainWindow=0;
static  HICON gCheckIcons[2] = {0,0};
static  HICON gBrowserRunningIcon = {0};

static  const int kLoadTimerID = 2;
static  const int kPingModuleTimerID = 3;

static  char  gExePath[1024]={0};   //used to keep a path to our executable...
static  const char* thePathSep="\\";
static  const char* gBrowserWindowName=0;
static  const char *gRegKey=0;
static  const char *gRegSubKey=0;

static  HINSTANCE gMainInst=0;      //application main instance handle
static  int gModulePercent = 100;   //This tells us the percent of modules to load (default value)
static  int gEntryPercent  = 50;    //Tells us the % of entry points per module (default value)
static  int gFrequencyPercent = 50; //Tells us relative frequency to call entry points (default value)

/***********************************************************
  call this to change the icon shown for our tray app 
 ***********************************************************/
void SetTrayIcon(const HICON hIcon){
  gTrayData.hIcon=hIcon;
  Shell_NotifyIcon(NIM_MODIFY, &gTrayData);
}

/***********************************************************
  call this to change the tooltip shown for our tray app
 ***********************************************************/
void SetTrayToolTip(const char *aTip){
  if(aTip) {
    if(!gTrayData.szTip[0])  //if previously no tooltip
      gTrayData.uFlags=gTrayData.uFlags | NIF_TIP;
    strcpy(gTrayData.szTip, aTip);
  }
  else gTrayData.uFlags=NIF_ICON | NIF_MESSAGE;
  Shell_NotifyIcon(NIM_MODIFY, &gTrayData);
}

/***********************************************************
  call this to init and display the tray app.
 ***********************************************************/
void ShowTrayApp(bool aVisible) {

  if(aVisible) {
    gTrayData.cbSize=sizeof(NOTIFYICONDATA);
    gTrayData.hWnd=gMainWindow;
    gTrayData.uID=IDI_CHECK;  //our tray ID
    gTrayData.uFlags= NIF_ICON | NIF_MESSAGE | NIF_TIP;
    gTrayData.uCallbackMessage=WM_TRAY; //send a WM_TRAY message when users clicks in our tray window
    gTrayData.hIcon=gCheckIcons[0]; //init our default icon
  
    Shell_NotifyIcon(NIM_ADD, &gTrayData); //now show our tray icon

    char theTip[256];
    if(LoadString(gMainInst,IDS_TOOLTIP,theTip,sizeof(theTip))){
      SetTrayToolTip(theTip);  
    }
    else SetTrayToolTip("Click to configure moz-preloader");
    SetTrayIcon(gCheckIcons[0]);
  }
  else {
    Shell_NotifyIcon(NIM_DELETE, &gTrayData);
  }
}

//********************************************************************


static bool  gUseFullModuleList = false;


//this enum distinguishes version of netscape (and mozilla).
enum eAppVersion {eNetscape65, eNetscape60, eMozilla, eNetscapePre60, eUserPath,eUnknownVersion,eAutoDetect};

static eAppVersion  gAppVersion=eNetscape65;

//Constants for my DLL loader to use...
static char gMozPath[2048]= {0};
static char gUserPath[2048]= {0};
static int  gPreloadJava = 1;

static char gMozModuleList[4096] = {0};
static char* gModuleCP = gMozModuleList;
static int  gModuleCount=0;
static int gMozPathLen=0;
static int gUserPathLen=0;

static const char *theMozPath=0;
static int  gDLLIndex=0;


/*********************************************************
  This counts the number of unique modules in the given
  list of modules, by counting semicolons.
 *********************************************************/
int CountModules(char*&aModuleList) {
  char *cp=aModuleList;
  
  int count=0;

  while(*cp) {
    if(*cp) {
      count++;
    }
    char *theSemi=strchr(cp,';');
    if(theSemi) {
      cp=theSemi+1; //skip the semi
    }
  }
  return count;
}


/*********************************************************
  This list describes the set of modules for NS6.0
  XXX This data should really live in a string table too.
 *********************************************************/
int Get60ModuleList(char*&aModuleList) {
  
  static char* theModuleList = 

    "nspr4;plds4;plc4;mozreg;xpcom;img3250;zlib;" \

    "gkgfxwin;gkwidget;" \

    "components\\gkparser;" \
    "jpeg3250;" \
    "js3250;" \
    "jsj3250;" \
    "jsdom;" \
    "components\\jsloader;" \

    "components\\activation;" \
    "components\\appcomps;" \
    "components\\addrbook;" \
    "components\\appshell;" \
    "components\\caps;" \
    "components\\chardet;" \
    "components\\chrome;" \
    "components\\cookie;" \
    "components\\docshell;" \
    "components\\editor;" \
    "components\\gkhtml;" \
    "components\\gkplugin;" \
    "components\\gkview;" \
    "gkwidget;" \
    "components\\jar50;" \
    "components\\lwbrk;" \
    "mozreg;" \
    "components\\necko;" \
    "components\\nsgif;" \
    "components\\nslocale;" \
    "components\\nsprefm;" \
    "components\\profile;" \
    "components\\psmglue;" \
    "components\\rdf;" \
    "components\\shistory;" \
    "components\\strres;" \
    "components\\txmgr;" \
    "components\\txtsvc;" \
    "components\\ucharuti;" \
    "components\\uconv;" \
    "components\\ucvlatin;" \
    "components\\ucvcn;" \
    "components\\ucvja;" \
    "components\\urildr;" \
    "components\\wallet;" \
    "components\\xpc3250;" \
    "components\\xpinstal;" \
    "components\\xppref32;" \
    "components\\mozbrwsr;" \
    "components\\nsjpg;" \
    "components\\oji;" \

    "msgbsutl;" \
    "components\\mork;" \
    "components\\msglocal;" \

    "xprt;" \
    "xptl;" \
    "xpcs;";

  strcpy(aModuleList,theModuleList);
  return CountModules(theModuleList);
}

/*********************************************************
  This list describes the set of modules for NS6.5
  XXX This data should really live in a string table too.
 *********************************************************/
int Get65ModuleList(char *&aModuleList) {
  static char* theModuleList = 

  "nspr4;plds4;plc4;mozreg;xpcom;img3250;zlib;" \

  "gkgfxwin;gkwidget;" \

  "components\\gkparser;" \
  "jpeg3250;" \
  "js3250;" \
  "jsj3250;" \
  "jsdom;" \
  "components\\jsloader;" \

  "components\\activation;" \
  "components\\addrbook;" \
  "components\\appcomps;" \
  "components\\appshell;" \
  "components\\embedcomponents;"
  "components\\caps;" \
  "components\\chardet;" \
  "components\\chrome;" \
  "components\\cookie;" \
  "components\\docshell;" \
  "components\\editor;" \
  "components\\gkplugin;" \
  "components\\gkview;" \
  "gkwidget;" \
  "gfx2;" \
  "components\\jar50;" \
  "components\\lwbrk;" \
  "components\\necko;" \
  "components\\nsgif;" \
  "components\\nslocale;" \
  "components\\nsprefm;" \
  "components\\profile;" \
  "components\\psmglue;" \
  "components\\rdf;" \
  "components\\shistory;" \
  "components\\strres;" \
  "components\\txmgr;" \
  "components\\txtsvc;" \
  "components\\ucharuti;" \
  "components\\uconv;" \
  "components\\ucvlatin;" \
  "components\\ucvcn;" \
  "components\\ucvja;" \
  "components\\urildr;" \
  "components\\wallet;" \
  "components\\xpc3250;" \
  "components\\xpinstal;" \
  "components\\xppref32;" \
  "components\\xmlextras;" \

  "components\\gklayout;" \
  "components\\gkcontent;" \
  "components\\mozbrwsr;" \
  "components\\nsjpg;" \
  "components\\oji;" \

  "msgbsutl;" \
  "components\\mork;" \
  "components\\msglocal;" \

  "xprt;" \
  "xptl;" \
  "xpcs;";

  strcpy(aModuleList,theModuleList);
  return CountModules(theModuleList);
}


/*********************************************************
  ...
 *********************************************************/
static char gKeyBuffer[512]={0};
static char gSubKeyBuffer[128]={0};
static char gWinNameBuffer[128]={0};     //this is the expected browser name.

void GetPathFromRegistry(eAppVersion aVersion, const char *&aKey, const char *&aSubkey, char *&aModuleList, int &aSize) {
  
  switch(aVersion) { 
  
    case eMozilla:

      aKey=(LoadString(gMainInst,IDS_MOZ_KEY,gKeyBuffer,sizeof(gKeyBuffer))) ? gKeyBuffer : "Software\\Mozilla.org\\Mozilla\\0.8 (en)\\Main"; 

      aSubkey = (LoadString(gMainInst,IDS_SUBKEY_INSTALL,gSubKeyBuffer,sizeof(gSubKeyBuffer))) ? gSubKeyBuffer : "Install Directory";

      gBrowserWindowName = (LoadString(gMainInst,IDS_MOZ_WINDOWNAME,gWinNameBuffer,sizeof(gWinNameBuffer))) ? gWinNameBuffer: "- Mozilla";

      aSize=Get65ModuleList(aModuleList);

      break;
    
    case eNetscape65:

      aKey=(LoadString(gMainInst,IDS_NS65_KEY,gKeyBuffer,sizeof(gKeyBuffer))) ? gKeyBuffer : "Software\\Netscape\\Netscape 6\\6.5 (en)\\Main"; 

      aSubkey = (LoadString(gMainInst,IDS_SUBKEY_INSTALL,gSubKeyBuffer,sizeof(gSubKeyBuffer))) ? gSubKeyBuffer : "Install Directory";

      gBrowserWindowName = (LoadString(gMainInst,IDS_NS_WINDOWNAME,gWinNameBuffer,sizeof(gWinNameBuffer))) ? gWinNameBuffer: "- Netstacpe 6";

      aSize=Get65ModuleList(aModuleList);

      break;
    
    case eNetscape60:

      aKey=(LoadString(gMainInst,IDS_NS60_KEY,gKeyBuffer,sizeof(gKeyBuffer))) ? gKeyBuffer : "Software\\Netscape\\Netscape 6\\6.0 (en)\\Main"; 

      aSubkey = (LoadString(gMainInst,IDS_SUBKEY_PATH,gSubKeyBuffer,sizeof(gSubKeyBuffer))) ? gSubKeyBuffer : "Path";

      gBrowserWindowName = (LoadString(gMainInst,IDS_NS_WINDOWNAME,gWinNameBuffer,sizeof(gWinNameBuffer))) ? gWinNameBuffer: "- Netstacpe 6";

      aSize=Get60ModuleList(aModuleList);

      break;

    case eNetscapePre60:

      aKey=(LoadString(gMainInst,IDS_PRE60_KEY,gKeyBuffer,sizeof(gKeyBuffer))) ? gKeyBuffer : "Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\netscp6.exe"; 

      aSubkey = (LoadString(gMainInst,IDS_SUBKEY_PATH,gSubKeyBuffer,sizeof(gSubKeyBuffer))) ? gSubKeyBuffer : "Path";

      gBrowserWindowName = (LoadString(gMainInst,IDS_NS_WINDOWNAME,gWinNameBuffer,sizeof(gWinNameBuffer))) ? gWinNameBuffer: "- Netstacpe 6";

      aSize=Get60ModuleList(aModuleList);

      break;

    case eUserPath:
      aKey  = 0;
      aSubkey= 0;
      strcpy(gMozPath,gUserPath);

      aSize=Get65ModuleList(aModuleList);

      gBrowserWindowName = (LoadString(gMainInst,IDS_MOZ_WINDOWNAME,gWinNameBuffer,sizeof(gWinNameBuffer))) ? gWinNameBuffer: "- Mozilla";

      break;

    case eUnknownVersion:
      break;
  }
}

/*********************************************************
  Get the path to the netscape6 browser via the registry...
 *********************************************************/
bool GetMozillaRegistryInfo(eAppVersion &aVersion) {

  //first we try to get the registry info based on the command line settings.
  //if that fails, we try others.

  bool found=false;
  LONG theOpenResult = 1; //any non-zero will do to initialize this...

  if ( aVersion == eUserPath) {
    found=true;
  }  

  while((ERROR_SUCCESS!=theOpenResult) && (aVersion<eUnknownVersion)) {
     
    GetPathFromRegistry(aVersion,gRegKey,gRegSubKey,gModuleCP,gModuleCount);

    CRegKey theRegKey;

    if((eUserPath!=aVersion) && (gRegKey)) {
      theOpenResult=theRegKey.Open(HKEY_LOCAL_MACHINE,gRegKey,KEY_QUERY_VALUE);

      if(ERROR_SUCCESS==theOpenResult) {
        DWORD theSize=1024;

        theRegKey.QueryValue(gMozPath,gRegSubKey,&theSize);

        if((ERROR_SUCCESS==theOpenResult) && (aVersion!=eUserPath)) {

          theSize=1024;
          char theCachedModuleList[1024] = {0};

          theRegKey.QueryValue(theCachedModuleList,"Modules",&theSize);

          if(theCachedModuleList[0]) {
            strcpy(gMozModuleList,theCachedModuleList);
          }
          else {
            theRegKey.Create(HKEY_LOCAL_MACHINE,gRegKey);
            theRegKey.SetValue( HKEY_LOCAL_MACHINE, gRegKey, gMozModuleList, "Modules");
          }
        }

        found=true;
        break;
      }
    }
    aVersion=eAppVersion(int(aVersion)+1);
    
  } //while

  gMozPathLen=strlen(gMozPath);
  gModuleCP = gMozModuleList;

  return found;
}


/*********************************************************
  Extract the "next" module name from our module name list.
  XXX The module names should come from string resources.
 *********************************************************/
void GetNextModuleName(char* aName) {
  //scan ahead to find the next ';' or the end of the string...
  bool done=false;
  char theChar=0;

  aName[0]=0;

  char *theCP=gModuleCP;

  while(*theCP) {
    theChar=*theCP;
    if(';'==theChar)
      break;
    else theCP++;
  }

  if(theCP!=gModuleCP) {
    size_t theSize=theCP-gModuleCP;
    strncpy(aName,gModuleCP,theSize);
    aName[theSize]=0;
    gModuleCP=theCP;
    while(';'==*gModuleCP)
      gModuleCP++;
  }

}

/****************************************************************
  The following types are fraudulent. They're just here so 
  we can write up calls entry points in each module.
 ****************************************************************/

const  int        gMaxDLLCount=256;
const  int        gMaxProcCount=512;
static HINSTANCE  gInstances[gMaxDLLCount];
static long*      gFuncTable[gMaxDLLCount][gMaxProcCount];
static int        gDLLCount=0;
static int        gMaxEntryIndex=0;


/****************************************************************
  This helper function is called by LoadModule. We separated this
  stuff out so we can use the same code to load java modules 
  that aren't in the mozilla directory. 
 ****************************************************************/
HINSTANCE LoadModuleFromPath(const char* aPath) {
 
  HINSTANCE theInstance=gInstances[gDLLCount++]=LoadLibrary(aPath);

    //we'll constrain the steprate to the range 1..20.
  static double kPercent=(100-gEntryPercent)/100.0;
  static const int kEntryStepRate=1+int(20*kPercent);
  int theEntryCount=0;

  if(theInstance) {
    //let's get addresses throughout the module, skipping by the rate of kEntryStepRate.
    for(int theEntryPoint=0;theEntryPoint<gMaxProcCount;theEntryPoint++){
      long *entry=(long*)::GetProcAddress(theInstance,MAKEINTRESOURCE(1+(kEntryStepRate*theEntryPoint)));
      if(entry) {
        gFuncTable[gDLLCount-1][theEntryCount++]=entry;
      }
      else {
        break;
      }
    }
    if(theEntryCount>gMaxEntryIndex)
      gMaxEntryIndex=theEntryCount; //we track the highest index we find.
  }

  return theInstance;
}


/****************************************************************
  Call this once for each module you want to preload.
  We use gEntryPercent to determine what percentage of entry 
  p oints to acquire (we will always get at least 1, and we'll
  skip at most 20 entry points at a time).
 ****************************************************************/
HINSTANCE LoadModule(const char* aName) {

  //we operate on gMozPath directly to avoid an unnecessary string copy.
  //when we're done with this method, we reset gMozpath to it's original value for reuse.

  strcat(gMozPath,aName);
  strcat(gMozPath,".dll");

  HINSTANCE theInstance = LoadModuleFromPath(gMozPath);

  gMozPath[gMozPathLen]=0;
  return theInstance;
}


BOOL CALLBACK EnumWindowsProc(HWND hwnd,LPARAM lParam ) {

  char buf[256]={0};
  if(GetWindowText(hwnd,buf,sizeof(buf))){
    if(strstr(buf,gBrowserWindowName)) {
      return FALSE; //stop iterating now...
    }
  }
  return TRUE;
}

/****************************************************************
  Call this to detect whether the browser is running.
  We cache the result to speed up this function (by preventing
  subsuquent searches of top level windows).
 ****************************************************************/
bool BrowserIsRunning() {
  static bool gBrowserIsRunning=false;
  
  if(!gBrowserIsRunning) {
    if(!EnumWindows(EnumWindowsProc,0)) {
      gBrowserIsRunning=true;
    }
  }

  return gBrowserIsRunning;
}

/****************************************************************
  This function get's called repeatedly to call on a timer,
  and it calls GetProcAddr() to keep modules from paging.

  NOTE: This method uses gFrequencyPercent to determine how much
  work to do each time it gets called.

 ****************************************************************/
VOID CALLBACK KeepAliveTimerProc(HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime  ) {

  //let's go see if mozilla is running.
  //if so, then bail out without pinging modules...
  
  if(BrowserIsRunning()) {
    return;
  }

  static bool theTimerIsRunning=false;
  static int  theCurrentModule=0;
  static int  theCurrentProc=0;

  if(!theTimerIsRunning) { //ignore other timer calls till we're done.
    theTimerIsRunning=true;

      //constrain the step to 1..gDLLCount
    static const double kPercent=gFrequencyPercent/100.0;
    static const int kMaxSteps=1+int(3*gDLLCount*kPercent);

    int count=0;

    //this version iterates the entry points in each module before moving to the next module

    while(count<kMaxSteps) {

      volatile long *p=(long*)gFuncTable[theCurrentModule][theCurrentProc];
      count++;
      if (p) {
        if (*p && p){  
        //don't actually invoke it, just cause it to load into memory...
        //note that modules have different number of entry points.
        //so not all modules have an entry point at theCurrentProc index.
          int x=10;
        }
      }

#define _DEPTH_FIRST
#ifdef _DEPTH_FIRST
      
      if(theCurrentModule >= gDLLCount) {
        theCurrentModule = 0;
        theCurrentProc = (theCurrentProc>=gMaxEntryIndex) ? 0 : theCurrentProc+1;
      }
      else {
        theCurrentModule++;
      }

#else //breadth first...

      if(theCurrentProc >= gMaxEntryIndex) {
        theCurrentProc=0;
        theCurrentModule = (theCurrentModule>=gDLLCount) ? 0 : theCurrentModule+1;
      }
      else {
        theCurrentProc++;
      }

#endif

    }

    theTimerIsRunning=false;
  }
}


/****************************************************************
  This gets called repeatedly by a windows timer, and loads
  the modules that it gets from the modulelist (see top of this file).

  This routine has been updated to account for the gModulesPercent
  setting (1-100). We'll load modules until we cross over this 
  percentage.
 ****************************************************************/
VOID CALLBACK LoadModuleTimerProc(HWND hwnd, UINT uMsg, UINT idEvent,DWORD dwTime) {

  static bool theTimerIsRunning=false;
  static int  gTrayIconIndex = 0;

  if(!theTimerIsRunning) {
    theTimerIsRunning=true;

      //gDLLCount is the number of modules loaded so far
      //gModuleCount is the total number of modules we know about
      //gModulePercent is the total % of modules we're being asked to load (config setting)

    double theMaxPercentToLoad=gModulePercent/100.0;

      //we'll only load more modules if haven't loaded the max percentage of modules
      //based on the configuration UI (stored in gModulePercent).

    const int theModulePerCallCount = 3; // specifies how many modules to load per timer callback.
    bool  done=false;

    for(int theModuleIndex=0;theModuleIndex<theModulePerCallCount;theModuleIndex++) {

      SetTrayIcon(gCheckIcons[gTrayIconIndex]);

      gTrayIconIndex = (gTrayIconIndex==1) ? 0 : gTrayIconIndex+1; //this toggles the preloader icon to show activity...

      char theDLLName[512];
      GetNextModuleName(theDLLName);

      if(theDLLName[0]) {
        if(gDLLCount/(gModuleCount*1.0)<=theMaxPercentToLoad) {
          HINSTANCE theInstance=LoadModule(theDLLName);
        }
        else done=true;
      }
      else done=true;

      if(done) {

          //if they've asked us to preload java, then do so here...
        if(gPreloadJava) {

          char kJavaRegStr[512] = {0};
          if(LoadString(gMainInst,IDS_JAVAREGSTR,kJavaRegStr,sizeof(kJavaRegStr))){

            CRegKey theRegKey;

            LONG theOpenResult=theRegKey.Open(HKEY_LOCAL_MACHINE,kJavaRegStr,KEY_QUERY_VALUE);
            if(ERROR_SUCCESS==theOpenResult) {

              char kJavaRegPath[128] = {0};
              if(LoadString(gMainInst,IDS_JAVAREGPATH,kJavaRegPath,sizeof(kJavaRegPath))){

                char thePath[1024] = {0};
                DWORD theSize=sizeof(thePath);
                theRegKey.QueryValue(thePath,kJavaRegPath,&theSize);

                if(thePath) {
                  //here we go; let's load java and awt...
                  char theTempPath[1024]={0};

                  sprintf(theTempPath,"%s\\hotspot\\jvm.dll",thePath);
                  LoadModuleFromPath(theTempPath);

                  sprintf(theTempPath,"%s\\verify.dll",thePath);
                  LoadModuleFromPath(theTempPath);

                  sprintf(theTempPath,"%s\\jpins32.dll",thePath);
                  LoadModuleFromPath(theTempPath);

                  sprintf(theTempPath,"%s\\jpishare.dll",thePath);
                  LoadModuleFromPath(theTempPath);

                  sprintf(theTempPath,"%s\\NPOJI600.dll",thePath);
                  LoadModuleFromPath(theTempPath);

                  sprintf(theTempPath,"%s\\java.dll",thePath);
                  LoadModuleFromPath(theTempPath);

                  sprintf(theTempPath,"%s\\awt.dll",thePath);
                  LoadModuleFromPath(theTempPath);

                }
              }
            }
          }

        }

        KillTimer(gMainWindow,kLoadTimerID);     
        SetTrayIcon(gCheckIcons[1]);
    
        //now make a new timer that calls GetProcAddr()...
        //we take gFrequencyPercent into account. We assume a timer range (min..max) of ~1second.

        int theFreq=50+(100-gFrequencyPercent)*10;

        UINT result=SetTimer(gMainWindow,kPingModuleTimerID,theFreq,KeepAliveTimerProc);
        return; //bail out    
      }
    }

    theTimerIsRunning=false;
  }
}
 
/****************************************
  we support the following args:
    -f(+/-) : use FULL module list (not really supported yet)
    -m : use mozilla installation
    -n : use netscape installation (this is the default)
    -p : use the given path (use this when you're running from a debug build rather than an installed version)
 ****************************************/
void ParseArgs(LPSTR &CmdLineArgs,eAppVersion &anAppVersion) {

  const char* cp=CmdLineArgs;
  
  while(*cp) {
    char theChar=*cp;
    if(('-'==theChar) || ('/'==theChar)){
      //we found a valid command, go get it...
      ++cp;
      char theCmd=*cp;

      switch(theCmd) {
        case 'n':
        case 'N':
          anAppVersion=eNetscape65;
          break;

        case 'm':
        case 'M':
          anAppVersion=eMozilla;
          break;

        case 'f':
        case 'F':
          ++cp;          
          if('+'==*cp) {
            gUseFullModuleList=true;
          }
          break;

        case 'p':
        case 'P':
          cp++; //skip over the command, then get the install path the user wants us to use.
          anAppVersion=eUserPath;
          while(*cp) {              //skip whitespace...
            if(' '!=*cp)
              break;
            cp++;
          }
            //now eat the path
          gMozPathLen=gUserPathLen=0;
          if('"'==*cp) { // Allow spaces in the path ie "C:\Program Files"
            cp++; // skip the "
            while(*cp) {
              if('"'!=*cp) {
                gUserPath[gUserPathLen++]=*cp;
              }
              cp++;
            }
          }
          else {
            while(*cp) {
              if(' '!=*cp) {
                gUserPath[gUserPathLen++]=*cp;
              }
              cp++;
            }
          }
          if(gUserPath[gUserPathLen-1]!= '\\') // make sure there's a \ on the end. 
            gUserPath[gUserPathLen++] = '\\';
          gUserPath[gUserPathLen]=0;
          continue;
          break;
        default:
          break;
      }
    }
    cp++; 
  }

}

/*********************************************************
  This is the event loop controller for our diable 
  dialog box UI.
 *********************************************************/
BOOL CALLBACK DisableDialogProc(HWND hwnd, UINT Msg, WPARAM wParam, LPARAM lParam) {

  switch(Msg) {

    case WM_INITDIALOG:
      {
        RECT cr1,cr2;

          //The rect code below is used to put our dialog right by the taskbar.
          //It makes it's presence more obvious to the user.

        HWND theTopWnd=GetDesktopWindow();
        GetClientRect(theTopWnd,&cr1);
        GetClientRect(hwnd,&cr2);
        int w=cr2.right-cr2.left;
        int h=cr2.bottom-cr2.top;

        SetWindowPos(hwnd,HWND_TOP,cr1.right-(w+50),cr1.bottom-(h+70),0,0,SWP_NOSIZE);
        return TRUE;
      }
  
    case WM_COMMAND:
      switch(LOWORD(wParam)) {
        case IDOK:
          EndDialog(hwnd,1);
          break;
        case IDCANCEL:
          EndDialog(hwnd,0);
          return TRUE;
          break;
        default:
          break;
      }//switch
    default:
      break;
  }
  
  return FALSE;
}

/*********************************************************
  This is the event loop controller for our performance
  conifuration dialog box UI.
 *********************************************************/
BOOL CALLBACK ConfigureDialogProc(HWND hwnd, UINT Msg, WPARAM wParam, LPARAM lParam) {

  int x=0;

  switch(Msg) {

    case WM_INITDIALOG:
      {
        RECT cr1,cr2;

          //The rect code below is used to put our dialog right by the taskbar.
          //It makes it's presence more obvious to the user.

        HWND theTopWnd=GetDesktopWindow();
        GetClientRect(theTopWnd,&cr1);
        GetClientRect(hwnd,&cr2);
        int w=cr2.right-cr2.left;
        int h=cr2.bottom-cr2.top;


        /*---------------------------------------------------------------
          Add members to our list control...
         ---------------------------------------------------------------*/

        HWND theListCtrl=GetDlgItem(hwnd,IDC_PRELOAD);

        SendMessage(theListCtrl, CB_RESETCONTENT, 0, 0);
        SendMessage(theListCtrl, CB_ADDSTRING, 0, (LPARAM)"Auto"); 
        SendMessage(theListCtrl, CB_ADDSTRING, 0, (LPARAM)"Mozilla"); 
        SendMessage(theListCtrl, CB_ADDSTRING, 0, (LPARAM)"Netscape-6.0"); 
        SendMessage(theListCtrl, CB_ADDSTRING, 0, (LPARAM)"Netscape-6.5"); 
        SendMessage(theListCtrl, CB_ADDSTRING, 0, (LPARAM)"Pre-Netscape-6.0"); 
        SendMessage(theListCtrl, CB_ADDSTRING, 0, (LPARAM)"User-defined"); 
        
        int theSel=0;
        switch(gAppVersion) {
          case eNetscape65: theSel=3; break;
          case eNetscape60: theSel=2; break;
          case eMozilla:    theSel=1; break;
          case eNetscapePre60:  theSel=4; break;
          case eUserPath:   theSel=5; break;
          default:
            break;
        }
        
        SendMessage(theListCtrl, CB_SETCURSEL, (WPARAM)theSel, 0);

        /*---------------------------------------------------------------
          Set the text of our edit control...
         ---------------------------------------------------------------*/

        HWND theEditCtrl=GetDlgItem(hwnd,IDC_EDIT1);
        SendMessage(theEditCtrl, WM_CLEAR, 0, 0); 
        SendMessage(theEditCtrl, EM_REPLACESEL, 0, (LPARAM)gMozPath); 

        /*---------------------------------------------------------------
          Set the state of our JAVA checkbox...
         ---------------------------------------------------------------*/

        HWND theJavaCheckBox =GetDlgItem(hwnd,IDC_JAVA);
        SendMessage(theJavaCheckBox, BM_SETCHECK, gPreloadJava, 0); 

        /*---------------------------------------------------------------
          add code here to set the range of our sliders...
         ---------------------------------------------------------------*/

        HWND theSlider=GetDlgItem(hwnd,IDC_MODULES);
        gModulePercent = SendMessage(theSlider, TBM_SETPOS, (BOOL)TRUE, (LONG)gModulePercent); 

        theSlider=GetDlgItem(hwnd,IDC_ENTRIES);
        gEntryPercent = SendMessage(theSlider, TBM_SETPOS, (BOOL)TRUE, (LONG)gEntryPercent); 

        theSlider=GetDlgItem(hwnd,IDC_FREQUENCY);
        gFrequencyPercent = SendMessage(theSlider, TBM_SETPOS, (BOOL)TRUE, (LONG)gFrequencyPercent); 
        
        SetWindowPos(hwnd,HWND_TOP,cr1.right-(w+50),cr1.bottom-(h+70),0,0,SWP_NOSIZE);
        return TRUE;
      }

    case WM_COMMAND:
      switch(LOWORD(wParam)) {

        case IDOK:

          {
            HWND theSlider=GetDlgItem(hwnd,IDC_MODULES);
            gModulePercent = SendMessage(theSlider, TBM_GETPOS, 0, 0); 

            theSlider=GetDlgItem(hwnd,IDC_ENTRIES);
            gEntryPercent = SendMessage(theSlider, TBM_GETPOS, 0, 0); 

            theSlider=GetDlgItem(hwnd,IDC_FREQUENCY);
            gFrequencyPercent = SendMessage(theSlider, TBM_GETPOS, 0, 0); 

            theSlider=GetDlgItem(hwnd,IDC_JAVA);
            gPreloadJava = (int)SendMessage(theSlider, BM_GETCHECK, 0, 0); 

            EndDialog(hwnd,1);

            //and now, let's save the configuration settings...

            CRegKey theRegKey;

            LONG theOpenResult=theRegKey.Open(HKEY_LOCAL_MACHINE,gRegKey,KEY_QUERY_VALUE);
            if(ERROR_SUCCESS!=theOpenResult) {
              theRegKey.Create(HKEY_LOCAL_MACHINE,gRegKey);
            }

            char theSettings[128];
            sprintf(theSettings,"%i %i %i %i %s",gModulePercent,gEntryPercent,gFrequencyPercent,gPreloadJava,gUserPath);
            theRegKey.SetValue( HKEY_LOCAL_MACHINE, gRegKey, theSettings, "Settings");
          }
          
          break;

        case IDCANCEL:
          EndDialog(hwnd,0);
          return TRUE;
          break;

        default:
          break;
      }//switch
    default:
      break;
  }
  
  return FALSE;
}

/*********************************************************
  The user asked us to disable the preloader. So we'll try
  to remove ourselves from both the current user start
  menu and ALL-USERS start menu.

  This ASSUMES that the installer put us into the startup
  folder using a shortcut to our current exe name.

  So, if the exe is called "preloader.exe", then we assume
  that the installer put us into the startup folder as:

  "shortcut to preloader.exe.lnk".
 *********************************************************/
void RemoveFromStartupFolder() {
  
  //Using the name of our running application, let's derive the
  //name of the link to it in the startup folder.

  char theAppName[128]={0};
  char theFilename[128]={0};
  char theFullPath[1024]={0};

  const char *theLastSep=strrchr(gExePath,thePathSep[0]);

  if(theLastSep) {
    theLastSep++; //skip the slash

    char prefix[512]={0};
    
    if(LoadString(gMainInst,IDS_STARTUP_FOLDER_PREFIX,prefix,sizeof(prefix))){
      
      char suffix[512]={0};
    
      if(LoadString(gMainInst,IDS_STARTUP_FOLDER_SUFFIX,suffix,sizeof(suffix))){
        strcpy(theFilename,prefix);
        strcat(theFilename,theLastSep);
        strcat(theFilename,suffix);
      }
    
    }
  }

  if(theFilename) { //dont try if you can't get a reasonable guess to the path in the startup folder.
  
    //-----------------------------------------------------------------------------
    //first, let's try to remove ourselves from the current users's startup folder...
    //-----------------------------------------------------------------------------

    char theUserKey[256]={0};
    if(LoadString(gMainInst,IDS_STARTUP_FOLDER_KEY,theUserKey,sizeof(theUserKey))) {

      CRegKey theRegKey;

      LONG theOpenResult=theRegKey.Open(HKEY_CURRENT_USER,theUserKey,KEY_QUERY_VALUE);

      if(ERROR_SUCCESS==theOpenResult) {
    
        DWORD theSize=sizeof(theFullPath);

        char theSubKey[128]={0};
        
        if(LoadString(gMainInst,IDS_STARTUP_FOLDER_SUBKEY1,theSubKey,sizeof(theSubKey))) {
         
          theRegKey.QueryValue(theFullPath,theSubKey,&theSize);
          if(theFullPath) {

            //Now let's construct a complete path to our preloader.exe.lnk.

            strcat(theFullPath,thePathSep);
            strcat(theFullPath,theFilename);

            //Ask the OS to remove us from the startup folder...

            DeleteFile(theFullPath);
          }
        }
      }
    }

    //-----------------------------------------------------------------------------
    //Next, let's try to remove ourselves from ALL_USERS startup folder...
    //-----------------------------------------------------------------------------

    CRegKey theRegKey2;

    LONG theOpenResult=theRegKey2.Open(HKEY_LOCAL_MACHINE,theUserKey,KEY_QUERY_VALUE);

    if(ERROR_SUCCESS==theOpenResult) {
  
      DWORD theSize=sizeof(theFullPath);

      char theSubKey[128]={0};
      
      if(LoadString(gMainInst,IDS_STARTUP_FOLDER_SUBKEY2,theSubKey,sizeof(theSubKey))) {
       
        theRegKey2.QueryValue(theFullPath,theSubKey,&theSize);
        if(theFullPath) {

          //Now let's construct a complete path to our preloader.exe.lnk.

          strcat(theFullPath,thePathSep);
          strcat(theFullPath,theFilename);

          //Ask the OS to remove us from the startup folder...

          DeleteFile(theFullPath);
        }
      }
    }

  }
}


/*********************************************************
  
 *********************************************************/
LRESULT CALLBACK WindowFunc(HWND hwnd, UINT Msg, WPARAM wParam, LPARAM lParam) {

  static HMENU hPopup=0;           //menu handle
  static int   gState=0;
  static bool  gConfigDialogShowing=false;

  switch(Msg) {

    case WM_CREATE: 
      //initialize icons

      gCheckIcons[0]=LoadIcon(gMainInst, MAKEINTRESOURCE(IDI_CHECK));
      gCheckIcons[1]=LoadIcon(gMainInst, MAKEINTRESOURCE(IDI_CHECK1));
      gBrowserRunningIcon = LoadIcon(gMainInst, MAKEINTRESOURCE(IDI_BROWSERRUNNING));

      //initialize popup menu
      hPopup=LoadMenu(gMainInst, MAKEINTRESOURCE(IDR_MENU1));

      gMainWindow=hwnd;
      ShowTrayApp(true); //now display our app in the system tray

      break;

    //notification from trayicon received
    case WM_TRAY:

      //if right mouse button pressed
      if(lParam == WM_RBUTTONDOWN && (!gConfigDialogShowing)) {
        POINT pt;
        GetCursorPos(&pt);  //get cursor position
        SetForegroundWindow(hwnd);  //set window to foreground
        //display popup menu
        BOOL bRes=TrackPopupMenu(GetSubMenu(hPopup, 0), TPM_BOTTOMALIGN | TPM_RIGHTALIGN | TPM_RIGHTBUTTON,
                                 pt.x, pt.y, 0, hwnd, 0);
        //send dummy message to window
        SendMessage(hwnd, WM_NULL, 0, 0);
      }
      break;

    //notification from menu received
    case WM_COMMAND:

      switch(LOWORD(wParam)) {

        case IDM_CONFIGURE:
          {
            if(!gConfigDialogShowing) {
              gConfigDialogShowing=true;
              if(DialogBox(gMainInst,MAKEINTRESOURCE(IDD_PERFORMANCE),hwnd,ConfigureDialogProc)) {
              }
              gConfigDialogShowing=false;
            }
          }
          break;

        case IDM_REMOVE:
          //show them the disable preloader dialog. If they say yes, remove ourselves from
          //the start menu (if that's where we were launched from) and quit.
          //now fall through on purpose...
          if(DialogBox(gMainInst,MAKEINTRESOURCE(DISABLE),hwnd,DisableDialogProc)) {
            //a non-zero result means we are supposed to kill the preloader 
            //and remove it from the startup folder.

            RemoveFromStartupFolder();
            //now fall thru...

          }
          else break;
                
        case IDM_EXIT:
          DestroyWindow(hwnd);
          break;

        //change icon in system tray; why bother?
        case IDM_CHANGE:
          if(!gState) {
            gState=1;
          } 
          else {
            gState=0;
          }
          break;
      }
      break;

    //exit application
    case WM_DESTROY:
      //release used resources

      ShowTrayApp(false);

      DestroyIcon(gCheckIcons[0]);
      DestroyIcon(gCheckIcons[1]);

      DestroyMenu(hPopup);
      PostQuitMessage(0);
      break;

    default:
      return DefWindowProc(hwnd, Msg, wParam, lParam);
  }
  return 0;
}

 
/*********************************************************
  Main 
 *********************************************************/
int WINAPI WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpszArgs,int nWinMode){

  memset(gFuncTable,0,sizeof(gFuncTable)); //init this to empty ptr's for good housekeeping...
   
  ::CreateMutex(NULL, FALSE, "YourMutexName");
  if (GetLastError()  == ERROR_ALREADY_EXISTS ) {
    //Bail out if we already have one instance running.
    exit(0);
  }

  WNDCLASS WinClass;

  static char theClassName[128];
  if(!LoadString(hInstance,IDS_CLASSNAME,theClassName,sizeof(theClassName)))
    strcpy(theClassName,"MozPreloader"); //as a backup; just being paranoid.

  //register window class
  WinClass.hInstance=hInstance;

  //---------------------------------------------------------------
  //  We hang on to the app name in case they disable us.
  //  We'll remove a .lnk entry of this name from the startup folder.
  //---------------------------------------------------------------

  GetModuleFileName(hInstance,gExePath,sizeof(gExePath)); //remember where we were run from in case we get disabled. 


  //---------------------------------------------------------------
  //  Now let's init and register our window class...
  //---------------------------------------------------------------

  gMainInst=hInstance;
  WinClass.lpszClassName=theClassName;  //specify class name
  WinClass.lpfnWndProc=WindowFunc;    //specify callback function
  WinClass.style=0;
  WinClass.hIcon=LoadIcon(NULL, IDI_APPLICATION); //specify icon
  WinClass.hCursor=LoadCursor(NULL, IDC_ARROW);   //specify cursor
  WinClass.lpszMenuName=0;
  WinClass.cbClsExtra=0;
  WinClass.cbWndExtra=0;
  WinClass.hbrBackground=(HBRUSH)GetStockObject(WHITE_BRUSH); //specify window background

  if(!RegisterClass(&WinClass))
    return 0;

  //create window
  HWND hwnd=CreateWindow(theClassName,0,WS_OVERLAPPEDWINDOW,10,10,10,10,HWND_DESKTOP,NULL,hInstance,NULL);
  gMainWindow=hwnd;

  //hide window
  ShowWindow(hwnd, SW_HIDE);

  //---------------------------------------------------------------
  //  Now let's grab the module settings info...
  //---------------------------------------------------------------

  bool useDebugModuleList=false;
  
  ParseArgs(lpszArgs,gAppVersion);

  if(GetMozillaRegistryInfo(gAppVersion)) {

    SetTrayIcon(gBrowserRunningIcon); //by default.

    //---------------------------------------------------------------
    //  Now let's grab the config settings (if they happen to exist)
    //---------------------------------------------------------------

    CRegKey theRegKey;

    LONG theOpenResult=theRegKey.Open(HKEY_LOCAL_MACHINE,gRegKey,KEY_QUERY_VALUE);
    if(ERROR_SUCCESS==theOpenResult) {

      const int theSize=1024;
      DWORD size=theSize;
      char theSettings[theSize] = {0};

      theOpenResult=theRegKey.QueryValue(theSettings,"Settings",&size);
      if(ERROR_SUCCESS==theOpenResult) {
        //now let's decode the settings from the string
        char *cp=theSettings;
        
        gModulePercent = atoi(cp);
        cp=strchr(cp,' '); //skip ahead to the next space...
        cp++; //and step over it

        gEntryPercent = atoi(cp);

        cp=strchr(cp,' '); //skip ahead to the next space...
        if(cp) {
          cp++; //and step over it
          gFrequencyPercent = atoi(cp);
        }

        cp=strchr(cp,' '); //skip ahead to the next space...
        if(cp) {
          cp++; //and step over it
          gPreloadJava = atoi(cp);
        }


        if(!gUserPath[0]) { //dont override something they provide at the 
          if(cp){
            cp=strchr(cp,'['); //seek to the seperator for the pathname (optional)
            if(cp && *cp) {
              char *endcp=strchr(cp,']'); //now grab the (optional) user defined path...
              if(endcp) {
                strcpy(gUserPath,cp);
              }
            }
          }
        }
      }
    }

    //---------------------------------------------------------------
    //  And start the timer...
    //---------------------------------------------------------------

    if(!BrowserIsRunning()) {
      SetTimer(hwnd,kLoadTimerID,50,LoadModuleTimerProc); //don't bother with the preloader if the browser is already up...
    }

  }

  //start message pump
  MSG WinMsg;
  while(GetMessage(&WinMsg, NULL, 0, 0)) {
    TranslateMessage(&WinMsg);
    DispatchMessage(&WinMsg);
  }

  return WinMsg.wParam;
}

