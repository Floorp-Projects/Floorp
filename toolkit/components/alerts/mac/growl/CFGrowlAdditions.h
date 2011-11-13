//
//  CFGrowlAdditions.h
//  Growl
//
//  Created by Peter Hosey on Wed Jun 18 2004.
//  Copyright 2005-2006 The Growl Project.
//
// This file is under the BSD License, refer to License.txt for details

#ifndef HAVE_CFGROWLADDITIONS_H
#define HAVE_CFGROWLADDITIONS_H

#include "CFGrowlDefines.h"

//see GrowlApplicationBridge-Carbon.c for rationale of using NSLog.
extern void NSLog(STRING_TYPE format, ...);

char *createFileSystemRepresentationOfString(STRING_TYPE str);
STRING_TYPE createStringWithDate(DATE_TYPE date);

STRING_TYPE createStringWithContentsOfFile(STRING_TYPE filename, CFStringEncoding encoding);

//you can leave out any of these three components. to leave out the character, pass 0xffff.
STRING_TYPE createStringWithStringAndCharacterAndString(STRING_TYPE str0, UniChar ch, STRING_TYPE str1);

char *copyCString(STRING_TYPE str, CFStringEncoding encoding);

STRING_TYPE copyCurrentProcessName(void);
URL_TYPE    copyCurrentProcessURL(void);
STRING_TYPE copyCurrentProcessPath(void);

URL_TYPE    copyTemporaryFolderURL(void);
STRING_TYPE copyTemporaryFolderPath(void);

STRING_TYPE createStringWithAddressData(DATA_TYPE aAddressData);
STRING_TYPE createHostNameForAddressData(DATA_TYPE aAddressData);

DATA_TYPE readFile(const char *filename);
URL_TYPE  copyURLForApplication(STRING_TYPE appName);

/*	@function	copyIconDataForPath
 *	@param	path	The POSIX path to the file or folder whose icon you want.
 *	@result	The icon data, in IconFamily format (same as used in the 'icns' resource and in .icns files). You are responsible for releasing this object.
 */
DATA_TYPE copyIconDataForPath(STRING_TYPE path);
/*	@function	copyIconDataForURL
 *	@param	URL	The URL to the file or folder whose icon you want.
 *	@result	The icon data, in IconFamily format (same as used in the 'icns' resource and in .icns files). You are responsible for releasing this object.
 */
DATA_TYPE copyIconDataForURL(URL_TYPE URL);

/*	@function	createURLByMakingDirectoryAtURLWithName
 *	@abstract	Create a directory.
 *	@discussion	This function has a useful side effect: if you pass
 *	 <code>NULL</code> for both parameters, this function will act basically as
 *	 CFURL version of <code>getcwd</code>(3).
 *
 *	 Also, for CF clients: the allocator used to create the returned URL will
 *	 be the allocator for the parent URL, the allocator for the name string, or
 *	 the default allocator, in that order of preference.
 *	@param	parent	The directory in which to create the new directory. If this is <code>NULL</code>, the current working directory (as returned by <code>getcwd</code>(3)) will be used.
 *	@param	name	The name of the directory you want to create. If this is <code>NULL</code>, the directory specified by the URL will be created.
 *	@result	The URL for the directory if it was successfully created (in which case, you are responsible for releasing this object); else, <code>NULL</code>.
 */
URL_TYPE createURLByMakingDirectoryAtURLWithName(URL_TYPE parent, STRING_TYPE name);

/*	@function	createURLByCopyingFileFromURLToDirectoryURL
 *	@param	file	The file to copy.
 *	@param	dest	The folder to copy it to.
 *	@result	The copy. You are responsible for releasing this object.
 */
URL_TYPE createURLByCopyingFileFromURLToDirectoryURL(URL_TYPE file, URL_TYPE dest);

/*	@function	createPropertyListFromURL
 *	@abstract	Reads a property list from the contents of an URL.
 *	@discussion	Creates a property list from the data at an URL (for example, a
 *	 file URL), and returns it.
 *	@param	file	The file to read.
 *	@param	mutability	A mutability-option constant indicating whether the property list (and possibly its contents) should be mutable.
 *	@param	outFormat	If the property list is read successfully, this will point to the format of the property list. You may pass NULL if you are not interested in this information. If the property list is not read successfully, the value at this pointer will be left unchanged.
 *	@param	outErrorString	If an error occurs, this will point to a string (which you are responsible for releasing) describing the error. You may pass NULL if you are not interested in this information. If no error occurs, the value at this pointer will be left unchanged.
 *	@result	The property list. You are responsible for releasing this object.
 */
PLIST_TYPE createPropertyListFromURL(URL_TYPE file, u_int32_t mutability, CFPropertyListFormat *outFormat, STRING_TYPE *outErrorString);

#endif
