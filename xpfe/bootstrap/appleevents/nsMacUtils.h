/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *  Simon Fraser <sfraser@netscape.com>
 */


#ifndef nsMacUtils_h_
#define nsMacUtils_h_

#include <MacTypes.h>
#include <Memory.h>
#include <Errors.h>
#include <Resources.h>

#define Min(a, b)   ((a) < (b) ? (a) : (b))

#ifdef __cplusplus
extern "C" {
#endif

OSErr MyNewHandle(long length, Handle *outHandle);
void MyDisposeHandle(Handle theHandle);

void MyHLock(Handle theHandle);
void MyHUnlock(Handle theHandle);



OSErr MyNewBlockClear(long length, void* *outBlock);
void MyDisposeBlock(void *dataBlock);


void StrCopySafe(char *dst, const char *src, long destLen);
void CopyPascalString (StringPtr to, const StringPtr from);
void CopyCToPascalString(const char *src, StringPtr dest, long maxLen);
void CopyPascalToCString(const StringPtr src, char *dest, long maxLen);

OSErr GetShortVersionString(short rID, StringPtr version);

#ifdef __cplusplus
}
#endif

#endif /* nsMacUtils_h_ */
