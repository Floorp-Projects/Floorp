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
 *   Simon Fraser   <sfraser@netscape.com>
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
#include "nsVoidArray.h"

#include "nsIURI.h"
#include "nsNetUtil.h"
#include "prmem.h"
#include "nsGfxUtils.h"

#include "nsIStreamLoader.h"
#include "nsICacheService.h"
#include "nsICacheSession.h"
#include "nsICacheEntryDescriptor.h"
#include "nsICachingChannel.h"

#include "nsITimer.h"
#include "nsITimerCallback.h"

#include <Gestalt.h>
#include <Sound.h>
#include <Movies.h>
#include <QuickTimeComponents.h>

#include "nsSound.h"

//#define SOUND_DEBUG

#pragma mark nsSoundRequest

class nsSoundRequest :  public nsIStreamLoaderObserver,
                        public nsITimerCallback
{
public:

                    nsSoundRequest();
  virtual           ~nsSoundRequest();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLOADEROBSERVER

  nsresult          Init(nsISound* aSound, nsIURL *aURL);
  nsresult          PlaySound();
  
  // nsITimerCallback
  NS_IMETHOD_(void) Notify(nsITimer *timer);
  

  static nsSoundRequest*  GetFromISupports(nsISupports* inSupports);

protected:


  OSType            GetFileFormat(const char* inData, long inDataSize, const nsACString& contentType);
  
  OSErr             ImportMovie(Handle inDataHandle, long inDataSize, const nsACString& contentType);
  PRBool            HaveQuickTime();

  nsresult          Cleanup();
  void              DisposeMovieData();
  
  PRBool            IsAnyMoviePlaying();
  
  OSErr             TaskActiveMovies(PRBool *outAllMoviesDone);

  static PRBool     TaskOneMovie(Movie inMovie);    // return true if done

protected:

  nsCOMPtr<nsISound>        mSound;       // back ptr, owned and released when play done  
  nsCOMPtr<nsITimer>        mTimer;
  
  Movie                     mMovie;       // the original movie, kept around as long as this request is cached
  Handle                    mDataHandle;  // data handle, has to persist for the lifetime of any movies
                                          // depending on it
  
  nsVoidArray               mMovies;      // list of playing movie clones, which are transient.
};

#pragma mark -


static PRUint32
SecondsFromPRTime(PRTime prTime)
{
  PRInt64 microSecondsPerSecond, intermediateResult;
  PRUint32 seconds;
  
  LL_I2L(microSecondsPerSecond, PR_USEC_PER_SEC);
  LL_DIV(intermediateResult, prTime, microSecondsPerSecond);
  LL_L2UI(seconds, intermediateResult);
  return seconds;
}

#pragma mark -

nsSound::nsSound()
{
  NS_INIT_REFCNT();
#ifdef SOUND_DEBUG
  printf("%%%%%%%% Made nsSound\n");
#endif
}

nsSound::~nsSound()
{
#ifdef SOUND_DEBUG
  printf("%%%%%%%% Deleted nsSound\n");
#endif
}

NS_IMPL_ISUPPORTS1(nsSound, nsISound);

NS_METHOD
nsSound::Beep()
{
  ::SysBeep(1);
  return NS_OK;
}

NS_IMETHODIMP
nsSound::PlaySystemSound(const char *aSoundAlias)
{
  return Beep();
}

// this currently does no caching of the sound buffer. It should
NS_METHOD
nsSound::Play(nsIURL *aURL)
{
  NS_ENSURE_ARG(aURL);

  nsresult rv;

  // try to get from cache
  nsCOMPtr<nsISupports>   requestSupports;
  (void)GetSoundFromCache(aURL, getter_AddRefs(requestSupports));
  if (requestSupports)
  {
    nsSoundRequest* cachedRequest = nsSoundRequest::GetFromISupports(requestSupports);
    // if it was cached, start playing right away
    cachedRequest->PlaySound();
  }
  else
  {
    nsSoundRequest* soundRequest;
    NS_NEWXPCOM(soundRequest, nsSoundRequest);
    if (!soundRequest)
      return NS_ERROR_OUT_OF_MEMORY;

    requestSupports = NS_STATIC_CAST(nsIStreamLoaderObserver*, soundRequest);
    nsresult  rv = soundRequest->Init(this, aURL);
    if (NS_FAILED(rv))
      return rv;
  }
  
  rv = AddRequest(requestSupports);
  if (NS_FAILED(rv))
    return rv;

  return NS_OK;
}


nsresult
nsSound::AddRequest(nsISupports* aSoundRequest)
{
  // only add if not already in the list
  PRInt32   index = mSoundRequests.IndexOf(aSoundRequest);
  if (index == -1)
  {
    nsresult  appended = mSoundRequests.AppendElement(aSoundRequest);
    if (!appended)
      return NS_ERROR_FAILURE;
  }
  
  return NS_OK;
}


nsresult
nsSound::RemoveRequest(nsISupports* aSoundRequest)
{
  nsresult  removed = mSoundRequests.RemoveElement(aSoundRequest);
  if (!removed)
    return NS_ERROR_FAILURE;

  return NS_OK;
}


nsresult
nsSound::GetCacheSession(nsICacheSession** outCacheSession)
{
  nsresult rv;

  nsCOMPtr<nsICacheService> cacheService = do_GetService("@mozilla.org/network/cache-service;1", &rv);
  if (NS_FAILED(rv)) return rv;
  
  return cacheService->CreateSession("sound",
                              nsICache::NOT_STREAM_BASED,
                              PR_FALSE, outCacheSession);
}


nsresult
nsSound::GetSoundFromCache(nsIURI* inURI, nsISupports** outSound)
{
  nsresult rv;
  
  nsXPIDLCString uriSpec;
  inURI->GetSpec(getter_Copies(uriSpec));

  nsCOMPtr<nsICacheSession> cacheSession;
  rv = GetCacheSession(getter_AddRefs(cacheSession));
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsICacheEntryDescriptor> entry;
  rv = cacheSession->OpenCacheEntry(uriSpec, nsICache::ACCESS_READ, nsICache::BLOCKING, getter_AddRefs(entry));

#ifdef SOUND_DEBUG
  printf("Got sound from cache with rv %ld\n", rv);
#endif

  if (NS_FAILED(rv)) return rv;
  
  return entry->GetCacheElement(outSound);
}


nsresult
nsSound::PutSoundInCache(nsIChannel* inChannel, PRUint32 inDataSize, nsISupports* inSound)
{
  nsresult rv;
  
  NS_ENSURE_ARG(inChannel && inSound);
  
  nsCOMPtr<nsIURI>  uri;
  inChannel->GetOriginalURI(getter_AddRefs(uri));
  if (!uri) return NS_ERROR_FAILURE;
  
  nsXPIDLCString uriSpec;
  rv = uri->GetSpec(getter_Copies(uriSpec));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsICacheSession> cacheSession;
  rv = GetCacheSession(getter_AddRefs(cacheSession));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsICacheEntryDescriptor> entry;
  rv = cacheSession->OpenCacheEntry(uriSpec, nsICache::ACCESS_WRITE, nsICache::BLOCKING, getter_AddRefs(entry));
#ifdef SOUND_DEBUG
  printf("Put sound in cache with rv %ld\n", rv);
#endif

  if (NS_FAILED(rv)) return rv;

  rv = entry->SetCacheElement(inSound);
  if (NS_FAILED(rv)) return rv;

  rv = entry->SetDataSize(inDataSize);
  if (NS_FAILED(rv)) return rv;
  
  PRUint32    expirationTime = 0;

  // try to get the expiration time from the URI load
  nsCOMPtr<nsICachingChannel> cachingChannel = do_QueryInterface(inChannel);
  if (cachingChannel)
  {
    nsCOMPtr<nsISupports> cacheToken;
    cachingChannel->GetCacheToken(getter_AddRefs(cacheToken));
    nsCOMPtr<nsICacheEntryInfo> cacheEntryInfo = do_QueryInterface(cacheToken);
    if (cacheEntryInfo)
    {
      cacheEntryInfo->GetExpirationTime(&expirationTime);
    }
  }
  
  if (expirationTime == 0)
  {
    // set it to some reasonable default, like now + 24 hours
    expirationTime = SecondsFromPRTime(PR_Now()) + 60 * 60 * 24;
  }
  
  rv = entry->SetExpirationTime(expirationTime);
  if (NS_FAILED(rv)) return rv;
  
  return entry->MarkValid();
}


#pragma mark -


NS_IMPL_ISUPPORTS2(nsSoundRequest, nsIStreamLoaderObserver, nsITimerCallback);

////////////////////////////////////////////////////////////////////////
nsSoundRequest::nsSoundRequest()
: mMovie(nsnull)
, mDataHandle(nsnull)
{
  NS_INIT_REFCNT();
#ifdef SOUND_DEBUG
  printf("%%%%%%%% Made nsSoundRequest\n");
#endif
}

nsSoundRequest::~nsSoundRequest()
{
#ifdef SOUND_DEBUG
  printf("%%%%%%%% Deleted nsSoundRequest\n");
#endif
  DisposeMovieData();
}


nsresult
nsSoundRequest::Init(nsISound* aSound, nsIURL *aURL)
{
  NS_ENSURE_ARG(aURL && aSound);

  mSound = aSound;
  
  // if quicktime is not installed, we can't do anything
  if (!HaveQuickTime())
    return NS_ERROR_NOT_IMPLEMENTED;

#ifdef SOUND_DEBUG
  printf("%%%%%%%% Playing nsSound\n");
#endif
  NS_ASSERTION(mMovie == nsnull, "nsSound being played twice");
  
  nsCOMPtr<nsIStreamLoader> streamLoader;
  return NS_NewStreamLoader(getter_AddRefs(streamLoader), aURL, NS_STATIC_CAST(nsIStreamLoaderObserver*, this));
}

NS_IMETHODIMP
nsSoundRequest::OnStreamComplete(nsIStreamLoader *aLoader,
                                        nsISupports *context,
                                        nsresult aStatus,
                                        PRUint32 stringLen,
                                        const char *stringData)
{
  NS_ENSURE_ARG(aLoader);
  
  if (NS_FAILED(aStatus))
    return NS_ERROR_FAILURE;

  nsXPIDLCString  contentType;

  nsCOMPtr<nsIRequest>    request;
  aLoader->GetRequest(getter_AddRefs(request));
  nsCOMPtr<nsIChannel>  channel = do_QueryInterface(request);
  if (channel)
    channel->GetContentType(getter_Copies(contentType));
  
  // we could use a Pointer data handler type, and avoid this
  // allocation/copy, in QuickTime 5 and above.
  OSErr     err;
  mDataHandle = ::TempNewHandle(stringLen, &err);
  if (!mDataHandle) return NS_ERROR_OUT_OF_MEMORY;

  ::BlockMoveData(stringData, *mDataHandle, stringLen);

  NS_ASSERTION(mMovie == nsnull, "nsSoundRequest has a movie already");
  
  err = ImportMovie(mDataHandle, stringLen, contentType);
  if (err != noErr) {
    Cleanup();
    return NS_ERROR_FAILURE;
  }

  nsSound*    macSound = NS_REINTERPRET_CAST(nsSound*, mSound.get());
  NS_ASSERTION(macSound, "Should have nsSound here");
  
  // put it in the cache. Not vital that this succeeds.
  // for the data size we just use the string data, since the movie simply wraps this
  // (we have to keep the handle around until the movies are done playing)
  nsresult rv = macSound->PutSoundInCache(channel, stringLen, NS_STATIC_CAST(nsIStreamLoaderObserver*, this));
  NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to put sound in cache");
  
  return PlaySound();
}


OSType
nsSoundRequest::GetFileFormat(const char* inData, long inDataSize, const nsACString& contentType)
{
  OSType    fileFormat = kQTFileTypeMovie;    // Default to just treating it like a movie.
                                              // Hopefully QuickTime will be able to import it.
  if (inDataSize >= 16)
  {
    // look for WAVE
    const char* dataPtr = inData;
    if (*(OSType *)dataPtr == 'RIFF')
    {
      dataPtr += 4;     // skip RIFF
      dataPtr += 4;     // skip length bytes
      if (*(OSType *)dataPtr == 'WAVE')
        return kQTFileTypeWave;
    }
    
    // look for AIFF
    dataPtr = inData;
    if (*(OSType *)dataPtr == 'FORM')
    {
      dataPtr += 4;     // skip FORM
      dataPtr += 4;     // skip length bytes
      if (*(OSType *)dataPtr == 'AIFF')
        return kQTFileTypeAIFF;
       
      if (*(OSType *)dataPtr == 'AIFC')
        return kQTFileTypeAIFC;
    }
  }
  
  if (inDataSize >= 4)
  {
    // look for midi
    if (*(OSType *)inData == 'MThd')
      return kQTFileTypeMIDI;
    
    // look for µLaw/Next-Sun file format (.au)
    if (*(OSType *)inData == '.snd')
      return kQTFileTypeMuLaw;
    
  }
  
  // MP3 files have a complex format that is not easily sniffed. Just go by
  // MIME type.
  if (contentType.Equals("audio/mpeg")    ||
      contentType.Equals("audio/mp3")     ||
      contentType.Equals("audio/mpeg3")   ||
      contentType.Equals("audio/x-mpeg3") ||
      contentType.Equals("audio/x-mp3")   ||
      contentType.Equals("audio/x-mpeg3"))
  {
    fileFormat = 'MP3 ';      // not sure why there is no enum for this
  }
    
  return fileFormat;
}

nsresult
nsSoundRequest::PlaySound()
{
  nsresult rv;

  // we'll have a timer already if the sound is still playing from a previous
  // request. In that case, we cloen the movie into a new one, so we can play it
  // again from the start.
  if (!mTimer)
  {  
    mTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);    // release previous timer, if any
    if (NS_FAILED(rv)) {
      Cleanup();
      return rv;
    }
    
    const PRInt32   kMovieTimerInterval = 250;      // 250 milliseconds
    rv = mTimer->Init(NS_STATIC_CAST(nsITimerCallback*, this), kMovieTimerInterval,
            NS_PRIORITY_NORMAL, NS_TYPE_REPEATING_PRECISE);
    if (NS_FAILED(rv)) {
      Cleanup();
      return rv;
    }
  }
  
  Movie   movieToPlay = mMovie;
  
  if (!::IsMovieDone(mMovie))     // if the current movie is still playing, clone it
  {
    Movie   newMovie = ::NewMovie(0);
    if (!newMovie) return NS_ERROR_FAILURE;
    
    // note that this copies refs, not all the data. So it should be fast
    OSErr err = ::InsertMovieSegment(mMovie, newMovie, 0, ::GetMovieDuration(mMovie), 0);
    if (err != noErr)
    {
      ::DisposeMovie(newMovie);
      return NS_ERROR_FAILURE;
    }
    
    // append it to the array
    PRBool  appended = mMovies.AppendElement((void *)newMovie);
    if (!appended) 
    {
      ::DisposeMovie(newMovie);
      return NS_ERROR_FAILURE;
    }

    movieToPlay = newMovie;
  }
  
  ::SetMovieVolume(movieToPlay, kFullVolume);
  ::GoToBeginningOfMovie(movieToPlay);
  ::StartMovie(movieToPlay);
  ::MoviesTask(movieToPlay, 0);
  
#ifdef SOUND_DEBUG
  printf("Starting movie playback\n");
#endif
  return NS_OK;
}

NS_IMETHODIMP_(void)
nsSoundRequest::Notify(nsITimer *timer)
{
  if (!mMovie)
  {
    NS_ASSERTION(0, "nsSoundRequest has no movie in timer callback");
    return;
  }
  
#ifdef SOUND_DEBUG
  printf("In movie timer callback\n");
#endif

  PRBool  moviesDone;

  TaskActiveMovies(&moviesDone);
  
  // we're done for now. Remember that this nsSoundRequest might be in the cache,
  // so won't necessarily go away.
  if (moviesDone)
    Cleanup();
}

OSErr
nsSoundRequest::ImportMovie(Handle inDataHandle, long inDataSize, const nsACString& contentType)
{
  Handle                  dataRef = nil;
  OSErr                   err = noErr;
  OSType                  fileFormat;
  
  {
    StHandleLocker  locker(inDataHandle);
    fileFormat = GetFileFormat(*inDataHandle, inDataSize, contentType);
  }

  err = ::PtrToHand(&inDataHandle, &dataRef, sizeof(Handle));
  if (err != noErr)
    return err;

  {
    MovieImportComponent  miComponent = ::OpenDefaultComponent(MovieImportType, fileFormat);
    Track                 targetTrack = nil;
    TimeValue             addedDuration = 0;
    long                  outFlags = 0;
    ComponentResult       compErr = noErr;

    if (!miComponent) {
      err = paramErr;
      goto bail;
    }
    
    NS_ASSERTION(mMovie == nsnull, "nsSoundRequest already has movie");
    mMovie = ::NewMovie(0);
    if (!mMovie) {
      err = ::GetMoviesError();
      goto bail;
    }

    compErr = ::MovieImportDataRef(miComponent,
                                dataRef,
                                HandleDataHandlerSubType,
                                mMovie,
                                nil,
                                &targetTrack,
                                nil,
                                &addedDuration,
                                movieImportCreateTrack,
                                &outFlags);

    if (compErr != noErr) {
      ::DisposeMovie(mMovie);
      mMovie = nil;
      err = compErr;
      goto bail;
    }

    // ensure that the track never draws on screen, otherwise we might be
    // suspecptible to spoofing attacks
    Rect   movieRect = {0};
    ::SetMovieBox(mMovie, &movieRect);
    
    ::GoToEndOfMovie(mMovie);   // simplifies the logic in PlaySound()
    
  bail:
    if (miComponent)
      ::CloseComponent(miComponent);
  }

  if (dataRef)
    ::DisposeHandle(dataRef);
  
  return err;
}

nsresult
nsSoundRequest::Cleanup()
{
  nsresult rv = NS_OK;
  
#ifdef SOUND_DEBUG
  printf("Movie playback done\n");
#endif
  
  // kill the timer
  if (mTimer) {
    mTimer->Cancel();
    mTimer = nsnull;
  }
    
  NS_ASSERTION(mMovies.Count() == 0, "Should not have any movies running here");  

  // remove from parent array. This could be the last ref, so we might be deleted here.
  if (mSound.get())
  {
    nsSound*    macSound = NS_REINTERPRET_CAST(nsSound*, mSound.get());
    rv = macSound->RemoveRequest(NS_STATIC_CAST(nsIStreamLoaderObserver*, this));
    mSound = nsnull;
  }
  
  return rv;
}

void
nsSoundRequest::DisposeMovieData()
{
  for (PRInt32 i = 0; i < mMovies.Count(); i ++)
  {
    Movie   thisMovie = (Movie)mMovies.ElementAt(i);
    ::DisposeMovie(mMovie);
  }
  
  mMovies.Clear();
  
  if (mMovie) {
    ::DisposeMovie(mMovie);
    mMovie = nsnull;
  }

  if (mDataHandle) {
    ::DisposeHandle(mDataHandle);
    mDataHandle = nsnull;
  } 
}


PRBool
nsSoundRequest::TaskOneMovie(Movie inMovie)    // return true if done
{
  PRBool    movieDone = PR_FALSE;
  
  ComponentResult status = ::GetMovieStatus(inMovie, nil);
  NS_ASSERTION(status == noErr, "Movie bad");
  if (status != noErr) {
    ::StopMovie(inMovie);
    movieDone = PR_TRUE;
  }

  movieDone |= ::IsMovieDone(inMovie);
  
  if (!movieDone)
    ::MoviesTask(inMovie, 0);

  return movieDone;
}

nsSoundRequest*
nsSoundRequest::GetFromISupports(nsISupports* inSupports)
{
  if (!inSupports) return nsnull;
  
  // test to see if this is really a nsSoundRequest by trying a QI to both
  // interfaces we support
  nsCOMPtr<nsIStreamLoaderObserver> loaderObserver  = do_QueryInterface(inSupports);
  nsCOMPtr<nsITimerCallback>        timerCallback   = do_QueryInterface(inSupports);
  if (!loaderObserver || !timerCallback) return nsnull;
  
  return NS_REINTERPRET_CAST(nsSoundRequest*, inSupports);
}

OSErr
nsSoundRequest::TaskActiveMovies(PRBool *outAllMoviesDone)
{
  PRBool    allMoviesDone = PR_FALSE;

  allMoviesDone = TaskOneMovie(mMovie);

  PRInt32 initMovieCount = mMovies.Count();
  PRInt32 curIndex = 0;
  
  for (PRInt32 i = 0; i < initMovieCount; i ++)
  {
    Movie   thisMovie     = (Movie)mMovies.ElementAt(curIndex);
    PRBool  thisMovieDone = TaskOneMovie(thisMovie);
    
    if (thisMovieDone)    // remove finished movies from the array
    {
      mMovies.RemoveElementAt(curIndex);
      ::DisposeMovie(thisMovie);
      // curIndex doesn't change
    }
    else
    {
      curIndex ++;
    } 
    allMoviesDone &= thisMovieDone;
  }
  
  *outAllMoviesDone = allMoviesDone;
  return noErr;  
}


PRBool
nsSoundRequest::IsAnyMoviePlaying()
{
  if (!::IsMovieDone(mMovie))
    return PR_TRUE;
  
  for (PRInt32 i = 0; i < mMovies.Count(); i ++)
  {
    Movie   thisMovie = (Movie)mMovies.ElementAt(i);
    if (!::IsMovieDone(thisMovie))
      return PR_TRUE;
  }
    
  return PR_FALSE;
}

PRBool
nsSoundRequest::HaveQuickTime()
{
  long  gestResult;
  OSErr err = Gestalt (gestaltQuickTime, &gestResult);
  return (err == noErr) && ((long)EnterMovies != kUnresolvedCFragSymbolAddress);
}

