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

#include "prlink.h"

#include "nsSound.h"

#include "nsIURI.h"
#include "nsNetUtil.h"
#include "prmem.h"

#include <unistd.h>

#include <gtk/gtk.h>
/* used with esd_open_sound */
static int esdref = -1;
static PRLibrary *lib = nsnull;

#define ESD_BITS16 (0x0001)
#define ESD_STEREO (0x0020)
#define ESD_STREAM (0x0000)
#define ESD_PLAY (0x1000)
#define ESD_DEFAULT_RATE 44100

typedef int (PR_CALLBACK *EsdOpenSoundType)(const char *host);
typedef int (PR_CALLBACK *EsdCloseType)(int);

/* used to play the sounds from the find symbol call */
typedef int (PR_CALLBACK *EsdPlayStreamFallbackType)(int, int, const char *, const char *);

NS_IMPL_ISUPPORTS1(nsSound, nsISound);

////////////////////////////////////////////////////////////////////////
nsSound::nsSound()
{
  NS_INIT_REFCNT();

  /* we don't need to do esd_open_sound if we are only going to play files
     but we will if we want to do things like streams, etc
  */

  EsdOpenSoundType EsdOpenSound;

  lib = PR_LoadLibrary("libesd.so");

  if (!lib)
    return;
  EsdOpenSound = (EsdOpenSoundType) PR_FindSymbol(lib, "esd_open_sound");
  esdref = (*EsdOpenSound)("localhost");
}

nsSound::~nsSound()
{
  /* see above comment */

  if (esdref != -1)
  {
    EsdCloseType EsdClose = (EsdCloseType) PR_FindSymbol(lib, "esd_close");
    (*EsdClose)(esdref);
    esdref = -1;
  }
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


NS_METHOD nsSound::Init(void)
{
  return NS_OK;
}


NS_METHOD nsSound::Beep()
{
	::gdk_beep();

  return NS_OK;
}

NS_METHOD nsSound::Play(nsIURI *aURI)
{
  nsresult rv;
  nsIInputStream *inputStream;
  int fd;
  PRUint32 totalLen = 0;
  PRUint32 len;

  if (lib)
  {
    EsdPlayStreamFallbackType EsdPlayStreamFallback = (EsdPlayStreamFallbackType) PR_FindSymbol(lib, "esd_play_stream_fallback");

    if ( mPlayBuf ) {
//          ::PlaySound(nsnull, nsnull, 0);       // stop what might be playing so we can free
          PR_Free( this->mPlayBuf );
          this->mPlayBuf = (char *) NULL;
    }
    rv = NS_OpenURI(&inputStream, aURI);
    fd = (*EsdPlayStreamFallback)(ESD_BITS16 | ESD_STEREO | ESD_STREAM | ESD_PLAY, ESD_DEFAULT_RATE, "localhost", "YabbaDabbaDoo"); 
    if (fd < 0) {
	NS_IF_RELEASE( inputStream );
	return NS_ERROR_FAILURE;
    }
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
	write( fd, this->mPlayBuf, totalLen );
    close( fd );
    NS_IF_RELEASE( inputStream );
    return NS_OK;
  } else 
	return NS_ERROR_NOT_IMPLEMENTED;
}
