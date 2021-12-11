/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
** Netscape portable install command.
**
** Brendan Eich, 7/20/95
*/
#include <stdio.h>  /* OSF/1 requires this before grp.h, so put it first */
#include <assert.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utime.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#ifdef USE_REENTRANT_LIBC
#include "libc_r.h"
#endif /* USE_REENTRANT_LIBC */

#include "pathsub.h"

#define HAVE_FCHMOD


/*
 * Does getcwd() take NULL as the first argument and malloc
 * the result buffer?
 */
#if !defined(DARWIN)
#define GETCWD_CAN_MALLOC
#endif

#if defined(LINUX) || defined(__GNU__) || defined(__GLIBC__)
#include <getopt.h>
#endif

#if defined(SCO) || defined(UNIXWARE)
#if !defined(S_ISLNK) && defined(S_IFLNK)
#define S_ISLNK(a)  (((a) & S_IFMT) == S_IFLNK)
#endif
#endif

#ifdef QNX
#define d_ino d_stat.st_ino
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

static int
mkdirs(char *path, mode_t mode)
{
    char *cp;
    struct stat sb;
    int res;

    while (*path == '/' && path[1] == '/') {
        path++;
    }
    for (cp = strrchr(path, '/'); cp && cp != path && cp[-1] == '/'; cp--)
        ;
    if (cp && cp != path) {
        *cp = '\0';
        if ((stat(path, &sb) < 0 || !S_ISDIR(sb.st_mode)) &&
            mkdirs(path, mode) < 0) {
            return -1;
        }
        *cp = '/';
    }
    res = mkdir(path, mode);
    if ((res != 0) && (errno == EEXIST)) {
        return 0;
    }
    else {
        return res;
    }
}

static uid_t
touid(char *owner)
{
    struct passwd *pw;
    uid_t uid;
    char *cp;

    pw = getpwnam(owner);
    if (pw) {
        return pw->pw_uid;
    }
    uid = strtol(owner, &cp, 0);
    if (uid == 0 && cp == owner) {
        fail("cannot find uid for %s", owner);
    }
    return uid;
}

static gid_t
togid(char *group)
{
    struct group *gr;
    gid_t gid;
    char *cp;

    gr = getgrnam(group);
    if (gr) {
        return gr->gr_gid;
    }
    gid = strtol(group, &cp, 0);
    if (gid == 0 && cp == group) {
        fail("cannot find gid for %s", group);
    }
    return gid;
}

int
main(int argc, char **argv)
{
    int onlydir, dodir, dolink, dorelsymlink, dotimes, opt, len, lplen, tdlen, bnlen, exists, fromfd, tofd, cc, wc;
    mode_t mode = 0755;
    char *linkprefix, *owner, *group, *cp, *cwd, *todir, *toname, *name, *base, *linkname, *bp, buf[BUFSIZ];
    uid_t uid;
    gid_t gid;
    struct stat sb, tosb;
    struct utimbuf utb;

    program = argv[0];
    cwd = linkname = linkprefix = owner = group = 0;
    onlydir = dodir = dolink = dorelsymlink = dotimes = lplen = 0;

    while ((opt = getopt(argc, argv, "C:DdlL:Rm:o:g:t")) != EOF) {
        switch (opt) {
            case 'C':
                cwd = optarg;
                break;
            case 'D':
                onlydir = 1;
                break;
            case 'd':
                dodir = 1;
                break;
            case 'l':
                dolink = 1;
                break;
            case 'L':
                linkprefix = optarg;
                lplen = strlen(linkprefix);
                dolink = 1;
                break;
            case 'R':
                dolink = dorelsymlink = 1;
                break;
            case 'm':
                mode = strtoul(optarg, &cp, 8);
                if (mode == 0 && cp == optarg) {
                    usage();
                }
                break;
            case 'o':
                owner = optarg;
                break;
            case 'g':
                group = optarg;
                break;
            case 't':
                dotimes = 1;
                break;
            default:
                usage();
        }
    }

    argc -= optind;
    argv += optind;
    if (argc < 2 - onlydir) {
        usage();
    }

    todir = argv[argc-1];
    if ((stat(todir, &sb) < 0 || !S_ISDIR(sb.st_mode)) &&
        mkdirs(todir, 0777) < 0) {
        fail("cannot make directory %s", todir);
    }
    if (onlydir) {
        return 0;
    }

    if (!cwd) {
#ifdef GETCWD_CAN_MALLOC
        cwd = getcwd(0, PATH_MAX);
#else
        cwd = malloc(PATH_MAX + 1);
        cwd = getcwd(cwd, PATH_MAX);
#endif
    }
    xchdir(todir);
#ifdef GETCWD_CAN_MALLOC
    todir = getcwd(0, PATH_MAX);
#else
    todir = malloc(PATH_MAX + 1);
    todir = getcwd(todir, PATH_MAX);
#endif
    xchdir(cwd);
    tdlen = strlen(todir);

    uid = owner ? touid(owner) : -1;
    gid = group ? togid(group) : -1;

    while (--argc > 0) {
        name = *argv++;
        len = strlen(name);
        base = xbasename(name);
        bnlen = strlen(base);
        toname = (char*)xmalloc(tdlen + 1 + bnlen + 1);
        sprintf(toname, "%s/%s", todir, base);
        exists = (lstat(toname, &tosb) == 0);

        if (dodir) {
            /* -d means create a directory, always */
            if (exists && !S_ISDIR(tosb.st_mode)) {
                (void) unlink(toname);
                exists = 0;
            }
            if (!exists && mkdir(toname, mode) < 0) {
                fail("cannot make directory %s", toname);
            }
            if ((owner || group) && chown(toname, uid, gid) < 0) {
                fail("cannot change owner of %s", toname);
            }
        } else if (dolink) {
            if (*name == '/') {
                /* source is absolute pathname, link to it directly */
                linkname = 0;
            } else {
                if (linkprefix) {
                    /* -L implies -l and prefixes names with a $cwd arg. */
                    len += lplen + 1;
                    linkname = (char*)xmalloc(len + 1);
                    sprintf(linkname, "%s/%s", linkprefix, name);
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
                (void) (S_ISDIR(tosb.st_mode) ? rmdir : unlink)(toname);
                exists = 0;
            }
            if (!exists && symlink(name, toname) < 0) {
                fail("cannot make symbolic link %s", toname);
            }
#ifdef HAVE_LCHOWN
            if ((owner || group) && lchown(toname, uid, gid) < 0) {
                fail("cannot change owner of %s", toname);
            }
#endif

            if (linkname) {
                free(linkname);
                linkname = 0;
            }
        } else {
            /* Copy from name to toname, which might be the same file. */
            fromfd = open(name, O_RDONLY);
            if (fromfd < 0 || fstat(fromfd, &sb) < 0) {
                fail("cannot access %s", name);
            }
            if (exists && (!S_ISREG(tosb.st_mode) || access(toname, W_OK) < 0)) {
                (void) (S_ISDIR(tosb.st_mode) ? rmdir : unlink)(toname);
            }
            tofd = open(toname, O_CREAT | O_WRONLY, 0666);
            if (tofd < 0) {
                fail("cannot create %s", toname);
            }

            bp = buf;
            while ((cc = read(fromfd, bp, sizeof buf)) > 0) {
                while ((wc = write(tofd, bp, cc)) > 0) {
                    if ((cc -= wc) == 0) {
                        break;
                    }
                    bp += wc;
                }
                if (wc < 0) {
                    fail("cannot write to %s", toname);
                }
            }
            if (cc < 0) {
                fail("cannot read from %s", name);
            }

            if (ftruncate(tofd, sb.st_size) < 0) {
                fail("cannot truncate %s", toname);
            }
            if (dotimes) {
                utb.actime = sb.st_atime;
                utb.modtime = sb.st_mtime;
                if (utime(toname, &utb) < 0) {
                    fail("cannot set times of %s", toname);
                }
            }
#ifdef HAVE_FCHMOD
            if (fchmod(tofd, mode) < 0)
#else
            if (chmod(toname, mode) < 0)
#endif
                fail("cannot change mode of %s", toname);
            if ((owner || group) && fchown(tofd, uid, gid) < 0) {
                fail("cannot change owner of %s", toname);
            }

            /* Must check for delayed (NFS) write errors on close. */
            if (close(tofd) < 0) {
                fail("cannot write to %s", toname);
            }
            close(fromfd);
        }

        free(toname);
    }

    free(cwd);
    free(todir);
    return 0;
}

/*
** Pathname subroutines.
**
** Brendan Eich, 8/29/95
*/

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
    if (error)

#ifdef USE_REENTRANT_LIBC
        R_STRERROR_R(errno);
    fprintf(stderr, ": %s", r_strerror_r);
#else
        fprintf(stderr, ": %s", strerror(errno));
#endif

    putc('\n', stderr);
    exit(1);
}

char *
getcomponent(char *path, char *name)
{
    if (*path == '\0') {
        return 0;
    }
    if (*path == '/') {
        *name++ = '/';
    } else {
        do {
            *name++ = *path++;
        } while (*path != '/' && *path != '\0');
    }
    *name = '\0';
    while (*path == '/') {
        path++;
    }
    return path;
}

#ifdef UNIXWARE_READDIR_BUFFER_TOO_SMALL
/* Sigh.  The static buffer in Unixware's readdir is too small. */
struct dirent * readdir(DIR *d)
{
    static struct dirent *buf = NULL;
#define MAX_PATH_LEN 1024


    if(buf == NULL)
        buf = (struct dirent *) malloc(sizeof(struct dirent) + MAX_PATH_LEN)
              ;
    return(readdir_r(d, buf));
}
#endif

char *
ino2name(ino_t ino, char *dir)
{
    DIR *dp;
    struct dirent *ep;
    char *name;

    dp = opendir("..");
    if (!dp) {
        fail("cannot read parent directory");
    }
    for (;;) {
        if (!(ep = readdir(dp))) {
            fail("cannot find current directory");
        }
        if (ep->d_ino == ino) {
            break;
        }
    }
    name = xstrdup(ep->d_name);
    closedir(dp);
    return name;
}

void *
xmalloc(size_t size)
{
    void *p = malloc(size);
    if (!p) {
        fail("cannot allocate %u bytes", size);
    }
    return p;
}

char *
xstrdup(char *s)
{
    return strcpy((char*)xmalloc(strlen(s) + 1), s);
}

char *
xbasename(char *path)
{
    char *cp;

    while ((cp = strrchr(path, '/')) && cp[1] == '\0') {
        *cp = '\0';
    }
    if (!cp) {
        return path;
    }
    return cp + 1;
}

void
xchdir(char *dir)
{
    if (chdir(dir) < 0) {
        fail("cannot change directory to %s", dir);
    }
}

int
relatepaths(char *from, char *to, char *outpath)
{
    char *cp, *cp2;
    int len;
    char buf[NAME_MAX];

    assert(*from == '/' && *to == '/');
    for (cp = to, cp2 = from; *cp == *cp2; cp++, cp2++)
        if (*cp == '\0') {
            break;
        }
    while (cp[-1] != '/') {
        cp--, cp2--;
    }
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
        if (strcmp(buf, ".") == 0) {
            continue;
        }
        if (strcmp(buf, "..") == 0) {
            if (stat(".", &sb) < 0) {
                fail("cannot stat current directory");
            }
            name = ino2name(sb.st_ino, "..");
            len = strlen(name);
            cp -= len + 1;
            strcpy(cp, name);
            cp[len] = '/';
            free(name);
            xchdir("..");
        } else {
            cp -= 3;
            memcpy(cp, "../", 3);
            xchdir(buf);
        }
    }
    strcpy(outpath, cp);
}
