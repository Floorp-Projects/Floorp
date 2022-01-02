/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string.h>

#include "nscore.h"
#include "plstr.h"
#include "prlink.h"

#include "nsSound.h"

#include "HeadlessSound.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIChannel.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "mozilla/FileUtils.h"
#include "mozilla/Unused.h"
#include "mozilla/WidgetUtils.h"
#include "nsIXULAppInfo.h"
#include "nsContentUtils.h"
#include "gfxPlatform.h"
#include "mozilla/ClearOnShutdown.h"

#include <stdio.h>
#include <unistd.h>

#include <gtk/gtk.h>
static PRLibrary* libcanberra = nullptr;

/* used to play sounds with libcanberra. */
typedef struct _ca_context ca_context;
typedef struct _ca_proplist ca_proplist;

typedef void (*ca_finish_callback_t)(ca_context* c, uint32_t id, int error_code,
                                     void* userdata);

typedef int (*ca_context_create_fn)(ca_context**);
typedef int (*ca_context_destroy_fn)(ca_context*);
typedef int (*ca_context_play_fn)(ca_context* c, uint32_t id, ...);
typedef int (*ca_context_change_props_fn)(ca_context* c, ...);
typedef int (*ca_proplist_create_fn)(ca_proplist**);
typedef int (*ca_proplist_destroy_fn)(ca_proplist*);
typedef int (*ca_proplist_sets_fn)(ca_proplist* c, const char* key,
                                   const char* value);
typedef int (*ca_context_play_full_fn)(ca_context* c, uint32_t id,
                                       ca_proplist* p, ca_finish_callback_t cb,
                                       void* userdata);

static ca_context_create_fn ca_context_create;
static ca_context_destroy_fn ca_context_destroy;
static ca_context_play_fn ca_context_play;
static ca_context_change_props_fn ca_context_change_props;
static ca_proplist_create_fn ca_proplist_create;
static ca_proplist_destroy_fn ca_proplist_destroy;
static ca_proplist_sets_fn ca_proplist_sets;
static ca_context_play_full_fn ca_context_play_full;

struct ScopedCanberraFile {
  explicit ScopedCanberraFile(nsIFile* file) : mFile(file){};

  ~ScopedCanberraFile() {
    if (mFile) {
      mFile->Remove(false);
    }
  }

  void forget() { mozilla::Unused << mFile.forget(); }
  nsIFile* operator->() { return mFile; }
  operator nsIFile*() { return mFile; }

  nsCOMPtr<nsIFile> mFile;
};

static ca_context* ca_context_get_default() {
  // This allows us to avoid race conditions with freeing the context by handing
  // that responsibility to Glib, and still use one context at a time
  static GPrivate ctx_private =
      G_PRIVATE_INIT((GDestroyNotify)ca_context_destroy);

  ca_context* ctx = (ca_context*)g_private_get(&ctx_private);

  if (ctx) {
    return ctx;
  }

  ca_context_create(&ctx);
  if (!ctx) {
    return nullptr;
  }

  g_private_set(&ctx_private, ctx);

  GtkSettings* settings = gtk_settings_get_default();
  if (g_object_class_find_property(G_OBJECT_GET_CLASS(settings),
                                   "gtk-sound-theme-name")) {
    gchar* sound_theme_name = nullptr;
    g_object_get(settings, "gtk-sound-theme-name", &sound_theme_name, nullptr);

    if (sound_theme_name) {
      ca_context_change_props(ctx, "canberra.xdg-theme.name", sound_theme_name,
                              nullptr);
      g_free(sound_theme_name);
    }
  }

  nsAutoString wbrand;
  mozilla::widget::WidgetUtils::GetBrandShortName(wbrand);
  ca_context_change_props(ctx, "application.name",
                          NS_ConvertUTF16toUTF8(wbrand).get(), nullptr);

  nsCOMPtr<nsIXULAppInfo> appInfo =
      do_GetService("@mozilla.org/xre/app-info;1");
  if (appInfo) {
    nsAutoCString version;
    appInfo->GetVersion(version);

    ca_context_change_props(ctx, "application.version", version.get(), nullptr);
  }

  ca_context_change_props(ctx, "application.icon_name", MOZ_APP_NAME, nullptr);

  return ctx;
}

static void ca_finish_cb(ca_context* c, uint32_t id, int error_code,
                         void* userdata) {
  nsIFile* file = reinterpret_cast<nsIFile*>(userdata);
  if (file) {
    file->Remove(false);
    NS_RELEASE(file);
  }
}

NS_IMPL_ISUPPORTS(nsSound, nsISound, nsIStreamLoaderObserver)

////////////////////////////////////////////////////////////////////////
nsSound::nsSound() { mInited = false; }

nsSound::~nsSound() = default;

NS_IMETHODIMP
nsSound::Init() {
  // This function is designed so that no library is compulsory, and
  // one library missing doesn't cause the other(s) to not be used.
  if (mInited) return NS_OK;

  mInited = true;

  if (!libcanberra) {
    libcanberra = PR_LoadLibrary("libcanberra.so.0");
    if (libcanberra) {
      ca_context_create = (ca_context_create_fn)PR_FindFunctionSymbol(
          libcanberra, "ca_context_create");
      if (!ca_context_create) {
#ifdef MOZ_TSAN
        // With TSan, we cannot unload libcanberra once we have loaded it
        // because TSan does not support unloading libraries that are matched
        // from its suppression list. Hence we just keep the library loaded in
        // TSan builds.
        libcanberra = nullptr;
        return NS_OK;
#endif
        PR_UnloadLibrary(libcanberra);
        libcanberra = nullptr;
      } else {
        // at this point we know we have a good libcanberra library
        ca_context_destroy = (ca_context_destroy_fn)PR_FindFunctionSymbol(
            libcanberra, "ca_context_destroy");
        ca_context_play = (ca_context_play_fn)PR_FindFunctionSymbol(
            libcanberra, "ca_context_play");
        ca_context_change_props =
            (ca_context_change_props_fn)PR_FindFunctionSymbol(
                libcanberra, "ca_context_change_props");
        ca_proplist_create = (ca_proplist_create_fn)PR_FindFunctionSymbol(
            libcanberra, "ca_proplist_create");
        ca_proplist_destroy = (ca_proplist_destroy_fn)PR_FindFunctionSymbol(
            libcanberra, "ca_proplist_destroy");
        ca_proplist_sets = (ca_proplist_sets_fn)PR_FindFunctionSymbol(
            libcanberra, "ca_proplist_sets");
        ca_context_play_full = (ca_context_play_full_fn)PR_FindFunctionSymbol(
            libcanberra, "ca_context_play_full");
      }
    }
  }

  return NS_OK;
}

/* static */
void nsSound::Shutdown() {
#ifndef MOZ_TSAN
  if (libcanberra) {
    PR_UnloadLibrary(libcanberra);
    libcanberra = nullptr;
  }
#endif
}

namespace mozilla {
namespace sound {
StaticRefPtr<nsISound> sInstance;
}
}  // namespace mozilla
/* static */
already_AddRefed<nsISound> nsSound::GetInstance() {
  using namespace mozilla::sound;

  if (!sInstance) {
    if (gfxPlatform::IsHeadless()) {
      sInstance = new mozilla::widget::HeadlessSound();
    } else {
      sInstance = new nsSound();
    }
    ClearOnShutdown(&sInstance);
  }

  RefPtr<nsISound> service = sInstance.get();
  return service.forget();
}

NS_IMETHODIMP nsSound::OnStreamComplete(nsIStreamLoader* aLoader,
                                        nsISupports* context, nsresult aStatus,
                                        uint32_t dataLen, const uint8_t* data) {
  // print a load error on bad status, and return
  if (NS_FAILED(aStatus)) {
#ifdef DEBUG
    if (aLoader) {
      nsCOMPtr<nsIRequest> request;
      aLoader->GetRequest(getter_AddRefs(request));
      if (request) {
        nsCOMPtr<nsIURI> uri;
        nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
        if (channel) {
          channel->GetURI(getter_AddRefs(uri));
          if (uri) {
            printf("Failed to load %s\n", uri->GetSpecOrDefault().get());
          }
        }
      }
    }
#endif
    return aStatus;
  }

  nsCOMPtr<nsIFile> tmpFile;
  nsDirectoryService::gService->Get(NS_OS_TEMP_DIR, NS_GET_IID(nsIFile),
                                    getter_AddRefs(tmpFile));

  nsresult rv =
      tmpFile->AppendNative(nsDependentCString("mozilla_audio_sample"));
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = tmpFile->CreateUnique(nsIFile::NORMAL_FILE_TYPE, PR_IRUSR | PR_IWUSR);
  if (NS_FAILED(rv)) {
    return rv;
  }

  ScopedCanberraFile canberraFile(tmpFile);

  mozilla::AutoFDClose fd;
  rv = canberraFile->OpenNSPRFileDesc(PR_WRONLY, PR_IRUSR | PR_IWUSR,
                                      &fd.rwget());
  if (NS_FAILED(rv)) {
    return rv;
  }

  // XXX: Should we do this on another thread?
  uint32_t length = dataLen;
  while (length > 0) {
    int32_t amount = PR_Write(fd, data, length);
    if (amount < 0) {
      return NS_ERROR_FAILURE;
    }
    length -= amount;
    data += amount;
  }

  ca_context* ctx = ca_context_get_default();
  if (!ctx) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  ca_proplist* p;
  ca_proplist_create(&p);
  if (!p) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsAutoCString path;
  rv = canberraFile->GetNativePath(path);
  if (NS_FAILED(rv)) {
    return rv;
  }

  ca_proplist_sets(p, "media.filename", path.get());
  if (ca_context_play_full(ctx, 0, p, ca_finish_cb, canberraFile) >= 0) {
    // Don't delete the temporary file here if ca_context_play_full succeeds
    canberraFile.forget();
  }
  ca_proplist_destroy(p);

  return NS_OK;
}

NS_IMETHODIMP nsSound::Beep() {
  ::gdk_beep();
  return NS_OK;
}

NS_IMETHODIMP nsSound::Play(nsIURL* aURL) {
  if (!mInited) Init();

  if (!libcanberra) return NS_ERROR_NOT_AVAILABLE;

  nsresult rv;
  if (aURL->SchemeIs("file")) {
    ca_context* ctx = ca_context_get_default();
    if (!ctx) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsAutoCString spec;
    rv = aURL->GetSpec(spec);
    if (NS_FAILED(rv)) {
      return rv;
    }
    gchar* path = g_filename_from_uri(spec.get(), nullptr, nullptr);
    if (!path) {
      return NS_ERROR_FILE_UNRECOGNIZED_PATH;
    }

    ca_context_play(ctx, 0, "media.filename", path, nullptr);
    g_free(path);
  } else {
    nsCOMPtr<nsIStreamLoader> loader;
    rv = NS_NewStreamLoader(
        getter_AddRefs(loader), aURL,
        this,  // aObserver
        nsContentUtils::GetSystemPrincipal(),
        nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
        nsIContentPolicy::TYPE_OTHER);
  }

  return rv;
}

NS_IMETHODIMP nsSound::PlayEventSound(uint32_t aEventId) {
  if (!mInited) Init();

  if (!libcanberra) return NS_OK;

  // Do we even want alert sounds?
  GtkSettings* settings = gtk_settings_get_default();

  if (g_object_class_find_property(G_OBJECT_GET_CLASS(settings),
                                   "gtk-enable-event-sounds")) {
    gboolean enable_sounds = TRUE;
    g_object_get(settings, "gtk-enable-event-sounds", &enable_sounds, nullptr);

    if (!enable_sounds) {
      return NS_OK;
    }
  }

  ca_context* ctx = ca_context_get_default();
  if (!ctx) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  switch (aEventId) {
    case EVENT_ALERT_DIALOG_OPEN:
      ca_context_play(ctx, 0, "event.id", "dialog-warning", nullptr);
      break;
    case EVENT_CONFIRM_DIALOG_OPEN:
      ca_context_play(ctx, 0, "event.id", "dialog-question", nullptr);
      break;
    case EVENT_NEW_MAIL_RECEIVED:
      ca_context_play(ctx, 0, "event.id", "message-new-email", nullptr);
      break;
    case EVENT_MENU_EXECUTE:
      ca_context_play(ctx, 0, "event.id", "menu-click", nullptr);
      break;
    case EVENT_MENU_POPUP:
      ca_context_play(ctx, 0, "event.id", "menu-popup", nullptr);
      break;
  }
  return NS_OK;
}
