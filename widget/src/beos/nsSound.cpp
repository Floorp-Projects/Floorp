/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


#include "nscore.h"
#include "nsIAllocator.h"
#include "plstr.h"
#include "stdio.h"

#include "nsSound.h"

#include <OS.h>
#include <SimpleGameSound.h>
#include <Entry.h>
#include <Beep.h>

NS_IMPL_ISUPPORTS(nsSound, nsCOMTypeInfo<nsISound>::GetIID());

////////////////////////////////////////////////////////////////////////
nsSound::nsSound()
 : mSound(0)
{
  NS_INIT_REFCNT();
}

nsSound::~nsSound()
{
	delete mSound;
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

// not currently used.. may go away
NS_METHOD nsSound::Init(void)
{
  return NS_OK;
}


NS_METHOD nsSound::Beep()
{
  ::beep();

  return NS_OK;
}

NS_METHOD nsSound::Play(nsIFileSpec *filespec)
{
	char *filename;
	filespec->GetNativePath(&filename);

	delete mSound;
	entry_ref	ref;
	BEntry e(filename);
	e.GetRef(&ref);
	mSound = new BSimpleGameSound(&ref);
	if(mSound->InitCheck() == B_OK)
		mSound->StartPlaying();

	nsCRT::free(filename);

	return NS_OK;
}
