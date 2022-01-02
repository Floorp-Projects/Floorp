/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef KeychainSecret_h
#define KeychainSecret_h

#include "CoreFoundation/CFBase.h"

#include "OSKeyStore.h"
#include "nsString.h"

template <typename T>
class ScopedCFType {
 public:
  explicit ScopedCFType(T value) : mValue(value) {}

  ~ScopedCFType() {
    if (mValue) {
      CFRelease((CFTypeRef)mValue);
    }
  }

  T get() { return mValue; }

  explicit operator bool() const { return mValue != nullptr; }

 private:
  T mValue;
};

class KeychainSecret final : public AbstractOSKeyStore {
 public:
  KeychainSecret();

  virtual nsresult RetrieveSecret(const nsACString& label,
                                  /* out */ nsACString& secret) override;
  virtual nsresult StoreSecret(const nsACString& secret,
                               const nsACString& label) override;
  virtual nsresult DeleteSecret(const nsACString& label) override;
  virtual nsresult Lock() override;
  virtual nsresult Unlock() override;

  virtual ~KeychainSecret();
};

#endif  // KeychainSecret_h
