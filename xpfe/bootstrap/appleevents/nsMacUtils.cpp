/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Simon Fraser <sfraser@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */



#include <stdlib.h>

#include "nsMacUtils.h"


OSErr MyNewHandle(long length, Handle *outHandle)
{
  Handle    tempHandle = NewHandle(length);
  if (!tempHandle) return memFullErr;
  *outHandle = tempHandle;
  return noErr;
}


void MyDisposeHandle(Handle theHandle)
{
  if (theHandle)
    DisposeHandle(theHandle);
}


void MyHLock(Handle theHandle)
{
  HLock(theHandle);
}

void MyHUnlock(Handle theHandle)
{
  HUnlock(theHandle);
}

#pragma mark -

OSErr MyNewBlockClear(long length, void* *outBlock)
{
  void* newData = calloc(length, 1);
  if (!newData) return memFullErr;
  *outBlock = newData;
  return noErr;
}

void MyDisposeBlock(void *dataBlock)
{
  if (dataBlock)
    free(dataBlock);
}

#pragma mark -

/*----------------------------------------------------------------------------
	StrCopySafe 
	
	strcpy which checks destination buffer length
			
----------------------------------------------------------------------------*/
void StrCopySafe(char *dst, const char *src, long destLen)
{
	const unsigned char	*p = (unsigned char *) src - 1;
	unsigned char 		*q = (unsigned char *) dst - 1;
	unsigned char		*end = (unsigned char *)dst + destLen;
	
	while ( q < end && (*++q = (*++p)) != '\0' ) {;}
	if (q == end)
		*q = '\0';
}

/*----------------------------------------------------------------------------
	CopyPascalString 
	
	Copy a pascal format string.
			
	Entry:	to = pointer to destination string.
			from = pointer to source string.
----------------------------------------------------------------------------*/

void CopyPascalString (StringPtr to, const StringPtr from)
{
	BlockMoveData(from, to, *from+1);
}


/*----------------------------------------------------------------------------
	CopyCToPascalString 
	
	Copy from a C string to a Pascal string, up to maxLen chars.
----------------------------------------------------------------------------*/

void CopyCToPascalString(const char *src, StringPtr dest, long maxLen)
{
	char		*s = (char *)src;
	char		*d = (char *)(&dest[1]);
	long		count = 0;
		
	while (count < maxLen && *s) {
		*d ++ = *s ++;
		count ++;
	}
	
	dest[0] = (unsigned char)(count);
}


/*----------------------------------------------------------------------------
	CopyPascalToCString 
	
	Copy from a C string to a Pascal string, up to maxLen chars.
----------------------------------------------------------------------------*/

void CopyPascalToCString(const StringPtr src, char *dest, long maxLen)
{
	short	len = Min(maxLen, src[0]);
	
	BlockMoveData(&src[1], dest, len);
	dest[len] = '\0';
}



/*----------------------------------------------------------------------------
	GetShortVersionString
	
	Extracts the version number string from the specified 'vers' resource and
	concats it onto the end of the specified string (Pascal-style). The short
	version number string is extracted.
	
	Exit:	function result = error code.
			*versionNumber = version number.
----------------------------------------------------------------------------*/

OSErr GetShortVersionString(short rID, StringPtr version)
{
	VersRecHndl		versionH;
	OSErr			error = resNotFound;
	
	versionH = (VersRecHndl)Get1Resource('vers', rID);		
	
	if (versionH)
	{
		CopyPascalString(version, (**versionH).shortVersion);
		ReleaseResource((Handle) versionH);
		error = noErr;
	}
	else
		CopyPascalString("\p<unknown>", version);

	return error;
}


