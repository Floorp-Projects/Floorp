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
	PRInt32 rc = 0;
    fd_set rd, wr;
    struct timeval tv, *tvp = NULL;
	PRPollDesc *pd, *epd;
	int i = 0, j = 0;
	int maxfd = -1;
    PRInt32 osfd;
    PRInt16 in_flags;
    PRFileDesc *bottom;

	/*printf("POLL: entering _MD_pr_poll\n");*/
	
	/*	
	 * Is it an empty set? If so, just sleep for the timeout and return
	 */    
    if (npds < 1)
    {
		/*printf("POLL: empty set.  exiting _MD_pr_poll\n");*/
        PR_Sleep(timeout);
		return rc;
    }

    FD_ZERO(&rd);
    FD_ZERO(&wr);

	/*	
	 * first, sort out the new connects, the reads, and the writes
	 */	
	epd = pds + npds;
	for(pd = pds; pd < epd; pd++)
	{
		in_flags = pd->in_flags;
		bottom = pd->fd;
		
		if(bottom != 0 && in_flags != 0)
		{
			while(bottom->lower != 0)
			{
				bottom = bottom->lower;
			}
			osfd = bottom->secret->md.osfd;
			
			if(in_flags & PR_POLL_WRITE || in_flags & PR_POLL_EXCEPT)
			{
				/*printf("POLL: adding to write\n");*/
				FD_SET(osfd, &wr);
				if( osfd > maxfd ) maxfd = osfd;
			}
			if(in_flags & PR_POLL_READ || in_flags & PR_POLL_EXCEPT)
			{
				/*printf("POLL: adding to read\n");*/
				FD_SET(osfd, &rd);
				if( osfd > maxfd ) maxfd = osfd;
			}
		}
		
		
	} 

	if(maxfd >= 0)
	{
		PRInt32 n;
		do {
		    PRIntervalTime start = PR_IntervalNow();
	  		if (timeout != PR_INTERVAL_NO_TIMEOUT)
			{
			  /*printf("POLL: timeout = %ld\n", (long) PR_IntervalToMicroseconds(timeout));*/
	      	  tv.tv_sec = PR_IntervalToSeconds(timeout);
	      	  tv.tv_usec = PR_IntervalToMicroseconds(timeout) % PR_USEC_PER_SEC;
	       	  tvp = &tv;
			}

			
			n = select(maxfd + 1, &rd, &wr, 0, tvp);
			/*printf("POLL: maxfd = %d, select returns %d\n", maxfd, n);*/
		    if ((n <= 0) && (timeout != PR_INTERVAL_NO_TIMEOUT))
			{
				timeout -= PR_IntervalNow() - start;
				if(timeout <= 0)
				{
					/* timed out */
					n = 0;
				}
			}
			
		} while(n < 0 && errno == EINTR);
		
		if(n > 0)
		{
			epd = pds + npds;
			for(pd = pds; pd < epd; pd++)
			{
				int selected;
				in_flags = pd->in_flags;
				bottom = pd->fd;
				selected = 0;
						
				if(bottom != 0 && in_flags != 0)
				{
					while(bottom->lower != 0)
					{
						bottom = bottom->lower;
					}
					osfd = bottom->secret->md.osfd;
					if (FD_ISSET(osfd, &rd))
					{
                        pd->out_flags |= PR_POLL_READ;
						selected++;
               		}
                	if (FD_ISSET(osfd, &wr)) 
					{			
                        pd->out_flags |= PR_POLL_WRITE;
						selected++;
	                }
					
					if(selected > 0)
					{
						rc++;
						/*
					 	 * check if it is a pending connect
						 */
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
									if(connectError == EINTR || connectError == EWOULDBLOCK
									  || connectError == EINPROGRESS || connectError == EALREADY)
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
				} else {
		        	pd->out_flags = 0;
                    continue;
				}
				
			}
		}
	}

	/*printf("POLL: exiting _MD_pr_poll\n");*/
	return rc;
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

