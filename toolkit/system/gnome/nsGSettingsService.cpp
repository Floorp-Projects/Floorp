/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "nsGSettingsService.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsMemory.h"
#include "prlink.h"
#include "nsComponentManagerUtils.h"
#include "nsIMutableArray.h"
#include "nsISupportsPrimitives.h"

#include <glib.h>
#include <glib-object.h>

using namespace mozilla;

typedef struct _GSettings GSettings;
typedef struct _GVariantType GVariantType;
typedef struct _GVariant GVariant;

#ifndef G_VARIANT_TYPE_INT32
#  define G_VARIANT_TYPE_INT32 ((const GVariantType*)"i")
#  define G_VARIANT_TYPE_BOOLEAN ((const GVariantType*)"b")
#  define G_VARIANT_TYPE_STRING ((const GVariantType*)"s")
#  define G_VARIANT_TYPE_OBJECT_PATH ((const GVariantType*)"o")
#  define G_VARIANT_TYPE_SIGNATURE ((const GVariantType*)"g")
#endif
#ifndef G_VARIANT_TYPE_STRING_ARRAY
#  define G_VARIANT_TYPE_STRING_ARRAY ((const GVariantType*)"as")
#endif

#define GSETTINGS_FUNCTIONS                                                   \
  FUNC(g_settings_new, GSettings*, (const char* schema))                      \
  FUNC(g_settings_list_schemas, const char* const*, (void))                   \
  FUNC(g_settings_list_keys, char**, (GSettings * settings))                  \
  FUNC(g_settings_get_value, GVariant*,                                       \
       (GSettings * settings, const char* key))                               \
  FUNC(g_settings_set_value, gboolean,                                        \
       (GSettings * settings, const char* key, GVariant* value))              \
  FUNC(g_settings_range_check, gboolean,                                      \
       (GSettings * settings, const char* key, GVariant* value))              \
  FUNC(g_variant_get_int32, gint32, (GVariant * variant))                     \
  FUNC(g_variant_get_boolean, gboolean, (GVariant * variant))                 \
  FUNC(g_variant_get_string, const char*, (GVariant * value, gsize * length)) \
  FUNC(g_variant_get_strv, const char**, (GVariant * value, gsize * length))  \
  FUNC(g_variant_is_of_type, gboolean,                                        \
       (GVariant * value, const GVariantType* type))                          \
  FUNC(g_variant_new_int32, GVariant*, (gint32 value))                        \
  FUNC(g_variant_new_boolean, GVariant*, (gboolean value))                    \
  FUNC(g_variant_new_string, GVariant*, (const char* string))                 \
  FUNC(g_variant_unref, void, (GVariant * value))

#define FUNC(name, type, params)      \
  typedef type(*_##name##_fn) params; \
  static _##name##_fn _##name;

GSETTINGS_FUNCTIONS

#undef FUNC

#define g_settings_new _g_settings_new
#define g_settings_list_schemas _g_settings_list_schemas
#define g_settings_list_keys _g_settings_list_keys
#define g_settings_get_value _g_settings_get_value
#define g_settings_set_value _g_settings_set_value
#define g_settings_range_check _g_settings_range_check
#define g_variant_get_int32 _g_variant_get_int32
#define g_variant_get_boolean _g_variant_get_boolean
#define g_variant_get_string _g_variant_get_string
#define g_variant_get_strv _g_variant_get_strv
#define g_variant_is_of_type _g_variant_is_of_type
#define g_variant_new_int32 _g_variant_new_int32
#define g_variant_new_boolean _g_variant_new_boolean
#define g_variant_new_string _g_variant_new_string
#define g_variant_unref _g_variant_unref

static PRLibrary* gioLib = nullptr;

class nsGSettingsCollection final : public nsIGSettingsCollection {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGSETTINGSCOLLECTION

  explicit nsGSettingsCollection(GSettings* aSettings)
      : mSettings(aSettings), mKeys(nullptr) {}

 private:
  ~nsGSettingsCollection();

  bool KeyExists(const nsACString& aKey);
  bool SetValue(const nsACString& aKey, GVariant* aValue);

  GSettings* mSettings;
  char** mKeys;
};

nsGSettingsCollection::~nsGSettingsCollection() {
  g_strfreev(mKeys);
  g_object_unref(mSettings);
}

bool nsGSettingsCollection::KeyExists(const nsACString& aKey) {
  if (!mKeys) mKeys = g_settings_list_keys(mSettings);

  for (uint32_t i = 0; mKeys[i] != nullptr; i++) {
    if (aKey.Equals(mKeys[i])) return true;
  }

  return false;
}

bool nsGSettingsCollection::SetValue(const nsACString& aKey, GVariant* aValue) {
  if (!KeyExists(aKey) ||
      !g_settings_range_check(mSettings, PromiseFlatCString(aKey).get(),
                              aValue)) {
    g_variant_unref(aValue);
    return false;
  }

  return g_settings_set_value(mSettings, PromiseFlatCString(aKey).get(),
                              aValue);
}

NS_IMPL_ISUPPORTS(nsGSettingsCollection, nsIGSettingsCollection)

NS_IMETHODIMP
nsGSettingsCollection::SetString(const nsACString& aKey,
                                 const nsACString& aValue) {
  GVariant* value = g_variant_new_string(PromiseFlatCString(aValue).get());
  if (!value) return NS_ERROR_OUT_OF_MEMORY;

  bool res = SetValue(aKey, value);

  return res ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGSettingsCollection::SetBoolean(const nsACString& aKey, bool aValue) {
  GVariant* value = g_variant_new_boolean(aValue);
  if (!value) return NS_ERROR_OUT_OF_MEMORY;

  bool res = SetValue(aKey, value);

  return res ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGSettingsCollection::SetInt(const nsACString& aKey, int32_t aValue) {
  GVariant* value = g_variant_new_int32(aValue);
  if (!value) return NS_ERROR_OUT_OF_MEMORY;

  bool res = SetValue(aKey, value);

  return res ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGSettingsCollection::GetString(const nsACString& aKey, nsACString& aResult) {
  if (!KeyExists(aKey)) return NS_ERROR_INVALID_ARG;

  GVariant* value =
      g_settings_get_value(mSettings, PromiseFlatCString(aKey).get());
  if (!g_variant_is_of_type(value, G_VARIANT_TYPE_STRING) &&
      !g_variant_is_of_type(value, G_VARIANT_TYPE_OBJECT_PATH) &&
      !g_variant_is_of_type(value, G_VARIANT_TYPE_SIGNATURE)) {
    g_variant_unref(value);
    return NS_ERROR_FAILURE;
  }

  aResult.Assign(g_variant_get_string(value, nullptr));
  g_variant_unref(value);

  return NS_OK;
}

NS_IMETHODIMP
nsGSettingsCollection::GetBoolean(const nsACString& aKey, bool* aResult) {
  NS_ENSURE_ARG_POINTER(aResult);

  if (!KeyExists(aKey)) return NS_ERROR_INVALID_ARG;

  GVariant* value =
      g_settings_get_value(mSettings, PromiseFlatCString(aKey).get());
  if (!g_variant_is_of_type(value, G_VARIANT_TYPE_BOOLEAN)) {
    g_variant_unref(value);
    return NS_ERROR_FAILURE;
  }

  gboolean res = g_variant_get_boolean(value);
  *aResult = res ? true : false;
  g_variant_unref(value);

  return NS_OK;
}

NS_IMETHODIMP
nsGSettingsCollection::GetInt(const nsACString& aKey, int32_t* aResult) {
  NS_ENSURE_ARG_POINTER(aResult);

  if (!KeyExists(aKey)) return NS_ERROR_INVALID_ARG;

  GVariant* value =
      g_settings_get_value(mSettings, PromiseFlatCString(aKey).get());
  if (!g_variant_is_of_type(value, G_VARIANT_TYPE_INT32)) {
    g_variant_unref(value);
    return NS_ERROR_FAILURE;
  }

  *aResult = g_variant_get_int32(value);
  g_variant_unref(value);

  return NS_OK;
}

// These types are local to nsGSettingsService::Init, but ISO C++98 doesn't
// allow a template (ArrayLength) to be instantiated based on a local type.
// Boo-urns!
typedef void (*nsGSettingsFunc)();
struct nsGSettingsDynamicFunction {
  const char* functionName;
  nsGSettingsFunc* function;
};

NS_IMETHODIMP
nsGSettingsCollection::GetStringList(const nsACString& aKey,
                                     nsIArray** aResult) {
  if (!KeyExists(aKey)) return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsIMutableArray> items(do_CreateInstance(NS_ARRAY_CONTRACTID));
  if (!items) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  GVariant* value =
      g_settings_get_value(mSettings, PromiseFlatCString(aKey).get());

  if (!g_variant_is_of_type(value, G_VARIANT_TYPE_STRING_ARRAY)) {
    g_variant_unref(value);
    return NS_ERROR_FAILURE;
  }

  const gchar** gs_strings = g_variant_get_strv(value, nullptr);
  if (!gs_strings) {
    // empty array
    items.forget(aResult);
    g_variant_unref(value);
    return NS_OK;
  }

  const gchar** p_gs_strings = gs_strings;
  while (*p_gs_strings != nullptr) {
    nsCOMPtr<nsISupportsCString> obj(
        do_CreateInstance(NS_SUPPORTS_CSTRING_CONTRACTID));
    if (obj) {
      obj->SetData(nsDependentCString(*p_gs_strings));
      items->AppendElement(obj);
    }
    p_gs_strings++;
  }
  g_free(gs_strings);
  items.forget(aResult);
  g_variant_unref(value);
  return NS_OK;
}

nsresult nsGSettingsService::Init() {
#define FUNC(name, type, params) {#name, (nsGSettingsFunc*)&_##name},
  static const nsGSettingsDynamicFunction kGSettingsSymbols[] = {
      GSETTINGS_FUNCTIONS};
#undef FUNC

  if (!gioLib) {
    gioLib = PR_LoadLibrary("libgio-2.0.so.0");
    if (!gioLib) return NS_ERROR_FAILURE;
  }

  for (auto GSettingsSymbol : kGSettingsSymbols) {
    *GSettingsSymbol.function =
        PR_FindFunctionSymbol(gioLib, GSettingsSymbol.functionName);
    if (!*GSettingsSymbol.function) {
      return NS_ERROR_FAILURE;
    }
  }

  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsGSettingsService, nsIGSettingsService)

nsGSettingsService::~nsGSettingsService() {
  if (gioLib) {
    PR_UnloadLibrary(gioLib);
    gioLib = nullptr;
  }
}

NS_IMETHODIMP
nsGSettingsService::GetCollectionForSchema(
    const nsACString& schema, nsIGSettingsCollection** collection) {
  NS_ENSURE_ARG_POINTER(collection);

  const char* const* schemas = g_settings_list_schemas();

  for (uint32_t i = 0; schemas[i] != nullptr; i++) {
    if (schema.Equals(schemas[i])) {
      GSettings* settings = g_settings_new(PromiseFlatCString(schema).get());
      nsGSettingsCollection* mozGSettings = new nsGSettingsCollection(settings);
      NS_ADDREF(*collection = mozGSettings);
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}
