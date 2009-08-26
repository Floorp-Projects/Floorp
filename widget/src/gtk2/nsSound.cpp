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

#include <stdio.h>
#include <unistd.h>

#include <gtk/gtk.h>
/* used with esd_open_sound */
static int esdref = -1;
static PRLibrary *elib = nsnull;
static PRLibrary *libcanberra = nsnull;
static PRLibrary* libasound = nsnull;

// the following from esd.h

#define ESD_BITS8  (0x0000)
#define ESD_BITS16 (0x0001) 
#define ESD_MONO (0x0010)
#define ESD_STEREO (0x0020) 
#define ESD_STREAM (0x0000)
#define ESD_PLAY (0x1000)

#define WAV_MIN_LENGTH 44

typedef int (*EsdOpenSoundType)(const char *host);
typedef int (*EsdCloseType)(int);

/* used to play the sounds from the find symbol call */
typedef int  (*EsdPlayStreamType) (int, int, const char *, const char *);
typedef int  (*EsdAudioOpenType)  (void);
typedef int  (*EsdAudioWriteType) (const void *, int);
typedef void (*EsdAudioCloseType) (void);

/* used to find and play common system event sounds.
   this interfaces with libcanberra.
 */
typedef struct _ca_context ca_context;

typedef int (*ca_context_create_fn) (ca_context **);
typedef int (*ca_context_destroy_fn) (ca_context *);
typedef int (*ca_context_play_fn) (ca_context *c,
                                   uint32_t id,
                                   ...);
typedef int (*ca_context_change_props_fn) (ca_context *c,
                                           ...);

static ca_context_create_fn ca_context_create;
static ca_context_destroy_fn ca_context_destroy;
static ca_context_play_fn ca_context_play;
static ca_context_change_props_fn ca_context_change_props;

/* we provide a blank error handler to silence ALSA's stderr
   messages on computers with no sound devices */
typedef void (*snd_lib_error_handler_t) (const char* file,
                                         int         line,
                                         const char* function,
                                         int         err,
                                         const char* format,
                                         ...);
typedef int (*snd_lib_error_set_handler_fn) (snd_lib_error_handler_t handler);

static void
quiet_error_handler(const char* file,
                    int         line,
                    const char* function,
                    int         err,
                    const char* format,
                    ...)
{
}

NS_IMPL_ISUPPORTS2(nsSound, nsISound, nsIStreamLoaderObserver)

////////////////////////////////////////////////////////////////////////
nsSound::nsSound()
{
    mInited = PR_FALSE;
}

nsSound::~nsSound()
{
    if (esdref >= 0) {
        EsdCloseType EsdClose = (EsdCloseType) PR_FindFunctionSymbol(elib, "esd_close");
        if (EsdClose)
            (*EsdClose)(esdref);
        esdref = -1;
    }
}

NS_IMETHODIMP
nsSound::Init()
{
    // This function is designed so that no library is compulsory, and
    // one library missing doesn't cause the other(s) to not be used.
    if (mInited) 
        return NS_OK;

    mInited = PR_TRUE;

    if (!elib) {
        elib = PR_LoadLibrary("libesd.so.0");
        if (elib) {
            EsdOpenSoundType EsdOpenSound =
                (EsdOpenSoundType) PR_FindFunctionSymbol(elib, "esd_open_sound");
            if (!EsdOpenSound) {
                PR_UnloadLibrary(elib);
                elib = nsnull;
            } else {
                esdref = (*EsdOpenSound)("localhost");
                if (esdref < 0) {
                    PR_UnloadLibrary(elib);
                    elib = nsnull;
                }
            }
        }
    }

    if (!libasound) {
        PRFuncPtr func = PR_FindFunctionSymbolAndLibrary("snd_lib_error_set_handler",
                                                         &libasound);
        if (libasound) {
            snd_lib_error_set_handler_fn snd_lib_error_set_handler =
                 (snd_lib_error_set_handler_fn) func;
            snd_lib_error_set_handler(quiet_error_handler);
        }
    }

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
            }
        }
    }

    return NS_OK;
}

/* static */ void
nsSound::Shutdown()
{
    if (elib) {
        PR_UnloadLibrary(elib);
        elib = nsnull;
    }
    if (libcanberra) {
        PR_UnloadLibrary(libcanberra);
        libcanberra = nsnull;
    }
    if (libasound) {
        PR_UnloadLibrary(libasound);
        libasound = nsnull;
    }
}

#define GET_WORD(s, i) (s[i+1] << 8) | s[i]
#define GET_DWORD(s, i) (s[i+3] << 24) | (s[i+2] << 16) | (s[i+1] << 8) | s[i]

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

    int fd, mask = 0;
    PRUint32 samples_per_sec = 0, avg_bytes_per_sec = 0, chunk_len = 0;
    PRUint16 format, channels = 1, bits_per_sample = 0;
    const PRUint8 *audio = nsnull;
    size_t audio_len = 0;

    if (dataLen < 4) {
        NS_WARNING("Sound stream too short to determine its type");
        return NS_ERROR_FAILURE;
    }

    if (memcmp(data, "RIFF", 4)) {
#ifdef DEBUG
        printf("We only support WAV files currently.\n");
#endif
        return NS_ERROR_FAILURE;
    }

    if (dataLen <= WAV_MIN_LENGTH) {
        NS_WARNING("WAV files should be longer than 44 bytes.");
        return NS_ERROR_FAILURE;
    }

    PRUint32 i = 12;
    while (i + 7 < dataLen) {
        if (!memcmp(data + i, "fmt ", 4) && !chunk_len) {
            i += 4;

            /* length of the rest of this subblock (should be 16 for PCM data */
            chunk_len = GET_DWORD(data, i);
            i += 4;

            if (chunk_len < 16 || i + chunk_len >= dataLen) {
                NS_WARNING("Invalid WAV file: bad fmt chunk.");
                return NS_ERROR_FAILURE;
            }

            format = GET_WORD(data, i);
            i += 2;

            channels = GET_WORD(data, i);
            i += 2;

            samples_per_sec = GET_DWORD(data, i);
            i += 4;

            avg_bytes_per_sec = GET_DWORD(data, i);
            i += 4;

            // block align
            i += 2;

            bits_per_sample = GET_WORD(data, i);
            i += 2;

            /* we don't support WAVs with odd compression codes */
            if (chunk_len != 16)
                NS_WARNING("Extra format bits found in WAV. Ignoring");

            i += chunk_len - 16;
        } else if (!memcmp(data + i, "data", 4)) {
            i += 4;
            if (!chunk_len) {
                NS_WARNING("Invalid WAV file: no fmt chunk found");
                return NS_ERROR_FAILURE;
            }

            audio_len = GET_DWORD(data, i);
            i += 4;

            /* try to play truncated WAVs */
            if (i + audio_len > dataLen)
                audio_len = dataLen - i;

            audio = data + i;
            break;
        } else {
            i += 4;
            i += GET_DWORD(data, i);
            i += 4;
        }
    }

    if (!audio) {
        NS_WARNING("Invalid WAV file: no data chunk found");
        return NS_ERROR_FAILURE;
    }

    /* No audio data? well, at least the WAV was valid. */
    if (!audio_len)
        return NS_OK;

#if 0
    printf("f: %d | c: %d | sps: %li | abps: %li | ba: %d | bps: %d | rate: %li\n",
         format, channels, samples_per_sec, avg_bytes_per_sec, block_align, bits_per_sample, rate);
#endif

    /* open up connection to esd */  
    EsdPlayStreamType EsdPlayStream = 
        (EsdPlayStreamType) PR_FindFunctionSymbol(elib, 
                                                  "esd_play_stream");
    if (!EsdPlayStream)
        return NS_ERROR_FAILURE;

    mask = ESD_PLAY | ESD_STREAM;

    if (bits_per_sample == 8)
        mask |= ESD_BITS8;
    else 
        mask |= ESD_BITS16;

    if (channels == 1)
        mask |= ESD_MONO;
    else 
        mask |= ESD_STEREO;

    nsAutoArrayPtr<PRUint8> buf;

    // ESD only handle little-endian data. 
    // Swap the byte order if we're on a big-endian architecture.
#ifdef IS_BIG_ENDIAN
    if (bits_per_sample != 8) {
        buf = new PRUint8[audio_len];
        if (!buf)
            return NS_ERROR_OUT_OF_MEMORY;
        for (PRUint32 j = 0; j + 2 < audio_len; j += 2) {
            buf[j]     = audio[j + 1];
            buf[j + 1] = audio[j];
        }

        audio = buf;
    }
#endif

    fd = (*EsdPlayStream)(mask, samples_per_sec, NULL, "mozillaSound"); 
  
    if (fd < 0) {
      int *esd_audio_format = (int *) PR_FindSymbol(elib, "esd_audio_format");
      int *esd_audio_rate = (int *) PR_FindSymbol(elib, "esd_audio_rate");
      EsdAudioOpenType EsdAudioOpen = (EsdAudioOpenType) PR_FindFunctionSymbol(elib, "esd_audio_open");
      EsdAudioWriteType EsdAudioWrite = (EsdAudioWriteType) PR_FindFunctionSymbol(elib, "esd_audio_write");
      EsdAudioCloseType EsdAudioClose = (EsdAudioCloseType) PR_FindFunctionSymbol(elib, "esd_audio_close");

      if (!esd_audio_format || !esd_audio_rate ||
          !EsdAudioOpen || !EsdAudioWrite || !EsdAudioClose)
          return NS_ERROR_FAILURE;

      *esd_audio_format = mask;
      *esd_audio_rate = samples_per_sec;
      fd = (*EsdAudioOpen)();

      if (fd < 0)
        return NS_ERROR_FAILURE;

      (*EsdAudioWrite)(audio, audio_len);
      (*EsdAudioClose)();
    } else {
      while (audio_len > 0) {
        ssize_t written = write(fd, audio, audio_len);
        if (written <= 0)
          break;
        audio += written;
        audio_len -= written;
      }
      close(fd);
    }

    return NS_OK;
}

NS_METHOD nsSound::Beep()
{
    ::gdk_beep();
    return NS_OK;
}

NS_METHOD nsSound::Play(nsIURL *aURL)
{
    nsresult rv;

    if (!mInited)
        Init();

    if (!elib) 
	    return NS_ERROR_FAILURE;

    nsCOMPtr<nsIStreamLoader> loader;
    rv = NS_NewStreamLoader(getter_AddRefs(loader), aURL, this);

    return rv;
}

NS_IMETHODIMP nsSound::PlayEventSound(PRUint32 aEventId)
{
    if (!mInited)
        Init();

    if (!libcanberra)
        return NS_OK;

    // Do we even want alert sounds?
    // If so, what sound theme are we using?
    GtkSettings* settings = gtk_settings_get_default();
    gchar* sound_theme_name = nsnull;

    if (g_object_class_find_property(G_OBJECT_GET_CLASS(settings), "gtk-sound-theme-name") &&
        g_object_class_find_property(G_OBJECT_GET_CLASS(settings), "gtk-enable-event-sounds")) {
        gboolean enable_sounds = TRUE;
        g_object_get(settings, "gtk-enable-event-sounds", &enable_sounds,
                               "gtk-sound-theme-name", &sound_theme_name,
                               NULL);

        if (!enable_sounds) {
            g_free(sound_theme_name);
            return NS_OK;
        }
    }

    // This allows us to avoid race conditions with freeing the context by handing that
    // responsibility to Glib, and still use one context at a time
    ca_context* ctx = nsnull;
    static GStaticPrivate ctx_static_private = G_STATIC_PRIVATE_INIT;
    ctx = (ca_context*) g_static_private_get(&ctx_static_private);
    if (!ctx) {
        ca_context_create(&ctx);
        if (!ctx) {
            g_free(sound_theme_name);
            return NS_ERROR_OUT_OF_MEMORY;
        }

        g_static_private_set(&ctx_static_private, ctx, (GDestroyNotify) ca_context_destroy);
    }

    if (sound_theme_name) {
        ca_context_change_props(ctx, "canberra.xdg-theme.name", sound_theme_name, NULL);
        g_free(sound_theme_name);
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
    rv = NS_NewLocalFile(aSoundAlias, PR_TRUE, 
                         getter_AddRefs(soundFile));
    NS_ENSURE_SUCCESS(rv,rv);

    rv = NS_NewFileURI(getter_AddRefs(fileURI), soundFile);
    NS_ENSURE_SUCCESS(rv,rv);

    nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(fileURI,&rv);
    NS_ENSURE_SUCCESS(rv,rv);

    rv = Play(fileURL);

    return rv;
}
