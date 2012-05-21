/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if (MOZ_PLATFORM_MAEMO == 5) && defined (MOZ_ENABLE_GNOMEVFS)
#include <glib.h>
#include <hildon-uri.h>
#include <hildon-mime.h>
#include <libosso.h>
#endif
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

#if (MOZ_PLATFORM_MAEMO == 5) && defined (MOZ_ENABLE_GNOMEVFS)
  if (NS_FAILED(rv)){
    HildonURIAction *action = hildon_uri_get_default_action(mSchemeOrType.get(), nsnull);
    if (action) {
      nsCAutoString spec;
      aURI->GetAsciiSpec(spec);
      if (hildon_uri_open(spec.get(), action, nsnull))
        rv = NS_OK;
      hildon_uri_action_unref(action);
    }
  }
#endif

#ifdef MOZ_WIDGET_QT
  if (NS_FAILED(rv)) {
    nsCAutoString spec;
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
  *_retval = false;
  nsRefPtr<nsMIMEInfoBase> mimeInfo = nsGNOMERegistry::GetFromType(mSchemeOrType);
  if (!mimeInfo) {
    nsCAutoString ext;
    nsresult rv = GetPrimaryExtension(ext);
    if (NS_SUCCEEDED(rv)) {
      mimeInfo = nsGNOMERegistry::GetFromExtension(ext);
    }
  }
  if (mimeInfo)
    *_retval = true;

  if (*_retval)
    return NS_OK;

#if (MOZ_PLATFORM_MAEMO == 5) && defined (MOZ_ENABLE_GNOMEVFS)
  HildonURIAction *action = hildon_uri_get_default_action(mSchemeOrType.get(), nsnull);
  if (action) {
    *_retval = true;
    hildon_uri_action_unref(action);
    return NS_OK;
  }
#endif

#if defined(MOZ_ENABLE_CONTENTACTION)
  ContentAction::Action action = 
    ContentAction::Action::defaultActionForFile(QUrl(), QString(mSchemeOrType.get()));
  if (action.isValid()) {
    *_retval = true;
    return NS_OK;
  }
#endif

  // If we didn't find a VFS handler, fallback.
  return nsMIMEInfoImpl::GetHasDefaultHandler(_retval);
}

nsresult
nsMIMEInfoUnix::LaunchDefaultWithFile(nsIFile *aFile)
{
  nsCAutoString nativePath;
  aFile->GetNativePath(nativePath);

#if (MOZ_PLATFORM_MAEMO == 5) && defined (MOZ_ENABLE_GNOMEVFS)
  if(NS_SUCCEEDED(LaunchDefaultWithDBus(PromiseFlatCString(nativePath).get())))
    return NS_OK;
#endif

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
  nsCAutoString uriSpec;
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
    nsCAutoString type;
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

#if (MOZ_PLATFORM_MAEMO == 5) && defined (MOZ_ENABLE_GNOMEVFS)

/* This method tries to launch the associated default handler for the given 
 * mime/file via hildon specific APIs (in this case hildon_mime_open_file* 
 * which are essetially wrappers to the DBUS mime_open method). 
 */

nsresult
nsMIMEInfoUnix::LaunchDefaultWithDBus(const char *aFilePath)
{
  const PRInt32 kHILDON_SUCCESS = 1;
  PRInt32 result = 0;
  DBusError err;
  dbus_error_init(&err);
  
  DBusConnection *connection = dbus_bus_get(DBUS_BUS_SESSION, &err);
  if (dbus_error_is_set(&err)) {
    dbus_error_free(&err);
    return NS_ERROR_FAILURE;
  }

  if (nsnull == connection)
    return NS_ERROR_FAILURE;

  result = hildon_mime_open_file_with_mime_type(connection,
                                                aFilePath,
                                                mSchemeOrType.get());
  if (result != kHILDON_SUCCESS)
    if (hildon_mime_open_file(connection, aFilePath) != kHILDON_SUCCESS)
      return NS_ERROR_FAILURE;

  return NS_OK;
}

/* static */ bool
nsMIMEInfoUnix::HandlerExists(const char *aProtocolScheme)
{
  bool isEnabled = false;
  HildonURIAction *action = hildon_uri_get_default_action(aProtocolScheme, nsnull);
  if (action) {
    isEnabled = true;
    hildon_uri_action_unref(action);
  }
  return isEnabled;
}

NS_IMETHODIMP
nsMIMEInfoUnix::GetPossibleApplicationHandlers(nsIMutableArray ** aPossibleAppHandlers)
{
  if (!mPossibleApplications) {
    mPossibleApplications = do_CreateInstance(NS_ARRAY_CONTRACTID);

    if (!mPossibleApplications)
      return NS_ERROR_OUT_OF_MEMORY;

    GSList *actions = hildon_uri_get_actions(mSchemeOrType.get(), nsnull);
    GSList *actionsPtr = actions;
    while (actionsPtr) {
      HildonURIAction *action = (HildonURIAction*)actionsPtr->data;
      actionsPtr = actionsPtr->next;
      nsDBusHandlerApp* app = new nsDBusHandlerApp();
      if (!app){
        hildon_uri_free_actions(actions);
        return NS_ERROR_OUT_OF_MEMORY;
      }
      nsDependentCString method(hildon_uri_action_get_method(action));
      nsDependentCString key(hildon_uri_action_get_service(action));
      nsCString service, objpath, interface;
      app->SetMethod(method);
      app->SetName(NS_ConvertUTF8toUTF16(key));

      if (key.FindChar('.', 0) > 0) {
        service.Assign(key);
        objpath.Assign(NS_LITERAL_CSTRING("/")+ key);
        objpath.ReplaceChar('.', '/');
        interface.Assign(key);
      } else {
        service.Assign(NS_LITERAL_CSTRING("com.nokia.")+ key);
        objpath.Assign(NS_LITERAL_CSTRING("/com/nokia/")+ key);
        interface.Assign(NS_LITERAL_CSTRING("com.nokia.")+ key);
      }

      app->SetService(service);
      app->SetObjectPath(objpath);
      app->SetDBusInterface(interface);

      mPossibleApplications->AppendElement(app, false);
    }
    hildon_uri_free_actions(actions);
  }

  *aPossibleAppHandlers = mPossibleApplications;
  NS_ADDREF(*aPossibleAppHandlers);
  return NS_OK;
}
#endif

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

