/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* OS2 IO module
 *
 * Assumes synchronous I/O.
 *
 */

#include "primpl.h"
#include "prio.h"
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <dirent.h>
#include <fcntl.h>
#include <io.h>

struct _MDLock               _pr_ioq_lock;

static PRBool isWSEB = PR_FALSE; /* whether we are using an OS/2 kernel that supports large files */

typedef APIRET (*DosOpenLType)(PSZ pszFileName, PHFILE pHf, PULONG pulAction,
                            LONGLONG cbFile, ULONG ulAttribute,
                            ULONG fsOpenFlags, ULONG fsOpenMode,
                            PEAOP2 peaop2);

typedef APIRET (*DosSetFileLocksLType)(HFILE hFile, PFILELOCKL pflUnlock,
                                    PFILELOCKL pflLock, ULONG timeout,
                                    ULONG flags);

typedef APIRET (*DosSetFilePtrLType)(HFILE hFile, LONGLONG ib, ULONG method,
                                  PLONGLONG ibActual);

DosOpenLType myDosOpenL;
DosSetFileLocksLType myDosSetFileLocksL;
DosSetFilePtrLType myDosSetFilePtrL;

void
_PR_MD_INIT_IO()
{
    APIRET rc;
    HMODULE module;

    sock_init();
    
    rc = DosLoadModule(NULL, 0, "DOSCALL1", &module);
    if (rc != NO_ERROR)
    {
        return;
    }
    rc = DosQueryProcAddr(module, 981, NULL, (PFN*) &myDosOpenL);
    if (rc != NO_ERROR)
    {
        return;
    }
    rc = DosQueryProcAddr(module, 986, NULL, (PFN*) &myDosSetFileLocksL);
    if (rc != NO_ERROR)
    {
        return;
    }
    rc = DosQueryProcAddr(module, 988, NULL, (PFN*) &myDosSetFilePtrL);
    if (rc != NO_ERROR)
    {
        return;
    }
    isWSEB = PR_TRUE;
}

PRStatus
_PR_MD_WAIT(PRThread *thread, PRIntervalTime ticks)
{
    PRInt32 rv;
    ULONG count;

    PRUint32 msecs = (ticks == PR_INTERVAL_NO_TIMEOUT) ?
        SEM_INDEFINITE_WAIT : PR_IntervalToMilliseconds(ticks);
    rv = DosWaitEventSem(thread->md.blocked_sema, msecs);
    DosResetEventSem(thread->md.blocked_sema, &count); 
    switch(rv) 
    {
        case NO_ERROR:
            return PR_SUCCESS;
            break;
        case ERROR_TIMEOUT:
            _PR_THREAD_LOCK(thread);
            if (thread->state == _PR_IO_WAIT) {
			  ;
            } else {
                if (thread->wait.cvar != NULL) {
                    thread->wait.cvar = NULL;
                    _PR_THREAD_UNLOCK(thread);
                } else {
                    /* The CVAR was notified just as the timeout
                     * occurred.  This led to us being notified twice.
                     * call SemRequest() to clear the semaphore.
                     */
                    _PR_THREAD_UNLOCK(thread);
                    rv = DosWaitEventSem(thread->md.blocked_sema, 0);
                    DosResetEventSem(thread->md.blocked_sema, &count); 
                    PR_ASSERT(rv == NO_ERROR);
                }
            }
            return PR_SUCCESS;
            break;
        default:
            break;
    }
    return PR_FAILURE;
}
PRStatus
_PR_MD_WAKEUP_WAITER(PRThread *thread)
{
    if ( _PR_IS_NATIVE_THREAD(thread) ) 
    {
        if (DosPostEventSem(thread->md.blocked_sema) != NO_ERROR)
            return PR_FAILURE;
        else
			return PR_SUCCESS;
	}
}


/* --- FILE IO ----------------------------------------------------------- */
/*
 *  _PR_MD_OPEN() -- Open a file
 *
 *  returns: a fileHandle
 *
 *  The NSPR open flags (osflags) are translated into flags for OS/2
 *
 *  Mode seems to be passed in as a unix style file permissions argument
 *  as in 0666, in the case of opening the logFile. 
 *
 */
PRInt32
_PR_MD_OPEN(const char *name, PRIntn osflags, int mode)
{
    HFILE file;
    PRInt32 access = OPEN_SHARE_DENYNONE;
    PRInt32 flags = 0L;
    APIRET rc = 0;
    PRUword actionTaken;

#ifdef MOZ_OS2_HIGH_MEMORY
    /*
     * All the pointer arguments (&file, &actionTaken and name) have to be in
     * low memory for DosOpen to use them.
     * The following moves name to low memory.
     */ 
    if ((ULONG)name >= 0x20000000)
    {
        size_t len = strlen(name) + 1;
        char *copy = (char *)alloca(len);
        memcpy(copy, name, len);
        name = copy;
    }
#endif

    if (osflags & PR_SYNC) access |= OPEN_FLAGS_WRITE_THROUGH;

    if (osflags & PR_RDONLY)
        access |= OPEN_ACCESS_READONLY;
    else if (osflags & PR_WRONLY)
        access |= OPEN_ACCESS_WRITEONLY;
    else if(osflags & PR_RDWR)
        access |= OPEN_ACCESS_READWRITE;

    if ( osflags & PR_CREATE_FILE && osflags & PR_EXCL )
    {
        flags = OPEN_ACTION_CREATE_IF_NEW | OPEN_ACTION_FAIL_IF_EXISTS;
    }
    else if (osflags & PR_CREATE_FILE)
    {
        if (osflags & PR_TRUNCATE)
            flags = OPEN_ACTION_CREATE_IF_NEW | OPEN_ACTION_REPLACE_IF_EXISTS;
        else
            flags = OPEN_ACTION_CREATE_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS;
    } 
    else
    {
        if (osflags & PR_TRUNCATE)
            flags = OPEN_ACTION_FAIL_IF_NEW | OPEN_ACTION_REPLACE_IF_EXISTS;
        else
            flags = OPEN_ACTION_FAIL_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS;
    }

    do {
        if (isWSEB)
        {
                rc = myDosOpenL((char*)name,
                     &file,            /* file handle if successful */
                     &actionTaken,     /* reason for failure        */
                     0,                /* initial size of new file  */
                     FILE_NORMAL,      /* file system attributes    */
                     flags,            /* Open flags                */
                     access,           /* Open mode and rights      */
                     0);               /* OS/2 Extended Attributes  */
        }
        else
        {
                rc = DosOpen((char*)name,
                     &file,            /* file handle if successful */
                     &actionTaken,     /* reason for failure        */
                     0,                /* initial size of new file  */
                     FILE_NORMAL,      /* file system attributes    */
                     flags,            /* Open flags                */
                     access,           /* Open mode and rights      */
                     0);               /* OS/2 Extended Attributes  */
        };
        if (rc == ERROR_TOO_MANY_OPEN_FILES) {
            ULONG CurMaxFH = 0;
            LONG ReqCount = 20;
            APIRET rc2;
            rc2 = DosSetRelMaxFH(&ReqCount, &CurMaxFH);
            if (rc2 != NO_ERROR) {
                break;
            }
        }
    } while (rc == ERROR_TOO_MANY_OPEN_FILES);

    if (rc != NO_ERROR) {
        _PR_MD_MAP_OPEN_ERROR(rc);
        return -1; 
    }

    return (PRInt32)file;
}

PRInt32
_PR_MD_READ(PRFileDesc *fd, void *buf, PRInt32 len)
{
    ULONG bytes;
    int rv;

    rv = DosRead((HFILE)fd->secret->md.osfd,
                 (PVOID)buf,
                 len,
                 &bytes);
    
    if (rv != NO_ERROR) 
    {
        /* ERROR_HANDLE_EOF can only be returned by async io */
        PR_ASSERT(rv != ERROR_HANDLE_EOF);
        if (rv == ERROR_BROKEN_PIPE)
            return 0;
		else {
			_PR_MD_MAP_READ_ERROR(rv);
        return -1;
    }
    }
    return (PRInt32)bytes;
}

PRInt32
_PR_MD_WRITE(PRFileDesc *fd, const void *buf, PRInt32 len)
{
    PRInt32 bytes;
    int rv; 

    rv = DosWrite((HFILE)fd->secret->md.osfd,
                  (PVOID)buf,
                  len,
                  (PULONG)&bytes);

    if (rv != NO_ERROR) 
    {
        _PR_MD_MAP_WRITE_ERROR(rv);
        return -1;
    }

    if (len != bytes) {
        rv = ERROR_DISK_FULL;
        _PR_MD_MAP_WRITE_ERROR(rv);
        return -1;
    }

    return bytes;
} /* --- end _PR_MD_WRITE() --- */

PRInt32
_PR_MD_LSEEK(PRFileDesc *fd, PRInt32 offset, PRSeekWhence whence)
{
    PRInt32 rv;
    PRUword newLocation;

    rv = DosSetFilePtr((HFILE)fd->secret->md.osfd, offset, whence, &newLocation);

	if (rv != NO_ERROR) {
		_PR_MD_MAP_LSEEK_ERROR(rv);
		return -1;
	} else
		return newLocation;
}

PRInt64
_PR_MD_LSEEK64(PRFileDesc *fd, PRInt64 offset, PRSeekWhence whence)
{
#ifdef NO_LONG_LONG
    PRInt64 result;
    PRInt32 rv, low = offset.lo, hi = offset.hi;
    PRUword newLocation;

    rv = DosSetFilePtr((HFILE)fd->secret->md.osfd, low, whence, &newLocation);
    rv = DosSetFilePtr((HFILE)fd->secret->md.osfd, hi, FILE_CURRENT, &newLocation);

  	if (rv != NO_ERROR) {
		_PR_MD_MAP_LSEEK_ERROR(rv);
		hi = newLocation = -1;
   }

    result.lo = newLocation;
    result.hi = hi;
	return result;

#else
    PRInt32 where, rc, lo = (PRInt32)offset, hi = (PRInt32)(offset >> 32);
    PRUint64 rv;
    PRUint32 newLocation, uhi;
    PRUint64 newLocationL;

    switch (whence)
      {
      case PR_SEEK_SET:
        where = FILE_BEGIN;
        break;
      case PR_SEEK_CUR:
        where = FILE_CURRENT;
        break;
      case PR_SEEK_END:
        where = FILE_END;
        break;
      default:
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return -1;
    }
    if (isWSEB)
    {
        rc = myDosSetFilePtrL((HFILE)fd->secret->md.osfd, offset, where, (PLONGLONG)&newLocationL);
    }
    else
    {
        rc = DosSetFilePtr((HFILE)fd->secret->md.osfd, lo, where, (PULONG)&newLocation);
    }
     
    if (rc != NO_ERROR) {
      _PR_MD_MAP_LSEEK_ERROR(rc);
      return -1;
    }
    
    if (isWSEB)
    {
        return newLocationL;
    }

    uhi = (PRUint32)hi;
    PR_ASSERT((PRInt32)uhi >= 0);
    rv = uhi;
    PR_ASSERT((PRInt64)rv >= 0);
    rv = (rv << 32);
    PR_ASSERT((PRInt64)rv >= 0);
    rv += newLocation;
    PR_ASSERT((PRInt64)rv >= 0);
    return (PRInt64)rv;
#endif
}

PRInt32
_PR_MD_FSYNC(PRFileDesc *fd)
{
    PRInt32 rc = DosResetBuffer((HFILE)fd->secret->md.osfd);

    if (rc != NO_ERROR) {
   	if (rc != ERROR_ACCESS_DENIED) {	
   			_PR_MD_MAP_FSYNC_ERROR(rc);
   	    return -1;
   	}
    }
    return 0;
}

PRInt32
_MD_CloseFile(PRInt32 osfd)
{
    PRInt32 rv;
    
    rv = DosClose((HFILE)osfd);
 	if (rv != NO_ERROR)
		_PR_MD_MAP_CLOSE_ERROR(rv);
    return rv;
}


/* --- DIR IO ------------------------------------------------------------ */
#define GetFileFromDIR(d)       (isWSEB?(d)->d_entry.large.achName:(d)->d_entry.small.achName)
#define GetFileAttr(d)          (isWSEB?(d)->d_entry.large.attrFile:(d)->d_entry.small.attrFile)

void FlipSlashes(char *cp, int len)
{
    while (--len >= 0) {
    if (cp[0] == '/') {
        cp[0] = PR_DIRECTORY_SEPARATOR;
    }
    cp++;
    }
}

/*
**
** Local implementations of standard Unix RTL functions which are not provided
** by the VAC RTL.
**
*/

PRInt32
_PR_MD_CLOSE_DIR(_MDDir *d)
{
   PRInt32 rc;

    if ( d ) {
      rc = DosFindClose(d->d_hdl);
      if(rc == NO_ERROR){
        d->magic = (PRUint32)-1;
        return PR_SUCCESS;
		} else {
			_PR_MD_MAP_CLOSEDIR_ERROR(rc);
        	return PR_FAILURE;
		}
    }
    PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
    return PR_FAILURE;
}


PRStatus
_PR_MD_OPEN_DIR(_MDDir *d, const char *name)
{
    char filename[ CCHMAXPATH ];
    PRUword numEntries, rc;

    numEntries = 1;

    PR_snprintf(filename, CCHMAXPATH, "%s%s%s",
                name, PR_DIRECTORY_SEPARATOR_STR, "*.*");
    FlipSlashes( filename, strlen(filename) );

    d->d_hdl = HDIR_CREATE;

    if (isWSEB)
    {
        rc = DosFindFirst( filename,
                           &d->d_hdl,
                           FILE_DIRECTORY | FILE_HIDDEN,
                           &(d->d_entry.large),
                           sizeof(d->d_entry.large),
                           &numEntries,
                           FIL_STANDARDL);
    }
    else
    {
        rc = DosFindFirst( filename,
                           &d->d_hdl,
                           FILE_DIRECTORY | FILE_HIDDEN,
                           &(d->d_entry.small),
                           sizeof(d->d_entry.small),
                           &numEntries,
                           FIL_STANDARD);
    }
    if ( rc != NO_ERROR ) {
		_PR_MD_MAP_OPENDIR_ERROR(rc);
        return PR_FAILURE;
    }
    d->firstEntry = PR_TRUE;
    d->magic = _MD_MAGIC_DIR;
    return PR_SUCCESS;
}

char *
_PR_MD_READ_DIR(_MDDir *d, PRIntn flags)
{
    PRUword numFiles = 1;
    BOOL rv;
    char *fileName;
    USHORT fileAttr;

    if ( d ) {
       while (1) {
           if (d->firstEntry) {
               d->firstEntry = PR_FALSE;
               rv = NO_ERROR;
           } else {
               rv = DosFindNext(d->d_hdl,
                                &(d->d_entry),
                                sizeof(d->d_entry),
                                &numFiles);
           }
           if (rv != NO_ERROR) {
               break;
           }
           fileName = GetFileFromDIR(d);
           fileAttr = GetFileAttr(d);
           if ( (flags & PR_SKIP_DOT) &&
                (fileName[0] == '.') && (fileName[1] == '\0'))
                continue;
           if ( (flags & PR_SKIP_DOT_DOT) &&
                (fileName[0] == '.') && (fileName[1] == '.') &&
                (fileName[2] == '\0'))
                continue;
			/*
			 * XXX
			 * Is this the correct definition of a hidden file on OS/2?
			 */
           if ((flags & PR_SKIP_NONE) && (fileAttr & FILE_HIDDEN))
                return fileName;
           else if ((flags & PR_SKIP_HIDDEN) && (fileAttr & FILE_HIDDEN))
                continue;
           return fileName;
        }
        PR_ASSERT(NO_ERROR != rv);
			_PR_MD_MAP_READDIR_ERROR(rv);
        return NULL;
		}
    PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
    return NULL;
}

PRInt32
_PR_MD_DELETE(const char *name)
{
    PRInt32 rc = DosDelete((char*)name);
    if(rc == NO_ERROR) {
        return 0;
    } else {
		_PR_MD_MAP_DELETE_ERROR(rc);
        return -1;
    }
}

PRInt32
_PR_MD_STAT(const char *fn, struct stat *info)
{
    PRInt32 rv;
    char filename[CCHMAXPATH];

    PR_snprintf(filename, CCHMAXPATH, "%s", fn);
    FlipSlashes(filename, strlen(filename));

    rv = _stat((char*)filename, info);
    if (-1 == rv) {
        /*
         * Check for MSVC runtime library _stat() bug.
         * (It's really a bug in FindFirstFile().)
         * If a pathname ends in a backslash or slash,
         * e.g., c:\temp\ or c:/temp/, _stat() will fail.
         * Note: a pathname ending in a slash (e.g., c:/temp/)
         * can be handled by _stat() on NT but not on Win95.
         *
         * We remove the backslash or slash at the end and
         * try again.  
         *
         * Not sure if this happens on OS/2 or not,
         * but it doesn't hurt to be careful.
         */

        int len = strlen(fn);
        if (len > 0 && len <= _MAX_PATH
                && (fn[len - 1] == '\\' || fn[len - 1] == '/')) {
            char newfn[_MAX_PATH + 1];

            strcpy(newfn, fn);
            newfn[len - 1] = '\0';
            rv = _stat(newfn, info);
        }
    }

    if (-1 == rv) {
        _PR_MD_MAP_STAT_ERROR(errno);
    }
    return rv;
}

PRInt32
_PR_MD_GETFILEINFO(const char *fn, PRFileInfo *info)
{
    struct stat sb;
    PRInt32 rv;
    PRInt64 s, s2us;
 
    if ( (rv = _PR_MD_STAT(fn, &sb)) == 0 ) {
        if (info) {
            if (S_IFREG & sb.st_mode)
                info->type = PR_FILE_FILE ;
            else if (S_IFDIR & sb.st_mode)
                info->type = PR_FILE_DIRECTORY;
            else
                info->type = PR_FILE_OTHER;
            info->size = sb.st_size;
            LL_I2L(s2us, PR_USEC_PER_SEC);
            LL_I2L(s, sb.st_mtime);
            LL_MUL(s, s, s2us);
            info->modifyTime = s;
            LL_I2L(s, sb.st_ctime);
            LL_MUL(s, s, s2us);
            info->creationTime = s;
        }
    }
    return rv;
}

PRInt32
_PR_MD_GETFILEINFO64(const char *fn, PRFileInfo64 *info)
{
    PRFileInfo info32;
    PRInt32 rv = _PR_MD_GETFILEINFO(fn, &info32);
    if (rv != 0)
    {
        return rv;
    }
    info->type = info32.type;
    LL_UI2L(info->size,info32.size);
    info->modifyTime = info32.modifyTime;
    info->creationTime = info32.creationTime;
    
    if (isWSEB)
    {
        APIRET rc ;
        FILESTATUS3L fstatus;

        rc = DosQueryPathInfo(fn, FIL_STANDARDL, &fstatus, sizeof(fstatus));

        if (NO_ERROR != rc)
        {
            _PR_MD_MAP_OPEN_ERROR(rc);
            return -1;
        }

        if (! (fstatus.attrFile & FILE_DIRECTORY))
        {
            info->size = fstatus.cbFile;
        }
    }

    return rv;
}

PRInt32
_PR_MD_GETOPENFILEINFO(const PRFileDesc *fd, PRFileInfo *info)
{
   /* For once, the VAC compiler/library did a nice thing.
    * The file handle used by the C runtime is the same one
    * returned by the OS when you call DosOpen().  This means
    * that you can take an OS HFILE and use it with C file
    * functions.  The only caveat is that you have to call
    * _setmode() first to initialize some junk.  This is
    * immensely useful because I did not have a clue how to
    * implement this function otherwise.  The windows folks
    * took the source from the Microsoft C library source, but
    * IBM wasn't kind enough to ship the source with VAC.
    * On second thought, the needed function could probably
    * be gotten from the OS/2 GNU library source, but the
    * point is now moot.
    */
   struct stat hinfo;
    PRInt64 s, s2us;

    _setmode(fd->secret->md.osfd, O_BINARY);
    if(fstat((int)fd->secret->md.osfd, &hinfo) != NO_ERROR) {
		_PR_MD_MAP_FSTAT_ERROR(errno);
        return -1;
	}

    if (hinfo.st_mode & S_IFDIR)
        info->type = PR_FILE_DIRECTORY;
    else
        info->type = PR_FILE_FILE;

    info->size = hinfo.st_size;
    LL_I2L(s2us, PR_USEC_PER_SEC);
    LL_I2L(s, hinfo.st_mtime);
    LL_MUL(s, s, s2us);
    info->modifyTime = s;
    LL_I2L(s, hinfo.st_ctime);
    LL_MUL(s, s, s2us);
    info->creationTime = s;

    return 0;
}

PRInt32
_PR_MD_GETOPENFILEINFO64(const PRFileDesc *fd, PRFileInfo64 *info)
{
    PRFileInfo info32;
    PRInt32 rv = _PR_MD_GETOPENFILEINFO(fd, &info32);
    if (0 == rv)
    {
       info->type = info32.type;
       LL_UI2L(info->size,info32.size);
    
       info->modifyTime = info32.modifyTime;
       info->creationTime = info32.creationTime;
    }
    
    if (isWSEB)
    {
        APIRET rc ;
        FILESTATUS3L fstatus;

        rc = DosQueryFileInfo(fd->secret->md.osfd, FIL_STANDARDL, &fstatus, sizeof(fstatus));

        if (NO_ERROR != rc)
        {
            _PR_MD_MAP_OPEN_ERROR(rc);
            return -1;
        }

        if (! (fstatus.attrFile & FILE_DIRECTORY))
        {
            info->size = fstatus.cbFile;
        }
    }

    return rv;
}


PRInt32
_PR_MD_RENAME(const char *from, const char *to)
{
   PRInt32 rc;
    /* Does this work with dot-relative pathnames? */
    if ( (rc = DosMove((char *)from, (char *)to)) == NO_ERROR) {
        return 0;
    } else {
		_PR_MD_MAP_RENAME_ERROR(rc);
        return -1;
    }
}

PRInt32
_PR_MD_ACCESS(const char *name, PRAccessHow how)
{
  PRInt32 rv;
    switch (how) {
      case PR_ACCESS_WRITE_OK:
        rv = access(name, 02);
		break;
      case PR_ACCESS_READ_OK:
        rv = access(name, 04);
		break;
      case PR_ACCESS_EXISTS:
        return access(name, 00);
	  	break;
      default:
		PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
		return -1;
    }
	if (rv < 0)
		_PR_MD_MAP_ACCESS_ERROR(errno);
    return rv;
}

PRInt32
_PR_MD_MKDIR(const char *name, PRIntn mode)
{
   PRInt32 rc;
    /* XXXMB - how to translate the "mode"??? */
    if ((rc = DosCreateDir((char *)name, NULL))== NO_ERROR) {
        return 0;
    } else {
		_PR_MD_MAP_MKDIR_ERROR(rc);
        return -1;
    }
}

PRInt32
_PR_MD_RMDIR(const char *name)
{
   PRInt32 rc;
    if ( (rc = DosDeleteDir((char *)name)) == NO_ERROR) {
        return 0;
    } else {
		_PR_MD_MAP_RMDIR_ERROR(rc);
        return -1;
    }
}

PRStatus
_PR_MD_LOCKFILE(PRInt32 f)
{
    PRInt32   rv;
    FILELOCK lock, unlock;
    FILELOCKL lockL, unlockL;
    
    lock.lOffset = 0;
    lockL.lOffset = 0;
    lock.lRange = 0xffffffff;
    lockL.lRange =  0xffffffffffffffff;
    unlock.lOffset = 0;
    unlock.lRange = 0;
    unlockL.lOffset = 0;
    unlockL.lRange = 0;

    /*
     * loop trying to DosSetFileLocks(),
     * pause for a few miliseconds when can't get the lock
     * and try again
     */
    for( rv = FALSE; rv == FALSE; /* do nothing */ )
    {
        if (isWSEB)
        {
	    rv = myDosSetFileLocksL( (HFILE) f,
			                    &unlockL, &lockL,
			                    0, 0);
        }
        else
        {
	    rv = DosSetFileLocks( (HFILE) f,
			                    &unlock, &lock,
			                    0, 0);
        }
		if ( rv != NO_ERROR )
        {
            DosSleep( 50 );  /* Sleep() a few milisecs and try again. */
        }            
    } /* end for() */
    return PR_SUCCESS;
} /* end _PR_MD_LOCKFILE() */

PRStatus
_PR_MD_TLOCKFILE(PRInt32 f)
{
    return _PR_MD_LOCKFILE(f);
} /* end _PR_MD_TLOCKFILE() */


PRStatus
_PR_MD_UNLOCKFILE(PRInt32 f)
{
    PRInt32   rv;
    FILELOCK lock, unlock;
    FILELOCKL lockL, unlockL;
    
    lock.lOffset = 0;
    lockL.lOffset = 0;
    lock.lRange = 0;
    lockL.lRange = 0;
    unlock.lOffset = 0;
    unlockL.lOffset = 0;
    unlock.lRange = 0xffffffff;
    unlockL.lRange = 0xffffffffffffffff;
    
    if (isWSEB)
    {
        rv = myDosSetFileLocksL( (HFILE) f,
                                        &unlockL, &lockL,
                                        0, 0);
    }
    else
    {
        rv = DosSetFileLocks( (HFILE) f,
                                    &unlock, &lock,
                                    0, 0);
    }
            
    if ( rv != NO_ERROR )
    {
        return PR_SUCCESS;
    }
    else
    {
        return PR_FAILURE;
    }
} /* end _PR_MD_UNLOCKFILE() */

PRStatus
_PR_MD_SET_FD_INHERITABLE(PRFileDesc *fd, PRBool inheritable)
{
    APIRET rc = 0;
    ULONG flags;
    switch (fd->methods->file_type)
    {
        case PR_DESC_PIPE:
        case PR_DESC_FILE:
            rc = DosQueryFHState((HFILE)fd->secret->md.osfd, &flags);
            if (rc != NO_ERROR) {
                PR_SetError(PR_UNKNOWN_ERROR, _MD_ERRNO());
                return PR_FAILURE;
            }

            if (inheritable)
              flags &= ~OPEN_FLAGS_NOINHERIT;
            else
              flags |= OPEN_FLAGS_NOINHERIT;

            /* Mask off flags DosSetFHState don't want. */
            flags &= (OPEN_FLAGS_WRITE_THROUGH | OPEN_FLAGS_FAIL_ON_ERROR | OPEN_FLAGS_NO_CACHE | OPEN_FLAGS_NOINHERIT);
            rc = DosSetFHState((HFILE)fd->secret->md.osfd, flags);
            if (rc != NO_ERROR) {
                PR_SetError(PR_UNKNOWN_ERROR, _MD_ERRNO());
                return PR_FAILURE;
            }
            break;

        case PR_DESC_LAYERED:
            /* what to do here? */
            PR_SetError(PR_UNKNOWN_ERROR, 87 /*ERROR_INVALID_PARAMETER*/);
            return PR_FAILURE;

        case PR_DESC_SOCKET_TCP:
        case PR_DESC_SOCKET_UDP:
            /* These are global on OS/2. */
            break;
    }

    return PR_SUCCESS;
}

void
_PR_MD_INIT_FD_INHERITABLE(PRFileDesc *fd, PRBool imported)
{
    /* XXX this function needs to be implemented */
    fd->secret->inheritable = _PR_TRI_UNKNOWN;
}

void
_PR_MD_QUERY_FD_INHERITABLE(PRFileDesc *fd)
{
    /* XXX this function needs to be reviewed */
    ULONG flags;

    PR_ASSERT(_PR_TRI_UNKNOWN == fd->secret->inheritable);
    if (DosQueryFHState((HFILE)fd->secret->md.osfd, &flags) == 0) {
        if (flags & OPEN_FLAGS_NOINHERIT) {
            fd->secret->inheritable = _PR_TRI_FALSE;
        } else {
            fd->secret->inheritable = _PR_TRI_TRUE;
        }
    }
}
