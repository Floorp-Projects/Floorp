/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNSSU2FToken_h
#define nsNSSU2FToken_h

#include "nsINSSU2FToken.h"

#include "nsNSSShutDown.h"
#include "ScopedNSSTypes.h"

#define NS_NSSU2FTOKEN_CID \
  {0x79f95a6c, 0xd0f7, 0x4d7d, {0xae, 0xaa, 0xcd, 0x0a, 0x04, 0xb6, 0x50, 0x89}}

class nsNSSU2FToken : public nsINSSU2FToken,
                      public nsNSSShutDownObject
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSINSSU2FTOKEN

  nsNSSU2FToken();

  // For nsNSSShutDownObject
  virtual void virtualDestroyNSSReference() override;
  void destructorSafeDestroyNSSReference();

private:
  bool mInitialized;
  mozilla::UniquePK11SymKey mWrappingKey;

  static const nsCString mSecretNickname;
  static const nsString mVersion;

  ~nsNSSU2FToken();
  nsresult GetOrCreateWrappingKey(const mozilla::UniquePK11SlotInfo& aSlot,
                                  const nsNSSShutDownPreventionLock&);
};

#endif // nsNSSU2FToken_h
