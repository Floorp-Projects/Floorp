/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string.h>
#include "mozilla/Telemetry.h"
#include "mozilla/Preferences.h"
#include "sqlite3.h"
#include "nsThreadUtils.h"
#include "mozilla/dom/quota/PersistenceType.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/QuotaObject.h"
#include "mozilla/IOInterposer.h"

// The last VFS version for which this file has been updated.
#define LAST_KNOWN_VFS_VERSION 3

// The last io_methods version for which this file has been updated.
#define LAST_KNOWN_IOMETHODS_VERSION 3

/**
 * This preference is a workaround to allow users/sysadmins to identify
 * that the profile exists on an NFS share whose implementation
 * is incompatible with SQLite's default locking implementation.
 * Bug 433129 attempted to automatically identify such file-systems, 
 * but a reliable way was not found and it was determined that the fallback 
 * locking is slower than POSIX locking, so we do not want to do it by default.
*/
#define PREF_NFS_FILESYSTEM   "storage.nfs_filesystem"

namespace {

using namespace mozilla;
using namespace mozilla::dom::quota;

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
  SQLITE_TELEMETRY("cookies.sqlite", COOKIES),
  SQLITE_TELEMETRY("webappsstore.sqlite", WEBAPPS),
  SQLITE_TELEMETRY(nullptr, OTHER)
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
   *
   * @param aOp optionally takes an IO operation to report through the
   * IOInterposer. Filename will be reported as NULL, and reference will be
   * either "sqlite-mainthread" or "sqlite-otherthread".
   */
  explicit IOThreadAutoTimer(Telemetry::ID aId,
    IOInterposeObserver::Operation aOp = IOInterposeObserver::OpNone)
    : start(TimeStamp::Now()),
      id(aId),
      op(aOp)
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
      id(Telemetry::HistogramCount),
      op(aOp)
  {
  }

  ~IOThreadAutoTimer()
  {
    TimeStamp end(TimeStamp::Now());
    uint32_t mainThread = NS_IsMainThread() ? 1 : 0;
    if (id != Telemetry::HistogramCount) {
      Telemetry::AccumulateTimeDelta(static_cast<Telemetry::ID>(id + mainThread),
                                     start, end);
    }
    // We don't report SQLite I/O on Windows because we have a comprehensive
    // mechanism for intercepting I/O on that platform that captures a superset
    // of the data captured here.
#if defined(MOZ_ENABLE_PROFILER_SPS) && !defined(XP_WIN)
    if (IOInterposer::IsObservedOperation(op)) {
      const char* main_ref  = "sqlite-mainthread";
      const char* other_ref = "sqlite-otherthread";

      // Create observation
      IOInterposeObserver::Observation ob(op, start, end,
                                          (mainThread ? main_ref : other_ref));
      // Report observation
      IOInterposer::Report(ob);
    }
#endif /* defined(MOZ_ENABLE_PROFILER_SPS) && !defined(XP_WIN) */
  }

private:
  const TimeStamp start;
  const Telemetry::ID id;
  IOInterposeObserver::Operation op;
};

struct telemetry_file {
  // Base class.  Must be first
  sqlite3_file base;

  // histograms pertaining to this file
  Histograms *histograms;

  // quota object for this file
  RefPtr<QuotaObject> quotaObject;

  // The chunk size for this file. See the documentation for
  // sqlite3_file_control() and FCNTL_CHUNK_SIZE.
  int fileChunkSize;

  // This contains the vfs that actually does work
  sqlite3_file pReal[1];
};

const char*
DatabasePathFromWALPath(const char *zWALName)
{
  /**
   * Do some sketchy pointer arithmetic to find the parameter key. The WAL
   * filename is in the middle of a big allocated block that contains:
   *
   *   - Random Values
   *   - Main Database Path
   *   - \0
   *   - Multiple URI components consisting of:
   *     - Key
   *     - \0
   *     - Value
   *     - \0
   *   - \0
   *   - Journal Path
   *   - \0
   *   - WAL Path (zWALName)
   *   - \0
   *
   * Because the main database path is preceded by a random value we have to be
   * careful when trying to figure out when we should terminate this loop.
   */
  MOZ_ASSERT(zWALName);

  nsDependentCSubstring dbPath(zWALName, strlen(zWALName));

  // Chop off the "-wal" suffix.
  NS_NAMED_LITERAL_CSTRING(kWALSuffix, "-wal");
  MOZ_ASSERT(StringEndsWith(dbPath, kWALSuffix));

  dbPath.Rebind(zWALName, dbPath.Length() - kWALSuffix.Length());
  MOZ_ASSERT(!dbPath.IsEmpty());

  // We want to scan to the end of the key/value URI pairs. Skip the preceding
  // null and go to the last char of the journal path.
  const char* cursor = zWALName - 2;

  // Make sure we just skipped a null.
  MOZ_ASSERT(!*(cursor + 1));

  // Walk backwards over the journal path.
  while (*cursor) {
    cursor--;
  }

  // There should be another null here.
  cursor--;
  MOZ_ASSERT(!*cursor);

  // Back up one more char to the last char of the previous string. It may be
  // the database path or it may be a key/value URI pair.
  cursor--;

#ifdef DEBUG
  {
    // Verify that we just walked over the journal path. Account for the two
    // nulls we just skipped.
    const char *journalStart = cursor + 3;

    nsDependentCSubstring journalPath(journalStart,
                                      strlen(journalStart));

    // Chop off the "-journal" suffix.
    NS_NAMED_LITERAL_CSTRING(kJournalSuffix, "-journal");
    MOZ_ASSERT(StringEndsWith(journalPath, kJournalSuffix));

    journalPath.Rebind(journalStart,
                        journalPath.Length() - kJournalSuffix.Length());
    MOZ_ASSERT(!journalPath.IsEmpty());

    // Make sure that the database name is a substring of the journal name.
    MOZ_ASSERT(journalPath == dbPath);
  }
#endif

  // Now we're either at the end of the key/value URI pairs or we're at the
  // end of the database path. Carefully walk backwards one character at a
  // time to do this safely without running past the beginning of the database
  // path.
  const char *const dbPathStart = dbPath.BeginReading();
  const char *dbPathCursor = dbPath.EndReading() - 1;
  bool isDBPath = true;

  while (true) {
    MOZ_ASSERT(*dbPathCursor, "dbPathCursor should never see a null char!");

    if (isDBPath) {
      isDBPath = dbPathStart <= dbPathCursor &&
                 *dbPathCursor == *cursor &&
                 *cursor;
    }

    if (!isDBPath) {
      // This isn't the database path so it must be a value. Scan past it and
      // the key also.
      for (size_t stringCount = 0; stringCount < 2; stringCount++) {
        // Scan past the string to the preceding null character.
        while (*cursor) {
          cursor--;
        }

        // Back up one more char to the last char of preceding string.
        cursor--;
      }

      // Reset and start again.
      dbPathCursor = dbPath.EndReading() - 1;
      isDBPath = true;

      continue;
    }

    MOZ_ASSERT(isDBPath);
    MOZ_ASSERT(*cursor);

    if (dbPathStart == dbPathCursor) {
      // Found the full database path, we're all done.
      MOZ_ASSERT(nsDependentCString(cursor) == dbPath);
      return cursor;
    }

    // Change the cursors and go through the loop again.
    cursor--;
    dbPathCursor--;
  }

  MOZ_CRASH("Should never get here!");
}

already_AddRefed<QuotaObject>
GetQuotaObjectFromNameAndParameters(const char *zName,
                                    const char *zURIParameterKey)
{
  MOZ_ASSERT(zName);
  MOZ_ASSERT(zURIParameterKey);

  const char *persistenceType =
    sqlite3_uri_parameter(zURIParameterKey, "persistenceType");
  if (!persistenceType) {
    return nullptr;
  }

  const char *group = sqlite3_uri_parameter(zURIParameterKey, "group");
  if (!group) {
    NS_WARNING("SQLite URI had 'persistenceType' but not 'group'?!");
    return nullptr;
  }

  const char *origin = sqlite3_uri_parameter(zURIParameterKey, "origin");
  if (!origin) {
    NS_WARNING("SQLite URI had 'persistenceType' and 'group' but not "
               "'origin'?!");
    return nullptr;
  }

  QuotaManager *quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  return quotaManager->GetQuotaObject(
    PersistenceTypeFromText(nsDependentCString(persistenceType)),
    nsDependentCString(group),
    nsDependentCString(origin),
    NS_ConvertUTF8toUTF16(zName));
}

void
MaybeEstablishQuotaControl(const char *zName,
                           telemetry_file *pFile,
                           int flags)
{
  MOZ_ASSERT(pFile);
  MOZ_ASSERT(!pFile->quotaObject);

  if (!(flags & (SQLITE_OPEN_URI | SQLITE_OPEN_WAL))) {
    return;
  }

  MOZ_ASSERT(zName);

  const char *zURIParameterKey = (flags & SQLITE_OPEN_WAL) ?
                                 DatabasePathFromWALPath(zName) :
                                 zName;

  MOZ_ASSERT(zURIParameterKey);

  pFile->quotaObject =
    GetQuotaObjectFromNameAndParameters(zName, zURIParameterKey);
}

/*
** Close a telemetry_file.
*/
int
xClose(sqlite3_file *pFile)
{
  telemetry_file *p = (telemetry_file *)pFile;
  int rc;
  { // Scope for IOThreadAutoTimer
    IOThreadAutoTimer ioTimer(IOInterposeObserver::OpClose);
    rc = p->pReal->pMethods->xClose(p->pReal);
  }
  if( rc==SQLITE_OK ){
    delete p->base.pMethods;
    p->base.pMethods = nullptr;
    p->quotaObject = nullptr;
#ifdef DEBUG
    p->fileChunkSize = 0;
#endif
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
  IOThreadAutoTimer ioTimer(p->histograms->readMS, IOInterposeObserver::OpRead);
  int rc;
  rc = p->pReal->pMethods->xRead(p->pReal, zBuf, iAmt, iOfst);
  // sqlite likes to read from empty files, this is normal, ignore it.
  if (rc != SQLITE_IOERR_SHORT_READ)
    Telemetry::Accumulate(p->histograms->readB, rc == SQLITE_OK ? iAmt : 0);
  return rc;
}

/*
** Return the current file-size of a telemetry_file.
*/
int
xFileSize(sqlite3_file *pFile, sqlite_int64 *pSize)
{
  IOThreadAutoTimer ioTimer(IOInterposeObserver::OpStat);
  telemetry_file *p = (telemetry_file *)pFile;
  int rc;
  rc = p->pReal->pMethods->xFileSize(p->pReal, pSize);
  return rc;
}

/*
** Write data to a telemetry_file.
*/
int
xWrite(sqlite3_file *pFile, const void *zBuf, int iAmt, sqlite_int64 iOfst)
{
  telemetry_file *p = (telemetry_file *)pFile;
  IOThreadAutoTimer ioTimer(p->histograms->writeMS, IOInterposeObserver::OpWrite);
  int rc;
  if (p->quotaObject) {
    MOZ_ASSERT(INT64_MAX - iOfst >= iAmt);
    if (!p->quotaObject->MaybeUpdateSize(iOfst + iAmt, /* aTruncate */ false)) {
      return SQLITE_FULL;
    }
  }
  rc = p->pReal->pMethods->xWrite(p->pReal, zBuf, iAmt, iOfst);
  Telemetry::Accumulate(p->histograms->writeB, rc == SQLITE_OK ? iAmt : 0);
  if (p->quotaObject && rc != SQLITE_OK) {
    NS_WARNING("xWrite failed on a quota-controlled file, attempting to "
               "update its current size...");
    sqlite_int64 currentSize;
    if (xFileSize(pFile, &currentSize) == SQLITE_OK) {
      p->quotaObject->MaybeUpdateSize(currentSize, /* aTruncate */ true);
    }
  }
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
      NS_WARNING("xTruncate failed on a quota-controlled file, attempting to "
                 "update its current size...");
      if (xFileSize(pFile, &size) == SQLITE_OK) {
        p->quotaObject->MaybeUpdateSize(size, /* aTruncate */ true);
      }
    }
  }
  return rc;
}

/*
** Sync a telemetry_file.
*/
int
xSync(sqlite3_file *pFile, int flags)
{
  telemetry_file *p = (telemetry_file *)pFile;
  IOThreadAutoTimer ioTimer(p->histograms->syncMS, IOInterposeObserver::OpFSync);
  return p->pReal->pMethods->xSync(p->pReal, flags);
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
      hintSize =
        ((hintSize + p->fileChunkSize - 1) / p->fileChunkSize) *
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
xFetch(sqlite3_file *pFile, sqlite3_int64 iOff, int iAmt, void **pp)
{
  telemetry_file *p = (telemetry_file *)pFile;
  MOZ_ASSERT(p->pReal->pMethods->iVersion >= 3);
  return p->pReal->pMethods->xFetch(p->pReal, iOff, iAmt, pp);
}

int
xUnfetch(sqlite3_file *pFile, sqlite3_int64 iOff, void *pResOut)
{
  telemetry_file *p = (telemetry_file *)pFile;
  MOZ_ASSERT(p->pReal->pMethods->iVersion >= 3);
  return p->pReal->pMethods->xUnfetch(p->pReal, iOff, pResOut);
}

int
xOpen(sqlite3_vfs* vfs, const char *zName, sqlite3_file* pFile,
          int flags, int *pOutFlags)
{
  IOThreadAutoTimer ioTimer(Telemetry::MOZ_SQLITE_OPEN_MS,
                            IOInterposeObserver::OpCreateOrOpen);
  Telemetry::AutoTimer<Telemetry::MOZ_SQLITE_OPEN_MS> timer;
  sqlite3_vfs *orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  int rc;
  telemetry_file *p = (telemetry_file *)pFile;
  Histograms *h = nullptr;
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

  MaybeEstablishQuotaControl(zName, p, flags);

  rc = orig_vfs->xOpen(orig_vfs, zName, p->pReal, flags, pOutFlags);
  if( rc != SQLITE_OK )
    return rc;
  if( p->pReal->pMethods ){
    sqlite3_io_methods *pNew = new sqlite3_io_methods;
    const sqlite3_io_methods *pSub = p->pReal->pMethods;
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

int
xDelete(sqlite3_vfs* vfs, const char *zName, int syncDir)
{
  sqlite3_vfs *orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  int rc;
  RefPtr<QuotaObject> quotaObject;

  if (StringEndsWith(nsDependentCString(zName), NS_LITERAL_CSTRING("-wal"))) {
    const char *zURIParameterKey = DatabasePathFromWALPath(zName);
    MOZ_ASSERT(zURIParameterKey);

    quotaObject = GetQuotaObjectFromNameAndParameters(zName, zURIParameterKey);
  }

  rc = orig_vfs->xDelete(orig_vfs, zName, syncDir);
  if (rc == SQLITE_OK && quotaObject) {
    MOZ_ALWAYS_TRUE(quotaObject->MaybeUpdateSize(0, /* aTruncate */ true));
  }

  return rc;
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

} // namespace

namespace mozilla {
namespace storage {

sqlite3_vfs* ConstructTelemetryVFS()
{
#if defined(XP_WIN)
#define EXPECTED_VFS     "win32"
#define EXPECTED_VFS_NFS "win32"
#else
#define EXPECTED_VFS     "unix"
#define EXPECTED_VFS_NFS "unix-excl"
#endif
  
  bool expected_vfs;
  sqlite3_vfs *vfs;
  if (Preferences::GetBool(PREF_NFS_FILESYSTEM)) {
    vfs = sqlite3_vfs_find(EXPECTED_VFS_NFS);
    expected_vfs = (vfs != nullptr);
  }
  else {
    vfs = sqlite3_vfs_find(nullptr);
    expected_vfs = vfs->zName && !strcmp(vfs->zName, EXPECTED_VFS);
  }
  if (!expected_vfs) {
    return nullptr;
  }

  sqlite3_vfs *tvfs = new ::sqlite3_vfs;
  memset(tvfs, 0, sizeof(::sqlite3_vfs));
  // If the VFS version is higher than the last known one, you should update
  // this VFS adding appropriate methods for any methods added in the version
  // change.
  tvfs->iVersion = vfs->iVersion;
  MOZ_ASSERT(vfs->iVersion <= LAST_KNOWN_VFS_VERSION);
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

already_AddRefed<QuotaObject>
GetQuotaObjectForFile(sqlite3_file *pFile)
{
  MOZ_ASSERT(pFile);

  telemetry_file *p = (telemetry_file *)pFile;
  RefPtr<QuotaObject> result = p->quotaObject;
  return result.forget();
}

} // namespace storage
} // namespace mozilla
