/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

NS_IMPL_ISUPPORTS2(nsSound, nsISound, nsIStreamLoaderObserver);

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
  ::SysBeep(1);

  return NS_OK;
}


#define kReadBufferSize		(4 * 1024)

// this currently does no cacheing of the sound buffer. It should
NS_METHOD nsSound::Play(nsIURL *aURL)
{
  // if quicktime is not installed, we can't do anything
  if (!HaveQuickTime())
    return NS_ERROR_NOT_IMPLEMENTED;

  nsCOMPtr<nsIStreamLoader> loader;
  nsresult rv = NS_NewStreamLoader(getter_AddRefs(loader), aURL, this);

  return rv;
}


NS_IMETHODIMP nsSound::OnStreamComplete(nsIStreamLoader *aLoader,
                                        nsISupports *context,
                                        nsresult aStatus,
                                        PRUint32 stringLen,
                                        const char *stringData)
{
  if (NS_FAILED(aStatus))
    return NS_ERROR_FAILURE;

  // we should really verify that this is a .wav file
  OSErr     err;
  Handle    dataHandle = ::TempNewHandle(stringLen, &err);
  if (!dataHandle) return NS_ERROR_OUT_OF_MEMORY;

  BlockMoveData(stringData, *dataHandle, stringLen);

  nsresult rv = PlaySound(dataHandle, stringLen);

  ::DisposeHandle(dataHandle);
  return rv;
}

nsresult nsSound::PlaySound(Handle waveDataHandle, long waveDataSize)
{
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
}


PRBool nsSound::HaveQuickTime()
{
  long  gestResult;
  OSErr err = Gestalt (gestaltQuickTime, &gestResult);
  return (err == noErr) && ((long)EnterMovies != kUnresolvedCFragSymbolAddress);
}
