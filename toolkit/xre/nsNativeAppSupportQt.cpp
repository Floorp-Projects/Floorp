/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 et sw=2 tw=80: */
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
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Oleg Romashin <romaxa@gmail.com>
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

#include <stdlib.h>
#include "nsNativeAppSupportBase.h"
#include "nsString.h"

#ifdef MOZ_ENABLE_LIBCONIC
#include <glib-object.h>
#endif

#if (MOZ_PLATFORM_MAEMO == 5)
#include <libosso.h>
#endif

class nsNativeAppSupportQt : public nsNativeAppSupportBase
{
public:
  NS_IMETHOD Start(PRBool* aRetVal);
  NS_IMETHOD Stop(PRBool* aResult);
#if (MOZ_PLATFORM_MAEMO == 5)
  // Osso context must be initialized for maemo5 otherwise we will be killed in ~20 seconds
  osso_context_t *m_osso_context;
#endif
};

NS_IMETHODIMP
nsNativeAppSupportQt::Start(PRBool* aRetVal)
{
  NS_ASSERTION(gAppData, "gAppData must not be null.");

  *aRetVal = PR_TRUE;
#ifdef MOZ_ENABLE_LIBCONIC
  g_type_init();
#endif

#if (MOZ_PLATFORM_MAEMO == 5)
  /* Initialize maemo application

     The initalization name will be of the form "Vendor.Name".
     If a Vendor isn't given, then we will just use "Name".

     Note that this value must match your X-Osso-Service name
     defined in your desktop file.  If it doesn't, the OSSO
     system will happily kill your process.
  */
  nsCAutoString applicationName;
  if (gAppData->vendor) {
      applicationName.Append(gAppData->vendor);
      applicationName.Append(".");
  }
  applicationName.Append(gAppData->name);
  ToLowerCase(applicationName);

  m_osso_context = osso_initialize(applicationName.get(),
                                   gAppData->version ? gAppData->version : "1.0",
                                   PR_TRUE,
                                   nsnull);

  /* Check that initilialization was ok */
  if (m_osso_context == nsnull) {
      return NS_ERROR_FAILURE;
  }
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsNativeAppSupportQt::Stop(PRBool* aResult)
{
  NS_ENSURE_ARG(aResult);
  *aResult = PR_TRUE;

#if (MOZ_PLATFORM_MAEMO == 5)
  if (m_osso_context) {
    osso_deinitialize(m_osso_context);
    m_osso_context = nsnull;
  }
#endif

  return NS_OK;
}

nsresult
NS_CreateNativeAppSupport(nsINativeAppSupport** aResult)
{
  nsNativeAppSupportBase* native = new nsNativeAppSupportQt();
  if (!native)
    return NS_ERROR_OUT_OF_MEMORY;

  *aResult = native;
  NS_ADDREF(*aResult);

  return NS_OK;
}
