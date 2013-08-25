/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_WIDGET_QT
#include <QDesktopServices>
#include <QUrl>
#include <QString>
#if (MOZ_ENABLE_CONTENTACTION)
#include <contentaction/contentaction.h>
#include "nsContentHandlerApp.h"
#endif
#endif

#include "nsMIMEInfoUnix.h"
#include "nsGNOMERegistry.h"
#include "nsIGIOService.h"
#include "nsNetCID.h"
#include "nsIIOService.h"
#include "nsIGnomeVFSService.h"
#include "nsAutoPtr.h"
#ifdef MOZ_ENABLE_DBUS
#include "nsDBusHandlerApp.h"
#endif

nsresult
nsMIMEInfoUnix::LoadUriInternal(nsIURI * aURI)
{
  nsresult rv = nsGNOMERegistry::LoadURL(aURI);

#ifdef MOZ_WIDGET_QT
  if (NS_FAILED(rv)) {
    nsAutoCString spec;
    aURI->GetAsciiSpec(spec);
    if (QDesktopServices::openUrl(QUrl(spec.get()))) {
      rv = NS_OK;
    }
  }
#endif

  return rv;
}

NS_IMETHODIMP
nsMIMEInfoUnix::GetHasDefaultHandler(bool *_retval)
{
  // if mDefaultApplication is set, it means the application has been set from
  // either /etc/mailcap or ${HOME}/.mailcap, in which case we don't want to
  // give the GNOME answer.
  if (mDefaultApplication)
    return nsMIMEInfoImpl::GetHasDefaultHandler(_retval);

  *_retval = false;
  nsRefPtr<nsMIMEInfoBase> mimeInfo = nsGNOMERegistry::GetFromType(mSchemeOrType);
  if (!mimeInfo) {
    nsAutoCString ext;
    nsresult rv = GetPrimaryExtension(ext);
    if (NS_SUCCEEDED(rv)) {
      mimeInfo = nsGNOMERegistry::GetFromExtension(ext);
    }
  }
  if (mimeInfo)
    *_retval = true;

  if (*_retval)
    return NS_OK;

#if defined(MOZ_ENABLE_CONTENTACTION)
  ContentAction::Action action = 
    ContentAction::Action::defaultActionForFile(QUrl(), QString(mSchemeOrType.get()));
  if (action.isValid()) {
    *_retval = true;
    return NS_OK;
  }
#endif

  return NS_OK;
}

nsresult
nsMIMEInfoUnix::LaunchDefaultWithFile(nsIFile *aFile)
{
  // if mDefaultApplication is set, it means the application has been set from
  // either /etc/mailcap or ${HOME}/.mailcap, in which case we don't want to
  // give the GNOME answer.
  if (mDefaultApplication)
    return nsMIMEInfoImpl::LaunchDefaultWithFile(aFile);

  nsAutoCString nativePath;
  aFile->GetNativePath(nativePath);

#if defined(MOZ_ENABLE_CONTENTACTION)
  QUrl uri = QUrl::fromLocalFile(QString::fromUtf8(nativePath.get()));
  ContentAction::Action action =
    ContentAction::Action::defaultActionForFile(uri, QString(mSchemeOrType.get()));
  if (action.isValid()) {
    action.trigger();
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
#endif

  nsCOMPtr<nsIGIOService> giovfs = do_GetService(NS_GIOSERVICE_CONTRACTID);
  nsAutoCString uriSpec;
  if (giovfs) {
    // nsGIOMimeApp->Launch wants a URI string instead of local file
    nsresult rv;
    nsCOMPtr<nsIIOService> ioservice = do_GetService(NS_IOSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIURI> uri;
    rv = ioservice->NewFileURI(aFile, getter_AddRefs(uri));
    NS_ENSURE_SUCCESS(rv, rv);
    uri->GetSpec(uriSpec);
  }

  nsCOMPtr<nsIGnomeVFSService> gnomevfs = do_GetService(NS_GNOMEVFSSERVICE_CONTRACTID);
  if (giovfs) {
    nsCOMPtr<nsIGIOMimeApp> app;
    if (NS_SUCCEEDED(giovfs->GetAppForMimeType(mSchemeOrType, getter_AddRefs(app))) && app)
      return app->Launch(uriSpec);
  } else if (gnomevfs) {
    /* Fallback to GnomeVFS */
    nsCOMPtr<nsIGnomeVFSMimeApp> app;
    if (NS_SUCCEEDED(gnomevfs->GetAppForMimeType(mSchemeOrType, getter_AddRefs(app))) && app)
      return app->Launch(nativePath);
  }

  // If we haven't got an app we try to get a valid one by searching for the
  // extension mapped type
  nsRefPtr<nsMIMEInfoBase> mimeInfo = nsGNOMERegistry::GetFromExtension(nativePath);
  if (mimeInfo) {
    nsAutoCString type;
    mimeInfo->GetType(type);
    if (giovfs) {
      nsCOMPtr<nsIGIOMimeApp> app;
      if (NS_SUCCEEDED(giovfs->GetAppForMimeType(type, getter_AddRefs(app))) && app)
        return app->Launch(uriSpec);
    } else if (gnomevfs) {
      nsCOMPtr<nsIGnomeVFSMimeApp> app;
      if (NS_SUCCEEDED(gnomevfs->GetAppForMimeType(type, getter_AddRefs(app))) && app)
        return app->Launch(nativePath);
    }
  }

  if (!mDefaultApplication)
    return NS_ERROR_FILE_NOT_FOUND;

  return LaunchWithIProcess(mDefaultApplication, nativePath);
}

#if defined(MOZ_ENABLE_CONTENTACTION)
NS_IMETHODIMP
nsMIMEInfoUnix::GetPossibleApplicationHandlers(nsIMutableArray ** aPossibleAppHandlers)
{
  if (!mPossibleApplications) {
    mPossibleApplications = do_CreateInstance(NS_ARRAY_CONTRACTID);

    if (!mPossibleApplications)
      return NS_ERROR_OUT_OF_MEMORY;

    QList<ContentAction::Action> actions =
      ContentAction::Action::actionsForFile(QUrl(), QString(mSchemeOrType.get()));

    for (int i = 0; i < actions.size(); ++i) {
      nsContentHandlerApp* app =
        new nsContentHandlerApp(nsString((PRUnichar*)actions[i].name().data()), 
                                mSchemeOrType, actions[i]);
      mPossibleApplications->AppendElement(app, false);
    }
  }

  *aPossibleAppHandlers = mPossibleApplications;
  NS_ADDREF(*aPossibleAppHandlers);
  return NS_OK;
}
#endif

