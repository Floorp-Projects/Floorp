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

#ifndef nsSocketTransportService_h___
#define nsSocketTransportService_h___

#include "nspr.h"
#include "nsIThread.h"
#include "nsISocketTransportService.h"
#include "nsIInputStream.h"

#if defined(XP_PC) || defined(XP_UNIX)
//
// Both Windows and Unix support PR_PollableEvents which are used to break
// the socket transport thread out of calls to PR_Poll(...) when new
// file descriptors must be added to the poll list...
//
#define USE_POLLABLE_EVENT
#endif

// Forward declarations...
class nsSocketTransport;

class nsSocketTransportService : public nsISocketTransportService,
                                 public nsIRunnable
{
public:
  NS_DECL_ISUPPORTS

  // nsISocketTransportService methods:
  NS_IMETHOD CreateTransport(const char* aHost, 
                             PRInt32 aPort,
                             nsIChannel** aResult);

  NS_IMETHOD CreateTransport(const char* aHost, 
                             PRInt32 aPort,
                             const char* aSocketType,
                             nsIChannel** aResult);

  NS_IMETHOD Shutdown(void);

  // nsIRunnable methods:
  NS_IMETHOD Run(void);

  // nsSocketTransportService methods:
  nsSocketTransportService();
  virtual ~nsSocketTransportService();

  static NS_METHOD
  Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

  nsresult Init(void);

  nsresult AddToWorkQ(nsSocketTransport* aTransport);

  // The following methods are called by the transport thread...
  nsresult ProcessWorkQ(void);

  nsresult AddToSelectList(nsSocketTransport* aTransport);
  nsresult RemoveFromSelectList(nsSocketTransport* aTransport);

protected:
  nsIThread*  mThread;
#ifdef USE_POLLABLE_EVENT
  PRFileDesc* mThreadEvent;
#endif /* USE_POLLABLE_EVENT */
  PRLock*     mThreadLock;
  PRBool      mThreadRunning;

  PRCList     mWorkQ;

  PRInt32       mSelectFDSetCount;
  PRPollDesc*   mSelectFDSet;
  nsSocketTransport** mActiveTransportList;
};


#endif /* nsSocketTransportService_h___ */


