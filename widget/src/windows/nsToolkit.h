/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef TOOLKIT_H      
#define TOOLKIT_H

#include "nsdefs.h"
#include "nsIToolkit.h"

struct MethodInfo;

/**
 * Wrapper around the thread running the message pump.
 * The toolkit abstraction is necessary because the message pump must
 * execute within the same thread that created the widget under Win32.
 */ 

class nsToolkit : public nsIToolkit
{

  public:
                            nsToolkit();

    NS_DECL_ISUPPORTS

    virtual void            Init(PRThread *aThread);


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
    static  void           SetMouseTrailerWindow(nsWindow * aNSWin);

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
      /// timer ID
    UINT mTimerId;
    //@}

};


#endif  // TOOLKIT_H
