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

#include "nsIInternetConfigService.h"

#include "nsITimer.h"
#include "nsITimerCallback.h"

#include <Gestalt.h>
#include <Sound.h>
#include <Movies.h>
#include <QuickTimeComponents.h>

#include "nsSound.h"

//#define SOUND_DEBUG

#pragma mark nsSoundRequest

// pure virtual base class for different types of sound requests
class nsSoundRequest : public nsITimerCallback
{
public:

                    nsSoundRequest();
  virtual           ~nsSoundRequest();

  NS_DECL_ISUPPORTS

  // nsITimerCallback
  NS_IMETHOD_(void) Notify(nsITimer *timer) = 0;    // pure virtual

  virtual nsresult  PlaySound() = 0;

  static nsSoundRequest*  GetFromISupports(nsISupports* inSupports);

protected:

  nsresult          Cleanup();

protected:

  nsCOMPtr<nsISound>  mSound;       // back ptr, owned and released when play done  
  nsCOMPtr<nsITimer>  mTimer;
};


// concrete class for playing system sounds asynchronously
class nsSystemSoundRequest : public nsSoundRequest
{
public:

                    nsSystemSoundRequest();
  virtual           ~nsSystemSoundRequest();

  NS_DECL_ISUPPORTS_INHERITED

  // nsITimerCallback
  NS_IMETHOD_(void) Notify(nsITimer *timer);

  nsresult          Init(nsISound* aSound, ConstStr255Param aSoundName);
  virtual nsresult  PlaySound();
  
protected:

  static pascal void SoundCallback(SndChannelPtr chan, SndCommand *theCmd);

  void               DonePlaying();

protected:
  
  Handle            mSoundHandle;     // resource handle.
  SndChannelPtr     mSndChannel;
  SndCallBackUPP    mSoundCallback;
  
  Boolean           mSoundDone;
};


// concrete class for playing URL-based sounds asynchronously
class nsMovieSoundRequest :   public nsSoundRequest,
                              public nsIStreamLoaderObserver
{
public:

                    nsMovieSoundRequest();
  virtual           ~nsMovieSoundRequest();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSISTREAMLOADEROBSERVER

  // nsITimerCallback
  NS_IMETHOD_(void) Notify(nsITimer *timer);

  nsresult          Init(nsISound* aSound, nsIURL *aURL);
  virtual nsresult  PlaySound();
  
protected:

  OSType            GetFileFormat(const char* inData, long inDataSize, const nsACString& contentType);
  
  OSErr             ImportMovie(Handle inDataHandle, long inDataSize, const nsACString& contentType);
  PRBool            HaveQuickTime();

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

static void
CopyCToPascalString(const char* inString, StringPtr outPString)
{
  SInt32   nameLen = strlen(inString) & 0xFF;    // max 255 chars
  ::BlockMoveData(inString, &outPString[1], nameLen);
  outPString[0] = nameLen;
}

#pragma mark -

nsSound::nsSound()
{
  NS_INIT_ISUPPORTS();
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
nsSound::PlaySystemSound(const char *aSoundName)
{
  nsCOMPtr<nsISupports> requestSupports;
  
  nsSystemSoundRequest* soundRequest;
  NS_NEWXPCOM(soundRequest, nsSystemSoundRequest);
  if (!soundRequest)
    return NS_ERROR_OUT_OF_MEMORY;

  requestSupports = NS_STATIC_CAST(nsITimerCallback*, soundRequest);
  
  Str255  soundResource;
  nsresult rv = GetSoundResourceName(aSoundName, soundResource);
  if (NS_FAILED(rv))
    return Beep();
  
  rv = soundRequest->Init(this, soundResource);
  if (NS_FAILED(rv))
    return Beep();

  rv = AddRequest(requestSupports);
  if (NS_FAILED(rv))
    return Beep();
  
  rv = soundRequest->PlaySound();
  if (NS_FAILED(rv))
    return Beep();
  
  return NS_OK;
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
    nsMovieSoundRequest* movieRequest = NS_STATIC_CAST(nsMovieSoundRequest*, cachedRequest);
    // if it was cached, start playing right away
    movieRequest->PlaySound();
  }
  else
  {
    nsMovieSoundRequest* soundRequest;
    NS_NEWXPCOM(soundRequest, nsMovieSoundRequest);
    if (!soundRequest)
      return NS_ERROR_OUT_OF_MEMORY;

    requestSupports = NS_STATIC_CAST(nsITimerCallback*, soundRequest);
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


nsresult
nsSound::GetSoundResourceName(const char* inSoundName, StringPtr outResourceName)
{
  nsresult rv = NS_OK;
  
  outResourceName[0] = 0;
  
  // if it's the special mail beep sound, get the real sound name from IC
  if (nsCRT::strcmp("_moz_mailbeep", inSoundName) == 0)
  {
    nsCOMPtr <nsIInternetConfigService> icService = do_GetService(NS_INTERNETCONFIGSERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv))
      return rv;

    nsXPIDLCString  newMailSound;
    rv = icService->GetString(nsIInternetConfigService::eICString_NewMailSoundName, getter_Copies(newMailSound));
    if (NS_FAILED(rv))
      return rv;
      
    CopyCToPascalString(newMailSound.get(), outResourceName);
    return NS_OK;
  }

  // if the name is not "Mailbeep", treat it as the name of a system sound
  CopyCToPascalString(inSoundName, outResourceName);
  return NS_OK;
}


#pragma mark -

nsSoundRequest::nsSoundRequest()
{
  NS_INIT_ISUPPORTS();
}

nsSoundRequest::~nsSoundRequest()
{
}

NS_IMPL_ISUPPORTS1(nsSoundRequest, nsITimerCallback);

nsSoundRequest*
nsSoundRequest::GetFromISupports(nsISupports* inSupports)
{
  if (!inSupports) return nsnull;
  
  // test to see if this is really a nsSoundRequest by trying a QI
  nsCOMPtr<nsITimerCallback>  timerCallback = do_QueryInterface(inSupports);
  if (!timerCallback) return nsnull;
  
  return NS_REINTERPRET_CAST(nsSoundRequest*, inSupports);
}


nsresult
nsSoundRequest::Cleanup()
{
  nsresult rv = NS_OK;
  
#ifdef SOUND_DEBUG
  printf("Sound playback done\n");
#endif
  
  // kill the timer
  if (mTimer) {
    mTimer->Cancel();
    mTimer = nsnull;
  }
  
  // remove from parent array. Use a deathGrip to ensure that it's OK
  // to clear mSound.
  nsCOMPtr<nsISupports>   deathGrip(this);
  if (mSound.get())
  {
    nsSound*    macSound = NS_REINTERPRET_CAST(nsSound*, mSound.get());
    rv = macSound->RemoveRequest(NS_STATIC_CAST(nsITimerCallback*, this));
    mSound = nsnull;
  }
  
  return rv;
}


#pragma mark -


nsSystemSoundRequest::nsSystemSoundRequest()
: mSoundHandle(nsnull)
, mSndChannel(nsnull)
, mSoundCallback(nsnull)
, mSoundDone(false)
{
#ifdef SOUND_DEBUG
  printf("%%%%%%%% Made nsSystemSoundRequest\n");
#endif
}

nsSystemSoundRequest::~nsSystemSoundRequest()
{
  if (mSoundHandle) {
    // unlock the sound resource handle and make it purgeable.
    ::HUnlock(mSoundHandle);
    ::HPurge(mSoundHandle);
  }

  if (mSndChannel)
    ::SndDisposeChannel(mSndChannel, true);

  if (mSoundCallback)
    DisposeSndCallBackUPP(mSoundCallback);
    
#ifdef SOUND_DEBUG
  printf("%%%%%%%% Deleted nsSystemSoundRequest\n");
#endif
}

NS_IMPL_ISUPPORTS_INHERITED0(nsSystemSoundRequest, nsSoundRequest);

nsresult
nsSystemSoundRequest::Init(nsISound* aSound, ConstStr255Param aSoundName)
{
  mSound = aSound;

  mSoundCallback = NewSndCallBackUPP(nsSystemSoundRequest::SoundCallback);
  if (!mSoundCallback) return NS_ERROR_OUT_OF_MEMORY;
  
  mSoundHandle = ::GetNamedResource('snd ', aSoundName);
  if (!mSoundHandle) return NS_ERROR_FAILURE;
  
  // make sure the resource is loaded
  ::LoadResource(mSoundHandle);
  if (!mSoundHandle || !*mSoundHandle) return NS_ERROR_FAILURE;
  
  // and lock it high
  ::HLockHi(mSoundHandle);
  
  OSErr err = ::SndNewChannel(&mSndChannel, 0, 0, mSoundCallback);
  if (err != noErr) return NS_ERROR_FAILURE;

  return NS_OK;
}

nsresult
nsSystemSoundRequest::PlaySound()
{
  NS_ASSERTION(mSndChannel && mSoundHandle, "Should have sound channel here");
  if (!mSndChannel || !mSoundHandle) {
    Cleanup();
    return NS_ERROR_NOT_INITIALIZED;
  }
  
  nsresult rv;
  // set up a timer. This is used to sniff for mSoundDone (which is set by the channel callback).
  mTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);    // release previous timer, if any
  if (NS_FAILED(rv)) {
    Cleanup();
    return rv;
  }

  OSErr err = ::SndPlay(mSndChannel, (SndListHandle)mSoundHandle, true /* async */);
  if (err != noErr) {
     Cleanup();
     return NS_ERROR_FAILURE;
  }

  // now queue up a sound completion command so we get a callback when
  // the sound is done.
  SndCommand    theCmd = { callBackCmd, 0, 0 };
  theCmd.param2 = (long)this;
  
  err = ::SndDoCommand(mSndChannel, &theCmd, false);   // wait for the channel
  if (err != noErr) {
    Cleanup();
    return NS_ERROR_FAILURE;
  }
  
  const PRInt32   kSoundTimerInterval = 250;      // 250 milliseconds
  rv = mTimer->Init(NS_STATIC_CAST(nsITimerCallback*, this), kSoundTimerInterval,
          NS_PRIORITY_NORMAL, NS_TYPE_REPEATING_PRECISE);
  if (NS_FAILED(rv)) {
    Cleanup();
    return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP_(void)
nsSystemSoundRequest::Notify(nsITimer *timer)
{
  if (mSoundDone)
  {
    Cleanup();
  }
}


// Note! Called at interrupt time
void
nsSystemSoundRequest::DonePlaying()
{
  mSoundDone = true;
}

/* static. Note! Called at interrupt time */
pascal void
nsSystemSoundRequest::SoundCallback(SndChannelPtr chan, SndCommand *theCmd)
{
  nsSystemSoundRequest*   soundRequest = NS_REINTERPRET_CAST(nsSystemSoundRequest*, theCmd->param2);
  if (soundRequest)
    soundRequest->DonePlaying();
}


#pragma mark -

NS_IMPL_ISUPPORTS_INHERITED1(nsMovieSoundRequest, nsSoundRequest, nsIStreamLoaderObserver);

////////////////////////////////////////////////////////////////////////
nsMovieSoundRequest::nsMovieSoundRequest()
: mMovie(nsnull)
, mDataHandle(nsnull)
{
#ifdef SOUND_DEBUG
  printf("%%%%%%%% Made nsMovieSoundRequest\n");
#endif
}

nsMovieSoundRequest::~nsMovieSoundRequest()
{
#ifdef SOUND_DEBUG
  printf("%%%%%%%% Deleted nsMovieSoundRequest\n");
#endif
  DisposeMovieData();
}


nsresult
nsMovieSoundRequest::Init(nsISound* aSound, nsIURL *aURL)
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
nsMovieSoundRequest::OnStreamComplete(nsIStreamLoader *aLoader,
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

  NS_ASSERTION(mMovie == nsnull, "nsMovieSoundRequest has a movie already");
  
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
  nsresult rv = macSound->PutSoundInCache(channel, stringLen, NS_STATIC_CAST(nsITimerCallback*, this));
  NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to put sound in cache");
  
  return PlaySound();
}


OSType
nsMovieSoundRequest::GetFileFormat(const char* inData, long inDataSize, const nsACString& contentType)
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
nsMovieSoundRequest::PlaySound()
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
nsMovieSoundRequest::Notify(nsITimer *timer)
{
  if (!mMovie)
  {
    NS_ASSERTION(0, "nsMovieSoundRequest has no movie in timer callback");
    return;
  }
  
#ifdef SOUND_DEBUG
  printf("In movie timer callback\n");
#endif

  PRBool  moviesDone;

  TaskActiveMovies(&moviesDone);
  
  // we're done for now. Remember that this nsMovieSoundRequest might be in the cache,
  // so won't necessarily go away.
  if (moviesDone)
    Cleanup();
}

OSErr
nsMovieSoundRequest::ImportMovie(Handle inDataHandle, long inDataSize, const nsACString& contentType)
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
    
    NS_ASSERTION(mMovie == nsnull, "nsMovieSoundRequest already has movie");
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
    {
      Rect   movieRect = {0};
      ::SetMovieBox(mMovie, &movieRect);
    }
    
    ::GoToEndOfMovie(mMovie);   // simplifies the logic in PlaySound()
    
  bail:
    if (miComponent)
      ::CloseComponent(miComponent);
  }

  if (dataRef)
    ::DisposeHandle(dataRef);
  
  return err;
}

void
nsMovieSoundRequest::DisposeMovieData()
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
nsMovieSoundRequest::TaskOneMovie(Movie inMovie)    // return true if done
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

OSErr
nsMovieSoundRequest::TaskActiveMovies(PRBool *outAllMoviesDone)
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
nsMovieSoundRequest::IsAnyMoviePlaying()
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
nsMovieSoundRequest::HaveQuickTime()
{
  long  gestResult;
  OSErr err = Gestalt (gestaltQuickTime, &gestResult);
  return (err == noErr) && ((long)EnterMovies != kUnresolvedCFragSymbolAddress);
}

