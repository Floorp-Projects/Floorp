/*
 * $XFree86: xc/lib/fontconfig/src/fcatomic.c,v 1.2 2002/03/04 21:15:28 tsi Exp $
 *
 * Copyright © 2002 Keith Packard, member of The XFree86 Project, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * fcatomic.c
 *
 * Lock cache and configuration files for atomic update
 *
 * Uses only regular filesystem calls so it should
 * work even in the absense of functioning file locking
 *
 *  Four files:
 *	file	    - the data file accessed by other apps.
 *	new	    - a new version of the data file while it's being written
 *	lck	    - the lock file
 *	tmp	    - a temporary file made unique with mkstemp
 *
 *  Here's how it works:
 *	Create 'tmp' and store our PID in it
 *	Attempt to link it to 'lck'
 *	Unlink 'tmp'
 *	If the link succeeded, the lock is held
 */

#include "fcint.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

#define NEW_NAME	".NEW"
#define LCK_NAME	".LCK"
#define TMP_NAME	".TMP-XXXXXX"

FcAtomic *
FcAtomicCreate (const FcChar8   *file)
{
    int	    file_len = strlen ((char *) file);
    int	    new_len = file_len + sizeof (NEW_NAME);
    int	    lck_len = file_len + sizeof (LCK_NAME);
    int	    tmp_len = file_len + sizeof (TMP_NAME);
    int	    total_len = (sizeof (FcAtomic) +
			 file_len + 1 +
			 new_len + 1 +
			 lck_len + 1 +
			 tmp_len + 1);
    FcAtomic	*atomic = malloc (total_len);
    if (!atomic)
	return 0;
    
    atomic->file = (FcChar8 *) (atomic + 1);
    strcpy ((char *) atomic->file, (char *) file);

    atomic->new = atomic->file + file_len + 1;
    strcpy ((char *) atomic->new, (char *) file);
    strcat ((char *) atomic->new, NEW_NAME);

    atomic->lck = atomic->new + new_len + 1;
    strcpy ((char *) atomic->lck, (char *) file);
    strcat ((char *) atomic->lck, LCK_NAME);

    atomic->tmp = atomic->lck + lck_len + 1;

    return atomic;
}

FcBool
FcAtomicLock (FcAtomic *atomic)
{
    int		fd = -1;
    FILE	*f = 0;
    int		ret;
    struct stat	lck_stat;

    strcpy ((char *) atomic->tmp, (char *) atomic->file);
    strcat ((char *) atomic->tmp, TMP_NAME);
    fd = mkstemp ((char *) atomic->tmp);
    if (fd < 0)
	return FcFalse;
    f = fdopen (fd, "w");
    if (!f)
    {
    	close (fd);
	unlink ((char *) atomic->tmp);
	return FcFalse;
    }
    ret = fprintf (f, "%ld\n", (long)getpid());
    if (ret <= 0)
    {
	fclose (f);
	unlink ((char *) atomic->tmp);
	return FcFalse;
    }
    if (fclose (f) == EOF)
    {
	unlink ((char *) atomic->tmp);
	return FcFalse;
    }
    ret = link ((char *) atomic->tmp, (char *) atomic->lck);
    (void) unlink ((char *) atomic->tmp);
    if (ret < 0)
    {
	/*
	 * If the file is around and old (> 10 minutes),
	 * assume the lock is stale.  This assumes that any
	 * machines sharing the same filesystem will have clocks
	 * reasonably close to each other.
	 */
	if (stat ((char *) atomic->lck, &lck_stat) >= 0)
	{
	    time_t  now = time (0);
	    if ((long int) (now - lck_stat.st_mtime) > 10 * 60)
	    {
		if (unlink ((char *) atomic->lck) == 0)
		    return FcAtomicLock (atomic);
	    }
	}
	return FcFalse;
    }
    (void) unlink ((char *) atomic->new);
    return FcTrue;
}

FcChar8 *
FcAtomicNewFile (FcAtomic *atomic)
{
    return atomic->new;
}

FcChar8 *
FcAtomicOrigFile (FcAtomic *atomic)
{
    return atomic->file;
}

FcBool
FcAtomicReplaceOrig (FcAtomic *atomic)
{
    if (rename ((char *) atomic->new, (char *) atomic->file) < 0)
	return FcFalse;
    return FcTrue;
}

void
FcAtomicDeleteNew (FcAtomic *atomic)
{
    unlink ((char *) atomic->new);
}

void
FcAtomicUnlock (FcAtomic *atomic)
{
    unlink ((char *) atomic->lck);
}

void
FcAtomicDestroy (FcAtomic *atomic)
{
    free (atomic);
}
