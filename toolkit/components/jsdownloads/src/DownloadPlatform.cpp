/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DownloadPlatform.h"
#include "nsAutoPtr.h"
#include "nsString.h"
#include "nsIURI.h"
#include "nsIFile.h"
#include "nsIObserverService.h"
#include "nsISupportsPrimitives.h"
#include "nsDirectoryServiceDefs.h"

#include "mozilla/Preferences.h"
#include "mozilla/Services.h"

#define PREF_BDM_ADDTORECENTDOCS "browser.download.manager.addToRecentDocs"

#ifdef XP_WIN
#include <shlobj.h>
#include <urlmon.h>
#include "nsILocalFileWin.h"
#endif

#ifdef XP_MACOSX
#include <CoreFoundation/CoreFoundation.h>
#endif

#ifdef MOZ_WIDGET_ANDROID
#include "AndroidBridge.h"
#endif

#ifdef MOZ_WIDGET_GTK
#include <gtk/gtk.h>
#endif

using namespace mozilla;

DownloadPlatform *DownloadPlatform::gDownloadPlatformService = nullptr;

NS_IMPL_ISUPPORTS(DownloadPlatform, mozIDownloadPlatform);

DownloadPlatform* DownloadPlatform::GetDownloadPlatform()
{
  if (!gDownloadPlatformService) {
    gDownloadPlatformService = new DownloadPlatform();
  }

  NS_ADDREF(gDownloadPlatformService);

#if defined(MOZ_WIDGET_GTK)
  g_type_init();
#endif

  return gDownloadPlatformService;
}

#ifdef MOZ_ENABLE_GIO
static void gio_set_metadata_done(GObject *source_obj, GAsyncResult *res, gpointer user_data)
{
  GError *err = nullptr;
  g_file_set_attributes_finish(G_FILE(source_obj), res, nullptr, &err);
  if (err) {
#ifdef DEBUG
    NS_DebugBreak(NS_DEBUG_WARNING, "Set file metadata failed: ", err->message, __FILE__, __LINE__);
#endif
    g_error_free(err);
  }
}
#endif

nsresult DownloadPlatform::DownloadDone(nsIURI* aSource, nsIFile* aTarget,
                                        const nsACString& aContentType, bool aIsPrivate)
{
#if defined(XP_WIN) || defined(XP_MACOSX) || defined(MOZ_WIDGET_ANDROID) \
 || defined(MOZ_WIDGET_GTK) || defined(MOZ_WIDGET_GONK)

  nsAutoString path;
  if (aTarget && NS_SUCCEEDED(aTarget->GetPath(path))) {
#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK) || defined(MOZ_WIDGET_ANDROID)
    // On Windows and Gtk, add the download to the system's "recent documents"
    // list, with a pref to disable.
    {
      bool addToRecentDocs = Preferences::GetBool(PREF_BDM_ADDTORECENTDOCS);
#ifdef MOZ_WIDGET_ANDROID
      if (addToRecentDocs) {
        mozilla::widget::android::DownloadsIntegration::ScanMedia(path, NS_ConvertUTF8toUTF16(aContentType));
      }
#else
      if (addToRecentDocs && !aIsPrivate) {
#ifdef XP_WIN
        ::SHAddToRecentDocs(SHARD_PATHW, path.get());
#elif defined(MOZ_WIDGET_GTK)
        GtkRecentManager* manager = gtk_recent_manager_get_default();

        gchar* uri = g_filename_to_uri(NS_ConvertUTF16toUTF8(path).get(),
                                       nullptr, nullptr);
        if (uri) {
          gtk_recent_manager_add_item(manager, uri);
          g_free(uri);
        }
#endif
      }
#endif
#ifdef MOZ_ENABLE_GIO
      // Use GIO to store the source URI for later display in the file manager.
      GFile* gio_file = g_file_new_for_path(NS_ConvertUTF16toUTF8(path).get());
      nsCString source_uri;
      aSource->GetSpec(source_uri);
      GFileInfo *file_info = g_file_info_new();
      g_file_info_set_attribute_string(file_info, "metadata::download-uri", source_uri.get());
      g_file_set_attributes_async(gio_file,
                                  file_info,
                                  G_FILE_QUERY_INFO_NONE,
                                  G_PRIORITY_DEFAULT,
                                  nullptr, gio_set_metadata_done, nullptr);
      g_object_unref(file_info);
      g_object_unref(gio_file);
#endif
    }
#endif

#ifdef XP_MACOSX
    // On OS X, make the downloads stack bounce.
    CFStringRef observedObject = ::CFStringCreateWithCString(kCFAllocatorDefault,
                                             NS_ConvertUTF16toUTF8(path).get(),
                                             kCFStringEncodingUTF8);
    CFNotificationCenterRef center = ::CFNotificationCenterGetDistributedCenter();
    ::CFNotificationCenterPostNotification(center, CFSTR("com.apple.DownloadFileFinished"),
                                           observedObject, nullptr, TRUE);
    ::CFRelease(observedObject);
#endif
    if (mozilla::Preferences::GetBool("device.storage.enabled", true)) {
      // Tell DeviceStorage that a new file may have been added.
      nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
      nsCOMPtr<nsISupportsString> pathString
        = do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID);
      if (obs && pathString) {
        if (NS_SUCCEEDED(pathString->SetData(path))) {
          (void)obs->NotifyObservers(pathString, "download-watcher-notify",
                                     MOZ_UTF16("modified"));
        }
      }
    }
  }

#ifdef XP_WIN
  // Adjust file attributes so that by default, new files are indexed by
  // desktop search services. Skip off those that land in the temp folder.
  nsCOMPtr<nsIFile> tempDir, fileDir;
  nsresult rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(tempDir));
  NS_ENSURE_SUCCESS(rv, rv);
  aTarget->GetParent(getter_AddRefs(fileDir));

  bool isTemp = false;
  if (fileDir) {
    fileDir->Equals(tempDir, &isTemp);
  }

  nsCOMPtr<nsILocalFileWin> localFileWin(do_QueryInterface(aTarget));
  if (!isTemp && localFileWin) {
    localFileWin->SetFileAttributesWin(nsILocalFileWin::WFA_SEARCH_INDEXED);
  }
#endif

#endif

  return NS_OK;
}

nsresult DownloadPlatform::MapUrlToZone(const nsAString& aURL,
                                        uint32_t* aZone)
{
#ifdef XP_WIN
  nsRefPtr<IInternetSecurityManager> inetSecMgr;
  if (FAILED(CoCreateInstance(CLSID_InternetSecurityManager, NULL,
                              CLSCTX_ALL, IID_IInternetSecurityManager,
                              getter_AddRefs(inetSecMgr)))) {
    return NS_ERROR_UNEXPECTED;
  }

  DWORD zone;
  if (inetSecMgr->MapUrlToZone(PromiseFlatString(aURL).get(),
                               &zone, 0) != S_OK) {
    return NS_ERROR_UNEXPECTED;
  } else {
    *aZone = zone;
  }

  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}
