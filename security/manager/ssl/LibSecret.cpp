/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LibSecret.h"

#include <gio/gio.h>
#include <gmodule.h>
#include <memory>

#include "mozilla/Base64.h"
#include "mozilla/Logging.h"
#include "MainThreadUtils.h"
#include "prlink.h"

// This is the implementation of LibSecret, an instantiation of OSKeyStore for
// Linux.

using namespace mozilla;

LazyLogModule gLibSecretLog("libsecret");

static PRLibrary* libsecret = nullptr;

typedef struct _SecretService SecretService;
typedef struct _SecretCollection SecretCollection;

typedef enum {
  SECRET_SCHEMA_NONE = 0,
  SECRET_SCHEMA_DONT_MATCH_NAME = 1 << 1
} SecretSchemaFlags;

typedef enum {
  SECRET_SCHEMA_ATTRIBUTE_STRING = 0,
  SECRET_SCHEMA_ATTRIBUTE_INTEGER = 1,
  SECRET_SCHEMA_ATTRIBUTE_BOOLEAN = 2,
} SecretSchemaAttributeType;

typedef struct {
  const gchar* name;
  SecretSchemaAttributeType type;
} SecretSchemaAttribute;

typedef struct {
  const gchar* name;
  SecretSchemaFlags flags;
  SecretSchemaAttribute attributes[32];

  /* <private> */
  gint reserved;
  gpointer reserved1;
  gpointer reserved2;
  gpointer reserved3;
  gpointer reserved4;
  gpointer reserved5;
  gpointer reserved6;
  gpointer reserved7;
} SecretSchema;

typedef enum {
  SECRET_COLLECTION_NONE = 0 << 0,
  SECRET_COLLECTION_LOAD_ITEMS = 1 << 1,
} SecretCollectionFlags;

typedef enum {
  SECRET_SERVICE_NONE = 0,
  SECRET_SERVICE_OPEN_SESSION = 1 << 1,
  SECRET_SERVICE_LOAD_COLLECTIONS = 1 << 2,
} SecretServiceFlags;

typedef enum {
  SECRET_ERROR_PROTOCOL = 1,
  SECRET_ERROR_IS_LOCKED = 2,
  SECRET_ERROR_NO_SUCH_OBJECT = 3,
  SECRET_ERROR_ALREADY_EXISTS = 4,
} SecretError;

#define SECRET_COLLECTION_DEFAULT "default"

typedef SecretCollection* (*secret_collection_for_alias_sync_fn)(
    SecretService*, const gchar*, SecretCollectionFlags, GCancellable*,
    GError**);
typedef SecretService* (*secret_service_get_sync_fn)(SecretServiceFlags,
                                                     GCancellable*, GError**);
typedef gboolean (*secret_password_clear_sync_fn)(const SecretSchema*,
                                                  GCancellable*, GError**, ...);
typedef gchar* (*secret_password_lookup_sync_fn)(const SecretSchema*,
                                                 GCancellable*, GError**, ...);
typedef gboolean (*secret_password_store_sync_fn)(const SecretSchema*,
                                                  const gchar*, const gchar*,
                                                  const gchar*, GCancellable*,
                                                  GError**, ...);
typedef void (*secret_password_free_fn)(const gchar*);
typedef GQuark (*secret_error_get_quark_fn)();

static secret_collection_for_alias_sync_fn secret_collection_for_alias_sync =
    nullptr;
static secret_service_get_sync_fn secret_service_get_sync = nullptr;
static secret_password_clear_sync_fn secret_password_clear_sync = nullptr;
static secret_password_lookup_sync_fn secret_password_lookup_sync = nullptr;
static secret_password_store_sync_fn secret_password_store_sync = nullptr;
static secret_password_free_fn secret_password_free = nullptr;
static secret_error_get_quark_fn secret_error_get_quark = nullptr;

nsresult MaybeLoadLibSecret() {
  MOZ_ASSERT(NS_IsMainThread());
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  if (!libsecret) {
    libsecret = PR_LoadLibrary("libsecret-1.so.0");
    if (!libsecret) {
      return NS_ERROR_NOT_AVAILABLE;
    }

// With TSan, we cannot unload libsecret once we have loaded it because
// TSan does not support unloading libraries that are matched from its
// suppression list. Hence we just keep the library loaded in TSan builds.
#ifdef MOZ_TSAN
#  define UNLOAD_LIBSECRET(x) \
    do {                      \
    } while (0)
#else
#  define UNLOAD_LIBSECRET(x) PR_UnloadLibrary(x)
#endif

#define FIND_FUNCTION_SYMBOL(function)                                   \
  function = (function##_fn)PR_FindFunctionSymbol(libsecret, #function); \
  if (!(function)) {                                                     \
    UNLOAD_LIBSECRET(libsecret);                                         \
    libsecret = nullptr;                                                 \
    return NS_ERROR_NOT_AVAILABLE;                                       \
  }
    FIND_FUNCTION_SYMBOL(secret_collection_for_alias_sync);
    FIND_FUNCTION_SYMBOL(secret_service_get_sync);
    FIND_FUNCTION_SYMBOL(secret_password_clear_sync);
    FIND_FUNCTION_SYMBOL(secret_password_lookup_sync);
    FIND_FUNCTION_SYMBOL(secret_password_store_sync);
    FIND_FUNCTION_SYMBOL(secret_password_free);
    FIND_FUNCTION_SYMBOL(secret_error_get_quark);
#undef FIND_FUNCTION_SYMBOL
  }

  return NS_OK;
}

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

typedef std::unique_ptr<GError, ScopedMaybeDelete<GError>> ScopedGError;
typedef std::unique_ptr<GList, ScopedMaybeDelete<GList>> ScopedGList;
typedef std::unique_ptr<char, ScopedMaybeDelete<char>> ScopedPassword;
typedef std::unique_ptr<SecretCollection, ScopedMaybeDelete<SecretCollection>>
    ScopedSecretCollection;
typedef std::unique_ptr<SecretService, ScopedMaybeDelete<SecretService>>
    ScopedSecretService;

LibSecret::LibSecret() = default;

LibSecret::~LibSecret() {
  MOZ_ASSERT(NS_IsMainThread());
  if (!NS_IsMainThread()) {
    return;
  }
  if (libsecret) {
    secret_collection_for_alias_sync = nullptr;
    secret_service_get_sync = nullptr;
    secret_password_clear_sync = nullptr;
    secret_password_lookup_sync = nullptr;
    secret_password_store_sync = nullptr;
    secret_password_free = nullptr;
    secret_error_get_quark = nullptr;
    UNLOAD_LIBSECRET(libsecret);
    libsecret = nullptr;
  }
}

static const SecretSchema kSchema = {
    "mozilla.firefox",
    SECRET_SCHEMA_NONE,
    {{"string", SECRET_SCHEMA_ATTRIBUTE_STRING}, /* the label */
     {"NULL", SECRET_SCHEMA_ATTRIBUTE_STRING}}};

nsresult GetScopedServices(ScopedSecretService& aSs,
                           ScopedSecretCollection& aSc) {
  MOZ_ASSERT(secret_service_get_sync && secret_collection_for_alias_sync);
  if (!secret_service_get_sync || !secret_collection_for_alias_sync) {
    return NS_ERROR_FAILURE;
  }
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

nsresult LibSecret::StoreSecret(const nsACString& aSecret,
                                const nsACString& aLabel) {
  MOZ_ASSERT(secret_password_store_sync);
  if (!secret_password_store_sync) {
    return NS_ERROR_FAILURE;
  }
  // libsecret expects a null-terminated string, so to be safe we store the
  // secret (which could be arbitrary bytes) base64-encoded.
  nsAutoCString base64;
  nsresult rv = Base64Encode(aSecret, base64);
  if (NS_FAILED(rv)) {
    MOZ_LOG(gLibSecretLog, LogLevel::Debug, ("Error base64-encoding secret"));
    return rv;
  }
  GError* raw_error = nullptr;
  bool stored = secret_password_store_sync(
      &kSchema, SECRET_COLLECTION_DEFAULT, PromiseFlatCString(aLabel).get(),
      PromiseFlatCString(base64).get(),
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
  MOZ_ASSERT(secret_password_clear_sync && secret_error_get_quark);
  if (!secret_password_clear_sync || !secret_error_get_quark) {
    return NS_ERROR_FAILURE;
  }
  GError* raw_error = nullptr;
  Unused << secret_password_clear_sync(
      &kSchema,
      nullptr,  // GCancellable
      &raw_error, "string", PromiseFlatCString(aLabel).get(), nullptr);
  ScopedGError error(raw_error);
  if (raw_error && !(raw_error->domain == secret_error_get_quark() &&
                     raw_error->code == SECRET_ERROR_NO_SUCH_OBJECT)) {
    MOZ_LOG(gLibSecretLog, LogLevel::Debug, ("Error deleting secret"));
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult LibSecret::RetrieveSecret(const nsACString& aLabel,
                                   /* out */ nsACString& aSecret) {
  MOZ_ASSERT(secret_password_lookup_sync && secret_password_free);
  if (!secret_password_lookup_sync || !secret_password_free) {
    return NS_ERROR_FAILURE;
  }
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
  // libsecret expects a null-terminated string, so to be safe we store the
  // secret (which could be arbitrary bytes) base64-encoded, which means we have
  // to base64-decode it here.
  nsAutoCString base64Encoded(s.get());
  nsresult rv = Base64Decode(base64Encoded, aSecret);
  if (NS_FAILED(rv)) {
    MOZ_LOG(gLibSecretLog, LogLevel::Debug, ("Error base64-decoding secret"));
    return rv;
  }

  return NS_OK;
}
