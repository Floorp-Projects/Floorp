/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
** Netscape portable install command.
*/
#include <stdio.h>  /* OSF/1 requires this before grp.h, so put it first */
#include <assert.h>
#include <fcntl.h>
#include <string.h>
#if defined(_WINDOWS)
#include <windows.h>
typedef unsigned int mode_t;
#else
#include <grp.h>
#include <pwd.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <utime.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include "pathsub.h"

#define HAVE_LCHOWN

#if defined(AIX) || defined(BSDI) || defined(HPUX) || defined(LINUX) || defined(SUNOS4) || defined(SCO) || defined(UNIXWARE) || defined(NTO) || defined(DARWIN) || defined(__riscos__)
#undef HAVE_LCHOWN
#endif

#define HAVE_FCHMOD

#ifdef LINUX
#include <getopt.h>
#endif

#if defined(SCO) || defined(UNIXWARE) || defined(SNI) || defined(NCR) || defined(NEC)
#if !defined(S_ISLNK) && defined(S_IFLNK)
#define S_ISLNK(a)	(((a) & S_IFMT) == S_IFLNK)
#endif
#endif

#if defined(SNI)
extern int fchmod(int fildes, mode_t mode);
#endif


#ifdef GETCWD_CANT_MALLOC
/*
 * this should probably go into a utility library in case other applications
 * need it.
 */
static char *
getcwd_do_malloc(char *path, int len) {

    if (!path) {
	path = malloc(PATH_MAX +1);
	if (!path) return NULL;
    }
    return getcwd(path, PATH_MAX);
}
#define GETCWD	getcwd_do_malloc
#else
#define GETCWD	getcwd
#endif


static void
usage(void)
{
    fprintf(stderr,
	"usage: %s [-C cwd] [-L linkprefix] [-m mode] [-o owner] [-g group]\n"
	"       %*s [-DdltR] file [file ...] directory\n",
	program, (int)strlen(program), "");
    exit(2);
}

/* this is more-or-less equivalent to mkdir -p */
static int
mkdirs(char *path, mode_t mode)
{
    char *      cp;
    int         rv;
    struct stat sb;

    if (!path || !path[0])
	fail("Null pointer or empty string passed to mkdirs()");
    while (*path == '/' && path[1] == '/')
	path++;
    for (cp = strrchr(path, '/'); cp && cp != path && *(cp - 1) == '/'; cp--);
    if (cp && cp != path) {
	*cp = '\0';
	if ((stat(path, &sb) < 0 || !S_ISDIR(sb.st_mode)) &&
	    mkdirs(path, mode) < 0) {
	    return -1;
	}
	*cp = '/';
    }
    rv = mkdir(path, mode);
    if (rv) {
	if (errno != EEXIST)
	    fail("mkdirs cannot make %s", path);
	fprintf(stderr, "directory creation race: %s\n", path);
	if (!stat(path, &sb) && S_ISDIR(sb.st_mode))
	    rv = 0;
    }
    return rv;
}

static uid_t
touid(char *owner)
{
    struct passwd *pw;
    uid_t uid;
    char *cp;

    if (!owner || !owner[0])
	fail("Null pointer or empty string passed to touid()");
    pw = getpwnam(owner);
    if (pw)
	return pw->pw_uid;
    uid = strtol(owner, &cp, 0);
    if (uid == 0 && cp == owner)
	fail("cannot find uid for %s", owner);
    return uid;
}

static gid_t
togid(char *group)
{
    struct group *gr;
    gid_t gid;
    char *cp;

    if (!group || !group[0])
	fail("Null pointer or empty string passed to togid()");
    gr = getgrnam(group);
    if (gr)
	return gr->gr_gid;
    gid = strtol(group, &cp, 0);
    if (gid == 0 && cp == group)
	fail("cannot find gid for %s", group);
    return gid;
}

void * const uninit = (void *)0xdeadbeef;

int
main(int argc, char **argv)
{
    char *	base		= uninit;
    char *	bp		= uninit;
    char *	cp		= uninit;
    char *	cwd		= 0;
    char *	group		= 0;
    char *	linkname	= 0;
    char *	linkprefix	= 0;
    char *	name		= uninit;
    char *	owner		= 0;
    char *	todir		= uninit;
    char *	toname		= uninit;

    int 	bnlen		= -1;
    int 	cc		= 0;
    int 	dodir		= 0;
    int 	dolink		= 0;
    int 	dorelsymlink	= 0;
    int 	dotimes		= 0;
    int 	exists		= 0;
    int 	fromfd		= -1;
    int 	len		= -1;
    int 	lplen		= 0;
    int		onlydir		= 0;
    int 	opt		= -1;
    int 	tdlen		= -1;
    int 	tofd		= -1;
    int 	wc		= -1;

    mode_t 	mode		= 0755;

    uid_t 	uid		= -1;
    gid_t 	gid		= -1;

    struct stat sb;
    struct stat tosb;
    struct utimbuf utb;
    char 	buf[BUFSIZ];

    program = strrchr(argv[0], '/');
    if (!program)
	program = strrchr(argv[0], '\\');
    program = program ? program+1 : argv[0];


    while ((opt = getopt(argc, argv, "C:DdlL:Rm:o:g:t")) != EOF) {
	switch (opt) {
	  case 'C': cwd = optarg;	break;
	  case 'D': onlydir = 1; 	break;
	  case 'd': dodir = 1; 		break;
	  case 'l': dolink = 1;		break;
	  case 'L':
	    linkprefix = optarg;
	    lplen = strlen(linkprefix);
	    dolink = 1;
	    break;
	  case 'R': dolink = dorelsymlink = 1; break;
	  case 'm':
	    mode = strtoul(optarg, &cp, 8);
	    if (mode == 0 && cp == optarg)
		usage();
	    break;
	  case 'o': owner = optarg; 	break;
	  case 'g': group = optarg; 	break;
	  case 't': dotimes = 1; 	break;
	  default:  usage();
	}
    }

    argc -= optind;
    argv += optind;
    if (argc < 2 - onlydir)
	usage();

    todir = argv[argc-1];
    if ((stat(todir, &sb) < 0 || !S_ISDIR(sb.st_mode)) &&
	mkdirs(todir, 0777) < 0) {
	fail("cannot mkdir -p %s", todir);
    }
    if (onlydir)
	return 0;

    if (!cwd) {
	cwd = GETCWD(0, PATH_MAX);
	if (!cwd)
	    fail("could not get CWD");
    }

    /* make sure we can get into todir. */
    xchdir(todir);
    todir = GETCWD(0, PATH_MAX);
    if (!todir)
	fail("could not get CWD in todir");
    tdlen = strlen(todir);

    /* back to original directory. */
    xchdir(cwd);

    uid = owner ? touid(owner) : -1;
    gid = group ? togid(group) : -1;

    while (--argc > 0) {
	name   = *argv++;
	len    = strlen(name);
	base   = xbasename(name);
	bnlen  = strlen(base);
    size_t toname_len = tdlen + 1 + bnlen + 1;
	toname = (char*)xmalloc(toname_len);
	snprintf(toname, toname_len, "%s/%s", todir, base);
retry:
	exists = (lstat(toname, &tosb) == 0);

	if (dodir) {
	    /* -d means create a directory, always */
	    if (exists && !S_ISDIR(tosb.st_mode)) {
		int rv = unlink(toname);
		if (rv)
		    fail("cannot unlink %s", toname);
		exists = 0;
	    }
	    if (!exists && mkdir(toname, mode) < 0) {
	    	/* we probably have two nsinstall programs in a race here. */
		if (errno == EEXIST && !stat(toname, &sb) &&
		    S_ISDIR(sb.st_mode)) {
		    fprintf(stderr, "directory creation race: %s\n", toname);
		    goto retry;
	    	}
		fail("cannot make directory %s", toname);
	    }
	    if ((owner || group) && chown(toname, uid, gid) < 0)
		fail("cannot change owner of %s", toname);
	} else if (dolink) {
	    if (*name == '/') {
		/* source is absolute pathname, link to it directly */
		linkname = 0;
	    } else {
		if (linkprefix) {
		    /* -L implies -l and prefixes names with a $cwd arg. */
		    len += lplen + 1;
		    linkname = (char*)xmalloc(len + 1);
		    snprintf(linkname, len+1, "%s/%s", linkprefix, name);
		} else if (dorelsymlink) {
		    /* Symlink the relative path from todir to source name. */
		    linkname = (char*)xmalloc(PATH_MAX);

		    if (*todir == '/') {
			/* todir is absolute: skip over common prefix. */
			lplen = relatepaths(todir, cwd, linkname);
			strcpy(linkname + lplen, name);
		    } else {
			/* todir is named by a relative path: reverse it. */
			reversepath(todir, name, len, linkname);
			xchdir(cwd);
		    }

		    len = strlen(linkname);
		}
		name = linkname;
	    }

	    /* Check for a pre-existing symlink with identical content. */
	    if (exists &&
		(!S_ISLNK(tosb.st_mode) ||
		 readlink(toname, buf, sizeof buf) != len ||
		 strncmp(buf, name, len) != 0)) {
		int rmrv;
		rmrv = (S_ISDIR(tosb.st_mode) ? rmdir : unlink)(toname);
		if (rmrv < 0) {
		    fail("destination exists, cannot remove %s", toname);
		}
		exists = 0;
	    }
	    if (!exists && symlink(name, toname) < 0) {
		if (errno == EEXIST) {
		    fprintf(stderr, "symlink creation race: %s\n", toname);
                    fail("symlink was attempted in working directory %s "
                         "from %s to %s.\n", cwd, name, toname);
		    goto retry;
		}
		diagnosePath(toname);
		fail("cannot make symbolic link %s", toname);
	    }
#ifdef HAVE_LCHOWN
	    if ((owner || group) && lchown(toname, uid, gid) < 0)
		fail("cannot change owner of %s", toname);
#endif

	    if (linkname) {
		free(linkname);
		linkname = 0;
	    }
	} else {
	    /* Copy from name to toname, which might be the same file. */
	    fromfd = open(name, O_RDONLY);
	    if (fromfd < 0 || fstat(fromfd, &sb) < 0)
		fail("cannot access %s", name);
	    if (exists &&
	        (!S_ISREG(tosb.st_mode) || access(toname, W_OK) < 0)) {
		int rmrv;
		rmrv = (S_ISDIR(tosb.st_mode) ? rmdir : unlink)(toname);
		if (rmrv < 0) {
		    fail("destination exists, cannot remove %s", toname);
		}
	    }
	    tofd = open(toname, O_CREAT | O_WRONLY, 0666);
	    if (tofd < 0)
		fail("cannot create %s", toname);

	    bp = buf;
	    while ((cc = read(fromfd, bp, sizeof buf)) > 0) {
		while ((wc = write(tofd, bp, cc)) > 0) {
		    if ((cc -= wc) == 0)
			break;
		    bp += wc;
		}
		if (wc < 0)
		    fail("cannot write to %s", toname);
	    }
	    if (cc < 0)
		fail("cannot read from %s", name);

	    if (ftruncate(tofd, sb.st_size) < 0)
		fail("cannot truncate %s", toname);
	    if (dotimes) {
		utb.actime = sb.st_atime;
		utb.modtime = sb.st_mtime;
		if (utime(toname, &utb) < 0)
		    fail("cannot set times of %s", toname);
	    }
#ifdef HAVE_FCHMOD
	    if (fchmod(tofd, mode) < 0)
#else
	    if (chmod(toname, mode) < 0)
#endif
		fail("cannot change mode of %s", toname);

	    if ((owner || group) && fchown(tofd, uid, gid) < 0)
		fail("cannot change owner of %s", toname);

	    /* Must check for delayed (NFS) write errors on close. */
	    if (close(tofd) < 0)
		fail("close reports write error on %s", toname);
	    close(fromfd);
	}

	free(toname);
    }

    free(cwd);
    free(todir);
    return 0;
}

