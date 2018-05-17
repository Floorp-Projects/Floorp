/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGIOService.h"
#include "nsString.h"
#include "nsIURI.h"
#include "nsTArray.h"
#include "nsIStringEnumerator.h"
#include "nsAutoPtr.h"
#include "nsIMIMEInfo.h"
#include "nsComponentManagerUtils.h"
#include "nsArray.h"
#include "nsIFile.h"
#include "nsPrintfCString.h"

#include <gio/gio.h>
#include <gtk/gtk.h>
#ifdef MOZ_ENABLE_DBUS
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#endif

// We use the same code as gtk_should_use_portal() to detect if we're in flatpak env
// https://github.com/GNOME/gtk/blob/e0ce028c88858b96aeda9e41734a39a3a04f705d/gtk/gtkprivate.c#L272
static bool GetShouldUseFlatpakPortal() {
  bool shouldUsePortal;
  char *path;
  path = g_build_filename(g_get_user_runtime_dir(), "flatpak-info", nullptr);
  if (g_file_test(path, G_FILE_TEST_EXISTS)) {
    shouldUsePortal = true;
  } else {
    shouldUsePortal = (g_getenv("GTK_USE_PORTAL") != nullptr);
  }
  g_free(path);
  return shouldUsePortal;
}

static bool ShouldUseFlatpakPortalImpl() {
  static bool sShouldUseFlatpakPortal = GetShouldUseFlatpakPortal();
  return sShouldUseFlatpakPortal;
}

class nsFlatpakHandlerApp : public nsIHandlerApp
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIHANDLERAPP
  nsFlatpakHandlerApp() = default;
private:
  virtual ~nsFlatpakHandlerApp() = default;

};

NS_IMPL_ISUPPORTS(nsFlatpakHandlerApp, nsIHandlerApp)

NS_IMETHODIMP
nsFlatpakHandlerApp::GetName(nsAString& aName)
{
  aName.AssignLiteral("System Handler");
  return NS_OK;
}

NS_IMETHODIMP
nsFlatpakHandlerApp::SetName(const nsAString& aName)
{
  // We don't implement SetName because flatpak system handler name is fixed
  return NS_OK;
}

NS_IMETHODIMP
nsFlatpakHandlerApp::GetDetailedDescription(nsAString& aDetailedDescription)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFlatpakHandlerApp::SetDetailedDescription(const nsAString& aDetailedDescription)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFlatpakHandlerApp::Equals(nsIHandlerApp* aHandlerApp, bool* _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFlatpakHandlerApp::LaunchWithURI(nsIURI* aUri, nsIInterfaceRequestor* aRequestor)
{
  nsCString spec;
  aUri->GetSpec(spec);
  GError *error = nullptr;

  // The TMPDIR where files are downloaded when user choose to open them
  // needs to be accessible from sandbox and host. The default settings
  // TMPDIR=/tmp is accessible only to the sandbox. That can be the reason
  // why the gtk_show_uri fails there.
  // The workaround is to set TMPDIR environment variable in sandbox to
  // $XDG_CACHE_HOME/tmp before executing Firefox.
  gtk_show_uri(nullptr, spec.get(), GDK_CURRENT_TIME, &error);
  if (error) {
    NS_WARNING(nsPrintfCString("Cannot launch flatpak handler: %s",
          error->message).get());
    g_error_free(error);
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

/**
 * Get command without any additional arguments
 * @param aCommandWithArguments full commandline input string
 * @param aCommand string for storing command without arguments
 * @return NS_ERROR_FAILURE when unable to parse commandline
 */
static nsresult
GetCommandFromCommandline(nsACString const& aCommandWithArguments, nsACString& aCommand) {
  GError *error = nullptr;
  gchar **argv = nullptr;
  if (!g_shell_parse_argv(aCommandWithArguments.BeginReading(), nullptr, &argv, &error) ||
      !argv[0]) {
    g_warning("Cannot parse command with arguments: %s", error->message);
    g_error_free(error);
    g_strfreev(argv);
    return NS_ERROR_FAILURE;
  }
  aCommand.Assign(argv[0]);
  g_strfreev(argv);
  return NS_OK;
}

class nsGIOMimeApp final : public nsIGIOMimeApp
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIHANDLERAPP
  NS_DECL_NSIGIOMIMEAPP

  explicit nsGIOMimeApp(GAppInfo* aApp) : mApp(aApp) {}

private:
  ~nsGIOMimeApp() { g_object_unref(mApp); }

  GAppInfo *mApp;
};

NS_IMPL_ISUPPORTS(nsGIOMimeApp, nsIGIOMimeApp, nsIHandlerApp)

NS_IMETHODIMP
nsGIOMimeApp::GetId(nsACString& aId)
{
  aId.Assign(g_app_info_get_id(mApp));
  return NS_OK;
}

NS_IMETHODIMP
nsGIOMimeApp::GetName(nsAString& aName)
{
  aName.Assign(NS_ConvertUTF8toUTF16(g_app_info_get_name(mApp)));
  return NS_OK;
}

NS_IMETHODIMP
nsGIOMimeApp::SetName(const nsAString& aName)
{
  // We don't implement SetName because we're using mGIOMimeApp instance for
  // obtaining application name
  return NS_OK;
}

NS_IMETHODIMP
nsGIOMimeApp::GetCommand(nsACString& aCommand)
{
  const char *cmd = g_app_info_get_commandline(mApp);
  if (!cmd)
    return NS_ERROR_FAILURE;
  aCommand.Assign(cmd);
  return NS_OK;
}

NS_IMETHODIMP
nsGIOMimeApp::GetExpectsURIs(int32_t* aExpects)
{
  *aExpects = g_app_info_supports_uris(mApp);
  return NS_OK;
}

NS_IMETHODIMP
nsGIOMimeApp::GetDetailedDescription(nsAString& aDetailedDescription)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsGIOMimeApp::SetDetailedDescription(const nsAString& aDetailedDescription)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsGIOMimeApp::Equals(nsIHandlerApp* aHandlerApp, bool* _retval)
{
  if (!aHandlerApp)
    return NS_ERROR_FAILURE;

  // Compare with nsILocalHandlerApp instance by name
  nsCOMPtr<nsILocalHandlerApp> localHandlerApp = do_QueryInterface(aHandlerApp);
  if (localHandlerApp) {
    nsAutoString theirName;
    nsAutoString thisName;
    GetName(thisName);
    localHandlerApp->GetName(theirName);
    *_retval = thisName.Equals(theirName);
    return NS_OK;
  }

  // Compare with nsIGIOMimeApp instance by command with stripped arguments
  nsCOMPtr<nsIGIOMimeApp> gioMimeApp = do_QueryInterface(aHandlerApp);
  if (gioMimeApp) {
    nsAutoCString thisCommandline, thisCommand;
    nsresult rv = GetCommand(thisCommandline);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = GetCommandFromCommandline(thisCommandline,
                                   thisCommand);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoCString theirCommandline, theirCommand;
    gioMimeApp->GetCommand(theirCommandline);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = GetCommandFromCommandline(theirCommandline,
                                   theirCommand);
    NS_ENSURE_SUCCESS(rv, rv);

    *_retval = thisCommand.Equals(theirCommand);
    return NS_OK;
  }

  // We can only compare with nsILocalHandlerApp and nsGIOMimeApp
  *_retval = false;
  return NS_OK;
}

NS_IMETHODIMP
nsGIOMimeApp::LaunchWithURI(nsIURI* aUri, nsIInterfaceRequestor* aRequestor)
{
  GList uris = { 0 };
  nsCString spec;
  aUri->GetSpec(spec);
  //nsPromiseFlatCString flatUri(aUri);
  uris.data = const_cast<char*>(spec.get());

  GError *error = nullptr;
  gboolean result = g_app_info_launch_uris(mApp, &uris, nullptr, &error);

  if (!result) {
    g_warning("Cannot launch application: %s", error->message);
    g_error_free(error);
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

class GIOUTF8StringEnumerator final : public nsIUTF8StringEnumerator
{
  ~GIOUTF8StringEnumerator() = default;

public:
  GIOUTF8StringEnumerator() : mIndex(0) { }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIUTF8STRINGENUMERATOR

  nsTArray<nsCString> mStrings;
  uint32_t            mIndex;
};

NS_IMPL_ISUPPORTS(GIOUTF8StringEnumerator, nsIUTF8StringEnumerator)

NS_IMETHODIMP
GIOUTF8StringEnumerator::HasMore(bool* aResult)
{
  *aResult = mIndex < mStrings.Length();
  return NS_OK;
}

NS_IMETHODIMP
GIOUTF8StringEnumerator::GetNext(nsACString& aResult)
{
  if (mIndex >= mStrings.Length())
    return NS_ERROR_UNEXPECTED;

  aResult.Assign(mStrings[mIndex]);
  ++mIndex;
  return NS_OK;
}

NS_IMETHODIMP
nsGIOMimeApp::GetSupportedURISchemes(nsIUTF8StringEnumerator** aSchemes)
{
  *aSchemes = nullptr;

  RefPtr<GIOUTF8StringEnumerator> array = new GIOUTF8StringEnumerator();
  NS_ENSURE_TRUE(array, NS_ERROR_OUT_OF_MEMORY);

  GVfs *gvfs = g_vfs_get_default();

  if (!gvfs) {
    g_warning("Cannot get GVfs object.");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  const gchar* const * uri_schemes = g_vfs_get_supported_uri_schemes(gvfs);

  while (*uri_schemes != nullptr) {
    if (!array->mStrings.AppendElement(*uri_schemes)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    uri_schemes++;
  }

  array.forget(aSchemes);
  return NS_OK;
}

NS_IMETHODIMP
nsGIOMimeApp::SetAsDefaultForMimeType(nsACString const& aMimeType)
{
  char *content_type =
    g_content_type_from_mime_type(PromiseFlatCString(aMimeType).get());
  if (!content_type)
    return NS_ERROR_FAILURE;
  GError *error = nullptr;
  g_app_info_set_as_default_for_type(mApp,
                                     content_type,
                                     &error);
  if (error) {
    g_warning("Cannot set application as default for MIME type (%s): %s",
              PromiseFlatCString(aMimeType).get(),
              error->message);
    g_error_free(error);
    g_free(content_type);
    return NS_ERROR_FAILURE;
  }

  g_free(content_type);
  return NS_OK;
}
/**
 * Set default application for files with given extensions
 * @param fileExts string of space separated extensions
 * @return NS_OK when application was set as default for given extensions,
 * NS_ERROR_FAILURE otherwise
 */
NS_IMETHODIMP
nsGIOMimeApp::SetAsDefaultForFileExtensions(nsACString const& fileExts)
{
  GError *error = nullptr;
  char *extensions = g_strdup(PromiseFlatCString(fileExts).get());
  char *ext_pos = extensions;
  char *space_pos;

  while ( (space_pos = strchr(ext_pos, ' ')) || (*ext_pos != '\0') ) {
    if (space_pos) {
      *space_pos = '\0';
    }
    g_app_info_set_as_default_for_extension(mApp, ext_pos, &error);
    if (error) {
      g_warning("Cannot set application as default for extension (%s): %s",
                ext_pos,
                error->message);
      g_error_free(error);
      g_free(extensions);
      return NS_ERROR_FAILURE;
    }
    if (space_pos) {
      ext_pos = space_pos + 1;
    } else {
      *ext_pos = '\0';
    }
  }
  g_free(extensions);
  return NS_OK;
}

/**
 * Set default application for URI's of a particular scheme
 * @param aURIScheme string containing the URI scheme
 * @return NS_OK when application was set as default for URI scheme,
 * NS_ERROR_FAILURE otherwise
 */
NS_IMETHODIMP
nsGIOMimeApp::SetAsDefaultForURIScheme(nsACString const& aURIScheme)
{
  GError *error = nullptr;
  nsAutoCString contentType("x-scheme-handler/");
  contentType.Append(aURIScheme);

  g_app_info_set_as_default_for_type(mApp,
                                     contentType.get(),
                                     &error);
  if (error) {
    g_warning("Cannot set application as default for URI scheme (%s): %s",
              PromiseFlatCString(aURIScheme).get(),
              error->message);
    g_error_free(error);
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsGIOService, nsIGIOService)

NS_IMETHODIMP
nsGIOService::GetMimeTypeFromExtension(const nsACString& aExtension,
                                             nsACString& aMimeType)
{
  nsAutoCString fileExtToUse("file.");
  fileExtToUse.Append(aExtension);

  gboolean result_uncertain;
  char *content_type = g_content_type_guess(fileExtToUse.get(),
                                            nullptr,
                                            0,
                                            &result_uncertain);
  if (!content_type)
    return NS_ERROR_FAILURE;

  char *mime_type = g_content_type_get_mime_type(content_type);
  if (!mime_type) {
    g_free(content_type);
    return NS_ERROR_FAILURE;
  }

  aMimeType.Assign(mime_type);

  g_free(mime_type);
  g_free(content_type);

  return NS_OK;
}
// used in nsGNOMERegistry
// -----------------------------------------------------------------------------
NS_IMETHODIMP
nsGIOService::GetAppForURIScheme(const nsACString& aURIScheme,
                                 nsIHandlerApp** aApp)
{
  *aApp = nullptr;

  // Application in flatpak sandbox does not have access to the list
  // of installed applications on the system. We use generic
  // nsFlatpakHandlerApp which forwards launch call to the system.
  if (ShouldUseFlatpakPortalImpl()) {
    nsFlatpakHandlerApp *mozApp = new nsFlatpakHandlerApp();
    NS_ADDREF(*aApp = mozApp);
    return NS_OK;
  }

  GAppInfo *app_info = g_app_info_get_default_for_uri_scheme(
                          PromiseFlatCString(aURIScheme).get());
  if (app_info) {
    nsGIOMimeApp *mozApp = new nsGIOMimeApp(app_info);
    NS_ADDREF(*aApp = mozApp);
  } else {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsGIOService::GetAppsForURIScheme(const nsACString& aURIScheme,
                                  nsIMutableArray** aResult)
{
  // We don't need to return the nsFlatpakHandlerApp here because
  // it would be skipped by the callers anyway.
  // The preferred handler is provided by GetAppForURIScheme.
  // This method returns all possible application handlers
  // including preferred one. The callers skips the preferred
  // handler in this list to avoid duplicate records in the list
  // they create.
  nsCOMPtr<nsIMutableArray> handlersArray =
    do_CreateInstance(NS_ARRAY_CONTRACTID);

  nsAutoCString contentType("x-scheme-handler/");
  contentType.Append(aURIScheme);

  GList* appInfoList = g_app_info_get_all_for_type(contentType.get());
  // g_app_info_get_all_for_type returns NULL when no appinfo is found
  // or error occurs (contentType is NULL). We are fine with empty app list
  // and we're sure that contentType is not NULL, so we won't return failure.
  if (appInfoList) {
    GList* appInfo = appInfoList;
    while (appInfo) {
      nsCOMPtr<nsIGIOMimeApp> mimeApp = new nsGIOMimeApp(G_APP_INFO(appInfo->data));
      handlersArray->AppendElement(mimeApp);
      appInfo = appInfo->next;
    }
    g_list_free(appInfoList);
  }
  NS_ADDREF(*aResult = handlersArray);
  return NS_OK;
}

NS_IMETHODIMP
nsGIOService::GetAppForMimeType(const nsACString& aMimeType,
                                nsIHandlerApp**   aApp)
{
  *aApp = nullptr;

  // Flatpak does not reveal installed application to the sandbox,
  // we need to create generic system handler.
  if (ShouldUseFlatpakPortalImpl()) {
    nsFlatpakHandlerApp *mozApp = new nsFlatpakHandlerApp();
    NS_ADDREF(*aApp = mozApp);
    return NS_OK;
  }

  char *content_type =
    g_content_type_from_mime_type(PromiseFlatCString(aMimeType).get());
  if (!content_type)
    return NS_ERROR_FAILURE;

  // GIO returns "unknown" appinfo for the application/octet-stream, which is
  // useless. It's better to fallback to create appinfo from file extension later.
  if (g_content_type_is_unknown(content_type)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  GAppInfo *app_info = g_app_info_get_default_for_type(content_type, false);
  if (app_info) {
    nsGIOMimeApp *mozApp = new nsGIOMimeApp(app_info);
    NS_ENSURE_TRUE(mozApp, NS_ERROR_OUT_OF_MEMORY);
    NS_ADDREF(*aApp = mozApp);
  } else {
    g_free(content_type);
    return NS_ERROR_FAILURE;
  }
  g_free(content_type);
  return NS_OK;
}

NS_IMETHODIMP
nsGIOService::GetDescriptionForMimeType(const nsACString& aMimeType,
                                              nsACString& aDescription)
{
  char *content_type =
    g_content_type_from_mime_type(PromiseFlatCString(aMimeType).get());
  if (!content_type)
    return NS_ERROR_FAILURE;

  char *desc = g_content_type_get_description(content_type);
  if (!desc) {
    g_free(content_type);
    return NS_ERROR_FAILURE;
  }

  aDescription.Assign(desc);
  g_free(content_type);
  g_free(desc);
  return NS_OK;
}

NS_IMETHODIMP
nsGIOService::ShowURI(nsIURI* aURI)
{
  nsAutoCString spec;
  nsresult rv = aURI->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);
  GError *error = nullptr;
  if (!g_app_info_launch_default_for_uri(spec.get(), nullptr, &error)) {
    g_warning("Could not launch default application for URI: %s", error->message);
    g_error_free(error);
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsGIOService::ShowURIForInput(const nsACString& aUri)
{
  GFile *file = g_file_new_for_commandline_arg(PromiseFlatCString(aUri).get());
  char* spec = g_file_get_uri(file);
  nsresult rv = NS_ERROR_FAILURE;
  GError *error = nullptr;

  g_app_info_launch_default_for_uri(spec, nullptr, &error);
  if (error) {
    g_warning("Cannot launch default application: %s", error->message);
    g_error_free(error);
  } else {
    rv = NS_OK;
  }
  g_object_unref(file);
  g_free(spec);

  return rv;
}

NS_IMETHODIMP
nsGIOService::OrgFreedesktopFileManager1ShowItems(const nsACString& aPath)
{
#ifndef MOZ_ENABLE_DBUS
  return NS_ERROR_FAILURE;
#else
  GError* error = nullptr;
  static bool org_freedesktop_FileManager1_exists = true;

  if (!org_freedesktop_FileManager1_exists) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  DBusGConnection* dbusGConnection = dbus_g_bus_get(DBUS_BUS_SESSION, &error);

  if (!dbusGConnection) {
    if (error) {
      g_printerr("Failed to open connection to session bus: %s\n", error->message);
      g_error_free(error);
    }
    return NS_ERROR_FAILURE;
  }

  char *uri = g_filename_to_uri(PromiseFlatCString(aPath).get(), nullptr, nullptr);
  if (uri == nullptr) {
    return NS_ERROR_FAILURE;
  }

  DBusConnection* dbusConnection = dbus_g_connection_get_connection(dbusGConnection);
  // Make sure we do not exit the entire program if DBus connection get lost.
  dbus_connection_set_exit_on_disconnect(dbusConnection, false);

  DBusGProxy* dbusGProxy = dbus_g_proxy_new_for_name(dbusGConnection,
                                                     "org.freedesktop.FileManager1",
                                                     "/org/freedesktop/FileManager1",
                                                     "org.freedesktop.FileManager1");

  const char *uris[2] = { uri, nullptr };
  gboolean rv_dbus_call = dbus_g_proxy_call (dbusGProxy, "ShowItems", nullptr, G_TYPE_STRV, uris,
                                             G_TYPE_STRING, "", G_TYPE_INVALID, G_TYPE_INVALID);

  g_object_unref(dbusGProxy);
  dbus_g_connection_unref(dbusGConnection);
  g_free(uri);

  if (!rv_dbus_call) {
    org_freedesktop_FileManager1_exists = false;
    return NS_ERROR_NOT_AVAILABLE;
  }

  return NS_OK;
#endif
}

/**
 * Find GIO Mime App from given commandline.
 * This is different from CreateAppFromCommand because instead of creating the
 * GIO Mime App in case it's not found in the GIO application list, the method
 * returns error.
 * @param aCmd command with parameters used to start the application
 * @return NS_OK when application is found, NS_ERROR_NOT_AVAILABLE otherwise
 */
NS_IMETHODIMP
nsGIOService::FindAppFromCommand(nsACString const& aCmd,
                                 nsIGIOMimeApp** aAppInfo)
{
  GAppInfo *app_info = nullptr, *app_info_from_list = nullptr;
  GList *apps = g_app_info_get_all();
  GList *apps_p = apps;

  // Try to find relevant and existing GAppInfo in all installed application
  // We do this by comparing each GAppInfo's executable with out own
  while (apps_p) {
    app_info_from_list = (GAppInfo*) apps_p->data;
    if (!app_info) {
      // If the executable is not absolute, get it's full path
      char *executable = g_find_program_in_path(g_app_info_get_executable(app_info_from_list));

      if (executable && strcmp(executable, PromiseFlatCString(aCmd).get()) == 0) {
        g_object_ref (app_info_from_list);
        app_info = app_info_from_list;
      }
      g_free(executable);
    }

    g_object_unref(app_info_from_list);
    apps_p = apps_p->next;
  }
  g_list_free(apps);
  if (app_info) {
    nsGIOMimeApp* app = new nsGIOMimeApp(app_info);
    NS_ENSURE_TRUE(app, NS_ERROR_OUT_OF_MEMORY);
    NS_ADDREF(*aAppInfo = app);
    return NS_OK;
  }

  *aAppInfo = nullptr;
  return NS_ERROR_NOT_AVAILABLE;
}

/**
 * Create application info for specified command and application name.
 * Command arguments are ignored and the "%u" is always added.
 * @param cmd command to execute
 * @param appName application name
 * @param appInfo location where created GAppInfo is stored
 * @return NS_OK when object is created, NS_ERROR_FILE_NOT_FOUND when executable
 * is not found in the system path or NS_ERROR_FAILURE otherwise.
 */
NS_IMETHODIMP
nsGIOService::CreateAppFromCommand(nsACString const& cmd,
                                   nsACString const& appName,
                                   nsIGIOMimeApp**   appInfo)
{
  GError *error = nullptr;
  *appInfo = nullptr;

  // Using G_APP_INFO_CREATE_SUPPORTS_URIS calling g_app_info_create_from_commandline
  // appends %u to the cmd even when cmd already contains this parameter.
  // To avoid that we're going to remove arguments before passing to it.
  nsAutoCString commandWithoutArgs;
  nsresult rv = GetCommandFromCommandline(cmd,
                                          commandWithoutArgs);
  NS_ENSURE_SUCCESS(rv, rv);
  GAppInfo *app_info = g_app_info_create_from_commandline(
      commandWithoutArgs.BeginReading(),
      PromiseFlatCString(appName).get(),
      G_APP_INFO_CREATE_SUPPORTS_URIS,
      &error);
  if (!app_info) {
    g_warning("Cannot create application info from command: %s", error->message);
    g_error_free(error);
    return NS_ERROR_FAILURE;
  }

  // Check if executable exist in path
  gchar* executableWithFullPath =
    g_find_program_in_path(commandWithoutArgs.BeginReading());
  if (!executableWithFullPath) {
    return NS_ERROR_FILE_NOT_FOUND;
  }
  g_free(executableWithFullPath);

  nsGIOMimeApp *mozApp = new nsGIOMimeApp(app_info);
  NS_ENSURE_TRUE(mozApp, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(*appInfo = mozApp);
  return NS_OK;
}

NS_IMETHODIMP
nsGIOService::ShouldUseFlatpakPortal(bool* aRes)
{
  *aRes = ShouldUseFlatpakPortalImpl();
  return NS_OK;
}
