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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 */

#ifndef _NSKEYGENTHREAD_H_
#define _NSKEYGENTHREAD_H_

#include "keyhi.h"
#include "nspr.h"

#include "nsIKeygenThread.h"

struct GenerateKeypairParameters
{
  SECKEYPrivateKey *privateKey;
  SECKEYPublicKey *publicKey;
  PK11SlotInfo *slot;
  PRUint32 keyGenMechanism;
  void *params;
};

class nsKeygenThread : public nsIKeygenThread
{
private:
  PRLock *mutex;
  
  nsIDOMWindowInternal* statusDialogPtr;

  PRBool iAmRunning;
  PRBool keygenReady;
  PRBool statusDialogClosed;
  
  static void run(void *args);
  
  PRThread *threadHandle;
  
  GenerateKeypairParameters *params;

public:
  nsKeygenThread();
  virtual ~nsKeygenThread();
  
  NS_DECL_NSIKEYGENTHREAD
  NS_DECL_ISUPPORTS

  // This transfers a reference of parameters to the thread
  // The parameters will not be copied, the caller keeps ownership.
  // Once the thread is finished, the original instance will be accessed 
  void SetParams(GenerateKeypairParameters *p);
  
  void Join(void);
};

#endif //_NSKEYGENTHREAD_H_
