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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */


#include "nscore.h"
#include "nsIMemory.h"
#include "plstr.h"
#include "stdio.h"

#include "nsSound.h"

#include <OS.h>
#include <SimpleGameSound.h>
#include <Entry.h>
#include <Beep.h>

NS_IMPL_ISUPPORTS1(nsSound, nsISound)

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

#if 0
// not currently used.. may go away
NS_METHOD nsSound::Init(void)
{
  return NS_OK;
}
#endif

NS_METHOD nsSound::Beep()
{
  ::beep();

  return NS_OK;
}

NS_METHOD nsSound::Play(nsIURL *aURL)
{
/*
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
*/
	return NS_OK;
}
