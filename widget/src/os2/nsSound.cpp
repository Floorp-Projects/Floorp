/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 *   John Fairhurst <john_fairhurst@iname.com>
 *   IBM Corp.
 */

#include "nscore.h"
#include "plstr.h"
#include <stdio.h>

#include <os2.h>

#include "nsSound.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIPref.h"

#include "nsDirectoryServiceDefs.h"

NS_IMPL_ISUPPORTS2(nsSound, nsISound, nsIStreamLoaderObserver)

////////////////////////////////////////////////////////////////////////
nsSound::nsSound()
{
  NS_INIT_ISUPPORTS();
}

nsSound::~nsSound()
{
}

NS_IMETHODIMP nsSound::OnStreamComplete(nsIStreamLoader *aLoader,
                                        nsISupports *context,
                                        nsresult aStatus,
                                        PRUint32 stringLen,
                                        const char *string)
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

  if (PL_strncmp(string, "RIFF", 4)) {
#ifdef DEBUG
    printf("We only support WAV files currently.\n");
#endif
    return NS_ERROR_FAILURE;
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
  fwrite(string, stringLen, 1, fp);
  fclose(fp);
  HOBJECT hobject = WinQueryObject(soundFilename.get());
  WinSetObjectData(hobject, "OPEN=DEFAULT");

  return NS_OK;

#ifdef OLDCODE /* Some day we could try to get this working */
  ULONG ulRC;
  CHAR errorBuffer[128];

  HMMIO hmmio;
  MMIOINFO mmioinfo;

  memset(&mmioinfo, 0, sizeof(MMIOINFO));
  mmioinfo.fccIOProc = FOURCC_MEM;
  mmioinfo.cchBuffer = stringLen;
  mmioinfo.pchBuffer = (char*)string;
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
  nsresult rv;
  nsCAutoString prefName("system.sound.");
  prefName += aSoundAlias;

  nsXPIDLCString soundPrefValue;

  nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
  if (NS_SUCCEEDED(rv) && prefs)
     rv = prefs->CopyCharPref(prefName.get(), getter_Copies(soundPrefValue));

  if (NS_SUCCEEDED(rv) && soundPrefValue.Length() > 0) {
    nsCOMPtr<nsIURI> soundURI;
    rv = NS_NewURI(getter_AddRefs(soundURI), soundPrefValue);
    nsCOMPtr<nsIURL> soundURL = do_QueryInterface(soundURI);
    rv = Play(soundURL);
    if (NS_SUCCEEDED(rv))
      return NS_OK;
  }
  Beep();
  return NS_OK;
}

