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
#include "../../../../xpcom/io/CocoaFileUtils.h"
#endif

#ifdef MOZ_WIDGET_ANDROID
#include "FennecJNIWrappers.h"
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

#ifdef XP_MACOSX
// Caller is responsible for freeing any result (CF Create Rule)
CFURLRef CreateCFURLFromNSIURI(nsIURI *aURI) {
  nsAutoCString spec;
  if (aURI) {
    aURI->GetSpec(spec);
  }

  CFStringRef urlStr = ::CFStringCreateWithCString(kCFAllocatorDefault,
                                                   spec.get(),
                                                   kCFStringEncodingUTF8);
  if (!urlStr) {
    return NULL;
  }

  CFURLRef url = ::CFURLCreateWithString(kCFAllocatorDefault,
                                         urlStr,
                                         NULL);

  ::CFRelease(urlStr);

  return url;
}
#endif

nsresult DownloadPlatform::DownloadDone(nsIURI* aSource, nsIURI* aReferrer, nsIFile* aTarget,
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
        java::DownloadsIntegration::ScanMedia(path, aContentType);
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
      nsresult rv = aSource->GetSpec(source_uri);
      NS_ENSURE_SUCCESS(rv, rv);
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

    // Add OS X origin and referrer file metadata
    CFStringRef pathCFStr = NULL;
    if (!path.IsEmpty()) {
      pathCFStr = ::CFStringCreateWithCharacters(kCFAllocatorDefault,
                                                 (const UniChar*)path.get(),
                                                 path.Length());
    }
    if (pathCFStr) {
      CFURLRef sourceCFURL = CreateCFURLFromNSIURI(aSource);
      CFURLRef referrerCFURL = CreateCFURLFromNSIURI(aReferrer);

      CocoaFileUtils::AddOriginMetadataToFile(pathCFStr,
                                              sourceCFURL,
                                              referrerCFURL);
      CocoaFileUtils::AddQuarantineMetadataToFile(pathCFStr,
                                                  sourceCFURL,
                                                  referrerCFURL);

      ::CFRelease(pathCFStr);
      if (sourceCFURL) {
        ::CFRelease(sourceCFURL);
      }
      if (referrerCFURL) {
        ::CFRelease(referrerCFURL);
      }
    }
#endif
    if (mozilla::Preferences::GetBool("device.storage.enabled", true)) {
      // Tell DeviceStorage that a new file may have been added.
      nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
      nsCOMPtr<nsISupportsString> pathString
        = do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID);
      if (obs && pathString) {
        if (NS_SUCCEEDED(pathString->SetData(path))) {
          (void)obs->NotifyObservers(pathString, "download-watcher-notify",
                                     u"modified");
        }
      }
    }
  }

#endif

  return NS_OK;
}

nsresult DownloadPlatform::MapUrlToZone(const nsAString& aURL,
                                        uint32_t* aZone)
{
#ifdef XP_WIN
  RefPtr<IInternetSecurityManager> inetSecMgr;
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
