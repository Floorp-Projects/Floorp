/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGnomeVFSService.h"
#include "nsStringAPI.h"
#include "nsIURI.h"
#include "nsTArray.h"
#include "nsIStringEnumerator.h"
#include "nsAutoPtr.h"

extern "C" {
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
}

class nsGnomeVFSMimeApp MOZ_FINAL : public nsIGnomeVFSMimeApp
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
nsGnomeVFSMimeApp::GetCanOpenMultipleFiles(bool* aCanOpen)
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
nsGnomeVFSMimeApp::Launch(const nsACString &aUri)
{
  char *uri = gnome_vfs_make_uri_from_input(PromiseFlatCString(aUri).get());

  if (! uri)
    return NS_ERROR_FAILURE;

  GList uris = { 0 };
  uris.data = uri;

  GnomeVFSResult result = gnome_vfs_mime_application_launch(mApp, &uris);
  g_free(uri);

  if (result != GNOME_VFS_OK)
    return NS_ERROR_FAILURE;

  return NS_OK;
}

class UTF8StringEnumerator MOZ_FINAL : public nsIUTF8StringEnumerator
{
public:
  UTF8StringEnumerator() : mIndex(0) { }
  ~UTF8StringEnumerator() { }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIUTF8STRINGENUMERATOR

  nsTArray<nsCString> mStrings;
  PRUint32            mIndex;
};

NS_IMPL_ISUPPORTS1(UTF8StringEnumerator, nsIUTF8StringEnumerator)

NS_IMETHODIMP
UTF8StringEnumerator::HasMore(bool *aResult)
{
  *aResult = mIndex < mStrings.Length();
  return NS_OK;
}

NS_IMETHODIMP
UTF8StringEnumerator::GetNext(nsACString& aResult)
{
  if (mIndex >= mStrings.Length())
    return NS_ERROR_UNEXPECTED;

  aResult.Assign(mStrings[mIndex]);
  ++mIndex;
  return NS_OK;
}

NS_IMETHODIMP
nsGnomeVFSMimeApp::GetSupportedURISchemes(nsIUTF8StringEnumerator** aSchemes)
{
  *aSchemes = nullptr;

  nsRefPtr<UTF8StringEnumerator> array = new UTF8StringEnumerator();
  NS_ENSURE_TRUE(array, NS_ERROR_OUT_OF_MEMORY);

  for (GList *list = mApp->supported_uri_schemes; list; list = list->next) {
    if (!array->mStrings.AppendElement((char*) list->data)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  NS_ADDREF(*aSchemes = array);
  return NS_OK;
}

NS_IMETHODIMP
nsGnomeVFSMimeApp::GetRequiresTerminal(bool* aRequires)
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
  nsCAutoString fileExtToUse(".");
  fileExtToUse.Append(aExtension);

  const char *mimeType = gnome_vfs_mime_type_from_name(fileExtToUse.get());
  aMimeType.Assign(mimeType);

  // |mimeType| points to internal gnome-vfs data, so don't free it.

  return NS_OK;
}

NS_IMETHODIMP
nsGnomeVFSService::GetAppForMimeType(const nsACString &aMimeType,
                                     nsIGnomeVFSMimeApp** aApp)
{
  *aApp = nullptr;
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

  if (gnome_vfs_url_show_with_env(spec.get(), NULL) == GNOME_VFS_OK)
    return NS_OK;

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGnomeVFSService::ShowURIForInput(const nsACString &aUri)
{
  char* spec = gnome_vfs_make_uri_from_input(PromiseFlatCString(aUri).get());
  nsresult rv = NS_ERROR_FAILURE;

  if (gnome_vfs_url_show_with_env(spec, NULL) == GNOME_VFS_OK)
    rv = NS_OK;

  g_free(spec);
  return rv;
}
