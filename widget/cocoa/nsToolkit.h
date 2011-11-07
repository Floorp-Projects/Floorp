/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsToolkit_h_
#define nsToolkit_h_

#include "nscore.h"
#include "nsCocoaFeatures.h"

#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#import <objc/Object.h>
#import <IOKit/IOKitLib.h>

class nsToolkit
{
public:
                     nsToolkit();
  virtual            ~nsToolkit();

  static nsToolkit*  GetToolkit();

  static void Shutdown() {
    delete gToolkit;
    gToolkit = nullptr;
  }

  static void        PostSleepWakeNotification(const char* aNotification);

  static nsresult    SwizzleMethods(Class aClass, SEL orgMethod, SEL posedMethod,
                                    bool classMethods = false);

  void               RegisterForAllProcessMouseEvents();
  void               UnregisterAllProcessMouseEventHandlers();

protected:

  nsresult           RegisterForSleepWakeNotifcations();
  void               RemoveSleepWakeNotifcations();

protected:

  static nsToolkit*  gToolkit;

  CFRunLoopSourceRef mSleepWakeNotificationRLS;
  io_object_t        mPowerNotifier;

  CFMachPortRef      mEventTapPort;
  CFRunLoopSourceRef mEventTapRLS;
};

#endif // nsToolkit_h_
