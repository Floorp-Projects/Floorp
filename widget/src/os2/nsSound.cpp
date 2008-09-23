/* vim: set sw=2 sts=2 et cin: */
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 *   John Fairhurst <john_fairhurst@iname.com>
 *   IBM Corp.
 *   Peter Weilbacher <mozilla@Weilbacher.org>
 *   Lars Erdmann
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nscore.h"
#include "plstr.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_MMIOOS2
#include <os2.h>
#include <mmioos2.h>
#include <mcios2.h>
#define MCI_ERROR_LENGTH 128

#include "nsSound.h"
#include "nsIURL.h"
#include "nsNetUtil.h"

#include "nsDirectoryServiceDefs.h"

#include "nsNativeCharsetUtils.h"

NS_IMPL_ISUPPORTS2(nsSound, nsISound, nsIStreamLoaderObserver)

static int sInitialized = 0;
static PRBool sMMPMInstalled = PR_FALSE;
static HMODULE sHModMMIO = NULLHANDLE;
static HMODULE sHModMDM = NULLHANDLE;

// function pointer definitions, include underscore (work around redef. warning)
HMMIO (*APIENTRY _mmioOpen)(PSZ, PMMIOINFO, ULONG);
USHORT (*APIENTRY _mmioClose)(HMMIO, USHORT);
ULONG (*APIENTRY _mmioGetFormats)(PMMFORMATINFO, LONG, PVOID, PLONG, ULONG, ULONG);
ULONG (*APIENTRY _mciSendCommand)(USHORT, USHORT, ULONG, PVOID, USHORT);
#ifdef DEBUG
ULONG (*APIENTRY _mmioGetLastError)(HMMIO);
ULONG (*APIENTRY _mmioQueryFormatCount)(PMMFORMATINFO, PLONG, ULONG, ULONG);
ULONG (*APIENTRY _mmioGetFormatName)(PMMFORMATINFO, PSZ, PLONG, ULONG, ULONG);
ULONG (*APIENTRY _mciGetErrorString)(ULONG, PSZ, USHORT);
#endif
ULONG (*APIENTRY _mmioIniFileHandler)(PMMINIFILEINFO, ULONG);

// argument structure to pass to the background thread
typedef struct _ARGBUFFER
{
  HEV hev;
  PRUint32 bufLen;
  const char *buffer;
  PSZ pszFilename;
} ARGBUFFER;

////////////////////////////////////////////////////////////////////////

static void InitGlobals(void)
{
  ULONG ulrc = 0;
  char LoadError[CCHMAXPATH];

  ulrc = DosLoadModule(LoadError, CCHMAXPATH, "MMIO", &sHModMMIO);
  ulrc += DosLoadModule(LoadError, CCHMAXPATH, "MDM", &sHModMDM);
  if (ulrc == NO_ERROR) {
#ifdef DEBUG
    printf("InitGlobals: MMOS2 is installed, both DLLs loaded\n");
#endif
    sMMPMInstalled = PR_TRUE;
    // MMOS2 is installed, so we can query the necessary functions
    // mmio functions are in MMIO.DLL
    ulrc = DosQueryProcAddr(sHModMMIO, 0L, "mmioOpen", (PFN *)&_mmioOpen);
    ulrc += DosQueryProcAddr(sHModMMIO, 0L, "mmioClose", (PFN *)&_mmioClose);
    ulrc += DosQueryProcAddr(sHModMMIO, 0L, "mmioGetFormats", (PFN *)&_mmioGetFormats);
    // mci functions are in MDM.DLL
    ulrc += DosQueryProcAddr(sHModMDM, 0L, "mciSendCommand", (PFN *)&_mciSendCommand);
#ifdef DEBUG
    ulrc += DosQueryProcAddr(sHModMMIO, 0L, "mmioGetLastError", (PFN *)&_mmioGetLastError);
    ulrc += DosQueryProcAddr(sHModMMIO, 0L, "mmioQueryFormatCount", (PFN *)&_mmioQueryFormatCount);
    ulrc += DosQueryProcAddr(sHModMMIO, 0L, "mmioGetFormatName", (PFN *)&_mmioGetFormatName);
    ulrc += DosQueryProcAddr(sHModMDM, 0L, "mciGetErrorString", (PFN *)&_mciGetErrorString);
#endif

    ulrc += DosQueryProcAddr(sHModMMIO, 0L, "mmioIniFileHandler", (PFN *)&_mmioIniFileHandler);

    // if one of these failed, we have some kind of non-functional MMOS2 installation
    if (ulrc != NO_ERROR) {
      NS_WARNING("MMOS2 is installed, but seems to have corrupt DLLs");
      sMMPMInstalled = PR_FALSE;
    }
  }
}

////////////////////////////////////////////////////////////////////////

// Tries to determine the data format in the buffer using file "magic"
// and a loop through MMOS2 audio codecs.
// Returns the FourCC handle for the format, or 0 when failing to find format
// and codec.
FOURCC determineFourCC(PRUint32 aDataLen, const char *aData)
{
  FOURCC fcc = 0;

  // Start to compare the first bytes of the data with magic to determine the
  // most likely format upfront.
  if (memcmp(aData, "RIFF", 4) == 0) {                                    // WAV
    fcc = mmioFOURCC('W', 'A', 'V', 'E');
  } else if (memcmp(aData, "ID3", 3) == 0 ||       // likely MP3 with ID3 header
             ((aData[0] & 0xFF) == 0xFF &&   // various versions of MPEG layer 3
              ((aData[1] & 0xFE) == 0xFA ||                                // v1
               (aData[1] & 0xFE) == 0xF2 ||                                // v2
               (aData[1] & 0xFE) == 0xE2)))                              // v2.5
  {
    fcc = mmioFOURCC('M','P','3',' ');
  } else if (memcmp(aData, "OggS", 4) == 0) {                             // OGG
    fcc = mmioFOURCC('O','G','G','S');
  } else if (memcmp(aData, "fLaC", 4) == 0) {                            // FLAC
    fcc = mmioFOURCC('f','L','a','C');
  }

  // The following is too flakey because several OS/2 IOProc don't behave as
  // they should and would cause us to crash. So just skip this for now...
#if 0
  if (fcc) // already found one
    return fcc;

  // None of the popular formats found, so use the list of MMOS2 audio codecs to
  // find one that can open the file.
  MMFORMATINFO mmfi;
  LONG lNum;
  memset(&mmfi, '\0', sizeof(mmfi));
  mmfi.ulStructLen = sizeof(mmfi);
  mmfi.ulMediaType |= MMIO_MEDIATYPE_AUDIO;
  ULONG ulrc = _mmioQueryFormatCount(&mmfi, &lNum, 0L, 0L);

  PMMFORMATINFO mmflist = (PMMFORMATINFO)calloc(lNum, sizeof(MMFORMATINFO));
  LONG lFormats;
  ulrc = _mmioGetFormats(&mmfi, lNum, mmflist, &lFormats, 0L, 0L);

  MMIOINFO mi;
  memset(&mi, '\0', sizeof(mi));
  mi.fccChildIOProc = FOURCC_MEM;
  unsigned char szBuffer[sizeof(FOURCC) + CCHMAXPATH + 4];
  for (int i = lFormats-1; i >= 0; i--) {
    // Loop through formats. Do it backwards to find at least WAV before the
    // faulty VORBIS/FLAC/MP3 IOProcs that will open any format.
    MMFORMATINFO mmfi = mmflist[i];
#ifdef DEBUG
    LONG lBytesRead;
    _mmioGetFormatName(&mmfi, (char *)szBuffer, &lBytesRead, 0L, 0L);
    printf("determineFour Codec %d: name=%s media=0x%lx ext=%s fcc=%c%c%c%c/%ld/%p\n",
           i, szBuffer, mmfi.ulMediaType, mmfi.szDefaultFormatExt,
           (char)(mmfi.fccIOProc), (char)(mmfi.fccIOProc >> 8),
           (char)(mmfi.fccIOProc >> 16), (char)(mmfi.fccIOProc >> 24),
           mmfi.fccIOProc, (void *)mmfi.fccIOProc);
#endif

    // this codec likely crashes the program when the buffer is not of the
    // expected format
    if (mmfi.fccIOProc == mmioFOURCC('A','V','C','A')) {
      continue;
    }

    mi.fccIOProc = mmfi.fccIOProc;
    HMMIO hmmio= _mmioOpen(NULL, &mi, MMIO_READ);
    if (hmmio) {
      fcc = mmfi.fccIOProc;
      _mmioClose(hmmio, 0);
      break;
    }
  }
  free(mmflist);
#endif

#ifdef DEBUG
  printf("determineFourCC: Codec fcc is 0x%lx or --%c%c%c%c--\n", fcc,
         (char)(fcc), (char)(fcc >> 8), (char)(fcc >> 16), (char)(fcc >> 24));
#endif

  return fcc;
}

// Play the sound that was set up in the argument structure. If an error occurs,
// beep at least.  To be used as function for a new background thread.
static void playSound(void *aArgs)
{
  ULONG ulrc = NO_ERROR;
  ARGBUFFER args;
  memcpy(&args, aArgs, sizeof(args));

  MMIOINFO mi;
  memset(&mi, '\0', sizeof(mi));
  HMMIO hmmio = NULLHANDLE;

  do { // inner block (break in case of error)
    if (args.pszFilename) {
      // determine size of file that we want to read
      FILESTATUS3 fs3;
      memset(&fs3, '\0', sizeof(fs3));
      ulrc = DosQueryPathInfo(args.pszFilename, FIL_STANDARD, &fs3, sizeof(fs3));
      mi.cchBuffer = fs3.cbFile;
    } else {
      // use size of the existing buffer
      mi.cchBuffer = args.bufLen;
    }

    // Read or copy the sound into a local memory buffer for easy playback.
    // (If we got the sound in a buffer originally, that buffer could
    // "disappear" while we are still playing it.)
    ulrc = DosAllocMem((PPVOID)&mi.pchBuffer, mi.cchBuffer,
#ifdef OS2_HIGH_MEMORY             /* only if compiled with high-memory  */
                       OBJ_ANY | /* support, we can allocate anywhere! */
#endif
                       PAG_READ | PAG_WRITE | PAG_COMMIT);
    if (ulrc != NO_ERROR) {
#ifdef DEBUG
      printf("playSound: Could not allocate the sound buffer, ulrc=%ld\n", ulrc);
#endif
      break;
    }

    if (args.pszFilename) {
      // read the sound from file into memory
      HFILE hf = NULLHANDLE;
      ULONG ulAction = 0;
      ulrc = DosOpen(args.pszFilename, &hf, &ulAction, 0, FILE_NORMAL,
                     OPEN_ACTION_OPEN_IF_EXISTS | OPEN_ACTION_FAIL_IF_NEW,
                     OPEN_ACCESS_READONLY | OPEN_SHARE_DENYNONE,
                     NULL);
      if (ulrc != NO_ERROR) {
#ifdef DEBUG
        printf("playSound: could not open the sound file \"%s\" (%ld)\n",
               args.pszFilename, ulrc);
#endif
        break;
      }
      ULONG ulRead = 0;
      ulrc = DosRead(hf, mi.pchBuffer, mi.cchBuffer, &ulRead);
      DosClose(hf);
      if (ulrc != NO_ERROR) {
#ifdef DEBUG
        printf("playSound: read %ld of %ld bytes from the sound file \"%s\" (%ld)\n",
               ulRead, mi.cchBuffer, args.pszFilename, ulrc);
#endif
        break;
      }
    } else {
      // copy the passed sound buffer into local memory
      memcpy(mi.pchBuffer, args.buffer, args.bufLen);
    }

    DosPostEventSem(args.hev); // calling thread can continue

    // Now the sound is loaded into memory in any case, play it from there.
    mi.fccChildIOProc = FOURCC_MEM;
    mi.fccIOProc = determineFourCC(mi.cchBuffer, mi.pchBuffer);
    if (!mi.fccIOProc) {
      NS_WARNING("playSound: unknown sound format in memory buffer");
      break;
    }
    mi.ulTranslate = MMIO_TRANSLATEDATA | MMIO_TRANSLATEHEADER;
    hmmio = _mmioOpen(NULL, &mi, MMIO_READ | MMIO_DENYWRITE);

    if (!hmmio) {
#ifdef DEBUG
      ULONG ulrc = _mmioGetLastError(hmmio);
      if (args.pszFilename) {
        printf("playSound: mmioOpen failed, cannot play sound from \"%s\" (%ld)\n",
               args.pszFilename, ulrc);
      } else {
        printf("playSound: mmioOpen failed, cannot play sound buffer (%ld)\n",
               ulrc);
      }
#endif
      break;
    }

    // open the sound device
    MCI_OPEN_PARMS mop;
    memset(&mop, '\0', sizeof(mop));
    mop.pszElementName = (PSZ)hmmio;
    mop.pszDeviceType = (PSZ)MAKEULONG(MCI_DEVTYPE_WAVEFORM_AUDIO, 0);
    ulrc = _mciSendCommand(0, MCI_OPEN,
                           MCI_OPEN_MMIO | MCI_OPEN_TYPE_ID | MCI_OPEN_SHAREABLE | MCI_WAIT,
                           (PVOID)&mop, 0);
    if (ulrc != MCIERR_SUCCESS) {
#ifdef DEBUG
      CHAR errorBuffer[MCI_ERROR_LENGTH];
      _mciGetErrorString(ulrc, errorBuffer, MCI_ERROR_LENGTH);
      printf("playSound: mciSendCommand with MCI_OPEN_MMIO returned %ld: %s\n",
             ulrc, errorBuffer);
#endif
      break;
    }

    // play the sound
    MCI_PLAY_PARMS mpp;
    memset(&mpp, '\0', sizeof(mpp));
    ulrc = _mciSendCommand(mop.usDeviceID, MCI_PLAY, MCI_WAIT, &mpp, 0);
#ifdef DEBUG
    // just ignore further failures in non-debug mode
    if (ulrc != MCIERR_SUCCESS) {
      CHAR errorBuffer[MCI_ERROR_LENGTH];
      _mciGetErrorString(ulrc, errorBuffer, MCI_ERROR_LENGTH);
      printf("playSound: mciSendCommand with MCI_PLAY returned %ld: %s\n",
             ulrc, errorBuffer);
    }
#endif

    // end playing
    ulrc = _mciSendCommand(mop.usDeviceID, MCI_STOP, MCI_WAIT, &mpp, 0); // be nice
    ulrc = _mciSendCommand(mop.usDeviceID, MCI_CLOSE, MCI_WAIT, &mpp, 0);
#ifdef DEBUG
    if (ulrc != MCIERR_SUCCESS) {
      CHAR errorBuffer[MCI_ERROR_LENGTH];
      _mciGetErrorString(ulrc, errorBuffer, MCI_ERROR_LENGTH);
      printf("playSound: mciSendCommand with MCI_CLOSE returned %ld: %s\n",
             ulrc, errorBuffer);
    }
#endif
    _mmioClose(hmmio, 0);
    DosFreeMem(mi.pchBuffer);
    _endthread();
  } while(0); // end of inner block

  // cleanup after an error
  WinAlarm(HWND_DESKTOP, WA_WARNING); // Beep()
  if (hmmio)
    _mmioClose(hmmio, 0);
  if (mi.pchBuffer)
    DosFreeMem(mi.pchBuffer);
}

////////////////////////////////////////////////////////////////////////

nsSound::nsSound()
{
  if (!sInitialized) {
    InitGlobals();
  }
  sInitialized++;
#ifdef DEBUG
  printf("nsSound::nsSound: sInitialized=%d\n", sInitialized);
#endif
}

nsSound::~nsSound()
{
  sInitialized--;
#ifdef DEBUG
  printf("nsSound::~nsSound: sInitialized=%d\n", sInitialized);
#endif
  // (try to) unload modules after last user ended
  if (!sInitialized) {
#ifdef DEBUG
    printf("nsSound::~nsSound: Trying to free modules...\n");
#endif
    ULONG ulrc;
    ulrc = DosFreeModule(sHModMMIO);
    ulrc += DosFreeModule(sHModMDM);
    if (ulrc != NO_ERROR) {
      NS_WARNING("DosFreeModule did not work");
    }
  }
}

NS_IMETHODIMP nsSound::OnStreamComplete(nsIStreamLoader *aLoader,
                                        nsISupports *context,
                                        nsresult aStatus,
                                        PRUint32 dataLen,
                                        const PRUint8 *data)
{
  if (NS_FAILED(aStatus)) {
#ifdef DEBUG
    if (aLoader) {
      nsCOMPtr<nsIRequest> request;
      aLoader->GetRequest(getter_AddRefs(request));
      if (request) {
        nsCOMPtr<nsIURI> uri;
        nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
        if (channel) {
            channel->GetURI(getter_AddRefs(uri));
            if (uri) {
                nsCAutoString uriSpec;
                uri->GetSpec(uriSpec);
                printf("Failed to load %s\n", uriSpec.get());
            }
        }
      }
    }
#endif
    return NS_ERROR_FAILURE;
  }

  if (!sMMPMInstalled) {
    NS_WARNING("Sound output only works with MMOS2 installed");
    Beep();
    return NS_OK;
  }

  ARGBUFFER arg;
  memset(&arg, '\0', sizeof(arg));
  APIRET rc = DosCreateEventSem(NULL, &(arg.hev), 0UL, 0UL);

  // Play the sound on a new thread using MMOS2, in this case pass
  // the memory buffer in the argument structure.
  arg.bufLen = dataLen;
  arg.buffer = (char *)data;
  _beginthread(playSound, NULL, 32768, (void *)&arg);

  // Wait until the buffer was copied, but not indefinitely to not block the
  // UI in case a really large sound file is copied.
  rc = DosWaitEventSem(arg.hev, 100);
  rc = DosCloseEventSem(arg.hev);

  return NS_OK;
}

NS_METHOD nsSound::Beep()
{
  WinAlarm(HWND_DESKTOP, WA_WARNING);

  return NS_OK;
}

NS_METHOD nsSound::Play(nsIURL *aURL)
{
  nsresult rv;

  nsCOMPtr<nsIStreamLoader> loader;
  rv = NS_NewStreamLoader(getter_AddRefs(loader), aURL, this);

  return rv;
}

NS_IMETHODIMP nsSound::Init()
{
  return NS_OK;
}

NS_IMETHODIMP nsSound::PlaySystemSound(const nsAString &aSoundAlias)
{
  // We don't have a default mail sound on OS/2, so just beep.
  // Also just beep if MMPM isn't installed.
  if (aSoundAlias.EqualsLiteral("_moz_mailbeep") || (!sMMPMInstalled)) {
    Beep();
    return NS_OK;
  }
  nsCAutoString nativeSoundAlias;
  NS_CopyUnicodeToNative(aSoundAlias, nativeSoundAlias);

  ARGBUFFER arg;
  memset(&arg, '\0', sizeof(arg));
  APIRET rc = DosCreateEventSem(NULL, &(arg.hev), 0UL, 0UL);

  // Play the sound on a new thread using MMOS2, in this case pass
  // the filename in the argument structure.
  arg.pszFilename = (PSZ)nativeSoundAlias.get();
  _beginthread(playSound, NULL, 32768, (void *)&arg);

  // Try to wait a while until the file is loaded, but not too long...
  rc = DosWaitEventSem(arg.hev, 100);
  rc = DosCloseEventSem(arg.hev);

  return NS_OK;
}
