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
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is
 * John Fairhurst, <john_fairhurst@iname.com>.
 * Portions created by the Initial Developer are Copyright (C) 1999
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
   HWND GetMainWindow() const { return mFrameWnd; }

 protected:
   PFNWP  fnwpDefFrame;
   nsSize mSizeClient;
   nsSize mSizeBorder;
   PRBool mNeedActivation;

   // Fires NS_ACTIVATE is mNeedActivation is set
   virtual void ActivateTopLevelWidget();

   // So we can create the frame, parent the client & position it right
   virtual void RealDoCreate( HWND hwndP, nsWindow *aParent,
                              const nsIntRect &aRect,
                              EVENT_CALLBACK aHandleEventFunction,
                              nsIDeviceContext *aContext,
                              nsIAppShell *aAppShell,
                              nsWidgetInitData *aInitData, HWND hwndO);

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
   NS_IMETHOD GetClientBounds( nsIntRect &aRect);

   friend MRESULT EXPENTRY fnwpFrame( HWND, ULONG, MPARAM, MPARAM);
   static BOOL fHiddenWindowCreated;
};

#endif
