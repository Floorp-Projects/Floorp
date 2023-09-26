/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "QuotaVFS.h"

#include "mozilla/dom/quota/PersistenceType.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/QuotaObject.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "mozilla/StaticPrefs_storage.h"
#include "nsDirectoryServiceDefs.h"
#include "nsEscape.h"
#include "sqlite3.h"

#if defined(XP_WIN) || defined(XP_UNIX)
#  include "mozilla/StaticPrefs_dom.h"
#endif

// The last VFS version for which this file has been updated.
#define LAST_KNOWN_VFS_VERSION 3

// The last io_methods version for which this file has been updated.
#define LAST_KNOWN_IOMETHODS_VERSION 3

namespace {

using namespace mozilla;
using namespace mozilla::dom::quota;

struct QuotaFile {
  // Base class.  Must be first
  sqlite3_file base;

  // quota object for this file
  RefPtr<QuotaObject> quotaObject;

  // The chunk size for this file. See the documentation for
  // sqlite3_file_control() and FCNTL_CHUNK_SIZE.
  int fileChunkSize;

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

void MaybeEstablishQuotaControl(const char* zName, QuotaFile* pFile,
                                int flags) {
  MOZ_ASSERT(pFile);
  MOZ_ASSERT(!pFile->quotaObject);

  if (!(flags & (SQLITE_OPEN_URI | SQLITE_OPEN_WAL))) {
    return;
  }
  pFile->quotaObject = GetQuotaObjectFromName(zName);
}

/*
** Close a QuotaFile.
*/
int QuotaClose(sqlite3_file* pFile) {
  QuotaFile* p = (QuotaFile*)pFile;
  int rc;
  rc = p->pReal->pMethods->xClose(p->pReal);
  if (rc == SQLITE_OK) {
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
** Read data from a QuotaFile.
*/
int QuotaRead(sqlite3_file* pFile, void* zBuf, int iAmt, sqlite_int64 iOfst) {
  QuotaFile* p = (QuotaFile*)pFile;
  int rc;
  rc = p->pReal->pMethods->xRead(p->pReal, zBuf, iAmt, iOfst);
  return rc;
}

/*
** Return the current file-size of a QuotaFile.
*/
int QuotaFileSize(sqlite3_file* pFile, sqlite_int64* pSize) {
  QuotaFile* p = (QuotaFile*)pFile;
  int rc;
  rc = p->pReal->pMethods->xFileSize(p->pReal, pSize);
  return rc;
}

/*
** Write data to a QuotaFile.
*/
int QuotaWrite(sqlite3_file* pFile, const void* zBuf, int iAmt,
               sqlite_int64 iOfst) {
  QuotaFile* p = (QuotaFile*)pFile;
  int rc;
  if (p->quotaObject) {
    MOZ_ASSERT(INT64_MAX - iOfst >= iAmt);
    if (!p->quotaObject->MaybeUpdateSize(iOfst + iAmt, /* aTruncate */ false)) {
      return SQLITE_FULL;
    }
  }
  rc = p->pReal->pMethods->xWrite(p->pReal, zBuf, iAmt, iOfst);
  if (p->quotaObject && rc != SQLITE_OK) {
    NS_WARNING(
        "xWrite failed on a quota-controlled file, attempting to "
        "update its current size...");
    sqlite_int64 currentSize;
    if (QuotaFileSize(pFile, &currentSize) == SQLITE_OK) {
      DebugOnly<bool> res =
          p->quotaObject->MaybeUpdateSize(currentSize, /* aTruncate */ true);
      MOZ_ASSERT(res);
    }
  }
  return rc;
}

/*
** Truncate a QuotaFile.
*/
int QuotaTruncate(sqlite3_file* pFile, sqlite_int64 size) {
  QuotaFile* p = (QuotaFile*)pFile;
  int rc;
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
      MOZ_ASSERT(QuotaFileSize(pFile, &newSize) == SQLITE_OK);
      MOZ_ASSERT(newSize == size);
#endif
    } else {
      NS_WARNING(
          "xTruncate failed on a quota-controlled file, attempting to "
          "update its current size...");
      if (QuotaFileSize(pFile, &size) == SQLITE_OK) {
        DebugOnly<bool> res =
            p->quotaObject->MaybeUpdateSize(size, /* aTruncate */ true);
        MOZ_ASSERT(res);
      }
    }
  }
  return rc;
}

/*
** Sync a QuotaFile.
*/
int QuotaSync(sqlite3_file* pFile, int flags) {
  QuotaFile* p = (QuotaFile*)pFile;
  return p->pReal->pMethods->xSync(p->pReal, flags);
}

/*
** Lock a QuotaFile.
*/
int QuotaLock(sqlite3_file* pFile, int eLock) {
  QuotaFile* p = (QuotaFile*)pFile;
  int rc;
  rc = p->pReal->pMethods->xLock(p->pReal, eLock);
  return rc;
}

/*
** Unlock a QuotaFile.
*/
int QuotaUnlock(sqlite3_file* pFile, int eLock) {
  QuotaFile* p = (QuotaFile*)pFile;
  int rc;
  rc = p->pReal->pMethods->xUnlock(p->pReal, eLock);
  return rc;
}

/*
** Check if another file-handle holds a RESERVED lock on a QuotaFile.
*/
int QuotaCheckReservedLock(sqlite3_file* pFile, int* pResOut) {
  QuotaFile* p = (QuotaFile*)pFile;
  int rc = p->pReal->pMethods->xCheckReservedLock(p->pReal, pResOut);
  return rc;
}

/*
** File control method. For custom operations on a QuotaFile.
*/
int QuotaFileControl(sqlite3_file* pFile, int op, void* pArg) {
  QuotaFile* p = (QuotaFile*)pFile;
  int rc;
  // Hook SQLITE_FCNTL_SIZE_HINT for quota-controlled files and do the necessary
  // work before passing to the SQLite VFS.
  if (op == SQLITE_FCNTL_SIZE_HINT && p->quotaObject) {
    sqlite3_int64 hintSize = *static_cast<sqlite3_int64*>(pArg);
    sqlite3_int64 currentSize;
    rc = QuotaFileSize(pFile, &currentSize);
    if (rc != SQLITE_OK) {
      return rc;
    }
    if (hintSize > currentSize) {
      rc = QuotaTruncate(pFile, hintSize);
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
    MOZ_ASSERT(QuotaFileSize(pFile, &currentSize) == SQLITE_OK);
    MOZ_ASSERT(currentSize >= hintSize);
  }
#endif
  return rc;
}

/*
** Return the sector-size in bytes for a QuotaFile.
*/
int QuotaSectorSize(sqlite3_file* pFile) {
  QuotaFile* p = (QuotaFile*)pFile;
  int rc;
  rc = p->pReal->pMethods->xSectorSize(p->pReal);
  return rc;
}

/*
** Return the device characteristic flags supported by a QuotaFile.
*/
int QuotaDeviceCharacteristics(sqlite3_file* pFile) {
  QuotaFile* p = (QuotaFile*)pFile;
  int rc;
  rc = p->pReal->pMethods->xDeviceCharacteristics(p->pReal);
  return rc;
}

/*
** Shared-memory operations.
*/
int QuotaShmLock(sqlite3_file* pFile, int ofst, int n, int flags) {
  QuotaFile* p = (QuotaFile*)pFile;
  return p->pReal->pMethods->xShmLock(p->pReal, ofst, n, flags);
}

int QuotaShmMap(sqlite3_file* pFile, int iRegion, int szRegion, int isWrite,
                void volatile** pp) {
  QuotaFile* p = (QuotaFile*)pFile;
  int rc;
  rc = p->pReal->pMethods->xShmMap(p->pReal, iRegion, szRegion, isWrite, pp);
  return rc;
}

void QuotaShmBarrier(sqlite3_file* pFile) {
  QuotaFile* p = (QuotaFile*)pFile;
  p->pReal->pMethods->xShmBarrier(p->pReal);
}

int QuotaShmUnmap(sqlite3_file* pFile, int delFlag) {
  QuotaFile* p = (QuotaFile*)pFile;
  int rc;
  rc = p->pReal->pMethods->xShmUnmap(p->pReal, delFlag);
  return rc;
}

int QuotaFetch(sqlite3_file* pFile, sqlite3_int64 iOff, int iAmt, void** pp) {
  QuotaFile* p = (QuotaFile*)pFile;
  MOZ_ASSERT(p->pReal->pMethods->iVersion >= 3);
  return p->pReal->pMethods->xFetch(p->pReal, iOff, iAmt, pp);
}

int QuotaUnfetch(sqlite3_file* pFile, sqlite3_int64 iOff, void* pResOut) {
  QuotaFile* p = (QuotaFile*)pFile;
  MOZ_ASSERT(p->pReal->pMethods->iVersion >= 3);
  return p->pReal->pMethods->xUnfetch(p->pReal, iOff, pResOut);
}

int QuotaOpen(sqlite3_vfs* vfs, const char* zName, sqlite3_file* pFile,
              int flags, int* pOutFlags) {
  sqlite3_vfs* orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  int rc;
  QuotaFile* p = (QuotaFile*)pFile;

  MaybeEstablishQuotaControl(zName, p, flags);

  rc = orig_vfs->xOpen(orig_vfs, zName, p->pReal, flags, pOutFlags);
  if (rc != SQLITE_OK) return rc;
  if (p->pReal->pMethods) {
    sqlite3_io_methods* pNew = new sqlite3_io_methods;
    const sqlite3_io_methods* pSub = p->pReal->pMethods;
    memset(pNew, 0, sizeof(*pNew));
    // If the io_methods version is higher than the last known one, you should
    // update this VFS adding appropriate IO methods for any methods added in
    // the version change.
    pNew->iVersion = pSub->iVersion;
    MOZ_ASSERT(pNew->iVersion <= LAST_KNOWN_IOMETHODS_VERSION);
    pNew->xClose = QuotaClose;
    pNew->xRead = QuotaRead;
    pNew->xWrite = QuotaWrite;
    pNew->xTruncate = QuotaTruncate;
    pNew->xSync = QuotaSync;
    pNew->xFileSize = QuotaFileSize;
    pNew->xLock = QuotaLock;
    pNew->xUnlock = QuotaUnlock;
    pNew->xCheckReservedLock = QuotaCheckReservedLock;
    pNew->xFileControl = QuotaFileControl;
    pNew->xSectorSize = QuotaSectorSize;
    pNew->xDeviceCharacteristics = QuotaDeviceCharacteristics;
    if (pNew->iVersion >= 2) {
      // Methods added in version 2.
      pNew->xShmMap = pSub->xShmMap ? QuotaShmMap : nullptr;
      pNew->xShmLock = pSub->xShmLock ? QuotaShmLock : nullptr;
      pNew->xShmBarrier = pSub->xShmBarrier ? QuotaShmBarrier : nullptr;
      pNew->xShmUnmap = pSub->xShmUnmap ? QuotaShmUnmap : nullptr;
    }
    if (pNew->iVersion >= 3) {
      // Methods added in version 3.
      // SQLite 3.7.17 calls these methods without checking for nullptr first,
      // so we always define them.  Verify that we're not going to call
      // nullptrs, though.
      MOZ_ASSERT(pSub->xFetch);
      pNew->xFetch = QuotaFetch;
      MOZ_ASSERT(pSub->xUnfetch);
      pNew->xUnfetch = QuotaUnfetch;
    }
    pFile->pMethods = pNew;
  }
  return rc;
}

int QuotaDelete(sqlite3_vfs* vfs, const char* zName, int syncDir) {
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

int QuotaAccess(sqlite3_vfs* vfs, const char* zName, int flags, int* pResOut) {
  sqlite3_vfs* orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  return orig_vfs->xAccess(orig_vfs, zName, flags, pResOut);
}

int QuotaFullPathname(sqlite3_vfs* vfs, const char* zName, int nOut,
                      char* zOut) {
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
    MOZ_ASSERT(static_cast<size_t>(nOut) > strlen(zName));

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
#elif defined(XP_UNIX)
  // SQLite canonicalizes (resolves path components) file paths on Unix which
  // doesn't work well with file path sanity checks in quota manager. This is
  // especially a problem on mac where /var is a symlink to /private/var.
  // Since QuotaVFS is used only by quota clients which never access databases
  // outside of PROFILE/storage, we override Unix xFullPathname with own
  // implementation that doesn't do any canonicalization.

  if (StaticPrefs::dom_quotaManager_overrideXFullPathnameUnix()) {
    if (nOut < 0) {
      // Match the return code used by SQLite's xFullPathname implementation
      // here and below.
      return SQLITE_CANTOPEN;
    }

    QM_TRY_INSPECT(
        const auto& path, ([&zName]() -> Result<nsString, nsresult> {
          NS_ConvertUTF8toUTF16 name(zName);

          if (name.First() == '/') {
            return std::move(name);
          }

          QM_TRY_INSPECT(const auto& file,
                         MOZ_TO_RESULT_INVOKE_TYPED(nsCOMPtr<nsIFile>,
                                                    NS_GetSpecialDirectory,
                                                    NS_OS_CURRENT_WORKING_DIR));

          QM_TRY(MOZ_TO_RESULT(file->Append(name)));

          QM_TRY_RETURN(
              MOZ_TO_RESULT_INVOKE_MEMBER_TYPED(nsString, file, GetPath));
        }()),
        SQLITE_CANTOPEN);

    QM_TRY_INSPECT(const auto& quotaFile, QM_NewLocalFile(path),
                   SQLITE_CANTOPEN);

    QM_TRY_INSPECT(
        const auto& quotaPath,
        MOZ_TO_RESULT_INVOKE_MEMBER_TYPED(nsString, quotaFile, GetPath),
        SQLITE_CANTOPEN);

    NS_ConvertUTF16toUTF8 sqlitePath(quotaPath);

    if (sqlitePath.Length() > (unsigned int)nOut) {
      return SQLITE_CANTOPEN;
    }

    nsCharTraits<char>::copy(zOut, sqlitePath.get(), sqlitePath.Length());
    zOut[sqlitePath.Length()] = '\0';

    return SQLITE_OK;
  }
#endif

  sqlite3_vfs* orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  return orig_vfs->xFullPathname(orig_vfs, zName, nOut, zOut);
}

void* QuotaDlOpen(sqlite3_vfs* vfs, const char* zFilename) {
  sqlite3_vfs* orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  return orig_vfs->xDlOpen(orig_vfs, zFilename);
}

void QuotaDlError(sqlite3_vfs* vfs, int nByte, char* zErrMsg) {
  sqlite3_vfs* orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  orig_vfs->xDlError(orig_vfs, nByte, zErrMsg);
}

void (*QuotaDlSym(sqlite3_vfs* vfs, void* pHdle, const char* zSym))(void) {
  sqlite3_vfs* orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  return orig_vfs->xDlSym(orig_vfs, pHdle, zSym);
}

void QuotaDlClose(sqlite3_vfs* vfs, void* pHandle) {
  sqlite3_vfs* orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  orig_vfs->xDlClose(orig_vfs, pHandle);
}

int QuotaRandomness(sqlite3_vfs* vfs, int nByte, char* zOut) {
  sqlite3_vfs* orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  return orig_vfs->xRandomness(orig_vfs, nByte, zOut);
}

int QuotaSleep(sqlite3_vfs* vfs, int microseconds) {
  sqlite3_vfs* orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  return orig_vfs->xSleep(orig_vfs, microseconds);
}

int QuotaCurrentTime(sqlite3_vfs* vfs, double* prNow) {
  sqlite3_vfs* orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  return orig_vfs->xCurrentTime(orig_vfs, prNow);
}

int QuotaGetLastError(sqlite3_vfs* vfs, int nBuf, char* zBuf) {
  sqlite3_vfs* orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  return orig_vfs->xGetLastError(orig_vfs, nBuf, zBuf);
}

int QuotaCurrentTimeInt64(sqlite3_vfs* vfs, sqlite3_int64* piNow) {
  sqlite3_vfs* orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  return orig_vfs->xCurrentTimeInt64(orig_vfs, piNow);
}

static int QuotaSetSystemCall(sqlite3_vfs* vfs, const char* zName,
                              sqlite3_syscall_ptr pFunc) {
  sqlite3_vfs* orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  return orig_vfs->xSetSystemCall(orig_vfs, zName, pFunc);
}

static sqlite3_syscall_ptr QuotaGetSystemCall(sqlite3_vfs* vfs,
                                              const char* zName) {
  sqlite3_vfs* orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  return orig_vfs->xGetSystemCall(orig_vfs, zName);
}

static const char* QuotaNextSystemCall(sqlite3_vfs* vfs, const char* zName) {
  sqlite3_vfs* orig_vfs = static_cast<sqlite3_vfs*>(vfs->pAppData);
  return orig_vfs->xNextSystemCall(orig_vfs, zName);
}

}  // namespace

namespace mozilla::storage::quotavfs {

const char* GetVFSName() { return "quotavfs"; }

UniquePtr<sqlite3_vfs> ConstructVFS(const char* aBaseVFSName) {
  MOZ_ASSERT(aBaseVFSName);

  if (sqlite3_vfs_find(GetVFSName()) != nullptr) {
    return nullptr;
  }
  sqlite3_vfs* vfs = sqlite3_vfs_find(aBaseVFSName);
  if (!vfs) {
    return nullptr;
  }

  auto qvfs = MakeUnique<sqlite3_vfs>();
  memset(qvfs.get(), 0, sizeof(::sqlite3_vfs));
  // If the VFS version is higher than the last known one, you should update
  // this VFS adding appropriate methods for any methods added in the version
  // change.
  qvfs->iVersion = vfs->iVersion;
  MOZ_ASSERT(vfs->iVersion <= LAST_KNOWN_VFS_VERSION);
  qvfs->szOsFile = static_cast<int>(sizeof(QuotaFile) - sizeof(sqlite3_file) +
                                    vfs->szOsFile);
  qvfs->mxPathname = vfs->mxPathname;
  qvfs->zName = GetVFSName();
  qvfs->pAppData = vfs;
  qvfs->xOpen = QuotaOpen;
  qvfs->xDelete = QuotaDelete;
  qvfs->xAccess = QuotaAccess;
  qvfs->xFullPathname = QuotaFullPathname;
  qvfs->xDlOpen = QuotaDlOpen;
  qvfs->xDlError = QuotaDlError;
  qvfs->xDlSym = QuotaDlSym;
  qvfs->xDlClose = QuotaDlClose;
  qvfs->xRandomness = QuotaRandomness;
  qvfs->xSleep = QuotaSleep;
  qvfs->xCurrentTime = QuotaCurrentTime;
  qvfs->xGetLastError = QuotaGetLastError;
  if (qvfs->iVersion >= 2) {
    // Methods added in version 2.
    qvfs->xCurrentTimeInt64 = QuotaCurrentTimeInt64;
  }
  if (qvfs->iVersion >= 3) {
    // Methods added in version 3.
    qvfs->xSetSystemCall = QuotaSetSystemCall;
    qvfs->xGetSystemCall = QuotaGetSystemCall;
    qvfs->xNextSystemCall = QuotaNextSystemCall;
  }
  return qvfs;
}

already_AddRefed<QuotaObject> GetQuotaObjectForFile(sqlite3_file* pFile) {
  MOZ_ASSERT(pFile);

  QuotaFile* p = (QuotaFile*)pFile;
  RefPtr<QuotaObject> result = p->quotaObject;
  return result.forget();
}

}  // namespace mozilla::storage::quotavfs
