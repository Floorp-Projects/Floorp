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
 * The Original Code is XPCOM utility functions for Mac OS X
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Mark Mentovai <mark@moxienet.com> (Original Author)
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

#include "nsMacUtilsImpl.h"

#include <CoreFoundation/CoreFoundation.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/sysctl.h>
#include <mach-o/fat.h>

NS_IMPL_ISUPPORTS1(nsMacUtilsImpl, nsIMacUtils)

/* readonly attribute boolean isUniversalBinary; */
// True when the main executable is a fat file supporting at least
// ppc and x86 (universal binary).
NS_IMETHODIMP nsMacUtilsImpl::GetIsUniversalBinary(PRBool *aIsUniversalBinary)
{
  static PRBool sInitialized = PR_FALSE,
                sIsUniversalBinary = PR_FALSE;

  if (sInitialized) {
    *aIsUniversalBinary = sIsUniversalBinary;
    return NS_OK;
  }

  PRBool foundPPC = PR_FALSE,
         foundX86 = PR_FALSE;
  CFURLRef executableURL = nsnull;
  int fd = -1;

  CFBundleRef mainBundle;
  if (!(mainBundle = ::CFBundleGetMainBundle()))
    goto done;

  if (!(executableURL = ::CFBundleCopyExecutableURL(mainBundle)))
    goto done;

  char executablePath[PATH_MAX];
  if (!::CFURLGetFileSystemRepresentation(executableURL, PR_TRUE,
                                          (UInt8*) executablePath,
                                          sizeof(executablePath)))
    goto done;

  if ((fd = open(executablePath, O_RDONLY)) == -1)
    goto done;

  struct fat_header fatHeader;
  if (read(fd, &fatHeader, sizeof(fatHeader)) != sizeof(fatHeader))
    goto done;

  // The fat header is always stored on disk as big-endian.
  fatHeader.magic = CFSwapInt32BigToHost(fatHeader.magic);
  fatHeader.nfat_arch = CFSwapInt32BigToHost(fatHeader.nfat_arch);

  // Main executable is thin.
  if (fatHeader.magic != FAT_MAGIC)
    goto done;

  // Loop over each architecture in the file.  We're presently only
  // interested in 32-bit PPC and x86.
  for (PRUint32 i = 0 ; i < fatHeader.nfat_arch ; i++) {
    struct fat_arch fatArch;
    if (read(fd, &fatArch, sizeof(fatArch)) != sizeof(fatArch))
      goto done;

    // This is still part of the fat header, so byte-swap as needed.
    fatArch.cputype = CFSwapInt32BigToHost(fatArch.cputype);

    // Don't mask out the ABI bits.  This allows identification of ppc64
    // as distinct from ppc.  CPU_TYPE_X86 is preferred to CPU_TYPE_I386
    // but does not exist prior to the 10.4 headers.
    if (fatArch.cputype == CPU_TYPE_POWERPC)
      foundPPC = PR_TRUE;
    else if (fatArch.cputype == CPU_TYPE_I386)
      foundX86 = PR_TRUE;
  }

  if (foundPPC && foundX86)
    sIsUniversalBinary = PR_TRUE;

done:
  if (fd != -1)
    close(fd);
  if (executableURL)
    ::CFRelease(executableURL);

  *aIsUniversalBinary = sIsUniversalBinary;
  sInitialized = PR_TRUE;

  return NS_OK;
}

/* readonly attribute boolean isTranslated; */
// True when running under binary translation (Rosetta).
NS_IMETHODIMP nsMacUtilsImpl::GetIsTranslated(PRBool *aIsTranslated)
{
#ifdef __ppc__
  static PRBool  sInitialized = PR_FALSE;

  // Initialize sIsNative to 1.  If the sysctl fails because it doesn't
  // exist, then translation is not possible, so the process must not be
  // running translated.
  static PRInt32 sIsNative = 1;

  if (!sInitialized) {
    size_t sz = sizeof(sIsNative);
    sysctlbyname("sysctl.proc_native", &sIsNative, &sz, NULL, 0);
    sInitialized = PR_TRUE;
  }

  *aIsTranslated = !sIsNative;
#else
  // Translation only exists for ppc code.  Other architectures aren't
  // translated.
  *aIsTranslated = PR_FALSE;
#endif

  return NS_OK;
}
