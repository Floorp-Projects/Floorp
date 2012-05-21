/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsSound_h__
#define __nsSound_h__

#include "nsISound.h"
#include "nsIStreamLoader.h"

class nsSound : public nsISound,
                public nsIStreamLoaderObserver

{
public: 
  nsSound();
  virtual ~nsSound();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISOUND
  NS_DECL_NSISTREAMLOADEROBSERVER

protected:
  nsresult PlaySoundFile(const nsAString &aSoundFile);
  nsresult PlaySoundFile(const nsACString &aSoundFile);
};

#endif /* __nsSound_h__ */
