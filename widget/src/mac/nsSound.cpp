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
#include "nsIAllocator.h"
#include "plstr.h"

#include "nsSound.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "prmem.h"

#include <Gestalt.h>
#include <Sound.h>
#include <QuickTimeComponents.h>

NS_IMPL_ISUPPORTS(nsSound, NS_GET_IID(nsISound));

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

  nsSound** mySound;

  *aSound = new nsSound();
  if (! *aSound)
    return NS_ERROR_OUT_OF_MEMORY;

  mySound = (nsSound **) aSound;
  
  NS_ADDREF(*aSound);
  return NS_OK;
}


NS_METHOD nsSound::Beep()
{
  ::SysBeep(1);

  return NS_OK;
}


#define kReadBufferSize		(4 * 1024)

// this currently does no cacheing of the sound buffer. It should
NS_METHOD nsSound::Play(nsIURI *aURI)
{
  // if quicktime is not installed, we can't do anything
  if (!HaveQuickTime())
    return NS_ERROR_NOT_IMPLEMENTED;
  
#if !TARGET_CARBON
  nsresult rv;
  nsCOMPtr<nsIInputStream> inputStream;

  rv = NS_OpenURI(getter_AddRefs(inputStream), aURI);
  if (NS_FAILED(rv)) return rv;
  
  PRUint32 handleLen = kReadBufferSize;

  Handle dataHandle = ::NewHandle(handleLen);  
  if (dataHandle == NULL)
  {
    OSErr err;
    dataHandle = ::TempNewHandle(handleLen, &err);
    if (!dataHandle) return NS_ERROR_OUT_OF_MEMORY;
  }

  PRUint32 len = 0;
  PRUint32 writeOffset = 0;
  
  do
  {
    ::HLock(dataHandle);
    rv = inputStream->Read(*dataHandle + writeOffset, kReadBufferSize, &len);
    ::HUnlock(dataHandle);
    
    writeOffset += len;
    
    // resize the handle in preparation for more
    if (len > 0)
    {
      ::SetHandleSize(dataHandle, writeOffset + kReadBufferSize);
      OSErr err = ::MemError();
      if (err != noErr)
      {
        ::DisposeHandle(dataHandle);
        return NS_ERROR_OUT_OF_MEMORY;
      }    
    }
    
  }
  while(len > 0);
  
  // resize the handle to the final size
  if (::GetHandleSize(dataHandle) != writeOffset)
  {
    ::SetHandleSize(dataHandle, writeOffset);
  }
  
  rv = PlaySound(dataHandle, writeOffset);
  
  ::DisposeHandle(dataHandle);
#endif
  return NS_OK;
}

nsresult nsSound::PlaySound(Handle waveDataHandle, long waveDataSize)
{
#if !TARGET_CARBON
  Handle                  dataRef = nil;
  Movie                   movie = nil;
  MovieImportComponent    miComponent = nil;
  Track                   targetTrack = nil;
  TimeValue               addedDuration = 0;
  long                    outFlags = 0;
  OSErr                   err = noErr;
  ComponentResult         compErr = noErr;
  
  err = ::PtrToHand(&waveDataHandle, &dataRef, sizeof(Handle));
  if (err != noErr) goto bail;
  
  miComponent = ::OpenDefaultComponent(MovieImportType, kQTFileTypeWave);
  if (!miComponent) {
    err = paramErr;
    goto bail;
  }
  
  movie = ::NewMovie(0);
  if (!movie) {
    err = paramErr;
    goto bail;
  }
  
  compErr = ::MovieImportDataRef(miComponent,
                              dataRef,
                              HandleDataHandlerSubType,
                              movie,
                              nil,
                              &targetTrack,
                              nil,
                              &addedDuration,
                              movieImportCreateTrack,
                              &outFlags);

  if (compErr != noErr) {
    err = compErr;
    goto bail;
  }
  
  ::SetMovieVolume(movie, kFullVolume);
  ::GoToBeginningOfMovie(movie);
  ::StartMovie(movie);

  while (! ::IsMovieDone(movie))
  {
    ::MoviesTask(movie, 0);
    err = ::GetMoviesError();
  }

bail:		// gasp, a goto label

  if (dataRef)
    ::DisposeHandle(dataRef);
  
  if (miComponent)
    ::CloseComponent(miComponent);
  
  if (movie)
    ::DisposeMovie(movie);
  
  return (err == noErr) ? NS_OK : NS_ERROR_FAILURE;
#else
  return NS_OK;
#endif
}


PRBool nsSound::HaveQuickTime()
{
  long  gestResult;
  OSErr err = Gestalt (gestaltQuickTime, &gestResult);
  return (err == noErr) && ((long)EnterMovies != kUnresolvedCFragSymbolAddress);
}
