/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 *
 */

#ifndef _nsframewindow_h
#define _nsframewindow_h

// Widget needs to treat the frame/client as one window - it's only really
// interested in the client.
//
// mWnd is the client window; mBounds holds the frame rectangle relative to
// the desktop.
//
// The frame itself is subclassed so OnMove events for the client happen.

#include "nsWindow.h"
#include "nssize.h"

class nsFrameWindow : public nsWindow
{
 public:
   nsFrameWindow();
   virtual ~nsFrameWindow();

   // So Destroy, Show, SetWindowPos, SetTitle, etc. work
   HWND GetMainWindow() const { return hwndFrame; }

 protected:
   HWND   hwndFrame;
   PFNWP  fnwpDefFrame;
   nsSize mSizeClient;
   nsSize mSizeBorder;

   // So we can create the frame, parent the client & position it right
   virtual void RealDoCreate( HWND hwndP, nsWindow *aParent,
                              const nsRect &aRect,
                              EVENT_CALLBACK aHandleEventFunction,
                              nsIDeviceContext *aContext,
                              nsIAppShell *aAppShell,
                              nsWidgetInitData *aInitData, HWND hwndO);

   // hook so dialog can be created looking like a dialog
   virtual ULONG GetFCFlags();

   // So correct sizing behaviour happens
   PRBool OnReposition( PSWP pSwp);

   // Set up client sizes from frame dimensions
   void    UpdateClientSize();
   PRInt32 GetClientHeight() { return mSizeClient.height; }

   // So we can catch move messages
   MRESULT FrameMessage( ULONG msg, MPARAM mp1, MPARAM mp2);

   NS_IMETHOD Show( PRBool bState);
   void SetWindowListVisibility( PRBool bState);

   // We have client
   NS_IMETHOD GetBorderSize( PRInt32 &aWidth, PRInt32 &aHeight);
   NS_IMETHOD GetClientBounds( nsRect &aRect);

   friend MRESULT EXPENTRY fnwpFrame( HWND, ULONG, MPARAM, MPARAM);
   virtual ULONG WindowStyle();
};

#endif
