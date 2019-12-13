/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>

#include <sys/param.h>

#include "MacAutoreleasePool.h"
#include "nsMacRemoteClient.h"
#include "RemoteUtils.h"

using namespace mozilla;

nsresult nsMacRemoteClient::Init() { return NS_OK; }

nsresult nsMacRemoteClient::SendCommandLine(const char* aProgram, const char* aProfile,
                                            int32_t argc, char** argv,
                                            const char* aDesktopStartupID, char** aResponse,
                                            bool* aSucceeded) {
  mozilla::MacAutoreleasePool pool;

  *aSucceeded = false;

  nsString className;
  BuildClassName(aProgram, aProfile, className);
  NSString* serverNameString =
      [NSString stringWithCharacters:reinterpret_cast<const unichar*>(className.get())
                              length:className.Length()];

  CFMessagePortRef messageServer = CFMessagePortCreateRemote(0, (CFStringRef)serverNameString);

  if (messageServer) {
    // Getting current process directory
    char cwdPtr[MAXPATHLEN + 1];
    getcwd(cwdPtr, MAXPATHLEN + 1);

    NSMutableArray* argumentsArray = [NSMutableArray array];
    for (int i = 0; i < argc; i++) {
      NSString* argument = [NSString stringWithUTF8String:argv[i]];
      [argumentsArray addObject:argument];
    }
    NSDictionary* dict = @{@"args" : argumentsArray};

    NSData* data = [NSKeyedArchiver archivedDataWithRootObject:dict];

    CFMessagePortSendRequest(messageServer, 0, (CFDataRef)data, 10.0, 0.0, NULL, NULL);

    CFMessagePortInvalidate(messageServer);
    CFRelease(messageServer);
    *aSucceeded = true;

  } else {
    // Remote Server not found. Doing nothing.
  }

  return NS_OK;
}
