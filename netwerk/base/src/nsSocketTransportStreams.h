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

#ifndef nsSocketTransportStreams_h___
#define nsSocketTransportStreams_h___

#include "prtypes.h"
#include "nsIBufferInputStream.h"


// Forward declarations...
class nsSocketTransport;


class nsSocketTransportStream : public nsIBufferInputStream
{
public:
  nsSocketTransportStream();

  // nsISupports methods:
  NS_DECL_ISUPPORTS

    // nsIBaseStream methods:
  NS_IMETHOD Close();

  // nsIInputStream methods:
  NS_IMETHOD GetLength(PRUint32 *aResult);
  NS_IMETHOD Read(char * aBuf, PRUint32 aCount, PRUint32 *aReadCount);

  // nsIBufferInputStream methods:
  NS_IMETHOD GetBuffer(nsIBuffer* *result);
  NS_IMETHOD Fill(const char* aBuf, PRUint32 aCount, PRUint32 *aWriteCount);
  NS_IMETHOD FillFrom(nsIInputStream *inStr, PRUint32 count, PRUint32 *_retval);

  // nsSocketTransportStream methods:
  nsresult Init(nsSocketTransport* aTransport, PRBool aBlockingFlag);
  nsresult BlockTransport(void);

  void Lock(void)   { NS_ASSERTION(mMonitor, "Monitor null."); PR_EnterMonitor(mMonitor); }
  void Notify(void) { NS_ASSERTION(mMonitor, "Monitor null."); PR_Notify(mMonitor); }
  void Wait(void)   { NS_ASSERTION(mMonitor, "Monitor null."); PR_Wait(mMonitor, PR_INTERVAL_NO_TIMEOUT); }
  void Unlock(void) { NS_ASSERTION(mMonitor, "Monitor null."); PR_ExitMonitor(mMonitor); }


protected:
  virtual ~nsSocketTransportStream();

private:
  PRBool  mIsTransportSuspended;
  PRBool  mIsStreamBlocking;

  PRMonitor*                mMonitor;
  nsIBufferInputStream*     mStream;
  nsSocketTransport*        mTransport;
};



#endif /* nsSocketTransportStreams_h___ */

