/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Travis Bogard <travis@netscape.com>
 *   Brian Ryner <bryner@brianryner.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsWebBrowserChrome_h__
#define nsWebBrowserChrome_h__

// Helper Classes
#include "nsCOMPtr.h"

// Interfaces Needed
#include "nsIWebBrowserChrome.h"
#include "nsIWebBrowserChromeFocus.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIEmbeddingSiteWindow.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIWebProgressListener.h"
#include "nsIChannel.h"
#include "nsTimer.h"
#include "nsWeakReference.h"

class nsBrowserWindow;

class nsWebBrowserChrome : public nsIWebBrowserChrome,
                           public nsIWebBrowserChromeFocus,
                           public nsIEmbeddingSiteWindow,
                           public nsIWebProgressListener,
                           public nsIInterfaceRequestor,
                           public nsSupportsWeakReference
{
friend class nsBrowserWindow;

public:
   NS_DECL_ISUPPORTS

   NS_DECL_NSIWEBBROWSERCHROME
   NS_DECL_NSIWEBBROWSERCHROMEFOCUS
   NS_DECL_NSIEMBEDDINGSITEWINDOW
   NS_DECL_NSIWEBPROGRESSLISTENER
   NS_DECL_NSIINTERFACEREQUESTOR

protected:
   nsWebBrowserChrome();
   virtual ~nsWebBrowserChrome();

   void BrowserWindow(nsBrowserWindow* aBrowserWindow);
   nsBrowserWindow* BrowserWindow();

   // Status Change Handling
   void OnLoadStart(nsIRequest* aRequest);
   void OnLoadFinished(nsIRequest* aRequest, PRInt32 aProgressStatusFlags);
   void OnStatusDNS(nsIChannel* aChannel);
   void OnStatusConnecting(nsIChannel* aChannel);
   void OnStatusRedirecting(nsIChannel* aChannel);
   void OnStatusNegotiating(nsIChannel* aChannel);
   void OnStatusTransferring(nsIRequest* aRequest);
   void OnWindowActivityStart();
   void OnWindowActivityFinished();

  void EnableParent(PRBool aEnable);

protected:
   nsBrowserWindow*  mBrowserWindow;

   PRPackedBool       mTimerSet;
   PRPackedBool       mContinueModalLoop;
   PRPackedBool       mSizeSet;
   PRPackedBool       mChromeLoaded;

   PRUint32           mChromeFlags;

   MOZ_TIMER_DECLARE(mTotalTime)
   nsresult mModalStatus;

   PRInt32 mActiveDocuments;
   PRInt32 mCurrent, mTotal, mProgress, mMaxProgress;
};

#endif /* nsWebBrowserChrome_h__ */
