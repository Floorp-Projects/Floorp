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
 *   Peter Hartshorn <peter@igelaus.com.au>
 */

#include "nscore.h"
#include "plstr.h"
#include "prlink.h"

#include "nsSound.h"

#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsCOMPtr.h"

#include <stdio.h>
#include <unistd.h>


NS_IMPL_THREADSAFE_ISUPPORTS2(nsSound, nsISound, nsIStreamLoaderObserver);

////////////////////////////////////////////////////////////////////////
nsSound::nsSound()
{
  
}

nsSound::~nsSound()
{

}

nsresult nsSound::Init()
{
  
#ifdef DEBUG_faulkner
  fprintf(stderr, "\n////////// nsSound::Init() in xlib called //////////\n");
#endif /* DEBUG_faulkner */
  return NS_OK;
}

NS_IMETHODIMP nsSound::OnStreamComplete(nsIStreamLoader *aLoader,
                                        nsISupports *context,
                                        nsresult aStatus,
                                        PRUint32 stringLen,
                                        const char *string)
{
#ifdef DEBUG_faulkner
  fprintf(stderr, "\n////////// nsSound::Init() in xlib called //////////\n");
#endif /* DEBUG_faulkner */
  return NS_OK;
}

NS_METHOD nsSound::Beep()
{
#ifdef DEBUG_faulkner
	fprintf(stderr, "\n////////// nsSound::Beep() in xlib called //////////\n");
#endif /* DEBUG_faulkner */
  return NS_OK;
}

NS_METHOD nsSound::Play(nsIURL *aURL)
{
#ifdef DEBUG_faulkner
  fprintf(stderr, "\n////////// nsSound::Play() in xlib called //////////\n");
#endif /* DEBUG_faulkner */
  return NS_OK;	
}

NS_IMETHODIMP nsSound::PlaySystemSound(const char *aSoundAlias)
{
  return Beep();
}
