/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <IOKit/IOKitLib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/param.h>

#include "MacRunFromDmgUtils.h"

#include "nsCocoaFeatures.h"
#include "nsCommandLine.h"
#include "nsCommandLineServiceMac.h"
#include "nsILocalFileMac.h"
#include "nsIMacDockSupport.h"
#include "nsObjCExceptions.h"
#include "nsString.h"

// For IOKit docs, see:
// https://developer.apple.com/documentation/iokit
// https://developer.apple.com/library/archive/documentation/DeviceDrivers/Conceptual/IOKitFundamentals/

namespace mozilla {
namespace MacRunFromDmgUtils {

bool IsAppRunningFromDmg() {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  const char* path = [[[NSBundle mainBundle] bundlePath] fileSystemRepresentation];

  struct statfs statfsBuf;
  if (statfs(path, &statfsBuf) != 0) {
    return false;
  }

  // Optimization to minimize impact on startup time:
  if (!(statfsBuf.f_flags & MNT_RDONLY)) {
    return false;
  }

  // Get the "BSD device name" ("diskXsY", as found in /dev) of the filesystem
  // our app is on so we can use IOKit to get its IOMedia object:
  const char devDirPath[] = "/dev/";
  const int devDirPathLength = strlen(devDirPath);
  if (strncmp(statfsBuf.f_mntfromname, devDirPath, devDirPathLength) != 0) {
    // This has been observed to happen under App Translocation. In this case,
    // path is the translocated path, and f_mntfromname is the path under
    // /Volumes. Do another stat on that path.
    nsCString volumesPath(statfsBuf.f_mntfromname);
    if (statfs(volumesPath.get(), &statfsBuf) != 0) {
      return false;
    }

    if (strncmp(statfsBuf.f_mntfromname, devDirPath, devDirPathLength) != 0) {
      // It still doesn't begin with /dev/, bail out.
      return false;
    }
  }
  const char* bsdDeviceName = statfsBuf.f_mntfromname + devDirPathLength;

  // Get the IOMedia object:
  // (Note: IOServiceGetMatchingServices takes ownership of serviceDict's ref.)
  CFMutableDictionaryRef serviceDict = IOBSDNameMatching(kIOMasterPortDefault, 0, bsdDeviceName);
  if (!serviceDict) {
    return false;
  }
  io_service_t media = IOServiceGetMatchingService(kIOMasterPortDefault, serviceDict);
  if (!media || !IOObjectConformsTo(media, "IOMedia")) {
    return false;
  }

  // Search the parent chain for a service implementing the disk image class
  // (taking care to start with `media` itself):
  io_service_t imageDrive = IO_OBJECT_NULL;
  io_iterator_t iter;
  if (IORegistryEntryCreateIterator(media, kIOServicePlane,
                                    kIORegistryIterateRecursively | kIORegistryIterateParents,
                                    &iter) != KERN_SUCCESS) {
    IOObjectRelease(media);
    return false;
  }
  const char* imageClass =
      nsCocoaFeatures::macOSVersionMajor() >= 12 ? "AppleDiskImageDevice" : "IOHDIXHDDrive";
  for (imageDrive = media; imageDrive; imageDrive = IOIteratorNext(iter)) {
    if (IOObjectConformsTo(imageDrive, imageClass)) {
      break;
    }
    IOObjectRelease(imageDrive);
  }
  IOObjectRelease(iter);

  if (imageDrive) {
    IOObjectRelease(imageDrive);
    return true;
  }
  return false;

  NS_OBJC_END_TRY_BLOCK_RETURN(false);
}

}  // namespace MacRunFromDmgUtils
}  // namespace mozilla
