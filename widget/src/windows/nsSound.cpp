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
 */

#include "nscore.h"
#include "plstr.h"
#include <stdio.h>

#include <windows.h>

#include "nsSound.h"
#include "nsIURL.h"
#include "nsNetUtil.h"

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

private:
  HINSTANCE mInstance;  
  PlayPtr mPlay;
};

////////////////////////////////////////////////////////////////////////

nsSound::nsSound()
{
  NS_INIT_REFCNT();
  mLastSound = nsnull;
}

nsSound::~nsSound()
{
  PurgeLastSound();
}

void nsSound::PurgeLastSound() {
  if (mLastSound) {
    // Purge the current sound buffer.
    ::PlaySound(NULL, NULL, SND_PURGE);  // This call halts the sound if it was still playing.
  
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
                                        PRUint32 stringLen,
                                        const char *string)
{
  // print a load error on bad status
  if (NS_FAILED(aStatus)) {
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
          char* uriSpec;
          uri->GetSpec(&uriSpec);
#ifdef DEBUG
          printf("Failed to load %s\n", uriSpec ? uriSpec : "");
#endif
        }
      }
    }
  }

  PurgeLastSound();

  if (string && stringLen > 0) {
    // XXX this shouldn't be SYNC, but unless we make a copy of the memory, we can't play it async.
    // We shouldn't have to hold on to this and free it.
    mLastSound = (char *) malloc(stringLen);
    memcpy(mLastSound, string, stringLen);
    ::PlaySound(mLastSound, nsnull, SND_MEMORY | SND_NODEFAULT | SND_ASYNC);
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


NS_IMETHODIMP nsSound::PlaySystemSound(const char *aSoundAlias)
{
  PurgeLastSound();

  ::PlaySound(aSoundAlias, nsnull, SND_ALIAS | SND_ASYNC);

  return NS_OK;
}

