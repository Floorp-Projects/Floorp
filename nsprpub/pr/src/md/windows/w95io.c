/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 * 
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* Windows 95 IO module
 *
 * Assumes synchronous I/O.
 *
 */

#include "primpl.h"
#include <direct.h>

struct _MDLock               _pr_ioq_lock;

/*
 * The NSPR epoch (00:00:00 1 Jan 1970 UTC) in FILETIME.
 * We store the value in a PRTime variable for convenience.
 * This constant is used by _PR_FileTimeToPRTime().
 */
static const PRTime _pr_filetime_offset = 116444736000000000i64;

void
_PR_MD_INIT_IO()
{
    WORD WSAVersion = 0x0101;
    WSADATA WSAData;

    WSAStartup(	WSAVersion, &WSAData );

#ifdef DEBUG
    /* Doublecheck _pr_filetime_offset's hard-coded value is correct. */
    {
        SYSTEMTIME systime;
        union {
           PRTime prt;
           FILETIME ft;
        } filetime;
        BOOL rv;

        systime.wYear = 1970;
        systime.wMonth = 1;
        /* wDayOfWeek is ignored */
        systime.wDay = 1;
        systime.wHour = 0;
        systime.wMinute = 0;
        systime.wSecond = 0;
        systime.wMilliseconds = 0;

        rv = SystemTimeToFileTime(&systime, &filetime.ft);
        PR_ASSERT(0 != rv);
        PR_ASSERT(filetime.prt == _pr_filetime_offset);
    }
#endif /* DEBUG */
}

PRStatus
_PR_MD_WAIT(PRThread *thread, PRIntervalTime ticks)
{
    DWORD rv;

    PRUint32 msecs = (ticks == PR_INTERVAL_NO_TIMEOUT) ?
        INFINITE : PR_IntervalToMilliseconds(ticks);
    rv = WaitForSingleObject(thread->md.blocked_sema, msecs);
    switch(rv) 
    {
        case WAIT_OBJECT_0:
            return PR_SUCCESS;
            break;
        case WAIT_TIMEOUT:
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
                     * call WaitForSingleObject() to clear the semaphore.
                     */
                    _PR_THREAD_UNLOCK(thread);
                    rv = WaitForSingleObject(thread->md.blocked_sema, 0);
                    PR_ASSERT(rv == WAIT_OBJECT_0);
                }
            }
            return PR_SUCCESS;
            break;
        default:
            return PR_FAILURE;
            break;
    }
}
PRStatus
_PR_MD_WAKEUP_WAITER(PRThread *thread)
{
    if ( _PR_IS_NATIVE_THREAD(thread) ) 
    {
        if (ReleaseSemaphore(thread->md.blocked_sema, 1, NULL) == FALSE)
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
 *  The NSPR open flags (osflags) are translated into flags for Win95
 *
 *  Mode seems to be passed in as a unix style file permissions argument
 *  as in 0666, in the case of opening the logFile. 
 *
 */
PRInt32
_PR_MD_OPEN(const char *name, PRIntn osflags, int mode)
{
    HANDLE file;
    PRInt32 access = 0;
    PRInt32 flags = 0;
    PRInt32 flag6 = 0;
    
    if (osflags & PR_SYNC) flag6 = FILE_FLAG_WRITE_THROUGH;
 
    if (osflags & PR_RDONLY || osflags & PR_RDWR)
        access |= GENERIC_READ;
    if (osflags & PR_WRONLY || osflags & PR_RDWR)
        access |= GENERIC_WRITE;
    if (osflags & PR_CREATE_FILE) {
        if (osflags & PR_TRUNCATE)
            flags = CREATE_ALWAYS;
        else
            flags = OPEN_ALWAYS;
    } else {
        if (osflags & PR_TRUNCATE)
            flags = TRUNCATE_EXISTING;
        else
            flags = OPEN_EXISTING;
    }

    file = CreateFile(name,
                      access,
                      FILE_SHARE_READ|FILE_SHARE_WRITE,
                      NULL,
                      flags,
                      flag6,
                      NULL);
    if (file == INVALID_HANDLE_VALUE) {
		_PR_MD_MAP_OPEN_ERROR(GetLastError());
        return -1; 
	}

    return (PRInt32)file;
}

PRInt32
_PR_MD_READ(PRFileDesc *fd, void *buf, PRInt32 len)
{
    PRUint32 bytes;
    int rv, err;

    rv = ReadFile((HANDLE)fd->secret->md.osfd,
            (LPVOID)buf,
            len,
            &bytes,
            NULL);
    
    if (rv == 0) 
    {
        err = GetLastError();
        /* ERROR_HANDLE_EOF can only be returned by async io */
        PR_ASSERT(err != ERROR_HANDLE_EOF);
        if (err == ERROR_BROKEN_PIPE)
            return 0;
		else {
			_PR_MD_MAP_READ_ERROR(err);
        return -1;
    }
    }
    return bytes;
}

PRInt32
_PR_MD_WRITE(PRFileDesc *fd, void *buf, PRInt32 len)
{
    PRInt32 f = fd->secret->md.osfd;
    PRInt32 bytes;
    int rv;
    PRThread *me = _PR_MD_CURRENT_THREAD();
    
    rv = WriteFile((HANDLE)f,
            buf,
            len,
            &bytes,
            NULL );
            
    if (rv == 0) 
    {
		_PR_MD_MAP_WRITE_ERROR(GetLastError());
        return -1;
    }
    return bytes;
} /* --- end _PR_MD_WRITE() --- */

PRInt32
_PR_MD_LSEEK(PRFileDesc *fd, PRInt32 offset, int whence)
{
    PRInt32 rv;

    rv = SetFilePointer((HANDLE)fd->secret->md.osfd, offset, 0, whence);

	/*
	 * If the lpDistanceToMoveHigh argument (third argument) is
	 * NULL, SetFilePointer returns 0xffffffff on failure.
	 */
	if (-1 == rv) {
		_PR_MD_MAP_LSEEK_ERROR(GetLastError());
		return -1;
	} else
		return rv;
}

PRInt64
_PR_MD_LSEEK64(PRFileDesc *fd, PRInt64 offset, int whence)
{
    PRInt64 result;
    PRInt32 rv, low = (PRInt32)offset, hi = (PRInt32)(offset >> 32);

    rv = SetFilePointer((HANDLE)fd->secret->md.osfd, low, &hi, whence);

	/*
	 * If the lpDistanceToMoveHigh argument (third argument) is
	 * NULL, SetFilePointer returns 0xffffffff on failure.
	 */
	if (-1 == rv)
    {
		_PR_MD_MAP_LSEEK_ERROR(GetLastError());
        return -1;
    }

    result = (hi << 32) + rv;
	return result;
}

/*
 * This is documented to succeed on read-only files, but Win32's
 * FlushFileBuffers functions fails with "access denied" in such a
 * case.  So we only signal an error if the error is *not* "access
 * denied".
 */
PRInt32
_PR_MD_FSYNC(PRFileDesc *fd)
{
    /*
     * From the documentation:
     *
     *	   On Windows NT, the function FlushFileBuffers fails if hFile
     *	   is a handle to console output. That is because console
     *	   output is not buffered. The function returns FALSE, and
     *	   GetLastError returns ERROR_INVALID_HANDLE.
     *
     * On the other hand, on Win95, it returns without error.  I cannot
     * assume that 0, 1, and 2 are console, because if someone closes
     * System.out and then opens a file, they might get file descriptor
     * 1.  An error on *that* version of 1 should be reported, whereas
     * an error on System.out (which was the original 1) should be
     * ignored.  So I use isatty() to ensure that such an error was due
     * to this bogosity, and if it was, I ignore the error.
     */

    BOOL ok = FlushFileBuffers((HANDLE)fd->secret->md.osfd);

    if (!ok) {
	DWORD err = GetLastError();
	if (err != ERROR_ACCESS_DENIED) {	// from winerror.h
			_PR_MD_MAP_FSYNC_ERROR(err);
	    return -1;
	}
    }
    return 0;
}

PRInt32
_MD_CloseFile(PRInt32 osfd)
{
    PRInt32 rv;
    
    rv = (CloseHandle((HANDLE)osfd))?0:-1;
	if (rv == -1)
		_PR_MD_MAP_CLOSE_ERROR(GetLastError());
    return rv;
}


/* --- DIR IO ------------------------------------------------------------ */
#define GetFileFromDIR(d)       (d)->d_entry.cFileName
#define FileIsHidden(d)	((d)->d_entry.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)

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
** by the VC RTL.
**
*/

PRStatus
_PR_MD_CLOSE_DIR(_MDDir *d)
{
    if ( d ) {
        if (FindClose(d->d_hdl)) {
        d->magic = (PRUint32)-1;
        return PR_SUCCESS;
		} else {
			_PR_MD_MAP_CLOSEDIR_ERROR(GetLastError());
        	return PR_FAILURE;
		}
    }
    PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
    return PR_FAILURE;
}


PRStatus
_PR_MD_OPEN_DIR(_MDDir *d, const char *name)
{
    char filename[ MAX_PATH ];
    int len;

    len = strlen(name);
    /* Need 5 bytes for \*.* and the trailing null byte. */
    if (len + 5 > MAX_PATH) {
        PR_SetError(PR_NAME_TOO_LONG_ERROR, 0);
        return PR_FAILURE;
    }
    strcpy(filename, name);

    /*
     * If 'name' ends in a slash or backslash, do not append
     * another backslash.
     */
    if (filename[len - 1] == '/' || filename[len - 1] == '\\') {
        len--;
    }
    strcpy(&filename[len], "\\*.*");
    FlipSlashes( filename, strlen(filename) );

    d->d_hdl = FindFirstFile( filename, &(d->d_entry) );
    if ( d->d_hdl == INVALID_HANDLE_VALUE ) {
		_PR_MD_MAP_OPENDIR_ERROR(GetLastError());
        return PR_FAILURE;
    }
    d->firstEntry = PR_TRUE;
    d->magic = _MD_MAGIC_DIR;
    return PR_SUCCESS;
}

char *
_PR_MD_READ_DIR(_MDDir *d, PRIntn flags)
{
    PRInt32 err;
    BOOL rv;
    char *fileName;

    if ( d ) {
        while (1) {
            if (d->firstEntry) {
                d->firstEntry = PR_FALSE;
                rv = 1;
            } else {
                rv = FindNextFile(d->d_hdl, &(d->d_entry));
            }
            if (rv == 0) {
                break;
            }
            fileName = GetFileFromDIR(d);
            if ( (flags & PR_SKIP_DOT) &&
                 (fileName[0] == '.') && (fileName[1] == '\0'))
                 continue;
            if ( (flags & PR_SKIP_DOT_DOT) &&
                 (fileName[0] == '.') && (fileName[1] == '.') &&
                 (fileName[2] == '\0'))
                 continue;
            if ( (flags & PR_SKIP_HIDDEN) && FileIsHidden(d))
                 continue;
            return fileName;
        }
        err = GetLastError();
        PR_ASSERT(NO_ERROR != err);
			_PR_MD_MAP_READDIR_ERROR(err);
        return NULL;
		}
    PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
    return NULL;
}

PRInt32
_PR_MD_DELETE(const char *name)
{
    if (DeleteFile(name)) {
        return 0;
    } else {
		_PR_MD_MAP_DELETE_ERROR(GetLastError());
        return -1;
    }
}

static void
_PR_FileTimeToPRTime(const FILETIME *filetime, PRTime *prtm)
{
    PR_ASSERT(sizeof(FILETIME) == sizeof(PRTime));
    CopyMemory(prtm, filetime, sizeof(PRTime));
    *prtm = (*prtm - _pr_filetime_offset) / 10i64;

#ifdef DEBUG
    /* Doublecheck our calculation. */
    {
        SYSTEMTIME systime;
        PRExplodedTime etm;
        PRTime cmp; /* for comparison */
        BOOL rv;

        rv = FileTimeToSystemTime(filetime, &systime);
        PR_ASSERT(0 != rv);

        /*
         * PR_ImplodeTime ignores wday and yday.
         */
        etm.tm_usec = systime.wMilliseconds * PR_USEC_PER_MSEC;
        etm.tm_sec = systime.wSecond;
        etm.tm_min = systime.wMinute;
        etm.tm_hour = systime.wHour;
        etm.tm_mday = systime.wDay;
        etm.tm_month = systime.wMonth - 1;
        etm.tm_year = systime.wYear;
        /*
         * It is not well-documented what time zone the FILETIME's
         * are in.  WIN32_FIND_DATA is documented to be in UTC (GMT).
         * But BY_HANDLE_FILE_INFORMATION is unclear about this.
         * By our best judgement, we assume that FILETIME is in UTC.
         */
        etm.tm_params.tp_gmt_offset = 0;
        etm.tm_params.tp_dst_offset = 0;
        cmp = PR_ImplodeTime(&etm);

        /*
         * SYSTEMTIME is in milliseconds precision, so we convert PRTime's
         * microseconds to milliseconds before doing the comparison.
         */
        PR_ASSERT((cmp / PR_USEC_PER_MSEC) == (*prtm / PR_USEC_PER_MSEC));
    }
#endif /* DEBUG */
}

PRInt32
_PR_MD_STAT(const char *fn, struct stat *info)
{
    PRInt32 rv;

    rv = _stat(fn, (struct _stat *)info);
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
         */

        int len = strlen(fn);
        if (len > 0 && len <= _MAX_PATH
                && (fn[len - 1] == '\\' || fn[len - 1] == '/')) {
            char newfn[_MAX_PATH + 1];

            strcpy(newfn, fn);
            newfn[len - 1] = '\0';
            rv = _stat(newfn, (struct _stat *)info);
        }
    }

    if (-1 == rv) {
        _PR_MD_MAP_STAT_ERROR(errno);
    }
    return rv;
}

#define _PR_IS_SLASH(ch) ((ch) == '/' || (ch) == '\\')

/*
 * IsRootDirectory --
 *
 * Return PR_TRUE if the pathname 'fn' is a valid root directory,
 * else return PR_FALSE.  The char buffer pointed to by 'fn' must
 * be writable.  During the execution of this function, the contents
 * of the buffer pointed to by 'fn' may be modified, but on return
 * the original contents will be restored.  'buflen' is the size of
 * the buffer pointed to by 'fn'.
 *
 * Root directories come in three formats:
 * 1. / or \, meaning the root directory of the current drive.
 * 2. C:/ or C:\, where C is a drive letter.
 * 3. \\<server name>\<share point name>\ or
 *    \\<server name>\<share point name>, meaning the root directory
 *    of a UNC (Universal Naming Convention) name.
 */

static PRBool
IsRootDirectory(char *fn, size_t buflen)
{
    char *p;
    PRBool slashAdded = PR_FALSE;
    PRBool rv = PR_FALSE;

    if (_PR_IS_SLASH(fn[0]) && fn[1] == '\0') {
        return PR_TRUE;
    }

    if (isalpha(fn[0]) && fn[1] == ':' && _PR_IS_SLASH(fn[2])
            && fn[3] == '\0') {
        rv = GetDriveType(fn) > 1 ? PR_TRUE : PR_FALSE;
        return rv;
    }

    /* The UNC root directory */

    if (_PR_IS_SLASH(fn[0]) && _PR_IS_SLASH(fn[1])) {
        /* The 'server' part should have at least one character. */
        p = &fn[2];
        if (*p == '\0' || _PR_IS_SLASH(*p)) {
            return PR_FALSE;
        }

        /* look for the next slash */
        do {
            p++;
        } while (*p != '\0' && !_PR_IS_SLASH(*p));
        if (*p == '\0') {
            return PR_FALSE;
        }

        /* The 'share' part should have at least one character. */
        p++;
        if (*p == '\0' || _PR_IS_SLASH(*p)) {
            return PR_FALSE;
        }

        /* look for the final slash */
        do {
            p++;
        } while (*p != '\0' && !_PR_IS_SLASH(*p));
        if (_PR_IS_SLASH(*p) && p[1] != '\0') {
            return PR_FALSE;
        }
        if (*p == '\0') {
            /*
             * GetDriveType() doesn't work correctly if the
             * path is of the form \\server\share, so we add
             * a final slash temporarily.
             */
            if ((p + 1) < (fn + buflen)) {
                *p++ = '\\';
                *p = '\0';
                slashAdded = PR_TRUE;
            } else {
                return PR_FALSE; /* name too long */
            }
        }
        rv = GetDriveType(fn) > 1 ? PR_TRUE : PR_FALSE;
        /* restore the 'fn' buffer */
        if (slashAdded) {
            *--p = '\0';
        }
    }
    return rv;
}

PRInt32
_PR_MD_GETFILEINFO64(const char *fn, PRFileInfo64 *info)
{
    HANDLE hFindFile;
    WIN32_FIND_DATA findFileData;
    char pathbuf[MAX_PATH + 1];
    
    if (NULL == fn || '\0' == *fn) {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return -1;
    }

    /*
     * FindFirstFile() expands wildcard characters.  So
     * we make sure the pathname contains no wildcard.
     */
    if (NULL != strpbrk(fn, "?*")) {
        PR_SetError(PR_FILE_NOT_FOUND_ERROR, 0);
        return -1;
    }

    hFindFile = FindFirstFile(fn, &findFileData);
    if (INVALID_HANDLE_VALUE == hFindFile) {
        DWORD len;
        char *filePart;

        /*
         * FindFirstFile() does not work correctly on root directories.
         * It also doesn't work correctly on a pathname that ends in a
         * slash.  So we first check to see if the pathname specifies a
         * root directory.  If not, and if the pathname ends in a slash,
         * we remove the final slash and try again.
         */

        /*
         * If the pathname does not contain ., \, and /, it cannot be
         * a root directory or a pathname that ends in a slash.
         */
        if (NULL == strpbrk(fn, ".\\/")) {
            _PR_MD_MAP_OPENDIR_ERROR(GetLastError());
            return -1;
        } 
        len = GetFullPathName(fn, sizeof(pathbuf), pathbuf,
                &filePart);
        PR_ASSERT(0 != len);
        if (len > sizeof(pathbuf)) {
            PR_SetError(PR_NAME_TOO_LONG_ERROR, 0);
            return -1;
        }
        if (IsRootDirectory(pathbuf, sizeof(pathbuf))) {
            info->type = PR_FILE_DIRECTORY;
            info->size = 0;
            /*
             * These timestamps don't make sense for root directories.
             */
            info->modifyTime = 0;
            info->creationTime = 0;
            return 0;
        }
        if (!_PR_IS_SLASH(pathbuf[len - 1])) {
            _PR_MD_MAP_OPENDIR_ERROR(GetLastError());
            return -1;
        } else {
            pathbuf[len - 1] = '\0';
            hFindFile = FindFirstFile(pathbuf, &findFileData);
            if (INVALID_HANDLE_VALUE == hFindFile) {
                _PR_MD_MAP_OPENDIR_ERROR(GetLastError());
                return -1;
            }
        }
    }

    FindClose(hFindFile);

    if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        info->type = PR_FILE_DIRECTORY;
    } else {
        info->type = PR_FILE_FILE;
    }

    info->size = findFileData.nFileSizeHigh;
    info->size = (info->size << 32) + findFileData.nFileSizeLow;

    _PR_FileTimeToPRTime(&findFileData.ftLastWriteTime, &info->modifyTime);

    if (0 == findFileData.ftCreationTime.dwLowDateTime &&
            0 == findFileData.ftCreationTime.dwHighDateTime) {
        info->creationTime = info->modifyTime;
    } else {
        _PR_FileTimeToPRTime(&findFileData.ftCreationTime,
                &info->creationTime);
    }

    return 0;
}

PRInt32
_PR_MD_GETFILEINFO(const char *fn, PRFileInfo *info)
{
    PRFileInfo64 info64;
    PRInt32 rv = _PR_MD_GETFILEINFO64(fn, &info64);
    if (0 == rv)
    {
        info->type = info64.type;
        info->size = (PRUint32) info64.size;
        info->modifyTime = info64.modifyTime;
        info->creationTime = info64.creationTime;
    }
    return rv;
}

PRInt32
_PR_MD_GETOPENFILEINFO64(const PRFileDesc *fd, PRFileInfo64 *info)
{
    int rv;

    BY_HANDLE_FILE_INFORMATION hinfo;

    rv = GetFileInformationByHandle((HANDLE)fd->secret->md.osfd, &hinfo);
    if (rv == FALSE) {
		_PR_MD_MAP_FSTAT_ERROR(GetLastError());
        return -1;
	}

    if (hinfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        info->type = PR_FILE_DIRECTORY;
    else
        info->type = PR_FILE_FILE;

    info->size = hinfo.nFileSizeHigh;
    info->size = (info->size << 32) + hinfo.nFileSizeLow;

    _PR_FileTimeToPRTime(&hinfo.ftLastWriteTime, &(info->modifyTime) );
    _PR_FileTimeToPRTime(&hinfo.ftCreationTime, &(info->creationTime) );

    return 0;
}

PRInt32
_PR_MD_GETOPENFILEINFO(const PRFileDesc *fd, PRFileInfo *info)
{
    PRFileInfo64 info64;
    int rv = _PR_MD_GETOPENFILEINFO64(fd, &info64);
    if (0 == rv)
    {
        info->type = info64.type;
        info->modifyTime = info64.modifyTime;
        info->creationTime = info64.creationTime;
        LL_L2I(info->size, info64.size);
    }
    return rv;
}

PRStatus
_PR_MD_SET_FD_INHERITABLE(PRFileDesc *fd, PRBool inheritable)
{
    BOOL rv;

    /*
     * The SetHandleInformation function fails with the
     * ERROR_CALL_NOT_IMPLEMENTED error on Win95.
     */
    rv = SetHandleInformation(
            (HANDLE)fd->secret->md.osfd,
            HANDLE_FLAG_INHERIT,
            inheritable ? HANDLE_FLAG_INHERIT : 0);
    if (0 == rv) {
        PR_SetError(PR_UNKNOWN_ERROR, GetLastError());
        return PR_FAILURE;
    }
    return PR_SUCCESS;
} 

PRInt32
_PR_MD_RENAME(const char *from, const char *to)
{
    /* Does this work with dot-relative pathnames? */
    if (MoveFile(from, to)) {
        return 0;
    } else {
		_PR_MD_MAP_RENAME_ERROR(GetLastError());
        return -1;
    }
}

PRInt32
_PR_MD_ACCESS(const char *name, PRIntn how)
{
PRInt32 rv;
    switch (how) {
      case PR_ACCESS_WRITE_OK:
        rv = _access(name, 02);
		break;
      case PR_ACCESS_READ_OK:
        rv = _access(name, 04);
		break;
      case PR_ACCESS_EXISTS:
        return _access(name, 00);
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
    /* XXXMB - how to translate the "mode"??? */
    if (CreateDirectory(name, NULL)) {
        return 0;
    } else {
		_PR_MD_MAP_MKDIR_ERROR(GetLastError());
        return -1;
    }
}

PRInt32
_PR_MD_RMDIR(const char *name)
{
    if (RemoveDirectory(name)) {
        return 0;
    } else {
		_PR_MD_MAP_RMDIR_ERROR(GetLastError());
        return -1;
    }
}

PRStatus
_PR_MD_LOCKFILE(PRInt32 f)
{
	PRInt32   rv;

	/*
     * loop trying to LockFile(),
     * pause for a few miliseconds when can't get the lock
     * and try again
     */
    for( rv = FALSE; rv == FALSE; /* do nothing */ )
    {
    
	    rv = LockFile( (HANDLE) f,
			0l, 0l,
			0x7fffffff, 0xffffffff ); 
		if ( rv == FALSE )
        {
        	DWORD rc = GetLastError();
            Sleep( 50 );  // Sleep() a few milisecs and try again.
        }            
    } /* end for() */
    return PR_SUCCESS;
} /* end _PR_MD_LOCKFILE() */

PRStatus
_PR_MD_TLOCKFILE(PRInt32 f)
{
	PRInt32   rv;
    
	/*
     * loop trying to LockFile(),
     * pause for a few miliseconds when can't get the lock
     * and try again
     */
    for( rv = FALSE; rv == FALSE; /* do nothing */ )
    {
    
	    rv = LockFile( (HANDLE) f,
			0l, 0l,
			0x7fffffff, 0xffffffff ); 
		if ( rv == FALSE )
        {
        	DWORD rc = GetLastError();
            Sleep( 50 );  // Sleep() a few milisecs and try again.
        }            
    } /* end for() */
    return PR_SUCCESS;
} /* end _PR_MD_TLOCKFILE() */


PRStatus
_PR_MD_UNLOCKFILE(PRInt32 f)
{
	PRInt32   rv;
    
    rv = UnlockFile( (HANDLE) f,
    		0l, 0l,
            0x7fffffff, 0xffffffff ); 
            
    if ( rv )
    {
    	return PR_SUCCESS;
    }
    else
    {
    	int err = GetLastError();
		return PR_FAILURE;
    }
} /* end _PR_MD_UNLOCKFILE() */

