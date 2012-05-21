/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string.h>
#include "mozilla/Telemetry.h"
#include "sqlite3.h"
#include "nsThreadUtils.h"
#include "mozilla/Util.h"

namespace {

using namespace mozilla;

struct Histograms {
  const char *name;
  const Telemetry::ID readB;
  const Telemetry::ID writeB;
  const Telemetry::ID readMS;
  const Telemetry::ID writeMS;
  const Telemetry::ID syncMS;
};

#define SQLITE_TELEMETRY(FILENAME, HGRAM) \
  { FILENAME, \
    Telemetry::MOZ_SQLITE_ ## HGRAM ## _READ_B, \
    Telemetry::MOZ_SQLITE_ ## HGRAM ## _WRITE_B, \
    Telemetry::MOZ_SQLITE_ ## HGRAM ## _READ_MS, \
    Telemetry::MOZ_SQLITE_ ## HGRAM ## _WRITE_MS, \
    Telemetry::MOZ_SQLITE_ ## HGRAM ## _SYNC_MS \
  }

Histograms gHistograms[] = {
  SQLITE_TELEMETRY("places.sqlite", PLACES),
  SQLITE_TELEMETRY("urlclassifier3.sqlite", URLCLASSIFIER),
  SQLITE_TELEMETRY("cookies.sqlite", COOKIES),
  SQLITE_TELEMETRY("webappsstore.sqlite", WEBAPPS),
  SQLITE_TELEMETRY(NULL, OTHER)
};
#undef SQLITE_TELEMETRY

/** RAII class for measuring how long io takes on/off main thread
 */
class IOThreadAutoTimer {
public:
  /** 
   * IOThreadAutoTimer measures time spent in IO. Additionally it
   * automatically determines whether IO is happening on the main
   * thread and picks an appropriate histogram.
   *
   * @param id takes a telemetry histogram id. The id+1 must be an
   * equivalent histogram for the main thread. Eg, MOZ_SQLITE_OPEN_MS 
   * is followed by MOZ_SQLITE_OPEN_MAIN_THREAD_MS.
   */
  IOThreadAutoTimer(Telemetry::ID id)
    : start(TimeStamp::Now()),
      id(id)
  {
  }

  ~IOThreadAutoTimer() {
    PRUint32 mainThread = NS_IsMainThread() ? 1 : 0;
    Telemetry::AccumulateTimeDelta(static_cast<Telemetry::ID>(id + mainThread),
                                   start);
  }

private:
  const TimeStamp start;
  const Telemetry::ID id;
};

struct telemetry_file {
  sqlite3_file base;        // Base class.  Must be first
  Histograms *histograms;   // histograms pertaining to this file
  sqlite3_file pReal[1];    // This contains the vfs that actually does work
};

/*
** Close a telemetry_file.
*/
int
xClose(sqlite3_file *pFile)
{
  telemetry_file *p = (telemetry_file *)pFile;
  int rc;
  rc = p->pReal->pMethods->xClose(p->pReal);
  if( rc==SQLITE_OK ){
    delete p->base.pMethods;
    p->base.pMethods = NULL;
  }
  return rc;
}

/*
** Read data from a telemetry_file.
*/
int
xRead(sqlite3_file *pFile, void *zBuf, int iAmt, sqlite_int64 iOfst)
{
  telemetry_file *p = (telemetry_file *)pFile;
  IOThreadAutoTimer ioTimer(p->histograms->readMS);
  int rc;
  rc = p->pReal->pMethods->xRead(p->pReal, zBuf, iAmt, iOfst);
  // sqlite likes to read from empty files, this is normal, ignore it.
  if (rc != SQLITE_IOERR_SHORT_READ)
    Telemetry::Accumulate(p->histograms->readB, rc == SQLITE_OK ? iAmt : 0);
  return rc;
}

/*
** Write data to a telemetry_file.
*/
int
xWrite(sqlite3_file *pFile, const void *zBuf, int iAmt, sqlite_int64 iOfst)
{
  telemetry_file *p = (telemetry_file *)pFile;
  IOThreadAutoTimer ioTimer(p->histograms->writeMS);
  int rc;
  rc = p->pReal->pMethods->xWrite(p->pReal, zBuf, iAmt, iOfst);
  Telemetry::Accumulate(p->histograms->writeB, rc == SQLITE_OK ? iAmt : 0);
  return rc;
}

/*
** Truncate a telemetry_file.
*/
int
xTruncate(sqlite3_file *pFile, sqlite_int64 size)
{
  IOThreadAutoTimer ioTimer(Telemetry::MOZ_SQLITE_TRUNCATE_MS);
  telemetry_file *p = (telemetry_file *)pFile;
  int rc;
  Telemetry::AutoTimer<Telemetry::MOZ_SQLITE_TRUNCATE_MS> timer;
  rc = p->pReal->pMethods->xTruncate(p->pReal, size);
  return rc;
}

/*
** Sync a telemetry_file.
*/
int
xSync(sqlite3_file *pFile, int flags)
{
  telemetry_file *p = (telemetry_file *)pFile;
  IOThreadAutoTimer ioTimer(p->histograms->syncMS);
  return p->pReal->pMethods->xSync(p->pReal, flags);
}

/*
** Return the current file-size of a telemetry_file.
*/
int
xFileSize(sqlite3_file *pFile, sqlite_int64 *pSize)
{
  telemetry_file *p = (telemetry_file *)pFile;
  int rc;
  rc = p->pReal->pMethods->xFileSize(p->pReal, pSize);
  return rc;
}

/*
** Lock a telemetry_file.
*/
int
xLock(sqlite3_file *pFile, int eLock)
{
  telemetry_file *p = (telemetry_file *)pFile;
  int rc;
  rc = p->pReal->pMethods->xLock(p->pReal, eLock);
  return rc;
}

/*
** Unlock a telemetry_file.
*/
int
xUnlock(sqlite3_file *pFile, int eLock)
{
  telemetry_file *p = (telemetry_file *)pFile;
  int rc;
  rc = p->pReal->pMethods->xUnlock(p->pReal, eLock);
  return rc;
}

/*
** Check if another file-handle holds a RESERVED lock on a telemetry_file.
*/
int
xCheckReservedLock(sqlite3_file *pFile, int *pResOut)
{
  telemetry_file *p = (telemetry_file *)pFile;
  int rc = p->pReal->pMethods->xCheckReservedLock(p->pReal, pResOut);
  return rc;
}

/*
** File control method. For custom operations on a telemetry_file.
*/
int
xFileControl(sqlite3_file *pFile, int op, void *pArg)
{
  telemetry_file *p = (telemetry_file *)pFile;
  int rc = p->pReal->pMethods->xFileControl(p->pReal, op, pArg);
  return rc;
}

/*
** Return the sector-size in bytes for a telemetry_file.
*/
int
xSectorSize(sqlite3_file *pFile)
{
  telemetry_file *p = (telemetry_file *)pFile;
  int rc;
  rc = p->pReal->pMethods->xSectorSize(p->pReal);
  return rc;
}

/*
** Return the device characteristic flags supported by a telemetry_file.
*/
int
xDeviceCharacteristics(sqlite3_file *pFile)
{
  telemetry_file *p = (telemetry_file *)pFile;
  int rc;
  rc = p->pReal->pMethods->xDeviceCharacteristics(p->pReal);
  return rc;
}

/*
** Shared-memory operations.
*/
int
xShmLock(sqlite3_file *pFile, int ofst, int n, int flags)
{
  telemetry_file *p = (telemetry_file *)pFile;
  return p->pReal->pMethods->xShmLock(p->pReal, ofst, n, flags);
}

int
xShmMap(sqlite3_file *pFile, int iRegion, int szRegion, int isWrite, void volatile **pp)
{
  telemetry_file *p = (telemetry_file *)pFile;
  int rc;
  rc = p->pReal->pMethods->xShmMap(p->pReal, iRegion, szRegion, isWrite, pp);
  return rc;
}

void
xShmBarrier(sqlite3_file *pFile){
  telemetry_file *p = (telemetry_file *)pFile;
  p->pReal->pMethods->xShmBarrier(p->pReal);
}

int
xShmUnmap(sqlite3_file *pFile, int delFlag){
  telemetry_file *p = (telemetry_file *)pFile;
  int rc;
  rc = p->pReal->pMethods->xShmUnmap(p->pReal, delFlag);
  return rc;
}
 
int
xOpen(sqlite3_vfs* vfs, const char *zName, sqlite3_file* pFile,
          int flags, int *pOutFlags)
{
  IOThreadAutoTimer ioTimer(Telemetry::MOZ_SQLITE_OPEN_MS);
  Telemetry::AutoTimer<Telemetry::MOZ_SQLITE_OPEN_MS> timer;
  sqlite3_vfs *orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  int rc;
  telemetry_file *p = (telemetry_file *)pFile;
  Histograms *h = NULL;
  // check if the filename is one we are probing for
  for(size_t i = 0;i < sizeof(gHistograms)/sizeof(gHistograms[0]);i++) {
    h = &gHistograms[i];
    // last probe is the fallback probe
    if (!h->name)
      break;
    if (!zName)
      continue;
    const char *match = strstr(zName, h->name);
    if (!match)
      continue;
    char c = match[strlen(h->name)];
    // include -wal/-journal too
    if (!c || c == '-')
      break;
  }
  p->histograms = h;
  rc = orig_vfs->xOpen(orig_vfs, zName, p->pReal, flags, pOutFlags);
  if( rc != SQLITE_OK )
    return rc;
  if( p->pReal->pMethods ){
    sqlite3_io_methods *pNew = new sqlite3_io_methods;
    const sqlite3_io_methods *pSub = p->pReal->pMethods;
    memset(pNew, 0, sizeof(*pNew));
    pNew->iVersion = pSub->iVersion;
    pNew->xClose = xClose;
    pNew->xRead = xRead;
    pNew->xWrite = xWrite;
    pNew->xTruncate = xTruncate;
    pNew->xSync = xSync;
    pNew->xFileSize = xFileSize;
    pNew->xLock = xLock;
    pNew->xUnlock = xUnlock;
    pNew->xCheckReservedLock = xCheckReservedLock;
    pNew->xFileControl = xFileControl;
    pNew->xSectorSize = xSectorSize;
    pNew->xDeviceCharacteristics = xDeviceCharacteristics;
    if( pNew->iVersion>=2 ){
      pNew->xShmMap = pSub->xShmMap ? xShmMap : 0;
      pNew->xShmLock = pSub->xShmLock ? xShmLock : 0;
      pNew->xShmBarrier = pSub->xShmBarrier ? xShmBarrier : 0;
      pNew->xShmUnmap = pSub->xShmUnmap ? xShmUnmap : 0;
    }
    pFile->pMethods = pNew;
  }
  return rc;
}

int
xDelete(sqlite3_vfs* vfs, const char *zName, int syncDir)
{
  sqlite3_vfs *orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  return orig_vfs->xDelete(orig_vfs, zName, syncDir);
}

int
xAccess(sqlite3_vfs *vfs, const char *zName, int flags, int *pResOut)
{
  sqlite3_vfs *orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  return orig_vfs->xAccess(orig_vfs, zName, flags, pResOut);
}

int
xFullPathname(sqlite3_vfs *vfs, const char *zName, int nOut, char *zOut)
{
  sqlite3_vfs *orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  return orig_vfs->xFullPathname(orig_vfs, zName, nOut, zOut);
}

void*
xDlOpen(sqlite3_vfs *vfs, const char *zFilename)
{
  sqlite3_vfs *orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  return orig_vfs->xDlOpen(orig_vfs, zFilename);
}

void
xDlError(sqlite3_vfs *vfs, int nByte, char *zErrMsg)
{
  sqlite3_vfs *orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  orig_vfs->xDlError(orig_vfs, nByte, zErrMsg);
}

void 
(*xDlSym(sqlite3_vfs *vfs, void *pHdle, const char *zSym))(void){
  sqlite3_vfs *orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  return orig_vfs->xDlSym(orig_vfs, pHdle, zSym);
}

void
xDlClose(sqlite3_vfs *vfs, void *pHandle)
{
  sqlite3_vfs *orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  orig_vfs->xDlClose(orig_vfs, pHandle);
}

int
xRandomness(sqlite3_vfs *vfs, int nByte, char *zOut)
{
  sqlite3_vfs *orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  return orig_vfs->xRandomness(orig_vfs, nByte, zOut);
}

int
xSleep(sqlite3_vfs *vfs, int microseconds)
{
  sqlite3_vfs *orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  return orig_vfs->xSleep(orig_vfs, microseconds);
}

int
xCurrentTime(sqlite3_vfs *vfs, double *prNow)
{
  sqlite3_vfs *orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  return orig_vfs->xCurrentTime(orig_vfs, prNow);
}

int
xGetLastError(sqlite3_vfs *vfs, int nBuf, char *zBuf)
{
  sqlite3_vfs *orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  return orig_vfs->xGetLastError(orig_vfs, nBuf, zBuf);
}

int
xCurrentTimeInt64(sqlite3_vfs *vfs, sqlite3_int64 *piNow)
{
  sqlite3_vfs *orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  return orig_vfs->xCurrentTimeInt64(orig_vfs, piNow);
}

static
int
xSetSystemCall(sqlite3_vfs *vfs, const char *zName, sqlite3_syscall_ptr pFunc)
{
  sqlite3_vfs *orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  return orig_vfs->xSetSystemCall(orig_vfs, zName, pFunc);
}

static
sqlite3_syscall_ptr
xGetSystemCall(sqlite3_vfs *vfs, const char *zName)
{
  sqlite3_vfs *orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  return orig_vfs->xGetSystemCall(orig_vfs, zName);
}

static
const char *
xNextSystemCall(sqlite3_vfs *vfs, const char *zName)
{
  sqlite3_vfs *orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  return orig_vfs->xNextSystemCall(orig_vfs, zName);
}

}

namespace mozilla {
namespace storage {

sqlite3_vfs* ConstructTelemetryVFS()
{
#if defined(XP_WIN)
#define EXPECTED_VFS "win32"
#else
#define EXPECTED_VFS "unix"
#endif

  sqlite3_vfs *vfs = sqlite3_vfs_find(NULL);
  const bool expected_vfs = vfs->zName && !strcmp(vfs->zName, EXPECTED_VFS);
  if (!expected_vfs) {
    return NULL;
  }

  sqlite3_vfs *tvfs = new ::sqlite3_vfs;
  memset(tvfs, 0, sizeof(::sqlite3_vfs));
  tvfs->iVersion = 3;
  // If the SQLite VFS version is updated, this shim must be updated as well.
  MOZ_ASSERT(vfs->iVersion == tvfs->iVersion);
  tvfs->szOsFile = sizeof(telemetry_file) - sizeof(sqlite3_file) + vfs->szOsFile;
  tvfs->mxPathname = vfs->mxPathname;
  tvfs->zName = "telemetry-vfs";
  tvfs->pAppData = vfs;
  tvfs->xOpen = xOpen;
  tvfs->xDelete = xDelete;
  tvfs->xAccess = xAccess;
  tvfs->xFullPathname = xFullPathname;
  tvfs->xDlOpen = xDlOpen;
  tvfs->xDlError = xDlError;
  tvfs->xDlSym = xDlSym;
  tvfs->xDlClose = xDlClose;
  tvfs->xRandomness = xRandomness;
  tvfs->xSleep = xSleep;
  tvfs->xCurrentTime = xCurrentTime;
  tvfs->xGetLastError = xGetLastError;
  // Added in version 2.
  tvfs->xCurrentTimeInt64 = xCurrentTimeInt64;
  // Added in version 3.
  tvfs->xSetSystemCall = xSetSystemCall;
  tvfs->xGetSystemCall = xGetSystemCall;
  tvfs->xNextSystemCall = xNextSystemCall;

  return tvfs;
}

}
}
