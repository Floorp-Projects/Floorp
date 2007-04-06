/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
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
 * The Original Code is a Cocoa widget run loop and event implementation.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Mark Mentovai <mark@moxienet.com> (Original Author)
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

/*
 * Runs the main native Cocoa run loop, interrupting it as needed to process
 * Gecko events.  
 */

#ifndef nsAppShell_h_
#define nsAppShell_h_

#include "nsBaseAppShell.h"

@class AppShellDelegate;

class nsAppShell : public nsBaseAppShell
{
public:
  NS_IMETHODIMP ResumeNative(void);
	
  nsAppShell();

  nsresult Init();

  NS_IMETHOD Run(void);
  NS_IMETHOD OnProcessNextEvent(nsIThreadInternal *aThread, PRBool aMayWait,
                                PRUint32 aRecursionDepth);
  NS_IMETHOD AfterProcessNextEvent(nsIThreadInternal *aThread,
                                   PRUint32 aRecursionDepth);

  // public only to be visible to Objective-C code that must call it
  void ProcessGeckoEvents();
  void WillTerminate();

protected:
  virtual ~nsAppShell();

  virtual void ScheduleNativeEventCallback();
  virtual PRBool ProcessNextNativeEvent(PRBool aMayWait);

protected:
  NSAutoreleasePool* mMainPool;
  CFMutableArrayRef  mAutoreleasePools;

  NSPort*            mPort;
  AppShellDelegate*  mDelegate;

  PRPackedBool       mRunningEventLoop;
  PRPackedBool       mTerminated;
  PRPackedBool       mSkippedNativeCallback;
};

#endif // nsAppShell_h_
