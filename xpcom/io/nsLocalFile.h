/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK *****
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
#elif defined(XP_MACOSX)
#include "nsLocalFileOSX.h"
#elif defined(XP_UNIX) || defined(XP_BEOS)
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
      case ENOENT:
        return NS_ERROR_FILE_TARGET_DOES_NOT_EXIST;
      case ENOTDIR:
        return NS_ERROR_FILE_DESTINATION_NOT_DIR;
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
      /*
       * On AIX 4.3, ENOTEMPTY is defined as EEXIST,
       * so there can't be cases for both without
       * preprocessing.
       */
#if ENOTEMPTY != EEXIST
      case ENOTEMPTY:
        return NS_ERROR_FILE_DIR_NOT_EMPTY;
#endif /* ENOTEMPTY != EEXIST */
      default:
        return NS_ERROR_FAILURE;
    }
}

#define NSRESULT_FOR_ERRNO() nsresultForErrno(errno)

void NS_StartupLocalFile();
void NS_ShutdownLocalFile();

#endif
