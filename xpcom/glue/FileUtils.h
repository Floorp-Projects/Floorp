/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
