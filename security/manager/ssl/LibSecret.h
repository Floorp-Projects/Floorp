/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LibSecret_h
#define LibSecret_h

#include "OSKeyStore.h"

#include "nsString.h"

nsresult MaybeLoadLibSecret();

class LibSecret final : public AbstractOSKeyStore {
 public:
  LibSecret();

  virtual nsresult RetrieveSecret(const nsACString& label,
                                  /* out */ nsACString& secret) override;
  virtual nsresult StoreSecret(const nsACString& secret,
                               const nsACString& label) override;
  virtual nsresult DeleteSecret(const nsACString& label) override;

  virtual ~LibSecret();
};

#endif  // LibSecret_h
