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

#include "nsFileSpec.h"
#include "nsIFileSpec.h"
#include "nsSound.h"

#include "nsIURI.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "prmem.h"

#include "nsXPIDLString.h"
#include "nsCOMPtr.h"
#include "nsFileSpec.h"
#include "nsIChromeRegistry.h"
#include "nsIFileLocator.h"
#include "nsFileLocations.h"

static NS_DEFINE_CID(kChromeRegistryCID, NS_CHROMEREGISTRY_CID);

#include <unistd.h>

#include <gtk/gtk.h>
/* used with esd_open_sound */
static int esdref = -1;
static PRLibrary *elib = nsnull;
static PRLibrary *alib = nsnull;

#define AF_DEFAULT_TRACK 1001    // audiofile.h

// the following from esd.h

#define ESD_BITS8  (0x0000)
#define ESD_BITS16 (0x0001) 
#define ESD_MONO (0x0010)
#define ESD_STEREO (0x0020) 
#define ESD_STREAM (0x0000)
#define ESD_PLAY (0x1000)

typedef void * (PR_CALLBACK *afOpenFileType)(const char *in_file, const char *mode,
	void *unused ); // set unused to (char *) NULL
typedef double (PR_CALLBACK *afGetRateType)(void *in_file, int track );
typedef void (PR_CALLBACK *afGetSampleFormatType)(void *in_file, int track,
	int *format, int *width );
typedef int (PR_CALLBACK *afGetChannelsType)(void *in_file, int track );
typedef int (PR_CALLBACK *afCloseFileType)(void *in_file );

typedef int (PR_CALLBACK *EsdOpenSoundType)(const char *host);
typedef int (PR_CALLBACK *EsdCloseType)(int);

/* used to play the sounds from the find symbol call */
typedef int (PR_CALLBACK *EsdPlayStreamFallbackType)(int, int, const char *, const char *);

NS_IMPL_ISUPPORTS1(nsSound, nsISound);

////////////////////////////////////////////////////////////////////////
nsSound::nsSound()
{
  NS_INIT_REFCNT();
  mInited = PR_FALSE;

  mPlayBuf    = NULL;
  mBuffer     = NULL;
  mBufferSize = 0;
}

nsSound::~nsSound()
{
  /* see above comment */

  if (esdref != -1)
  {
    EsdCloseType EsdClose = (EsdCloseType) PR_FindSymbol(elib, "esd_close");
    (*EsdClose)(esdref);
    esdref = -1;
  }
  PR_FREEIF( mPlayBuf );
  PR_FREEIF( mBuffer );
  mInited = PR_FALSE;
}

nsresult nsSound::Init()
{
  /* we don't need to do esd_open_sound if we are only going to play files
     but we will if we want to do things like streams, etc
  */

  EsdOpenSoundType EsdOpenSound;

  elib = PR_LoadLibrary("libesd.so");

  if (!elib)
    return NS_ERROR_FAILURE;
  alib = PR_LoadLibrary("libaudiofile.so.0");

  if (!alib) 
    return NS_ERROR_FAILURE;
  EsdOpenSound = (EsdOpenSoundType) PR_FindSymbol(elib, "esd_open_sound");
  if ( !EsdOpenSound )
	return NS_ERROR_FAILURE;
  esdref = (*EsdOpenSound)("localhost");
  if ( !esdref )
	return NS_ERROR_FAILURE;
  return NS_OK;
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

nsresult nsSound::AllocateBuffers( void )
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
	::gdk_beep();

  return NS_OK;
}

NS_METHOD nsSound::Play(nsIURI *aURI)
{
  nsresult rv;
  char *spec;
  nsCOMPtr<nsIInputStream> inputStream;
  int mask, fd, format, width, channels;
  PRUint32 len, totalLen = 0;
  nsXPIDLCString tempPath;
  void *afRef;
  double rate;

  nsCOMPtr<nsIFileLocator> fl;

  if ( !mInited && NS_FAILED((rv=AllocateBuffers())) )
	return rv;

  rv = nsComponentManager::CreateInstance( "component://netscape/filelocator", 
	nsnull, NS_GET_IID(nsIFileLocator), getter_AddRefs(fl));

  if (NS_FAILED(rv))
    return NS_OK;

  // Build a fileSpec that points to the destination
  // (profile dir + chrome + package + provider + chrome.rdf)
  nsCOMPtr<nsIFileSpec> chromeFileInterface;
  fl->GetFileLocation(nsSpecialFileSpec::App_ChromeDirectory, 
	getter_AddRefs(chromeFileInterface));

  NS_WITH_SERVICE(nsIChromeRegistry, reg, kChromeRegistryCID, &rv);
  if (NS_FAILED(rv)) 
	return rv;
  nsCOMPtr<nsIURI> chromeURI;
  rv = aURI->Clone(getter_AddRefs(chromeURI)); // don't mangle the original
  if (NS_FAILED(rv)) 
	return rv;
  rv = reg->ConvertChromeURL(chromeURI);
  if (NS_FAILED(rv)) 
	return rv;

  if (elib && alib)
  {
    EsdPlayStreamFallbackType EsdPlayStreamFallback = (EsdPlayStreamFallbackType) PR_FindSymbol(elib, "esd_play_stream_fallback");
    afOpenFileType afOpenFile = (afOpenFileType) PR_FindSymbol(alib, "afOpenFile"); 
    afGetRateType afGetRate = (afGetRateType) PR_FindSymbol(alib, "afGetRate"); 
    afGetSampleFormatType afGetSampleFormat = (afGetSampleFormatType) PR_FindSymbol(alib, "afGetSampleFormat"); 
    afGetChannelsType afGetChannels = (afGetChannelsType) PR_FindSymbol(alib, "afGetChannels"); 
    afCloseFileType afCloseFile = (afCloseFileType) PR_FindSymbol(alib, "afCloseFile"); 

    if ( !EsdPlayStreamFallback || !afOpenFile || !afGetRate || !afGetSampleFormat || !afGetChannels || !afCloseFile ) {
	return NS_ERROR_FAILURE;
    }
    if ( mPlayBuf ) {
          PR_Free( this->mPlayBuf );
          this->mPlayBuf = (char *) NULL;
    }

    rv = NS_OpenURI(getter_AddRefs(inputStream), aURI);

    nsCOMPtr<nsIURL> aUrl = do_QueryInterface(chromeURI, &rv);
    nsFileSpec chromeFile;
    if (NS_SUCCEEDED(rv) && aUrl) { 
    	rv = aUrl->GetFilePath(getter_Copies(tempPath));
    	if ( NS_FAILED(rv) || tempPath == NULL ) {
		printf( "Sound: unable to get path on URI\n" );
        	return NS_ERROR_FAILURE;
    	}
        if (chromeFileInterface) {
    		chromeFileInterface->GetFileSpec(&chromeFile);
    		chromeFile += "/../";
    		chromeFile += (const char*)tempPath;
	}
    	rv = aUrl->GetSpec(&spec);
    	printf( "Sound: tempPath '%s' spec '%s' chromeFile '%s'\n", 
		(const char *) tempPath, spec, (const char *) chromeFile );
    } else {
        return NS_ERROR_FAILURE;
    }
    
    afRef = (*afOpenFile)(chromeFile, "r", (char *) NULL );
    if ( afRef == (void *) NULL ) {
	printf( "Sound: unable to open input stream '%s'\n", (const char *) tempPath );
        return NS_ERROR_FAILURE;
    }
    rate = (*afGetRate)( afRef, AF_DEFAULT_TRACK );
    (*afGetSampleFormat)( afRef, AF_DEFAULT_TRACK, &format, &width );
    channels = (*afGetChannels)( afRef, AF_DEFAULT_TRACK );
   
    printf( "Sound: rate %f width %d channels %d\n", rate, width, channels );
    mask = ESD_PLAY | ESD_STREAM; 
    if ( width == 8 )
        mask |= ESD_BITS8;
    else 
        mask |= ESD_BITS16;
    if ( channels == 1 )
        mask |= ESD_MONO;
    else 
        mask |= ESD_STEREO;

    fd = (*EsdPlayStreamFallback)(mask, (int) rate, "localhost", "mozillansSound"); 
    if (fd < 0) {
	return NS_ERROR_FAILURE;
    }
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
	write( fd, this->mPlayBuf, totalLen );
    close( fd );
    (*afCloseFile)( afRef );
    return NS_OK;
  } else 
	return NS_ERROR_NOT_IMPLEMENTED;
}
