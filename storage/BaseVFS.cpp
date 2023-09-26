/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BaseVFS.h"

#include <string.h>
#include "sqlite3.h"
#include "mozilla/net/IOActivityMonitor.h"

namespace {

// The last VFS version for which this file has been updated.
constexpr int kLastKnowVfsVersion = 3;

// The last io_methods version for which this file has been updated.
constexpr int kLastKnownIOMethodsVersion = 3;

using namespace mozilla;
using namespace mozilla::net;

struct BaseFile {
  // Base class.  Must be first
  sqlite3_file base;
  // The filename
  char* location;
  // This points to the underlying sqlite3_file
  sqlite3_file pReal[1];
};

int BaseClose(sqlite3_file* pFile) {
  BaseFile* p = (BaseFile*)pFile;
  delete[] p->location;
  return p->pReal->pMethods->xClose(p->pReal);
}

int BaseRead(sqlite3_file* pFile, void* zBuf, int iAmt, sqlite_int64 iOfst) {
  BaseFile* p = (BaseFile*)pFile;
  int rc = p->pReal->pMethods->xRead(p->pReal, zBuf, iAmt, iOfst);
  if (rc == SQLITE_OK && IOActivityMonitor::IsActive()) {
    IOActivityMonitor::Read(nsDependentCString(p->location), iAmt);
  }
  return rc;
}

int BaseWrite(sqlite3_file* pFile, const void* zBuf, int iAmt,
              sqlite_int64 iOfst) {
  BaseFile* p = (BaseFile*)pFile;
  int rc = p->pReal->pMethods->xWrite(p->pReal, zBuf, iAmt, iOfst);
  if (rc == SQLITE_OK && IOActivityMonitor::IsActive()) {
    IOActivityMonitor::Write(nsDependentCString(p->location), iAmt);
  }
  return rc;
}

int BaseTruncate(sqlite3_file* pFile, sqlite_int64 size) {
  BaseFile* p = (BaseFile*)pFile;
  return p->pReal->pMethods->xTruncate(p->pReal, size);
}

int BaseSync(sqlite3_file* pFile, int flags) {
  BaseFile* p = (BaseFile*)pFile;
  return p->pReal->pMethods->xSync(p->pReal, flags);
}

int BaseFileSize(sqlite3_file* pFile, sqlite_int64* pSize) {
  BaseFile* p = (BaseFile*)pFile;
  return p->pReal->pMethods->xFileSize(p->pReal, pSize);
}

int BaseLock(sqlite3_file* pFile, int eLock) {
  BaseFile* p = (BaseFile*)pFile;
  return p->pReal->pMethods->xLock(p->pReal, eLock);
}

int BaseUnlock(sqlite3_file* pFile, int eLock) {
  BaseFile* p = (BaseFile*)pFile;
  return p->pReal->pMethods->xUnlock(p->pReal, eLock);
}

int BaseCheckReservedLock(sqlite3_file* pFile, int* pResOut) {
  BaseFile* p = (BaseFile*)pFile;
  return p->pReal->pMethods->xCheckReservedLock(p->pReal, pResOut);
}

int BaseFileControl(sqlite3_file* pFile, int op, void* pArg) {
#if defined(MOZ_SQLITE_PERSIST_AUXILIARY_FILES)
  // Persist auxiliary files (-shm and -wal) on disk, because creating and
  // deleting them may be expensive on slow storage.
  // Only do this when there is a journal size limit, so the journal is
  // truncated instead of deleted on shutdown, that feels safer if the user
  // moves a database file around without its auxiliary files.
  MOZ_ASSERT(
      ::sqlite3_compileoption_used("DEFAULT_JOURNAL_SIZE_LIMIT"),
      "A journal size limit ensures the journal is truncated on shutdown");
  if (op == SQLITE_FCNTL_PERSIST_WAL) {
    *static_cast<int*>(pArg) = 1;
    return SQLITE_OK;
  }
#endif
  BaseFile* p = (BaseFile*)pFile;
  return p->pReal->pMethods->xFileControl(p->pReal, op, pArg);
}

int BaseSectorSize(sqlite3_file* pFile) {
  BaseFile* p = (BaseFile*)pFile;
  return p->pReal->pMethods->xSectorSize(p->pReal);
}

int BaseDeviceCharacteristics(sqlite3_file* pFile) {
  BaseFile* p = (BaseFile*)pFile;
  return p->pReal->pMethods->xDeviceCharacteristics(p->pReal);
}

int BaseShmMap(sqlite3_file* pFile, int iPg, int pgsz, int bExtend,
               void volatile** pp) {
  BaseFile* p = (BaseFile*)pFile;
  return p->pReal->pMethods->xShmMap(p->pReal, iPg, pgsz, bExtend, pp);
}

int BaseShmLock(sqlite3_file* pFile, int offset, int n, int flags) {
  BaseFile* p = (BaseFile*)pFile;
  return p->pReal->pMethods->xShmLock(p->pReal, offset, n, flags);
}

void BaseShmBarrier(sqlite3_file* pFile) {
  BaseFile* p = (BaseFile*)pFile;
  return p->pReal->pMethods->xShmBarrier(p->pReal);
}

int BaseShmUnmap(sqlite3_file* pFile, int deleteFlag) {
  BaseFile* p = (BaseFile*)pFile;
  return p->pReal->pMethods->xShmUnmap(p->pReal, deleteFlag);
}

int BaseFetch(sqlite3_file* pFile, sqlite3_int64 iOfst, int iAmt, void** pp) {
  BaseFile* p = (BaseFile*)pFile;
  return p->pReal->pMethods->xFetch(p->pReal, iOfst, iAmt, pp);
}

int BaseUnfetch(sqlite3_file* pFile, sqlite3_int64 iOfst, void* pPage) {
  BaseFile* p = (BaseFile*)pFile;
  return p->pReal->pMethods->xUnfetch(p->pReal, iOfst, pPage);
}

int BaseOpen(sqlite3_vfs* vfs, const char* zName, sqlite3_file* pFile,
             int flags, int* pOutFlags) {
  BaseFile* p = (BaseFile*)pFile;
  if (zName) {
    p->location = new char[7 + strlen(zName) + 1];
    strcpy(p->location, "file://");
    strcpy(p->location + 7, zName);
  } else {
    p->location = new char[8];
    strcpy(p->location, "file://");
  }

  sqlite3_vfs* origVfs = (sqlite3_vfs*)(vfs->pAppData);
  int rc = origVfs->xOpen(origVfs, zName, p->pReal, flags, pOutFlags);
  if (rc) {
    return rc;
  }
  if (p->pReal->pMethods) {
    // If the io_methods version is higher than the last known one, you should
    // update this VFS adding appropriate IO methods for any methods added in
    // the version change.
    MOZ_ASSERT(p->pReal->pMethods->iVersion == kLastKnownIOMethodsVersion);
    static const sqlite3_io_methods IOmethods = {
        kLastKnownIOMethodsVersion, /* iVersion */
        BaseClose,                  /* xClose */
        BaseRead,                   /* xRead */
        BaseWrite,                  /* xWrite */
        BaseTruncate,               /* xTruncate */
        BaseSync,                   /* xSync */
        BaseFileSize,               /* xFileSize */
        BaseLock,                   /* xLock */
        BaseUnlock,                 /* xUnlock */
        BaseCheckReservedLock,      /* xCheckReservedLock */
        BaseFileControl,            /* xFileControl */
        BaseSectorSize,             /* xSectorSize */
        BaseDeviceCharacteristics,  /* xDeviceCharacteristics */
        BaseShmMap,                 /* xShmMap */
        BaseShmLock,                /* xShmLock */
        BaseShmBarrier,             /* xShmBarrier */
        BaseShmUnmap,               /* xShmUnmap */
        BaseFetch,                  /* xFetch */
        BaseUnfetch                 /* xUnfetch */
    };
    pFile->pMethods = &IOmethods;
  }

  return SQLITE_OK;
}

}  // namespace

namespace mozilla::storage::basevfs {

const char* GetVFSName(bool exclusive) {
  return exclusive ? "base-vfs-excl" : "base-vfs";
}

UniquePtr<sqlite3_vfs> ConstructVFS(bool exclusive) {
#if defined(XP_WIN)
#  define EXPECTED_VFS "win32"
#  define EXPECTED_VFS_EXCL "win32"
#else
#  define EXPECTED_VFS "unix"
#  define EXPECTED_VFS_EXCL "unix-excl"
#endif

  if (sqlite3_vfs_find(GetVFSName(exclusive))) {
    return nullptr;
  }

  bool found;
  sqlite3_vfs* origVfs;
  if (!exclusive) {
    // Use the non-exclusive VFS.
    origVfs = sqlite3_vfs_find(nullptr);
    found = origVfs && origVfs->zName && !strcmp(origVfs->zName, EXPECTED_VFS);
  } else {
    origVfs = sqlite3_vfs_find(EXPECTED_VFS_EXCL);
    found = (origVfs != nullptr);
  }
  if (!found) {
    return nullptr;
  }

  // If the VFS version is higher than the last known one, you should update
  // this VFS adding appropriate methods for any methods added in the version
  // change.
  MOZ_ASSERT(origVfs->iVersion == kLastKnowVfsVersion);

  sqlite3_vfs vfs = {
      kLastKnowVfsVersion,                                    /* iVersion  */
      origVfs->szOsFile + static_cast<int>(sizeof(BaseFile)), /* szOsFile */
      origVfs->mxPathname,                                    /* mxPathname */
      nullptr,                                                /* pNext */
      GetVFSName(exclusive),                                  /* zName */
      origVfs,                                                /* pAppData */
      BaseOpen,                                               /* xOpen */
      origVfs->xDelete,                                       /* xDelete */
      origVfs->xAccess,                                       /* xAccess */
      origVfs->xFullPathname,     /* xFullPathname */
      origVfs->xDlOpen,           /* xDlOpen */
      origVfs->xDlError,          /* xDlError */
      origVfs->xDlSym,            /* xDlSym */
      origVfs->xDlClose,          /* xDlClose */
      origVfs->xRandomness,       /* xRandomness */
      origVfs->xSleep,            /* xSleep */
      origVfs->xCurrentTime,      /* xCurrentTime */
      origVfs->xGetLastError,     /* xGetLastError */
      origVfs->xCurrentTimeInt64, /* xCurrentTimeInt64 */
      origVfs->xSetSystemCall,    /* xSetSystemCall */
      origVfs->xGetSystemCall,    /* xGetSystemCall */
      origVfs->xNextSystemCall    /* xNextSystemCall */
  };

  return MakeUnique<sqlite3_vfs>(vfs);
}

}  // namespace mozilla::storage::basevfs
