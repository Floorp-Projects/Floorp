/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
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
 * Copyright (C) 2001 Netscape Communications Corporation.
 * All Rights Reserved.
 * 
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#ifndef imgThread_h__
#define imgThread_h__

#include "nsCOMPtr.h"

#include "nsIRunnable.h"
#include "nsIThread.h"
#include "nsIEventQueue.h"

#include "prmon.h"

#define NS_IMGTHREAD_CID \
{ /* dfc3d8ba-1dd1-11b2-be51-e90c9f8718d5 */         \
     0xdfc3d8ba,                                     \
     0x1dd1,                                         \
     0x11b2,                                         \
    {0xbe, 0x51, 0xe9, 0x0c, 0x9f, 0x87, 0x18, 0xd5} \
}

class imgThread : public nsIRunnable
{
public:
  imgThread();
  virtual ~imgThread();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  /* additional members */
  nsresult Init();
  nsresult GetEventQueue(nsIEventQueue **aEventQueue);

private:
  nsCOMPtr<nsIThread> mThread;

  nsCOMPtr<nsIEventQueue> mEventQueue;

  PRLock *mLock;
  PRMonitor *mMonitor;
};

#endif
