/*-
 * Copyright (c) 1991, 1993
 *  The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. ***REMOVED*** - see
 *    ftp://ftp.cs.berkeley.edu/pub/4bsd/README.Impt.License.Change
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)db.c    8.4 (Berkeley) 2/21/94";
#endif /* LIBC_SCCS and not lint */

#ifndef __DBINTERFACE_PRIVATE
#define __DBINTERFACE_PRIVATE
#endif
#ifdef macintosh
#include <unix.h>
#else
#include <sys/types.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>

#include "mcom_db.h"

/* a global flag that locks closed all databases */
int all_databases_locked_closed = 0;

/* set or unset a global lock flag to disable the
 * opening of any DBM file
 */
void
dbSetOrClearDBLock(DBLockFlagEnum type)
{
    if (type == LockOutDatabase)
        all_databases_locked_closed = 1;
    else
        all_databases_locked_closed = 0;
}

DB *
dbopen(const char *fname, int flags, int mode, DBTYPE type, const void *openinfo)
{

    /* lock out all file databases.  Let in-memory databases through
     */
    if (all_databases_locked_closed && fname) {
        errno = EINVAL;
        return (NULL);
    }

#define DB_FLAGS (DB_LOCK | DB_SHMEM | DB_TXN)

#if 0 /* most systems don't have EXLOCK and SHLOCK */
#define USE_OPEN_FLAGS                                     \
    (O_CREAT | O_EXCL | O_EXLOCK | O_NONBLOCK | O_RDONLY | \
     O_RDWR | O_SHLOCK | O_TRUNC)
#else
#define USE_OPEN_FLAGS             \
    (O_CREAT | O_EXCL | O_RDONLY | \
     O_RDWR | O_TRUNC)
#endif

    if ((flags & ~(USE_OPEN_FLAGS | DB_FLAGS)) == 0)
        switch (type) {
/* we don't need btree and recno right now */
#if 0
            case DB_BTREE:
                return (__bt_open(fname, flags & USE_OPEN_FLAGS,
                    mode, openinfo, flags & DB_FLAGS));
            case DB_RECNO:
                return (__rec_open(fname, flags & USE_OPEN_FLAGS,
                    mode, openinfo, flags & DB_FLAGS));
#endif

            case DB_HASH:
                return (__hash_open(fname, flags & USE_OPEN_FLAGS,
                                    mode, (const HASHINFO *)openinfo, flags & DB_FLAGS));
            default:
                break;
        }
    errno = EINVAL;
    return (NULL);
}

static int
__dberr()
{
    return (RET_ERROR);
}

/*
 * __DBPANIC -- Stop.
 *
 * Parameters:
 *  dbp:    pointer to the DB structure.
 */
void
__dbpanic(DB *dbp)
{
    /* The only thing that can succeed is a close. */
    dbp->del = (int (*)(const struct __db *, const DBT *, uint))__dberr;
    dbp->fd = (int (*)(const struct __db *))__dberr;
    dbp->get = (int (*)(const struct __db *, const DBT *, DBT *, uint))__dberr;
    dbp->put = (int (*)(const struct __db *, DBT *, const DBT *, uint))__dberr;
    dbp->seq = (int (*)(const struct __db *, DBT *, DBT *, uint))__dberr;
    dbp->sync = (int (*)(const struct __db *, uint))__dberr;
}
