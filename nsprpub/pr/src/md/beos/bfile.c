/* -*- Mode: C++; tab-width: 8; c-basic-offset: 8 -*- */
/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape Portable Runtime (NSPR).
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
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

PRStatus
_MD_set_fd_inheritable (PRFileDesc *fd, PRBool inheritable)
{
        int rv;
	
        rv = fcntl(fd->secret->md.osfd, F_SETFD, inheritable ? 0 : FD_CLOEXEC);
        if (-1 == rv) {
                PR_SetError(PR_UNKNOWN_ERROR, _MD_ERRNO());
                return PR_FAILURE;
        }
        return PR_SUCCESS;
}

void
_MD_init_fd_inheritable (PRFileDesc *fd, PRBool imported)
{
	if (imported) {
		fd->secret->inheritable = _PR_TRI_UNKNOWN;
	} else {
		int flags = fcntl(fd->secret->md.osfd, F_GETFD, 0);
		if (flags == -1) {
			PR_SetError(PR_UNKNOWN_ERROR, _MD_ERRNO());
			return;
		}
		fd->secret->inheritable = (flags & FD_CLOEXEC) ? 
			_PR_TRI_TRUE : _PR_TRI_FALSE;
	}
}

void
_MD_query_fd_inheritable (PRFileDesc *fd)
{
	int flags;
	
	PR_ASSERT(_PR_TRI_UNKNOWN == fd->secret->inheritable);
	flags = fcntl(fd->secret->md.osfd, F_GETFD, 0);
	PR_ASSERT(-1 != flags);
	fd->secret->inheritable = (flags & FD_CLOEXEC) ?
		_PR_TRI_FALSE : _PR_TRI_TRUE;
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

#ifndef BONE_VERSION /* Writev moves to bnet.c with BONE */
PRInt32
_MD_writev (PRFileDesc *fd, const PRIOVec *iov, PRInt32 iov_size,
	    PRIntervalTime timeout)
{
    return PR_NOT_IMPLEMENTED_ERROR;
}
#endif

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
_MD_pr_poll(PRPollDesc *pds, PRIntn npds, PRIntervalTime timeout)
{
	PRInt32 rv = 0;
	PRThread *me = _PR_MD_CURRENT_THREAD();
	/*
	 * This code is almost a duplicate of w32poll.c's _PR_MD_PR_POLL().
	 */
	fd_set rd, wt, ex;
	PRFileDesc *bottom;
	PRPollDesc *pd, *epd;
	PRInt32 maxfd = -1, ready, err;
	PRIntervalTime remaining, elapsed, start;

	struct timeval tv, *tvp = NULL;

	if (_PR_PENDING_INTERRUPT(me))
	{
		me->flags &= ~_PR_INTERRUPT;
		PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
		return -1;
	}

	if (0 == npds) {
		PR_Sleep(timeout);
		return rv;
	}

	FD_ZERO(&rd);
	FD_ZERO(&wt);
	FD_ZERO(&ex);

	ready = 0;
	for (pd = pds, epd = pd + npds; pd < epd; pd++)
	{
		PRInt16 in_flags_read = 0, in_flags_write = 0;
		PRInt16 out_flags_read = 0, out_flags_write = 0; 
		
		if ((NULL != pd->fd) && (0 != pd->in_flags))
		{
			if (pd->in_flags & PR_POLL_READ)
			{
				in_flags_read = (pd->fd->methods->poll)(pd->fd, pd->in_flags & ~PR_POLL_WRITE, &out_flags_read);
			}
			if (pd->in_flags & PR_POLL_WRITE)
			{
				in_flags_write = (pd->fd->methods->poll)(pd->fd, pd->in_flags & ~PR_POLL_READ, &out_flags_write);
			}
			if ((0 != (in_flags_read & out_flags_read))
			    || (0 != (in_flags_write & out_flags_write)))
			{
				/* this one's ready right now */
				if (0 == ready)
				{
					/*
					 * We will have to return without calling the
					 * system poll/select function.  So zero the
					 * out_flags fields of all the poll descriptors
					 * before this one. 
					 */
					PRPollDesc *prev;
					for (prev = pds; prev < pd; prev++)
					{
						prev->out_flags = 0;
					}
				}
				ready += 1;
				pd->out_flags = out_flags_read | out_flags_write;
			}
			else
			{
				pd->out_flags = 0;  /* pre-condition */
				
				/* make sure this is an NSPR supported stack */
				bottom = PR_GetIdentitiesLayer(pd->fd, PR_NSPR_IO_LAYER);
				PR_ASSERT(NULL != bottom);  /* what to do about that? */
				if ((NULL != bottom)
				    && (_PR_FILEDESC_OPEN == bottom->secret->state))
				{
					if (0 == ready)
					{
						PRInt32 osfd = bottom->secret->md.osfd; 
						if (osfd > maxfd) maxfd = osfd;
						if (in_flags_read & PR_POLL_READ)
						{
							pd->out_flags |= _PR_POLL_READ_SYS_READ;
							FD_SET(osfd, &rd);
						}
						if (in_flags_read & PR_POLL_WRITE)
						{
							pd->out_flags |= _PR_POLL_READ_SYS_WRITE;
							FD_SET(osfd, &wt);
						}
						if (in_flags_write & PR_POLL_READ)
						{
							pd->out_flags |= _PR_POLL_WRITE_SYS_READ;
							FD_SET(osfd, &rd);
						}
						if (in_flags_write & PR_POLL_WRITE)
						{
							pd->out_flags |= _PR_POLL_WRITE_SYS_WRITE;
							FD_SET(osfd, &wt);
						}
						if (pd->in_flags & PR_POLL_EXCEPT) FD_SET(osfd, &ex);
					}
				}
				else
				{
					if (0 == ready)
					{
						PRPollDesc *prev;
						for (prev = pds; prev < pd; prev++)
						{
							prev->out_flags = 0;
						}
					}
					ready += 1;  /* this will cause an abrupt return */
					pd->out_flags = PR_POLL_NVAL;  /* bogii */
				}
			}
		}
		else
		{
			pd->out_flags = 0;
		}
	}

	if (0 != ready) return ready;  /* no need to block */

	remaining = timeout;
	start = PR_IntervalNow(); 

 retry:
	if (timeout != PR_INTERVAL_NO_TIMEOUT)
	{
		PRInt32 ticksPerSecond = PR_TicksPerSecond();
		tv.tv_sec = remaining / ticksPerSecond;
		tv.tv_usec = PR_IntervalToMicroseconds( remaining % ticksPerSecond );
		tvp = &tv;
	}
	
	ready = _MD_SELECT(maxfd + 1, &rd, &wt, &ex, tvp);
	
	if (ready == -1 && errno == EINTR)
	{
		if (timeout == PR_INTERVAL_NO_TIMEOUT) goto retry;
		else
		{
			elapsed = (PRIntervalTime) (PR_IntervalNow() - start);
			if (elapsed > timeout) ready = 0;  /* timed out */
			else
			{
				remaining = timeout - elapsed;
				goto retry; 
			}
		}
	} 

	/*
	** Now to unravel the select sets back into the client's poll
	** descriptor list. Is this possibly an area for pissing away
	** a few cycles or what?
	*/
	if (ready > 0)
	{
		ready = 0;
		for (pd = pds, epd = pd + npds; pd < epd; pd++)
		{
			PRInt16 out_flags = 0;
			if ((NULL != pd->fd) && (0 != pd->in_flags))
			{
				PRInt32 osfd;
				bottom = PR_GetIdentitiesLayer(pd->fd, PR_NSPR_IO_LAYER);
				PR_ASSERT(NULL != bottom);
				
				osfd = bottom->secret->md.osfd; 
				
				if (FD_ISSET(osfd, &rd))
				{
					if (pd->out_flags & _PR_POLL_READ_SYS_READ)
						out_flags |= PR_POLL_READ;
					if (pd->out_flags & _PR_POLL_WRITE_SYS_READ)
						out_flags |= PR_POLL_WRITE;
				}
				if (FD_ISSET(osfd, &wt))
				{
					if (pd->out_flags & _PR_POLL_READ_SYS_WRITE)
						out_flags |= PR_POLL_READ;
					if (pd->out_flags & _PR_POLL_WRITE_SYS_WRITE)
						out_flags |= PR_POLL_WRITE;
				}
				if (FD_ISSET(osfd, &ex)) out_flags |= PR_POLL_EXCEPT;

/* Workaround for nonblocking connects under net_server */
#ifndef BONE_VERSION 		
				if (out_flags)
				{
					/* check if it is a pending connect */
					int i = 0, j = 0;
					PR_Lock( _connectLock );
					for( i = 0; i < connectCount; i++ ) 
					{
						if(connectList[i].osfd == osfd)
						{
							int connectError;
							int connectResult;
					
							connectResult = connect(connectList[i].osfd,
							                        &connectList[i].addr,
							                        connectList[i].addrlen);
							connectError = errno;
					
							if(connectResult < 0 ) 
							{
								if(connectError == EINTR || connectError == EWOULDBLOCK ||
					  		   connectError == EINPROGRESS || connectError == EALREADY)
								{
									break;
								}
							}
					
							if(i == (connectCount - 1))
							{
								connectList[i].osfd = -1;
							} else {
								for(j = i; j < connectCount; j++ )
								{
									memcpy( &connectList[j], &connectList[j+1],
									        sizeof(connectList[j]));
								}
							}
							connectCount--;
					
							bottom->secret->md.connectReturnValue = connectResult;
							bottom->secret->md.connectReturnError = connectError;
							bottom->secret->md.connectValueValid = PR_TRUE;
							break;
						}
					}
					PR_Unlock( _connectLock );
				}
#endif
			}
			pd->out_flags = out_flags;
			if (out_flags) ready++;
		}
		PR_ASSERT(ready > 0);
	}
	else if (ready < 0)
	{ 
		err = _MD_ERRNO();
		if (err == EBADF)
		{
			/* Find the bad fds */
			ready = 0;
			for (pd = pds, epd = pd + npds; pd < epd; pd++)
			{
				pd->out_flags = 0;
				if ((NULL != pd->fd) && (0 != pd->in_flags))
				{
					bottom = PR_GetIdentitiesLayer(pd->fd, PR_NSPR_IO_LAYER);
					if (fcntl(bottom->secret->md.osfd, F_GETFL, 0) == -1)
					{
						pd->out_flags = PR_POLL_NVAL;
						ready++;
					}
				}
			}
			PR_ASSERT(ready > 0);
		}
		else _PR_MD_MAP_SELECT_ERROR(err);
	}
	
	return ready;
}  /* _MD_pr_poll */

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

