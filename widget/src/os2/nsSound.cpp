/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 *   John Fairhurst <john_fairhurst@iname.com>
 *   IBM Corp.
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

#include "nscore.h"
#include "plstr.h"
#include <stdio.h>
#include <string.h>

#define INCL_DOS
#define INCL_DOSERRORS
#include <os2.h>

#include "nsSound.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIPref.h"

#include "nsDirectoryServiceDefs.h"

NS_IMPL_ISUPPORTS2(nsSound, nsISound, nsIStreamLoaderObserver)

static int                gInitialized = 0;
static PRBool             gMMPMInstalled = PR_FALSE;

static void
InitGlobals(void)
{
  APIRET ulrc;
  HMODULE hmod;
  char LoadError[CCHMAXPATH];
  ulrc = DosLoadModule(LoadError, CCHMAXPATH, "MMPM", &hmod);
  if (ulrc == NO_ERROR) {
    gMMPMInstalled = PR_TRUE;
  }
  DosFreeModule(hmod);
  gInitialized = 1;
}


////////////////////////////////////////////////////////////////////////
nsSound::nsSound()
{
  if (!gInitialized) {
    InitGlobals();
  }
}

nsSound::~nsSound()
{
}

NS_IMETHODIMP nsSound::OnStreamComplete(nsIStreamLoader *aLoader,
                                        nsISupports *context,
                                        nsresult aStatus,
                                        PRUint32 dataLen,
                                        const PRUint8 *data)
{

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
    return NS_ERROR_FAILURE;
  }

  if (memcmp(data, "RIFF", 4) || (!gMMPMInstalled)) {
    NS_WARNING("We only support WAV files currently.\n");
    Beep();
    return NS_OK;
  }

  nsresult rv;
    
  static const char *kSoundTmpFileName = "mozsound.wav";

  nsCOMPtr<nsIFile> soundTmp;
  rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(soundTmp));
  if (NS_FAILED(rv)) return rv;
  rv = soundTmp->AppendNative(nsDependentCString(kSoundTmpFileName));
  nsCAutoString soundFilename;
  (void) soundTmp->GetNativePath(soundFilename);
  FILE *fp = fopen(soundFilename.get(), "wb+");
  if (fp) {
    fwrite(data, dataLen, 1, fp);
    fclose(fp);
    HOBJECT hobject = WinQueryObject(soundFilename.get());
    WinSetObjectData(hobject, "OPEN=DEFAULT");
  } else {
    NS_WARNING("Could not open WAV file for binary writing.\n");
    Beep();
  }

  return NS_OK;

#ifdef OLDCODE /* Some day we could try to get this working */
  ULONG ulRC;
  CHAR errorBuffer[128];

  HMMIO hmmio;
  MMIOINFO mmioinfo;

  memset(&mmioinfo, 0, sizeof(MMIOINFO));
  mmioinfo.fccIOProc = FOURCC_MEM;
  mmioinfo.cchBuffer = dataLen;
  mmioinfo.pchBuffer = (char*)data;
  USHORT usDeviceID;

  hmmio = mmioOpen(NULL, &mmioinfo, MMIO_READWRITE);

  MCI_OPEN_PARMS mop;
  memset(&mop, 0, sizeof(MCI_OPEN_PARMS));

  mop.pszElementName = (char*)hmmio;
  CHAR DeviceType[] = "waveaudio";
  mop.pszDeviceType = (PSZ)&DeviceType;

  ulRC = mciSendCommand(0, MCI_OPEN, MCI_OPEN_MMIO | MCI_WAIT, &mop, 0);

  if (ulRC != MCIERR_SUCCESS) {
     ulRC = mciGetErrorString(ulRC, errorBuffer, 128);
  }

  usDeviceID = mop.usDeviceID;

  MCI_OPEN_PARMS mpp;

  memset(&mpp, 0, sizeof(MCI_OPEN_PARMS));
  ulRC = mciSendCommand(usDeviceID, MCI_PLAY, MCI_WAIT, &mpp, 0);

  if (ulRC != MCIERR_SUCCESS) {
     ulRC = mciGetErrorString(ulRC, errorBuffer, 128);
  }

  MCI_GENERIC_PARMS mgp;
  memset(&mgp, 0, sizeof(MCI_GENERIC_PARMS));
  ulRC = mciSendCommand(usDeviceID, MCI_CLOSE, MCI_WAIT, &mgp, 0);

  if (ulRC != MCIERR_SUCCESS) {
     ulRC = mciGetErrorString(ulRC, errorBuffer, 128);
  }

  mmioClose(hmmio, 0);
#endif
}

NS_METHOD nsSound::Beep()
{
  WinAlarm(HWND_DESKTOP, WA_WARNING);

  return NS_OK;
}

NS_METHOD nsSound::Play(nsIURL *aURL)
{
  nsresult rv;

  nsCOMPtr<nsIStreamLoader> loader;
  rv = NS_NewStreamLoader(getter_AddRefs(loader), aURL, this);

  return rv;
}

NS_IMETHODIMP nsSound::Init()
{
  return NS_OK;
}

NS_IMETHODIMP nsSound::PlaySystemSound(const char *aSoundAlias)
{
  /* We don't have a default mail sound on OS/2, so just beep */
  /* Also just beep if MMPM isn't installed */
  if ((strcmp("_moz_mailbeep", aSoundAlias) == 0) || (!gMMPMInstalled)) {
    Beep();
  }
  else {
    HOBJECT hobject = WinQueryObject(aSoundAlias);
    if (hobject)
      WinSetObjectData(hobject, "OPEN=DEFAULT");
    else 
      Beep();
  }
  return NS_OK;
}

