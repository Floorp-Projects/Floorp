/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_FileUtils_h
#define mozilla_FileUtils_h

#include "nscore.h" // nullptr

#if defined(XP_UNIX) || defined(XP_OS2)
# include <unistd.h>
#elif defined(XP_WIN)
# include <io.h>
#endif
#include "prio.h"

#include "mozilla/Scoped.h"
#include "nsIFile.h"
#include <errno.h>
#include <limits.h>

namespace mozilla {

#if defined(XP_WIN)
typedef void* filedesc_t;
typedef const wchar_t* pathstr_t;
#else
typedef int filedesc_t;
typedef const char* pathstr_t;
#endif

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
      while ((close(fd) == -1) && (errno == EINTR)) {
        ;
      }
    }
  }
};
typedef Scoped<ScopedCloseFDTraits> ScopedClose;

#if !defined(XPCOM_GLUE)

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
 * Fallocate efficiently and continuously allocates files via fallocate-type APIs.
 * This is useful for avoiding fragmentation.
 * On sucess the file be padded with zeros to grow to aLength.
 *
 * @param aFD file descriptor.
 * @param aLength length of file to grow to.
 * @return true on success.
 */
NS_COM_GLUE bool fallocate(PRFileDesc *aFD, int64_t aLength);

/**
 * Use readahead to preload shared libraries into the file cache before loading.
 * WARNING: This function should not be used without a telemetry field trial 
 *          demonstrating a clear performance improvement!
 *
 * @param aFile nsIFile representing path to shared library
 */
NS_COM_GLUE void ReadAheadLib(nsIFile* aFile);

/**
 * Use readahead to preload a file into the file cache before reading.
 * WARNING: This function should not be used without a telemetry field trial 
 *          demonstrating a clear performance improvement!
 *
 * @param aFile nsIFile representing path to shared library
 * @param aOffset Offset into the file to begin preloading
 * @param aCount Number of bytes to preload (SIZE_MAX implies file size)
 * @param aOutFd Pointer to file descriptor. If specified, ReadAheadFile will
 *        return its internal, opened file descriptor instead of closing it.
 */
NS_COM_GLUE void ReadAheadFile(nsIFile* aFile, const size_t aOffset = 0,
                               const size_t aCount = SIZE_MAX,
                               filedesc_t* aOutFd = nullptr);

#endif // !defined(XPCOM_GLUE)

/**
 * Use readahead to preload shared libraries into the file cache before loading.
 * WARNING: This function should not be used without a telemetry field trial 
 *          demonstrating a clear performance improvement!
 *
 * @param aFilePath path to shared library
 */
NS_COM_GLUE void ReadAheadLib(pathstr_t aFilePath);

/**
 * Use readahead to preload a file into the file cache before loading.
 * WARNING: This function should not be used without a telemetry field trial 
 *          demonstrating a clear performance improvement!
 *
 * @param aFilePath path to shared library
 * @param aOffset Offset into the file to begin preloading
 * @param aCount Number of bytes to preload (SIZE_MAX implies file size)
 * @param aOutFd Pointer to file descriptor. If specified, ReadAheadFile will
 *        return its internal, opened file descriptor instead of closing it.
 */
NS_COM_GLUE void ReadAheadFile(pathstr_t aFilePath, const size_t aOffset = 0,
                               const size_t aCount = SIZE_MAX,
                               filedesc_t* aOutFd = nullptr);

/**
 * Use readahead to preload a file into the file cache before reading.
 * When this function exits, the file pointer is guaranteed to be in the same
 * position it was in before this function was called.
 * WARNING: This function should not be used without a telemetry field trial 
 *          demonstrating a clear performance improvement!
 *
 * @param aFd file descriptor opened for read access
 * (on Windows, file must be opened with FILE_FLAG_SEQUENTIAL_SCAN)
 * @param aOffset Offset into the file to begin preloading
 * @param aCount Number of bytes to preload (SIZE_MAX implies file size)
 */
NS_COM_GLUE void ReadAhead(filedesc_t aFd, const size_t aOffset = 0,
                           const size_t aCount = SIZE_MAX);


} // namespace mozilla
#endif
