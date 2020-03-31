/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OSReauthenticator.h"

#include "nsCocoaUtils.h"

using namespace mozilla;

#include <CoreFoundation/CoreFoundation.h>
#include <LocalAuthentication/LocalAuthentication.h>

static const int32_t kPasswordNotSetErrorCode = -1000;

nsresult ReauthenticateUserMacOS(const nsAString& aPrompt,
                                 /* out */ bool& aReauthenticated) {
  // The idea here is that we ask to be authorized to unlock the user's session.
  // This should cause a prompt to come up for the user asking them for their
  // password. If they correctly enter it, we'll set aReauthenticated to true.

  LAContext* context = [[LAContext alloc] init];
  NSString* prompt = nsCocoaUtils::ToNSString(aPrompt);

  dispatch_semaphore_t sema = dispatch_semaphore_create(0);

  __block BOOL biometricSuccess = NO;  // mark variable r/w across the block

  // Note: This is an async callback in an already-async Promise chain.
  [context evaluatePolicy:LAPolicyDeviceOwnerAuthentication
          localizedReason:prompt
                    reply:^(BOOL success, NSError* error) {
                      dispatch_async(dispatch_get_main_queue(), ^{
                        // error is not particularly useful in this context, and we have no
                        // mechanism to really return it. We could use it to set the nsresult,
                        // but this is a best-effort mechanism and there's no particular case for
                        // propagating up XPCOM. The one exception being a user account that
                        // has no passcode set, which we handle below.
                        if (success || [error code] == kPasswordNotSetErrorCode) {
                          biometricSuccess = YES;
                        }
                        dispatch_semaphore_signal(sema);
                      });
                    }];

  // What we want to do here is convert this into a blocking call, since
  // our calling methods expect us to block and set aReauthenticated on return.
  dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
  dispatch_release(sema);
  sema = NULL;

  aReauthenticated = biometricSuccess;

  [context release];
  return NS_OK;
}
