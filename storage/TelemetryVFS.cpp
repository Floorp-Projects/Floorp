/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string.h>
#include "mozilla/Telemetry.h"
#include "sqlite3.h"
#include "nsThreadUtils.h"
#include "mozilla/dom/quota/PersistenceType.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/QuotaObject.h"
#include "mozilla/net/IOActivityMonitor.h"
#include "mozilla/IOInterposer.h"
#include "nsEscape.h"
#include "mozilla/StaticPrefs_storage.h"

#ifdef XP_WIN
#  include "mozilla/StaticPrefs_dom.h"
#endif

// The last VFS version for which this file has been updated.
#define LAST_KNOWN_VFS_VERSION 3

// The last io_methods version for which this file has been updated.
#define LAST_KNOWN_IOMETHODS_VERSION 3

namespace {

using namespace mozilla;
using namespace mozilla::dom::quota;
using namespace mozilla::net;

struct Histograms {
  const char* name;
  const Telemetry::HistogramID readB;
  const Telemetry::HistogramID writeB;
  const Telemetry::HistogramID readMS;
  const Telemetry::HistogramID writeMS;
  const Telemetry::HistogramID syncMS;
};

#define SQLITE_TELEMETRY(FILENAME, HGRAM)             \
  {                                                   \
    FILENAME, Telemetry::MOZ_SQLITE_##HGRAM##_READ_B, \
        Telemetry::MOZ_SQLITE_##HGRAM##_WRITE_B,      \
        Telemetry::MOZ_SQLITE_##HGRAM##_READ_MS,      \
        Telemetry::MOZ_SQLITE_##HGRAM##_WRITE_MS,     \
        Telemetry::MOZ_SQLITE_##HGRAM##_SYNC_MS       \
  }

Histograms gHistograms[] = {SQLITE_TELEMETRY("places.sqlite", PLACES),
                            SQLITE_TELEMETRY("cookies.sqlite", COOKIES),
                            SQLITE_TELEMETRY("webappsstore.sqlite", WEBAPPS),
                            SQLITE_TELEMETRY(nullptr, OTHER)};
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
   *
   * @param aOp optionally takes an IO operation to report through the
   * IOInterposer. Filename will be reported as NULL, and reference will be
   * either "sqlite-mainthread" or "sqlite-otherthread".
   */
  explicit IOThreadAutoTimer(
      Telemetry::HistogramID aId,
      IOInterposeObserver::Operation aOp = IOInterposeObserver::OpNone)
      : start(TimeStamp::Now()),
        id(aId)
#if !defined(XP_WIN)
        ,
        op(aOp)
#endif
  {
  }

  /**
   * This constructor is for when we want to report an operation to
   * IOInterposer but do not require a telemetry probe.
   *
   * @param aOp IO Operation to report through the IOInterposer.
   */
  explicit IOThreadAutoTimer(IOInterposeObserver::Operation aOp)
      : start(TimeStamp::Now()),
        id(Telemetry::HistogramCount)
#if !defined(XP_WIN)
        ,
        op(aOp)
#endif
  {
  }

  ~IOThreadAutoTimer() {
    TimeStamp end(TimeStamp::Now());
    uint32_t mainThread = NS_IsMainThread() ? 1 : 0;
    if (id != Telemetry::HistogramCount) {
      Telemetry::AccumulateTimeDelta(
          static_cast<Telemetry::HistogramID>(id + mainThread), start, end);
    }
    // We don't report SQLite I/O on Windows because we have a comprehensive
    // mechanism for intercepting I/O on that platform that captures a superset
    // of the data captured here.
#if !defined(XP_WIN)
    if (IOInterposer::IsObservedOperation(op)) {
      const char* main_ref = "sqlite-mainthread";
      const char* other_ref = "sqlite-otherthread";

      // Create observation
      IOInterposeObserver::Observation ob(op, start, end,
                                          (mainThread ? main_ref : other_ref));
      // Report observation
      IOInterposer::Report(ob);
    }
#endif /* !defined(XP_WIN) */
  }

 private:
  const TimeStamp start;
  const Telemetry::HistogramID id;
#if !defined(XP_WIN)
  IOInterposeObserver::Operation op;
#endif
};

struct telemetry_file {
  // Base class.  Must be first
  sqlite3_file base;

  // histograms pertaining to this file
  Histograms* histograms;

  // quota object for this file
  RefPtr<QuotaObject> quotaObject;

  // The chunk size for this file. See the documentation for
  // sqlite3_file_control() and FCNTL_CHUNK_SIZE.
  int fileChunkSize;

  // The filename
  char* location;

  // This contains the vfs that actually does work
  sqlite3_file pReal[1];
};

already_AddRefed<QuotaObject> GetQuotaObjectFromName(const char* zName) {
  MOZ_ASSERT(zName);

  const char* directoryLockIdParam =
      sqlite3_uri_parameter(zName, "directoryLockId");
  if (!directoryLockIdParam) {
    return nullptr;
  }

  nsresult rv;
  const int64_t directoryLockId =
      nsDependentCString(directoryLockIdParam).ToInteger64(&rv);
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  return quotaManager->GetQuotaObject(directoryLockId,
                                      NS_ConvertUTF8toUTF16(zName));
}

void MaybeEstablishQuotaControl(const char* zName, telemetry_file* pFile,
                                int flags) {
  MOZ_ASSERT(pFile);
  MOZ_ASSERT(!pFile->quotaObject);

  if (!(flags & (SQLITE_OPEN_URI | SQLITE_OPEN_WAL))) {
    return;
  }
  pFile->quotaObject = GetQuotaObjectFromName(zName);
}

/*
** Close a telemetry_file.
*/
int xClose(sqlite3_file* pFile) {
  telemetry_file* p = (telemetry_file*)pFile;
  int rc;
  {  // Scope for IOThreadAutoTimer
    IOThreadAutoTimer ioTimer(IOInterposeObserver::OpClose);
    rc = p->pReal->pMethods->xClose(p->pReal);
  }
  if (rc == SQLITE_OK) {
    delete p->base.pMethods;
    p->base.pMethods = nullptr;
    p->quotaObject = nullptr;
    delete[] p->location;
#ifdef DEBUG
    p->fileChunkSize = 0;
#endif
  }
  return rc;
}

/*
** Read data from a telemetry_file.
*/
int xRead(sqlite3_file* pFile, void* zBuf, int iAmt, sqlite_int64 iOfst) {
  telemetry_file* p = (telemetry_file*)pFile;
  IOThreadAutoTimer ioTimer(p->histograms->readMS, IOInterposeObserver::OpRead);
  int rc;
  rc = p->pReal->pMethods->xRead(p->pReal, zBuf, iAmt, iOfst);
  if (rc == SQLITE_OK && IOActivityMonitor::IsActive()) {
    IOActivityMonitor::Read(nsDependentCString(p->location), iAmt);
  }
  // sqlite likes to read from empty files, this is normal, ignore it.
  if (rc != SQLITE_IOERR_SHORT_READ)
    Telemetry::Accumulate(p->histograms->readB, rc == SQLITE_OK ? iAmt : 0);
  return rc;
}

/*
** Return the current file-size of a telemetry_file.
*/
int xFileSize(sqlite3_file* pFile, sqlite_int64* pSize) {
  IOThreadAutoTimer ioTimer(IOInterposeObserver::OpStat);
  telemetry_file* p = (telemetry_file*)pFile;
  int rc;
  rc = p->pReal->pMethods->xFileSize(p->pReal, pSize);
  return rc;
}

/*
** Write data to a telemetry_file.
*/
int xWrite(sqlite3_file* pFile, const void* zBuf, int iAmt,
           sqlite_int64 iOfst) {
  telemetry_file* p = (telemetry_file*)pFile;
  IOThreadAutoTimer ioTimer(p->histograms->writeMS,
                            IOInterposeObserver::OpWrite);
  int rc;
  if (p->quotaObject) {
    MOZ_ASSERT(INT64_MAX - iOfst >= iAmt);
    if (!p->quotaObject->MaybeUpdateSize(iOfst + iAmt, /* aTruncate */ false)) {
      return SQLITE_FULL;
    }
  }
  rc = p->pReal->pMethods->xWrite(p->pReal, zBuf, iAmt, iOfst);
  if (rc == SQLITE_OK && IOActivityMonitor::IsActive()) {
    IOActivityMonitor::Write(nsDependentCString(p->location), iAmt);
  }

  Telemetry::Accumulate(p->histograms->writeB, rc == SQLITE_OK ? iAmt : 0);
  if (p->quotaObject && rc != SQLITE_OK) {
    NS_WARNING(
        "xWrite failed on a quota-controlled file, attempting to "
        "update its current size...");
    sqlite_int64 currentSize;
    if (xFileSize(pFile, &currentSize) == SQLITE_OK) {
      DebugOnly<bool> res =
          p->quotaObject->MaybeUpdateSize(currentSize, /* aTruncate */ true);
      MOZ_ASSERT(res);
    }
  }
  return rc;
}

/*
** Truncate a telemetry_file.
*/
int xTruncate(sqlite3_file* pFile, sqlite_int64 size) {
  IOThreadAutoTimer ioTimer(Telemetry::MOZ_SQLITE_TRUNCATE_MS);
  telemetry_file* p = (telemetry_file*)pFile;
  int rc;
  Telemetry::AutoTimer<Telemetry::MOZ_SQLITE_TRUNCATE_MS> timer;
  if (p->quotaObject) {
    if (p->fileChunkSize > 0) {
      // Round up to the smallest multiple of the chunk size that will hold all
      // the data.
      size =
          ((size + p->fileChunkSize - 1) / p->fileChunkSize) * p->fileChunkSize;
    }
    if (!p->quotaObject->MaybeUpdateSize(size, /* aTruncate */ true)) {
      return SQLITE_FULL;
    }
  }
  rc = p->pReal->pMethods->xTruncate(p->pReal, size);
  if (p->quotaObject) {
    if (rc == SQLITE_OK) {
#ifdef DEBUG
      // Make sure xTruncate set the size exactly as we calculated above.
      sqlite_int64 newSize;
      MOZ_ASSERT(xFileSize(pFile, &newSize) == SQLITE_OK);
      MOZ_ASSERT(newSize == size);
#endif
    } else {
      NS_WARNING(
          "xTruncate failed on a quota-controlled file, attempting to "
          "update its current size...");
      if (xFileSize(pFile, &size) == SQLITE_OK) {
        DebugOnly<bool> res =
            p->quotaObject->MaybeUpdateSize(size, /* aTruncate */ true);
        MOZ_ASSERT(res);
      }
    }
  }
  return rc;
}

/*
** Sync a telemetry_file.
*/
int xSync(sqlite3_file* pFile, int flags) {
  telemetry_file* p = (telemetry_file*)pFile;
  IOThreadAutoTimer ioTimer(p->histograms->syncMS,
                            IOInterposeObserver::OpFSync);
  return p->pReal->pMethods->xSync(p->pReal, flags);
}

/*
** Lock a telemetry_file.
*/
int xLock(sqlite3_file* pFile, int eLock) {
  telemetry_file* p = (telemetry_file*)pFile;
  int rc;
  rc = p->pReal->pMethods->xLock(p->pReal, eLock);
  return rc;
}

/*
** Unlock a telemetry_file.
*/
int xUnlock(sqlite3_file* pFile, int eLock) {
  telemetry_file* p = (telemetry_file*)pFile;
  int rc;
  rc = p->pReal->pMethods->xUnlock(p->pReal, eLock);
  return rc;
}

/*
** Check if another file-handle holds a RESERVED lock on a telemetry_file.
*/
int xCheckReservedLock(sqlite3_file* pFile, int* pResOut) {
  telemetry_file* p = (telemetry_file*)pFile;
  int rc = p->pReal->pMethods->xCheckReservedLock(p->pReal, pResOut);
  return rc;
}

/*
** File control method. For custom operations on a telemetry_file.
*/
int xFileControl(sqlite3_file* pFile, int op, void* pArg) {
  telemetry_file* p = (telemetry_file*)pFile;
  int rc;
  // Hook SQLITE_FCNTL_SIZE_HINT for quota-controlled files and do the necessary
  // work before passing to the SQLite VFS.
  if (op == SQLITE_FCNTL_SIZE_HINT && p->quotaObject) {
    sqlite3_int64 hintSize = *static_cast<sqlite3_int64*>(pArg);
    sqlite3_int64 currentSize;
    rc = xFileSize(pFile, &currentSize);
    if (rc != SQLITE_OK) {
      return rc;
    }
    if (hintSize > currentSize) {
      rc = xTruncate(pFile, hintSize);
      if (rc != SQLITE_OK) {
        return rc;
      }
    }
  }
  rc = p->pReal->pMethods->xFileControl(p->pReal, op, pArg);
  // Grab the file chunk size after the SQLite VFS has approved.
  if (op == SQLITE_FCNTL_CHUNK_SIZE && rc == SQLITE_OK) {
    p->fileChunkSize = *static_cast<int*>(pArg);
  }
#ifdef DEBUG
  if (op == SQLITE_FCNTL_SIZE_HINT && p->quotaObject && rc == SQLITE_OK) {
    sqlite3_int64 hintSize = *static_cast<sqlite3_int64*>(pArg);
    if (p->fileChunkSize > 0) {
      hintSize = ((hintSize + p->fileChunkSize - 1) / p->fileChunkSize) *
                 p->fileChunkSize;
    }
    sqlite3_int64 currentSize;
    MOZ_ASSERT(xFileSize(pFile, &currentSize) == SQLITE_OK);
    MOZ_ASSERT(currentSize >= hintSize);
  }
#endif
  return rc;
}

/*
** Return the sector-size in bytes for a telemetry_file.
*/
int xSectorSize(sqlite3_file* pFile) {
  telemetry_file* p = (telemetry_file*)pFile;
  int rc;
  rc = p->pReal->pMethods->xSectorSize(p->pReal);
  return rc;
}

/*
** Return the device characteristic flags supported by a telemetry_file.
*/
int xDeviceCharacteristics(sqlite3_file* pFile) {
  telemetry_file* p = (telemetry_file*)pFile;
  int rc;
  rc = p->pReal->pMethods->xDeviceCharacteristics(p->pReal);
  return rc;
}

/*
** Shared-memory operations.
*/
int xShmLock(sqlite3_file* pFile, int ofst, int n, int flags) {
  telemetry_file* p = (telemetry_file*)pFile;
  return p->pReal->pMethods->xShmLock(p->pReal, ofst, n, flags);
}

int xShmMap(sqlite3_file* pFile, int iRegion, int szRegion, int isWrite,
            void volatile** pp) {
  telemetry_file* p = (telemetry_file*)pFile;
  int rc;
  rc = p->pReal->pMethods->xShmMap(p->pReal, iRegion, szRegion, isWrite, pp);
  return rc;
}

void xShmBarrier(sqlite3_file* pFile) {
  telemetry_file* p = (telemetry_file*)pFile;
  p->pReal->pMethods->xShmBarrier(p->pReal);
}

int xShmUnmap(sqlite3_file* pFile, int delFlag) {
  telemetry_file* p = (telemetry_file*)pFile;
  int rc;
  rc = p->pReal->pMethods->xShmUnmap(p->pReal, delFlag);
  return rc;
}

int xFetch(sqlite3_file* pFile, sqlite3_int64 iOff, int iAmt, void** pp) {
  telemetry_file* p = (telemetry_file*)pFile;
  MOZ_ASSERT(p->pReal->pMethods->iVersion >= 3);
  return p->pReal->pMethods->xFetch(p->pReal, iOff, iAmt, pp);
}

int xUnfetch(sqlite3_file* pFile, sqlite3_int64 iOff, void* pResOut) {
  telemetry_file* p = (telemetry_file*)pFile;
  MOZ_ASSERT(p->pReal->pMethods->iVersion >= 3);
  return p->pReal->pMethods->xUnfetch(p->pReal, iOff, pResOut);
}

int xOpen(sqlite3_vfs* vfs, const char* zName, sqlite3_file* pFile, int flags,
          int* pOutFlags) {
  IOThreadAutoTimer ioTimer(Telemetry::MOZ_SQLITE_OPEN_MS,
                            IOInterposeObserver::OpCreateOrOpen);
  Telemetry::AutoTimer<Telemetry::MOZ_SQLITE_OPEN_MS> timer;
  sqlite3_vfs* orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  int rc;
  telemetry_file* p = (telemetry_file*)pFile;
  Histograms* h = nullptr;
  // check if the filename is one we are probing for
  for (size_t i = 0; i < sizeof(gHistograms) / sizeof(gHistograms[0]); i++) {
    h = &gHistograms[i];
    // last probe is the fallback probe
    if (!h->name) break;
    if (!zName) continue;
    const char* match = strstr(zName, h->name);
    if (!match) continue;
    char c = match[strlen(h->name)];
    // include -wal/-journal too
    if (!c || c == '-') break;
  }
  p->histograms = h;

  MaybeEstablishQuotaControl(zName, p, flags);

  rc = orig_vfs->xOpen(orig_vfs, zName, p->pReal, flags, pOutFlags);
  if (rc != SQLITE_OK) return rc;

  if (zName) {
    p->location = new char[7 + strlen(zName) + 1];
    strcpy(p->location, "file://");
    strcpy(p->location + 7, zName);
  } else {
    p->location = new char[8];
    strcpy(p->location, "file://");
  }

  if (p->pReal->pMethods) {
    sqlite3_io_methods* pNew = new sqlite3_io_methods;
    const sqlite3_io_methods* pSub = p->pReal->pMethods;
    memset(pNew, 0, sizeof(*pNew));
    // If the io_methods version is higher than the last known one, you should
    // update this VFS adding appropriate IO methods for any methods added in
    // the version change.
    pNew->iVersion = pSub->iVersion;
    MOZ_ASSERT(pNew->iVersion <= LAST_KNOWN_IOMETHODS_VERSION);
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
    if (pNew->iVersion >= 2) {
      // Methods added in version 2.
      pNew->xShmMap = pSub->xShmMap ? xShmMap : 0;
      pNew->xShmLock = pSub->xShmLock ? xShmLock : 0;
      pNew->xShmBarrier = pSub->xShmBarrier ? xShmBarrier : 0;
      pNew->xShmUnmap = pSub->xShmUnmap ? xShmUnmap : 0;
    }
    if (pNew->iVersion >= 3) {
      // Methods added in version 3.
      // SQLite 3.7.17 calls these methods without checking for nullptr first,
      // so we always define them.  Verify that we're not going to call
      // nullptrs, though.
      MOZ_ASSERT(pSub->xFetch);
      pNew->xFetch = xFetch;
      MOZ_ASSERT(pSub->xUnfetch);
      pNew->xUnfetch = xUnfetch;
    }
    pFile->pMethods = pNew;
  }
  return rc;
}

int xDelete(sqlite3_vfs* vfs, const char* zName, int syncDir) {
  sqlite3_vfs* orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  int rc;
  RefPtr<QuotaObject> quotaObject;

  if (StringEndsWith(nsDependentCString(zName), "-wal"_ns)) {
    quotaObject = GetQuotaObjectFromName(zName);
  }

  rc = orig_vfs->xDelete(orig_vfs, zName, syncDir);
  if (rc == SQLITE_OK && quotaObject) {
    MOZ_ALWAYS_TRUE(quotaObject->MaybeUpdateSize(0, /* aTruncate */ true));
  }

  return rc;
}

int xAccess(sqlite3_vfs* vfs, const char* zName, int flags, int* pResOut) {
  sqlite3_vfs* orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  return orig_vfs->xAccess(orig_vfs, zName, flags, pResOut);
}

int xFullPathname(sqlite3_vfs* vfs, const char* zName, int nOut, char* zOut) {
#if defined(XP_WIN)
  // SQLite uses GetFullPathnameW which also normailizes file path. If a file
  // component ends with a dot, it would be removed. However, it's not desired.
  //
  // And that would result SQLite uses wrong database and quotaObject.
  // Note that we are safe to avoid the GetFullPathnameW call for \\?\ prefixed
  // paths.
  // And note that this hack will be removed once the issue is fixed directly in
  // SQLite.

  // zName that starts with "//?/" is the case when a file URI was passed and
  // zName that starts with "\\?\" is the case when a normal path was passed
  // (not file URI).
  if (StaticPrefs::dom_quotaManager_overrideXFullPathname() &&
      ((zName[0] == '/' && zName[1] == '/' && zName[2] == '?' &&
        zName[3] == '/') ||
       (zName[0] == '\\' && zName[1] == '\\' && zName[2] == '?' &&
        zName[3] == '\\'))) {
    MOZ_ASSERT(nOut >= vfs->mxPathname);
    MOZ_ASSERT(nOut > strlen(zName));

    size_t index = 0;
    while (zName[index] != '\0') {
      if (zName[index] == '/') {
        zOut[index] = '\\';
      } else {
        zOut[index] = zName[index];
      }

      index++;
    }
    zOut[index] = '\0';

    return SQLITE_OK;
  }
#endif

  sqlite3_vfs* orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  return orig_vfs->xFullPathname(orig_vfs, zName, nOut, zOut);
}

void* xDlOpen(sqlite3_vfs* vfs, const char* zFilename) {
  sqlite3_vfs* orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  return orig_vfs->xDlOpen(orig_vfs, zFilename);
}

void xDlError(sqlite3_vfs* vfs, int nByte, char* zErrMsg) {
  sqlite3_vfs* orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  orig_vfs->xDlError(orig_vfs, nByte, zErrMsg);
}

void (*xDlSym(sqlite3_vfs* vfs, void* pHdle, const char* zSym))(void) {
  sqlite3_vfs* orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  return orig_vfs->xDlSym(orig_vfs, pHdle, zSym);
}

void xDlClose(sqlite3_vfs* vfs, void* pHandle) {
  sqlite3_vfs* orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  orig_vfs->xDlClose(orig_vfs, pHandle);
}

int xRandomness(sqlite3_vfs* vfs, int nByte, char* zOut) {
  sqlite3_vfs* orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  return orig_vfs->xRandomness(orig_vfs, nByte, zOut);
}

int xSleep(sqlite3_vfs* vfs, int microseconds) {
  sqlite3_vfs* orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  return orig_vfs->xSleep(orig_vfs, microseconds);
}

int xCurrentTime(sqlite3_vfs* vfs, double* prNow) {
  sqlite3_vfs* orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  return orig_vfs->xCurrentTime(orig_vfs, prNow);
}

int xGetLastError(sqlite3_vfs* vfs, int nBuf, char* zBuf) {
  sqlite3_vfs* orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  return orig_vfs->xGetLastError(orig_vfs, nBuf, zBuf);
}

int xCurrentTimeInt64(sqlite3_vfs* vfs, sqlite3_int64* piNow) {
  sqlite3_vfs* orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  return orig_vfs->xCurrentTimeInt64(orig_vfs, piNow);
}

static int xSetSystemCall(sqlite3_vfs* vfs, const char* zName,
                          sqlite3_syscall_ptr pFunc) {
  sqlite3_vfs* orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  return orig_vfs->xSetSystemCall(orig_vfs, zName, pFunc);
}

static sqlite3_syscall_ptr xGetSystemCall(sqlite3_vfs* vfs, const char* zName) {
  sqlite3_vfs* orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  return orig_vfs->xGetSystemCall(orig_vfs, zName);
}

static const char* xNextSystemCall(sqlite3_vfs* vfs, const char* zName) {
  sqlite3_vfs* orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  return orig_vfs->xNextSystemCall(orig_vfs, zName);
}

}  // namespace

namespace mozilla {
namespace storage {

const char* GetTelemetryVFSName(bool exclusive) {
  return exclusive ? "telemetry-vfs-excl" : "telemetry-vfs";
}

UniquePtr<sqlite3_vfs> ConstructTelemetryVFS(bool exclusive) {
#if defined(XP_WIN)
#  define EXPECTED_VFS "win32"
#  define EXPECTED_VFS_EXCL "win32"
#else
#  define EXPECTED_VFS "unix"
#  define EXPECTED_VFS_EXCL "unix-excl"
#endif

  bool expected_vfs;
  sqlite3_vfs* vfs;
  if (!exclusive) {
    // Use the non-exclusive VFS.
    vfs = sqlite3_vfs_find(nullptr);
    expected_vfs = vfs->zName && !strcmp(vfs->zName, EXPECTED_VFS);
  } else {
    vfs = sqlite3_vfs_find(EXPECTED_VFS_EXCL);
    expected_vfs = (vfs != nullptr);
  }
  if (!expected_vfs) {
    return nullptr;
  }

  auto tvfs = MakeUnique<::sqlite3_vfs>();
  memset(tvfs.get(), 0, sizeof(::sqlite3_vfs));
  // If the VFS version is higher than the last known one, you should update
  // this VFS adding appropriate methods for any methods added in the version
  // change.
  tvfs->iVersion = vfs->iVersion;
  MOZ_ASSERT(vfs->iVersion <= LAST_KNOWN_VFS_VERSION);
  tvfs->szOsFile =
      sizeof(telemetry_file) - sizeof(sqlite3_file) + vfs->szOsFile;
  tvfs->mxPathname = vfs->mxPathname;
  tvfs->zName = GetTelemetryVFSName(exclusive);
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
  if (tvfs->iVersion >= 2) {
    // Methods added in version 2.
    tvfs->xCurrentTimeInt64 = xCurrentTimeInt64;
  }
  if (tvfs->iVersion >= 3) {
    // Methods added in version 3.
    tvfs->xSetSystemCall = xSetSystemCall;
    tvfs->xGetSystemCall = xGetSystemCall;
    tvfs->xNextSystemCall = xNextSystemCall;
  }
  return tvfs;
}

already_AddRefed<QuotaObject> GetQuotaObjectForFile(sqlite3_file* pFile) {
  MOZ_ASSERT(pFile);

  telemetry_file* p = (telemetry_file*)pFile;
  RefPtr<QuotaObject> result = p->quotaObject;
  return result.forget();
}

}  // namespace storage
}  // namespace mozilla
