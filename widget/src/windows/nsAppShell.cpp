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

nsresult nsAppShell::GetNativeEvent(void *& aEvent, nsIWidget* aWidget, PRBool &aIsInWindow, PRBool &aIsMouseEvent)
{
  aIsInWindow = PR_TRUE;
  static MSG msg;
  BOOL isOK = GetMessage(&msg, NULL, 0, 0);
  if (msg.message != WM_TIMER)
    printf("-> %d", msg.message);

  if (isOK) {
    TranslateMessage(&msg);
    /*if (msg.message == 275) {
      aIsInWindow = 0;
      aIsMouseEvent = 1;
      return NS_ERROR_FAILURE;
    } else {
      printf("******** %d\n", msg.message);
    }*/
    switch (msg.message) {
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
        aIsMouseEvent = PR_TRUE;
        break;
      default:
        aIsMouseEvent = PR_FALSE;
    } // switch
    aEvent = &msg;
    if (nsnull != aWidget) {
      // Get Native Window for dialog window
      HWND win;
      win = (HWND)aWidget->GetNativeData(NS_NATIVE_WINDOW);

      // Find top most window of event window
      HWND eWin = msg.hwnd;
      if (NULL != eWin) {
        /*HWND parent = ::GetParent(eWin);
        while (parent != NULL) {
          eWin = parent;
          parent = ::GetParent(eWin);
        }
        */
        if (win == eWin) {
          printf(" Short circut");
          aIsInWindow = PR_TRUE;
        } else {
          RECT r;
          VERIFY(::GetWindowRect(win, &r));
          if (msg.pt.x >= r.left && msg.pt.x <= r.right && msg.pt.y >= r.top && msg.pt.y <= r.bottom) {
            aIsInWindow = PR_TRUE;
          } else {
            aIsInWindow = PR_FALSE;
          }
        }
      }
    }
    printf("%s%s", aIsMouseEvent ? "mouse " : "", aIsInWindow ? "window" : "");
    printf("\n");
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult nsAppShell::DispatchNativeEvent(void * aEvent)
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

