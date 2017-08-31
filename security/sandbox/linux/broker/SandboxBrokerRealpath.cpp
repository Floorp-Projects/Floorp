/*
 * Copyright (c) 2003 Constantin S. Svintsoff <kostik@iclub.nsu.ru>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * This is originally from:
 * android-n-mr2-preview-1-303-gccec0f4c1
 * libc/upstream-freebsd/lib/libc/stdlib/realpath.c
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)realpath.c	8.1 (Berkeley) 2/16/94";
#endif /* LIBC_SCCS and not lint */
#include <sys/param.h>
#include <sys/stat.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "base/string_util.h"
#include "SandboxBroker.h"

// Original copy in, but not usable from here:
// toolkit/crashreporter/google-breakpad/src/common/linux/linux_libc_support.cc
static size_t my_strlcat(char* s1, const char* s2, size_t len) {
  size_t pos1 = 0;

  while (pos1 < len && s1[pos1] != '\0')
    pos1++;

  if (pos1 == len)
    return pos1;

  return pos1 + base::strlcpy(s1 + pos1, s2, len - pos1);
}

namespace mozilla {

/*
 * Original: realpath
 * Find the real name of path, by removing all ".", ".." and symlink
 * components.  Returns (resolved) on success, or (NULL) on failure,
 * in which case the path which caused trouble is left in (resolved).
 * Changes:
 * Resolve relative paths, but don't allow backing out of a symlink
 * target. Fail with permission error if any dir is writable.
 */
char* SandboxBroker::SymlinkPath(const Policy* policy,
                                 const char* __restrict path,
                                 char* __restrict resolved,
                                 int* perms)
{
    struct stat sb;
    char *p, *q, *s;
    size_t left_len, resolved_len, backup_allowed;
    unsigned symlinks;
    int m, slen;
    char left[PATH_MAX], next_token[PATH_MAX], symlink[PATH_MAX];

    if (*perms) {
        *perms = 0;
    }
    if (path == NULL) {
        errno = EINVAL;
        return (NULL);
    }
    if (path[0] == '\0') {
        errno = ENOENT;
        return (NULL);
    }
    if (resolved == NULL) {
        resolved = (char*)malloc(PATH_MAX);
        if (resolved == NULL)
            return (NULL);
        m = 1;
    } else
        m = 0;
    symlinks = 0;
    backup_allowed = PATH_MAX;
    if (path[0] == '/') {
        resolved[0] = '/';
        resolved[1] = '\0';
        if (path[1] == '\0')
            return (resolved);
        resolved_len = 1;
        left_len = base::strlcpy(left, path + 1, sizeof(left));
    } else {
        if (getcwd(resolved, PATH_MAX) == NULL) {
            if (m)
                free(resolved);
            else {
                resolved[0] = '.';
                resolved[1] = '\0';
            }
            return (NULL);
        }
        resolved_len = strlen(resolved);
        left_len = base::strlcpy(left, path, sizeof(left));
    }
    if (left_len >= sizeof(left) || resolved_len >= PATH_MAX) {
        if (m)
            free(resolved);
        errno = ENAMETOOLONG;
        return (NULL);
    }

    /*
     * Iterate over path components in `left'.
     */
    while (left_len != 0) {
        /*
         * Extract the next path component and adjust `left'
         * and its length.
         */
        p = strchr(left, '/');
        s = p ? p : left + left_len;
        if (s - left >= (ssize_t)sizeof(next_token)) {
            if (m)
                free(resolved);
            errno = ENAMETOOLONG;
            return (NULL);
        }
        memcpy(next_token, left, s - left);
        next_token[s - left] = '\0';
        left_len -= s - left;
        if (p != NULL)
            memmove(left, s + 1, left_len + 1);
        if (resolved[resolved_len - 1] != '/') {
            if (resolved_len + 1 >= PATH_MAX) {
                if (m)
                    free(resolved);
                errno = ENAMETOOLONG;
                return (NULL);
            }
            resolved[resolved_len++] = '/';
            resolved[resolved_len] = '\0';
        }
        if (next_token[0] == '\0') {
            /* Handle consequential slashes. */
            continue;
        }
        else if (strcmp(next_token, ".") == 0)
            continue;
        else if (strcmp(next_token, "..") == 0) {
            /*
             * Strip the last path component except when we have
             * single "/"
             */
            if (resolved_len > 1) {
                if (backup_allowed > 0) {
                    resolved[resolved_len - 1] = '\0';
                    q = strrchr(resolved, '/') + 1;
                    *q = '\0';
                    resolved_len = q - resolved;
                    backup_allowed--;
                } else {
                    // Backing out past a symlink target.
                    // We don't allow this, because it can eliminate
                    // permissions we accumulated while descending.
                    if (m)
                        free(resolved);
                    errno = EPERM;
                    return (NULL);
                }
            }
            continue;
        }

        /*
         * Append the next path component and lstat() it.
         */
        resolved_len = my_strlcat(resolved, next_token, PATH_MAX);
        backup_allowed++;
        if (resolved_len >= PATH_MAX) {
            if (m)
                free(resolved);
            errno = ENAMETOOLONG;
            return (NULL);
        }
        if (lstat(resolved, &sb) != 0) {
            if (m)
                free(resolved);
            return (NULL);
        }
        if (S_ISLNK(sb.st_mode)) {
            if (symlinks++ > MAXSYMLINKS) {
                if (m)
                    free(resolved);
                errno = ELOOP;
                return (NULL);
            }
            /* Our changes start here:
             * It's a symlink, check for write permissions on the path where
             * it sits in, in which case we won't resolve and just error out. */
            int link_path_perms = policy->Lookup(resolved);
            if (link_path_perms & MAY_WRITE) {
                if (m)
                    free(resolved);
                errno = EPERM;
                return (NULL);
            } else {
                /* Accumulate permissions so far */
                *perms |= link_path_perms;
            }
            /* Original symlink lookup code */
            slen = readlink(resolved, symlink, sizeof(symlink) - 1);
            if (slen < 0) {
                if (m)
                    free(resolved);
                return (NULL);
            }
            symlink[slen] = '\0';
            if (symlink[0] == '/') {
                resolved[1] = 0;
                resolved_len = 1;
            } else if (resolved_len > 1) {
                /* Strip the last path component. */
                resolved[resolved_len - 1] = '\0';
                q = strrchr(resolved, '/') + 1;
                *q = '\0';
                resolved_len = q - resolved;
            }

            /*
             * If there are any path components left, then
             * append them to symlink. The result is placed
             * in `left'.
             */
            if (p != NULL) {
                if (symlink[slen - 1] != '/') {
                    if (slen + 1 >= (ssize_t)sizeof(symlink)) {
                        if (m)
                            free(resolved);
                        errno = ENAMETOOLONG;
                        return (NULL);
                    }
                    symlink[slen] = '/';
                    symlink[slen + 1] = 0;
                }
                left_len = my_strlcat(symlink, left, sizeof(symlink));
                if (left_len >= sizeof(left)) {
                    if (m)
                        free(resolved);
                    errno = ENAMETOOLONG;
                    return (NULL);
                }
            }
            left_len = base::strlcpy(left, symlink, sizeof(left));
            backup_allowed = 0;
        } else if (!S_ISDIR(sb.st_mode) && p != NULL) {
            if (m)
                free(resolved);
            errno = ENOTDIR;
            return (NULL);
        }
    }

    /*
     * Remove trailing slash except when the resolved pathname
     * is a single "/".
     */
    if (resolved_len > 1 && resolved[resolved_len - 1] == '/')
        resolved[resolved_len - 1] = '\0';

    /* Accumulate permissions. */
    *perms |= policy->Lookup(resolved);

    return (resolved);
}

}
