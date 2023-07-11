/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSSKeyStore_h
#define NSSKeyStore_h

#include "OSKeyStore.h"
#include "nsString.h"

class NSSKeyStore final : public AbstractOSKeyStore {
 public:
  NSSKeyStore();

  virtual nsresult RetrieveSecret(const nsACString& aLabel,
                                  /* out */ nsACString& aSecret) override;
  virtual nsresult StoreSecret(const nsACString& secret,
                               const nsACString& label) override;
  virtual nsresult DeleteSecret(const nsACString& label) override;
  virtual nsresult EncryptDecrypt(const nsACString& label,
                                  const std::vector<uint8_t>& inBytes,
                                  std::vector<uint8_t>& outBytes,
                                  bool encrypt) override;
  virtual bool SecretAvailable(const nsACString& label) override;
  virtual ~NSSKeyStore();

 private:
  nsresult InitToken();
  mozilla::UniquePK11SlotInfo mSlot = nullptr;
};

#endif  // NSSKeyStore_h
