/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsSocketTransport_h___
#define nsSocketTransport_h___

#include "prclist.h"
#include "prio.h"
#include "prnetdb.h"

#include "nsITransport.h"
#include "nsIInputStream.h"
#include "nsIByteBufferInputStream.h"

enum nsSocketState {
  eSocketState_Created      = 0,
  eSocketState_WaitDNS      = 1,
  eSocketState_Closed       = 2,
  eSocketState_WaitConnect  = 3,
  eSocketState_Connected    = 4,
  eSocketState_WaitRead     = 5,
  eSocketState_WaitWrite    = 6,
  eSocketState_Done         = 7,
  eSocketState_Timeout      = 8,
  eSocketState_Error        = 9,
  eSocketState_Max          = 10
};

enum nsSocketOperation {
  eSocketOperation_None     = 0,
  eSocketOperation_Connect  = 1,
  eSocketOperation_Read     = 2,
  eSocketOperation_Write    = 3,
  eSocketOperation_Max      = 4
};


class nsSocketTransportService;

class nsSocketTransport : public PRCList,
                          public nsITransport
{
public:
  // nsISupports methods:
  NS_DECL_ISUPPORTS

  // nsICancelable methods:
  NS_IMETHOD Cancel(void);
  NS_IMETHOD Suspend(void);
  NS_IMETHOD Resume(void);

  // nsITransport methods:
  NS_IMETHOD AsyncRead(nsISupports* context,
                       PLEventQueue* appEventQueue,
                       nsIStreamListener* listener);
  NS_IMETHOD AsyncWrite(nsIInputStream* fromStream,
                        nsISupports* context,
                        PLEventQueue* appEventQueue,
                        nsIStreamObserver* observer);
  NS_IMETHOD OpenInputStream(nsIInputStream* *result);
  NS_IMETHOD OpenOutputStream(nsIOutputStream* *result);


  // nsSocketTransport methods:
  nsSocketTransport();
  virtual ~nsSocketTransport();

  nsresult Init(nsSocketTransportService* aService,
                const char* aHost, 
                PRInt32 aPort);
  nsresult Process(PRInt16 aSelectFlags);

  nsresult doConnection(PRInt16 aSelectFlags);
  nsresult doResolveHost(void);
  nsresult doRead(PRInt16 aSelectFlags);
  nsresult doWrite(PRInt16 aSelectFlags);

  nsresult CloseConnection(void);

  PRFileDesc* GetSocket(void)      { return mSocketFD;    }
  PRInt16     GetSelectFlags(void) { return mSelectFlags; }

protected:
  nsSocketState     mCurrentState;
  nsSocketOperation mOperation;

  PRFileDesc*   mSocketFD;
  PRNetAddr     mNetAddress;
  PRInt16       mSelectFlags;

  char*         mHostName;
  PRInt32       mPort;

  nsISupports* mContext;
  nsIStreamListener* mListener;
  nsIByteBufferInputStream* mReadStream;
  nsIInputStream* mWriteStream;

  nsSocketTransportService* mService;
};

#endif /* nsSocketTransport_h___ */
