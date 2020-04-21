/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Logging.h"
#include "nsCocoaUtils.h"
#include "nsIFile.h"
#include "DesktopBackgroundImage.h"

#import <Foundation/Foundation.h>

extern mozilla::LazyLogModule gCocoaUtilsLog;
#undef LOG
#define LOG(...) MOZ_LOG(gCocoaUtilsLog, LogLevel::Debug, (__VA_ARGS__))

namespace mozilla {
namespace widget {

void SetDesktopImage(nsIFile* aImage) {
  nsAutoCString imagePath;
  nsresult rv = aImage->GetNativePath(imagePath);
  if (NS_FAILED(rv)) {
    LOG("%s ERROR: failed to get image path", __func__);
    return;
  }

  bool exists = false;
  rv = aImage->Exists(&exists);
  if (NS_FAILED(rv) || !exists) {
    LOG("%s ERROR: file \"%s\" does not exist", __func__, imagePath.get());
    return;
  }

  NSString* urlString = [NSString stringWithUTF8String:imagePath.get()];
  if (!urlString) {
    LOG("%s ERROR: null image path \"%s\"", __func__, imagePath.get());
    return;
  }

  NSURL* url = [NSURL fileURLWithPath:urlString];
  if (!url) {
    LOG("%s ERROR: null image path URL \"%s\"", __func__, imagePath.get());
    return;
  }

  // Only apply the background to the screen with focus
  NSScreen* currentScreen = [NSScreen mainScreen];
  if (!currentScreen) {
    LOG("%s ERROR: got null NSScreen", __func__);
    return;
  }

  // Use existing options for this screen
  NSDictionary* screenOptions =
      [[NSWorkspace sharedWorkspace] desktopImageOptionsForScreen:currentScreen];

  NSError* error = nil;
  if (![[NSWorkspace sharedWorkspace] setDesktopImageURL:url
                                               forScreen:currentScreen
                                                 options:screenOptions
                                                   error:&error]) {
    LOG("%s ERROR: setDesktopImageURL failed (%ld)", __func__, (long)[error code]);
  }
}

}  // namespace widget
}  // namespace mozilla
