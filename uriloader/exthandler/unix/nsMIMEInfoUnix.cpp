/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMIMEInfoUnix.h"
#include "nsGNOMERegistry.h"
#include "nsIGIOService.h"
#include "nsNetCID.h"
#include "nsIIOService.h"
#include "nsLocalFile.h"

#ifdef MOZ_ENABLE_DBUS
#  include "nsDBusHandlerApp.h"
#endif

nsresult nsMIMEInfoUnix::LoadUriInternal(nsIURI* aURI) {
  return nsGNOMERegistry::LoadURL(aURI);
}

NS_IMETHODIMP nsMIMEInfoUnix::GetDefaultExecutable(nsIFile** aExecutable) {
  // This needs to be implemented before FirefoxBridge will work on Linux.
  // To implement this and be consistent, GetHasDefaultHandler and
  // LaunchDefaultWithFile should probably be made to be consistent.
  // Right now, they aren't. GetHasDefaultHandler reports true in cases
  // where calling LaunchDefaultWithFile will fail due to not finding the
  // right executable.

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMIMEInfoUnix::GetHasDefaultHandler(bool* _retval) {
  // if a default app is set, it means the application has been set from
  // either /etc/mailcap or ${HOME}/.mailcap, in which case we don't want to
  // give the GNOME answer.
  if (GetDefaultApplication()) {
    return nsMIMEInfoImpl::GetHasDefaultHandler(_retval);
  }

  *_retval = false;

  if (mClass == eProtocolInfo) {
    *_retval = nsGNOMERegistry::HandlerExists(mSchemeOrType.get());
  } else {
    RefPtr<nsMIMEInfoBase> mimeInfo =
        nsGNOMERegistry::GetFromType(mSchemeOrType);
    if (!mimeInfo) {
      nsAutoCString ext;
      nsresult rv = GetPrimaryExtension(ext);
      if (NS_SUCCEEDED(rv)) {
        mimeInfo = nsGNOMERegistry::GetFromExtension(ext);
      }
    }
    if (mimeInfo) *_retval = true;
  }

  if (*_retval) return NS_OK;

  return NS_OK;
}

nsresult nsMIMEInfoUnix::LaunchDefaultWithFile(nsIFile* aFile) {
  // if a default app is set, it means the application has been set from
  // either /etc/mailcap or ${HOME}/.mailcap, in which case we don't want to
  // give the GNOME answer.
  if (GetDefaultApplication()) {
    return nsMIMEInfoImpl::LaunchDefaultWithFile(aFile);
  }

  nsAutoCString nativePath;
  aFile->GetNativePath(nativePath);

  nsCOMPtr<nsIGIOService> giovfs = do_GetService(NS_GIOSERVICE_CONTRACTID);
  if (!giovfs) {
    return NS_ERROR_FAILURE;
  }

  // nsGIOMimeApp->Launch wants a URI string instead of local file
  nsresult rv;
  nsCOMPtr<nsIIOService> ioservice =
      do_GetService(NS_IOSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIURI> uri;
  rv = ioservice->NewFileURI(aFile, getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHandlerApp> app;
  if (NS_FAILED(
          giovfs->GetAppForMimeType(mSchemeOrType, getter_AddRefs(app))) ||
      !app) {
    return NS_ERROR_FILE_NOT_FOUND;
  }

  return app->LaunchWithURI(uri, nullptr);
}
