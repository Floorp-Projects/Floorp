/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGNOMERegistry.h"
#include "nsString.h"
#include "nsIComponentManager.h"
#include "nsMIMEInfoUnix.h"
#include "nsAutoPtr.h"
#include "nsIGIOService.h"

/* static */ bool
nsGNOMERegistry::HandlerExists(const char *aProtocolScheme)
{
  nsCOMPtr<nsIGIOService> giovfs = do_GetService(NS_GIOSERVICE_CONTRACTID);
  if (!giovfs) {
    return false;
  }

  nsCOMPtr<nsIHandlerApp> app;
  return NS_SUCCEEDED(giovfs->GetAppForURIScheme(nsDependentCString(aProtocolScheme),
                                                 getter_AddRefs(app)));
}

// XXX Check HandlerExists() before calling LoadURL.

/* static */ nsresult
nsGNOMERegistry::LoadURL(nsIURI *aURL)
{
  nsCOMPtr<nsIGIOService> giovfs = do_GetService(NS_GIOSERVICE_CONTRACTID);
  if (!giovfs) {
    return NS_ERROR_FAILURE;
  }

  return giovfs->ShowURI(aURL);
}

/* static */ void
nsGNOMERegistry::GetAppDescForScheme(const nsACString& aScheme,
                                     nsAString& aDesc)
{
  nsCOMPtr<nsIGIOService> giovfs = do_GetService(NS_GIOSERVICE_CONTRACTID);
  if (!giovfs)
    return;

  nsCOMPtr<nsIHandlerApp> app;
  if (NS_FAILED(giovfs->GetAppForURIScheme(aScheme, getter_AddRefs(app))))
    return;

  app->GetName(aDesc);
}


/* static */ already_AddRefed<nsMIMEInfoBase>
nsGNOMERegistry::GetFromExtension(const nsACString& aFileExt)
{
  nsAutoCString mimeType;
  nsCOMPtr<nsIGIOService> giovfs = do_GetService(NS_GIOSERVICE_CONTRACTID);
  if (!giovfs) {
    return nullptr;
  }

  // Get the MIME type from the extension, then call GetFromType to
  // fill in the MIMEInfo.
  if (NS_FAILED(giovfs->GetMimeTypeFromExtension(aFileExt, mimeType)) ||
      mimeType.EqualsLiteral("application/octet-stream")) {
    return nullptr;
  }

  RefPtr<nsMIMEInfoBase> mi = GetFromType(mimeType);
  if (mi) {
    mi->AppendExtension(aFileExt);
  }

  return mi.forget();
}

/* static */ already_AddRefed<nsMIMEInfoBase>
nsGNOMERegistry::GetFromType(const nsACString& aMIMEType)
{
  RefPtr<nsMIMEInfoUnix> mimeInfo = new nsMIMEInfoUnix(aMIMEType);
  NS_ENSURE_TRUE(mimeInfo, nullptr);

  nsAutoString name;
  nsAutoCString description;

  nsCOMPtr<nsIGIOService> giovfs = do_GetService(NS_GIOSERVICE_CONTRACTID);
  if (!giovfs) {
    return nullptr;
  }

  nsCOMPtr<nsIHandlerApp> handlerApp;
  if (NS_FAILED(giovfs->GetAppForMimeType(aMIMEType, getter_AddRefs(handlerApp))) ||
      !handlerApp) {
    return nullptr;
  }
  handlerApp->GetName(name);
  giovfs->GetDescriptionForMimeType(aMIMEType, description);

  mimeInfo->SetDefaultDescription(name);
  mimeInfo->SetPreferredAction(nsIMIMEInfo::useSystemDefault);
  mimeInfo->SetDescription(NS_ConvertUTF8toUTF16(description));

  return mimeInfo.forget();
}
