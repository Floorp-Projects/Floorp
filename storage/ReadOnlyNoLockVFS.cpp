/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This VFS is built on top of the (unix|win32)-none, but it additionally
 * sets any opened file as immutable, that allows to also open in read-only
 * mode databases using WAL, or other journals that need auxiliary files, when
 * such files cannot be created.
 * This is useful when trying to read from third-party databases, avoiding any
 * risk of creating auxiliary files (e.g. journals).
 * It can only be used on read-only connections, because being a no-lock VFS
 * it would be trivial to corrupt the data.
 */

#include "nsDebug.h"
#include "sqlite3.h"

#define ORIGVFS(p) ((sqlite3_vfs*)((p)->pAppData))

#if defined(XP_WIN)
#  define BASE_VFS "win32-none"
#else
#  define BASE_VFS "unix-none"
#endif

#define VFS_NAME "readonly-immutable-nolock"

namespace {

static int vfsOpen(sqlite3_vfs* vfs, const char* zName, sqlite3_file* pFile,
                   int flags, int* pOutFlags) {
  if ((flags & SQLITE_OPEN_READONLY) == 0) {
    // This is not done to be used in readwrite connections.
    return SQLITE_CANTOPEN;
  }

  sqlite3_vfs* pOrigVfs = ORIGVFS(vfs);
  int rc = pOrigVfs->xOpen(pOrigVfs, zName, pFile, flags, pOutFlags);
  if (rc != SQLITE_OK) {
    return rc;
  }

  const sqlite3_io_methods* pOrigMethods = pFile->pMethods;

  // If the IO version is higher than the last known one, you should update
  // this IO adding appropriate methods for any methods added in the version
  // change.
  MOZ_ASSERT(pOrigMethods->iVersion <= 3);

  static const sqlite3_io_methods vfs_io_methods = {
      pOrigMethods->iVersion,           /* iVersion */
      pOrigMethods->xClose,             /* xClose */
      pOrigMethods->xRead,              /* xRead */
      pOrigMethods->xWrite,             /* xWrite */
      pOrigMethods->xTruncate,          /* xTruncate */
      pOrigMethods->xSync,              /* xSync */
      pOrigMethods->xFileSize,          /* xFileSize */
      pOrigMethods->xLock,              /* xLock */
      pOrigMethods->xUnlock,            /* xUnlock */
      pOrigMethods->xCheckReservedLock, /* xCheckReservedLock */
      pOrigMethods->xFileControl,       /* xFileControl */
      pOrigMethods->xSectorSize,        /* xSectorSize */
      [](sqlite3_file*) {
        return SQLITE_IOCAP_IMMUTABLE;
      },                         /* xDeviceCharacteristics */
      pOrigMethods->xShmMap,     /* xShmMap */
      pOrigMethods->xShmLock,    /* xShmLock */
      pOrigMethods->xShmBarrier, /* xShmBarrier */
      pOrigMethods->xShmUnmap,   /* xShmUnmap */
      pOrigMethods->xFetch,      /* xFetch */
      pOrigMethods->xUnfetch     /* xUnfetch */
  };
  pFile->pMethods = &vfs_io_methods;
  if (pOutFlags) {
    *pOutFlags = flags;
  }

  return SQLITE_OK;
}

}  // namespace

namespace mozilla::storage {

UniquePtr<sqlite3_vfs> ConstructReadOnlyNoLockVFS() {
  if (sqlite3_vfs_find(VFS_NAME) != nullptr) {
    return nullptr;
  }
  sqlite3_vfs* pOrigVfs = sqlite3_vfs_find(BASE_VFS);
  if (!pOrigVfs) {
    return nullptr;
  }

  // If the VFS version is higher than the last known one, you should update
  // this VFS adding appropriate methods for any methods added in the version
  // change.
  MOZ_ASSERT(pOrigVfs->iVersion <= 3);

  static const sqlite3_vfs vfs = {
      pOrigVfs->iVersion,          /* iVersion  */
      pOrigVfs->szOsFile,          /* szOsFile */
      pOrigVfs->mxPathname,        /* mxPathname */
      nullptr,                     /* pNext */
      VFS_NAME,                    /* zName */
      pOrigVfs,                    /* pAppData */
      vfsOpen,                     /* xOpen */
      pOrigVfs->xDelete,           /* xDelete */
      pOrigVfs->xAccess,           /* xAccess */
      pOrigVfs->xFullPathname,     /* xFullPathname */
      pOrigVfs->xDlOpen,           /* xDlOpen */
      pOrigVfs->xDlError,          /* xDlError */
      pOrigVfs->xDlSym,            /* xDlSym */
      pOrigVfs->xDlClose,          /* xDlClose */
      pOrigVfs->xRandomness,       /* xRandomness */
      pOrigVfs->xSleep,            /* xSleep */
      pOrigVfs->xCurrentTime,      /* xCurrentTime */
      pOrigVfs->xGetLastError,     /* xGetLastError */
      pOrigVfs->xCurrentTimeInt64, /* xCurrentTimeInt64 */
      pOrigVfs->xSetSystemCall,    /* xSetSystemCall */
      pOrigVfs->xGetSystemCall,    /* xGetSystemCall */
      pOrigVfs->xNextSystemCall    /* xNextSystemCall */
  };

  return MakeUnique<sqlite3_vfs>(vfs);
}

}  // namespace mozilla::storage
