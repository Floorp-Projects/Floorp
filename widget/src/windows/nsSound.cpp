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
#include <stdio.h>

#include <windows.h>

#include "nsSound.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "prmem.h"

NS_IMPL_ISUPPORTS(nsSound, NS_GET_IID(nsISound));

////////////////////////////////////////////////////////////////////////
nsSound::nsSound()
{
  NS_INIT_REFCNT();
  mInited = PR_FALSE;
}

nsSound::~nsSound()
{
	if (mPlayBuf)
		PR_Free( mPlayBuf );
	if (mBuffer)
		PR_Free( mBuffer );
	mInited = PR_FALSE;
}

nsresult NS_NewSound(nsISound** aSound)
{
  NS_PRECONDITION(aSound != nsnull, "null ptr");
  if (!aSound)
    return NS_ERROR_NULL_POINTER;

  nsSound* mySound = nsnull;
  NS_NEWXPCOM(mySound, nsSound);
  if (!mySound)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = mySound->Init();
  if (NS_FAILED(rv)) {
    delete mySound;
    return rv;
  }

  // QI does the addref
  return mySound->QueryInterface(NS_GET_IID(nsISound), (void**)aSound);

}

nsresult nsSound::Init() 
{
	return NS_OK;
}

nsresult nsSound::AllocateBuffers()
{
  nsresult rv = NS_OK;
  if ( mInited == PR_TRUE )      
        return( rv );
  mBufferSize = 4098;
  mBuffer = (char *) PR_Malloc( mBufferSize );
  if ( mBuffer == (char *) NULL ) {
        rv = NS_ERROR_OUT_OF_MEMORY;
        return( rv );
  }
  mPlayBuf = (char *) NULL;
  mInited = PR_TRUE;
  return rv;
}


NS_METHOD nsSound::Beep()
{
  ::MessageBeep(0);

  return NS_OK;
}

NS_METHOD nsSound::Play(nsIURI *aURI)
{
  nsresult rv;
  nsCOMPtr <nsIInputStream>inputStream;
  PRUint32 totalLen = 0;
  PRUint32 len;
 
  if ( !mInited && NS_FAILED((rv = AllocateBuffers())) )
        return rv;
 
  if ( mPlayBuf ) {
	  ::PlaySound(nsnull, nsnull, 0);	// stop what might be playing so we can free 
	  PR_Free( this->mPlayBuf );
	  this->mPlayBuf = (char *) NULL;
  }
  rv = NS_OpenURI(getter_AddRefs(inputStream), aURI);
  if (NS_FAILED(rv)) 
	  return rv;
  do {
	rv = inputStream->Read(this->mBuffer, this->mBufferSize, &len);
	if ( len ) {
		totalLen += len;
		if ( this->mPlayBuf == (char *) NULL ) {
			this->mPlayBuf = (char *) PR_Malloc( len );
			if ( this->mPlayBuf == (char *) NULL ) {
					return NS_ERROR_OUT_OF_MEMORY;
			}
			memcpy( this->mPlayBuf, this->mBuffer, len );
		}
		else {
			this->mPlayBuf = (char *) PR_Realloc( this->mPlayBuf, totalLen );
			if ( this->mPlayBuf == (char *) NULL ) {
					return NS_ERROR_OUT_OF_MEMORY;
			}
			memcpy( this->mPlayBuf + (totalLen - len), this->mBuffer, len );
		}
	}
  } while (len > 0);
  if ( this->mPlayBuf != (char *) NULL )
	::PlaySound(this->mPlayBuf, nsnull, SND_MEMORY | SND_NODEFAULT | SND_ASYNC);
  return NS_OK;
}

