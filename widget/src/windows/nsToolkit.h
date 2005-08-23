/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef TOOLKIT_H      
#define TOOLKIT_H

#include "nsdefs.h"
#include "nsIToolkit.h"

#include "nsWindowAPI.h"
#include "nsITimer.h"
#include "nsCOMPtr.h"

#include "aimm.h"

struct MethodInfo;
class nsIEventQueue;

// we used to use MAX_PATH
// which works great for one file
// but for multiple files, the format is
// dirpath\0\file1\0file2\0...filen\0\0
// and that can quickly be more than MAX_PATH (260)
// see bug #172001 for more details
#define FILE_BUFFER_SIZE 4096 


/**
 * Wrapper around the thread running the message pump.
 * The toolkit abstraction is necessary because the message pump must
 * execute within the same thread that created the widget under Win32.
 */ 

class nsToolkit : public nsIToolkit
{

  public:

            NS_DECL_ISUPPORTS

                            nsToolkit();
            NS_IMETHOD      Init(PRThread *aThread);
            void            CallMethod(MethodInfo *info);
            // Return whether the current thread is the application's Gui thread.  
            PRBool          IsGuiThread(void)      { return (PRBool)(mGuiThread == PR_GetCurrentThread());}
            PRThread*       GetGuiThread(void)       { return mGuiThread;   }
            HWND            GetDispatchWindow(void)  { return mDispatchWnd; }
            void            CreateInternalWindow(PRThread *aThread);
            // Return whether the user is currently moving any application window
            PRBool          UserIsMovingWindow(void);
            nsIEventQueue*  GetEventQueue(void);

private:
                            ~nsToolkit();
            void            CreateUIThread(void);

public:
    // Window procedure for the internal window
    static LRESULT CALLBACK WindowProc(HWND hWnd, 
                                        UINT Msg, 
                                        WPARAM WParam, 
                                        LPARAM lParam);

protected:
    // Handle of the window used to receive dispatch messages.
    HWND        mDispatchWnd;
    // Thread Id of the "main" Gui thread.
    PRThread    *mGuiThread;

public:
    static HINSTANCE mDllInstance;
    // OS flag
    static PRBool    mIsNT;
    static PRBool    mIsWinXP;
    static PRBool    mUseImeApiW;
    static PRBool    mW2KXP_CP936;

    static void Startup(HINSTANCE hModule);
    static void Shutdown();

    // Active Input Method support
    static IActiveIMMApp *gAIMMApp;
    static PRInt32       gAIMMCount;

    // Ansi API support
    static HMODULE              mShell32Module;
    static NS_DefWindowProc     mDefWindowProc;
    static NS_CallWindowProc    mCallWindowProc;
    static NS_SetWindowLong     mSetWindowLong;
    static NS_GetWindowLong     mGetWindowLong;
    static NS_SendMessage       mSendMessage;
    static NS_DispatchMessage   mDispatchMessage;
    static NS_GetMessage        mGetMessage;
    static NS_PeekMessage       mPeekMessage;
    static NS_GetOpenFileName   mGetOpenFileName;
    static NS_GetSaveFileName   mGetSaveFileName;
    static NS_GetClassName      mGetClassName;
    static NS_CreateWindowEx    mCreateWindowEx;
    static NS_RegisterClass     mRegisterClass;
    static NS_UnregisterClass   mUnregisterClass;
    static NS_SHGetPathFromIDList mSHGetPathFromIDList;
#ifndef WINCE
    static NS_SHBrowseForFolder   mSHBrowseForFolder;
#endif
};

#define WM_CALLMETHOD   (WM_USER+1)

inline void nsToolkit::CallMethod(MethodInfo *info)
{
    NS_PRECONDITION(::IsWindow(mDispatchWnd), "Invalid window handle");
    ::SendMessage(mDispatchWnd, WM_CALLMETHOD, (WPARAM)0, (LPARAM)info);
}

class  nsWindow;

/**
 * Makes sure exit/enter mouse messages are always dispatched.
 * In the case where the mouse has exited the outer most window the
 * only way to tell if it has exited is to set a timer and look at the
 * mouse pointer to see if it is within the outer most window.
 */ 

class MouseTrailer 
{
public:
    static MouseTrailer  &GetSingleton() { return mSingleton; }
    
    nsWindow             *GetMouseTrailerWindow() { return mHoldMouseWindow; }
    nsWindow             *GetCaptureWindow() { return mCaptureWindow; }

    void                  SetMouseTrailerWindow(nsWindow * aNSWin);
    void                  SetCaptureWindow(nsWindow * aNSWin);
    void                  IgnoreNextCycle() { mIgnoreNextCycle = PR_TRUE; } 
    void                  DestroyTimer();
                          ~MouseTrailer();

private:
                          MouseTrailer();

    nsresult              CreateTimer();

    static void           TimerProc(nsITimer* aTimer, void* aClosure);

    // Global nsToolkit Instance
    static MouseTrailer   mSingleton;

    // information for mouse enter/exit events
    // last window
    nsWindow             *mHoldMouseWindow;
    nsWindow             *mCaptureWindow;
    PRBool                mIsInCaptureMode;
    PRBool                mIgnoreNextCycle;
    // timer
    nsCOMPtr<nsITimer>    mTimer;

};

//-------------------------------------------------------------------------
//
// Native IMM wrapper
//
//-------------------------------------------------------------------------
class nsIMM
{
  //prototypes for DLL function calls...
  typedef LONG (CALLBACK *GetCompStrPtr)       (HIMC, DWORD, LPVOID, DWORD);
  typedef LONG (CALLBACK *GetContextPtr)       (HWND);
  typedef LONG (CALLBACK *RelContextPtr)       (HWND, HIMC);
  typedef LONG (CALLBACK *NotifyIMEPtr)        (HIMC, DWORD, DWORD, DWORD);
  typedef LONG (CALLBACK *SetCandWindowPtr)    (HIMC, LPCANDIDATEFORM);
  typedef LONG (CALLBACK *SetCompWindowPtr)    (HIMC, LPCOMPOSITIONFORM);
  typedef LONG (CALLBACK *GetCompWindowPtr)    (HIMC, LPCOMPOSITIONFORM);
  typedef LONG (CALLBACK *GetPropertyPtr)      (HKL, DWORD);
  typedef LONG (CALLBACK *GetDefaultIMEWndPtr) (HWND);
  typedef BOOL (CALLBACK *GetOpenStatusPtr)    (HIMC);
  typedef BOOL (CALLBACK *SetOpenStatusPtr)    (HIMC, BOOL);
public:

  static nsIMM& LoadModule();

  nsIMM(const char* aModuleName="IMM32.DLL");
  ~nsIMM();

  LONG GetCompositionStringA(HIMC aIMC, DWORD aIndex,
                             LPVOID aBuf, DWORD aBufLen);
  LONG GetCompositionStringW(HIMC aIMC, DWORD aIndex,
                             LPVOID aBuf, DWORD aBufLen);
  LONG GetContext(HWND aWnd);
  LONG ReleaseContext(HWND aWnd, HIMC aIMC);
  LONG NotifyIME(HIMC aIMC, DWORD aAction, DWORD aIndex, DWORD aValue);
  LONG SetCandidateWindow(HIMC aIMC, LPCANDIDATEFORM aCandidateForm);
  LONG SetCompositionWindow(HIMC aIMC, LPCOMPOSITIONFORM aCompositionForm);
  LONG GetCompositionWindow(HIMC aIMC,LPCOMPOSITIONFORM aCompositionForm);
  LONG GetProperty(HKL aKL, DWORD aIndex);
  LONG GetDefaultIMEWnd(HWND aWnd);
  BOOL GetOpenStatus(HIMC aIMC);
  BOOL SetOpenStatus(HIMC aIMC, BOOL aStatus);
private:

  HINSTANCE           mInstance;
  GetCompStrPtr       mGetCompositionStringA;
  GetCompStrPtr       mGetCompositionStringW;
  GetContextPtr       mGetContext;
  RelContextPtr       mReleaseContext;
  NotifyIMEPtr        mNotifyIME;
  SetCandWindowPtr    mSetCandiateWindow;
  SetCompWindowPtr    mSetCompositionWindow;
  GetCompWindowPtr    mGetCompositionWindow;
  GetPropertyPtr      mGetProperty;
  GetDefaultIMEWndPtr mGetDefaultIMEWnd;
  GetOpenStatusPtr    mGetOpenStatus;
  SetOpenStatusPtr    mSetOpenStatus;
};

//-------------------------------------------------------------------------
//
// Native WinNLS wrapper
//
//-------------------------------------------------------------------------
class nsWinNLS
{
  //prototypes for DLL function calls...
  typedef LONG (CALLBACK *WINNLSEnableIMEPtr)       (HWND, BOOL);
  typedef LONG (CALLBACK *WINNLSGetEnableStatusPtr) (HWND);
public:

  static nsWinNLS& LoadModule();

  nsWinNLS(const char* aModuleName = "USER32.DLL");
  ~nsWinNLS();

  PRBool SetIMEEnableStatus(HWND aWnd, PRBool aState);
  PRBool GetIMEEnableStatus(HWND aWnd);

  PRBool CanUseSetIMEEnableStatus();
  PRBool CanUseGetIMEEnableStatus();

private:

  HINSTANCE                mInstance;
  WINNLSEnableIMEPtr       mWINNLSEnableIME;
  WINNLSGetEnableStatusPtr mWINNLSGetEnableStatus;
};

//-------------------------------------------------------------------------
//
// Macro for Active Input Method Manager (AIMM) support.
// Use AIMM method instead of Win32 Imm APIs.
//
//-------------------------------------------------------------------------
#define NS_IMM_GETCOMPOSITIONSTRINGA(hIMC, dwIndex, pBuf, \
                                     dwBufLen, compStrLen) \
{ \
  compStrLen = 0; \
  if (nsToolkit::gAIMMApp) \
    nsToolkit::gAIMMApp->GetCompositionStringA(hIMC, dwIndex, \
                                               dwBufLen, &(compStrLen), \
                                               pBuf); \
 else { \
    nsIMM &theIMM = nsIMM::LoadModule(); \
    compStrLen = \
      theIMM.GetCompositionStringA(hIMC, dwIndex, pBuf, dwBufLen); \
 } \
}

#define NS_IMM_GETCOMPOSITIONSTRINGW(hIMC, dwIndex, pBuf, \
                                     dwBufLen, compStrLen) \
{ \
  compStrLen = 0; \
  if (nsToolkit::gAIMMApp) \
    nsToolkit::gAIMMApp->GetCompositionStringW(hIMC, dwIndex, \
                                               dwBufLen, &(compStrLen), \
                                               pBuf); \
  else { \
    nsIMM &theIMM = nsIMM::LoadModule(); \
    compStrLen = \
      theIMM.GetCompositionStringW(hIMC, dwIndex, pBuf, dwBufLen); \
  } \
}

#define NS_IMM_GETCONTEXT(hWnd, hIMC) \
{ \
  hIMC = NULL; \
  if (nsToolkit::gAIMMApp) \
    nsToolkit::gAIMMApp->GetContext(hWnd, &(hIMC)); \
  else { \
    nsIMM& theIMM = nsIMM::LoadModule(); \
    hIMC = (HIMC)theIMM.GetContext(hWnd);  \
  } \
}

#define NS_IMM_RELEASECONTEXT(hWnd, hIMC) \
{ \
  if (nsToolkit::gAIMMApp) \
    nsToolkit::gAIMMApp->ReleaseContext(hWnd, hIMC); \
  else { \
    nsIMM &theIMM = nsIMM::LoadModule(); \
    theIMM.ReleaseContext(hWnd, hIMC); \
  } \
}

#define NS_IMM_NOTIFYIME(hIMC, dwAction, dwIndex, dwValue, bRtn) \
{ \
  bRtn = TRUE; \
  if (nsToolkit::gAIMMApp) { \
    bRtn = (nsToolkit::gAIMMApp->NotifyIME(hIMC, dwAction, \
                                           dwIndex, dwValue) == S_OK); \
  }\
  else { \
    nsIMM &theIMM = nsIMM::LoadModule(); \
    (theIMM.NotifyIME(hIMC, dwAction, dwIndex, dwValue)); \
  } \
}

#define NS_IMM_SETCANDIDATEWINDOW(hIMC, candForm) \
{ \
  if (nsToolkit::gAIMMApp) \
    nsToolkit::gAIMMApp->SetCandidateWindow(hIMC, candForm); \
  else { \
    nsIMM &theIMM = nsIMM::LoadModule(); \
    theIMM.SetCandidateWindow(hIMC, candForm); \
  } \
}

#define NS_IMM_SETCOMPOSITIONWINDOW(hIMC, compForm) \
{ \
  if (nsToolkit::gAIMMApp) \
    nsToolkit::gAIMMApp->SetCompositionWindow(hIMC, compForm); \
  else { \
    nsIMM &theIMM = nsIMM::LoadModule(); \
    theIMM.SetCompositionWindow(hIMC, compForm); \
  } \
}

#define NS_IMM_GETCOMPOSITIONWINDOW(hIMC, compForm) \
{ \
  if (nsToolkit::gAIMMApp) \
    nsToolkit::gAIMMApp->GetCompositionWindow(hIMC, compForm); \
  else { \
    nsIMM &theIMM = nsIMM::LoadModule(); \
    theIMM.GetCompositionWindow(hIMC, compForm); \
  } \
}

#define NS_IMM_GETPROPERTY(hKL, dwIndex, dwProp) \
{ \
  if (nsToolkit::gAIMMApp) \
    nsToolkit::gAIMMApp->GetProperty(hKL, dwIndex, &(dwProp)); \
  else { \
    nsIMM& theIMM = nsIMM::LoadModule(); \
    dwProp = (DWORD)theIMM.GetProperty(hKL, dwIndex);  \
  } \
}

#define NS_IMM_GETDEFAULTIMEWND(hWnd, phDefWnd) \
{ \
  if (nsToolkit::gAIMMApp) \
    return nsToolkit::gAIMMApp->GetDefaultIMEWnd(hWnd, phDefWnd); \
  else { \
    nsIMM& theIMM = nsIMM::LoadModule(); \
    *(phDefWnd) = (HWND)theIMM.GetDefaultIMEWnd(hWnd);  \
  } \
}

#define NS_IMM_GETOPENSTATUS(hIMC, bRtn) \
{ \
  if (nsToolkit::gAIMMApp) \
    bRtn = nsToolkit::gAIMMApp->GetOpenStatus(hIMC); \
  else { \
    nsIMM& theIMM = nsIMM::LoadModule(); \
    bRtn = theIMM.GetOpenStatus(hIMC);  \
  } \
}

#define NS_IMM_SETOPENSTATUS(hIMC, bOpen) \
{ \
  if (nsToolkit::gAIMMApp) \
    nsToolkit::gAIMMApp->SetOpenStatus(hIMC, bOpen); \
  else { \
    nsIMM& theIMM = nsIMM::LoadModule(); \
    theIMM.SetOpenStatus(hIMC, bOpen);  \
  } \
}

//-------------------------------------------------------------------------
//
// Macro for Input Method A/W conversion.
//
// On Windows 2000, ImmGetCompositionStringA() doesn't work well using IME of
// different code page.  (See BUG # 29606)
// And ImmGetCompositionStringW() doesn't work on Windows 9x.
//
//-------------------------------------------------------------------------

#define NS_IMM_GETCOMPOSITIONSTRING(hIMC, dwIndex, cBuf, dwBufLen, lRtn) \
{ \
  if (nsToolkit::mUseImeApiW) { \
    NS_IMM_GETCOMPOSITIONSTRINGW(hIMC, dwIndex, cBuf, dwBufLen, lRtn); \
  } else { \
    NS_IMM_GETCOMPOSITIONSTRINGA(hIMC, dwIndex, cBuf, dwBufLen, lRtn); \
  } \
}

//-------------------------------------------------------------------------
//
// related WM_IME_REQUEST definition.
// VC++5.0 header doesn't have reconversion structure and message.
//
//-------------------------------------------------------------------------

#ifndef WM_IME_REQUEST
typedef struct tagRECONVERTSTRING {
  DWORD dwSize;
  DWORD dwVersion;
  DWORD dwStrLen;
  DWORD dwStrOffset;
  DWORD dwCompStrLen;
  DWORD dwCompStrOffset;
  DWORD dwTargetStrLen;
  DWORD dwTargetStrOffset;
} RECONVERTSTRING, FAR * LPRECONVERTSTRING;

typedef struct tagIMECHARPOSITION {
  DWORD dwSize;
  DWORD dwCharPos;
  POINT pt;
  UINT  cLineHeight;
  RECT  rcDocument;
} IMECHARPOSITION, *PIMECHARPOSITION;

#define IMR_RECONVERTSTRING             0x0004
#define IMR_QUERYCHARPOSITION           0x0006
#define WM_IME_REQUEST                  0x0288
#endif    // #ifndef WM_IME_REQUEST

//-------------------------------------------------------------------------
//
// from http://msdn.microsoft.com/library/specs/msime.h
//
//-------------------------------------------------------------------------

#define RWM_RECONVERT       TEXT("MSIMEReconvert")
#define RWM_MOUSE           TEXT("MSIMEMouseOperation")

#define IMEMOUSE_NONE       0x00    // no mouse button was pushed
#define IMEMOUSE_LDOWN      0x01
#define IMEMOUSE_RDOWN      0x02
#define IMEMOUSE_MDOWN      0x04
#define IMEMOUSE_WUP        0x10    // wheel up
#define IMEMOUSE_WDOWN      0x20    // wheel down

//-------------------------------------------------------------------------
//
// from http://www.justsystem.co.jp/tech/atok/api12_04.html#4_11
//
//-------------------------------------------------------------------------

#define MSGNAME_ATOK_RECONVERT TEXT("Atok Message for ReconvertString")

#endif  // TOOLKIT_H
