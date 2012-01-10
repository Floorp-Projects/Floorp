/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
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

#include <string.h>

#include "nscore.h"
#include "plstr.h"
#include "prlink.h"

#include "nsSound.h"

#include "nsIURL.h"
#include "nsIFileURL.h"
#include "nsNetUtil.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsString.h"
#include "nsDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "mozilla/FileUtils.h"

#include <stdio.h>
#include <unistd.h>

#include <gtk/gtk.h>
static PRLibrary *libcanberra = nsnull;

/* used to play sounds with libcanberra. */
typedef struct _ca_context ca_context;
typedef struct _ca_proplist ca_proplist;

typedef void (*ca_finish_callback_t) (ca_context *c,
                                      uint32_t id,
                                      int error_code,
                                      void *userdata);

typedef int (*ca_context_create_fn) (ca_context **);
typedef int (*ca_context_destroy_fn) (ca_context *);
typedef int (*ca_context_play_fn) (ca_context *c,
                                   uint32_t id,
                                   ...);
typedef int (*ca_context_change_props_fn) (ca_context *c,
                                           ...);
typedef int (*ca_proplist_create_fn) (ca_proplist **);
typedef int (*ca_proplist_destroy_fn) (ca_proplist *);
typedef int (*ca_proplist_sets_fn) (ca_proplist *c,
                                    const char *key,
                                    const char *value);
typedef int (*ca_context_play_full_fn) (ca_context *c,
                                        uint32_t id,
                                        ca_proplist *p,
                                        ca_finish_callback_t cb,
                                        void *userdata);

static ca_context_create_fn ca_context_create;
static ca_context_destroy_fn ca_context_destroy;
static ca_context_play_fn ca_context_play;
static ca_context_change_props_fn ca_context_change_props;
static ca_proplist_create_fn ca_proplist_create;
static ca_proplist_destroy_fn ca_proplist_destroy;
static ca_proplist_sets_fn ca_proplist_sets;
static ca_context_play_full_fn ca_context_play_full;

struct ScopedCanberraFile {
    ScopedCanberraFile(nsILocalFile *file): mFile(file) {};

    ~ScopedCanberraFile() {
        if (mFile) {
            mFile->Remove(PR_FALSE);
        }
    }

    void forget() {
        mFile.forget();
    }
    nsILocalFile* operator->() { return mFile; }
    operator nsILocalFile*() { return mFile; }

    nsCOMPtr<nsILocalFile> mFile;
};

static ca_context*
ca_context_get_default()
{
    // This allows us to avoid race conditions with freeing the context by handing that
    // responsibility to Glib, and still use one context at a time
    static GStaticPrivate ctx_static_private = G_STATIC_PRIVATE_INIT;

    ca_context* ctx = (ca_context*) g_static_private_get(&ctx_static_private);

    if (ctx) {
        return ctx;
    }

    ca_context_create(&ctx);
    if (!ctx) {
        return nsnull;
    }

    g_static_private_set(&ctx_static_private, ctx, (GDestroyNotify) ca_context_destroy);

    GtkSettings* settings = gtk_settings_get_default();
    if (g_object_class_find_property(G_OBJECT_GET_CLASS(settings),
                                     "gtk-sound-theme-name")) {
        gchar* sound_theme_name = nsnull;
        g_object_get(settings, "gtk-sound-theme-name", &sound_theme_name, NULL);

        if (sound_theme_name) {
            ca_context_change_props(ctx, "canberra.xdg-theme.name", sound_theme_name, NULL);
            g_free(sound_theme_name);
        }
    }

    return ctx;
}

static void
ca_finish_cb(ca_context *c,
             uint32_t id,
             int error_code,
             void *userdata)
{
    nsILocalFile *file = reinterpret_cast<nsILocalFile *>(userdata);
    if (file) {
        file->Remove(PR_FALSE);
        NS_RELEASE(file);
    }
}

NS_IMPL_ISUPPORTS2(nsSound, nsISound, nsIStreamLoaderObserver)

////////////////////////////////////////////////////////////////////////
nsSound::nsSound()
{
    mInited = false;
}

nsSound::~nsSound()
{
}

NS_IMETHODIMP
nsSound::Init()
{
    // This function is designed so that no library is compulsory, and
    // one library missing doesn't cause the other(s) to not be used.
    if (mInited) 
        return NS_OK;

    mInited = true;

    if (!libcanberra) {
        libcanberra = PR_LoadLibrary("libcanberra.so.0");
        if (libcanberra) {
            ca_context_create = (ca_context_create_fn) PR_FindFunctionSymbol(libcanberra, "ca_context_create");
            if (!ca_context_create) {
                PR_UnloadLibrary(libcanberra);
                libcanberra = nsnull;
            } else {
                // at this point we know we have a good libcanberra library
                ca_context_destroy = (ca_context_destroy_fn) PR_FindFunctionSymbol(libcanberra, "ca_context_destroy");
                ca_context_play = (ca_context_play_fn) PR_FindFunctionSymbol(libcanberra, "ca_context_play");
                ca_context_change_props = (ca_context_change_props_fn) PR_FindFunctionSymbol(libcanberra, "ca_context_change_props");
                ca_proplist_create = (ca_proplist_create_fn) PR_FindFunctionSymbol(libcanberra, "ca_proplist_create");
                ca_proplist_destroy = (ca_proplist_destroy_fn) PR_FindFunctionSymbol(libcanberra, "ca_proplist_destroy");
                ca_proplist_sets = (ca_proplist_sets_fn) PR_FindFunctionSymbol(libcanberra, "ca_proplist_sets");
                ca_context_play_full = (ca_context_play_full_fn) PR_FindFunctionSymbol(libcanberra, "ca_context_play_full");
            }
        }
    }

    return NS_OK;
}

/* static */ void
nsSound::Shutdown()
{
    if (libcanberra) {
        PR_UnloadLibrary(libcanberra);
        libcanberra = nsnull;
    }
}

NS_IMETHODIMP nsSound::OnStreamComplete(nsIStreamLoader *aLoader,
                                        nsISupports *context,
                                        nsresult aStatus,
                                        PRUint32 dataLen,
                                        const PRUint8 *data)
{
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
                            nsCAutoString uriSpec;
                            uri->GetSpec(uriSpec);
                            printf("Failed to load %s\n", uriSpec.get());
                      }
                }
            }
        }
#endif
        return aStatus;
    }

    nsCOMPtr<nsILocalFile> tmpFile;
    nsDirectoryService::gService->Get(NS_OS_TEMP_DIR, NS_GET_IID(nsILocalFile),
                                      getter_AddRefs(tmpFile));

    nsresult rv = tmpFile->AppendNative(nsDependentCString("mozilla_audio_sample"));
    if (NS_FAILED(rv)) {
        return rv;
    }

    rv = tmpFile->CreateUnique(nsIFile::NORMAL_FILE_TYPE, PR_IRUSR | PR_IWUSR);
    if (NS_FAILED(rv)) {
        return rv;
    }

    ScopedCanberraFile canberraFile(tmpFile);

    mozilla::AutoFDClose fd;
    rv = canberraFile->OpenNSPRFileDesc(PR_WRONLY, PR_IRUSR | PR_IWUSR, &fd);
    if (NS_FAILED(rv)) {
        return rv;
    }

    // XXX: Should we do this on another thread?
    PRUint32 length = dataLen;
    while (length > 0) {
        PRInt32 amount = PR_Write(fd, data, length);
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

    ca_proplist *p;
    ca_proplist_create(&p);
    if (!p) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    nsCAutoString path;
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

NS_METHOD nsSound::Beep()
{
    ::gdk_beep();
    return NS_OK;
}

NS_METHOD nsSound::Play(nsIURL *aURL)
{
    if (!mInited)
        Init();

    if (!libcanberra)
        return NS_ERROR_NOT_AVAILABLE;

    bool isFile;
    nsresult rv = aURL->SchemeIs("file", &isFile);
    if (NS_SUCCEEDED(rv) && isFile) {
        ca_context* ctx = ca_context_get_default();
        if (!ctx) {
            return NS_ERROR_OUT_OF_MEMORY;
        }

        nsCAutoString path;
        rv = aURL->GetPath(path);
        if (NS_FAILED(rv)) {
            return rv;
        }

        ca_context_play(ctx, 0, "media.filename", path.get(), NULL);
    } else {
        nsCOMPtr<nsIStreamLoader> loader;
        rv = NS_NewStreamLoader(getter_AddRefs(loader), aURL, this);
    }

    return rv;
}

NS_IMETHODIMP nsSound::PlayEventSound(PRUint32 aEventId)
{
    if (!mInited)
        Init();

    if (!libcanberra)
        return NS_OK;

    // Do we even want alert sounds?
    GtkSettings* settings = gtk_settings_get_default();

    if (g_object_class_find_property(G_OBJECT_GET_CLASS(settings),
                                     "gtk-enable-event-sounds")) {
        gboolean enable_sounds = TRUE;
        g_object_get(settings, "gtk-enable-event-sounds", &enable_sounds, NULL);

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
            ca_context_play(ctx, 0, "event.id", "dialog-warning", NULL);
            break;
        case EVENT_CONFIRM_DIALOG_OPEN:
            ca_context_play(ctx, 0, "event.id", "dialog-question", NULL);
            break;
        case EVENT_NEW_MAIL_RECEIVED:
            ca_context_play(ctx, 0, "event.id", "message-new-email", NULL);
            break;
        case EVENT_MENU_EXECUTE:
            ca_context_play(ctx, 0, "event.id", "menu-click", NULL);
            break;
        case EVENT_MENU_POPUP:
            ca_context_play(ctx, 0, "event.id", "menu-popup", NULL);
            break;
    }
    return NS_OK;
}

NS_IMETHODIMP nsSound::PlaySystemSound(const nsAString &aSoundAlias)
{
    if (NS_IsMozAliasSound(aSoundAlias)) {
        NS_WARNING("nsISound::playSystemSound is called with \"_moz_\" events, they are obsolete, use nsISound::playEventSound instead");
        PRUint32 eventId;
        if (aSoundAlias.Equals(NS_SYSSOUND_ALERT_DIALOG))
            eventId = EVENT_ALERT_DIALOG_OPEN;
        else if (aSoundAlias.Equals(NS_SYSSOUND_CONFIRM_DIALOG))
            eventId = EVENT_CONFIRM_DIALOG_OPEN;
        else if (aSoundAlias.Equals(NS_SYSSOUND_MAIL_BEEP))
            eventId = EVENT_NEW_MAIL_RECEIVED;
        else if (aSoundAlias.Equals(NS_SYSSOUND_MENU_EXECUTE))
            eventId = EVENT_MENU_EXECUTE;
        else if (aSoundAlias.Equals(NS_SYSSOUND_MENU_POPUP))
            eventId = EVENT_MENU_POPUP;
        else
            return NS_OK;
        return PlayEventSound(eventId);
    }

    nsresult rv;
    nsCOMPtr <nsIURI> fileURI;

    // create a nsILocalFile and then a nsIFileURL from that
    nsCOMPtr <nsILocalFile> soundFile;
    rv = NS_NewLocalFile(aSoundAlias, true, 
                         getter_AddRefs(soundFile));
    NS_ENSURE_SUCCESS(rv,rv);

    rv = NS_NewFileURI(getter_AddRefs(fileURI), soundFile);
    NS_ENSURE_SUCCESS(rv,rv);

    nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(fileURI,&rv);
    NS_ENSURE_SUCCESS(rv,rv);

    rv = Play(fileURL);

    return rv;
}
