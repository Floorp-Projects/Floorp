/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>

#include "nsMacWebAppUtils.h"
#include "nsCOMPtr.h"
#include "nsCocoaUtils.h"
#include "nsString.h"

// This must be included last:
#include "nsObjCExceptions.h"

// Find the path to the app with the given bundleIdentifier, if any.
// Note that the OS will return the path to the newest binary, if there is more than one.
// The determination of 'newest' is complex and beyond the scope of this comment.

NS_IMPL_ISUPPORTS(nsMacWebAppUtils, nsIMacWebAppUtils)

NS_IMETHODIMP nsMacWebAppUtils::PathForAppWithIdentifier(const nsAString& bundleIdentifier,
                                                         nsAString& outPath) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  outPath.Truncate();

  nsAutoreleasePool localPool;

  // note that the result of this expression might be nil, meaning no matching app was found.
  NSString* temp = [[NSWorkspace sharedWorkspace]
      absolutePathForAppBundleWithIdentifier:
          [NSString stringWithCharacters:reinterpret_cast<const unichar*>(
                                             ((nsString)bundleIdentifier).get())
                                  length:((nsString)bundleIdentifier).Length()]];

  if (temp) {
    // Copy out the resultant absolute path into outPath if non-nil.
    nsCocoaUtils::GetStringForNSString(temp, outPath);
  }

  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

NS_IMETHODIMP nsMacWebAppUtils::LaunchAppWithIdentifier(const nsAString& bundleIdentifier) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  nsAutoreleasePool localPool;

  // Note this might return false, meaning the app wasnt launched for some reason.
  BOOL success = [[NSWorkspace sharedWorkspace]
       launchAppWithBundleIdentifier:[NSString
                                         stringWithCharacters:reinterpret_cast<const unichar*>(
                                                                  ((nsString)bundleIdentifier)
                                                                      .get())
                                                       length:((nsString)bundleIdentifier).Length()]
                             options:(NSWorkspaceLaunchOptions)0
      additionalEventParamDescriptor:nil
                    launchIdentifier:NULL];

  return success ? NS_OK : NS_ERROR_FAILURE;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

NS_IMETHODIMP nsMacWebAppUtils::TrashApp(const nsAString& path, nsITrashAppCallback* aCallback) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  if (NS_WARN_IF(!aCallback)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsITrashAppCallback> callback = aCallback;

  NSString* tempString =
      [NSString stringWithCharacters:reinterpret_cast<const unichar*>(((nsString)path).get())
                              length:path.Length()];

  [[NSWorkspace sharedWorkspace]
            recycleURLs:[NSArray arrayWithObject:[NSURL fileURLWithPath:tempString]]
      completionHandler:^(NSDictionary* newURLs, NSError* error) {
        nsresult rv = (error == nil) ? NS_OK : NS_ERROR_FAILURE;
        callback->TrashAppFinished(rv);
      }];

  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}
