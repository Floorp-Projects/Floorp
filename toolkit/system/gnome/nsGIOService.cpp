/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGIOService.h"
#include "nsString.h"
#include "nsIURI.h"
#include "nsIFile.h"
#include "nsTArray.h"
#include "nsStringEnumerator.h"
#include "nsIMIMEInfo.h"
#include "nsComponentManagerUtils.h"
#include "nsArray.h"
#include "nsPrintfCString.h"
#include "mozilla/GRefPtr.h"
#include "mozilla/GUniquePtr.h"
#include "mozilla/UniquePtrExtensions.h"
#include "mozilla/WidgetUtilsGtk.h"
#include "mozilla/StaticPrefs_widget.h"
#include "mozilla/net/DNS.h"

#include <gio/gio.h>
#include <gtk/gtk.h>
#ifdef MOZ_ENABLE_DBUS
#  include <fcntl.h>
#  include <dlfcn.h>
#  include "mozilla/widget/AsyncDBus.h"
#  include "mozilla/WidgetUtilsGtk.h"
#endif

using namespace mozilla;

class nsFlatpakHandlerApp : public nsIHandlerApp {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIHANDLERAPP
  nsFlatpakHandlerApp() = default;

 private:
  virtual ~nsFlatpakHandlerApp() = default;
};

NS_IMPL_ISUPPORTS(nsFlatpakHandlerApp, nsIHandlerApp)

NS_IMETHODIMP
nsFlatpakHandlerApp::GetName(nsAString& aName) {
  aName.AssignLiteral("System Handler");
  return NS_OK;
}

NS_IMETHODIMP
nsFlatpakHandlerApp::SetName(const nsAString& aName) {
  // We don't implement SetName because flatpak system handler name is fixed
  return NS_OK;
}

NS_IMETHODIMP
nsFlatpakHandlerApp::GetDetailedDescription(nsAString& aDetailedDescription) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFlatpakHandlerApp::SetDetailedDescription(
    const nsAString& aDetailedDescription) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFlatpakHandlerApp::Equals(nsIHandlerApp* aHandlerApp, bool* _retval) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFlatpakHandlerApp::LaunchWithURI(
    nsIURI* aUri, mozilla::dom::BrowsingContext* aBrowsingContext) {
  nsCString spec;
  aUri->GetSpec(spec);
  GUniquePtr<GError> error;

  // The TMPDIR where files are downloaded when user choose to open them
  // needs to be accessible from sandbox and host. The default settings
  // TMPDIR=/tmp is accessible only to the sandbox. That can be the reason
  // why the gtk_show_uri fails there.
  // The workaround is to set TMPDIR environment variable in sandbox to
  // $XDG_CACHE_HOME/tmp before executing Firefox.
  gtk_show_uri(nullptr, spec.get(), GDK_CURRENT_TIME, getter_Transfers(error));
  if (error) {
    NS_WARNING(
        nsPrintfCString("Cannot launch flatpak handler: %s", error->message)
            .get());
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
static nsresult GetCommandFromCommandline(
    nsACString const& aCommandWithArguments, nsACString& aCommand) {
  GUniquePtr<GError> error;
  gchar** argv = nullptr;
  if (!g_shell_parse_argv(aCommandWithArguments.BeginReading(), nullptr, &argv,
                          getter_Transfers(error)) ||
      !argv[0]) {
    g_warning("Cannot parse command with arguments: %s", error->message);
    g_strfreev(argv);
    return NS_ERROR_FAILURE;
  }
  aCommand.Assign(argv[0]);
  g_strfreev(argv);
  return NS_OK;
}

class nsGIOMimeApp final : public nsIGIOMimeApp {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIHANDLERAPP
  NS_DECL_NSIGIOMIMEAPP

  explicit nsGIOMimeApp(already_AddRefed<GAppInfo> aApp) : mApp(aApp) {}

 private:
  ~nsGIOMimeApp() = default;

  RefPtr<GAppInfo> mApp;
};

NS_IMPL_ISUPPORTS(nsGIOMimeApp, nsIGIOMimeApp, nsIHandlerApp)

NS_IMETHODIMP
nsGIOMimeApp::GetId(nsACString& aId) {
  aId.Assign(g_app_info_get_id(mApp));
  return NS_OK;
}

NS_IMETHODIMP
nsGIOMimeApp::GetName(nsAString& aName) {
  aName.Assign(NS_ConvertUTF8toUTF16(g_app_info_get_name(mApp)));
  return NS_OK;
}

NS_IMETHODIMP
nsGIOMimeApp::SetName(const nsAString& aName) {
  // We don't implement SetName because we're using mGIOMimeApp instance for
  // obtaining application name
  return NS_OK;
}

NS_IMETHODIMP
nsGIOMimeApp::GetCommand(nsACString& aCommand) {
  const char* cmd = g_app_info_get_commandline(mApp);
  if (!cmd) return NS_ERROR_FAILURE;
  aCommand.Assign(cmd);
  return NS_OK;
}

NS_IMETHODIMP
nsGIOMimeApp::GetExpectsURIs(int32_t* aExpects) {
  *aExpects = g_app_info_supports_uris(mApp);
  return NS_OK;
}

NS_IMETHODIMP
nsGIOMimeApp::GetDetailedDescription(nsAString& aDetailedDescription) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsGIOMimeApp::SetDetailedDescription(const nsAString& aDetailedDescription) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsGIOMimeApp::Equals(nsIHandlerApp* aHandlerApp, bool* _retval) {
  if (!aHandlerApp) return NS_ERROR_FAILURE;

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

    rv = GetCommandFromCommandline(thisCommandline, thisCommand);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoCString theirCommandline, theirCommand;
    gioMimeApp->GetCommand(theirCommandline);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = GetCommandFromCommandline(theirCommandline, theirCommand);
    NS_ENSURE_SUCCESS(rv, rv);

    *_retval = thisCommand.Equals(theirCommand);
    return NS_OK;
  }

  // We can only compare with nsILocalHandlerApp and nsGIOMimeApp
  *_retval = false;
  return NS_OK;
}

static RefPtr<GAppLaunchContext> GetLaunchContext() {
  RefPtr<GAppLaunchContext> context = dont_AddRef(g_app_launch_context_new());
  // Unset this before launching third-party MIME handlers. Otherwise, if
  // Thunderbird sets this in its startup script (as it does in Debian and
  // Fedora), and Firefox does not set this in its startup script (it doesn't in
  // Debian), then Firefox will think it is part of Thunderbird and try to make
  // Thunderbird the default browser. See bug 1494436.
  g_app_launch_context_unsetenv(context, "MOZ_APP_LAUNCHER");
  return context;
}

NS_IMETHODIMP
nsGIOMimeApp::LaunchWithURI(nsIURI* aUri,
                            mozilla::dom::BrowsingContext* aBrowsingContext) {
  GList uris = {0};
  nsCString spec;
  aUri->GetSpec(spec);
  // nsPromiseFlatCString flatUri(aUri);
  uris.data = const_cast<char*>(spec.get());

  GUniquePtr<GError> error;
  gboolean result = g_app_info_launch_uris(
      mApp, &uris, GetLaunchContext().get(), getter_Transfers(error));
  if (!result) {
    g_warning("Cannot launch application: %s", error->message);
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

class GIOUTF8StringEnumerator final : public nsStringEnumeratorBase {
  ~GIOUTF8StringEnumerator() = default;

 public:
  GIOUTF8StringEnumerator() : mIndex(0) {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIUTF8STRINGENUMERATOR

  using nsStringEnumeratorBase::GetNext;

  nsTArray<nsCString> mStrings;
  uint32_t mIndex;
};

NS_IMPL_ISUPPORTS(GIOUTF8StringEnumerator, nsIUTF8StringEnumerator,
                  nsIStringEnumerator)

NS_IMETHODIMP
GIOUTF8StringEnumerator::HasMore(bool* aResult) {
  *aResult = mIndex < mStrings.Length();
  return NS_OK;
}

NS_IMETHODIMP
GIOUTF8StringEnumerator::GetNext(nsACString& aResult) {
  if (mIndex >= mStrings.Length()) return NS_ERROR_UNEXPECTED;

  aResult.Assign(mStrings[mIndex]);
  ++mIndex;
  return NS_OK;
}

NS_IMETHODIMP
nsGIOMimeApp::GetSupportedURISchemes(nsIUTF8StringEnumerator** aSchemes) {
  *aSchemes = nullptr;

  RefPtr<GIOUTF8StringEnumerator> array = new GIOUTF8StringEnumerator();

  GVfs* gvfs = g_vfs_get_default();

  if (!gvfs) {
    g_warning("Cannot get GVfs object.");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  const gchar* const* uri_schemes = g_vfs_get_supported_uri_schemes(gvfs);

  while (*uri_schemes != nullptr) {
    // XXX(Bug 1631371) Check if this should use a fallible operation as it
    // pretended earlier.
    array->mStrings.AppendElement(*uri_schemes);
    uri_schemes++;
  }

  array.forget(aSchemes);
  return NS_OK;
}

NS_IMETHODIMP
nsGIOMimeApp::SetAsDefaultForMimeType(nsACString const& aMimeType) {
  GUniquePtr<char> content_type(
      g_content_type_from_mime_type(PromiseFlatCString(aMimeType).get()));
  if (!content_type) return NS_ERROR_FAILURE;
  GUniquePtr<GError> error;
  g_app_info_set_as_default_for_type(mApp, content_type.get(),
                                     getter_Transfers(error));
  if (error) {
    g_warning("Cannot set application as default for MIME type (%s): %s",
              PromiseFlatCString(aMimeType).get(), error->message);
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}
/**
 * Set default application for files with given extensions
 * @param fileExts string of space separated extensions
 * @return NS_OK when application was set as default for given extensions,
 * NS_ERROR_FAILURE otherwise
 */
NS_IMETHODIMP
nsGIOMimeApp::SetAsDefaultForFileExtensions(nsACString const& fileExts) {
  GUniquePtr<GError> error;
  GUniquePtr<char> extensions(g_strdup(PromiseFlatCString(fileExts).get()));
  char* ext_pos = extensions.get();
  char* space_pos;

  while ((space_pos = strchr(ext_pos, ' ')) || (*ext_pos != '\0')) {
    if (space_pos) {
      *space_pos = '\0';
    }
    g_app_info_set_as_default_for_extension(mApp, ext_pos,
                                            getter_Transfers(error));
    if (error) {
      g_warning("Cannot set application as default for extension (%s): %s",
                ext_pos, error->message);
      return NS_ERROR_FAILURE;
    }
    if (space_pos) {
      ext_pos = space_pos + 1;
    } else {
      *ext_pos = '\0';
    }
  }
  return NS_OK;
}

/**
 * Set default application for URI's of a particular scheme
 * @param aURIScheme string containing the URI scheme
 * @return NS_OK when application was set as default for URI scheme,
 * NS_ERROR_FAILURE otherwise
 */
NS_IMETHODIMP
nsGIOMimeApp::SetAsDefaultForURIScheme(nsACString const& aURIScheme) {
  GUniquePtr<GError> error;
  nsAutoCString contentType("x-scheme-handler/");
  contentType.Append(aURIScheme);

  g_app_info_set_as_default_for_type(mApp, contentType.get(),
                                     getter_Transfers(error));
  if (error) {
    g_warning("Cannot set application as default for URI scheme (%s): %s",
              PromiseFlatCString(aURIScheme).get(), error->message);
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsGIOService, nsIGIOService)

NS_IMETHODIMP
nsGIOService::GetMimeTypeFromExtension(const nsACString& aExtension,
                                       nsACString& aMimeType) {
  nsAutoCString fileExtToUse("file.");
  fileExtToUse.Append(aExtension);

  gboolean result_uncertain;
  GUniquePtr<char> content_type(
      g_content_type_guess(fileExtToUse.get(), nullptr, 0, &result_uncertain));
  if (!content_type) {
    return NS_ERROR_FAILURE;
  }

  GUniquePtr<char> mime_type(g_content_type_get_mime_type(content_type.get()));
  if (!mime_type) {
    return NS_ERROR_FAILURE;
  }

  aMimeType.Assign(mime_type.get());
  return NS_OK;
}
// used in nsGNOMERegistry
// -----------------------------------------------------------------------------
NS_IMETHODIMP
nsGIOService::GetAppForURIScheme(const nsACString& aURIScheme,
                                 nsIHandlerApp** aApp) {
  *aApp = nullptr;

  // Application in flatpak sandbox does not have access to the list
  // of installed applications on the system. We use generic
  // nsFlatpakHandlerApp which forwards launch call to the system.
  if (widget::ShouldUsePortal(widget::PortalKind::MimeHandler)) {
    if (mozilla::net::IsLoopbackHostname(aURIScheme)) {
      // When the user writes foo:1234, we try to handle it natively using
      // GetAppForURIScheme, and if that fails, we carry on. On flatpak there's
      // no way to know if an app has handlers or not. Some things like
      // localhost:1234 are really unlikely to be handled by native
      // apps, and we're much better off returning an error here instead.
      return NS_ERROR_FAILURE;
    }
    RefPtr<nsFlatpakHandlerApp> mozApp = new nsFlatpakHandlerApp();
    mozApp.forget(aApp);
    return NS_OK;
  }

  RefPtr<GAppInfo> app_info = dont_AddRef(g_app_info_get_default_for_uri_scheme(
      PromiseFlatCString(aURIScheme).get()));
  if (!app_info) {
    return NS_ERROR_FAILURE;
  }
  RefPtr<nsGIOMimeApp> mozApp = new nsGIOMimeApp(app_info.forget());
  mozApp.forget(aApp);
  return NS_OK;
}

NS_IMETHODIMP
nsGIOService::GetAppsForURIScheme(const nsACString& aURIScheme,
                                  nsIMutableArray** aResult) {
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
      nsCOMPtr<nsIGIOMimeApp> mimeApp =
          new nsGIOMimeApp(dont_AddRef(G_APP_INFO(appInfo->data)));
      handlersArray->AppendElement(mimeApp);
      appInfo = appInfo->next;
    }
    g_list_free(appInfoList);
  }
  handlersArray.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsGIOService::GetAppForMimeType(const nsACString& aMimeType,
                                nsIHandlerApp** aApp) {
  *aApp = nullptr;

  // Flatpak does not reveal installed application to the sandbox,
  // we need to create generic system handler.
  if (widget::ShouldUsePortal(widget::PortalKind::MimeHandler)) {
    RefPtr<nsFlatpakHandlerApp> mozApp = new nsFlatpakHandlerApp();
    mozApp.forget(aApp);
    return NS_OK;
  }

  GUniquePtr<char> content_type(
      g_content_type_from_mime_type(PromiseFlatCString(aMimeType).get()));
  if (!content_type) {
    return NS_ERROR_FAILURE;
  }

  // GIO returns "unknown" appinfo for the application/octet-stream, which is
  // useless. It's better to fallback to create appinfo from file extension
  // later.
  if (g_content_type_is_unknown(content_type.get())) {
    return NS_ERROR_NOT_AVAILABLE;
  }

#if defined(__OpenBSD__) && defined(MOZ_SANDBOX)
  // g_app_info_get_default_for_type will fail on OpenBSD's veiled filesystem
  // since we most likely don't have direct access to the binaries that are
  // registered as defaults for this type.  Fake it up by just executing
  // xdg-open via gio-launch-desktop (which we do have access to) and letting
  // it figure out which program to execute for this MIME type
  RefPtr<GAppInfo> app_info = dont_AddRef(g_app_info_create_from_commandline(
      "/usr/local/bin/xdg-open",
      nsPrintfCString("System default for %s", content_type.get()).get(),
      G_APP_INFO_CREATE_NONE, NULL));
#else
  RefPtr<GAppInfo> app_info =
      dont_AddRef(g_app_info_get_default_for_type(content_type.get(), false));
#endif
  if (!app_info) {
    return NS_ERROR_FAILURE;
  }
  RefPtr<nsGIOMimeApp> mozApp = new nsGIOMimeApp(app_info.forget());
  mozApp.forget(aApp);
  return NS_OK;
}

NS_IMETHODIMP
nsGIOService::GetDescriptionForMimeType(const nsACString& aMimeType,
                                        nsACString& aDescription) {
  GUniquePtr<char> content_type(
      g_content_type_from_mime_type(PromiseFlatCString(aMimeType).get()));
  if (!content_type) {
    return NS_ERROR_FAILURE;
  }

  GUniquePtr<char> desc(g_content_type_get_description(content_type.get()));
  if (!desc) {
    return NS_ERROR_FAILURE;
  }

  aDescription.Assign(desc.get());
  return NS_OK;
}

nsresult nsGIOService::ShowURI(nsIURI* aURI) {
  nsAutoCString spec;
  MOZ_TRY(aURI->GetSpec(spec));
  GUniquePtr<GError> error;
  if (!g_app_info_launch_default_for_uri(spec.get(), GetLaunchContext().get(),
                                         getter_Transfers(error))) {
    g_warning("Could not launch default application for URI: %s",
              error->message);
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

static nsresult LaunchPath(const nsACString& aPath) {
  RefPtr<GFile> file = dont_AddRef(
      g_file_new_for_commandline_arg(PromiseFlatCString(aPath).get()));
  GUniquePtr<char> spec(g_file_get_uri(file));
  GUniquePtr<GError> error;
  g_app_info_launch_default_for_uri(spec.get(), GetLaunchContext().get(),
                                    getter_Transfers(error));
  if (error) {
    g_warning("Cannot launch default application: %s", error->message);
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult nsGIOService::LaunchFile(const nsACString& aPath) {
  return LaunchPath(aPath);
}

static nsresult RevealDirectory(nsIFile* aFile, bool aForce) {
  nsAutoCString path;
  if (bool isDir; NS_SUCCEEDED(aFile->IsDirectory(&isDir)) && isDir) {
    MOZ_TRY(aFile->GetNativePath(path));
    return LaunchPath(path);
  }

  if (!aForce) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIFile> parentDir;
  MOZ_TRY(aFile->GetParent(getter_AddRefs(parentDir)));
  MOZ_TRY(parentDir->GetNativePath(path));
  return LaunchPath(path);
}

#ifdef MOZ_ENABLE_DBUS
// Classic DBus
const char kFreedesktopFileManagerName[] = "org.freedesktop.FileManager1";
const char kFreedesktopFileManagerPath[] = "/org/freedesktop/FileManager1";
const char kMethodShowItems[] = "ShowItems";

// Portal for Snap, Flatpak
const char kFreedesktopPortalName[] = "org.freedesktop.portal.Desktop";
const char kFreedesktopPortalPath[] = "/org/freedesktop/portal/desktop";
const char kFreedesktopPortalOpenURI[] = "org.freedesktop.portal.OpenURI";
const char kMethodOpenDirectory[] = "OpenDirectory";

static nsresult RevealFileViaDBusWithProxy(GDBusProxy* aProxy, nsIFile* aFile,
                                           const char* aMethod) {
  nsAutoCString path;
  MOZ_TRY(aFile->GetNativePath(path));

  RefPtr<mozilla::widget::DBusCallPromise> dbusPromise;
  const char* startupId = "";

  const int32_t timeout =
      StaticPrefs::widget_gtk_file_manager_show_items_timeout_ms();

  if (!(strcmp(aMethod, kMethodOpenDirectory) == 0)) {
    GUniquePtr<gchar> uri(g_filename_to_uri(path.get(), nullptr, nullptr));
    if (!uri) {
      RevealDirectory(aFile, /* aForce = */ true);
      return NS_ERROR_FAILURE;
    }

    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE_STRING_ARRAY);
    g_variant_builder_add(&builder, "s", uri.get());

    RefPtr<GVariant> variant = dont_AddRef(
        g_variant_ref_sink(g_variant_new("(ass)", &builder, startupId)));
    g_variant_builder_clear(&builder);

    dbusPromise = widget::DBusProxyCall(aProxy, aMethod, variant,
                                        G_DBUS_CALL_FLAGS_NONE, timeout);
  } else {
    int fd = open(path.get(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
      g_printerr("Failed to open file: %s returned %d\n", path.get(), errno);
      RevealDirectory(aFile, /* aForce = */ true);
      return NS_ERROR_FAILURE;
    }

    GVariantBuilder options;
    g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);

    static auto g_unix_fd_list_new_from_array =
        (GUnixFDList * (*)(const gint* fds, gint n_fds))
            dlsym(RTLD_DEFAULT, "g_unix_fd_list_new_from_array");

    // Will take ownership of the fd, so we dont have to care about it anymore
    RefPtr<GUnixFDList> fd_list =
        dont_AddRef(g_unix_fd_list_new_from_array(&fd, 1));

    RefPtr<GVariant> variant = dont_AddRef(
        g_variant_ref_sink(g_variant_new("(sha{sv})", startupId, 0, &options)));
    g_variant_builder_clear(&options);

    dbusPromise = widget::DBusProxyCallWithUnixFDList(
        aProxy, aMethod, variant, G_DBUS_CALL_FLAGS_NONE, timeout, fd_list);
  }

  dbusPromise->Then(
      GetCurrentSerialEventTarget(), __func__,
      [](RefPtr<GVariant>&& aResult) {
        // Do nothing, file is shown, we're done.
      },
      [file = RefPtr{aFile}, aMethod](GUniquePtr<GError>&& aError) {
        g_printerr("Failed to query file manager via %s: %s\n", aMethod,
                   aError->message);
        RevealDirectory(file, /* aForce = */ true);
      });
  return NS_OK;
}

static void RevealFileViaDBus(nsIFile* aFile, const char* aName,
                              const char* aPath, const char* aCall,
                              const char* aMethod) {
  widget::CreateDBusProxyForBus(
      G_BUS_TYPE_SESSION,
      GDBusProxyFlags(G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS |
                      G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES),
      /* aInterfaceInfo = */ nullptr, aName, aPath, aCall)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [file = RefPtr{aFile}, aMethod](RefPtr<GDBusProxy>&& aProxy) {
            RevealFileViaDBusWithProxy(aProxy.get(), file, aMethod);
          },
          [file = RefPtr{aFile}, aName](GUniquePtr<GError>&& aError) {
            g_printerr("Failed to create DBUS proxy for %s: %s\n", aName,
                       aError->message);
            RevealDirectory(file, /* aForce = */ true);
          });
}

static void RevealFileViaDBusClassic(nsIFile* aFile) {
  RevealFileViaDBus(aFile, kFreedesktopFileManagerName,
                    kFreedesktopFileManagerPath, kFreedesktopFileManagerName,
                    kMethodShowItems);
}

static void RevealFileViaDBusPortal(nsIFile* aFile) {
  RevealFileViaDBus(aFile, kFreedesktopPortalName, kFreedesktopPortalPath,
                    kFreedesktopPortalOpenURI, kMethodOpenDirectory);
}
#endif

nsresult nsGIOService::RevealFile(nsIFile* aFile) {
#ifdef MOZ_ENABLE_DBUS
  if (NS_SUCCEEDED(RevealDirectory(aFile, /* aForce = */ false))) {
    return NS_OK;
  }
  if (ShouldUsePortal(widget::PortalKind::OpenUri)) {
    RevealFileViaDBusPortal(aFile);
  } else {
    RevealFileViaDBusClassic(aFile);
  }
  return NS_OK;
#else
  return RevealDirectory(aFile, /* aForce = */ true);
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
                                 nsIGIOMimeApp** aAppInfo) {
  RefPtr<GAppInfo> app_info;

  GList* apps = g_app_info_get_all();

  // Try to find relevant and existing GAppInfo in all installed application
  // We do this by comparing each GAppInfo's executable with out own
  for (GList* node = apps; node; node = node->next) {
    RefPtr<GAppInfo> app_info_from_list = dont_AddRef((GAppInfo*)node->data);
    node->data = nullptr;
    if (!app_info) {
      // If the executable is not absolute, get it's full path
      GUniquePtr<char> executable(g_find_program_in_path(
          g_app_info_get_executable(app_info_from_list)));

      if (executable &&
          strcmp(executable.get(), PromiseFlatCString(aCmd).get()) == 0) {
        app_info = std::move(app_info_from_list);
        // Can't break here because we need to keep iterating to unref the other
        // nodes.
      }
    }
  }

  g_list_free(apps);
  if (!app_info) {
    *aAppInfo = nullptr;
    return NS_ERROR_NOT_AVAILABLE;
  }
  RefPtr<nsGIOMimeApp> app = new nsGIOMimeApp(app_info.forget());
  app.forget(aAppInfo);
  return NS_OK;
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
                                   nsIGIOMimeApp** appInfo) {
  *appInfo = nullptr;

  // Using G_APP_INFO_CREATE_SUPPORTS_URIS calling
  // g_app_info_create_from_commandline appends %u to the cmd even when cmd
  // already contains this parameter. To avoid that we're going to remove
  // arguments before passing to it.
  nsAutoCString commandWithoutArgs;
  nsresult rv = GetCommandFromCommandline(cmd, commandWithoutArgs);
  NS_ENSURE_SUCCESS(rv, rv);

  GUniquePtr<GError> error;
  RefPtr<GAppInfo> app_info = dont_AddRef(g_app_info_create_from_commandline(
      commandWithoutArgs.BeginReading(), PromiseFlatCString(appName).get(),
      G_APP_INFO_CREATE_SUPPORTS_URIS, getter_Transfers(error)));
  if (!app_info) {
    g_warning("Cannot create application info from command: %s",
              error->message);
    return NS_ERROR_FAILURE;
  }

  // Check if executable exist in path
  GUniquePtr<gchar> executableWithFullPath(
      g_find_program_in_path(commandWithoutArgs.BeginReading()));
  if (!executableWithFullPath) {
    return NS_ERROR_FILE_NOT_FOUND;
  }
  RefPtr<nsGIOMimeApp> mozApp = new nsGIOMimeApp(app_info.forget());
  mozApp.forget(appInfo);
  return NS_OK;
}
