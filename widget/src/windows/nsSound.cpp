/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nscore.h"
#include "nsIAllocator.h"
#include "plstr.h"
#include "stdio.h"

#include <windows.h>

#include "nsSound.h"

NS_IMPL_ISUPPORTS(nsSound, nsCOMTypeInfo<nsISound>::GetIID());

////////////////////////////////////////////////////////////////////////
nsSound::nsSound()
{
  NS_INIT_REFCNT();
}

nsSound::~nsSound()
{
}

nsresult NS_NewSound(nsISound** aSound)
{
  NS_PRECONDITION(aSound != nsnull, "null ptr");
  if (! aSound)
    return NS_ERROR_NULL_POINTER;
  
  *aSound = new nsSound();
  if (! *aSound)
    return NS_ERROR_OUT_OF_MEMORY;
  
  NS_ADDREF(*aSound);
  return NS_OK;
}


NS_METHOD nsSound::Init(void)
{
  return NS_OK;
}


NS_METHOD nsSound::Beep()
{
  ::MessageBeep(0);

  return NS_OK;
}

NS_METHOD nsSound::Play(nsIFileSpec *filespec)
{
  char *filename;
  filespec->GetNativePath(&filename);

  ::PlaySound(filename, nsnull, SND_FILENAME | SND_NODEFAULT | SND_ASYNC);

  nsCRT::free(filename);

  return NS_OK;
}

