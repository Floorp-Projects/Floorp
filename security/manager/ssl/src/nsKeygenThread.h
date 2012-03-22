/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
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

#ifndef _NSKEYGENTHREAD_H_
#define _NSKEYGENTHREAD_H_

#include "keyhi.h"
#include "nspr.h"

#include "mozilla/Mutex.h"
#include "nsIKeygenThread.h"
#include "nsCOMPtr.h"

class nsIRunnable;

class nsKeygenThread : public nsIKeygenThread
{
private:
  mozilla::Mutex mutex;
  
  nsCOMPtr<nsIRunnable> mNotifyObserver;

  bool iAmRunning;
  bool keygenReady;
  bool statusDialogClosed;
  bool alreadyReceivedParams;

  SECKEYPrivateKey *privateKey;
  SECKEYPublicKey *publicKey;
  PK11SlotInfo *slot;
  PK11AttrFlags flags;
  PK11SlotInfo *altSlot;
  PK11AttrFlags altFlags;
  PK11SlotInfo *usedSlot;
  PRUint32 keyGenMechanism;
  void *params;
  void *wincx;

  PRThread *threadHandle;
  
public:
  nsKeygenThread();
  virtual ~nsKeygenThread();
  
  NS_DECL_NSIKEYGENTHREAD
  NS_DECL_ISUPPORTS

  void SetParams(
    PK11SlotInfo *a_slot,
    PK11AttrFlags a_flags,
    PK11SlotInfo *a_alternative_slot,
    PK11AttrFlags a_alternative_flags,
    PRUint32 a_keyGenMechanism,
    void *a_params,
    void *a_wincx );

  nsresult ConsumeResult(
    PK11SlotInfo **a_used_slot,
    SECKEYPrivateKey **a_privateKey,
    SECKEYPublicKey **a_publicKey);
  
  void Join(void);

  void Run(void);
};

#endif //_NSKEYGENTHREAD_H_
