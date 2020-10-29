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
** written to disk by XOR-ing a hex pattern passed in as a query parameter.
**
** COMPILING
**
** This extension requires SQLite 3.32.0 or later.
**
** To build this extension as a separately loaded shared library or
** DLL, use compiler command-lines similar to the following:
**
**   (linux)    gcc -fPIC -shared obfsvfs.c -o obfsvfs.so
**   (mac)      clang -fPIC -dynamiclib obfsvfs.c -o obfsvfs.dylib
**   (windows)  cl obfsvfs.c -link -dll -out:obfsvfs.dll
**
** You may want to add additional compiler options, of course,
** according to the needs of your project.
**
** If you want to statically link this extension with your product,
** then compile it like any other C-language module but add the
** "-DSQLITE_OBFSVFS_STATIC" option so that this module knows that
** it is being statically linked rather than dynamically linked
**
** LOADING
**
** To load this extension as a shared library, you first have to
** bring up a dummy SQLite database connection to use as the argument
** to the sqlite3_load_extension() API call.  Then you invoke the
** sqlite3_load_extension() API and shutdown the dummy database
** connection.  All subsequent database connections that are opened
** will include this extension.  For example:
**
**     sqlite3 *db;
**     sqlite3_open(":memory:", &db);
**     sqlite3_load_extention(db, "./obfsvfs");
**     sqlite3_close(db);
**
** If this extension is compiled with -DSQLITE_OBFSVFS_STATIC and
** statically linked against the application, initialize it using
** a single API call as follows:
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
#ifdef SQLITE_OBFSVFS_STATIC
#  include "sqlite3.h"
#else
#  include "sqlite3ext.h"
SQLITE_EXTENSION_INIT1
#endif
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <stdio.h> /* For debugging only */

/*
** Forward declaration of objects used by this utility
*/
typedef struct sqlite3_vfs ObfsVfs;
typedef struct ObfsFile ObfsFile;

/*
** Useful datatype abbreviations
*/
#if !defined(SQLITE_CORE)
typedef unsigned char u8;
#endif

/* Access to a lower-level VFS that (might) implement dynamic loading,
** access to randomness, etc.
*/
#define ORIGVFS(p) ((sqlite3_vfs*)((p)->pAppData))
#define ORIGFILE(p) ((sqlite3_file*)(((ObfsFile*)(p)) + 1))

/*
** Size of the obfuscation key
*/
#define OBFS_KEYSZ 32

/*
** Database page size for obfuscated databases
*/
#define OBFS_PGSZ 8192

/* An open file */
struct ObfsFile {
  sqlite3_file base;   /* IO methods */
  const char* zFName;  /* Original name of the file */
  char inCkpt;         /* Currently doing a checkpoint */
  ObfsFile* pPartner;  /* Ptr from WAL to main-db, or from main-db to WAL */
  void* pTemp;         /* Temporary storage for encoded pages */
  u8 aKey[OBFS_KEYSZ]; /* Obfuscation key */
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
static int obfsDelete(sqlite3_vfs*, const char* zName, int syncDir);
static int obfsAccess(sqlite3_vfs*, const char* zName, int flags, int*);
static int obfsFullPathname(sqlite3_vfs*, const char* zName, int, char* zOut);
static void* obfsDlOpen(sqlite3_vfs*, const char* zFilename);
static void obfsDlError(sqlite3_vfs*, int nByte, char* zErrMsg);
static void (*obfsDlSym(sqlite3_vfs* pVfs, void* p, const char* zSym))(void);
static void obfsDlClose(sqlite3_vfs*, void*);
static int obfsRandomness(sqlite3_vfs*, int nByte, char* zOut);
static int obfsSleep(sqlite3_vfs*, int microseconds);
static int obfsCurrentTime(sqlite3_vfs*, double*);
static int obfsGetLastError(sqlite3_vfs*, int, char*);
static int obfsCurrentTimeInt64(sqlite3_vfs*, sqlite3_int64*);
static int obfsSetSystemCall(sqlite3_vfs*, const char*, sqlite3_syscall_ptr);
static sqlite3_syscall_ptr obfsGetSystemCall(sqlite3_vfs*, const char* z);
static const char* obfsNextSystemCall(sqlite3_vfs*, const char* zName);

static sqlite3_vfs obfs_vfs = {
    3,                    /* iVersion (set when registered) */
    0,                    /* szOsFile (set when registered) */
    1024,                 /* mxPathname */
    0,                    /* pNext */
    "obfsvfs",            /* zName */
    0,                    /* pAppData (set when registered) */
    obfsOpen,             /* xOpen */
    obfsDelete,           /* xDelete */
    obfsAccess,           /* xAccess */
    obfsFullPathname,     /* xFullPathname */
    obfsDlOpen,           /* xDlOpen */
    obfsDlError,          /* xDlError */
    obfsDlSym,            /* xDlSym */
    obfsDlClose,          /* xDlClose */
    obfsRandomness,       /* xRandomness */
    obfsSleep,            /* xSleep */
    obfsCurrentTime,      /* xCurrentTime */
    obfsGetLastError,     /* xGetLastError */
    obfsCurrentTimeInt64, /* xCurrentTimeInt64 */
    obfsSetSystemCall,    /* xSetSystemCall */
    obfsGetSystemCall,    /* xGetSystemCall */
    obfsNextSystemCall    /* xNextSystemCall */
};

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

/* Obfuscate a page using the key in p->aKey[].
**
** A new random nonce is created and stored in the last 32 bytes
** of the page.  All other bytes of the page are XOR-ed against both
** the key and the nonce.  Except, for page-1 (including the SQLite
** database header) the first 32 bytes are not obfuscated
**
** Return a pointer to the obfuscated content, which is held in the
** p->aTemp buffer.  Or return a NULL pointer if something goes wrong.
** Errors are reported using sqlite3_log().
*/
static void* obfsEncode(
    ObfsFile* p, /* File containing page to be obfuscated */
    u8* a,       /* database page to be obfuscated */
    int nByte /* Bytes of content in a[]. Must be a multiple of OBFS_KEYSZ. */
) {
  u8 aKey[OBFS_KEYSZ];
  u8* pOut;
  int i;

  assert((OBFS_KEYSZ & (OBFS_KEYSZ - 1)) == 0);
  sqlite3_randomness(OBFS_KEYSZ, aKey);
  pOut = (u8*)p->pTemp;
  if (pOut == 0) {
    pOut = static_cast<u8*>(sqlite3_malloc64(nByte));
    if (pOut == 0) {
      sqlite3_log(SQLITE_NOMEM,
                  "unable to allocate a buffer in which to"
                  " write obfuscated database content for %s",
                  p->zFName);
      return 0;
    }
  }
  memcpy(pOut + nByte - OBFS_KEYSZ, aKey, OBFS_KEYSZ);
  for (i = 0; i < OBFS_KEYSZ; i++) {
    aKey[i] ^= p->aKey[i];
  }
  if (memcmp(a, "SQLite format 3", 16) == 0) {
    i = OBFS_KEYSZ;
    if (a[20] != OBFS_KEYSZ) {
      sqlite3_log(SQLITE_IOERR,
                  "obfuscated database must have reserve-bytes"
                  " set to %d",
                  OBFS_KEYSZ);
      return 0;
    }
    memcpy(pOut, a, OBFS_KEYSZ);
  } else {
    i = 0;
  }
  while (i < nByte - OBFS_KEYSZ) {
    pOut[i] = aKey[i & (OBFS_KEYSZ - 1)] ^ a[i];
    i++;
  }
  return pOut;
}

/* De-obfuscate a page using the key in p->aKey[].
**
** Deobfuscation consists of XORing all bytes against both the key
** and the nonce stored in the last 32-bytes of the page.  The
** deobfuscation is done in-place.
**
** For pages that begin with the SQLite header text, the first
** 32 bytes are not deobfuscated.
*/
static void obfsDecode(
    ObfsFile* p, /* File containing page to be obfuscated */
    u8* a,       /* database page to be obfuscated */
    int nByte /* Bytes of content in a[]. Must be a multiple of OBFS_KEYSZ. */
) {
  u8 aKey[OBFS_KEYSZ];
  int i;

  for (i = 0; i < OBFS_KEYSZ; i++) {
    aKey[i] = p->aKey[i] ^ a[i + nByte - OBFS_KEYSZ];
  }
  if (memcmp(a, "SQLite format 3", 16) == 0) {
    i = OBFS_KEYSZ;
  } else {
    i = 0;
  }
  while (i < nByte - OBFS_KEYSZ) {
    a[i] ^= aKey[i & (OBFS_KEYSZ - 1)];
    i++;
  }
}

/*
** Close an obfsucated file.
*/
static int obfsClose(sqlite3_file* pFile) {
  ObfsFile* p = (ObfsFile*)pFile;
  if (p->pPartner) {
    assert(p->pPartner->pPartner == p);
    p->pPartner->pPartner = 0;
    p->pPartner = 0;
  }
  sqlite3_free(p->pTemp);
  pFile = ORIGFILE(pFile);
  return pFile->pMethods->xClose(pFile);
}

/*
** Read data from an obfuscated file.
**
** If the file is less than one full page in length, then return
** a substitute "prototype" page-1.  This prototype page one
** specifies a database in WAL mode with an 8192-byte page size
** and a 32-byte reserve-byte value.  Those settings are necessary
** for obfuscation to function correctly.
*/
static int obfsRead(sqlite3_file* pFile, void* zBuf, int iAmt,
                    sqlite_int64 iOfst) {
  int rc;
  ObfsFile* p = (ObfsFile*)pFile;
  pFile = ORIGFILE(pFile);
  rc = pFile->pMethods->xRead(pFile, zBuf, iAmt, iOfst);
  if (rc == SQLITE_OK) {
    if (iAmt == OBFS_PGSZ && !p->inCkpt) {
      obfsDecode(p, (u8*)zBuf, iAmt);
    }
  } else if (SQLITE_IOERR_SHORT_READ && iOfst == 0 && iAmt >= 100) {
    static const unsigned char aEmptyDb[] = {
        0x53, 0x51, 0x4c, 0x69, 0x74, 0x65, 0x20, 0x66, 0x6f, 0x72, 0x6d,
        0x61, 0x74, 0x20, 0x33, 0x00, 0x20, 0x00, 0x02, 0x02, 0x20, 0x40,
        0x20, 0x20, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01};
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
    if (zBuf == 0) return SQLITE_IOERR;
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
    assert(azArg[1] != 0);
    if (azArg[2] != 0 && sqlite3_stricmp(azArg[1], "page_size") == 0) {
      /* Do not allow page size changes on an obfuscated database */
      return SQLITE_OK;
    }
  } else if (op == SQLITE_FCNTL_CKPT_START || op == SQLITE_FCNTL_CKPT_DONE) {
    p->inCkpt = op == SQLITE_FCNTL_CKPT_START;
    if (p->pPartner) p->pPartner->inCkpt = p->inCkpt;
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
  *pp = 0;
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
  assert((h >= '0' && h <= '9') || (h >= 'a' && h <= 'f') ||
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
** put an error message in sqlite3_log() and return SQLITE_CANTOPEN.
*/
static int obfsOpen(sqlite3_vfs* pVfs, const char* zName, sqlite3_file* pFile,
                    int flags, int* pOutFlags) {
  ObfsFile* p;
  sqlite3_file* pSubFile;
  sqlite3_vfs* pSubVfs;
  int rc, i;
  const char* zKey;
  u8 aKey[OBFS_KEYSZ];
  pSubVfs = ORIGVFS(pVfs);
  if (flags &
      (SQLITE_OPEN_MAIN_DB | SQLITE_OPEN_WAL | SQLITE_OPEN_MAIN_JOURNAL)) {
    zKey = sqlite3_uri_parameter(zName, "key");
  } else {
    zKey = 0;
  }
  if (zKey == 0) {
    return pSubVfs->xOpen(pSubVfs, zName, pFile, flags, pOutFlags);
  }
  for (i = 0;
       i < OBFS_KEYSZ && isxdigit(zKey[i * 2]) && isxdigit(zKey[i * 2 + 1]);
       i++) {
    aKey[i] = (obfsHexToInt(zKey[i * 2]) << 8) | obfsHexToInt(zKey[i * 2 + 1]);
  }
  if (i != OBFS_KEYSZ) {
    sqlite3_log(SQLITE_CANTOPEN, "invalid query parameter on %s: key=%s", zName,
                zKey);
    return SQLITE_CANTOPEN;
  }
  p = (ObfsFile*)pFile;
  memset(p, 0, sizeof(*p));
  memcpy(p->aKey, aKey, sizeof(aKey));
  pSubFile = ORIGFILE(pFile);
  p->base.pMethods = &obfs_io_methods;
  rc = pSubVfs->xOpen(pSubVfs, zName, pSubFile, flags, pOutFlags);
  if (rc) goto obfs_open_done;
  if (flags & (SQLITE_OPEN_WAL | SQLITE_OPEN_MAIN_JOURNAL)) {
    sqlite3_file* pDb = sqlite3_database_file_object(zName);
    p->pPartner = (ObfsFile*)pDb;
    assert(p->pPartner->pPartner == 0);
    p->pPartner->pPartner = p;
  }
  p->zFName = zName;
obfs_open_done:
  if (rc) pFile->pMethods = 0;
  return rc;
}

/*
** All other VFS methods are pass-thrus.
*/
static int obfsDelete(sqlite3_vfs* pVfs, const char* zPath, int dirSync) {
  return ORIGVFS(pVfs)->xDelete(ORIGVFS(pVfs), zPath, dirSync);
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
static int obfsSleep(sqlite3_vfs* pVfs, int nMicro) {
  return ORIGVFS(pVfs)->xSleep(ORIGVFS(pVfs), nMicro);
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

/*
** Register the obfuscator VFS as the default VFS for the system.
*/
static int obfsRegisterVfs(void) {
  int rc = SQLITE_OK;
  sqlite3_vfs* pOrig;
  if (sqlite3_vfs_find("obfs") != 0) return SQLITE_OK;
  pOrig = sqlite3_vfs_find(0);
  obfs_vfs.iVersion = pOrig->iVersion;
  obfs_vfs.pAppData = pOrig;
  obfs_vfs.szOsFile = pOrig->szOsFile + sizeof(ObfsFile);
  rc = sqlite3_vfs_register(&obfs_vfs, 1);
  return rc;
}

#if defined(SQLITE_OBFSVFS_STATIC)
/* This variant of the initializer runs when the extension is
** statically linked.
*/
int sqlite3_register_obfsvfs(const char* NotUsed) {
  (void)NotUsed;
  return obfsRegisterVfs();
}
#endif /* defined(SQLITE_OBFSVFS_STATIC */

#if !defined(SQLITE_OBFSVFS_STATIC)
/* This variant of the initializer function is used when the
** extension is shared library to be loaded at run-time.
*/
#  ifdef _WIN32
__declspec(dllexport)
#  endif
    /*
    ** This routine is called by sqlite3_load_extension() when the
    ** extension is first loaded.
    ***/
    int sqlite3_obfsvfs_init(sqlite3* db, char** pzErrMsg,
                             const sqlite3_api_routines* pApi) {
  int rc;
  SQLITE_EXTENSION_INIT2(pApi);
  (void)pzErrMsg; /* not used */
  rc = obfsRegisterVfs();
  if (rc == SQLITE_OK) rc = SQLITE_OK_LOAD_PERMANENTLY;
  return rc;
}
#endif /* !defined(SQLITE_OBFSVFS_STATIC) */
