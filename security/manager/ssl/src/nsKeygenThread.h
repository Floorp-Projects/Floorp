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

class nsKeygenThread : public nsIKeygenThread
{
private:
  PRLock *mutex;
  
  nsIDOMWindowInternal* statusDialogPtr;

  PRBool iAmRunning;
  PRBool keygenReady;
  PRBool statusDialogClosed;
  PRBool alreadyReceivedParams;

  SECKEYPrivateKey *privateKey;
  SECKEYPublicKey *publicKey;
  PK11SlotInfo *slot;
  PRUint32 keyGenMechanism;
  void *params;
  PRBool isPerm;
  PRBool isSensitive;
  void *wincx;

  PRThread *threadHandle;
  
public:
  nsKeygenThread();
  virtual ~nsKeygenThread();
  
  NS_DECL_NSIKEYGENTHREAD
  NS_DECL_ISUPPORTS

  void SetParams(
    PK11SlotInfo *a_slot,
    PRUint32 a_keyGenMechanism,
    void *a_params,
    PRBool a_isPerm,
    PRBool a_isSensitive,
    void *a_wincx );

  nsresult GetParams(
    SECKEYPrivateKey **a_privateKey,
    SECKEYPublicKey **a_publicKey);
  
  void Join(void);

  void Run(void);
};

#endif //_NSKEYGENTHREAD_H_
