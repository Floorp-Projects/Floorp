/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <AppKit/AppKit.h>
#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <IOKit/IOKitLib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/param.h>

#include "MacRunFromDmgUtils.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/intl/Localization.h"
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

/**
 * Opens a dialog to ask the user whether the app should be installed to their
 * Applications folder.  Returns true if the dialog is successfully opened and
 * the user accept, otherwise returns false.
 */
static bool AskUserIfWeShouldInstall() {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  // Try to get the localized strings:
  nsTArray<nsCString> resIds = {
      "branding/brand.ftl"_ns,
      "toolkit/global/run-from-dmg.ftl"_ns,
  };
  RefPtr<intl::Localization> l10n = intl::Localization::Create(resIds, true);

  ErrorResult rv;
  nsAutoCString mozTitle, mozMessage, mozInstall, mozDontInstall;
  l10n->FormatValueSync("prompt-to-install-title"_ns, {}, mozTitle, rv);
  if (rv.Failed()) {
    return false;
  }
  l10n->FormatValueSync("prompt-to-install-message"_ns, {}, mozMessage, rv);
  if (rv.Failed()) {
    return false;
  }
  l10n->FormatValueSync("prompt-to-install-yes-button"_ns, {}, mozInstall, rv);
  if (rv.Failed()) {
    return false;
  }
  l10n->FormatValueSync("prompt-to-install-no-button"_ns, {}, mozDontInstall, rv);
  if (rv.Failed()) {
    return false;
  }

  NSString* title = [NSString stringWithUTF8String:reinterpret_cast<const char*>(mozTitle.get())];
  NSString* message =
      [NSString stringWithUTF8String:reinterpret_cast<const char*>(mozMessage.get())];
  NSString* install =
      [NSString stringWithUTF8String:reinterpret_cast<const char*>(mozInstall.get())];
  NSString* dontInstall =
      [NSString stringWithUTF8String:reinterpret_cast<const char*>(mozDontInstall.get())];

  NSAlert* alert = [[[NSAlert alloc] init] autorelease];

  // Note that we don't set an icon since the app icon is used by default.
  [alert setAlertStyle:NSAlertStyleInformational];
  [alert setMessageText:title];
  [alert setInformativeText:message];
  // Note that if the user hits 'Enter' the "Install" button is activated,
  // whereas if they hit  'Space' the "Don't Install" button is activated.
  // That's standard behavior so probably desirable.
  [alert addButtonWithTitle:install];
  NSButton* dontInstallButton = [alert addButtonWithTitle:dontInstall];
  // Since the "Don't Install" button doesn't have the title "Cancel" we need
  // to map the Escape key to it manually:
  [dontInstallButton setKeyEquivalent:@"\e"];

  NSInteger result = [alert runModal];

  return result == NSAlertFirstButtonReturn;

  NS_OBJC_END_TRY_BLOCK_RETURN(false);
}

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
    // Does this ever happen?
    return false;
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
