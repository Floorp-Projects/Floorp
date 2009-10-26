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
 *   Rich Walsh <dragtext@e-vertise.com>
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

/*****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define INCL_DOS
#define INCL_WINSHELLDATA
#define INCL_MMIOOS2
#include <os2.h>
#include <mmioos2.h>
#include <mcios2.h>

#include "nsSound.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsNativeCharsetUtils.h"

NS_IMPL_ISUPPORTS2(nsSound, nsISound, nsIStreamLoaderObserver)

/*****************************************************************************/

// argument structure to pass to the background thread
typedef struct _ARGBUFFER
{
  PRUint32  bufLen;
  char      buffer[1];
} ARGBUFFER;

#ifdef DEBUG
  #define DBG_MSG(x)    fprintf(stderr, x "\n")
#else
  #define DBG_MSG(x)
#endif

// the number of defined Mozilla events (see nsISound.idl)
#define EVENT_CNT       7

/*****************************************************************************/

// static variables
static PRBool   sDllError = FALSE;      // set if the MMOS2 dlls fail to load
static char *   sSoundFiles[EVENT_CNT] = {0}; // an array of sound file names

// function pointer definitions (underscore works around redef. warning)
static HMMIO  (*APIENTRY _mmioOpen)(PSZ, PMMIOINFO, ULONG);
static USHORT (*APIENTRY _mmioClose)(HMMIO, USHORT);
static ULONG  (*APIENTRY _mciSendCommand)(USHORT, USHORT, ULONG, PVOID, USHORT);

// helper functions
static void   initSounds(void);
static PRBool initDlls(void);
static void   playSound(void *aArgs);
static FOURCC determineFourCC(PRUint32 aDataLen, const char *aData);

/*****************************************************************************/
/*  nsSound implementation                                                   */
/*****************************************************************************/

// Get the list of sound files associated with mozilla events the first time
// this class is instantiated.  However, defer initialization of the MMOS2
// dlls until they're actually needed (which may be never).

nsSound::nsSound()
{
  initSounds();
}

/*****************************************************************************/

nsSound::~nsSound()
{
}

/*****************************************************************************/

NS_IMETHODIMP nsSound::Init()
{
  return NS_OK;
}

/*****************************************************************************/

NS_IMETHODIMP nsSound::Beep()
{
  WinAlarm(HWND_DESKTOP, WA_WARNING);
  return NS_OK;
}

/*****************************************************************************/

// All attempts to play a sound file should be routed through this method.

NS_IMETHODIMP nsSound::Play(nsIURL *aURL)
{
  if (sDllError) {
    DBG_MSG("nsSound::Play:  MMOS2 initialization failed");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIStreamLoader> loader;
  return NS_NewStreamLoader(getter_AddRefs(loader), aURL, this);
}

/*****************************************************************************/

// After a sound has been loaded, start a new thread to play it.

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
            fprintf(stderr, "nsSound::OnStreamComplete:  failed to load %s\n",
                    uriSpec.get());
          }
        }
      }
    }
#endif
    return NS_ERROR_FAILURE;
  }

  // allocate a buffer to hold the ARGBUFFER struct and sound data;
  // try using high-memory - if that fails, try low-memory
  ARGBUFFER *   arg;
  if (DosAllocMem((PPVOID)&arg, sizeof(ARGBUFFER) + dataLen,
                  OBJ_ANY | PAG_READ | PAG_WRITE | PAG_COMMIT)) {
    if (DosAllocMem((PPVOID)&arg, sizeof(ARGBUFFER) + dataLen,
                    PAG_READ | PAG_WRITE | PAG_COMMIT)) {
      DBG_MSG("nsSound::OnStreamComplete:  DosAllocMem failed");
      return NS_ERROR_FAILURE;
    }
  }

  // copy the sound data
  arg->bufLen = dataLen;
  memcpy(arg->buffer, data, dataLen);

  // Play the sound on a new thread using MMOS2
  if (_beginthread(playSound, NULL, 32768, (void*)arg) < 0) {
    DosFreeMem((void*)arg);
    DBG_MSG("nsSound::OnStreamComplete:  _beginthread failed");
  }

  return NS_OK;
}

/*****************************************************************************/

// This is obsolete and shouldn't get called (in theory).

NS_IMETHODIMP nsSound::PlaySystemSound(const nsAString &aSoundAlias)
{
  if (aSoundAlias.IsEmpty())
    return NS_OK;

  if (NS_IsMozAliasSound(aSoundAlias)) {
    DBG_MSG("nsISound::playSystemSound was called with \"_moz_\" events, "
               "they are obsolete, use nsISound::playEventSound instead");

    PRUint32 eventId;
    if (aSoundAlias.Equals(NS_SYSSOUND_MAIL_BEEP)) {
      eventId = EVENT_NEW_MAIL_RECEIVED;
    } else if (aSoundAlias.Equals(NS_SYSSOUND_ALERT_DIALOG)) {
      eventId = EVENT_ALERT_DIALOG_OPEN;
    } else if (aSoundAlias.Equals(NS_SYSSOUND_CONFIRM_DIALOG)) {
      eventId = EVENT_CONFIRM_DIALOG_OPEN;
    } else if (aSoundAlias.Equals(NS_SYSSOUND_PROMPT_DIALOG)) {
      eventId = EVENT_PROMPT_DIALOG_OPEN;
    } else if (aSoundAlias.Equals(NS_SYSSOUND_SELECT_DIALOG)) {
      eventId = EVENT_SELECT_DIALOG_OPEN;
    } else if (aSoundAlias.Equals(NS_SYSSOUND_MENU_EXECUTE)) {
      eventId = EVENT_MENU_EXECUTE;
    } else if (aSoundAlias.Equals(NS_SYSSOUND_MENU_POPUP)) {
      eventId = EVENT_MENU_POPUP;
    } else {
      return NS_OK;
    }
    return PlayEventSound(eventId);
  }

  // assume aSoundAlias is a file name
  return PlaySoundFile(aSoundAlias);
}

/*****************************************************************************/

// Attempt to play whatever event sounds the user has enabled.
// If the attempt fails or a sound isn't set, fall back to a
// beep for selected events.

NS_IMETHODIMP nsSound::PlayEventSound(PRUint32 aEventId)
{
  if (!sDllError &&
      aEventId < EVENT_CNT &&
      sSoundFiles[aEventId] &&
      NS_SUCCEEDED(PlaySoundFile(
                   nsDependentCString(sSoundFiles[aEventId])))) {
    return NS_OK;
  }

  switch(aEventId) {
    case EVENT_NEW_MAIL_RECEIVED:
      return Beep();                    // play "Warning" sound
    case EVENT_ALERT_DIALOG_OPEN:
      WinAlarm(HWND_DESKTOP, WA_ERROR); // play "Error" sound
      break;
    case EVENT_CONFIRM_DIALOG_OPEN:
      WinAlarm(HWND_DESKTOP, WA_NOTE);  // play "Information" sound
      break;
    case EVENT_PROMPT_DIALOG_OPEN:
    case EVENT_SELECT_DIALOG_OPEN:
    case EVENT_MENU_EXECUTE:
    case EVENT_MENU_POPUP:
      break;
  }

  return NS_OK;
}

/*****************************************************************************/

// Convert a UCS file path to native charset, then play it.

nsresult nsSound::PlaySoundFile(const nsAString &aSoundFile)
{
  nsCAutoString buf;
  nsresult rv = NS_CopyUnicodeToNative(aSoundFile, buf);
  NS_ENSURE_SUCCESS(rv,rv);

  return PlaySoundFile(buf);
}

/*****************************************************************************/

// Take a native charset path, convert it to a file URL, then play the file.

nsresult nsSound::PlaySoundFile(const nsACString &aSoundFile)
{
  nsresult rv;
  nsCOMPtr <nsILocalFile> soundFile;
  rv = NS_NewNativeLocalFile(aSoundFile, PR_FALSE, 
                             getter_AddRefs(soundFile));
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr <nsIURI> fileURI;
  rv = NS_NewFileURI(getter_AddRefs(fileURI), soundFile);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(fileURI,&rv);
  NS_ENSURE_SUCCESS(rv,rv);

  return Play(fileURL);
}

/*****************************************************************************/
/*  static helper functions                                                  */
/*****************************************************************************/

// This function loads the names of sound files associated with Mozilla
// events from mmpm.ini.  Because of the overhead, it only makes one
// attempt per session and caches the results.
//
// Mozilla event sounds can be added to mmpm.ini via a REXX script,
// and can be edited using the 'Sound' object in the System Setup folder.
// mmpm.ini entries have this format:  [fq_path#event_name#volume]
// 'fq_path' identifies the file; 'event_name' is the description that
// appears in the Sound object's listbox;  'volume' is a numeric string
// used by the WPS.  Here's the REXX command to add a "New Mail" sound:
//   result = SysIni('C:\MMOS2\MMPM.INI', 'MMPM2_AlarmSounds', '800',
//                   'C:\MMOS2\SOUNDS\NOTES.WAV#New Mail#80');
// Note that the key value is a numeric string.  The base index for
// Mozilla event keys is an arbitrary number that defaults to 800 if
// an entry for "MOZILLA_Events\BaseIndex" can't be found in mmpm.ini.

static void initSounds(void)
{
  static PRBool sSoundInit = FALSE;

  if (sSoundInit) {
    return;
  }
  sSoundInit = TRUE;

  // Confirm mmpm.ini exists where it's expected to prevent
  // PrfOpenProfile() from creating a new empty file there.
  FILESTATUS3 fs3;
  char   buffer[CCHMAXPATH];
  char * ptr;
  ULONG  rc = DosScanEnv("MMBASE", const_cast<const char **>(&ptr));
  if (!rc) {
    strcpy(buffer, ptr);
    ptr = strchr(buffer, ';');
    if (!ptr) {
      ptr = strchr(buffer, 0);
    }
    strcpy(ptr, "\\MMPM.INI");
    rc = DosQueryPathInfo(buffer, FIL_STANDARD, &fs3, sizeof(fs3));
  }
  if (rc) {
    ULONG ulBootDrive = 0;
    strcpy(buffer, "x:\\MMOS2\\MMPM.INI");
    DosQuerySysInfo( QSV_BOOT_DRIVE, QSV_BOOT_DRIVE,
                     &ulBootDrive, sizeof ulBootDrive);
    buffer[0] = 0x40 + ulBootDrive;
    rc = DosQueryPathInfo(buffer, FIL_STANDARD, &fs3, sizeof(fs3));
  }
  if (rc) {
    DBG_MSG("initSounds:  unable to locate mmpm.ini");
    return;
  }

  HINI hini = PrfOpenProfile(0, buffer);
  if (!hini) {
    DBG_MSG("initSounds:  unable to open mmpm.ini");
    return;
  }

  // If a base index has been set, use it, provided it doesn't
  // collide with the system-defined indices (0 - 12)
  LONG baseNdx = PrfQueryProfileInt(hini, "MOZILLA_Events",
                                    "BaseIndex", 800);
  if (baseNdx <= 12) {
    baseNdx = 800;
  }

  // For each event, see if there's an entry in mmpm.ini.
  // If so, extract the file path, confirm it's valid,
  // then duplicate the string & save it for later use.
  for (LONG i = 0; i < EVENT_CNT; i++) {
    char  key[16];
    ultoa(i + baseNdx, key, 10);
    if (!PrfQueryProfileString(hini, "MMPM2_AlarmSounds", key,
                               0, buffer, sizeof(buffer))) {
      continue;
    }
    ptr = strchr(buffer, '#');
    if (!ptr || ptr == buffer) {
      continue;
    }
    *ptr = 0;
    if (DosQueryPathInfo(buffer, FIL_STANDARD, &fs3, sizeof(fs3)) ||
        (fs3.attrFile & FILE_DIRECTORY) || !fs3.cbFile) {
      continue;
    }
    sSoundFiles[i] = strdup(buffer);
  }

  PrfCloseProfile(hini);
  return;
}

/*****************************************************************************/

// Only one attempt is made per session to initialize MMOS2.  Once
// the dlls are loaded, they remain loaded to avoid stability issues.

static PRBool initDlls(void)
{
  static PRBool sDllInit = FALSE;

  if (sDllInit) {
    return TRUE;
  }
  sDllInit = TRUE;

  HMODULE   hmodMMIO = 0;
  HMODULE   hmodMDM = 0;
  char      szError[32];
  if (DosLoadModule(szError, sizeof(szError), "MMIO", &hmodMMIO) ||
      DosLoadModule(szError, sizeof(szError), "MDM", &hmodMDM)) {
    DBG_MSG("initDlls:  DosLoadModule failed");
    sDllError = TRUE;
    return FALSE;
  }

  if (DosQueryProcAddr(hmodMMIO, 0L, "mmioOpen", (PFN *)&_mmioOpen) ||
      DosQueryProcAddr(hmodMMIO, 0L, "mmioClose", (PFN *)&_mmioClose) ||
      DosQueryProcAddr(hmodMDM,  0L, "mciSendCommand", (PFN *)&_mciSendCommand)) {
    DBG_MSG("initDlls:  DosQueryProcAddr failed");
    sDllError = TRUE;
    return FALSE;
  }

  return TRUE;
}

/*****************************************************************************/

// Background thread proc to play the sound that was set up in
// the argument structure.  If an error occurs, beep at least.
// aArgs is allocated from system memory & must be freed.

static void playSound(void * aArgs)
{
  BOOL        fOK = FALSE;
  HMMIO       hmmio = 0;
  MMIOINFO    mi;

  do {
    if (!initDlls())
      break;

    memset(&mi, 0, sizeof(MMIOINFO));
    mi.cchBuffer = ((ARGBUFFER*)aArgs)->bufLen;
    mi.pchBuffer = ((ARGBUFFER*)aArgs)->buffer;
    mi.ulTranslate = MMIO_TRANSLATEDATA | MMIO_TRANSLATEHEADER;
    mi.fccChildIOProc = FOURCC_MEM;
    mi.fccIOProc = determineFourCC(mi.cchBuffer, mi.pchBuffer);
    if (!mi.fccIOProc) {
      DBG_MSG("playSound:  unknown sound format");
      break;
    }

    hmmio = _mmioOpen(NULL, &mi, MMIO_READ | MMIO_DENYWRITE);
    if (!hmmio) {
      DBG_MSG("playSound:  _mmioOpen failed");
      break;
    }

    // open the sound device
    MCI_OPEN_PARMS mop;
    memset(&mop, 0, sizeof(mop));
    mop.pszElementName = (PSZ)hmmio;
    mop.pszDeviceType = (PSZ)MAKEULONG(MCI_DEVTYPE_WAVEFORM_AUDIO, 0);
    if (_mciSendCommand(0, MCI_OPEN,
                        MCI_OPEN_MMIO | MCI_OPEN_TYPE_ID |
                        MCI_OPEN_SHAREABLE | MCI_WAIT,
                        (PVOID)&mop, 0)) {
      DBG_MSG("playSound:  MCI_OPEN failed");
      break;
    }
    fOK = TRUE;

    // play the sound
    MCI_PLAY_PARMS mpp;
    memset(&mpp, 0, sizeof(mpp));
    if (_mciSendCommand(mop.usDeviceID, MCI_PLAY, MCI_WAIT, &mpp, 0)) {
      DBG_MSG("playSound:  MCI_PLAY failed");
    }

    // stop & close the device
    _mciSendCommand(mop.usDeviceID, MCI_STOP,  MCI_WAIT, &mpp, 0);
    if (_mciSendCommand(mop.usDeviceID, MCI_CLOSE, MCI_WAIT, &mpp, 0)) {
      DBG_MSG("playSound:  MCI_CLOSE failed");
    }

  } while (0);

  if (!fOK)
    WinAlarm(HWND_DESKTOP, WA_WARNING);

  if (hmmio)
    _mmioClose(hmmio, 0);
  DosFreeMem(aArgs);

  _endthread();
}

/*****************************************************************************/

// Try to determine the data format in the buffer using file "magic".
// Returns the FourCC handle for the format, or 0 when failing to
// find format and codec.

static FOURCC determineFourCC(PRUint32 aDataLen, const char *aData)
{
  FOURCC fcc = 0;

  // Compare the first bytes of the data with magic to determine
  // the most likely format (other possible formats are ignored
  // because the mmio procs for them are too unreliable)
  if (memcmp(aData, "RIFF", 4) == 0)        // WAV
    fcc = mmioFOURCC('W', 'A', 'V', 'E');
  else
  if (memcmp(aData, "ID3", 3) == 0)         // likely MP3 with ID3 header
    fcc = mmioFOURCC('M','P','3',' ');
  else
  if ((aData[0] & 0xFF) == 0xFF &&          // various versions of MPEG layer 3
      ((aData[1] & 0xFE) == 0xFA ||         // v1
       (aData[1] & 0xFE) == 0xF2 ||         // v2
       (aData[1] & 0xFE) == 0xE2))          // v2.5
    fcc = mmioFOURCC('M','P','3',' ');
  else
  if (memcmp(aData, "OggS", 4) == 0)        // OGG
    fcc = mmioFOURCC('O','G','G','S');
  else
  if (memcmp(aData, "fLaC", 4) == 0)        // FLAC
    fcc = mmioFOURCC('f','L','a','C');

  return fcc;
}

/*****************************************************************************/

