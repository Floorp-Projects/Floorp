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

#include "nsToolkit.h"
#include "nsWindow.h"
#include "prmon.h"
#include "prtime.h"
#include "nsGUIEvent.h"
#include "plevent.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsIEventQueue.h"
#ifdef MOZ_AIMM
// objbase.h must be declared before initguid.h to use the |DEFINE_GUID|'s in aimm.h
#include <objbase.h>
#include <initguid.h>
#include "aimm.h"
#endif

// unknwn.h is needed to build with WIN32_LEAN_AND_MEAN
#include <unknwn.h>

static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

// Cached reference to event queue service
static nsCOMPtr<nsIEventQueueService>  gEventQueueService;

NS_IMPL_ISUPPORTS1(nsToolkit, nsIToolkit)

// If PR_TRUE the user is currently moving a top level window.
static PRBool gIsMovingWindow = PR_FALSE;

// Message filter used to determine if the user is currently 
// moving a top-level window.
static HHOOK   nsMsgFilterHook = NULL;

//
// Static thread local storage index of the Toolkit 
// object associated with a given thread...
//
static PRUintn gToolkitTLSIndex = 0;


HINSTANCE nsToolkit::mDllInstance = 0;
PRBool    nsToolkit::mIsNT = PR_FALSE;
PRBool    nsToolkit::mUseImeApiW  = PR_FALSE;
PRBool    nsToolkit::mW2KXP_CP936 = PR_FALSE;
PRBool    nsToolkit::mIsWinXP     = PR_FALSE;

#ifdef MOZ_AIMM
IActiveIMMApp* nsToolkit::gAIMMApp   = NULL;
PRInt32        nsToolkit::gAIMMCount = 0;
#endif

nsWindow     *MouseTrailer::mCaptureWindow  = NULL;
nsWindow     *MouseTrailer::mHoldMouse      = NULL;
MouseTrailer *MouseTrailer::theMouseTrailer = NULL;
PRBool        MouseTrailer::gIgnoreNextCycle(PR_FALSE);
PRBool        MouseTrailer::mIsInCaptureMode(PR_FALSE);

#ifndef MOZ_STATIC_COMPONENT_LIBS
//
// Dll entry point. Keep the dll instance
//
BOOL APIENTRY DllMain(  HINSTANCE hModule, 
                        DWORD reason, 
                        LPVOID lpReserved )
{
    switch( reason ) {
        case DLL_PROCESS_ATTACH:
            nsToolkit::Startup(hModule);
            break;

        case DLL_THREAD_ATTACH:
            break;
    
        case DLL_THREAD_DETACH:
            break;
    
        case DLL_PROCESS_DETACH:
            nsToolkit::Shutdown();
            break;

    }

    return TRUE;
}
#endif

//
// main for the message pump thread
//
PRBool gThreadState = PR_FALSE;

struct ThreadInitInfo {
    PRMonitor *monitor;
    nsToolkit *toolkit;
};

/* Detect when the user is moving a top-level window */

LRESULT CALLBACK DetectWindowMove(int code, WPARAM wParam, LPARAM lParam)
{
    /* This msg filter is required to determine when the user has
     * clicked in the window title bar and is moving the window. 
     */

    CWPSTRUCT* sysMsg = (CWPSTRUCT*)lParam;
    if (sysMsg) {
      if (sysMsg->message == WM_ENTERSIZEMOVE) {
        gIsMovingWindow = PR_TRUE; 
        // Notify xpcom that it should favor interactivity
        // over performance because the user is moving a 
        // window
        PL_FavorPerformanceHint(PR_FALSE, 0);
      } else if (sysMsg->message == WM_EXITSIZEMOVE) {
        gIsMovingWindow = PR_FALSE;
        // Notify xpcom that it should go back to its 
        // previous performance setting which may favor
        // performance over interactivity
        PL_FavorPerformanceHint(PR_TRUE, 0);
      }
    }

    return CallNextHookEx(nsMsgFilterHook, code, wParam, lParam);
}




#ifdef MOZ_UNICODE

#include "nsWindowAPI.h"

#define MAX_CLASS_NAME  128
#define MAX_MENU_NAME   128
#define MAX_FILTER_NAME 256

int ConvertAtoW(LPCSTR aStrInA, int aBufferSize, LPWSTR aStrOutW)
{
  return MultiByteToWideChar(CP_ACP, 0, aStrInA, -1, aStrOutW, aBufferSize) ;
}

int ConvertWtoA(LPCWSTR aStrInW, int aBufferSizeOut, LPSTR aStrOutA)
{
  int  numCharsConverted;
  char defaultStr[] = "?";

  if ((!aStrInW) || (!aStrOutA))
    return 0;

  aStrOutA[0] = '\0';

  numCharsConverted = WideCharToMultiByte(CP_ACP, 0, aStrInW, -1, 
      aStrOutA, aBufferSizeOut, defaultStr, NULL);

  if (!numCharsConverted)
    return 0 ;

  if (numCharsConverted < aBufferSizeOut)  {
    *(aStrOutA+numCharsConverted) = '\0' ; // Null terminate
    return (numCharsConverted) ; 
  }

  return 0 ;
}

BOOL CallOpenSaveFileNameA(LPOPENFILENAMEW aFileNameW, BOOL aOpen)
{
  BOOL rtn;
  OPENFILENAMEA ofnA;
  char filterA[MAX_FILTER_NAME+2];
  char customFilterA[MAX_FILTER_NAME+1];
  char fileA[MAX_PATH+1];
  char fileTitleA[MAX_PATH+1];
  char initDirA[MAX_PATH+1];
  char titleA[MAX_PATH+1];
  char defExtA[MAX_PATH+1];
  char tempNameA[MAX_PATH+1];

  memset(&ofnA, 0, sizeof(OPENFILENAMEA));
  ofnA.lStructSize = sizeof(OPENFILENAME); 
  ofnA.hwndOwner = aFileNameW->hwndOwner; 
  ofnA.hInstance = aFileNameW->hInstance; 
  if (aFileNameW->lpstrFilter)  {
    // find the true filter length
    int len = 0;
    while ((aFileNameW->lpstrFilter[len]) ||
           (aFileNameW->lpstrFilter[len+1]))
    {
      ++len;
    }

    len = WideCharToMultiByte(CP_ACP, 0,
                              aFileNameW->lpstrFilter,
                              len,
                              filterA,
                              MAX_FILTER_NAME, NULL, NULL);
    filterA[len] = '\0';
    filterA[len+1] = '\0';
    ofnA.lpstrFilter = filterA; 
  }
  if (aFileNameW->lpstrCustomFilter)  {
    ConvertWtoA(aFileNameW->lpstrCustomFilter, MAX_FILTER_NAME, customFilterA);
    ofnA.lpstrCustomFilter = customFilterA; 
    ofnA.nMaxCustFilter = MAX_FILTER_NAME;  
  }
  ofnA.nFilterIndex = aFileNameW->nFilterIndex; // Index of pair of filter strings. Should be ok.
  if (aFileNameW->lpstrFile)  {
    ConvertWtoA(aFileNameW->lpstrFile, MAX_PATH, fileA);
    ofnA.lpstrFile = fileA;
    ofnA.nMaxFile = MAX_PATH;
    if (strlen(fileA))  {
      // find last file offset
      ofnA.nFileOffset = strrchr(fileA, '\\') - fileA + 1; 
      // find last file extension offset
      ofnA.nFileExtension = strrchr(fileA, '.') - fileA + 1; 
    }
  }
  if (aFileNameW->lpstrFileTitle) {
    ConvertWtoA(aFileNameW->lpstrFileTitle, MAX_PATH, fileTitleA);
    ofnA.lpstrFileTitle = fileTitleA;
    ofnA.nMaxFileTitle = MAX_PATH;  
  }
  if (aFileNameW->lpstrInitialDir)  {
    ConvertWtoA(aFileNameW->lpstrInitialDir, MAX_PATH, initDirA);
    ofnA.lpstrInitialDir = initDirA; 
  }
  if (aFileNameW->lpstrTitle) {
    ConvertWtoA(aFileNameW->lpstrTitle, MAX_PATH, titleA);
    ofnA.lpstrTitle = titleA; 
  }
  ofnA.Flags = aFileNameW->Flags; 
  if (aFileNameW->lpstrDefExt)  {
    ConvertWtoA(aFileNameW->lpstrDefExt, MAX_PATH, defExtA);
    ofnA.lpstrDefExt = defExtA; 
  }
  ofnA.lCustData = aFileNameW->lCustData; // Warning:  No WtoA() is done to application-defined data 
  ofnA.lpfnHook = aFileNameW->lpfnHook;   
  if (aFileNameW->lpTemplateName) {
    ConvertWtoA(aFileNameW->lpTemplateName, MAX_PATH, tempNameA);
    ofnA.lpTemplateName = tempNameA; 
  }
  
  if (aOpen)
    rtn = GetOpenFileNameA(&ofnA);
  else
    rtn = GetSaveFileNameA(&ofnA);

  if (!rtn)
    return 0;

  if (ofnA.lpstrFile) {
    if ((ofnA.Flags & OFN_ALLOWMULTISELECT) && (ofnA.Flags & OFN_EXPLORER)) {
      // lpstrFile contains the directory and file name strings 
      // which are NULL separated, with an extra NULL character after the last file name. 
      int lenA = 0;
      while ((ofnA.lpstrFile[lenA]) || (ofnA.lpstrFile[lenA+1]))
      {
        ++lenA;
      }
      // get the size of required Wide Char and make sure aFileNameW->lpstrFile has enough space
      int lenW = MultiByteToWideChar(CP_ACP, 0, ofnA.lpstrFile, lenA, 0, 0);
      if (aFileNameW->nMaxFile < lenW+2)
        return 0; // doesn't have enough allocated space
      MultiByteToWideChar(CP_ACP, 0, ofnA.lpstrFile, lenA, aFileNameW->lpstrFile, aFileNameW->nMaxFile);
      aFileNameW->lpstrFile[lenW] = '\0';
      aFileNameW->lpstrFile[lenW+1] = '\0';
    }
    else  { 
      ConvertAtoW(ofnA.lpstrFile, aFileNameW->nMaxFile, aFileNameW->lpstrFile);
    }
  }

  aFileNameW->nFilterIndex = ofnA.nFilterIndex;

  return rtn;
}

BOOL WINAPI nsGetOpenFileName(LPOPENFILENAMEW aOpenFileNameW)
{
  return CallOpenSaveFileNameA(aOpenFileNameW, TRUE);
}

BOOL WINAPI nsGetSaveFileName(LPOPENFILENAMEW aSaveFileNameW)
{
  return CallOpenSaveFileNameA(aSaveFileNameW, FALSE);
}

int WINAPI nsGetClassName(HWND aWnd, LPWSTR aClassName, int aMaxCount)
{
  char classNameA[MAX_CLASS_NAME];

  if (!GetClassNameA(aWnd, classNameA, MAX_CLASS_NAME))
    return 0;

  aMaxCount = ConvertAtoW(classNameA, MAX_CLASS_NAME, aClassName);

  return aMaxCount;
}

HWND WINAPI nsCreateWindowEx(DWORD aExStyle,      
                             LPCWSTR aClassNameW,  
                             LPCWSTR aWindowNameW, 
                             DWORD aStyle,        
                             int aX,                
                             int aY,                
                             int aWidth,           
                             int aHeight,          
                             HWND aWndParent,      
                             HMENU aMenu,          
                             HINSTANCE aInstance,  
                             LPVOID aParam)
{
  char classNameA [MAX_CLASS_NAME];
  char windowNameA[MAX_CLASS_NAME];

  // Convert class name and Window name from Unicode to ANSI
  if (aClassNameW)
      ConvertWtoA(aClassNameW, MAX_CLASS_NAME, classNameA);
  if (aWindowNameW)
      ConvertWtoA(aWindowNameW, MAX_CLASS_NAME, windowNameA);
  
  // so far only NULL is passed
  if (aParam != NULL) {
    NS_ASSERTION(0 , "non-NULL lParam is provided in CreateWindowExA");
    return NULL;
  }

  return CreateWindowExA(aExStyle, classNameA, windowNameA,
      aStyle, aX, aY, aWidth, aHeight, aWndParent, aMenu, aInstance, aParam) ;
}

LRESULT WINAPI nsSendMessage(HWND aWnd, UINT aMsg, WPARAM awParam, LPARAM alParam)
{
  // ************ Developers **********************************************
  // As far as I am aware, WM_SETTEXT is the only text related message 
  // we call to SendMessage().  When you need to send other text related message,
  // please use appropriate converters.
  NS_ASSERTION((WM_SETTEXT == aMsg || WM_SETICON == aMsg || WM_SETFONT == aMsg), 
			"Warning. Make sure sending non-Unicode string to ::SendMessage().");
  if (WM_SETTEXT == aMsg)  {
    char title[MAX_PATH];
    if (alParam)
      ConvertWtoA((LPCWSTR)alParam, MAX_CLASS_NAME, title);
    return SendMessageA(aWnd, aMsg, awParam, (LPARAM)&title);
  }

  return SendMessageA(aWnd, aMsg, awParam, alParam);
}

ATOM WINAPI nsRegisterClass(const WNDCLASSW *aClassW)
{
  WNDCLASSA wClass;
  char classNameA[MAX_CLASS_NAME];
  char menuNameA[MAX_MENU_NAME];

  // Set up ANSI version of class struct
  wClass.cbClsExtra   = aClassW->cbClsExtra;
  wClass.cbWndExtra   = aClassW->cbWndExtra;
  wClass.hbrBackground= aClassW->hbrBackground;
  wClass.hCursor      = aClassW->hCursor;
  wClass.hIcon        = aClassW->hIcon;
  wClass.hInstance    = aClassW->hInstance;
  wClass.lpfnWndProc  = aClassW->lpfnWndProc;
  wClass.style        = aClassW->style;

  if (NULL == aClassW->lpszClassName)
    return 0 ;

  wClass.lpszClassName = classNameA;
  if (aClassW->lpszClassName)
    ConvertWtoA(aClassW->lpszClassName, MAX_CLASS_NAME, classNameA);
  
  wClass.lpszMenuName = menuNameA; 
  if (aClassW->lpszMenuName)
    ConvertWtoA(aClassW->lpszMenuName, MAX_MENU_NAME, menuNameA);

  return RegisterClassA(&wClass);
}

BOOL WINAPI nsUnregisterClass(LPCWSTR aClassW, HINSTANCE aInst)
{
  char classA[MAX_PATH+1];

  if (aClassW)  {
    ConvertWtoA(aClassW, MAX_PATH, classA);
    return UnregisterClassA((LPCSTR)classA, aInst);
  }
  return FALSE;
}

BOOL WINAPI nsSHGetPathFromIDList(LPCITEMIDLIST aIdList, LPWSTR aPathW)
{
  char pathA[MAX_PATH+1];

  if (aPathW)  {
    ConvertWtoA(aPathW, MAX_PATH, pathA);
    if (SHGetPathFromIDListA(aIdList, pathA)) {
      ConvertAtoW(pathA, MAX_PATH, aPathW);
      return TRUE;
    }
  }

  return FALSE;
}

LPITEMIDLIST WINAPI nsSHBrowseForFolder(LPBROWSEINFOW aBiW)
{
  BROWSEINFO biA;
  LPITEMIDLIST itemIdList;
  char displayNameA[MAX_PATH];
  char titleA[MAX_PATH];

  memset(&biA, 0, sizeof(BROWSEINFO));
  biA.hwndOwner = aBiW->hwndOwner;
  biA.pidlRoot = aBiW->pidlRoot;
  if (aBiW->pszDisplayName)  {
    ConvertWtoA(aBiW->pszDisplayName, MAX_PATH, displayNameA);
    biA.pszDisplayName = displayNameA; 
  }
  if (aBiW->lpszTitle)  {
    ConvertWtoA(aBiW->lpszTitle, MAX_PATH, titleA);
    biA.lpszTitle = titleA; 
  }
  biA.ulFlags = aBiW->ulFlags;
  biA.lpfn = aBiW->lpfn;
  biA.lParam = aBiW->lParam;
  biA.iImage = aBiW->iImage;

  itemIdList = SHBrowseForFolderA(&biA);
  if (biA.pszDisplayName)  {
    ConvertAtoW(biA.pszDisplayName, MAX_PATH, aBiW->pszDisplayName);
  }
  return itemIdList;
}

HMODULE             nsToolkit::mShell32Module = NULL;
NS_DefWindowProc    nsToolkit::mDefWindowProc = DefWindowProcA;
NS_CallWindowProc   nsToolkit::mCallWindowProc = CallWindowProcA;
NS_SetWindowLong    nsToolkit::mSetWindowLong = SetWindowLongA;
NS_GetWindowLong    nsToolkit::mGetWindowLong = GetWindowLongA;
NS_SendMessage      nsToolkit::mSendMessage = nsSendMessage;
NS_DispatchMessage  nsToolkit::mDispatchMessage = DispatchMessageA;
NS_GetMessage       nsToolkit::mGetMessage = GetMessageA;
NS_PeekMessage      nsToolkit::mPeekMessage = PeekMessageA;
NS_GetOpenFileName  nsToolkit::mGetOpenFileName = nsGetOpenFileName;
NS_GetSaveFileName  nsToolkit::mGetSaveFileName = nsGetSaveFileName;
NS_GetClassName     nsToolkit::mGetClassName = nsGetClassName;
NS_CreateWindowEx   nsToolkit::mCreateWindowEx = nsCreateWindowEx;
NS_RegisterClass    nsToolkit::mRegisterClass = nsRegisterClass; 
NS_UnregisterClass  nsToolkit::mUnregisterClass = nsUnregisterClass; 
NS_SHGetPathFromIDList  nsToolkit::mSHGetPathFromIDList = nsSHGetPathFromIDList; 
NS_SHBrowseForFolder    nsToolkit::mSHBrowseForFolder = nsSHBrowseForFolder; 

#endif /* MOZ_UNICODE */

void RunPump(void* arg)
{
    ThreadInitInfo *info = (ThreadInitInfo*)arg;
    ::PR_EnterMonitor(info->monitor);

#ifdef MOZ_AIMM
    // Start Active Input Method Manager on this thread
    if(nsToolkit::gAIMMApp)
        nsToolkit::gAIMMApp->Activate(TRUE);
#endif

    // do registration and creation in this thread
    info->toolkit->CreateInternalWindow(PR_GetCurrentThread());

    gThreadState = PR_TRUE;

    ::PR_Notify(info->monitor);
    ::PR_ExitMonitor(info->monitor);

    delete info;

    // Process messages
    MSG msg;
#ifdef MOZ_UNICODE
    while (nsToolkit::mGetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        nsToolkit::mDispatchMessage(&msg);
    }
#else    
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
#endif
}

//-------------------------------------------------------------------------
//
// constructor
//
//-------------------------------------------------------------------------
nsToolkit::nsToolkit()  
{
    NS_INIT_ISUPPORTS();
    mGuiThread  = NULL;
    mDispatchWnd = 0;

#ifdef MOZ_AIMM
    //
    // Initialize COM since create Active Input Method Manager object
    //
    if (!nsToolkit::gAIMMCount)
      ::CoInitialize(NULL);

    if(!nsToolkit::gAIMMApp)
      ::CoCreateInstance(CLSID_CActiveIMM, NULL, CLSCTX_INPROC_SERVER, IID_IActiveIMMApp, (void**) &nsToolkit::gAIMMApp);

    nsToolkit::gAIMMCount++;
#endif

#ifdef MOZ_STATIC_COMPONENT_LIBS
    nsToolkit::Startup(GetModuleHandle(NULL));
#endif
}


//-------------------------------------------------------------------------
//
// destructor
//
//-------------------------------------------------------------------------
nsToolkit::~nsToolkit()
{
    NS_PRECONDITION(::IsWindow(mDispatchWnd), "Invalid window handle");

#ifdef MOZ_AIMM
    nsToolkit::gAIMMCount--;

    if (!nsToolkit::gAIMMCount) {
        if(nsToolkit::gAIMMApp) {
            nsToolkit::gAIMMApp->Deactivate();
            nsToolkit::gAIMMApp->Release();
            nsToolkit::gAIMMApp = NULL;
        }
        ::CoUninitialize();
    }
#endif

    // Destroy the Dispatch Window
    ::DestroyWindow(mDispatchWnd);
    mDispatchWnd = NULL;

    // Remove the TLS reference to the toolkit...
    PR_SetThreadPrivate(gToolkitTLSIndex, nsnull);

    // Remove reference to cached event queue
    gEventQueueService = nsnull;

    // Unhook the filter used to determine when
    // the user is moving a top-level window.
    if (nsMsgFilterHook != NULL) {
      UnhookWindowsHookEx(nsMsgFilterHook);
      nsMsgFilterHook = NULL;
    }

#ifdef MOZ_STATIC_COMPONENT_LIBS
    nsToolkit::Shutdown();
#endif
}


void
nsToolkit::Startup(HMODULE hModule)
{
    //
    // Set flag of nsToolkit::mUseImeApiW due to using Unicode API.
    //

    OSVERSIONINFOEX osversion;
    BOOL osVersionInfoEx;
    
    ::ZeroMemory(&osversion, sizeof(OSVERSIONINFOEX));
    osversion.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

    if (!(osVersionInfoEx = GetVersionEx((OSVERSIONINFO *)&osversion))) {
      // if OSVERSIONINFOEX doesn't work, try OSVERSIONINFO.
      osversion.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
      if (!GetVersionEx((OSVERSIONINFO *)&osversion)) {
        // maybe we are running on very old Windows OS. Assign FALSE.
        nsToolkit::mUseImeApiW = PR_FALSE; 
        return;
      }
    }

    nsToolkit::mIsNT = (osversion.dwPlatformId == VER_PLATFORM_WIN32_NT);
    if (nsToolkit::mIsNT)  {
#ifdef MOZ_UNICODE 
      // For Windows 9x base OS nsFoo is already pointing to A functions
      // However on NT base OS we should point them to respective W functions
      nsToolkit::mDefWindowProc = DefWindowProcW;
      nsToolkit::mCallWindowProc = CallWindowProcW;
      nsToolkit::mSetWindowLong = SetWindowLongW;
      nsToolkit::mGetWindowLong = GetWindowLongW; 
      nsToolkit::mSendMessage = SendMessageW;
      nsToolkit::mDispatchMessage = DispatchMessageW;
      nsToolkit::mGetMessage = GetMessageW;
      nsToolkit::mPeekMessage = PeekMessageW;
      nsToolkit::mGetOpenFileName = GetOpenFileNameW;
      nsToolkit::mGetSaveFileName = GetSaveFileNameW;
      nsToolkit::mGetClassName = GetClassNameW;
      nsToolkit::mCreateWindowEx = CreateWindowExW;
      nsToolkit::mRegisterClass = RegisterClassW; 
      nsToolkit::mUnregisterClass = UnregisterClassW; 
      // Explicit call of SHxxxW in Win95 makes moz fails to run (170969)
      // we use GetProcAddress() to hide
      nsToolkit::mShell32Module = ::LoadLibrary("Shell32.dll");
      if (nsToolkit::mShell32Module) {
        nsToolkit::mSHGetPathFromIDList = (NS_SHGetPathFromIDList)GetProcAddress(nsToolkit::mShell32Module, "SHGetPathFromIDListW"); 
        if (!nsToolkit::mSHGetPathFromIDList)
          nsToolkit::mSHGetPathFromIDList = &nsSHGetPathFromIDList;
        nsToolkit::mSHBrowseForFolder = (NS_SHBrowseForFolder)GetProcAddress(nsToolkit::mShell32Module, "SHBrowseForFolderW"); 
        if (!nsToolkit::mSHBrowseForFolder)
          nsToolkit::mSHBrowseForFolder = &nsSHBrowseForFolder;
      }
#endif /* MOZ_UNICODE */ 
      nsToolkit::mUseImeApiW = PR_TRUE;
      // XXX Hack for stopping the crash (125573)
      if (osversion.dwMajorVersion == 5 && (osversion.dwMinorVersion == 0 || osversion.dwMinorVersion == 1))  { 
        nsToolkit::mIsWinXP = (osversion.dwMinorVersion == 1);
        // "Microsoft Windows 2000 " or "Microsoft Windows XP "
        if (936 == ::GetACP())  {  // Chinese (PRC, Singapore)
          nsToolkit::mUseImeApiW = PR_FALSE;
          nsToolkit::mW2KXP_CP936 = PR_TRUE;
        }
      }
    }
    nsToolkit::mDllInstance = hModule;

    //
    // register the internal window class
    //
#ifdef MOZ_UNICODE
    WNDCLASSW wc;
#else
    WNDCLASS wc;
#endif /* MOZ_UNICODE */

    wc.style            = CS_GLOBALCLASS;
    wc.lpfnWndProc      = nsToolkit::WindowProc;
    wc.cbClsExtra       = 0;
    wc.cbWndExtra       = 0;
    wc.hInstance        = nsToolkit::mDllInstance;
    wc.hIcon            = NULL;
    wc.hCursor          = NULL;
    wc.hbrBackground    = NULL;
    wc.lpszMenuName     = NULL;
#ifdef MOZ_UNICODE
    wc.lpszClassName    = L"nsToolkitClass";
    VERIFY(nsToolkit::mRegisterClass(&wc));
#else
    wc.lpszClassName    = "nsToolkitClass";
    VERIFY(::RegisterClass(&wc));
#endif /* MOZ_UNICODE */
}


void
nsToolkit::Shutdown()
{
    if (nsToolkit::mShell32Module)
      ::FreeLibrary(nsToolkit::mShell32Module);

    //VERIFY(::UnregisterClass("nsToolkitClass", nsToolkit::mDllInstance));
    nsToolkit::mUnregisterClass(L"nsToolkitClass", nsToolkit::mDllInstance);
}

nsIEventQueue* 
nsToolkit::GetEventQueue()
{
  if (! gEventQueueService) {
    gEventQueueService = do_GetService(kEventQueueServiceCID);
  }

  if (gEventQueueService) {
    nsCOMPtr<nsIEventQueue> eventQueue;
    gEventQueueService->GetSpecialEventQueue(
      nsIEventQueueService::UI_THREAD_EVENT_QUEUE, 
      getter_AddRefs(eventQueue));
    return eventQueue;
  }
  
  return nsnull;
}

//-------------------------------------------------------------------------
//
// Register the window class for the internal window and create the window
//
//-------------------------------------------------------------------------
void nsToolkit::CreateInternalWindow(PRThread *aThread)
{
    
    NS_PRECONDITION(aThread, "null thread");
    mGuiThread  = aThread;

    //
    // create the internal window
    //
    mDispatchWnd = ::CreateWindow("nsToolkitClass",
                                       "NetscapeDispatchWnd",
                                       WS_DISABLED,
                                       -50, -50,
                                       10, 10,
                                       NULL,
                                       NULL,
                                       nsToolkit::mDllInstance,
                                       NULL);
    VERIFY(mDispatchWnd);
}


//-------------------------------------------------------------------------
//
// Create a new thread and run the message pump in there
//
//-------------------------------------------------------------------------
void nsToolkit::CreateUIThread()
{
    PRMonitor *monitor = ::PR_NewMonitor();

    ::PR_EnterMonitor(monitor);

    ThreadInitInfo *ti = new ThreadInitInfo();
    ti->monitor = monitor;
    ti->toolkit = this;

    // create a gui thread
    mGuiThread = ::PR_CreateThread(PR_SYSTEM_THREAD,
                                    RunPump,
                                    (void*)ti,
                                    PR_PRIORITY_NORMAL,
                                    PR_LOCAL_THREAD,
                                    PR_UNJOINABLE_THREAD,
                                    0);

    // wait for the gui thread to start
    while(gThreadState == PR_FALSE) {
        ::PR_Wait(monitor, PR_INTERVAL_NO_TIMEOUT);
    }

    // at this point the thread is running
    ::PR_ExitMonitor(monitor);
    ::PR_DestroyMonitor(monitor);
}


//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_METHOD nsToolkit::Init(PRThread *aThread)
{
    // Store the thread ID of the thread containing the message pump.  
    // If no thread is provided create one
    if (NULL != aThread) {

#ifdef MOZ_AIMM
        // Start Active Input Method Manager on this thread
        if(nsToolkit::gAIMMApp)
            nsToolkit::gAIMMApp->Activate(TRUE);
#endif

        CreateInternalWindow(aThread);
    } else {
        // create a thread where the message pump will run
        CreateUIThread();
    }

    // Hook window move messages so the toolkit can report when
    // the user is moving a top-level window.
    if (nsMsgFilterHook == NULL) {
      nsMsgFilterHook = SetWindowsHookEx(WH_CALLWNDPROC, DetectWindowMove, 
                                                NULL, GetCurrentThreadId());
    }

    return NS_OK;
}

PRBool nsToolkit::UserIsMovingWindow(void)
{
    return gIsMovingWindow;
}

//-------------------------------------------------------------------------
//
// nsToolkit WindowProc. Used to call methods on the "main GUI thread"...
//
//-------------------------------------------------------------------------
LRESULT CALLBACK nsToolkit::WindowProc(HWND hWnd, UINT msg, WPARAM wParam, 
                                            LPARAM lParam)
{
    switch (msg) {
        case WM_CALLMETHOD:
        {
            MethodInfo *info = (MethodInfo *)lParam;
            return info->Invoke();
        }

        case WM_SYSCOLORCHANGE:
        {
          // WM_SYSCOLORCHANGE messages are only dispatched to top
          // level windows but NS_SYSCOLORCHANGE messages must be dispatched
          // to all windows including child windows. We dispatch these messages 
          // from the nsToolkit because if we are running embedded we may not 
          // have a top-level nsIWidget window.
          
          // On WIN32 all windows are automatically invalidated after the 
          // WM_SYSCOLORCHANGE is dispatched so the window is drawn using
          // the current system colors.
          nsWindow::GlobalMsgWindowProc(hWnd, msg, wParam, lParam);
        }

    }

#ifdef MOZ_AIMM
    if(nsToolkit::gAIMMApp) {
        LRESULT lResult;
        if (nsToolkit::gAIMMApp->OnDefWindowProc(hWnd, msg, wParam, lParam, &lResult) == S_OK)
            return lResult;
    }
#endif

#ifdef MOZ_UNICODE
    return nsToolkit::mDefWindowProc(hWnd, msg, wParam, lParam);
#else
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
#endif
}



//-------------------------------------------------------------------------
//
// Return the nsIToolkit for the current thread.  If a toolkit does not
// yet exist, then one will be created...
//
//-------------------------------------------------------------------------
NS_METHOD NS_GetCurrentToolkit(nsIToolkit* *aResult)
{
  nsIToolkit* toolkit = nsnull;
  nsresult rv = NS_OK;
  PRStatus status;

  // Create the TLS index the first time through...
  if (0 == gToolkitTLSIndex) {
    status = PR_NewThreadPrivateIndex(&gToolkitTLSIndex, NULL);
    if (PR_FAILURE == status) {
      rv = NS_ERROR_FAILURE;
    }
  }

  if (NS_SUCCEEDED(rv)) {
    toolkit = (nsIToolkit*)PR_GetThreadPrivate(gToolkitTLSIndex);

    //
    // Create a new toolkit for this thread...
    //
    if (!toolkit) {
      toolkit = new nsToolkit();

      if (!toolkit) {
        rv = NS_ERROR_OUT_OF_MEMORY;
      } else {
        NS_ADDREF(toolkit);
        toolkit->Init(PR_GetCurrentThread());
        //
        // The reference stored in the TLS is weak.  It is removed in the
        // nsToolkit destructor...
        //
        PR_SetThreadPrivate(gToolkitTLSIndex, (void*)toolkit);
      }
    } else {
      NS_ADDREF(toolkit);
    }
    *aResult = toolkit;
  }

  return rv;
}


//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
MouseTrailer * MouseTrailer::GetMouseTrailer(DWORD aThreadID) {
  if (nsnull == MouseTrailer::theMouseTrailer) {
    MouseTrailer::theMouseTrailer = new MouseTrailer();
  }
  return MouseTrailer::theMouseTrailer;

}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
nsWindow * MouseTrailer::GetMouseTrailerWindow() {
  return MouseTrailer::mHoldMouse;
}
//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
void MouseTrailer::SetMouseTrailerWindow(nsWindow * aNSWin) {
  MouseTrailer::mHoldMouse = aNSWin;
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
MouseTrailer::MouseTrailer()
{
  mTimerId         = 0;
  mHoldMouse       = NULL;
}


//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
MouseTrailer::~MouseTrailer()
{
    DestroyTimer();
    if (mHoldMouse) {
        NS_RELEASE(mHoldMouse);
    }
}


//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
UINT MouseTrailer::CreateTimer()
{
  if (!mTimerId) {
    mTimerId = ::SetTimer(NULL, 0, 200, (TIMERPROC)MouseTrailer::TimerProc);
  } 

  return mTimerId;
}


//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
void MouseTrailer::DestroyTimer()
{
  if (mTimerId) {
    ::KillTimer(NULL, mTimerId);
    mTimerId = 0;
  }

}
//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
void MouseTrailer::SetCaptureWindow(nsWindow * aNSWin) 
{ 
  mCaptureWindow = aNSWin;
  if (nsnull != mCaptureWindow) {
    mIsInCaptureMode = PR_TRUE;
  }
}

#define TIMER_DEBUG 1
//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
void CALLBACK MouseTrailer::TimerProc(HWND hWnd, UINT msg, UINT event, DWORD time)
{
  // Check to see if we are in mouse capture mode,
  // Once capture ends we could still get back one more timer event 
  // Capture could end outside our window
  // Also, for some reason when the mouse is on the frame it thinks that
  // it is inside the window that is being captured.
  if (nsnull != MouseTrailer::mCaptureWindow) {
    if (MouseTrailer::mCaptureWindow != MouseTrailer::mHoldMouse) {
      return;
    }
  } else {
    if (mIsInCaptureMode) {
      // the mHoldMouse could be bad from rolling over the frame, so clear 
      // it if we were capturing and now this is the first timer call back 
      // since we canceled the capture
      MouseTrailer::mHoldMouse = nsnull;
      mIsInCaptureMode = PR_FALSE;
      return;
    }
  }

    if (MouseTrailer::mHoldMouse && ::IsWindow(MouseTrailer::mHoldMouse->GetWindowHandle())) {
      if (gIgnoreNextCycle) {
        gIgnoreNextCycle = PR_FALSE;
      }
      else {
        POINT mp;
        DWORD pos = ::GetMessagePos();
        mp.x = LOWORD(pos);
        mp.y = HIWORD(pos);

        if (::WindowFromPoint(mp) != mHoldMouse->GetWindowHandle()) {
            int64 time = PR_Now(); // time in milliseconds
            ::ScreenToClient(mHoldMouse->GetWindowHandle(), &mp);

            //notify someone that a mouse exit happened
            if (nsnull != MouseTrailer::mHoldMouse) {
              MouseTrailer::mHoldMouse->DispatchMouseEvent(NS_MOUSE_EXIT);
            }
  
            // we are out of this window and of any window, destroy timer
            MouseTrailer::theMouseTrailer->DestroyTimer();
            MouseTrailer::mHoldMouse = NULL;
        }
      }
    } else {
      MouseTrailer::theMouseTrailer->DestroyTimer();
      MouseTrailer::mHoldMouse = NULL;
    }
}



