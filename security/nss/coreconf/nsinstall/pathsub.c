/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
** Pathname subroutines.
*/
#if defined(FREEBSD) || defined(BSDI) || defined(DARWIN)
#include <sys/types.h>
#endif /* FREEBSD */
#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "pathsub.h"
#ifdef USE_REENTRANT_LIBC
#include "libc_r.h"
#endif /* USE_REENTRANT_LIBC */

char *program;

void
fail(char *format, ...)
{
    int error;
    va_list ap;

#ifdef USE_REENTRANT_LIBC
    R_STRERROR_INIT_R();
#endif

    error = errno;
    fprintf(stderr, "%s: ", program);
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
    if (error) {

#ifdef USE_REENTRANT_LIBC
    R_STRERROR_R(errno);
	fprintf(stderr, ": %s", r_strerror_r);
#else
	fprintf(stderr, ": %s", strerror(errno));
#endif
    }
 
    putc('\n', stderr);
    abort();
    exit(1);
}

char *
getcomponent(char *path, char *name)
{
    if (*path == '\0')
	return 0;
    if (*path == '/') {
	*name++ = '/';
    } else {
	do {
	    *name++ = *path++;
	} while (*path != '/' && *path != '\0');
    }
    *name = '\0';
    while (*path == '/')
	path++;
    return path;
}

#ifdef UNIXWARE
/* The static buffer in Unixware's readdir is too small. */
struct dirent * readdir(DIR *d)
{
    static struct dirent *buf = NULL;
#define MAX_PATH_LEN 1024

    if (buf == NULL)
	buf = (struct dirent *)xmalloc(sizeof(struct dirent) + MAX_PATH_LEN) ;
    return readdir_r(d, buf);
}
#endif

/* APPARENT BUG - ignores argument "dir", uses ".." instead. */
char *
ino2name(ino_t ino, char *dir)
{
    DIR *dp;
    struct dirent *ep;
    char *name;

    dp = opendir("..");		/* XXX */
    if (!dp)
	fail("cannot read parent directory");
    for (;;) {
	if (!(ep = readdir(dp)))
	    fail("cannot find current directory");
	if (ep->d_ino == ino)
	    break;
    }
    name = xstrdup(ep->d_name);
    closedir(dp);
    return name;
}

void *
xmalloc(size_t size)
{
    void *p;

    if (size <= 0)
	fail("attempted to allocate %u bytes", size);
    p = malloc(size);
    if (!p)
	fail("cannot allocate %u bytes", size);
    return p;
}

char *
xstrdup(char *s)
{
    if (!s || !s[0]) 
	fail("Null pointer or empty string passed to xstrdup()");
    return strcpy((char*)xmalloc(strlen(s) + 1), s);
}

char *
xbasename(char *path)
{
    char *cp;

    if (!path || !path[0]) 
	fail("Null pointer or empty string passed to xbasename()");
    while ((cp = strrchr(path, '/')) && cp[1] == '\0')
	*cp = '\0';
    if (!cp) return path;
    return cp + 1;
}

void
xchdir(char *dir)
{
    if (!dir || !dir[0]) 
	fail("Null pointer or empty string passed to xchdir()");
    if (chdir(dir) < 0)
	fail("cannot change directory to %s", dir);
}

int
relatepaths(char *from, char *to, char *outpath)
{
    char *cp, *cp2;
    int len;
    char buf[NAME_MAX];

    if (!from || *from != '/')
	fail("relatepaths: from path does not start with /");
    if (!to || *to != '/')
	fail("relatepaths: to   path does not start with /");

    for (cp = to, cp2 = from; *cp == *cp2; cp++, cp2++)
	if (*cp == '\0')
	    break;
    while (cp[-1] != '/')
	cp--, cp2--;
    if (cp - 1 == to) {
	/* closest common ancestor is /, so use full pathname */
	len = strlen(strcpy(outpath, to));
	if (outpath[len] != '/') {
	    outpath[len++] = '/';
	    outpath[len] = '\0';
	}
    } else {
	len = 0;
	while ((cp2 = getcomponent(cp2, buf)) != 0) {
	    strcpy(outpath + len, "../");
	    len += 3;
	}
	while ((cp = getcomponent(cp, buf)) != 0) {
	    sprintf(outpath + len, "%s/", buf);
	    len += strlen(outpath + len);
	}
    }
    return len;
}

void
reversepath(char *inpath, char *name, int len, char *outpath)
{
    char *cp, *cp2;
    char buf[NAME_MAX];
    struct stat sb;

    cp = strcpy(outpath + PATH_MAX - (len + 1), name);
    cp2 = inpath;
    while ((cp2 = getcomponent(cp2, buf)) != 0) {
	if (strcmp(buf, ".") == 0)
	    continue;
	if (strcmp(buf, "..") == 0) {
	    if (stat(".", &sb) < 0)
		fail("cannot stat current directory");
	    name = ino2name(sb.st_ino, "..");
	    len = strlen(name);
	    cp -= len + 1;
	    strcpy(cp, name);
	    cp[len] = '/';
	    free(name);
	    xchdir("..");
	} else {
	    cp -= 3;
	    strncpy(cp, "../", 3);
	    xchdir(buf);
	}
    }
    strcpy(outpath, cp);
}

void
diagnosePath(const char * path)
{
    char *	myPath;
    char *      slash;
    int		rv;
    struct stat sb;
    char 	buf[BUFSIZ];

    if (!path || !path[0]) 
	fail("Null pointer or empty string passed to mkdirs()");
    myPath = strdup(path);
    if (!myPath)
	fail("strdup() failed!");
    do {
    	rv = lstat(myPath, &sb);
	if (rv < 0) {
	    perror(myPath);
	} else if (S_ISLNK(sb.st_mode)) {
	    rv = readlink(myPath, buf, sizeof(buf) - 1);
	    if (rv < 0) {
	    	perror("readlink");
		buf[0] = 0;
	    } else {
	    	buf[rv] = 0;
	    }
	    fprintf(stderr, "%s is a link to %s\n", myPath, buf);
	} else if (S_ISDIR(sb.st_mode)) {
	    fprintf(stderr, "%s is a directory\n", myPath);
	    rv = access(myPath, X_OK);
	    if (rv < 0) {
	    	fprintf(stderr, "%s: no search permission\n", myPath);
	    }
	} else {
	    fprintf(stderr, "%s is a file !?!\n", myPath);
	    rv = access(myPath, F_OK);
	    if (rv < 0) {
	    	fprintf(stderr, "%s does not exist\n", myPath);
	    }
	}

	/* chop path off one level. */
	slash = strrchr(myPath, '/');
	if (!slash)
	    slash = strrchr(myPath, '\\');
	if (!slash)
	    slash = myPath;
	*slash = 0;
    } while (myPath[0]);
    free(myPath);
}
