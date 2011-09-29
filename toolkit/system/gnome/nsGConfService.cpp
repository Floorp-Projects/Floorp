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
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>
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

#include "nsGConfService.h"
#include "nsStringAPI.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsISupportsPrimitives.h"
#include "nsIMutableArray.h"

#include <gconf/gconf-client.h>

nsGConfService::~nsGConfService()
{
  if (mClient)
    g_object_unref(mClient);
}

nsresult
nsGConfService::Init()
{
  mClient = gconf_client_get_default();
  return mClient ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMPL_ISUPPORTS1(nsGConfService, nsIGConfService)

NS_IMETHODIMP
nsGConfService::GetBool(const nsACString &aKey, bool *aResult)
{
  GError* error = nsnull;
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
  GError* error = nsnull;
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
nsGConfService::GetInt(const nsACString &aKey, PRInt32* aResult)
{
  GError* error = nsnull;
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
  GError* error = nsnull;
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
    
  GError* error = nsnull;
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
    items->AppendElement(obj, PR_FALSE);
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
                                     aValue, nsnull);

  return res ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGConfService::SetString(const nsACString &aKey, const nsACString &aValue)
{
  bool res = gconf_client_set_string(mClient, PromiseFlatCString(aKey).get(),
                                       PromiseFlatCString(aValue).get(),
                                       nsnull);

  return res ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGConfService::SetInt(const nsACString &aKey, PRInt32 aValue)
{
  bool res = gconf_client_set_int(mClient, PromiseFlatCString(aKey).get(),
                                    aValue, nsnull);

  return res ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGConfService::SetFloat(const nsACString &aKey, float aValue)
{
  bool res = gconf_client_set_float(mClient, PromiseFlatCString(aKey).get(),
                                      aValue, nsnull);

  return res ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGConfService::GetAppForProtocol(const nsACString &aScheme, bool *aEnabled,
                                  nsACString &aHandler)
{
  nsCAutoString key("/desktop/gnome/url-handlers/");
  key.Append(aScheme);
  key.Append("/command");

  GError *err = nsnull;
  gchar *command = gconf_client_get_string(mClient, key.get(), &err);
  if (!err && command) {
    key.Replace(key.Length() - 7, 7, NS_LITERAL_CSTRING("enabled"));
    *aEnabled = gconf_client_get_bool(mClient, key.get(), &err);
  } else {
    *aEnabled = PR_FALSE;
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
  nsCAutoString key("/desktop/gnome/url-handlers/");
  key.Append(aScheme);
  key.Append("/requires_terminal");

  GError *err = nsnull;
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
  nsCAutoString key("/desktop/gnome/url-handlers/");
  key.Append(aScheme);
  key.Append("/command");

  bool res = gconf_client_set_string(mClient, key.get(),
                                       PromiseFlatCString(aCommand).get(),
                                       nsnull);
  if (res) {
    key.Replace(key.Length() - 7, 7, NS_LITERAL_CSTRING("enabled"));
    res = gconf_client_set_bool(mClient, key.get(), PR_TRUE, nsnull);
    if (res) {
      key.Replace(key.Length() - 7, 7, NS_LITERAL_CSTRING("needs_terminal"));
      res = gconf_client_set_bool(mClient, key.get(), PR_FALSE, nsnull);
      if (res) {
        key.Replace(key.Length() - 14, 14, NS_LITERAL_CSTRING("command-id"));
        res = gconf_client_unset(mClient, key.get(), nsnull);
      }
    }
  }

  return res ? NS_OK : NS_ERROR_FAILURE;
}
