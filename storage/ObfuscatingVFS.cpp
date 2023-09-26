/*
** 2020-04-20
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
******************************************************************************
**
** This file implements a VFS shim that obfuscates database content
** written to disk by applying a CipherStrategy.
**
** COMPILING
**
** This extension requires SQLite 3.32.0 or later.
**
**
** LOADING
**
** Initialize it using a single API call as follows:
**
**     sqlite3_obfsvfs_init();
**
** Obfsvfs is a VFS Shim. When loaded, "obfsvfs" becomes the new
** default VFS and it uses the prior default VFS as the next VFS
** down in the stack.  This is normally what you want.  However, it
** complex situations where multiple VFS shims are being loaded,
** it might be important to ensure that obfsvfs is loaded in the
** correct order so that it sequences itself into the default VFS
** Shim stack in the right order.
**
** USING
**
** Open database connections using the sqlite3_open_v2() with
** the SQLITE_OPEN_URI flag and using a URI filename that includes
** the query parameter "key=XXXXXXXXXXX..." where the XXXX... consists
** of 64 hexadecimal digits (32 bytes of content).
**
** Create a new encrypted database by opening a file that does not
** yet exist using the key= query parameter.
**
** LIMITATIONS:
**
**    *   An obfuscated database must be created as such.  There is
**        no way to convert an existing database file into an
**        obfuscated database file other than to run ".dump" on the
**        older database and reimport the SQL text into a new
**        obfuscated database.
**
**    *   There is no way to change the key value, other than to
**        ".dump" and restore the database
**
**    *   The database page size must be exactly 8192 bytes.  No other
**        database page sizes are currently supported.
**
**    *   Memory-mapped I/O does not work for obfuscated databases.
**        If you think about it, memory-mapped I/O doesn't make any
**        sense for obfuscated databases since you have to make a
**        copy of the content to deobfuscate anyhow - you might as
**        well use normal read()/write().
**
**    *   Only the main database, the rollback journal, and WAL file
**        are obfuscated.  Other temporary files used for things like
**        SAVEPOINTs or as part of a large external sort remain
**        unobfuscated.
**
**    *   Requires SQLite 3.32.0 or later.
*/
#include "ObfuscatingVFS.h"

#include <string.h>
#include <ctype.h>
#include <stdio.h> /* For debugging only */

#include "mozilla/dom/quota/IPCStreamCipherStrategy.h"
#include "mozilla/ScopeExit.h"
#include "nsPrintfCString.h"
#include "QuotaVFS.h"
#include "sqlite3.h"

/*
** Forward declaration of objects used by this utility
*/
using ObfsVfs = sqlite3_vfs;

/*
** Useful datatype abbreviations
*/
#if !defined(SQLITE_CORE)
using u8 = unsigned char;
#endif

/* Access to a lower-level VFS that (might) implement dynamic loading,
** access to randomness, etc.
*/
#define ORIGVFS(p) ((sqlite3_vfs*)((p)->pAppData))
#define ORIGFILE(p) ((sqlite3_file*)(((ObfsFile*)(p)) + 1))

/*
** Database page size for obfuscated databases
*/
#define OBFS_PGSZ 8192

#define WAL_FRAMEHDRSIZE 24

using namespace mozilla;
using namespace mozilla::dom::quota;

/* An open file */
struct ObfsFile {
  sqlite3_file base;  /* IO methods */
  const char* zFName; /* Original name of the file */
  bool inCkpt;        /* Currently doing a checkpoint */
  ObfsFile* pPartner; /* Ptr from WAL to main-db, or from main-db to WAL */
  void* pTemp;        /* Temporary storage for encoded pages */
  IPCStreamCipherStrategy*
      encryptCipherStrategy; /* CipherStrategy for encryption */
  IPCStreamCipherStrategy*
      decryptCipherStrategy; /* CipherStrategy for decryption */
};

/*
** Methods for ObfsFile
*/
static int obfsClose(sqlite3_file*);
static int obfsRead(sqlite3_file*, void*, int iAmt, sqlite3_int64 iOfst);
static int obfsWrite(sqlite3_file*, const void*, int iAmt, sqlite3_int64 iOfst);
static int obfsTruncate(sqlite3_file*, sqlite3_int64 size);
static int obfsSync(sqlite3_file*, int flags);
static int obfsFileSize(sqlite3_file*, sqlite3_int64* pSize);
static int obfsLock(sqlite3_file*, int);
static int obfsUnlock(sqlite3_file*, int);
static int obfsCheckReservedLock(sqlite3_file*, int* pResOut);
static int obfsFileControl(sqlite3_file*, int op, void* pArg);
static int obfsSectorSize(sqlite3_file*);
static int obfsDeviceCharacteristics(sqlite3_file*);
static int obfsShmMap(sqlite3_file*, int iPg, int pgsz, int, void volatile**);
static int obfsShmLock(sqlite3_file*, int offset, int n, int flags);
static void obfsShmBarrier(sqlite3_file*);
static int obfsShmUnmap(sqlite3_file*, int deleteFlag);
static int obfsFetch(sqlite3_file*, sqlite3_int64 iOfst, int iAmt, void** pp);
static int obfsUnfetch(sqlite3_file*, sqlite3_int64 iOfst, void* p);

/*
** Methods for ObfsVfs
*/
static int obfsOpen(sqlite3_vfs*, const char*, sqlite3_file*, int, int*);
static int obfsDelete(sqlite3_vfs*, const char* zPath, int syncDir);
static int obfsAccess(sqlite3_vfs*, const char* zPath, int flags, int*);
static int obfsFullPathname(sqlite3_vfs*, const char* zPath, int, char* zOut);
static void* obfsDlOpen(sqlite3_vfs*, const char* zPath);
static void obfsDlError(sqlite3_vfs*, int nByte, char* zErrMsg);
static void (*obfsDlSym(sqlite3_vfs* pVfs, void* p, const char* zSym))(void);
static void obfsDlClose(sqlite3_vfs*, void*);
static int obfsRandomness(sqlite3_vfs*, int nByte, char* zBufOut);
static int obfsSleep(sqlite3_vfs*, int nMicroseconds);
static int obfsCurrentTime(sqlite3_vfs*, double*);
static int obfsGetLastError(sqlite3_vfs*, int, char*);
static int obfsCurrentTimeInt64(sqlite3_vfs*, sqlite3_int64*);
static int obfsSetSystemCall(sqlite3_vfs*, const char*, sqlite3_syscall_ptr);
static sqlite3_syscall_ptr obfsGetSystemCall(sqlite3_vfs*, const char* z);
static const char* obfsNextSystemCall(sqlite3_vfs*, const char* zName);

static const sqlite3_io_methods obfs_io_methods = {
    3,                         /* iVersion */
    obfsClose,                 /* xClose */
    obfsRead,                  /* xRead */
    obfsWrite,                 /* xWrite */
    obfsTruncate,              /* xTruncate */
    obfsSync,                  /* xSync */
    obfsFileSize,              /* xFileSize */
    obfsLock,                  /* xLock */
    obfsUnlock,                /* xUnlock */
    obfsCheckReservedLock,     /* xCheckReservedLock */
    obfsFileControl,           /* xFileControl */
    obfsSectorSize,            /* xSectorSize */
    obfsDeviceCharacteristics, /* xDeviceCharacteristics */
    obfsShmMap,                /* xShmMap */
    obfsShmLock,               /* xShmLock */
    obfsShmBarrier,            /* xShmBarrier */
    obfsShmUnmap,              /* xShmUnmap */
    obfsFetch,                 /* xFetch */
    obfsUnfetch                /* xUnfetch */
};

static constexpr int kKeyBytes = 32;
static constexpr int kIvBytes = IPCStreamCipherStrategy::BlockPrefixLength;
static constexpr int kClearTextPrefixBytesOnFirstPage = 32;
static constexpr int kReservedBytes = 32;
static constexpr int kBasicBlockSize = IPCStreamCipherStrategy::BasicBlockSize;
static_assert(kClearTextPrefixBytesOnFirstPage % kBasicBlockSize == 0);
static_assert(kReservedBytes % kBasicBlockSize == 0);

/* Obfuscate a page using p->encryptCipherStrategy.
**
** A new random nonce is created and stored in the last 32 bytes
** of the page.  All other bytes of the page are obfuscasted using the
** CipherStrategy.  Except, for page-1 (including the SQLite
** database header) the first 32 bytes are not obfuscated
**
** Return a pointer to the obfuscated content, which is held in the
** p->pTemp buffer.  Or return a NULL pointer if something goes wrong.
** Errors are reported using NS_WARNING().
*/
static void* obfsEncode(ObfsFile* p, /* File containing page to be obfuscated */
                        u8* a,       /* database page to be obfuscated */
                        int nByte /* Bytes of content in a[]. Must be a multiple
                                     of kBasicBlockSize. */
) {
  u8 aIv[kIvBytes];
  u8* pOut;
  int i;

  static_assert((kIvBytes & (kIvBytes - 1)) == 0);
  sqlite3_randomness(kIvBytes, aIv);
  pOut = (u8*)p->pTemp;
  if (pOut == nullptr) {
    pOut = static_cast<u8*>(sqlite3_malloc64(nByte));
    if (pOut == nullptr) {
      NS_WARNING(nsPrintfCString("unable to allocate a buffer in which to"
                                 " write obfuscated database content for %s",
                                 p->zFName)
                     .get());
      return nullptr;
    }
    p->pTemp = pOut;
  }
  if (memcmp(a, "SQLite format 3", 16) == 0) {
    i = kClearTextPrefixBytesOnFirstPage;
    if (a[20] != kReservedBytes) {
      NS_WARNING(nsPrintfCString("obfuscated database must have reserved-bytes"
                                 " set to %d",
                                 kReservedBytes)
                     .get());
      return nullptr;
    }
    memcpy(pOut, a, kClearTextPrefixBytesOnFirstPage);
  } else {
    i = 0;
  }
  const int payloadLength = nByte - kReservedBytes - i;
  MOZ_ASSERT(payloadLength > 0);
  // XXX I guess this can be done in-place as well, then we don't need the
  // temporary page at all, I guess?
  p->encryptCipherStrategy->Cipher(
      Span{aIv}, Span{a + i, static_cast<unsigned>(payloadLength)},
      Span{pOut + i, static_cast<unsigned>(payloadLength)});
  memcpy(pOut + nByte - kReservedBytes, aIv, kIvBytes);

  return pOut;
}

/* De-obfuscate a page using p->decryptCipherStrategy.
**
** The deobfuscation is done in-place.
**
** For pages that begin with the SQLite header text, the first
** 32 bytes are not deobfuscated.
*/
static void obfsDecode(ObfsFile* p, /* File containing page to be obfuscated */
                       u8* a,       /* database page to be obfuscated */
                       int nByte /* Bytes of content in a[]. Must be a multiple
                                    of kBasicBlockSize. */
) {
  int i;

  if (memcmp(a, "SQLite format 3", 16) == 0) {
    i = kClearTextPrefixBytesOnFirstPage;
  } else {
    i = 0;
  }
  const int payloadLength = nByte - kReservedBytes - i;
  MOZ_ASSERT(payloadLength > 0);
  p->decryptCipherStrategy->Cipher(
      Span{a + nByte - kReservedBytes, kIvBytes},
      Span{a + i, static_cast<unsigned>(payloadLength)},
      Span{a + i, static_cast<unsigned>(payloadLength)});
  memset(a + nByte - kReservedBytes, 0, kIvBytes);
}

/*
** Close an obfsucated file.
*/
static int obfsClose(sqlite3_file* pFile) {
  ObfsFile* p = (ObfsFile*)pFile;
  if (p->pPartner) {
    MOZ_ASSERT(p->pPartner->pPartner == p);
    p->pPartner->pPartner = nullptr;
    p->pPartner = nullptr;
  }
  sqlite3_free(p->pTemp);

  delete p->decryptCipherStrategy;
  delete p->encryptCipherStrategy;

  pFile = ORIGFILE(pFile);
  return pFile->pMethods->xClose(pFile);
}

/*
** Read data from an obfuscated file.
**
** If the file is less than one full page in length, then return
** a substitute "prototype" page-1.  This prototype page one
** specifies a database in WAL mode with an 8192-byte page size
** and a 32-byte reserved-bytes value.  Those settings are necessary
** for obfuscation to function correctly.
*/
static int obfsRead(sqlite3_file* pFile, void* zBuf, int iAmt,
                    sqlite_int64 iOfst) {
  int rc;
  ObfsFile* p = (ObfsFile*)pFile;
  pFile = ORIGFILE(pFile);
  rc = pFile->pMethods->xRead(pFile, zBuf, iAmt, iOfst);
  if (rc == SQLITE_OK) {
    if ((iAmt == OBFS_PGSZ || iAmt == OBFS_PGSZ + WAL_FRAMEHDRSIZE) &&
        !p->inCkpt) {
      obfsDecode(p, ((u8*)zBuf) + iAmt - OBFS_PGSZ, OBFS_PGSZ);
    }
  } else if (rc == SQLITE_IOERR_SHORT_READ && iOfst == 0 && iAmt >= 100) {
    static const unsigned char aEmptyDb[] = {
        // Offset 0, Size 16, The header string: "SQLite format 3\000"
        0x53, 0x51, 0x4c, 0x69, 0x74, 0x65, 0x20, 0x66, 0x6f, 0x72, 0x6d, 0x61,
        0x74, 0x20, 0x33, 0x00,
        // XXX Add description for other fields
        0x20, 0x00, 0x02, 0x02, kReservedBytes, 0x40, 0x20, 0x20, 0x00, 0x00,
        0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00,
        // Offset 52, Size 4, The page number of the largest root b-tree page
        // when in auto-vacuum or incremental-vacuum modes, or zero otherwise.
        0x00, 0x00, 0x00, 0x01};

    memcpy(zBuf, aEmptyDb, sizeof(aEmptyDb));
    memset(((u8*)zBuf) + sizeof(aEmptyDb), 0, iAmt - sizeof(aEmptyDb));
    rc = SQLITE_OK;
  }
  return rc;
}

/*
** Write data to an obfuscated file or journal.
*/
static int obfsWrite(sqlite3_file* pFile, const void* zBuf, int iAmt,
                     sqlite_int64 iOfst) {
  ObfsFile* p = (ObfsFile*)pFile;
  pFile = ORIGFILE(pFile);
  if (iAmt == OBFS_PGSZ && !p->inCkpt) {
    zBuf = obfsEncode(p, (u8*)zBuf, iAmt);
    if (zBuf == nullptr) {
      return SQLITE_IOERR;
    }
  }
  return pFile->pMethods->xWrite(pFile, zBuf, iAmt, iOfst);
}

/*
** Truncate an obfuscated file.
*/
static int obfsTruncate(sqlite3_file* pFile, sqlite_int64 size) {
  pFile = ORIGFILE(pFile);
  return pFile->pMethods->xTruncate(pFile, size);
}

/*
** Sync an obfuscated file.
*/
static int obfsSync(sqlite3_file* pFile, int flags) {
  pFile = ORIGFILE(pFile);
  return pFile->pMethods->xSync(pFile, flags);
}

/*
** Return the current file-size of an obfuscated file.
*/
static int obfsFileSize(sqlite3_file* pFile, sqlite_int64* pSize) {
  ObfsFile* p = (ObfsFile*)pFile;
  pFile = ORIGFILE(p);
  return pFile->pMethods->xFileSize(pFile, pSize);
}

/*
** Lock an obfuscated file.
*/
static int obfsLock(sqlite3_file* pFile, int eLock) {
  pFile = ORIGFILE(pFile);
  return pFile->pMethods->xLock(pFile, eLock);
}

/*
** Unlock an obfuscated file.
*/
static int obfsUnlock(sqlite3_file* pFile, int eLock) {
  pFile = ORIGFILE(pFile);
  return pFile->pMethods->xUnlock(pFile, eLock);
}

/*
** Check if another file-handle holds a RESERVED lock on an obfuscated file.
*/
static int obfsCheckReservedLock(sqlite3_file* pFile, int* pResOut) {
  pFile = ORIGFILE(pFile);
  return pFile->pMethods->xCheckReservedLock(pFile, pResOut);
}

/*
** File control method. For custom operations on an obfuscated file.
*/
static int obfsFileControl(sqlite3_file* pFile, int op, void* pArg) {
  int rc;
  ObfsFile* p = (ObfsFile*)pFile;
  pFile = ORIGFILE(pFile);
  if (op == SQLITE_FCNTL_PRAGMA) {
    char** azArg = (char**)pArg;
    MOZ_ASSERT(azArg[1] != nullptr);
    if (azArg[2] != nullptr && sqlite3_stricmp(azArg[1], "page_size") == 0) {
      /* Do not allow page size changes on an obfuscated database */
      return SQLITE_OK;
    }
  } else if (op == SQLITE_FCNTL_CKPT_START || op == SQLITE_FCNTL_CKPT_DONE) {
    p->inCkpt = op == SQLITE_FCNTL_CKPT_START;
    if (p->pPartner) {
      p->pPartner->inCkpt = p->inCkpt;
    }
  }
  rc = pFile->pMethods->xFileControl(pFile, op, pArg);
  if (rc == SQLITE_OK && op == SQLITE_FCNTL_VFSNAME) {
    *(char**)pArg = sqlite3_mprintf("obfs/%z", *(char**)pArg);
  }
  return rc;
}

/*
** Return the sector-size in bytes for an obfuscated file.
*/
static int obfsSectorSize(sqlite3_file* pFile) {
  pFile = ORIGFILE(pFile);
  return pFile->pMethods->xSectorSize(pFile);
}

/*
** Return the device characteristic flags supported by an obfuscated file.
*/
static int obfsDeviceCharacteristics(sqlite3_file* pFile) {
  pFile = ORIGFILE(pFile);
  return pFile->pMethods->xDeviceCharacteristics(pFile);
}

/* Create a shared memory file mapping */
static int obfsShmMap(sqlite3_file* pFile, int iPg, int pgsz, int bExtend,
                      void volatile** pp) {
  pFile = ORIGFILE(pFile);
  return pFile->pMethods->xShmMap(pFile, iPg, pgsz, bExtend, pp);
}

/* Perform locking on a shared-memory segment */
static int obfsShmLock(sqlite3_file* pFile, int offset, int n, int flags) {
  pFile = ORIGFILE(pFile);
  return pFile->pMethods->xShmLock(pFile, offset, n, flags);
}

/* Memory barrier operation on shared memory */
static void obfsShmBarrier(sqlite3_file* pFile) {
  pFile = ORIGFILE(pFile);
  pFile->pMethods->xShmBarrier(pFile);
}

/* Unmap a shared memory segment */
static int obfsShmUnmap(sqlite3_file* pFile, int deleteFlag) {
  pFile = ORIGFILE(pFile);
  return pFile->pMethods->xShmUnmap(pFile, deleteFlag);
}

/* Fetch a page of a memory-mapped file */
static int obfsFetch(sqlite3_file* pFile, sqlite3_int64 iOfst, int iAmt,
                     void** pp) {
  *pp = nullptr;
  return SQLITE_OK;
}

/* Release a memory-mapped page */
static int obfsUnfetch(sqlite3_file* pFile, sqlite3_int64 iOfst, void* pPage) {
  pFile = ORIGFILE(pFile);
  return pFile->pMethods->xUnfetch(pFile, iOfst, pPage);
}

/*
** Translate a single byte of Hex into an integer.
** This routine only works if h really is a valid hexadecimal
** character:  0..9a..fA..F
*/
static u8 obfsHexToInt(int h) {
  MOZ_ASSERT((h >= '0' && h <= '9') || (h >= 'a' && h <= 'f') ||
             (h >= 'A' && h <= 'F'));
#if 1 /* ASCII */
  h += 9 * (1 & (h >> 6));
#else /* EBCDIC */
  h += 9 * (1 & ~(h >> 4));
#endif
  return (u8)(h & 0xf);
}

/*
** Open a new file.
**
** If the file is an ordinary database file, or a rollback or WAL journal
** file, and if the key=XXXX parameter exists, then try to open the file
** as an obfuscated database.  All other open attempts fall through into
** the lower-level VFS shim.
**
** If the key=XXXX parameter exists but is not 64-bytes of hex key, then
** put an error message in NS_WARNING() and return SQLITE_CANTOPEN.
*/
static int obfsOpen(sqlite3_vfs* pVfs, const char* zName, sqlite3_file* pFile,
                    int flags, int* pOutFlags) {
  ObfsFile* p;
  sqlite3_file* pSubFile;
  sqlite3_vfs* pSubVfs;
  int rc, i;
  const char* zKey;
  u8 aKey[kKeyBytes];
  pSubVfs = ORIGVFS(pVfs);
  if (flags &
      (SQLITE_OPEN_MAIN_DB | SQLITE_OPEN_WAL | SQLITE_OPEN_MAIN_JOURNAL)) {
    zKey = sqlite3_uri_parameter(zName, "key");
  } else {
    zKey = nullptr;
  }
  if (zKey == nullptr) {
    return pSubVfs->xOpen(pSubVfs, zName, pFile, flags, pOutFlags);
  }
  for (i = 0;
       i < kKeyBytes && isxdigit(zKey[i * 2]) && isxdigit(zKey[i * 2 + 1]);
       i++) {
    aKey[i] = (obfsHexToInt(zKey[i * 2]) << 4) | obfsHexToInt(zKey[i * 2 + 1]);
  }
  if (i != kKeyBytes) {
    NS_WARNING(
        nsPrintfCString("invalid query parameter on %s: key=%s", zName, zKey)
            .get());
    return SQLITE_CANTOPEN;
  }
  p = (ObfsFile*)pFile;
  memset(p, 0, sizeof(*p));

  auto encryptCipherStrategy = MakeUnique<IPCStreamCipherStrategy>();
  auto decryptCipherStrategy = MakeUnique<IPCStreamCipherStrategy>();

  auto resetMethods = MakeScopeExit([pFile] { pFile->pMethods = nullptr; });

  if (NS_WARN_IF(NS_FAILED(encryptCipherStrategy->Init(
          CipherMode::Encrypt, Span{aKey, sizeof(aKey)},
          IPCStreamCipherStrategy::MakeBlockPrefix())))) {
    return SQLITE_ERROR;
  }

  if (NS_WARN_IF(NS_FAILED(decryptCipherStrategy->Init(
          CipherMode::Decrypt, Span{aKey, sizeof(aKey)})))) {
    return SQLITE_ERROR;
  }

  pSubFile = ORIGFILE(pFile);
  p->base.pMethods = &obfs_io_methods;
  rc = pSubVfs->xOpen(pSubVfs, zName, pSubFile, flags, pOutFlags);
  if (rc) {
    return rc;
  }

  resetMethods.release();

  if (flags & (SQLITE_OPEN_WAL | SQLITE_OPEN_MAIN_JOURNAL)) {
    sqlite3_file* pDb = sqlite3_database_file_object(zName);
    p->pPartner = (ObfsFile*)pDb;
    MOZ_ASSERT(p->pPartner->pPartner == nullptr);
    p->pPartner->pPartner = p;
  }
  p->zFName = zName;

  p->encryptCipherStrategy = encryptCipherStrategy.release();
  p->decryptCipherStrategy = decryptCipherStrategy.release();

  return SQLITE_OK;
}

/*
** All other VFS methods are pass-thrus.
*/
static int obfsDelete(sqlite3_vfs* pVfs, const char* zPath, int syncDir) {
  return ORIGVFS(pVfs)->xDelete(ORIGVFS(pVfs), zPath, syncDir);
}
static int obfsAccess(sqlite3_vfs* pVfs, const char* zPath, int flags,
                      int* pResOut) {
  return ORIGVFS(pVfs)->xAccess(ORIGVFS(pVfs), zPath, flags, pResOut);
}
static int obfsFullPathname(sqlite3_vfs* pVfs, const char* zPath, int nOut,
                            char* zOut) {
  return ORIGVFS(pVfs)->xFullPathname(ORIGVFS(pVfs), zPath, nOut, zOut);
}
static void* obfsDlOpen(sqlite3_vfs* pVfs, const char* zPath) {
  return ORIGVFS(pVfs)->xDlOpen(ORIGVFS(pVfs), zPath);
}
static void obfsDlError(sqlite3_vfs* pVfs, int nByte, char* zErrMsg) {
  ORIGVFS(pVfs)->xDlError(ORIGVFS(pVfs), nByte, zErrMsg);
}
static void (*obfsDlSym(sqlite3_vfs* pVfs, void* p, const char* zSym))(void) {
  return ORIGVFS(pVfs)->xDlSym(ORIGVFS(pVfs), p, zSym);
}
static void obfsDlClose(sqlite3_vfs* pVfs, void* pHandle) {
  ORIGVFS(pVfs)->xDlClose(ORIGVFS(pVfs), pHandle);
}
static int obfsRandomness(sqlite3_vfs* pVfs, int nByte, char* zBufOut) {
  return ORIGVFS(pVfs)->xRandomness(ORIGVFS(pVfs), nByte, zBufOut);
}
static int obfsSleep(sqlite3_vfs* pVfs, int nMicroseconds) {
  return ORIGVFS(pVfs)->xSleep(ORIGVFS(pVfs), nMicroseconds);
}
static int obfsCurrentTime(sqlite3_vfs* pVfs, double* pTimeOut) {
  return ORIGVFS(pVfs)->xCurrentTime(ORIGVFS(pVfs), pTimeOut);
}
static int obfsGetLastError(sqlite3_vfs* pVfs, int a, char* b) {
  return ORIGVFS(pVfs)->xGetLastError(ORIGVFS(pVfs), a, b);
}
static int obfsCurrentTimeInt64(sqlite3_vfs* pVfs, sqlite3_int64* p) {
  return ORIGVFS(pVfs)->xCurrentTimeInt64(ORIGVFS(pVfs), p);
}
static int obfsSetSystemCall(sqlite3_vfs* pVfs, const char* zName,
                             sqlite3_syscall_ptr pCall) {
  return ORIGVFS(pVfs)->xSetSystemCall(ORIGVFS(pVfs), zName, pCall);
}
static sqlite3_syscall_ptr obfsGetSystemCall(sqlite3_vfs* pVfs,
                                             const char* zName) {
  return ORIGVFS(pVfs)->xGetSystemCall(ORIGVFS(pVfs), zName);
}
static const char* obfsNextSystemCall(sqlite3_vfs* pVfs, const char* zName) {
  return ORIGVFS(pVfs)->xNextSystemCall(ORIGVFS(pVfs), zName);
}

namespace mozilla::storage::obfsvfs {

const char* GetVFSName() { return "obfsvfs"; }

UniquePtr<sqlite3_vfs> ConstructVFS(const char* aBaseVFSName) {
  MOZ_ASSERT(aBaseVFSName);

  if (sqlite3_vfs_find(GetVFSName()) != nullptr) {
    return nullptr;
  }
  sqlite3_vfs* const pOrig = sqlite3_vfs_find(aBaseVFSName);
  if (pOrig == nullptr) {
    return nullptr;
  }

#ifdef DEBUG
  // If the VFS version is higher than the last known one, you should update
  // this VFS adding appropriate methods for any methods added in the version
  // change.
  static constexpr int kLastKnownVfsVersion = 3;
  MOZ_ASSERT(pOrig->iVersion <= kLastKnownVfsVersion);
#endif

  const sqlite3_vfs obfs_vfs = {
      pOrig->iVersion,                                      /* iVersion  */
      static_cast<int>(pOrig->szOsFile + sizeof(ObfsFile)), /* szOsFile */
      pOrig->mxPathname,                                    /* mxPathname */
      nullptr,                                              /* pNext */
      GetVFSName(),                                         /* zName */
      pOrig,                                                /* pAppData */
      obfsOpen,                                             /* xOpen */
      obfsDelete,                                           /* xDelete */
      obfsAccess,                                           /* xAccess */
      obfsFullPathname,                                     /* xFullPathname */
      obfsDlOpen,                                           /* xDlOpen */
      obfsDlError,                                          /* xDlError */
      obfsDlSym,                                            /* xDlSym */
      obfsDlClose,                                          /* xDlClose */
      obfsRandomness,                                       /* xRandomness */
      obfsSleep,                                            /* xSleep */
      obfsCurrentTime,                                      /* xCurrentTime */
      obfsGetLastError,                                     /* xGetLastError */
      obfsCurrentTimeInt64, /* xCurrentTimeInt64 */
      obfsSetSystemCall,    /* xSetSystemCall */
      obfsGetSystemCall,    /* xGetSystemCall */
      obfsNextSystemCall    /* xNextSystemCall */
  };

  return MakeUnique<sqlite3_vfs>(obfs_vfs);
}

already_AddRefed<QuotaObject> GetQuotaObjectForFile(sqlite3_file* pFile) {
  return quotavfs::GetQuotaObjectForFile(ORIGFILE(pFile));
}

}  // namespace mozilla::storage::obfsvfs
