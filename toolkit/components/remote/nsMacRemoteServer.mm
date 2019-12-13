/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>
#import <CoreServices/CoreServices.h>

#include "MacAutoreleasePool.h"
#include "nsCOMPtr.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIWindowMediator.h"
#include "nsIWidget.h"
#include "nsICommandLineRunner.h"
#include "nsICommandLine.h"
#include "nsCommandLine.h"
#include "nsIDocShell.h"
#include "nsMacRemoteServer.h"
#include "nsXPCOM.h"
#include "RemoteUtils.h"

CFDataRef messageServerCallback(CFMessagePortRef aLocal, int32_t aMsgid, CFDataRef aData,
                                void* aInfo) {
  // One of the clients submitted a structure.
  static_cast<nsMacRemoteServer*>(aInfo)->HandleCommandLine(aData);

  return NULL;
}

// aData contains serialized Dictionary, which in turn contains command line arguments
void nsMacRemoteServer::HandleCommandLine(CFDataRef aData) {
  mozilla::MacAutoreleasePool pool;

  if (aData) {
    NSDictionary* dict = [NSKeyedUnarchiver unarchiveObjectWithData:(NSData*)aData];
    if (dict && [dict isKindOfClass:[NSDictionary class]]) {
      NSArray* args = dict[@"args"];
      if (!args) {
        NS_ERROR("Wrong parameters passed to the Remote Server");
        return;
      }

      nsCOMPtr<nsICommandLineRunner> cmdLine(new nsCommandLine());

      // Converting Objective-C array into a C array,
      // which nsICommandLineRunner understands.
      int argc = [args count];
      const char** argv = new const char*[argc];
      for (int i = 0; i < argc; i++) {
        const char* arg = [[args objectAtIndex:i] UTF8String];
        argv[i] = arg;
      }

      nsresult rv = cmdLine->Init(argc, argv, nullptr, nsICommandLine::STATE_REMOTE_AUTO);

      // Cleaning up C array.
      delete[] argv;

      if (NS_FAILED(rv)) {
        NS_ERROR("Error initializing command line.");
        return;
      }

      // Processing the command line, passed from a remote instance
      // in the current instance.
      cmdLine->Run();

      // And bring the app's window to front.
      [[NSRunningApplication currentApplication]
          activateWithOptions:NSApplicationActivateIgnoringOtherApps];
    }
  }
}

nsresult nsMacRemoteServer::Startup(const char* aAppName, const char* aProfileName) {
  // This is the first instance ever.
  // Let's register a notification listener here,
  // In case future instances would want to notify us about command line arguments
  // passed to them. Note, that if mozilla process is restarting, we still need to
  // register for notifications.

  mozilla::MacAutoreleasePool pool;

  nsString className;
  BuildClassName(aAppName, aProfileName, className);

  NSString* serverNameString =
      [NSString stringWithCharacters:reinterpret_cast<const unichar*>(className.get())
                              length:className.Length()];

  CFMessagePortContext context;
  context.copyDescription = NULL;
  context.info = this;
  context.release = NULL;
  context.retain = NULL;
  context.version = NULL;
  mMessageServer = CFMessagePortCreateLocal(NULL, (CFStringRef)serverNameString,
                                            messageServerCallback, &context, NULL);
  if (!mMessageServer) {
    return NS_ERROR_FAILURE;
  }
  mRunLoopSource = CFMessagePortCreateRunLoopSource(NULL, mMessageServer, 0);
  if (!mRunLoopSource) {
    CFRelease(mMessageServer);
    mMessageServer = NULL;
    return NS_ERROR_FAILURE;
  }
  CFRunLoopRef runLoop = CFRunLoopGetMain();
  CFRunLoopAddSource(runLoop, mRunLoopSource, kCFRunLoopDefaultMode);

  return NS_OK;
}

void nsMacRemoteServer::Shutdown() {
  // 1) Invalidate server connection
  if (mMessageServer) {
    CFMessagePortInvalidate(mMessageServer);
  }

  // 2) Release run loop source
  if (mRunLoopSource) {
    CFRelease(mRunLoopSource);
    mRunLoopSource = NULL;
  }

  // 3) Release server connection
  if (mMessageServer) {
    CFRelease(mMessageServer);
    mMessageServer = NULL;
  }
}
