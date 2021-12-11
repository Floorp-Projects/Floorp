/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <Foundation/Foundation.h>

#include "mac_utils.h"
#include "nsXPCOM.h"
#include "mozilla/MacStringHelpers.h"
#include "mozilla/Unused.h"

void GetObjCExceptionInfo(void* inException, nsACString& outString) {
  NSException* e = (NSException*)inException;

  NSString* name = [e name];
  NSString* reason = [e reason];
  NSArray* stackAddresses = [e callStackReturnAddresses];

  outString.AssignLiteral("\nObj-C Exception data:\n");
  outString.Append([name UTF8String]);
  outString.AppendLiteral(": ");
  outString.Append([reason UTF8String]);
  outString.AppendLiteral("\n\nThrown at stack:\n");
  for (NSNumber* address in stackAddresses) {
    outString.AppendPrintf("0x%lx\n", [address unsignedIntegerValue]);
  }
  outString.AppendLiteral("\n");
}
