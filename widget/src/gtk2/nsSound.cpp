/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation.
 * All Rights Reserved.
 * 
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#include "nscore.h"
#include "plstr.h"
#include "prlink.h"

#include "nsSound.h"

#include "nsIURL.h"
#include "nsIFileURL.h"
#include "nsNetUtil.h"
#include "nsCOMPtr.h"

#include <stdio.h>
#include <unistd.h>

#include <gtk/gtk.h>
/* used with esd_open_sound */
static int esdref = -1;
static PRLibrary *elib = nsnull;

// the following from esd.h

#define ESD_BITS8  (0x0000)
#define ESD_BITS16 (0x0001) 
#define ESD_MONO (0x0010)
#define ESD_STEREO (0x0020) 
#define ESD_STREAM (0x0000)
#define ESD_PLAY (0x1000)

typedef int (PR_CALLBACK *EsdOpenSoundType)(const char *host);
typedef int (PR_CALLBACK *EsdCloseType)(int);

/* used to play the sounds from the find symbol call */
typedef int (PR_CALLBACK *EsdPlayStreamFallbackType)  (int, 
                                                       int, 
                                                       const char *, 
                                                       const char *);

NS_IMPL_ISUPPORTS2(nsSound, nsISound, nsIStreamLoaderObserver)

////////////////////////////////////////////////////////////////////////
nsSound::nsSound()
{
    mInited = PR_FALSE;
}

nsSound::~nsSound()
{
    /* see above comment */
    if (esdref != -1) {
        EsdCloseType EsdClose = (EsdCloseType) PR_FindSymbol(elib, "esd_close");
        (*EsdClose)(esdref);
        esdref = -1;
    }
}

NS_IMETHODIMP
nsSound::Init()
{
    /* we don't need to do esd_open_sound if we are only going to play files
       but we will if we want to do things like streams, etc
    */
    if (mInited) 
        return NS_OK;
    if (elib) 
        return NS_OK;

    EsdOpenSoundType EsdOpenSound;

    elib = PR_LoadLibrary("libesd.so.0");
    if (!elib) return NS_ERROR_FAILURE;

    EsdOpenSound = (EsdOpenSoundType) PR_FindSymbol(elib, "esd_open_sound");

    if (!EsdOpenSound)
        return NS_ERROR_FAILURE;

    esdref = (*EsdOpenSound)("localhost");

    if (!esdref)
        return NS_ERROR_FAILURE;

    mInited = PR_TRUE;

    return NS_OK;
}

#define GET_WORD(s, i) ((unsigned char)s[i+1] << 8) | (unsigned char)s[i]
#define GET_DWORD(s, i) ((unsigned char)s[i+3] << 24) | \
    ((unsigned char)s[i+2] << 16) | ((unsigned char)s[i+1] << 8) | \
    (unsigned char)s[i]

NS_IMETHODIMP nsSound::OnStreamComplete(nsIStreamLoader *aLoader,
                                        nsISupports *context,
                                        nsresult aStatus,
                                        PRUint32 stringLen,
                                        const char *string)
{

#ifdef DEBUG
    // print a load error on bad status
    if (NS_FAILED(aStatus)) {
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
    }
#endif

    int fd, mask = 0;
    unsigned long samples_per_sec=0, avg_bytes_per_sec=0;
    unsigned long rate=0;
    unsigned short format, channels = 1, block_align, bits_per_sample=0;

    if (PL_strncmp(string, "RIFF", 4)) {
#ifdef DEBUG
        printf("We only support WAV files currently.\n");
#endif
        return NS_ERROR_FAILURE;
    }

    PRUint32 i;
    for (i= 0; i < stringLen; i++) {
        if (i+3 <= stringLen) 
            if ((string[i] == 'f') &&
                (string[i+1] == 'm') &&
                (string[i+2] == 't') &&
                (string[i+3] == ' ')) {
                i += 4;

                /* length of the rest of this subblock (should be 16 for PCM data */
                i+=4;
    
                format = GET_WORD(string, i);
                i+=2;

                channels = GET_WORD(string, i);
                i+=2;

                samples_per_sec = GET_DWORD(string, i);
                i+=4;

                avg_bytes_per_sec = GET_DWORD(string, i);
                i+=4;

                block_align = GET_WORD(string, i);
                i+=2;

                bits_per_sample = GET_WORD(string, i);
                i+=2;

                rate = samples_per_sec;

                break;
            }
    }

#if 0
    printf("f: %d | c: %d | sps: %li | abps: %li | ba: %d | bps: %d | rate: %li\n",
         format, channels, samples_per_sec, avg_bytes_per_sec, block_align, bits_per_sample, rate);
#endif

    /* open up conneciton to esd */  
    EsdPlayStreamFallbackType EsdPlayStreamFallback = 
        (EsdPlayStreamFallbackType) PR_FindSymbol(elib, 
                                                  "esd_play_stream_fallback");
  
    mask = ESD_PLAY | ESD_STREAM;

    if (bits_per_sample == 8)
        mask |= ESD_BITS8;
    else 
        mask |= ESD_BITS16;

    if (channels == 1)
        mask |= ESD_MONO;
    else 
        mask |= ESD_STEREO;

    fd = (*EsdPlayStreamFallback)(mask, rate, NULL, "mozillaSound"); 
  
    if (fd < 0) {
        return NS_ERROR_FAILURE;
    }

    /* write data out */
    write(fd, string, stringLen);
  
    close(fd);

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

NS_IMETHODIMP nsSound::PlaySystemSound(const char *aSoundAlias)
{
    if (!aSoundAlias)
        return NS_ERROR_FAILURE;

    if (strcmp(aSoundAlias, "_moz_mailbeep") == 0) {
        return Beep();
    }

    nsresult rv;
    nsCOMPtr <nsIURI> fileURI;

    // create a nsILocalFile and then a nsIFileURL from that
    nsCOMPtr <nsILocalFile> soundFile;
    rv = NS_NewNativeLocalFile(nsDependentCString(aSoundAlias), PR_TRUE, 
                               getter_AddRefs(soundFile));
    NS_ENSURE_SUCCESS(rv,rv);

    rv = NS_NewFileURI(getter_AddRefs(fileURI), soundFile);
    NS_ENSURE_SUCCESS(rv,rv);

    nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(fileURI,&rv);
    NS_ENSURE_SUCCESS(rv,rv);

    rv = Play(fileURL);

    return rv;
}
