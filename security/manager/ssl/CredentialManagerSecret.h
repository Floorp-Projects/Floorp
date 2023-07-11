/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CredentialManagerSecret_h
#define CredentialManagerSecret_h

#include "OSKeyStore.h"
#include "nsString.h"

class CredentialManagerSecret final : public AbstractOSKeyStore {
 public:
  CredentialManagerSecret();

  virtual nsresult RetrieveSecret(const nsACString& label,
                                  /* out */ nsACString& secret) override;
  virtual nsresult StoreSecret(const nsACString& secret,
                               const nsACString& label) override;
  virtual nsresult DeleteSecret(const nsACString& label) override;

  virtual ~CredentialManagerSecret();
};

#endif  // CredentialManagerSecret_h
