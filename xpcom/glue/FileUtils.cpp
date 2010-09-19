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
#if defined(XP_UNIX)
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#elif defined(XP_WIN)
#include <windows.h>
#endif

#include "nscore.h"
#include "private/pprio.h"
#include "mozilla/FileUtils.h"

bool 
mozilla::fallocate(PRFileDesc *aFD, PRInt64 aLength) 
{
#if defined(HAVE_POSIX_FALLOCATE)
  return posix_fallocate(PR_FileDesc2NativeHandle(aFD), 0, aLength) == 0;
#elif defined(XP_WIN)
  return PR_Seek64(aFD, aLength, PR_SEEK_SET) == aLength
    && 0 != SetEndOfFile((HANDLE)PR_FileDesc2NativeHandle(aFD));
#elif defined(XP_MACOSX)
  int fd = PR_FileDesc2NativeHandle(aFD);
  fstore_t store = {F_ALLOCATECONTIG, F_PEOFPOSMODE, 0, aLength};
  // Try to get a continous chunk of disk space
  int ret = fcntl(fd, F_PREALLOCATE, &store);
	if(-1 == ret){
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
  return nWrite == 1;
#endif
  return false;
}
