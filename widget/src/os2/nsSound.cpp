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

NS_IMPL_ISUPPORTS2(nsSound, nsISound, nsIStreamLoaderObserver)

////////////////////////////////////////////////////////////////////////
nsSound::nsSound()
{
  NS_INIT_REFCNT();
}

nsSound::~nsSound()
{
}

NS_METHOD nsSound::Beep()
{
  WinAlarm(HWND_DESKTOP, WA_WARNING);

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
      aLoader->GetRequest(getter_AddRefs(request));
      if (request) {
        nsCOMPtr<nsIURI> uri;
        nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
        if (channel) {
            channel->GetURI(getter_AddRefs(uri));
            if (uri) {
                char* uriSpec;
                uri->GetSpec(&uriSpec);
#ifdef DEBUG
                printf("Failed to load %s\n", uriSpec ? uriSpec : "");
#endif
                CRTFREEIF(uriSpec);
            }
        }
      }
    }
  }

  if (string && stringLen > 0) {
    // XXX this shouldn't be SYNC, but unless we make a copy of the memory, we can't play it async.
    // We shouldn't have to hold on to this and free it.
    //char *playBuf = (char *) PR_Malloc(stringLen /* * sizeof(char) ?*/  );
    //memcpy(playBuf, string, stringLen);
    HOBJECT hobject = WinQueryObject(string);
    WinSetObjectData(hobject, "OPEN=DEFAULT");
  }

  return NS_OK;
}

NS_METHOD nsSound::Play(nsIURL *aURL)
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
  return Beep();
}

