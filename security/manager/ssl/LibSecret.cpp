/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LibSecret.h"

#include <gmodule.h>

#include "mozilla/Base64.h"

// This is the implementation of LibSecret, an instantiation of OSKeyStore for
// Linux.

using namespace mozilla;

LazyLogModule gLibSecretLog("libsecret");

LibSecret::LibSecret() {}

LibSecret::~LibSecret() {}

static const SecretSchema kSchema = {
    "mozilla.firefox",
    SECRET_SCHEMA_NONE,
    {{"string", SECRET_SCHEMA_ATTRIBUTE_STRING}, /* the label */
     {"NULL", SECRET_SCHEMA_ATTRIBUTE_STRING}}};

nsresult GetScopedServices(ScopedSecretService& aSs,
                           ScopedSecretCollection& aSc) {
  GError* raw_error = nullptr;
  aSs = ScopedSecretService(secret_service_get_sync(
      static_cast<SecretServiceFlags>(
          SECRET_SERVICE_OPEN_SESSION),  // SecretServiceFlags
      nullptr,                           // GCancellable
      &raw_error));
  ScopedGError error(raw_error);
  if (error || !aSs) {
    MOZ_LOG(gLibSecretLog, LogLevel::Debug, ("Couldn't get a secret service"));
    return NS_ERROR_FAILURE;
  }

  aSc = ScopedSecretCollection(secret_collection_for_alias_sync(
      aSs.get(), "default", static_cast<SecretCollectionFlags>(0),
      nullptr,  // GCancellable
      &raw_error));
  error.reset(raw_error);
  if (!aSc) {
    MOZ_LOG(gLibSecretLog, LogLevel::Debug,
            ("Couldn't get a secret collection"));
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult LibSecret::Lock() {
  ScopedSecretService ss;
  ScopedSecretCollection sc;
  if (NS_FAILED(GetScopedServices(ss, sc))) {
    return NS_ERROR_FAILURE;
  }

  GError* raw_error = nullptr;
  GList* collections = nullptr;
  ScopedGList collectionList(g_list_append(collections, sc.get()));
  int numLocked = secret_service_lock_sync(ss.get(), collectionList.get(),
                                           nullptr,  // GCancellable
                                           nullptr,  // list of locked items
                                           &raw_error);
  ScopedGError error(raw_error);
  if (numLocked != 1) {
    MOZ_LOG(gLibSecretLog, LogLevel::Debug,
            ("Couldn't lock secret collection"));
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult LibSecret::Unlock() {
  // Accessing the secret service unlocks it. So calling this separately isn't
  // actually necessary.
  ScopedSecretService ss;
  ScopedSecretCollection sc;
  if (NS_FAILED(GetScopedServices(ss, sc))) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult LibSecret::StoreSecret(const nsACString& aSecret,
                                const nsACString& aLabel) {
  GError* raw_error = nullptr;
  bool stored = secret_password_store_sync(
      &kSchema, SECRET_COLLECTION_DEFAULT, PromiseFlatCString(aLabel).get(),
      PromiseFlatCString(aSecret).get(),
      nullptr,  // GCancellable
      &raw_error, "string", PromiseFlatCString(aLabel).get(), nullptr);
  ScopedGError error(raw_error);
  if (raw_error) {
    MOZ_LOG(gLibSecretLog, LogLevel::Debug, ("Error storing secret"));
    return NS_ERROR_FAILURE;
  }

  return stored ? NS_OK : NS_ERROR_FAILURE;
}

nsresult LibSecret::DeleteSecret(const nsACString& aLabel) {
  GError* raw_error = nullptr;
  bool r = secret_password_clear_sync(
      &kSchema,
      nullptr,  // GCancellable
      &raw_error, "string", PromiseFlatCString(aLabel).get(), nullptr);
  ScopedGError error(raw_error);
  if (raw_error) {
    MOZ_LOG(gLibSecretLog, LogLevel::Debug, ("Error deleting secret"));
    return NS_ERROR_FAILURE;
  }

  return r ? NS_OK : NS_ERROR_FAILURE;
}

nsresult LibSecret::RetrieveSecret(const nsACString& aLabel,
                                   /* out */ nsACString& aSecret) {
  GError* raw_error = nullptr;
  aSecret.Truncate();
  ScopedPassword s(secret_password_lookup_sync(
      &kSchema,
      nullptr,  // GCancellable
      &raw_error, "string", PromiseFlatCString(aLabel).get(), nullptr));
  ScopedGError error(raw_error);
  if (raw_error || !s) {
    MOZ_LOG(gLibSecretLog, LogLevel::Debug,
            ("Error retrieving secret or didn't find it"));
    return NS_ERROR_FAILURE;
  }
  aSecret.Assign(s.get(), strlen(s.get()));

  return NS_OK;
}
