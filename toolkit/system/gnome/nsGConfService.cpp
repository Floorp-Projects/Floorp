/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"
#include "nsGConfService.h"
#include "nsStringAPI.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsISupportsPrimitives.h"
#include "nsIMutableArray.h"
#include "prlink.h"

#include <gconf/gconf-client.h>

using namespace mozilla;

#define GCONF_FUNCTIONS \
  FUNC(gconf_client_get_default, GConfClient*, (void)) \
  FUNC(gconf_client_get_bool, gboolean, (GConfClient*, const gchar*, GError**)) \
  FUNC(gconf_client_get_string, gchar*, (GConfClient*, const gchar*, GError**)) \
  FUNC(gconf_client_get_int, gint, (GConfClient*, const gchar*, GError**)) \
  FUNC(gconf_client_get_float, gdouble, (GConfClient*, const gchar*, GError**)) \
  FUNC(gconf_client_get_list, GSList*, (GConfClient*, const gchar*, GConfValueType, GError**)) \
  FUNC(gconf_client_set_bool, gboolean, (GConfClient*, const gchar*, gboolean, GError**)) \
  FUNC(gconf_client_set_string, gboolean, (GConfClient*, const gchar*, const gchar*, GError**)) \
  FUNC(gconf_client_set_int, gboolean, (GConfClient*, const gchar*, gint, GError**)) \
  FUNC(gconf_client_set_float, gboolean, (GConfClient*, const gchar*, gdouble, GError**)) \
  FUNC(gconf_client_unset, gboolean, (GConfClient*, const gchar*, GError**))

#define FUNC(name, type, params) \
  typedef type (*_##name##_fn) params; \
  static _##name##_fn _##name;

GCONF_FUNCTIONS

#undef FUNC

#define gconf_client_get_default _gconf_client_get_default
#define gconf_client_get_bool _gconf_client_get_bool
#define gconf_client_get_string _gconf_client_get_string
#define gconf_client_get_int _gconf_client_get_int
#define gconf_client_get_float _gconf_client_get_float
#define gconf_client_get_list _gconf_client_get_list
#define gconf_client_set_bool _gconf_client_set_bool
#define gconf_client_set_string _gconf_client_set_string
#define gconf_client_set_int _gconf_client_set_int
#define gconf_client_set_float _gconf_client_set_float
#define gconf_client_unset _gconf_client_unset

static PRLibrary *gconfLib = nullptr;

typedef void (*nsGConfFunc)();
struct nsGConfDynamicFunction {
  const char *functionName;
  nsGConfFunc *function;
};

nsGConfService::~nsGConfService()
{
  if (mClient)
    g_object_unref(mClient);

  // We don't unload gconf here because liborbit uses atexit(). In addition to
  // this, it's not a good idea to unload any gobject based library, as it
  // leaves types registered in glib's type system
}

nsresult
nsGConfService::Init()
{
#define FUNC(name, type, params) { #name, (nsGConfFunc *)&_##name },
  static const nsGConfDynamicFunction kGConfSymbols[] = {
    GCONF_FUNCTIONS
  };
#undef FUNC

  if (!gconfLib) {
    gconfLib = PR_LoadLibrary("libgconf-2.so.4");
    if (!gconfLib)
      return NS_ERROR_FAILURE;
  }

  for (uint32_t i = 0; i < ArrayLength(kGConfSymbols); i++) {
    *kGConfSymbols[i].function =
      PR_FindFunctionSymbol(gconfLib, kGConfSymbols[i].functionName);
    if (!*kGConfSymbols[i].function) {
      return NS_ERROR_FAILURE;
    }
  }

  mClient = gconf_client_get_default();
  return mClient ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMPL_ISUPPORTS1(nsGConfService, nsIGConfService)

NS_IMETHODIMP
nsGConfService::GetBool(const nsACString &aKey, bool *aResult)
{
  GError* error = nullptr;
  *aResult = gconf_client_get_bool(mClient, PromiseFlatCString(aKey).get(),
                                   &error);

  if (error) {
    g_error_free(error);
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGConfService::GetString(const nsACString &aKey, nsACString &aResult)
{
  GError* error = nullptr;
  gchar *result = gconf_client_get_string(mClient,
                                          PromiseFlatCString(aKey).get(),
                                          &error);

  if (error) {
    g_error_free(error);
    return NS_ERROR_FAILURE;
  }

  // We do a string copy here so that the caller doesn't need to worry about
  // freeing the string with g_free().

  aResult.Assign(result);
  g_free(result);

  return NS_OK;
}

NS_IMETHODIMP
nsGConfService::GetInt(const nsACString &aKey, int32_t* aResult)
{
  GError* error = nullptr;
  *aResult = gconf_client_get_int(mClient, PromiseFlatCString(aKey).get(),
                                  &error);

  if (error) {
    g_error_free(error);
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGConfService::GetFloat(const nsACString &aKey, float* aResult)
{
  GError* error = nullptr;
  *aResult = gconf_client_get_float(mClient, PromiseFlatCString(aKey).get(),
                                    &error);

  if (error) {
    g_error_free(error);
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGConfService::GetStringList(const nsACString &aKey, nsIArray** aResult)
{
  nsCOMPtr<nsIMutableArray> items(do_CreateInstance(NS_ARRAY_CONTRACTID));
  if (!items)
    return NS_ERROR_OUT_OF_MEMORY;
    
  GError* error = nullptr;
  GSList* list = gconf_client_get_list(mClient, PromiseFlatCString(aKey).get(),
                                       GCONF_VALUE_STRING, &error);
  if (error) {
    g_error_free(error);
    return NS_ERROR_FAILURE;
  }

  for (GSList* l = list; l; l = l->next) {
    nsCOMPtr<nsISupportsString> obj(do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID));
    if (!obj) {
      g_slist_free(list);
      return NS_ERROR_OUT_OF_MEMORY;
    }
    obj->SetData(NS_ConvertUTF8toUTF16((const char*)l->data));
    items->AppendElement(obj, false);
    g_free(l->data);
  }
  
  g_slist_free(list);
  NS_ADDREF(*aResult = items);
  return NS_OK;
}

NS_IMETHODIMP
nsGConfService::SetBool(const nsACString &aKey, bool aValue)
{
  bool res = gconf_client_set_bool(mClient, PromiseFlatCString(aKey).get(),
                                     aValue, nullptr);

  return res ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGConfService::SetString(const nsACString &aKey, const nsACString &aValue)
{
  bool res = gconf_client_set_string(mClient, PromiseFlatCString(aKey).get(),
                                       PromiseFlatCString(aValue).get(),
                                       nullptr);

  return res ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGConfService::SetInt(const nsACString &aKey, int32_t aValue)
{
  bool res = gconf_client_set_int(mClient, PromiseFlatCString(aKey).get(),
                                    aValue, nullptr);

  return res ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGConfService::SetFloat(const nsACString &aKey, float aValue)
{
  bool res = gconf_client_set_float(mClient, PromiseFlatCString(aKey).get(),
                                      aValue, nullptr);

  return res ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGConfService::GetAppForProtocol(const nsACString &aScheme, bool *aEnabled,
                                  nsACString &aHandler)
{
  nsAutoCString key("/desktop/gnome/url-handlers/");
  key.Append(aScheme);
  key.Append("/command");

  GError *err = nullptr;
  gchar *command = gconf_client_get_string(mClient, key.get(), &err);
  if (!err && command) {
    key.Replace(key.Length() - 7, 7, NS_LITERAL_CSTRING("enabled"));
    *aEnabled = gconf_client_get_bool(mClient, key.get(), &err);
  } else {
    *aEnabled = false;
  }

  aHandler.Assign(command);
  g_free(command);

  if (err) {
    g_error_free(err);
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGConfService::HandlerRequiresTerminal(const nsACString &aScheme,
                                        bool *aResult)
{
  nsAutoCString key("/desktop/gnome/url-handlers/");
  key.Append(aScheme);
  key.Append("/requires_terminal");

  GError *err = nullptr;
  *aResult = gconf_client_get_bool(mClient, key.get(), &err);
  if (err) {
    g_error_free(err);
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGConfService::SetAppForProtocol(const nsACString &aScheme,
                                  const nsACString &aCommand)
{
  nsAutoCString key("/desktop/gnome/url-handlers/");
  key.Append(aScheme);
  key.Append("/command");

  bool res = gconf_client_set_string(mClient, key.get(),
                                       PromiseFlatCString(aCommand).get(),
                                       nullptr);
  if (res) {
    key.Replace(key.Length() - 7, 7, NS_LITERAL_CSTRING("enabled"));
    res = gconf_client_set_bool(mClient, key.get(), true, nullptr);
    if (res) {
      key.Replace(key.Length() - 7, 7, NS_LITERAL_CSTRING("needs_terminal"));
      res = gconf_client_set_bool(mClient, key.get(), false, nullptr);
      if (res) {
        key.Replace(key.Length() - 14, 14, NS_LITERAL_CSTRING("command-id"));
        res = gconf_client_unset(mClient, key.get(), nullptr);
      }
    }
  }

  return res ? NS_OK : NS_ERROR_FAILURE;
}
