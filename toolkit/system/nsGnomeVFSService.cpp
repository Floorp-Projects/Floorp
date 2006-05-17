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

#include "nsGnomeVFSService.h"
#include "nsStringEnumerator.h"
#include "nsVoidArray.h"
#include "nsString.h"
#include "nsIURI.h"

extern "C" {
#include <libgnomevfs/gnome-vfs-application-registry.h>
#include <libgnomevfs/gnome-vfs-init.h>
#include <libgnomevfs/gnome-vfs-mime.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs-mime-info.h>
#include <libgnome/gnome-url.h>
}

class nsGnomeVFSMimeApp : public nsIGnomeVFSMimeApp
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGNOMEVFSMIMEAPP

  nsGnomeVFSMimeApp(GnomeVFSMimeApplication* aApp) : mApp(aApp) {}
  ~nsGnomeVFSMimeApp() { gnome_vfs_mime_application_free(mApp); }

private:
  GnomeVFSMimeApplication *mApp;
};

NS_IMPL_ISUPPORTS1(nsGnomeVFSMimeApp, nsIGnomeVFSMimeApp)

NS_IMETHODIMP
nsGnomeVFSMimeApp::GetId(nsACString& aId)
{
  aId.Assign(mApp->id);
  return NS_OK;
}

NS_IMETHODIMP
nsGnomeVFSMimeApp::GetName(nsACString& aName)
{
  aName.Assign(mApp->name);
  return NS_OK;
}

NS_IMETHODIMP
nsGnomeVFSMimeApp::GetCommand(nsACString& aCommand)
{
  aCommand.Assign(mApp->command);
  return NS_OK;
}

NS_IMETHODIMP
nsGnomeVFSMimeApp::GetCanOpenMultipleFiles(PRBool* aCanOpen)
{
  *aCanOpen = mApp->can_open_multiple_files;
  return NS_OK;
}

NS_IMETHODIMP
nsGnomeVFSMimeApp::GetExpectsURIs(PRInt32* aExpects)
{
  *aExpects = mApp->expects_uris;
  return NS_OK;
}

NS_IMETHODIMP
nsGnomeVFSMimeApp::GetSupportedURISchemes(nsIUTF8StringEnumerator** aSchemes)
{
  *aSchemes = nsnull;

  nsCStringArray *array = new nsCStringArray();
  NS_ENSURE_TRUE(array, NS_ERROR_OUT_OF_MEMORY);

  for (GList *list = mApp->supported_uri_schemes; list; list = list->next) {
    if (!array->AppendCString(nsDependentCString((char*) list->data))) {
      delete array;
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  return NS_NewAdoptingUTF8StringEnumerator(aSchemes, array);
}

NS_IMETHODIMP
nsGnomeVFSMimeApp::GetRequiresTerminal(PRBool* aRequires)
{
  *aRequires = mApp->requires_terminal;
  return NS_OK;
}

nsresult
nsGnomeVFSService::Init()
{
  return gnome_vfs_init() ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMPL_ISUPPORTS1(nsGnomeVFSService, nsIGnomeVFSService)

NS_IMETHODIMP
nsGnomeVFSService::GetMimeTypeFromExtension(const nsACString &aExtension,
                                            nsACString& aMimeType)
{
  nsCAutoString fileExtToUse(NS_LITERAL_CSTRING(".") + aExtension);

  const char *mimeType = gnome_vfs_mime_type_from_name(fileExtToUse.get());
  aMimeType.Assign(mimeType);

  // |mimeType| points to internal gnome-vfs data, so don't free it.

  return NS_OK;
}

NS_IMETHODIMP
nsGnomeVFSService::GetAppForMimeType(const nsACString &aMimeType,
                                     nsIGnomeVFSMimeApp** aApp)
{
  *aApp = nsnull;
  GnomeVFSMimeApplication *app =
   gnome_vfs_mime_get_default_application(PromiseFlatCString(aMimeType).get());

  if (app) {
    nsGnomeVFSMimeApp *mozApp = new nsGnomeVFSMimeApp(app);
    NS_ENSURE_TRUE(mozApp, NS_ERROR_OUT_OF_MEMORY);

    NS_ADDREF(*aApp = mozApp);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGnomeVFSService::SetAppForMimeType(const nsACString &aMimeType,
                                     const nsACString &aId)
{
  gnome_vfs_mime_set_default_application(PromiseFlatCString(aMimeType).get(),
                                         PromiseFlatCString(aId).get());
  return NS_OK;
}

NS_IMETHODIMP
nsGnomeVFSService::SetIconForMimeType(const nsACString &aMimeType,
                                      const nsACString &aIconName)
{
  gnome_vfs_mime_set_icon(PromiseFlatCString(aMimeType).get(),
                          PromiseFlatCString(aIconName).get());
  return NS_OK;
}

NS_IMETHODIMP
nsGnomeVFSService::GetDescriptionForMimeType(const nsACString &aMimeType,
                                             nsACString& aDescription)
{
  const char *desc =
    gnome_vfs_mime_get_description(PromiseFlatCString(aMimeType).get());
  aDescription.Assign(desc);

  // |desc| points to internal gnome-vfs data, so don't free it.

  return NS_OK;
}

NS_IMETHODIMP
nsGnomeVFSService::ShowURI(nsIURI *aURI)
{
  nsCAutoString spec;
  aURI->GetSpec(spec);

  if (gnome_url_show(spec.get(), NULL))
    return NS_OK;

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGnomeVFSService::SetAppStringKey(const nsACString &aID,
                                   PRInt32           aKey,
                                   const nsACString &aValue)
{
  const char *key;

  if (aKey == APP_KEY_COMMAND)
    key = GNOME_VFS_APPLICATION_REGISTRY_COMMAND;
  else if (aKey == APP_KEY_NAME)
    key = GNOME_VFS_APPLICATION_REGISTRY_NAME;
  else if (aKey == APP_KEY_SUPPORTED_URI_SCHEMES)
    key = "supported_uri_schemes";
  else if (aKey == APP_KEY_EXPECTS_URIS)
    key = "expects_uris";
  else
    return NS_ERROR_NOT_AVAILABLE;

  gnome_vfs_application_registry_set_value(PromiseFlatCString(aID).get(), key,
                                           PromiseFlatCString(aValue).get());
  return NS_OK;
}

NS_IMETHODIMP
nsGnomeVFSService::SetAppBoolKey(const nsACString &aID,
                                 PRInt32          aKey,
                                 PRBool           aValue)
{
  const char *key;

  if (aKey == APP_KEY_CAN_OPEN_MULTIPLE)
    key = GNOME_VFS_APPLICATION_REGISTRY_CAN_OPEN_MULTIPLE_FILES;
  else if (aKey == APP_KEY_REQUIRES_TERMINAL)
    key = GNOME_VFS_APPLICATION_REGISTRY_REQUIRES_TERMINAL;
  else
    return NS_ERROR_NOT_AVAILABLE;

  gnome_vfs_application_registry_set_bool_value(PromiseFlatCString(aID).get(),
                                                key, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsGnomeVFSService::AddMimeType(const nsACString &aID, const nsACString &aType)
{
  gnome_vfs_application_registry_add_mime_type(PromiseFlatCString(aID).get(),
                                              PromiseFlatCString(aType).get());
  return NS_OK;
}

NS_IMETHODIMP
nsGnomeVFSService::SyncAppRegistry()
{
  gnome_vfs_application_registry_sync();
  return NS_OK;
}

NS_IMETHODIMP
nsGnomeVFSService::SetMimeExtensions(const nsACString &aMimeType,
                                     const nsACString &aExtensionsList)
{
  GnomeVFSResult res =
    gnome_vfs_mime_set_extensions_list(PromiseFlatCString(aMimeType).get(),
                                    PromiseFlatCString(aExtensionsList).get());
  return (res == GNOME_VFS_OK) ? NS_OK : NS_ERROR_FAILURE;
}
