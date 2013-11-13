/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This Original Code has been modified by IBM Corporation. Modifications made by IBM 
 * described herein are Copyright (c) International Business Machines Corporation, 2000.
 * Modifications to Mozilla code or documentation identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 04/20/2000       IBM Corp.      OS/2 build.
 */

#ifndef _NS_LOCAL_FILE_H_
#define _NS_LOCAL_FILE_H_

#include "nscore.h"

#define NS_LOCAL_FILE_CID {0x2e23e220, 0x60be, 0x11d3, {0x8c, 0x4a, 0x00, 0x00, 0x64, 0x65, 0x73, 0x74}}

#define NS_DECL_NSLOCALFILE_UNICODE_METHODS                                                      \
    nsresult AppendUnicode(const PRUnichar *aNode);                                              \
    nsresult GetUnicodeLeafName(PRUnichar **aLeafName);                                          \
    nsresult SetUnicodeLeafName(const PRUnichar *aLeafName);                                     \
    nsresult CopyToUnicode(nsIFile *aNewParentDir, const PRUnichar *aNewLeafName);               \
    nsresult CopyToFollowingLinksUnicode(nsIFile *aNewParentDir, const PRUnichar *aNewLeafName); \
    nsresult MoveToUnicode(nsIFile *aNewParentDir, const PRUnichar *aNewLeafName);               \
    nsresult GetUnicodeTarget(PRUnichar **aTarget);                                              \
    nsresult GetUnicodePath(PRUnichar **aPath);                                                  \
    nsresult InitWithUnicodePath(const PRUnichar *aPath);                                        \
    nsresult AppendRelativeUnicodePath(const PRUnichar *aRelativePath);

// nsXPComInit needs to know about how we are implemented,
// so here we will export it.  Other users should not depend
// on this.

#include <errno.h>
#include "nsILocalFile.h"

#ifdef XP_WIN
#include "nsLocalFileWin.h"
#elif defined(XP_UNIX)
#include "nsLocalFileUnix.h"
#elif defined(XP_OS2)
#include "nsLocalFileOS2.h"
#else
#error NOT_IMPLEMENTED
#endif

#define NSRESULT_FOR_RETURN(ret) (((ret) < 0) ? NSRESULT_FOR_ERRNO() : NS_OK)

inline nsresult
nsresultForErrno(int err)
{
    switch (err) {
      case 0:
        return NS_OK;
#ifdef EDQUOT
      case EDQUOT: /* Quota exceeded */
        // FALLTHROUGH to return NS_ERROR_FILE_DISK_FULL
#endif
      case ENOSPC:
        return NS_ERROR_FILE_DISK_FULL;
#ifdef EISDIR
      case EISDIR:    /*      Is a directory. */
        return NS_ERROR_FILE_IS_DIRECTORY;
#endif
      case ENAMETOOLONG: 
        return NS_ERROR_FILE_NAME_TOO_LONG;
      case ENOEXEC:  /*     Executable file format error. */
        return NS_ERROR_FILE_EXECUTION_FAILED;
      case ENOENT:
        return NS_ERROR_FILE_TARGET_DOES_NOT_EXIST;
      case ENOTDIR:
        return NS_ERROR_FILE_DESTINATION_NOT_DIR;
#ifdef ELOOP
      case ELOOP:
        return NS_ERROR_FILE_UNRESOLVABLE_SYMLINK;
#endif /* ELOOP */
#ifdef ENOLINK
      case ENOLINK:
        return NS_ERROR_FILE_UNRESOLVABLE_SYMLINK;
#endif /* ENOLINK */
      case EEXIST:
        return NS_ERROR_FILE_ALREADY_EXISTS;
#ifdef EPERM
      case EPERM:
#endif /* EPERM */
      case EACCES:
        return NS_ERROR_FILE_ACCESS_DENIED;
#ifdef EROFS
      case EROFS: /*     Read-only file system. */
        return NS_ERROR_FILE_READ_ONLY;
#endif
      /*
       * On AIX 4.3, ENOTEMPTY is defined as EEXIST,
       * so there can't be cases for both without
       * preprocessing.
       */
#if ENOTEMPTY != EEXIST
      case ENOTEMPTY:
        return NS_ERROR_FILE_DIR_NOT_EMPTY;
#endif /* ENOTEMPTY != EEXIST */
        /* Note that nsIFile.createUnique() returns
           NS_ERROR_FILE_TOO_BIG when it cannot create a temporary
           file with a unique filename.
           See https://developer.mozilla.org/en-US/docs/Table_Of_Errors 
           Other usages of NS_ERROR_FILE_TOO_BIG in the source tree
           are in line with the POSIX semantics of EFBIG.
           So this is a reasonably good approximation.
        */
      case EFBIG: /*     File too large. */
        return NS_ERROR_FILE_TOO_BIG;

      default:
        return NS_ERROR_FAILURE;
    }
}

#define NSRESULT_FOR_ERRNO() nsresultForErrno(errno)

void NS_StartupLocalFile();
void NS_ShutdownLocalFile();

#endif
