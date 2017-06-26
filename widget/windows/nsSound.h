/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsSound_h__
#define __nsSound_h__

#include "nsISound.h"
#include "nsIObserver.h"
#include "nsIStreamLoader.h"
#include "nsCOMPtr.h"
#include "mozilla/StaticPtr.h"

class nsIThread;

class nsSound : public nsISound
              , public nsIStreamLoaderObserver
              , public nsIObserver

{
public:
  nsSound();
  static already_AddRefed<nsISound> GetInstance();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISOUND
  NS_DECL_NSISTREAMLOADEROBSERVER
  NS_DECL_NSIOBSERVER

private:
  virtual ~nsSound();
  void PurgeLastSound();

private:
  nsresult CreatePlayerThread();

  nsCOMPtr<nsIThread> mPlayerThread;
  bool mInited;

  static mozilla::StaticRefPtr<nsISound> sInstance;
};

#endif /* __nsSound_h__ */
