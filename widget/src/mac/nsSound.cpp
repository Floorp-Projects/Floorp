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

#include "nsSound.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "prmem.h"

NS_IMPL_ISUPPORTS(nsSound, nsCOMTypeInfo<nsISound>::GetIID());

////////////////////////////////////////////////////////////////////////
nsSound::nsSound()
{
  NS_INIT_REFCNT();
}

nsSound::~nsSound()
{
        if (mPlayBuf)
                PR_Free( mPlayBuf );
        if (mBuffer)
                PR_Free( mBuffer );
}

nsresult NS_NewSound(nsISound** aSound)
{
  NS_PRECONDITION(aSound != nsnull, "null ptr");
  if (! aSound)
    return NS_ERROR_NULL_POINTER;

  nsSound** mySound;

  *aSound = new nsSound();
  if (! *aSound)
    return NS_ERROR_OUT_OF_MEMORY;

  mySound = (nsSound **) aSound;
  (*mySound)->mBufferSize = 4098;
  (*mySound)->mBuffer = (char *) PR_Malloc( (*mySound)->mBufferSize );
  if ( (*mySound)->mBuffer == (char *) NULL )
        return NS_ERROR_OUT_OF_MEMORY;
  (*mySound)->mPlayBuf = (char *) NULL;
  
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
  ::SysBeep(1);

  return NS_OK;
}

NS_METHOD nsSound::Play(nsIURI *aURI)
{
#if 0
  nsresult rv;
  nsIInputStream *inputStream;
  PRUint32 totalLen = 0;
  PRUint32 len;

  if ( mPlayBuf ) {
          ::PlaySound(nsnull, nsnull, 0);       // stop what might be playing so we can free
          PR_Free( this->mPlayBuf );
          this->mPlayBuf = (char *) NULL;
  }
  rv = NS_OpenURI(&inputStream, aURI);
  if (NS_FAILED(rv))
          return rv;
  do {
        rv = inputStream->Read(this->mBuffer, this->mBufferSize, &len);
        if ( len ) {
                totalLen += len;
                if ( this->mPlayBuf == (char *) NULL ) {
                        this->mPlayBuf = (char *) PR_Malloc( len );
                        if ( this->mPlayBuf == (char *) NULL ) {
                                        NS_IF_RELEASE( inputStream );
                                        return NS_ERROR_OUT_OF_MEMORY;
                        }
                        memcpy( this->mPlayBuf, this->mBuffer, len );
                }
                else {
                        this->mPlayBuf = (char *) PR_Realloc( this->mPlayBuf, totalLen );
                        if ( this->mPlayBuf == (char *) NULL ) {
                                        NS_IF_RELEASE( inputStream );
                                        return NS_ERROR_OUT_OF_MEMORY;
                        }
                        memcpy( this->mPlayBuf + (totalLen - len), this->mBuffer, len );
                }
        }
  } while (len > 0);
  if ( this->mPlayBuf != (char *) NULL )
/* XXX call something similar to Win32 PlaySound() here */
        ::PlaySound(this->mPlayBuf, nsnull, SND_MEMORY | SND_NODEFAULT | SND_ASYNC);
  NS_IF_RELEASE( inputStream );
  return NS_OK;
#endif
  NS_NOTYETIMPLEMENTED("nsSound::Play");
  return NS_OK;
}
