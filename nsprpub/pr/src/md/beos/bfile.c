/* -*- Mode: C++; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "MPL"); you may not use this file except in
 * compliance with the MPL.  You may obtain a copy of the MPL at
 * http:// www.mozilla.org/MPL/
 *
 * Software distributed under the MPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the MPL
 * for the specific language governing rights and limitations under the
 * MPL.
 */

#include "primpl.h"

/*
** Global lock variable used to bracket calls into rusty libraries that
** aren't thread safe (like libc, libX, etc).
*/
static PRLock *_pr_rename_lock = NULL; 

void
_MD_InitIO (void)
{
}

PRStatus
_MD_open_dir (_MDDir *md,const char *name)
{
int err;

	md->d = opendir(name);
	if (!md->d) {
		err = _MD_ERRNO();
		_PR_MD_MAP_OPENDIR_ERROR(err);
		return PR_FAILURE;
	}
	return PR_SUCCESS;
}

char*
_MD_read_dir (_MDDir *md, PRIntn flags)
{
struct dirent *de;
int err;

	for (;;) {
		/*
		 * XXX: readdir() is not MT-safe
		 */
		de = readdir(md->d);

		if (!de) {
			err = _MD_ERRNO();
			_PR_MD_MAP_READDIR_ERROR(err);
			return 0;
		}

		if ((flags & PR_SKIP_DOT) &&
		    (de->d_name[0] == '.') && (de->d_name[1] == 0))
			continue;

		if ((flags & PR_SKIP_DOT_DOT) &&
		    (de->d_name[0] == '.') && (de->d_name[1] == '.') &&
		    (de->d_name[2] == 0))
			continue;

		if ((flags & PR_SKIP_HIDDEN) && (de->d_name[1] == '.'))
			continue;

		break;
	}
	return de->d_name;
}


PRInt32
_MD_close_dir (_MDDir *md)
{
int rv = 0, err;

	if (md->d) {
		rv = closedir(md->d);
		if (rv == -1) {
			err = _MD_ERRNO();
			_PR_MD_MAP_CLOSEDIR_ERROR(err);
		}
	}
	return(rv);
}

void
_MD_make_nonblock (PRFileDesc *fd)
{
	int blocking = 1;
	setsockopt(fd->secret->md.osfd, SOL_SOCKET, SO_NONBLOCK, &blocking, sizeof(blocking));

}

PRInt32
_MD_open (const char *name, PRIntn flags, PRIntn mode)
{
	PRInt32 osflags;
	PRInt32 rv, err;

	if (flags & PR_RDWR) {
		osflags = O_RDWR;
	} else if (flags & PR_WRONLY) {
		osflags = O_WRONLY;
	} else {
		osflags = O_RDONLY;
	}

        if (flags & PR_APPEND)
                osflags |= O_APPEND;
        if (flags & PR_TRUNCATE)
                osflags |= O_TRUNC;
        if (flags & PR_SYNC) {
/* Ummmm.  BeOS doesn't appear to
   support sync in any way shape or
   form. */
		return PR_NOT_IMPLEMENTED_ERROR;
        }

	/*
	** On creations we hold the 'create' lock in order to enforce
	** the semantics of PR_Rename. (see the latter for more details)
	*/
	if (flags & PR_CREATE_FILE)
	{
		osflags |= O_CREAT ;
		if (NULL !=_pr_rename_lock)
		    PR_Lock(_pr_rename_lock);
	}

        rv = open(name, osflags, mode);

        if (rv < 0) {
                err = _MD_ERRNO();
                _PR_MD_MAP_OPEN_ERROR(err);
        }                                                                      

    if ((flags & PR_CREATE_FILE) && (NULL !=_pr_rename_lock))
        PR_Unlock(_pr_rename_lock);
        return rv;
}

PRInt32
_MD_close_file (PRInt32 osfd)
{
PRInt32 rv, err;

	rv = close(osfd);
	if (rv == -1) {
		err = _MD_ERRNO();
		_PR_MD_MAP_CLOSE_ERROR(err);
	}
	return(rv);
}

PRInt32
_MD_read (PRFileDesc *fd, void *buf, PRInt32 amount)
{
    PRInt32 rv, err;
    PRInt32 osfd = fd->secret->md.osfd;

    rv = read( osfd, buf, amount );
    if (rv < 0) {
	err = _MD_ERRNO();
	_PR_MD_MAP_READ_ERROR(err);
    }
    return(rv);
}

PRInt32
_MD_write (PRFileDesc *fd, const void *buf, PRInt32 amount)
{
    PRInt32 rv, err;
    PRInt32 osfd = fd->secret->md.osfd;

    rv = write( osfd, buf, amount );

    if( rv < 0 ) {

	err = _MD_ERRNO();
	_PR_MD_MAP_WRITE_ERROR(err);
    }
    return( rv );
}

PRInt32
_MD_writev (PRFileDesc *fd, struct PRIOVec *iov, PRInt32 iov_size,
	    PRIntervalTime timeout)
{
    return PR_NOT_IMPLEMENTED_ERROR;
}

PRInt32
_MD_lseek (PRFileDesc *fd, PRInt32 offset, int whence)
{
PRInt32 rv, err;

    rv = lseek (fd->secret->md.osfd, offset, whence);
    if (rv == -1) {
        err = _MD_ERRNO();
	_PR_MD_MAP_LSEEK_ERROR(err);
    }
    return( rv );
}

PRInt64
_MD_lseek64 (PRFileDesc *fd, PRInt64 offset, int whence)
{
PRInt32 rv, err;

/* According to the BeOS headers, lseek accepts a
 * variable of type off_t for the offset, and off_t
 * is defined to be a 64-bit value.  So no special
 * cracking needs to be done on "offset".
 */

    rv = lseek (fd->secret->md.osfd, offset, whence);
    if (rv == -1) {
        err = _MD_ERRNO();
	_PR_MD_MAP_LSEEK_ERROR(err);
    }
    return( rv );
}

PRInt32
_MD_fsync (PRFileDesc *fd)
{
PRInt32 rv, err;

    rv = fsync(fd->secret->md.osfd);
    if (rv == -1) {
	err = _MD_ERRNO();
	_PR_MD_MAP_FSYNC_ERROR(err);
    }
    return(rv);
}

PRInt32
_MD_delete (const char *name)
{
PRInt32 rv, err;

    rv = unlink(name);
    if (rv == -1)
    {
	err = _MD_ERRNO();
        _PR_MD_MAP_UNLINK_ERROR(err);
    }
    return (rv);
}

PRInt32
_MD_getfileinfo (const char *fn, PRFileInfo *info)
{
struct stat sb;
PRInt32 rv, err;
PRInt64 s, s2us;

	rv = stat(fn, &sb);
	if (rv < 0) {
		err = _MD_ERRNO();
		_PR_MD_MAP_STAT_ERROR(err);
	} else if (info) {
		if (S_IFREG & sb.st_mode)
			info->type = PR_FILE_FILE;
		else if (S_IFDIR & sb.st_mode)
			info->type = PR_FILE_DIRECTORY;
		else
			info->type = PR_FILE_OTHER;

		/* Must truncate file size for the 32 bit
		   version */
		info->size = (sb.st_size & 0xffffffff);
		LL_I2L(s, sb.st_mtime);
		LL_I2L(s2us, PR_USEC_PER_SEC);
		LL_MUL(s, s, s2us);
		info->modifyTime = s;
		LL_I2L(s, sb.st_ctime);
		LL_MUL(s, s, s2us);
		info->creationTime = s;
	}
	return rv;
}

PRInt32
_MD_getfileinfo64 (const char *fn, PRFileInfo64 *info)
{
struct stat sb;
PRInt32 rv, err;
PRInt64 s, s2us;

	rv = stat(fn, &sb);
	if (rv < 0) {
		err = _MD_ERRNO();
		_PR_MD_MAP_STAT_ERROR(err);
	} else if (info) {
		if (S_IFREG & sb.st_mode)
			info->type = PR_FILE_FILE;
		else if (S_IFDIR & sb.st_mode)
			info->type = PR_FILE_DIRECTORY;
		else
			info->type = PR_FILE_OTHER;
	
		/* For the 64 bit version we can use
		 * the native st_size without modification
		 */
		info->size = sb.st_size;
		LL_I2L(s, sb.st_mtime);
		LL_I2L(s2us, PR_USEC_PER_SEC);
		LL_MUL(s, s, s2us);
		info->modifyTime = s;
		LL_I2L(s, sb.st_ctime);
		LL_MUL(s, s, s2us);
		info->creationTime = s;
	}
	return rv;
}

PRInt32
_MD_getopenfileinfo (const PRFileDesc *fd, PRFileInfo *info)
{
        struct stat sb;
        PRInt64 s, s2us;
        PRInt32 rv, err;

        rv = fstat(fd->secret->md.osfd, &sb);
        if (rv < 0) {
                        err = _MD_ERRNO();
                        _PR_MD_MAP_FSTAT_ERROR(err);
        } else if (info) {
                if (info) {
                        if (S_IFREG & sb.st_mode)
                                info->type = PR_FILE_FILE ;
                        else if (S_IFDIR & sb.st_mode)
                                info->type = PR_FILE_DIRECTORY;
                        else
                                info->type = PR_FILE_OTHER;
			/* Use lower 32 bits of file size */
                        info->size = ( sb.st_size & 0xffffffff);
                        LL_I2L(s, sb.st_mtime);
                        LL_I2L(s2us, PR_USEC_PER_SEC);
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
_MD_getopenfileinfo64 (const PRFileDesc *fd, PRFileInfo64 *info)
{
        struct stat sb;
        PRInt64 s, s2us;
        PRInt32 rv, err;

        rv = fstat(fd->secret->md.osfd, &sb);
        if (rv < 0) {
                        err = _MD_ERRNO();
                        _PR_MD_MAP_FSTAT_ERROR(err);
        } else if (info) {
                if (info) {
                        if (S_IFREG & sb.st_mode)
                                info->type = PR_FILE_FILE ;
                        else if (S_IFDIR & sb.st_mode)
                                info->type = PR_FILE_DIRECTORY;
                        else
                                info->type = PR_FILE_OTHER;
                        info->size = sb.st_size;
                        LL_I2L(s, sb.st_mtime);
                        LL_I2L(s2us, PR_USEC_PER_SEC);
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
_MD_rename (const char *from, const char *to)
{
    PRInt32 rv = -1, err;

    /*
    ** This is trying to enforce the semantics of WINDOZE' rename
    ** operation. That means one is not allowed to rename over top
    ** of an existing file. Holding a lock across these two function
    ** and the open function is known to be a bad idea, but ....
    */
    if (NULL != _pr_rename_lock)
        PR_Lock(_pr_rename_lock);
    if (0 == access(to, F_OK))
        PR_SetError(PR_FILE_EXISTS_ERROR, 0);
    else
    {
            rv = rename(from, to);
            if (rv < 0) {
                    err = _MD_ERRNO();
                    _PR_MD_MAP_RENAME_ERROR(err);
            }
    }
    if (NULL != _pr_rename_lock)
        PR_Unlock(_pr_rename_lock);
    return rv; 
}

PRInt32
_MD_access (const char *name, PRIntn how)
{
PRInt32 rv, err;
int amode;

	switch (how) {
		case PR_ACCESS_WRITE_OK:
			amode = W_OK;
			break;
		case PR_ACCESS_READ_OK:
			amode = R_OK;
			break;
		case PR_ACCESS_EXISTS:
			amode = F_OK;
			break;
		default:
			PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
			rv = -1;
			goto done;
	}
	rv = access(name, amode);

	if (rv < 0) {
		err = _MD_ERRNO();
		_PR_MD_MAP_ACCESS_ERROR(err);
	}

done:
	return(rv);
}

PRInt32
_MD_stat (const char *name, struct stat *buf)
{
    return PR_NOT_IMPLEMENTED_ERROR;
}

PRInt32
_MD_mkdir (const char *name, PRIntn mode)
{
    status_t rv;
    int err;

    /*
    ** This lock is used to enforce rename semantics as described
    ** in PR_Rename. Look there for more fun details.
    */
    if (NULL !=_pr_rename_lock)
        PR_Lock(_pr_rename_lock);

    rv = mkdir(name, mode);
    if (rv < 0) {
	err = _MD_ERRNO();
	_PR_MD_MAP_MKDIR_ERROR(err);
    }
    if (NULL !=_pr_rename_lock)
        PR_Unlock(_pr_rename_lock);
    return rv; 
}

PRInt32
_MD_rmdir (const char *name)
{
int rv, err;

        rv = rmdir(name);
        if (rv == -1) {
                        err = _MD_ERRNO();
                        _PR_MD_MAP_RMDIR_ERROR(err);
        }
        return rv;
}

PRInt32
_MD_pr_poll (PRPollDesc *pds, PRIntn npds, PRIntervalTime timeout)
{
    PRPollDesc *pd, *epd;
    PRInt32 n, err, pdcnt;
    PRThread *me = _PR_MD_CURRENT_THREAD();

    fd_set rd, wt, ex;
    struct timeval tv, *tvp = NULL;
    int maxfd = -1;                      
    int rv;

    ConnectListNode currentConnectList[64];
    int currentConnectListCount = 0;
    int i,j;
    int connectResult = 0;
    int connectError = 0;

    /*
     * For restarting _MD_SELECT() if it is interrupted by a signal.
     * We use these variables to figure out how much time has elapsed
     * and how much of the timeout still remains.
     */
    PRIntervalTime start, elapsed, remaining;

    FD_ZERO(&rd);
    FD_ZERO(&wt);
    FD_ZERO(&ex);

    for (pd = pds, epd = pd + npds; pd < epd; pd++) {
         PRInt32 osfd;
         PRInt16 in_flags = pd->in_flags;
         PRFileDesc *bottom = pd->fd;
 
         if ((NULL == bottom) || (in_flags == 0)) {
             continue;
         }
         while (bottom->lower != NULL) {
             bottom = bottom->lower;
         }
         osfd = bottom->secret->md.osfd;

	if( (in_flags & PR_POLL_WRITE) || (in_flags & PR_POLL_EXCEPT) ) {

	    PR_Lock( _connectLock );

	    for( i = 0; i < connectCount; i++ ) {

		if( connectList[i].osfd == osfd ) {

		    memcpy( &currentConnectList[currentConnectListCount], &connectList[i], sizeof( connectList[i] ) );
		    currentConnectListCount++;
	            break;
		}
	    }

	    PR_Unlock( _connectLock );
	}

        if (in_flags & PR_POLL_READ)  {
                FD_SET(osfd, &rd); 
		if( osfd > maxfd ) maxfd = osfd;
        }
   }
   if (timeout != PR_INTERVAL_NO_TIMEOUT) {
        tv.tv_sec = PR_IntervalToSeconds(timeout);
        tv.tv_usec = PR_IntervalToMicroseconds(timeout) % PR_USEC_PER_SEC;
        tvp = &tv;
        start = PR_IntervalNow();
   }

   if( currentConnectListCount > 0 ) {

	tv.tv_sec = 0;
	tv.tv_usec = 100000L;
	tvp = &tv;
	start = PR_IntervalNow();
   }

retry:
   if( currentConnectListCount > 0 ) {

	for( i = 0; i < currentConnectListCount; i++ ) {

		connectResult = connect( currentConnectList[i].osfd,
				&currentConnectList[i].addr,
				&currentConnectList[i].addrlen );
		connectError = _MD_ERRNO();

		if( ( connectResult < 0 ) && 
			( connectError == EINTR ||
			  connectError == EWOULDBLOCK ||
			  connectError == EINPROGRESS ||
			  connectError == EALREADY ) ) {

		    continue;
		}

		PR_Lock( _connectLock );

		for( j = 0; j < connectCount; j++ ) {

		    if( connectList[j].osfd == currentConnectList[i].osfd ) {

			if( j == ( connectCount - 1 ) ) {

				connectList[j].osfd = -1;

			} else {

			    for( ; j < connectCount; j++ )
				memcpy( &connectList[j], &connectList[j+1], sizeof( connectList[j] ) );
			}
			connectCount--;
			break;
		    }
		}

		PR_Unlock( _connectLock );

		FD_ZERO( &rd );
		FD_SET( currentConnectList[i].osfd, &wt );
		FD_SET( currentConnectList[i].osfd, &ex );
		n = 1;
		goto afterselect;
	}
   }

   if( maxfd == -1 ) {
	snooze( 100000L );
	goto retry;
   }
   n = select(maxfd + 1, &rd, NULL, NULL, tvp);
afterselect:
   if ( (n == -1 && errno == EINTR) || (n == 0 && currentConnectListCount > 0 ) ) {
        if (timeout == PR_INTERVAL_NO_TIMEOUT) {
                goto retry;
        } else {
                elapsed = (PRIntervalTime) (PR_IntervalNow() - start);
                if (elapsed > timeout) {
                        n = 0;  /* timed out */
           } else {
                remaining = timeout - elapsed;
                tv.tv_sec = PR_IntervalToSeconds(remaining);
                tv.tv_usec = PR_IntervalToMicroseconds(
                                remaining - PR_SecondsToInterval(tv.tv_sec));
                goto retry;
           }
       }
   }

   if (n > 0) {
        n = 0;
        for (pd = pds, epd = pd + npds; pd < epd; pd++) {
                PRInt32 osfd;
                PRInt16 in_flags = pd->in_flags;
                PRInt16 out_flags = 0;
                PRFileDesc *bottom = pd->fd;

                if ((NULL == bottom) || (in_flags == 0)) {
                        pd->out_flags = 0;
                        continue;
                }
                while (bottom->lower != NULL) {
                        bottom = bottom->lower;
                }
                osfd = bottom->secret->md.osfd;

                if ((in_flags & PR_POLL_READ) && FD_ISSET(osfd, &rd))  {
                        out_flags |= PR_POLL_READ;
                }
                if ((in_flags & PR_POLL_WRITE) && FD_ISSET(osfd, &wt)) {
                        out_flags |= PR_POLL_WRITE;
                }
                if ((in_flags & PR_POLL_EXCEPT) && FD_ISSET(osfd, &ex)) {
                        out_flags |= PR_POLL_EXCEPT;
                }

		if ( FD_ISSET(osfd, &wt) && FD_ISSET(osfd, &ex ) ) {

		    bottom->secret->md.connectReturnValue = connectResult;
		    bottom->secret->md.connectReturnError = connectError;
		    bottom->secret->md.connectValueValid = PR_TRUE;
		}

                pd->out_flags = out_flags;
                if (out_flags) {
                        n++;
                }
        }
        PR_ASSERT(n > 0);
   } else if (n < 0) {
        err = _MD_ERRNO();
        if (err == EBADF) {
	    /* Find the bad fds */
	    n = 0;
	    for (pd = pds, epd = pd + npds; pd < epd; pd++) {
		int optval;
		int optlen = sizeof(optval);
		PRFileDesc *bottom = pd->fd;

		pd->out_flags = 0;
		if ((NULL == bottom) || (pd->in_flags == 0)) {
		    continue;
		}
		while (bottom->lower != NULL) {
		    bottom = bottom->lower;
		}
#if 0
/*
 * BeOS doesn't have this feature of getsockopt.
*/
		if (getsockopt(bottom->secret->md.osfd, SOL_SOCKET,
			SO_TYPE, (char *) &optval, &optlen) == -1) {
		    PR_ASSERT(_MD_ERRNO() == ENOTSOCK);
		    if (_MD_ERRNO() == ENOTSOCK) {
			pd->out_flags = PR_POLL_NVAL;
			n++;
		    }
		}
#endif
	    }
	    PR_ASSERT(n > 0);
        } else {
	    PR_ASSERT(err != EINTR);  /* should have been handled above */
	    _PR_MD_MAP_SELECT_ERROR(err);
        }
   }
   return n;
}

/*
 * File locking.
 */

PRStatus
_MD_lockfile (PRInt32 osfd)
{
    PRInt32 rv;
    struct flock linfo;

    linfo.l_type = 
    linfo.l_whence = SEEK_SET;
    linfo.l_start = 0;
    linfo.l_len = 0;

    rv = fcntl(osfd, F_SETLKW, &linfo);
    if (rv == 0)
	return PR_SUCCESS;

    _PR_MD_MAP_FLOCK_ERROR(_MD_ERRNO());
    return PR_FAILURE;
}

PRStatus
_MD_tlockfile (PRInt32 osfd)
{
    PRInt32 rv;
    struct flock linfo;

    linfo.l_type = 
    linfo.l_whence = SEEK_SET;
    linfo.l_start = 0;
    linfo.l_len = 0;

    rv = fcntl(osfd, F_SETLK, &linfo);
    if (rv == 0)
	return PR_SUCCESS;

    _PR_MD_MAP_FLOCK_ERROR(_MD_ERRNO());
    return PR_FAILURE;
}

PRStatus
_MD_unlockfile (PRInt32 osfd)
{
    PRInt32 rv;
    struct flock linfo;

    linfo.l_type = 
    linfo.l_whence = SEEK_SET;
    linfo.l_start = 0;
    linfo.l_len = 0;

    rv = fcntl(osfd, F_UNLCK, &linfo);

    if (rv == 0)
	return PR_SUCCESS;

    _PR_MD_MAP_FLOCK_ERROR(_MD_ERRNO());
    return PR_FAILURE;
}

