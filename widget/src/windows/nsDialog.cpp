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

#include "nsDialog.h"
//#include "nsWindow.h"
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsStringUtil.h"
#include <windows.h>

#include "nsIAppShell.h"
#include "nsIFontMetrics.h"
#include "nsIFontCache.h"
#include "nsGUIEvent.h"
#include "nsIRenderingContext.h"
#include "nsIDeviceContext.h"
#include "nsRect.h"
#include "nsTransform2D.h"
#include "nsStringUtil.h"
#include <windows.h>
#include "nsGfxCIID.h"
#include "resource.h"
#include <commctrl.h>

//-------------------------------------------------------------------------
//
// nsDialog constructor
//
//-------------------------------------------------------------------------
nsDialog::nsDialog(nsISupports *aOuter) : nsWindow(aOuter)
{
    /* hDlgPrint = CreateDialog (hInst, (LPCTSTR) "PrintDlgBox", hwnd, PrintDlgProc) ;
     SetDlgItemText (hDlgPrint, IDD_FNAME, szTitleName) ;

     SetAbortProc (pd.hDC, AbortProc) ;

     GetWindowText (hwnd, (PTSTR) di.lpszDocName, sizeof (PTSTR)) ;
     */


}

//-------------------------------------------------------------------------
//
// nsDialog destructor
//
//-------------------------------------------------------------------------
nsDialog::~nsDialog()
{
}

//-------------------------------------------------------------------------
//
// Set this button label
//
//-------------------------------------------------------------------------
void nsDialog::SetLabel(const nsString& aText)
{
  NS_ALLOC_STR_BUF(label, aText, 256);
  VERIFY(::SetWindowText(mWnd, label));
  NS_FREE_STR_BUF(label);
}

//-------------------------------------------------------------------------
//
// Get this button label
//
//-------------------------------------------------------------------------
void nsDialog::GetLabel(nsString& aBuffer)
{
  int actualSize = ::GetWindowTextLength(mWnd)+1;
  NS_ALLOC_CHAR_BUF(label, 256, actualSize);
  ::GetWindowText(mWnd, label, actualSize);
  aBuffer.SetLength(0);
  aBuffer.Append(label);
  NS_FREE_CHAR_BUF(label);
}



//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsresult nsDialog::QueryObject(const nsIID& aIID, void** aInstancePtr)
{
  nsresult result = nsWindow::QueryObject(aIID, aInstancePtr);

  static NS_DEFINE_IID(kInsDialogIID, NS_IDIALOG_IID);
  if (result == NS_NOINTERFACE && aIID.Equals(kInsDialogIID)) {
      *aInstancePtr = (void*) ((nsIDialog*)this);
      AddRef();
      result = NS_OK;
  }

  return result;
}


//-------------------------------------------------------------------------
//
// move, paint, resizes message - ignore
//
//-------------------------------------------------------------------------
PRBool nsDialog::OnMove(PRInt32, PRInt32)
{
  return PR_FALSE;
}

PRBool nsDialog::OnPaint()
{
#if 0
  return nsWindow::OnPaint();
#else
      nsRect    bounds;
    PRBool result = PR_TRUE;
    PAINTSTRUCT ps;
    HDC hDC = ::BeginPaint(mWnd, &ps);

    RECT rect;
    ::GetClientRect(mWnd, &rect);

    if (rect.left || rect.right || rect.top || rect.bottom) {
        // call the event callback 
        if (mEventCallback) {
            nsPaintEvent event;

            InitEvent(event, NS_PAINT);
            event.widget = (nsIWidget *)((nsWindow*)this);

            //::GetWindowRect(mWnd, &rect);
            //::GetClientRect(mWnd, &rect);
            nsRect rect(rect.left, 
                        rect.top, 
                        rect.right - rect.left, 
                        rect.bottom - rect.top);
            event.rect = &rect;
            event.eventStructType = NS_PAINT_EVENT;

            ::EndPaint(mWnd, &ps);

            static NS_DEFINE_IID(kRenderingContextCID, NS_RENDERING_CONTEXT_CID);
            static NS_DEFINE_IID(kRenderingContextIID, NS_IRENDERING_CONTEXT_IID);

            if (NS_OK == NSRepository::CreateInstance(kRenderingContextCID, nsnull, kRenderingContextIID, (void **)&event.renderingContext))
            {
              event.renderingContext->Init(mContext, (nsIWidget *)((nsWindow*)this));
              result = DispatchEvent(&event);
              NS_RELEASE(event.renderingContext);
            }
            else
              result = PR_FALSE;
        }
        else
            ::EndPaint(mWnd, &ps);
    }
    else
        ::EndPaint(mWnd, &ps);

//    ::EndPaint(mWnd, &ps);

    return result;
#endif
}

PRBool nsDialog::OnResize(nsRect &aWindowRect)
{
  return nsWindow::OnResize(aWindowRect);
}

//-------------------------------------------------------------------------
//
// return the window class name and initialize the class if needed
//
//-------------------------------------------------------------------------
LPCTSTR nsDialog::WindowClass()
{
  return nsWindow::WindowClass();
}

//-------------------------------------------------------------------------
//
// return window styles
//
//-------------------------------------------------------------------------
DWORD nsDialog::WindowStyle()
{ 
  return DS_MODALFRAME;//WS_CHILD | WS_CLIPSIBLINGS; 
}

//-------------------------------------------------------------------------
//
// return window extended styles
//
//-------------------------------------------------------------------------
DWORD nsDialog::WindowExStyle()
{
  return WS_EX_DLGMODALFRAME;
}


//-------------------------------------------------------------------------
//
// get position/dimensions
//
//-------------------------------------------------------------------------

void nsDialog::GetBounds(nsRect &aRect)
{
    nsWindow::GetBounds(aRect);
}

