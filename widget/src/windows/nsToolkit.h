/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef TOOLKIT_H      
#define TOOLKIT_H

#include "nsdefs.h"
#include "nsIToolkit.h"

#ifdef MOZ_AIMM
struct IActiveIMMApp;
#endif

struct MethodInfo;

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

#ifdef MOZ_AIMM
    // Active Input Method support
    static IActiveIMMApp *gAIMMApp;
    static PRInt32       gAIMMCount;
#endif

public:
    // wrapper API for Unicode window support

    static BOOL nsPeekMessage(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin,
                              UINT wMsgFilterMax, UINT wRemoveMsg)
    {
        return nsToolkit::mIsNT ?
            ::PeekMessageW(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg) :
            ::PeekMessage(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg);
    }

    static BOOL nsGetMessage(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin,
        UINT wMsgFilterMax)
    {
        return nsToolkit::mIsNT ?
            ::GetMessageW(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax) :
            ::GetMessage(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax);
    }

    static LRESULT nsDispatchMessage(CONST MSG *lpmsg)
    {
        return nsToolkit::mIsNT ?
            ::DispatchMessageW(lpmsg) :
            ::DispatchMessage(lpmsg);
    }

    static LRESULT nsDefWindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
    {
        return nsToolkit::mIsNT ?
            ::DefWindowProcW(hWnd, Msg, wParam, lParam) :
            ::DefWindowProc(hWnd, Msg, wParam, lParam);
    }

    static LONG nsGetWindowLong(HWND hWnd, int nIndex)
    {
        return nsToolkit::mIsNT ?
            ::GetWindowLongW(hWnd, nIndex) :
            ::GetWindowLong(hWnd, nIndex);
    }

    static LONG nsSetWindowLong(HWND hWnd, int nIndex, LONG dwNewLong)
    {
        return nsToolkit::mIsNT ?
            ::SetWindowLongW(hWnd, nIndex, dwNewLong) :
            ::SetWindowLong(hWnd, nIndex, dwNewLong);
    }

    static HWND nsCreateWindow(LPCSTR lpClassName,
                               LPCSTR lpWindowName, DWORD dwStyle,
                               int x, int y, int nWidth, int nHeight,
                               HWND hWndParent, HMENU hMenu,
                               HINSTANCE hInstance, LPVOID lpParam)
    {
        return nsCreateWindowEx(WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR,
                                lpClassName, lpWindowName, dwStyle,
                                x, y, nWidth, nHeight,
                                hWndParent, hMenu, hInstance, lpParam);
    }

    static HWND nsCreateWindowEx(DWORD dwExStyle, LPCSTR lpClassName,
                                 LPCSTR lpWindowName, DWORD dwStyle,
                                 int x, int y, int nWidth, int nHeight,
                                 HWND hWndParent, HMENU hMenu,
                                 HINSTANCE hInstance, LPVOID lpParam);

    static ATOM nsRegisterClass(CONST WNDCLASS *lpWndClass);
    static BOOL nsUnregisterClass(LPCSTR lpClassName, HINSTANCE hInstance);

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

class MouseTrailer {

public:
    static  MouseTrailer * GetMouseTrailer(DWORD aThreadID);
    static  nsWindow     * GetMouseTrailerWindow();
    static  nsWindow     * GetCaptureWindow() { return mCaptureWindow; }

    static  void           SetMouseTrailerWindow(nsWindow * aNSWin);
    static  void           SetCaptureWindow(nsWindow * aNSWin);
    static  void           IgnoreNextCycle() { gIgnoreNextCycle = PR_TRUE; } 


private:
      /// Global nsToolkit Instance
    static MouseTrailer* theMouseTrailer;

public:
                            ~MouseTrailer();

            UINT            CreateTimer();
            void            DestroyTimer();

private:
      /**
       * Handle timer events
       * @param hWnd handle to window
       * @param msg  Win32 message
       * @param event Win32 event
       * @param time time of the event
       */
    static  void            CALLBACK TimerProc(HWND hWnd, 
                                                UINT msg, 
                                                UINT event, 
                                                DWORD time);

                            MouseTrailer();

private:
    /* global information for mouse enter/exit events
     */
    //@{
      /// last window
    static nsWindow* mHoldMouse;
    static nsWindow* mCaptureWindow;
    static PRBool    mIsInCaptureMode;
      /// timer ID
    UINT   mTimerId;
    static PRBool gIgnoreNextCycle;
    //@}

};


#endif  // TOOLKIT_H
