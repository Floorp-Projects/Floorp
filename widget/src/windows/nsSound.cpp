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

#include <windows.h>

// mmsystem.h is needed to build with WIN32_LEAN_AND_MEAN
#include <mmsystem.h>

#include "nsSound.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsCRT.h"

#include "nsNativeCharsetUtils.h"

NS_IMPL_ISUPPORTS2(nsSound, nsISound, nsIStreamLoaderObserver)

////////////////////////////////////////////////////////////////////////
// This hidden class is used to load the winmm.dll when it's needed.

class CWinMM {
  typedef int (CALLBACK *PlayPtr)(const char*,HMODULE,DWORD);

public:

  static CWinMM& GetModule() {
    static CWinMM gSharedWinMM;  //construct this only after you *really* need it.
    return gSharedWinMM;
  }


  CWinMM(const char* aModuleName="WINMM.DLL") {
    mInstance=::LoadLibrary(aModuleName);  
    mPlay=(mInstance) ? (PlayPtr)GetProcAddress(mInstance,"PlaySound") : 0;
    sIsInitialized = PR_TRUE;
  }

  ~CWinMM() {
    if(mInstance)
      ::FreeLibrary(mInstance);
    mInstance=0;
    mPlay=0;
  }

  BOOL PlaySound(const char *aSoundFile,HMODULE aModule,DWORD aOptions) {
    return (mPlay) ? mPlay(aSoundFile, aModule, aOptions) : FALSE;
  }

  static BOOL IsInitialized() {
    return sIsInitialized;
  }
 
private:
  HINSTANCE mInstance;  
  PlayPtr mPlay;
  static BOOL sIsInitialized;
};

BOOL CWinMM::sIsInitialized = PR_FALSE;

////////////////////////////////////////////////////////////////////////

nsSound::nsSound()
{
  mLastSound = nsnull;
}

nsSound::~nsSound()
{
  PurgeLastSound();
}

void nsSound::PurgeLastSound() {
  if (mLastSound) {
    // Purge the current sound buffer.
    CWinMM& theMM = CWinMM::GetModule();
    theMM.PlaySound(nsnull, nsnull, SND_PURGE); // This call halts the sound if it was still playing.

    // Now delete the buffer.
    free(mLastSound);
    mLastSound = nsnull;
  }
}

NS_IMETHODIMP nsSound::Beep()
{
  ::MessageBeep(0);

  return NS_OK;
}

NS_IMETHODIMP nsSound::OnStreamComplete(nsIStreamLoader *aLoader,
                                        nsISupports *context,
                                        nsresult aStatus,
                                        PRUint32 dataLen,
                                        const PRUint8 *data)
{
  // print a load error on bad status
  if (NS_FAILED(aStatus)) {
#ifdef DEBUG
    if (aLoader) {
      nsCOMPtr<nsIRequest> request;
      nsCOMPtr<nsIChannel> channel;
      aLoader->GetRequest(getter_AddRefs(request));
      if (request)
          channel = do_QueryInterface(request);
      if (channel) {
        nsCOMPtr<nsIURI> uri;
        channel->GetURI(getter_AddRefs(uri));
        if (uri) {
          nsCAutoString uriSpec;
          uri->GetSpec(uriSpec);
          printf("Failed to load %s\n", uriSpec.get());
        }
      }
    }
#endif
    return aStatus;
  }

  PurgeLastSound();

  if (data && dataLen > 0) {
    DWORD flags = SND_MEMORY | SND_NODEFAULT;
    // We try to make a copy so we can play it async.
    mLastSound = (PRUint8 *) malloc(dataLen);
    if (mLastSound) {
      memcpy(mLastSound, data, dataLen);
      data = mLastSound;
      flags |= SND_ASYNC;
    }

    CWinMM& theMM = CWinMM::GetModule();
    theMM.PlaySound(NS_REINTERPRET_CAST(const char*, data), 0, flags);
  }

  return NS_OK;
}

NS_IMETHODIMP nsSound::Play(nsIURL *aURL)
{
  nsresult rv;

#ifdef DEBUG_SOUND
  char *url;
  aURL->GetSpec(&url);
  printf("%s\n", url);
#endif

  nsCOMPtr<nsIStreamLoader> loader;
  rv = NS_NewStreamLoader(getter_AddRefs(loader), aURL, this);

  return rv;
}


NS_IMETHODIMP nsSound::Init()
{
  if (CWinMM::IsInitialized())
    return NS_OK;
  CWinMM& theMM = CWinMM::GetModule();

  // This call halts a sound if it was still playing.
  // We have to use the sound library for something to make sure
  // it is initialized.
  // If we wait until the first sound is played, there will
  // be a time lag as the library gets loaded.
  theMM.PlaySound(nsnull, nsnull, SND_PURGE); 

  return NS_OK;
}


NS_IMETHODIMP nsSound::PlaySystemSound(const nsAString &aSoundAlias)
{
  PurgeLastSound();

  CWinMM& theMM = CWinMM::GetModule();

  if (aSoundAlias.EqualsLiteral("_moz_mailbeep")) {
    theMM.PlaySound("MailBeep", nsnull, SND_ALIAS | SND_ASYNC);
  }
  else {
    nsCAutoString nativeSoundAlias;
    NS_CopyUnicodeToNative(aSoundAlias, nativeSoundAlias);
    theMM.PlaySound(nativeSoundAlias.get(), nsnull, SND_ALIAS | SND_ASYNC);
  }

  return NS_OK;
}

