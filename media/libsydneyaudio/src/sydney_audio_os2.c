/* ***** BEGIN LICENSE BLOCK *****
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
 * The Initial Developers of the Original Code are
 *   Andrew Zabolotny (libdart) - Copyright (C) 1998
 *   CSIRO (libsydneyaudio)- Copyright (C) 2007
 *   Richard Walsh (OS/2 implementation) - Copyright (C) 2008
 * Portions created by the Initial Developers are Copyright (c) 1998,2007,2008,
 * the Initial Developers. All Rights Reserved.
 *
 * Contributor(s):
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
 * ***** END LICENSE BLOCK ***** *
 */

/*****************************************************************************/
/*  OVERVIEW
 *  
 *  Unlike other DART implementations which pull data into the backend
 *  as needed, this one relies on the upstream code to provide sufficient
 *  data in a well regulated stream.  If other activities in the system
 *  interrupt that stream, the sound device may run out of data.  While
 *  it should simply pause until more data is available, on some machines
 *  a buffer underrun causes the device to stop responding and to ignore
 *  new data until an MCI_STOP or MCI_PAUSE command is issued.
 *  
 *  The solution used here is to track the number of buffers in use and
 *  to pause the device when the count falls below a threshold.  Writing
 *  a new buffer to the device causes playback to resume automatically.
 *  To support this scheme, the code uses atomic operations on 2 counters
 *  to pass buffer counts between its two threads (the app's decode thread
 *  and DART's event thread).  It also has the event thread do as little
 *  as possible to ensure it's not busy when a buffer-free event occurs.
 *
 */
/*****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "sydney_audio.h"

#define INCL_DOS
#define INCL_MCIOS2
#include <os2.h>
#include <os2me.h>
#include <386/builtin.h>

/*****************************************************************************/

/* this will have to be changed to a variable
 * if other than 16-bit samples are ever supported */
#define SAOS2_SAMPLE_SIZE   2

/* the number of buffers to allocate;  each buffer requires
 * 64kb of linear address space in the low-mem private arena;
 * actual physical memory used depends on each buffer's size */
#define SAOS2_BUF_CNT       40

/* the minimum number of milliseconds worth of data required before
 * a buffer is written to the device - the actual number of ms per
 * write will usually be greater;  the size of each buffer is based
 * on this figure and the stream's rate & number of channels */
#define SAOS2_MS_PER_WRITE  40

/* if the number of buffers in use is less than this value,
 * os2_mixer_event() will pause the device to prevent an underrun */
#define SAOS2_UNDERRUN_CNT  2

/* wait 5 seconds for a buffer to become free -
 * an indefinite wait invites a hung thread */
#define SAOS2_WAIT          5000

/*****************************************************************************/
/*  Debug  */

#ifdef DEBUG
  #ifndef SAOS2_ERROR
    #define SAOS2_ERROR
  #endif
#endif

#ifdef SAOS2_ERROR
  static int  os2_error_msg(int rtn, char * func, char * msg, uint32_t err);
  #define os2_error(rtn, func, msg, err)    os2_error_msg(rtn, func, msg, err)
#else
  #define os2_error(rtn, func, msg, err)    rtn
#endif

/*****************************************************************************/
/*  OS/2 implementation of sa_stream_t  */

struct sa_stream {

  /* audio format info */
  const char *      client_name;
  sa_mode_t         mode;
  sa_pcm_format_t   format;
  uint32_t          rate;
  uint32_t          nchannels;
  uint32_t          bps;

  /* device info */
  uint16_t          hwDeviceID;
  uint32_t          hwMixHandle;
  PMIXERPROC        hwWriteProc;

  /* buffer allocations */
  int32_t           bufCnt;
  size_t            bufSize;
  size_t            bufMin;
  PMCI_MIX_BUFFER   bufList;

  /* buffer usage tracking */
  volatile uint32_t freeNew;
  int32_t           freeCnt;
  int32_t           freeNdx;
  volatile uint32_t usedNew;
  int32_t           usedCnt;
  int32_t           usedMin;

  /* miscellaneous */
  volatile uint32_t playing;
  volatile uint32_t writeTime;
  volatile uint32_t writeNew;
  int64_t           writePos;
};

/*****************************************************************************/
/*  Private (static) Functions  */

static int32_t  os2_mixer_event(uint32_t ulStatus, PMCI_MIX_BUFFER pBuffer,
                                uint32_t ulFlags);
static void     os2_stop_device(uint16_t hwDeviceID);
static int      os2_pause_device(uint16_t hwDeviceID, uint32_t release);
static int      os2_get_free_count(sa_stream_t *s, int32_t count);

/*****************************************************************************/
/*  Mozilla-specific Additions  */

/* load mdm.dll on demand */
static int      os2_load_mdm(void);

/* invoke mciSendCommand() via a static variable */
typedef ULONG _System     MCISENDCOMMAND(USHORT, USHORT, ULONG, PVOID, USHORT);
static MCISENDCOMMAND *   _mciSendCommand = 0;

/*****************************************************************************/
/*  Sydney Audio Functions                                                   */
/*****************************************************************************/

/** Normal way to open a PCM device */

int     sa_stream_create_pcm(sa_stream_t **  s, 
                             const char *    client_name, 
                             sa_mode_t       mode, 
                             sa_pcm_format_t format, 
                             unsigned int    rate, 
                             unsigned int    nchannels)
{
  uint32_t      status = SA_SUCCESS;
  uint32_t      size;
  uint32_t      rc;
  sa_stream_t * sTemp = 0;

  /* this do{}while(0) "loop" makes it easy to ensure
   * resources are freed on exit if there's an error */
do {
  /* load mdm.dll if it isn't already loaded */
  if (os2_load_mdm() != SA_SUCCESS)
    return SA_ERROR_SYSTEM;

  if (mode != SA_MODE_WRONLY || format != SA_PCM_FORMAT_S16_LE)
    return os2_error(SA_ERROR_NOT_SUPPORTED, "sa_stream_create_pcm",
                     "invalid mode or format", 0);

  if (!s)
    return os2_error(SA_ERROR_INVALID, "sa_stream_create_pcm",
                     "s is null", 0);
  *s = 0;

  /* the MCI_MIX_BUFFERs must be in low memory or terrible things will
   * happen! - since there's extra space, put 'sa_stream' there too */
  size = sizeof(sa_stream_t) + sizeof(PMCI_MIX_BUFFER) * SAOS2_BUF_CNT;
  rc = DosAllocMem((void**)&sTemp, size,
                   PAG_COMMIT | PAG_READ | PAG_WRITE);
  if (rc) {
    status = os2_error(SA_ERROR_OOM, "sa_stream_create_pcm",
                       "DosAllocMem - rc=", rc);
    break;
  }

  memset(sTemp, 0, size);
  sTemp->bufList = (PMCI_MIX_BUFFER)&sTemp[1];

  /* fill in the miscellanea */
  sTemp->client_name = client_name;
  sTemp->mode        = mode;
  sTemp->format      = format;
  sTemp->rate        = rate;
  sTemp->nchannels   = nchannels;
  sTemp->bps         = rate * nchannels * SAOS2_SAMPLE_SIZE;

  /* each buffer requires 64k of linear address space;
   * the actual physical memory used is much less */
  sTemp->bufCnt  = SAOS2_BUF_CNT;

  /* a buffer must contain at least 'bufmin' bytes before it's written
   * to the device - this equates to SAOS2_MS_PER_WRITE worth of data */
  sTemp->bufMin  = (sTemp->bps * SAOS2_MS_PER_WRITE) / 1000;

  /* 'bufSize' is 150% of 'bufmin' rounded up to the nearest page
   * boundary, then rounded down to a multiple of the frame size;
   * this ensures that all data delivered to sa_stream_write() will
   * fit in a single buffer & that all committed memory can be used */
  sTemp->bufSize = (((3 * sTemp->bufMin) / 2) + 0xfff) & ~0xfff;
  sTemp->bufSize -= sTemp->bufSize % (SAOS2_SAMPLE_SIZE * nchannels);

  *s = sTemp;

} while (0);

  /* on error, free any allocations */
  if (status != SA_SUCCESS && sTemp) {
    if (sTemp)
      DosFreeMem(sTemp);
  }

  return status;
}

/*****************************************************************************/

/** Initialise the device */

int     sa_stream_open(sa_stream_t *s)
{
  int                 status = SA_SUCCESS;
  uint32_t            rc;
  int32_t             ctr;
  uint32_t            bufCntRequested;
  MCI_AMP_OPEN_PARMS  AmpOpenParms;
  MCI_MIXSETUP_PARMS  MixSetupParms;
  MCI_BUFFER_PARMS    BufferParms;

  if (!s)
    return os2_error(SA_ERROR_NO_INIT, "sa_stream_open", "s is null", 0);

do {
  /* s->bufCnt will be restored after successfully allocating buffers */
  bufCntRequested = s->bufCnt;
  s->bufCnt = 0;

  /* open the Amp-Mixer using the default device in shared mode */
  memset(&AmpOpenParms, 0, sizeof(MCI_AMP_OPEN_PARMS));
  AmpOpenParms.pszDeviceType = (PSZ)(MCI_DEVTYPE_AUDIO_AMPMIX | 0);

  rc = _mciSendCommand(0, MCI_OPEN,
                      MCI_WAIT | MCI_OPEN_TYPE_ID | MCI_OPEN_SHAREABLE,
                      (void*)&AmpOpenParms, 0);
  if (LOUSHORT(rc)) {
    status = os2_error(SA_ERROR_NO_DEVICE, "sa_stream_open",
                       "MCI_OPEN - rc=", LOUSHORT(rc));
    break;
  }

  /* save the device ID */
  s->hwDeviceID = AmpOpenParms.usDeviceID;

  /* setup the Amp-Mixer to play wave data */
  memset(&MixSetupParms, 0, sizeof(MCI_MIXSETUP_PARMS));
  MixSetupParms.ulBitsPerSample = 16;
  MixSetupParms.ulFormatTag     = MCI_WAVE_FORMAT_PCM;
  MixSetupParms.ulFormatMode    = MCI_PLAY;
  MixSetupParms.ulSamplesPerSec = s->rate;
  MixSetupParms.ulChannels      = s->nchannels;
  MixSetupParms.ulDeviceType    = MCI_DEVTYPE_WAVEFORM_AUDIO;
  MixSetupParms.pmixEvent       = (MIXEREVENT*)os2_mixer_event;

  rc = _mciSendCommand(s->hwDeviceID, MCI_MIXSETUP,
                      MCI_WAIT | MCI_MIXSETUP_INIT,
                      (void*)&MixSetupParms, 0);
  if (LOUSHORT(rc)) {
    status = os2_error(SA_ERROR_NOT_SUPPORTED, "sa_stream_open",
                       "MCI_MIXSETUP - rc=", LOUSHORT(rc));
    break;
  }

  /* save hw info we'll need later */
  s->hwMixHandle = MixSetupParms.ulMixHandle;
  s->hwWriteProc = MixSetupParms.pmixWrite;

  /* allocate device buffers from the Amp-Mixer */
  BufferParms.ulStructLength = sizeof(MCI_BUFFER_PARMS);
  BufferParms.ulNumBuffers   = bufCntRequested;
  BufferParms.ulBufferSize   = s->bufSize;
  BufferParms.pBufList       = s->bufList;

  rc = _mciSendCommand(s->hwDeviceID, MCI_BUFFER,
                      MCI_WAIT | MCI_ALLOCATE_MEMORY,
                      (void*)&BufferParms, 0);
  if (LOUSHORT(rc)) {
    status = os2_error(SA_ERROR_OOM, "sa_stream_open",
                       "MCI_ALLOCATE_MEMORY - rc=", LOUSHORT(rc));
    break;
  }

  /* MCI_ALLOCATE_MEMORY may have decreased the,
   * number of buffers, so update the counts */
  s->bufCnt  = BufferParms.ulNumBuffers;
  s->freeCnt = BufferParms.ulNumBuffers;

  /* sa_stream_write() & os2_mixer_event() require these initializations */
  s->usedMin = SAOS2_UNDERRUN_CNT;
  for (ctr = 0; ctr < s->bufCnt; ctr++) {
    s->bufList[ctr].ulStructLength = sizeof(MCI_MIX_BUFFER);
    s->bufList[ctr].ulBufferLength = 0;
    s->bufList[ctr].ulUserParm = (uint32_t)s;
  }

} while (0);

  return status;
}

/*****************************************************************************/

/** Close/destroy everything */

int     sa_stream_destroy(sa_stream_t *s)
{
  int               status = SA_SUCCESS;
  uint32_t          rc;
  MCI_GENERIC_PARMS GenericParms = { 0 };
  MCI_BUFFER_PARMS  BufferParms;

  if (!s)
    return os2_error(SA_ERROR_NO_INIT, "sa_stream_destroy", "s is null", 0);

  /* if the device was opened, close it */
  if (s->hwDeviceID) {

    /* prevent os2_mixer_event() from reacting to a buffer under-run */
    s->bufMin = 0;
    s->playing = FALSE;

    /* If another instance has already acquired the device the
     * MCI commands below will fail, so re-acquire it temporarily.
     * MCI_CLOSE will release the device to the previous owner. */
    rc = _mciSendCommand(s->hwDeviceID, MCI_ACQUIREDEVICE,
                         MCI_WAIT,
                         (void*)&GenericParms, 0);
    if (LOUSHORT(rc))
      os2_error(0, "sa_stream_destroy",
                "MCI_ACQUIREDEVICE - rc=", LOUSHORT(rc));

    /* stop the device (which may not actually be playing) */
    os2_stop_device(s->hwDeviceID);

    /* if hardware buffers were allocated, free them */
    if (s->bufCnt) {
      BufferParms.hwndCallback   = 0;
      BufferParms.ulStructLength = sizeof(MCI_BUFFER_PARMS);
      BufferParms.ulNumBuffers   = s->bufCnt;
      BufferParms.ulBufferSize   = s->bufSize;
      BufferParms.pBufList       = s->bufList;

      rc = _mciSendCommand(s->hwDeviceID, MCI_BUFFER,
                          MCI_WAIT | MCI_DEALLOCATE_MEMORY,
                          (void*)&BufferParms, 0);
      if (LOUSHORT(rc))
        status = os2_error(SA_ERROR_SYSTEM, "sa_stream_destroy",
                           "MCI_DEALLOCATE_MEMORY - rc=", LOUSHORT(rc));
    }

    rc = _mciSendCommand(s->hwDeviceID, MCI_CLOSE,
                        MCI_WAIT,
                        (void*)&GenericParms, 0);
    if (LOUSHORT(rc))
      status = os2_error(SA_ERROR_SYSTEM, "sa_stream_destroy",
                         "MCI_CLOSE - rc=", LOUSHORT(rc));
  }

  /* free other resources we allocated */
  DosFreeMem(s);

  return status;
}

/*****************************************************************************/

/** Interleaved playback function */

int     sa_stream_write(sa_stream_t * s, const void * data, size_t nbytes)
{
  uint32_t        rc;
  size_t          cnt;
  PMCI_MIX_BUFFER pHW;

  if (!s)
    return os2_error(SA_ERROR_NO_INIT, "sa_stream_write", "s is null", 0);
  if (!data)
    return os2_error(SA_ERROR_INVALID, "sa_stream_write", "data is null", 0);

  /* exit if no data */
  if (!nbytes)
    return SA_SUCCESS;

  /* This should only loop on the last write before sa_stream_drain()
   * is called;  at other times, 'nbytes' won't exceed 'bufSize'. */
  while (nbytes) {
    size_t  offs;
    size_t  left;

    /* get the count of free buffers, wait until at least one
     * is available (in practice, this should never block) */
    if (os2_get_free_count(s, 1))
      return SA_ERROR_SYSTEM;

    /* copy as much as will fit into the buffer */
    pHW = &(s->bufList[s->freeNdx]);

    offs = pHW->ulBufferLength;
    left = s->bufSize - offs;
    cnt = (nbytes > left) ? left : nbytes;
    memcpy(&((char*)pHW->pBuffer)[offs], (char*)data, cnt);

    pHW->ulBufferLength += cnt;
    nbytes -= cnt;
    data = (char*)data + cnt;

    /* don't dispatch the buffer until it has bufMin bytes */
    if (pHW->ulBufferLength < s->bufMin)
      continue;

    /* write the buffer to the device */
    rc = s->hwWriteProc(s->hwMixHandle, pHW, 1);
    if (LOUSHORT(rc)) {
      pHW->ulBufferLength = 0;
      return os2_error(SA_ERROR_SYSTEM, "sa_stream_write",
                       "mixWrite - rc=", LOUSHORT(rc));
    }

    /* signal the event thread that a new buffer is now in use */
    __atomic_increment(&s->usedNew);
    s->playing = TRUE;

    s->freeCnt--;
    s->freeNdx = (s->freeNdx + 1) % s->bufCnt;
  }

  return SA_SUCCESS;
}

/*****************************************************************************/

/** sync/timing */

int     sa_stream_get_position(sa_stream_t *s, sa_position_t position, int64_t *pos)
{
  uint32_t      rc;
  uint32_t      then;
  uint32_t      now;

  if (!s || !pos)
    return os2_error(SA_ERROR_NO_INIT, "sa_stream_get_position",
                     "s or pos is null", 0);

  if (position != SA_POSITION_WRITE_SOFTWARE)
    return os2_error(SA_ERROR_NOT_SUPPORTED, "sa_stream_get_position",
                     "unsupported postion type=", position);

  /* Return the count of bytes that are known to have been played
   * already plus an adjustment for the number that may have been
   * played since the last mixer event.  Since both 'writePos' and
   * 'writeTime' are volatile, the loop ensures both are in sync.
   * Note:  the MCI command to get stream position isn't usable - 
   * it returns a time value that resets when the stream is paused. */

  do {
    then = s->writeTime;
    s->writePos += __atomic_xchg(&s->writeNew, 0);
    *pos = s->writePos;

    /* adjust if device is playing & there's been at least one write */
    if (s->playing && s->writePos) {
      DosQuerySysInfo(QSV_MS_COUNT, QSV_MS_COUNT, &now, sizeof(now));
      *pos += ((now - then) * s->bps) / 1000;
    }
  } while (then != s->writeTime);

  return SA_SUCCESS;
}

/*****************************************************************************/

/** Resume playing after a pause */

int     sa_stream_resume(sa_stream_t *s)
{
  uint32_t          rc;
  MCI_GENERIC_PARMS GenericParms = { 0 };

  if (!s)
    return os2_error(SA_ERROR_NO_INIT, "sa_stream_resume",
                     "s is null", 0);

  rc = _mciSendCommand(s->hwDeviceID, MCI_ACQUIREDEVICE,
                      MCI_WAIT,
                      (void*)&GenericParms, 0);
  if (LOUSHORT(rc))
    return os2_error(SA_ERROR_SYSTEM, "sa_stream_resume",
                     "MCI_ACQUIREDEVICE - rc=", LOUSHORT(rc));

  /* this may produce a spurious error if the device
   * was just acquired, so report it but ignore it */
  rc = _mciSendCommand(s->hwDeviceID, MCI_RESUME,
                      MCI_WAIT,
                      (void*)&GenericParms, 0);
  if (LOUSHORT(rc))
    os2_error(SA_ERROR_SYSTEM, "sa_stream_resume",
              "MCI_RESUME - rc=", LOUSHORT(rc));

  /* reset the last write time so get_position() doesn't over-adjust */
  DosQuerySysInfo(QSV_MS_COUNT, QSV_MS_COUNT,
                  (void*)&s->writeTime, sizeof(s->writeTime));
  s->playing = TRUE;

  return SA_SUCCESS;
}

/*****************************************************************************/

/** Pause audio playback (do not empty the buffer) */

int     sa_stream_pause(sa_stream_t *s)
{
  if (!s)
    return os2_error(SA_ERROR_NO_INIT, "sa_stream_pause", "s is null", 0);

  /* pause & release device */
  s->playing = FALSE;
  return os2_pause_device(s->hwDeviceID, TRUE);
}

/*****************************************************************************/

/** Block until all audio has been played */

int     sa_stream_drain(sa_stream_t *s)
{
  int       status = SA_SUCCESS;
  char      buf[32];

  if (!s)
    return os2_error(SA_ERROR_NO_INIT, "sa_stream_drain", "s is null", 0);

  /* keep os2_mixer_event() from reacting to buffer under-runs */
  s->usedMin = 0;

  /* perform the smallest possible write to force any
   * partially-filled buffer to be written to the device */
  memset(buf, 0, sizeof(buf));
  s->bufMin = 0;
  sa_stream_write(s, buf, s->nchannels * SAOS2_SAMPLE_SIZE);

  /* DART won't start playing until 2 buffers have been written,
   * so write a dummy 2nd buffer if writePos is still zero */
  if (!s->writePos)
    s->writePos += __atomic_xchg(&s->writeNew, 0);
  if (!s->writePos)
    sa_stream_write(s, buf, s->nchannels * SAOS2_SAMPLE_SIZE);

  /* wait for all buffers to become free */
  if (!status)
    status = os2_get_free_count(s, s->bufCnt);
  s->playing = FALSE;

  /* stop the device so it doesn't misbehave due to an under-run */
  os2_stop_device(s->hwDeviceID);

  return status;
}

/*****************************************************************************/

/** Query how much can be written without blocking */

int     sa_stream_get_write_size(sa_stream_t *s, size_t *size)
{
  if (!s)
    return os2_error(SA_ERROR_NO_INIT, "sa_stream_get_write_size",
                     "s is null", 0);

  /* return a non-zero value here in case the upstream code ignores
   * the return code - if so, sa_stream_write() will fail instead */
  if (os2_get_free_count(s, 0)) {
    *size = s->bufSize;
    return SA_ERROR_SYSTEM;
  }

  *size = s->freeCnt * s->bufSize;

  return SA_SUCCESS;
}

/*****************************************************************************/

/** set absolute volume using a value ranging from 0.0 to 1.0 */

int     sa_stream_set_volume_abs(sa_stream_t *s, float vol)
{
  uint32_t      rc;
  MCI_SET_PARMS SetParms;

  if (!s)
    return os2_error(SA_ERROR_NO_INIT, "sa_stream_set_volume_abs",
                     "s is null", 0);

  /* convert f.p. value to an integer value ranging
   * from 0 to 100 and apply to both channels */
  SetParms.ulLevel = (vol * 100);
  SetParms.ulAudio = MCI_SET_AUDIO_ALL;

  rc = _mciSendCommand(s->hwDeviceID, MCI_SET,
                      MCI_WAIT | MCI_SET_AUDIO | MCI_SET_VOLUME,
                      (void*)&SetParms, 0);
  if (LOUSHORT(rc))
    return os2_error(SA_ERROR_SYSTEM, "sa_stream_set_volume_abs",
                     "MCI_SET_VOLUME - rc=", LOUSHORT(rc));

  return SA_SUCCESS;
}

/*****************************************************************************/

/** get absolute volume as a value ranging from 0.0 to 1.0 */

int     sa_stream_get_volume_abs(sa_stream_t *s, float *vol)
{
  int              status = SA_SUCCESS;
  uint32_t         rc;
  MCI_STATUS_PARMS StatusParms;

  if (!s || !vol)
    return os2_error(SA_ERROR_NO_INIT, "sa_stream_get_volume_abs",
                     "s or vol is null", 0);

  memset(&StatusParms, 0, sizeof(MCI_STATUS_PARMS));
  StatusParms.ulItem = MCI_STATUS_VOLUME;

  rc = _mciSendCommand(s->hwDeviceID, MCI_STATUS,
                      MCI_WAIT | MCI_STATUS_ITEM,
                      (void*)&StatusParms, 0);
  if (LOUSHORT(rc)) {
    /* if there's an error, return a reasonable value */
    StatusParms.ulReturn = (50 | 50 << 16);
    status = os2_error(SA_ERROR_SYSTEM, "sa_stream_get_volume_abs",
                       "MCI_STATUS_VOLUME - rc=", LOUSHORT(rc));
  }

  /* left channel is the low-order word, right channel is the
   * high-order word - convert the average of the channels from
   * an integer (range 0 - 100) to a floating point value */

  *vol = (LOUSHORT(StatusParms.ulReturn) +
          HIUSHORT(StatusParms.ulReturn)) / 200.0;

  return status;
}

/*****************************************************************************/
/*  Private (static) Functions                                               */
/*****************************************************************************/

/** signal the decode thread that a buffer is available -
 ** this runs on a separate high-priority thread created by DART */

static int32_t os2_mixer_event(uint32_t ulStatus, PMCI_MIX_BUFFER pBuffer,
                               uint32_t ulFlags)
{
  sa_stream_t * s;

  /* check for errors */
  if (ulFlags & MIX_STREAM_ERROR)
    os2_error(0, "os2_mixer_event", "MIX_STREAM_ERROR - status=", ulStatus);

  if (!(ulFlags & MIX_WRITE_COMPLETE))
    return os2_error(TRUE, "os2_mixer_event",
                     "unexpected event - flag=", ulFlags);

  if (!pBuffer || !pBuffer->ulUserParm)
    return os2_error(TRUE, "os2_mixer_event", "null pointer", 0);

  /* Note: this thread doesn't use a mutex to avoid a deadlock with the one
   * DART uses to prevent MCI operations while this function is running */
  s = (sa_stream_t *)pBuffer->ulUserParm;

  /* update the number of buffers that are now in use */
  s->usedCnt += __atomic_xchg(&s->usedNew, 0);
  s->usedCnt--;

  /* if fewer than 2 buffers are in use, enter recovery mode -
   * if we wait until they're all free, it's often too late; */
  if (s->usedCnt < s->usedMin) {
    s->playing = FALSE;
    os2_pause_device(s->hwDeviceID, FALSE);
    os2_error(0, "os2_mixer_event",
              "too few buffers in use - recovering", 0);
  }

  /* pass the number of newly played bytes to the other thread;
   * get the time so the other thread can estimate how many
   * additional bytes have been consumed since this event */
  __atomic_add(&s->writeNew, pBuffer->ulBufferLength);
  pBuffer->ulBufferLength = 0;
  DosQuerySysInfo(QSV_MS_COUNT, QSV_MS_COUNT,
                  (void*)&s->writeTime, sizeof(s->writeTime));

  /* signal the decode thread that a buffer is available */
  __atomic_increment(&s->freeNew);

  return TRUE;
}

/*****************************************************************************/

/** stop playback */

static void os2_stop_device(uint16_t hwDeviceID)
{
  uint32_t          rc;
  MCI_GENERIC_PARMS GenericParms = { 0 };

  rc = _mciSendCommand(hwDeviceID, MCI_STOP,
                      MCI_WAIT,
                      (void*)&GenericParms, 0);
  if (LOUSHORT(rc))
    os2_error(0, "os2_stop_device", "MCI_STOP - rc=", LOUSHORT(rc));

  return;
}

/*****************************************************************************/

/** pause playback and optionally release device */

static int  os2_pause_device(uint16_t hwDeviceID, uint32_t release)
{
  uint32_t          rc;
  MCI_GENERIC_PARMS GenericParms = { 0 };

  rc = _mciSendCommand(hwDeviceID, MCI_PAUSE,
                      MCI_WAIT,
                      (void*)&GenericParms, 0);
  if (LOUSHORT(rc))
    return os2_error(SA_ERROR_SYSTEM, "os2_pause_device",
                     "MCI_PAUSE - rc=", LOUSHORT(rc));

  if (release)
    _mciSendCommand(hwDeviceID, MCI_RELEASEDEVICE,
                   MCI_WAIT,
                   (void*)&GenericParms, 0);

  return SA_SUCCESS;
}

/*****************************************************************************/

/** update the count of free buffers, returning when 'count' are available */

static int  os2_get_free_count(sa_stream_t *s, int32_t count)
{
  uint32_t  timeout = 0;

  while (1) {
    uint32_t now;

    s->freeCnt += __atomic_xchg(&s->freeNew, 0);
    if (s->freeCnt >= count)
      break;

    /* get the current time in milliseconds */
    DosQuerySysInfo(QSV_MS_COUNT, QSV_MS_COUNT, &now, sizeof(now));
    if (!timeout)
      timeout = now + SAOS2_WAIT;

    if (now > timeout)
      return os2_error(SA_ERROR_SYSTEM, "os2_get_free_count",
                       "timed-out waiting for free buffer(s)", 0);

    DosSleep(1);
  }

  return SA_SUCCESS;
}

/*****************************************************************************/

#ifdef SAOS2_ERROR

/** display an error message & return whatever value was passed in */

static int  os2_error_msg(int rtn, char * func, char * msg, uint32_t err)
{
  if (!err)
    fprintf(stderr, "sa_os2 error - %s:  %s\n", func, msg);
  else
    fprintf(stderr, "sa_os2 error - %s:  %s %u\n", func, msg, err);
  fflush(stderr);

  return rtn;
}

#endif

/*****************************************************************************/
/*  Mozilla-specific Function                                                */
/*****************************************************************************/

/** load mdm.dll & get the entrypoint for mciSendCommand() */

static int  os2_load_mdm(void)
{
  uint32_t  rc;
  HMODULE   hmod;
  char      text[32];

  if (_mciSendCommand)
    return SA_SUCCESS;

  rc = DosLoadModule(text, sizeof(text), "MDM", &hmod);
  if (rc)
    return os2_error(SA_ERROR_SYSTEM, "os2_load_mdm",
                     "DosLoadModule - rc=", rc);

  /* the ordinal for mciSendCommand is '1' */
  rc = DosQueryProcAddr(hmod, 1, 0, (PFN*)&_mciSendCommand);
  if (rc) {
    _mciSendCommand = 0;
    return os2_error(SA_ERROR_SYSTEM, "os2_load_mdm",
                     "DosQueryProcAddr - rc=", rc);
  }

  return SA_SUCCESS;
}

/*****************************************************************************/
/*  Not Implemented / Not Supported                                          */
/*****************************************************************************/

#define UNSUPPORTED(func)   func { return SA_ERROR_NOT_SUPPORTED; }

UNSUPPORTED(int sa_stream_create_opaque(sa_stream_t **s, const char *client_name, sa_mode_t mode, const char *codec))
UNSUPPORTED(int sa_stream_set_write_lower_watermark(sa_stream_t *s, size_t size))
UNSUPPORTED(int sa_stream_set_read_lower_watermark(sa_stream_t *s, size_t size))
UNSUPPORTED(int sa_stream_set_write_upper_watermark(sa_stream_t *s, size_t size))
UNSUPPORTED(int sa_stream_set_read_upper_watermark(sa_stream_t *s, size_t size))
UNSUPPORTED(int sa_stream_set_channel_map(sa_stream_t *s, const sa_channel_t map[], unsigned int n))
UNSUPPORTED(int sa_stream_set_xrun_mode(sa_stream_t *s, sa_xrun_mode_t mode))
UNSUPPORTED(int sa_stream_set_non_interleaved(sa_stream_t *s, int enable))
UNSUPPORTED(int sa_stream_set_dynamic_rate(sa_stream_t *s, int enable))
UNSUPPORTED(int sa_stream_set_driver(sa_stream_t *s, const char *driver))
UNSUPPORTED(int sa_stream_start_thread(sa_stream_t *s, sa_event_callback_t callback))
UNSUPPORTED(int sa_stream_stop_thread(sa_stream_t *s))
UNSUPPORTED(int sa_stream_change_device(sa_stream_t *s, const char *device_name))
UNSUPPORTED(int sa_stream_change_read_volume(sa_stream_t *s, const int32_t vol[], unsigned int n))
UNSUPPORTED(int sa_stream_change_write_volume(sa_stream_t *s, const int32_t vol[], unsigned int n))
UNSUPPORTED(int sa_stream_change_rate(sa_stream_t *s, unsigned int rate))
UNSUPPORTED(int sa_stream_change_meta_data(sa_stream_t *s, const char *name, const void *data, size_t size))
UNSUPPORTED(int sa_stream_change_user_data(sa_stream_t *s, const void *value))
UNSUPPORTED(int sa_stream_set_adjust_rate(sa_stream_t *s, sa_adjust_t direction))
UNSUPPORTED(int sa_stream_set_adjust_nchannels(sa_stream_t *s, sa_adjust_t direction))
UNSUPPORTED(int sa_stream_set_adjust_pcm_format(sa_stream_t *s, sa_adjust_t direction))
UNSUPPORTED(int sa_stream_set_adjust_watermarks(sa_stream_t *s, sa_adjust_t direction))
UNSUPPORTED(int sa_stream_get_mode(sa_stream_t *s, sa_mode_t *access_mode))
UNSUPPORTED(int sa_stream_get_codec(sa_stream_t *s, char *codec, size_t *size))
UNSUPPORTED(int sa_stream_get_pcm_format(sa_stream_t *s, sa_pcm_format_t *format))
UNSUPPORTED(int sa_stream_get_rate(sa_stream_t *s, unsigned int *rate))
UNSUPPORTED(int sa_stream_get_nchannels(sa_stream_t *s, int *nchannels))
UNSUPPORTED(int sa_stream_get_user_data(sa_stream_t *s, void **value))
UNSUPPORTED(int sa_stream_get_write_lower_watermark(sa_stream_t *s, size_t *size))
UNSUPPORTED(int sa_stream_get_read_lower_watermark(sa_stream_t *s, size_t *size))
UNSUPPORTED(int sa_stream_get_write_upper_watermark(sa_stream_t *s, size_t *size))
UNSUPPORTED(int sa_stream_get_read_upper_watermark(sa_stream_t *s, size_t *size))
UNSUPPORTED(int sa_stream_get_channel_map(sa_stream_t *s, sa_channel_t map[], unsigned int *n))
UNSUPPORTED(int sa_stream_get_xrun_mode(sa_stream_t *s, sa_xrun_mode_t *mode))
UNSUPPORTED(int sa_stream_get_non_interleaved(sa_stream_t *s, int *enabled))
UNSUPPORTED(int sa_stream_get_dynamic_rate(sa_stream_t *s, int *enabled))
UNSUPPORTED(int sa_stream_get_driver(sa_stream_t *s, char *driver_name, size_t *size))
UNSUPPORTED(int sa_stream_get_device(sa_stream_t *s, char *device_name, size_t *size))
UNSUPPORTED(int sa_stream_get_read_volume(sa_stream_t *s, int32_t vol[], unsigned int *n))
UNSUPPORTED(int sa_stream_get_write_volume(sa_stream_t *s, int32_t vol[], unsigned int *n))
UNSUPPORTED(int sa_stream_get_meta_data(sa_stream_t *s, const char *name, void*data, size_t *size))
UNSUPPORTED(int sa_stream_get_adjust_rate(sa_stream_t *s, sa_adjust_t *direction))
UNSUPPORTED(int sa_stream_get_adjust_nchannels(sa_stream_t *s, sa_adjust_t *direction))
UNSUPPORTED(int sa_stream_get_adjust_pcm_format(sa_stream_t *s, sa_adjust_t *direction))
UNSUPPORTED(int sa_stream_get_adjust_watermarks(sa_stream_t *s, sa_adjust_t *direction))
UNSUPPORTED(int sa_stream_get_state(sa_stream_t *s, sa_state_t *state))
UNSUPPORTED(int sa_stream_get_event_error(sa_stream_t *s, sa_error_t *error))
UNSUPPORTED(int sa_stream_get_event_notify(sa_stream_t *s, sa_notify_t *notify))
UNSUPPORTED(int sa_stream_read(sa_stream_t *s, void *data, size_t nbytes))
UNSUPPORTED(int sa_stream_read_ni(sa_stream_t *s, unsigned int channel, void *data, size_t nbytes))
UNSUPPORTED(int sa_stream_write_ni(sa_stream_t *s, unsigned int channel, const void *data, size_t nbytes))
UNSUPPORTED(int sa_stream_pwrite(sa_stream_t *s, const void *data, size_t nbytes, int64_t offset, sa_seek_t whence))
UNSUPPORTED(int sa_stream_pwrite_ni(sa_stream_t *s, unsigned int channel, const void *data, size_t nbytes, int64_t offset, sa_seek_t whence))
UNSUPPORTED(int sa_stream_get_read_size(sa_stream_t *s, size_t *size))
UNSUPPORTED(int sa_stream_get_min_write(sa_stream_t *s, size_t *size))

const char *sa_strerror(int code) { return NULL; }

/*****************************************************************************/

