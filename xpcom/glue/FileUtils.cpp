/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if defined(XP_UNIX)
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#elif defined(XP_WIN)
#include <windows.h>
#elif defined(XP_OS2)
#define INCL_DOSFILEMGR
#include <os2.h>
#endif

#include "nscore.h"
#include "private/pprio.h"
#include "mozilla/FileUtils.h"
#include "mozilla/FunctionTimer.h"

bool 
mozilla::fallocate(PRFileDesc *aFD, PRInt64 aLength) 
{
  NS_TIME_FUNCTION;
#if defined(HAVE_POSIX_FALLOCATE)
  return posix_fallocate(PR_FileDesc2NativeHandle(aFD), 0, aLength) == 0;
#elif defined(XP_WIN)
  PROffset64 oldpos = PR_Seek64(aFD, 0, PR_SEEK_CUR);
  if (oldpos == -1)
    return false;

  if (PR_Seek64(aFD, aLength, PR_SEEK_SET) != aLength)
    return false;

  bool retval = (0 != SetEndOfFile((HANDLE)PR_FileDesc2NativeHandle(aFD)));

  PR_Seek64(aFD, oldpos, PR_SEEK_SET);
  return retval;
#elif defined(XP_OS2)
  return aLength <= PR_UINT32_MAX
    && 0 == DosSetFileSize(PR_FileDesc2NativeHandle(aFD), (PRUint32)aLength);
#elif defined(XP_MACOSX)
  int fd = PR_FileDesc2NativeHandle(aFD);
  fstore_t store = {F_ALLOCATECONTIG, F_PEOFPOSMODE, 0, aLength};
  // Try to get a continous chunk of disk space
  int ret = fcntl(fd, F_PREALLOCATE, &store);
  if (-1 == ret) {
    // OK, perhaps we are too fragmented, allocate non-continuous
    store.fst_flags = F_ALLOCATEALL;
    ret = fcntl(fd, F_PREALLOCATE, &store);
    if (-1 == ret)
      return false;
  }
  return 0 == ftruncate(fd, aLength);
#elif defined(XP_UNIX)
  // The following is copied from fcntlSizeHint in sqlite
  /* If the OS does not have posix_fallocate(), fake it. First use
  ** ftruncate() to set the file size, then write a single byte to
  ** the last byte in each block within the extended region. This
  ** is the same technique used by glibc to implement posix_fallocate()
  ** on systems that do not have a real fallocate() system call.
  */
  PROffset64 oldpos = PR_Seek64(aFD, 0, PR_SEEK_CUR);
  if (oldpos == -1)
    return false;

  struct stat buf;
  int fd = PR_FileDesc2NativeHandle(aFD);
  if (fstat(fd, &buf))
    return false;

  if (buf.st_size >= aLength)
    return false;

  const int nBlk = buf.st_blksize;

  if (!nBlk)
    return false;

  if (ftruncate(fd, aLength))
    return false;

  int nWrite; // Return value from write()
  PRInt64 iWrite = ((buf.st_size + 2 * nBlk - 1) / nBlk) * nBlk - 1; // Next offset to write to
  do {
    nWrite = 0;
    if (PR_Seek64(aFD, iWrite, PR_SEEK_SET) == iWrite)
      nWrite = PR_Write(aFD, "", 1);
    iWrite += nBlk;
  } while (nWrite == 1 && iWrite < aLength);

  PR_Seek64(aFD, oldpos, PR_SEEK_SET);
  return nWrite == 1;
#endif
  return false;
}
