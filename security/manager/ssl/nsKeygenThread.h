/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NSKEYGENTHREAD_H_
#define _NSKEYGENTHREAD_H_

#include "keyhi.h"
#include "nspr.h"

#include "mozilla/Mutex.h"
#include "nsIKeygenThread.h"
#include "nsCOMPtr.h"

class nsIRunnable;

class nsKeygenThread : public nsIKeygenThread {
 private:
  mozilla::Mutex mutex;

  nsCOMPtr<nsIRunnable> mNotifyObserver;

  bool iAmRunning;
  bool keygenReady;
  bool statusDialogClosed;
  bool alreadyReceivedParams;

  SECKEYPrivateKey* privateKey;
  SECKEYPublicKey* publicKey;
  PK11SlotInfo* slot;
  PK11AttrFlags flags;
  PK11SlotInfo* altSlot;
  PK11AttrFlags altFlags;
  PK11SlotInfo* usedSlot;
  uint32_t keyGenMechanism;
  void* params;
  void* wincx;

  PRThread* threadHandle;

 protected:
  virtual ~nsKeygenThread();

 public:
  nsKeygenThread();

  NS_DECL_NSIKEYGENTHREAD
  NS_DECL_THREADSAFE_ISUPPORTS

  void SetParams(PK11SlotInfo* a_slot, PK11AttrFlags a_flags,
                 PK11SlotInfo* a_alternative_slot,
                 PK11AttrFlags a_alternative_flags, uint32_t a_keyGenMechanism,
                 void* a_params, void* a_wincx);

  nsresult ConsumeResult(PK11SlotInfo** a_used_slot,
                         SECKEYPrivateKey** a_privateKey,
                         SECKEYPublicKey** a_publicKey);

  void Join(void);

  void Run(void);
};

#endif  //_NSKEYGENTHREAD_H_
