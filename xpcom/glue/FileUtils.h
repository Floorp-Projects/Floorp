/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Benjamin Smedberg <benjamin@smedbergs.us>
 *   Taras Glek <tglek@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef mozilla_FileUtils_h
#define mozilla_FileUtils_h

#include "nscore.h" // nsnull

#if defined(XP_UNIX) || defined(XP_OS2)
# include <unistd.h>
#elif defined(XP_WIN)
# include <io.h>
#endif
#include "prio.h"

#include "mozilla/Scoped.h"

namespace mozilla {

/**
 * AutoFDClose is a RAII wrapper for PRFileDesc.
 *
 * Instances |PR_Close| their fds when they go out of scope.
 **/
struct ScopedClosePRFDTraits
{
  typedef PRFileDesc* type;
  static type empty() { return NULL; }
  static void release(type fd) {
    if (fd != NULL) {
      PR_Close(fd);
    }
  }
};
typedef Scoped<ScopedClosePRFDTraits> AutoFDClose;

/**
 * ScopedCloseFD is a RAII wrapper for POSIX file descriptors
 *
 * Instances |close()| their fds when they go out of scope.
 */
struct ScopedCloseFDTraits
{
  typedef int type;
  static type empty() { return -1; }
  static void release(type fd) {
    if (fd != -1) {
      close(fd);
    }
  }
};
typedef Scoped<ScopedCloseFDTraits> ScopedClose;

/**
 * Fallocate efficiently and continuously allocates files via fallocate-type APIs.
 * This is useful for avoiding fragmentation.
 * On sucess the file be padded with zeros to grow to aLength.
 *
 * @param aFD file descriptor.
 * @param aLength length of file to grow to.
 * @return true on success.
 */
NS_COM_GLUE bool fallocate(PRFileDesc *aFD, PRInt64 aLength);

} // namespace mozilla
#endif
