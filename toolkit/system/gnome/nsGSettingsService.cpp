/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla GNOME integration code.
 *
 * The Initial Developer of the Original Code is
 * Canonical Ltd.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Chris Coulson <chris.coulson@canonical.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "mozilla/Util.h"

#include "nsGSettingsService.h"
#include "nsStringAPI.h"
#include "nsCOMPtr.h"
#include "nsMemory.h"
#include "prlink.h"
#include "nsComponentManagerUtils.h"

#include <glib.h>
#include <glib-object.h>

using namespace mozilla;

typedef struct _GSettings GSettings;
typedef struct _GVariantType GVariantType;
typedef struct _GVariant GVariant;

#ifndef G_VARIANT_TYPE_INT32
# define G_VARIANT_TYPE_INT32        ((const GVariantType *) "i")
# define G_VARIANT_TYPE_BOOLEAN      ((const GVariantType *) "b")
# define G_VARIANT_TYPE_STRING       ((const GVariantType *) "s")
# define G_VARIANT_TYPE_OBJECT_PATH  ((const GVariantType *) "o")
# define G_VARIANT_TYPE_SIGNATURE    ((const GVariantType *) "g")
#endif

#define GSETTINGS_FUNCTIONS \
  FUNC(g_settings_new, GSettings *, (const char* schema)) \
  FUNC(g_settings_list_schemas, const char * const *, (void)) \
  FUNC(g_settings_list_keys, char **, (GSettings* settings)) \
  FUNC(g_settings_get_value, GVariant *, (GSettings* settings, const char* key)) \
  FUNC(g_settings_set_value, gboolean, (GSettings* settings, const char* key, GVariant* value)) \
  FUNC(g_settings_range_check, gboolean, (GSettings* settings, const char* key, GVariant* value)) \
  FUNC(g_variant_get_int32, gint32, (GVariant* variant)) \
  FUNC(g_variant_get_boolean, gboolean, (GVariant* variant)) \
  FUNC(g_variant_get_string, const char *, (GVariant* value, gsize* length)) \
  FUNC(g_variant_is_of_type, gboolean, (GVariant* value, const GVariantType* type)) \
  FUNC(g_variant_new_int32, GVariant *, (gint32 value)) \
  FUNC(g_variant_new_boolean, GVariant *, (gboolean value)) \
  FUNC(g_variant_new_string, GVariant *, (const char* string)) \
  FUNC(g_variant_unref, void, (GVariant* value))

#define FUNC(name, type, params) \
  typedef type (*_##name##_fn) params; \
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
#define g_variant_is_of_type _g_variant_is_of_type
#define g_variant_new_int32 _g_variant_new_int32
#define g_variant_new_boolean _g_variant_new_boolean
#define g_variant_new_string _g_variant_new_string
#define g_variant_unref _g_variant_unref

static PRLibrary *gioLib = nsnull;

class nsGSettingsCollection : public nsIGSettingsCollection
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGSETTINGSCOLLECTION

  nsGSettingsCollection(GSettings* aSettings) : mSettings(aSettings),
                                                mKeys(NULL) {};
  ~nsGSettingsCollection();

private:
  bool KeyExists(const nsACString& aKey);
  bool SetValue(const nsACString& aKey,
                  GVariant *aValue);

  GSettings *mSettings;
  char **mKeys;
};

nsGSettingsCollection::~nsGSettingsCollection()
{
  g_strfreev(mKeys);
  g_object_unref(mSettings);
}

bool
nsGSettingsCollection::KeyExists(const nsACString& aKey)
{
  if (!mKeys)
    mKeys = g_settings_list_keys(mSettings);

  for (PRUint32 i = 0; mKeys[i] != NULL; i++) {
    if (aKey.Equals(mKeys[i]))
      return PR_TRUE;
  }

  return PR_FALSE;
}

bool
nsGSettingsCollection::SetValue(const nsACString& aKey,
                                GVariant *aValue)
{
  if (!KeyExists(aKey) ||
      !g_settings_range_check(mSettings,
                              PromiseFlatCString(aKey).get(),
                              aValue)) {
    g_variant_unref(aValue);
    return PR_FALSE;
  }

  return g_settings_set_value(mSettings,
                              PromiseFlatCString(aKey).get(),
                              aValue);
}

NS_IMPL_ISUPPORTS1(nsGSettingsCollection, nsIGSettingsCollection)

NS_IMETHODIMP
nsGSettingsCollection::SetString(const nsACString& aKey,
                                 const nsACString& aValue)
{
  GVariant *value = g_variant_new_string(PromiseFlatCString(aValue).get());
  if (!value)
    return NS_ERROR_OUT_OF_MEMORY;

  bool res = SetValue(aKey, value);

  return res ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGSettingsCollection::SetBoolean(const nsACString& aKey,
                                  bool aValue)
{
  GVariant *value = g_variant_new_boolean(aValue);
  if (!value)
    return NS_ERROR_OUT_OF_MEMORY;

  bool res = SetValue(aKey, value);

  return res ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGSettingsCollection::SetInt(const nsACString& aKey,
                              PRInt32 aValue)
{
  GVariant *value = g_variant_new_int32(aValue);
  if (!value)
    return NS_ERROR_OUT_OF_MEMORY;

  bool res = SetValue(aKey, value);

  return res ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGSettingsCollection::GetString(const nsACString& aKey,
                                 nsACString& aResult)
{
  if (!KeyExists(aKey))
    return NS_ERROR_INVALID_ARG;

  GVariant *value = g_settings_get_value(mSettings,
                                         PromiseFlatCString(aKey).get());
  if (!g_variant_is_of_type(value, G_VARIANT_TYPE_STRING) &&
      !g_variant_is_of_type(value, G_VARIANT_TYPE_OBJECT_PATH) &&
      !g_variant_is_of_type(value, G_VARIANT_TYPE_SIGNATURE)) {
    g_variant_unref(value);
    return NS_ERROR_FAILURE;
  }

  aResult.Assign(g_variant_get_string(value, NULL));
  g_variant_unref(value);

  return NS_OK;
}

NS_IMETHODIMP
nsGSettingsCollection::GetBoolean(const nsACString& aKey,
                                  bool* aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  if (!KeyExists(aKey))
    return NS_ERROR_INVALID_ARG;

  GVariant *value = g_settings_get_value(mSettings,
                                         PromiseFlatCString(aKey).get());
  if (!g_variant_is_of_type(value, G_VARIANT_TYPE_BOOLEAN)) {
    g_variant_unref(value);
    return NS_ERROR_FAILURE;
  }

  gboolean res = g_variant_get_boolean(value);
  *aResult = res ? PR_TRUE : PR_FALSE;
  g_variant_unref(value);

  return NS_OK;
}

NS_IMETHODIMP
nsGSettingsCollection::GetInt(const nsACString& aKey,
                              PRInt32* aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  if (!KeyExists(aKey))
    return NS_ERROR_INVALID_ARG;

  GVariant *value = g_settings_get_value(mSettings,
                                         PromiseFlatCString(aKey).get());
  if (!g_variant_is_of_type(value, G_VARIANT_TYPE_INT32)) {
    g_variant_unref(value);
    return NS_ERROR_FAILURE;
  }

  *aResult = g_variant_get_int32(value);
  g_variant_unref(value);

  return NS_OK;
}

nsresult
nsGSettingsService::Init()
{
#define FUNC(name, type, params) { #name, (nsGSettingsFunc *)&_##name },
  typedef void (*nsGSettingsFunc)();
  static const struct nsGSettingsDynamicFunction {
    const char *functionName;
    nsGSettingsFunc *function;
  } kGSettingsSymbols[] = {
    GSETTINGS_FUNCTIONS
  };
#undef FUNC

  if (!gioLib) {
    gioLib = PR_LoadLibrary("libgio-2.0.so.0");
    if (!gioLib)
      return NS_ERROR_FAILURE;
  }

  for (PRUint32 i = 0; i < ArrayLength(kGSettingsSymbols); i++) {
    *kGSettingsSymbols[i].function =
      PR_FindFunctionSymbol(gioLib, kGSettingsSymbols[i].functionName);
    if (!*kGSettingsSymbols[i].function) {
      PR_UnloadLibrary(gioLib);
      return NS_ERROR_FAILURE;
    }
  }

  return NS_OK;
}

NS_IMPL_ISUPPORTS1(nsGSettingsService, nsIGSettingsService)

nsGSettingsService::~nsGSettingsService()
{
  if (gioLib) {
    PR_UnloadLibrary(gioLib);
    gioLib = nsnull;
  }
}

NS_IMETHODIMP
nsGSettingsService::GetCollectionForSchema(const nsACString& schema,
                                           nsIGSettingsCollection** collection)
{
  NS_ENSURE_ARG_POINTER(collection);

  const char * const *schemas = g_settings_list_schemas();

  for (PRUint32 i = 0; schemas[i] != NULL; i++) {
    if (schema.Equals(schemas[i])) {
      GSettings *settings = g_settings_new(PromiseFlatCString(schema).get());
      nsGSettingsCollection *mozGSettings = new nsGSettingsCollection(settings);
      NS_ADDREF(*collection = mozGSettings);
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}
