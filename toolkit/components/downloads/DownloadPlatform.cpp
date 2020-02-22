/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DownloadPlatform.h"
#include "nsNetUtil.h"
#include "nsString.h"
#include "nsINestedURI.h"
#include "nsIProtocolHandler.h"
#include "nsIURI.h"
#include "nsIFile.h"
#include "nsDirectoryServiceDefs.h"
#include "nsThreadUtils.h"
#include "xpcpublic.h"

#include "mozilla/dom/Promise.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"

#define PREF_BDM_ADDTORECENTDOCS "browser.download.manager.addToRecentDocs"

#ifdef XP_WIN
#  include <shlobj.h>
#  include <urlmon.h>
#  include "nsILocalFileWin.h"
#  include "WinTaskbar.h"
#endif

#ifdef XP_MACOSX
#  include <CoreFoundation/CoreFoundation.h>
#  include "../../../xpcom/io/CocoaFileUtils.h"
#endif

#ifdef MOZ_WIDGET_GTK
#  include <gtk/gtk.h>
#endif

using namespace mozilla;
using dom::Promise;

DownloadPlatform* DownloadPlatform::gDownloadPlatformService = nullptr;

NS_IMPL_ISUPPORTS(DownloadPlatform, mozIDownloadPlatform);

DownloadPlatform* DownloadPlatform::GetDownloadPlatform() {
  if (!gDownloadPlatformService) {
    gDownloadPlatformService = new DownloadPlatform();
  }

  NS_ADDREF(gDownloadPlatformService);

#if defined(MOZ_WIDGET_GTK)
  g_type_init();
#endif

  return gDownloadPlatformService;
}

#ifdef MOZ_WIDGET_GTK
static void gio_set_metadata_done(GObject* source_obj, GAsyncResult* res,
                                  gpointer user_data) {
  GError* err = nullptr;
  g_file_set_attributes_finish(G_FILE(source_obj), res, nullptr, &err);
  if (err) {
#  ifdef DEBUG
    NS_DebugBreak(NS_DEBUG_WARNING, "Set file metadata failed: ", err->message,
                  __FILE__, __LINE__);
#  endif
    g_error_free(err);
  }
}
#endif

#ifdef XP_MACOSX
// Caller is responsible for freeing any result (CF Create Rule)
CFURLRef CreateCFURLFromNSIURI(nsIURI* aURI) {
  nsAutoCString spec;
  if (aURI) {
    aURI->GetSpec(spec);
  }

  CFStringRef urlStr = ::CFStringCreateWithCString(
      kCFAllocatorDefault, spec.get(), kCFStringEncodingUTF8);
  if (!urlStr) {
    return NULL;
  }

  CFURLRef url = ::CFURLCreateWithString(kCFAllocatorDefault, urlStr, NULL);

  ::CFRelease(urlStr);

  return url;
}
#endif

#ifdef XP_WIN
static void AddToRecentDocs(nsIFile* aTarget, nsAutoString& aPath) {
  nsString modelId;
  if (mozilla::widget::WinTaskbar::GetAppUserModelID(modelId)) {
    nsCOMPtr<nsIURI> uri;
    if (NS_SUCCEEDED(NS_NewFileURI(getter_AddRefs(uri), aTarget)) && uri) {
      nsCString spec;
      if (NS_SUCCEEDED(uri->GetSpec(spec))) {
        IShellItem2* psi = nullptr;
        if (SUCCEEDED(
                SHCreateItemFromParsingName(NS_ConvertASCIItoUTF16(spec).get(),
                                            nullptr, IID_PPV_ARGS(&psi)))) {
          SHARDAPPIDINFO info = {psi, modelId.get()};
          ::SHAddToRecentDocs(SHARD_APPIDINFO, &info);
          psi->Release();
          return;
        }
      }
    }
  }

  ::SHAddToRecentDocs(SHARD_PATHW, aPath.get());
}
#endif

nsresult DownloadPlatform::DownloadDone(nsIURI* aSource, nsIURI* aReferrer,
                                        nsIFile* aTarget,
                                        const nsACString& aContentType,
                                        bool aIsPrivate, JSContext* aCx,
                                        Promise** aPromise) {
  nsIGlobalObject* globalObject =
      xpc::NativeGlobal(JS::CurrentGlobalOrNull(aCx));

  if (NS_WARN_IF(!globalObject)) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult result;
  RefPtr<Promise> promise = Promise::Create(globalObject, result);

  if (NS_WARN_IF(result.Failed())) {
    return result.StealNSResult();
  }

  nsresult rv = NS_OK;
  bool pendingAsyncOperations = false;

#if defined(XP_WIN) || defined(XP_MACOSX) || defined(MOZ_WIDGET_ANDROID) || \
    defined(MOZ_WIDGET_GTK)

  nsAutoString path;
  if (aTarget && NS_SUCCEEDED(aTarget->GetPath(path))) {
#  if defined(XP_WIN) || defined(MOZ_WIDGET_GTK) || defined(MOZ_WIDGET_ANDROID)
    // On Windows and Gtk, add the download to the system's "recent documents"
    // list, with a pref to disable.
    {
#    ifndef MOZ_WIDGET_ANDROID
      bool addToRecentDocs = Preferences::GetBool(PREF_BDM_ADDTORECENTDOCS);
      if (addToRecentDocs && !aIsPrivate) {
#      ifdef XP_WIN
        AddToRecentDocs(aTarget, path);
#      elif defined(MOZ_WIDGET_GTK)
        GtkRecentManager* manager = gtk_recent_manager_get_default();

        gchar* uri = g_filename_to_uri(NS_ConvertUTF16toUTF8(path).get(),
                                       nullptr, nullptr);
        if (uri) {
          gtk_recent_manager_add_item(manager, uri);
          g_free(uri);
        }
#      endif
      }
#    endif
#    ifdef MOZ_WIDGET_GTK
      // Use GIO to store the source URI for later display in the file manager.
      GFile* gio_file = g_file_new_for_path(NS_ConvertUTF16toUTF8(path).get());
      nsCString source_uri;
      nsresult rv = aSource->GetSpec(source_uri);
      NS_ENSURE_SUCCESS(rv, rv);
      GFileInfo* file_info = g_file_info_new();
      g_file_info_set_attribute_string(file_info, "metadata::download-uri",
                                       source_uri.get());
      g_file_set_attributes_async(gio_file, file_info, G_FILE_QUERY_INFO_NONE,
                                  G_PRIORITY_DEFAULT, nullptr,
                                  gio_set_metadata_done, nullptr);
      g_object_unref(file_info);
      g_object_unref(gio_file);
#    endif
    }
#  endif

#  ifdef XP_MACOSX
    // On OS X, make the downloads stack bounce.
    CFStringRef observedObject = ::CFStringCreateWithCString(
        kCFAllocatorDefault, NS_ConvertUTF16toUTF8(path).get(),
        kCFStringEncodingUTF8);
    CFNotificationCenterRef center =
        ::CFNotificationCenterGetDistributedCenter();
    ::CFNotificationCenterPostNotification(
        center, CFSTR("com.apple.DownloadFileFinished"), observedObject,
        nullptr, TRUE);
    ::CFRelease(observedObject);

    // Add OS X origin and referrer file metadata
    CFStringRef pathCFStr = NULL;
    if (!path.IsEmpty()) {
      pathCFStr = ::CFStringCreateWithCharacters(
          kCFAllocatorDefault, (const UniChar*)path.get(), path.Length());
    }
    if (pathCFStr && !aIsPrivate) {
      bool isFromWeb = IsURLPossiblyFromWeb(aSource);
      nsCOMPtr<nsIURI> source(aSource);
      nsCOMPtr<nsIURI> referrer(aReferrer);

      rv = NS_DispatchBackgroundTask(
          NS_NewRunnableFunction(
              "DownloadPlatform::DownloadDone",
              [pathCFStr, isFromWeb, source, referrer, promise]() mutable {
                CFURLRef sourceCFURL = CreateCFURLFromNSIURI(source);
                CFURLRef referrerCFURL = CreateCFURLFromNSIURI(referrer);

                CocoaFileUtils::AddOriginMetadataToFile(pathCFStr, sourceCFURL,
                                                        referrerCFURL);
                CocoaFileUtils::AddQuarantineMetadataToFile(
                    pathCFStr, sourceCFURL, referrerCFURL, isFromWeb);
                ::CFRelease(pathCFStr);
                if (sourceCFURL) {
                  ::CFRelease(sourceCFURL);
                }
                if (referrerCFURL) {
                  ::CFRelease(referrerCFURL);
                }

                DebugOnly<nsresult> rv =
                    NS_DispatchToMainThread(NS_NewRunnableFunction(
                        "DownloadPlatform::DownloadDoneResolve",
                        [promise = std::move(promise)]() {
                          promise->MaybeResolveWithUndefined();
                        }));
                MOZ_ASSERT(NS_SUCCEEDED(rv));
                // In non-debug builds, if we've for some reason failed to
                // dispatch a runnable to the main thread to resolve the
                // Promise, then it's unlikely we can reject it either. At that
                // point, the Promise is going to remain in pending limbo until
                // its global goes away.
              }),
          NS_DISPATCH_EVENT_MAY_BLOCK);

      if (NS_SUCCEEDED(rv)) {
        pendingAsyncOperations = true;
      }
    }
#  endif
  }

#endif

  if (!pendingAsyncOperations) {
    promise->MaybeResolveWithUndefined();
  }
  promise.forget(aPromise);
  return rv;
}

nsresult DownloadPlatform::MapUrlToZone(const nsAString& aURL,
                                        uint32_t* aZone) {
#ifdef XP_WIN
  RefPtr<IInternetSecurityManager> inetSecMgr;
  if (FAILED(CoCreateInstance(CLSID_InternetSecurityManager, NULL, CLSCTX_ALL,
                              IID_IInternetSecurityManager,
                              getter_AddRefs(inetSecMgr)))) {
    return NS_ERROR_UNEXPECTED;
  }

  DWORD zone;
  if (inetSecMgr->MapUrlToZone(PromiseFlatString(aURL).get(), &zone, 0) !=
      S_OK) {
    return NS_ERROR_UNEXPECTED;
  } else {
    *aZone = zone;
  }

  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

// Check if a URI is likely to be web-based, by checking its URI flags.
// If in doubt (e.g. if anything fails during the check) claims things
// are from the web.
bool DownloadPlatform::IsURLPossiblyFromWeb(nsIURI* aURI) {
  nsCOMPtr<nsIIOService> ios = do_GetIOService();
  nsCOMPtr<nsIURI> uri = aURI;
  if (!ios) {
    return true;
  }

  while (uri) {
    // We're not using nsIIOService::ProtocolHasFlags because it doesn't
    // take per-URI flags into account. We're also not using
    // NS_URIChainHasFlags because we're checking for *any* of 3 flags
    // to be present on *all* of the nested URIs, which it can't do.
    nsAutoCString scheme;
    nsresult rv = uri->GetScheme(scheme);
    if (NS_FAILED(rv)) {
      return true;
    }
    nsCOMPtr<nsIProtocolHandler> ph;
    rv = ios->GetProtocolHandler(scheme.get(), getter_AddRefs(ph));
    if (NS_FAILED(rv)) {
      return true;
    }
    uint32_t flags;
    rv = ph->DoGetProtocolFlags(uri, &flags);
    if (NS_FAILED(rv)) {
      return true;
    }
    // If not dangerous to load, not a UI resource and not a local file,
    // assume this is from the web:
    if (!(flags & nsIProtocolHandler::URI_DANGEROUS_TO_LOAD) &&
        !(flags & nsIProtocolHandler::URI_IS_UI_RESOURCE) &&
        !(flags & nsIProtocolHandler::URI_IS_LOCAL_FILE)) {
      return true;
    }
    // Otherwise, check if the URI is nested, and if so go through
    // the loop again:
    nsCOMPtr<nsINestedURI> nestedURI = do_QueryInterface(uri);
    uri = nullptr;
    if (nestedURI) {
      rv = nestedURI->GetInnerURI(getter_AddRefs(uri));
      if (NS_FAILED(rv)) {
        return true;
      }
    }
  }
  return false;
}
