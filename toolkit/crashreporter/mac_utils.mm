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

  nsAutoString nameStr;
  nsAutoString reasonStr;

  mozilla::Unused << mozilla::CopyCocoaStringToXPCOMString(name, nameStr);
  mozilla::Unused << mozilla::CopyCocoaStringToXPCOMString(reason, reasonStr);

  outString.AssignLiteral("\nObj-C Exception data:\n");
  AppendUTF16toUTF8(nameStr, outString);
  outString.AppendLiteral(": ");
  AppendUTF16toUTF8(reasonStr, outString);
}
