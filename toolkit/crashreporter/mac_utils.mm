/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <Foundation/Foundation.h>

#include "mac_utils.h"
#include "nsXPCOM.h"

void GetObjCExceptionInfo(void* inException, nsACString& outString)
{
  NSException* e = (NSException*)inException;

  NSString* name = [e name];
  NSString* reason = [e reason];
  unsigned int nameLength = [name length];
  unsigned int reasonLength = [reason length];

  unichar* nameBuffer = (unichar*)NS_Alloc(sizeof(unichar) * (nameLength + 1));
  if (!nameBuffer)
    return;
  unichar* reasonBuffer = (unichar*)NS_Alloc(sizeof(unichar) * (reasonLength + 1));
  if (!reasonBuffer) {
    NS_Free(nameBuffer);
    return;
  }

  [name getCharacters:nameBuffer];
  [reason getCharacters:reasonBuffer];
  nameBuffer[nameLength] = '\0';
  reasonBuffer[reasonLength] = '\0';

  outString.AssignLiteral("\nObj-C Exception data:\n");
  AppendUTF16toUTF8(reinterpret_cast<const PRUnichar*>(nameBuffer), outString);
  outString.AppendLiteral(": ");
  AppendUTF16toUTF8(reinterpret_cast<const PRUnichar*>(reasonBuffer), outString);

  NS_Free(nameBuffer);
  NS_Free(reasonBuffer);
}
