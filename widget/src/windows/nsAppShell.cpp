/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/*
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

#include "nsAppShell.h"
#include "nsIWidget.h"
#include "nsSelectionMgr.h"
#include <windows.h>

NS_IMPL_ISUPPORTS(nsAppShell, NS_IAPPSHELL_IID) 

//-------------------------------------------------------------------------
//
// nsAppShell constructor
//
//-------------------------------------------------------------------------
nsAppShell::nsAppShell()  
{ 
  NS_INIT_REFCNT();
  mDispatchListener = 0;
  mSelectionMgr = 0;
}



//-------------------------------------------------------------------------
//
// Create the application shell
//
//-------------------------------------------------------------------------

NS_METHOD nsAppShell::Create(int* argc, char ** argv)
{
  // Create the selection manager
  if (!mSelectionMgr)
      NS_NewSelectionMgr(&mSelectionMgr);

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsAppShell::SetDispatchListener(nsDispatchListener* aDispatchListener) 
{
  mDispatchListener = aDispatchListener;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Enter a message handler loop
//
//-------------------------------------------------------------------------

nsresult nsAppShell::Run()
{
  NS_ADDREF_THIS();
    // Process messages
  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
    if (mDispatchListener)
      mDispatchListener->AfterDispatch();
  }
  Release();
  return msg.wParam;
}

NS_METHOD
nsAppShell::GetNativeEvent(PRBool &aRealEvent, void *&aEvent)
{
  static MSG msg;

  BOOL isOK = GetMessage(&msg, NULL, 0, 0);
#ifdef DEBUG_danm
  if (msg.message != WM_TIMER)
    printf("-> %d", msg.message);
#endif

  if (isOK) {
    TranslateMessage(&msg);
    aEvent = &msg;
    aRealEvent = PR_TRUE;
    return NS_OK;
  }
  aRealEvent = PR_FALSE;
  return NS_ERROR_FAILURE;
}

NS_METHOD
nsAppShell::EventIsForModalWindow(PRBool aRealEvent, void *aEvent,
                            nsIWidget *aWidget, PRBool *aForWindow)
{
  PRBool isInWindow,
         isMouseEvent;
  MSG    *msg = (MSG *) aEvent;

  if (aRealEvent == PR_FALSE) {
     *aForWindow = PR_FALSE;
     return NS_OK;
   }

   isInWindow = PR_FALSE;
   if (aWidget != nsnull) {
     // Get Native Window for dialog window
     HWND win;
     win = (HWND)aWidget->GetNativeData(NS_NATIVE_WINDOW);

     // Find top most window of event window
     HWND eWin = msg->hwnd;
     if (NULL != eWin) {
       /*HWND parent = ::GetParent(eWin);
       while (parent != NULL) {
         eWin = parent;
         parent = ::GetParent(eWin);
       }
       */
       if (win == eWin) {
#ifdef DEBUG_danm
         printf(" Short circut");
#endif
         isInWindow = PR_TRUE;
       } else {
         RECT r;
         VERIFY(::GetWindowRect(win, &r));
         if (msg->pt.x >= r.left && msg->pt.x <= r.right && msg->pt.y >= r.top && msg->pt.y <= r.bottom)
           isInWindow = PR_TRUE;
       }
     }
   }

  isMouseEvent = PR_FALSE;
  switch (msg->message) {
     case WM_MOUSEMOVE:
     case WM_LBUTTONDOWN:
     case WM_LBUTTONUP:
     case WM_LBUTTONDBLCLK:
     case WM_MBUTTONDOWN:
     case WM_MBUTTONUP:
     case WM_MBUTTONDBLCLK:
     case WM_RBUTTONDOWN:
     case WM_RBUTTONUP:
     case WM_RBUTTONDBLCLK:
       isMouseEvent = PR_TRUE;
  }

  *aForWindow = isInWindow == PR_TRUE || isMouseEvent == PR_FALSE ?
                  PR_TRUE : PR_FALSE;

  return NS_OK;
}

nsresult nsAppShell::DispatchNativeEvent(PRBool aRealEvent, void *aEvent)
{
  DispatchMessage((MSG *)aEvent);
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Exit a message handler loop
//
//-------------------------------------------------------------------------

NS_METHOD nsAppShell::Exit()
{
  PostQuitMessage(0);
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// nsAppShell destructor
//
//-------------------------------------------------------------------------
nsAppShell::~nsAppShell()
{
  NS_IF_RELEASE(mSelectionMgr);
}

//-------------------------------------------------------------------------
//
// GetNativeData
//
//-------------------------------------------------------------------------
void* nsAppShell::GetNativeData(PRUint32 aDataType)
{
  if (aDataType == NS_NATIVE_SHELL) {
    return NULL;
  }
  return nsnull;
}

NS_METHOD
nsAppShell::GetSelectionMgr(nsISelectionMgr** aSelectionMgr)
{
  *aSelectionMgr = mSelectionMgr;
  NS_IF_ADDREF(mSelectionMgr);
  if (!mSelectionMgr)
    return NS_ERROR_NOT_INITIALIZED;
  return NS_OK;
}

