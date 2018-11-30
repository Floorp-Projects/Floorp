/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_LIB_SECRET
#error LibSecret OSKeyStore included when MOZ_LIB_SECRET is not defined!
#endif

#ifndef LibSecret_h
#define LibSecret_h

#include "OSKeyStore.h"

#include <libsecret/secret.h>
#include <memory>
#include <vector>

#include "nsString.h"

struct ScopedDelete {
  void operator()(SecretService* ss) {
    if (ss) g_object_unref(ss);
  }
  void operator()(SecretCollection* sc) {
    if (sc) g_object_unref(sc);
  }
  void operator()(GError* error) {
    if (error) g_error_free(error);
  }
  void operator()(GList* list) {
    if (list) g_list_free(list);
  }
  void operator()(SecretValue* val) {
    if (val) secret_value_unref(val);
  }
  void operator()(SecretItem* val) {
    if (val) g_object_unref(val);
  }
  void operator()(char* val) {
    if (val) secret_password_free(val);
  }
};

template <class T>
struct ScopedMaybeDelete {
  void operator()(T* ptr) {
    if (ptr) {
      ScopedDelete del;
      del(ptr);
    }
  }
};

#define SCOPED(x) typedef std::unique_ptr<x, ScopedMaybeDelete<x>> Scoped##x

SCOPED(SecretService);
SCOPED(SecretCollection);
SCOPED(GError);
SCOPED(GList);
SCOPED(SecretValue);
SCOPED(SecretItem);
typedef std::unique_ptr<char, ScopedMaybeDelete<char>> ScopedPassword;

#undef SCOPED

class LibSecret final : public AbstractOSKeyStore {
 public:
  LibSecret();

  virtual nsresult RetrieveSecret(const nsACString& label,
                                  /* out */ nsACString& secret) override;
  virtual nsresult StoreSecret(const nsACString& secret,
                               const nsACString& label) override;
  virtual nsresult DeleteSecret(const nsACString& label) override;
  virtual nsresult Lock() override;
  virtual nsresult Unlock() override;

  virtual ~LibSecret();
};

#endif  // LibSecret_h
