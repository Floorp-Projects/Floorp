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
 * The Original Code is the GNOME helper app implementation.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>  (Original Author)
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

#include "nsGNOMERegistry.h"
#include "prlink.h"
#include "prmem.h"
#include "nsString.h"
#include "nsIComponentManager.h"
#include "nsILocalFile.h"
#include "nsMIMEInfoImpl.h"
#include "nsAutoPtr.h"

#include <glib.h>
#include <glib-object.h>

static PRLibrary *gconfLib;
static PRLibrary *gnomeLib;
static PRLibrary *vfsLib;

typedef struct _GConfClient GConfClient;
typedef struct _GnomeProgram GnomeProgram;
typedef struct _GnomeModuleInfo GnomeModuleInfo;

typedef struct {
  char *id;
  char *name;
  char *command;
  /* there is more here, but we don't need it */
} GnomeVFSMimeApplication;

typedef GConfClient * (*_gconf_client_get_default_fn)();
typedef gchar * (*_gconf_client_get_string_fn)(GConfClient *,
					       const char *, GError **);
typedef gboolean (*_gconf_client_get_bool_fn)(GConfClient *,
                                              const char *, GError **);
typedef gboolean (*_gnome_url_show_fn)(const char *, GError **);
typedef const char * (*_gnome_vfs_mime_type_from_name_fn)(const char *);
typedef GList * (*_gnome_vfs_mime_get_extensions_list_fn)(const char *);
typedef void (*_gnome_vfs_mime_extensions_list_free_fn)(GList *);
typedef const char * (*_gnome_vfs_mime_get_description_fn)(const char *);
typedef GnomeVFSMimeApplication * (*_gnome_vfs_mime_get_default_application_fn)
                                                                (const char *);
typedef void (*_gnome_vfs_mime_application_free_fn)(GnomeVFSMimeApplication *);
typedef GnomeProgram * (*_gnome_program_init_fn)(const char *, const char *,
						 const GnomeModuleInfo *, int,
						 char **, const char *, ...);
typedef const GnomeModuleInfo * (*_libgnome_module_info_get_fn)();
typedef GnomeProgram * (*_gnome_program_get_fn)();

#define DECL_FUNC_PTR(func) static _##func##_fn _##func

DECL_FUNC_PTR(gconf_client_get_default);
DECL_FUNC_PTR(gconf_client_get_string);
DECL_FUNC_PTR(gconf_client_get_bool);
DECL_FUNC_PTR(gnome_url_show);
DECL_FUNC_PTR(gnome_vfs_mime_type_from_name);
DECL_FUNC_PTR(gnome_vfs_mime_get_extensions_list);
DECL_FUNC_PTR(gnome_vfs_mime_extensions_list_free);
DECL_FUNC_PTR(gnome_vfs_mime_get_description);
DECL_FUNC_PTR(gnome_vfs_mime_get_default_application);
DECL_FUNC_PTR(gnome_vfs_mime_application_free);
DECL_FUNC_PTR(gnome_program_init);
DECL_FUNC_PTR(libgnome_module_info_get);
DECL_FUNC_PTR(gnome_program_get);

static void
CleanUp()
{
  // Unload all libraries
  if (gnomeLib)
    PR_UnloadLibrary(gnomeLib);
  if (gconfLib)
    PR_UnloadLibrary(gconfLib);
  if (vfsLib)
    PR_UnloadLibrary(vfsLib);

  gnomeLib = gconfLib = vfsLib = nsnull;
}

static PRLibrary *
LoadVersionedLibrary(const char* libName, const char* libVersion)
{
  char *platformLibName = PR_GetLibraryName(nsnull, libName);
  nsCAutoString versionLibName(platformLibName);
  versionLibName.Append(libVersion);
  PR_Free(platformLibName);
  return PR_LoadLibrary(versionLibName.get());
}

/* static */ void
nsGNOMERegistry::Startup()
{
  #define ENSURE_LIB(lib) \
    PR_BEGIN_MACRO \
    if (!lib) { \
      CleanUp(); \
      return; \
    } \
    PR_END_MACRO

  #define GET_LIB_FUNCTION(lib, func) \
    PR_BEGIN_MACRO \
    _##func = (_##func##_fn) PR_FindFunctionSymbol(lib##Lib, #func); \
    if (!_##func) { \
      CleanUp(); \
      return; \
    } \
    PR_END_MACRO

  // Attempt to open libgconf
  gconfLib = LoadVersionedLibrary("gconf-2", ".4");
  ENSURE_LIB(gconfLib);

  GET_LIB_FUNCTION(gconf, gconf_client_get_default);
  GET_LIB_FUNCTION(gconf, gconf_client_get_string);
  GET_LIB_FUNCTION(gconf, gconf_client_get_bool);

  // Attempt to open libgnome
  gnomeLib = LoadVersionedLibrary("gnome-2", ".0");
  ENSURE_LIB(gnomeLib);

  GET_LIB_FUNCTION(gnome, gnome_url_show);
  GET_LIB_FUNCTION(gnome, gnome_program_init);
  GET_LIB_FUNCTION(gnome, libgnome_module_info_get);
  GET_LIB_FUNCTION(gnome, gnome_program_get);

  // Attempt to open libgnomevfs
  vfsLib = LoadVersionedLibrary("gnomevfs-2", ".0");
  ENSURE_LIB(vfsLib);

  GET_LIB_FUNCTION(vfs, gnome_vfs_mime_type_from_name);
  GET_LIB_FUNCTION(vfs, gnome_vfs_mime_get_extensions_list);
  GET_LIB_FUNCTION(vfs, gnome_vfs_mime_extensions_list_free);
  GET_LIB_FUNCTION(vfs, gnome_vfs_mime_get_description);
  GET_LIB_FUNCTION(vfs, gnome_vfs_mime_get_default_application);
  GET_LIB_FUNCTION(vfs, gnome_vfs_mime_application_free);

  // Initialize GNOME, if it's not already initialized.  It's not
  // necessary to tell GNOME about our actual command line arguments.

  if (!_gnome_program_get()) {
    char *argv[1] = { "gecko" };
    _gnome_program_init("Gecko", "1.0", _libgnome_module_info_get(),
                        1, argv, NULL);
  }

  // Note: after GNOME has been initialized, do not ever unload these
  // libraries.  They register atexit handlers, so if they are unloaded, we'll
  // crash on exit.
}

/**
 * Finds the application for a given protocol.
 *
 * @param aProtocolScheme
 *        Protocol to look up. For example, "ghelp" or "mailto".
 *
 * @return UTF-8 string identifying the application. Must be freed with
 *         g_free.
 *         NULL on error.
 */
static gchar* getAppForScheme(const nsACString& aProtocolScheme)
{
  if (!gconfLib)
    return nsnull;

  GConfClient *client = _gconf_client_get_default();
  NS_ASSERTION(client, "no gconf client");

  nsCAutoString gconfPath(NS_LITERAL_CSTRING("/desktop/gnome/url-handlers/") +
                          aProtocolScheme +
                          NS_LITERAL_CSTRING("/command"));

  gchar *app = _gconf_client_get_string(client, gconfPath.get(), NULL);
  g_object_unref(G_OBJECT(client));

  return app;
}

/* static */ PRBool
nsGNOMERegistry::HandlerExists(const char *aProtocolScheme)
{
  gchar *app = getAppForScheme(nsDependentCString(aProtocolScheme));

  if (app) {
    g_free(app);
    GConfClient *client = _gconf_client_get_default();

    nsCAutoString enabledPath(NS_LITERAL_CSTRING("/desktop/gnome/url-handlers/") +
                              nsDependentCString(aProtocolScheme) +
                              NS_LITERAL_CSTRING("/enabled"));
    gboolean isEnabled = _gconf_client_get_bool(client, enabledPath.get(), NULL);

    g_object_unref(G_OBJECT(client));

    return isEnabled ? PR_TRUE : PR_FALSE;
  }

  return PR_FALSE;
}

/* static */ nsresult
nsGNOMERegistry::LoadURL(nsIURI *aURL)
{
  if (!gconfLib)
    return NS_ERROR_FAILURE;

  nsCAutoString spec;
  aURL->GetAsciiSpec(spec);

  // XXX what if gnome_url_show() uses the default handler and that's us?

  if (_gnome_url_show(spec.get(), NULL))
    return NS_OK;

  return NS_ERROR_FAILURE;
}

/* static */ void
nsGNOMERegistry::GetAppDescForScheme(const nsACString& aScheme,
                                     nsAString& aDesc)
{
  char *app = getAppForScheme(aScheme);

  if (app) {
    CopyUTF8toUTF16(app, aDesc);
    g_free(app);
  }
}


/* static */ already_AddRefed<nsMIMEInfoBase>
nsGNOMERegistry::GetFromExtension(const char *aFileExt)
{
  if (!gconfLib)
    return nsnull;

  // Get the MIME type from the extension, then call GetFromType to
  // fill in the MIMEInfo.

  nsCAutoString fileExtToUse;
  if (aFileExt && aFileExt[0] != '.')
    fileExtToUse = '.';

  fileExtToUse.Append(aFileExt);

  const char *mimeType = _gnome_vfs_mime_type_from_name(fileExtToUse.get());
  if (!strcmp(mimeType, "application/octet-stream"))
    return nsnull;

  return GetFromType(mimeType);
}

/* static */ already_AddRefed<nsMIMEInfoBase>
nsGNOMERegistry::GetFromType(const char *aMIMEType)
{
  if (!gconfLib)
    return nsnull;

  GnomeVFSMimeApplication *handlerApp = _gnome_vfs_mime_get_default_application(aMIMEType);
  if (!handlerApp)
    return nsnull;

  nsRefPtr<nsMIMEInfoImpl> mimeInfo = new nsMIMEInfoImpl(aMIMEType);
  NS_ENSURE_TRUE(mimeInfo, nsnull);

  // Get the list of extensions and append then to the mimeInfo.
  GList *extensions = _gnome_vfs_mime_get_extensions_list(aMIMEType);
  for (GList *extension = extensions; extension; extension = extension->next)
    mimeInfo->AppendExtension(nsDependentCString((const char *) extension->data));

  _gnome_vfs_mime_extensions_list_free(extensions);

  const char *description = _gnome_vfs_mime_get_description(aMIMEType);
  mimeInfo->SetDescription(NS_ConvertUTF8toUCS2(description));

  // Convert UTF-8 registry value to filesystem encoding, which
  // g_find_program_in_path() uses.

  gchar *nativeCommand = g_filename_from_utf8(handlerApp->command,
                                              -1, NULL, NULL, NULL);
  if (!nativeCommand) {
    NS_ERROR("Could not convert helper app command to filesystem encoding");
    _gnome_vfs_mime_application_free(handlerApp);
    return nsnull;
  }

  gchar *commandPath = g_find_program_in_path(nativeCommand);

  g_free(nativeCommand);

  if (!commandPath) {
    _gnome_vfs_mime_application_free(handlerApp);
    return nsnull;
  }

  nsCOMPtr<nsILocalFile> appFile;
  NS_NewNativeLocalFile(nsDependentCString(commandPath), PR_TRUE,
                        getter_AddRefs(appFile));
  if (appFile) {
    mimeInfo->SetDefaultApplication(appFile);
    mimeInfo->SetDefaultDescription(NS_ConvertUTF8toUCS2(handlerApp->name));
    mimeInfo->SetPreferredAction(nsIMIMEInfo::useSystemDefault);
  }

  g_free(commandPath);

  _gnome_vfs_mime_application_free(handlerApp);

  nsMIMEInfoBase* retval;
  NS_ADDREF((retval = mimeInfo));
  return retval;
}
