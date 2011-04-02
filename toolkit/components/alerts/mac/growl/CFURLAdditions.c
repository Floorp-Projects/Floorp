//
//  CFURLAdditions.c
//  Growl
//
//  Created by Karl Adam on Fri May 28 2004.
//  Copyright 2004-2006 The Growl Project. All rights reserved.
//
// This file is under the BSD License, refer to License.txt for details

#include "CFURLAdditions.h"
#include "CFGrowlAdditions.h"
#include "CFMutableDictionaryAdditions.h"
#include <Carbon/Carbon.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define _CFURLAliasDataKey  CFSTR("_CFURLAliasData")
#define _CFURLStringKey     CFSTR("_CFURLString")
#define _CFURLStringTypeKey CFSTR("_CFURLStringType")

//'alias' as in the Alias Manager.
URL_TYPE createFileURLWithAliasData(DATA_TYPE aliasData) {
	if (!aliasData) {
		NSLog(CFSTR("WARNING: createFileURLWithAliasData called with NULL aliasData"));
		return NULL;
	}

	CFURLRef url = NULL;

	AliasHandle alias = NULL;
	OSStatus err = PtrToHand(CFDataGetBytePtr(aliasData), (Handle *)&alias, CFDataGetLength(aliasData));
	if (err != noErr) {
		NSLog(CFSTR("in createFileURLWithAliasData: Could not allocate an alias handle from %u bytes of alias data (data follows) because PtrToHand returned %li\n%@"), CFDataGetLength(aliasData), aliasData, (long)err);
	} else {
		CFStringRef path = NULL;
		/*
		 * FSResolveAlias mounts disk images or network shares to resolve
		 * aliases, thus we resort to FSCopyAliasInfo.
		 */
		err = FSCopyAliasInfo(alias,
							  /* targetName */ NULL,
							  /* volumeName */ NULL,
							  &path,
							  /* whichInfo */ NULL,
							  /* info */ NULL);
		DisposeHandle((Handle)alias);
		if (err != noErr) {
			if (err != fnfErr) //ignore file-not-found; it's harmless
				NSLog(CFSTR("in createFileURLWithAliasData: Could not resolve alias (alias data follows) because FSResolveAlias returned %li - will try path\n%@"), (long)err, aliasData);
		} else if (path) {
			url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, path, kCFURLPOSIXPathStyle, true);
		} else {
			NSLog(CFSTR("in createFileURLWithAliasData: FSCopyAliasInfo returned a NULL path"));
		}
		CFRelease(path);
	}

	return url;
}

DATA_TYPE createAliasDataWithURL(URL_TYPE theURL) {
	//return NULL for non-file: URLs.
	CFStringRef scheme = CFURLCopyScheme(theURL);
	CFComparisonResult isFileURL = CFStringCompare(scheme, CFSTR("file"), kCFCompareCaseInsensitive);
	CFRelease(scheme);
	if (isFileURL != kCFCompareEqualTo)
		return NULL;

	CFDataRef aliasData = NULL;

	FSRef fsref;
	if (CFURLGetFSRef(theURL, &fsref)) {
		AliasHandle alias = NULL;
		OSStatus    err   = FSNewAlias(/*fromFile*/ NULL, &fsref, &alias);
		if (err != noErr) {
			NSLog(CFSTR("in createAliasDataForURL: FSNewAlias for %@ returned %li"), theURL, (long)err);
		} else {
			HLock((Handle)alias);

			aliasData = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)*alias, GetHandleSize((Handle)alias));

			HUnlock((Handle)alias);
			DisposeHandle((Handle)alias);
		}
	}

	return aliasData;
}

//these are the type of external representations used by Dock.app.
URL_TYPE createFileURLWithDockDescription(DICTIONARY_TYPE dict) {
	CFURLRef url = NULL;

	CFStringRef path      = CFDictionaryGetValue(dict, _CFURLStringKey);
	CFDataRef   aliasData = CFDictionaryGetValue(dict, _CFURLAliasDataKey);

	if (aliasData)
		url = createFileURLWithAliasData(aliasData);

	if (!url) {
		if (path) {
			CFNumberRef pathStyleNum = CFDictionaryGetValue(dict, _CFURLStringTypeKey);
			CFURLPathStyle pathStyle = kCFURLPOSIXPathStyle;
			
			if (pathStyleNum)
				CFNumberGetValue(pathStyleNum, kCFNumberIntType, &pathStyle);

			char *filename = createFileSystemRepresentationOfString(path);
			int fd = open(filename, O_RDONLY, 0);
			free(filename);
			if (fd != -1) {
				struct stat sb;
				fstat(fd, &sb);
				close(fd);
				url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, path, pathStyle, /*isDirectory*/ (sb.st_mode & S_IFDIR));
			}
		}
	}

	return url;
}

DICTIONARY_TYPE createDockDescriptionWithURL(URL_TYPE theURL) {
	CFMutableDictionaryRef dict;

	if (!theURL) {
		NSLog(CFSTR("%@"), CFSTR("in createDockDescriptionWithURL: Cannot copy Dock description for a NULL URL"));
		return NULL;
	}

	CFStringRef path     = CFURLCopyFileSystemPath(theURL, kCFURLPOSIXPathStyle);
	CFDataRef aliasData  = createAliasDataWithURL(theURL);

	if (path || aliasData) {
		dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 3, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

		if (path) {
			CFDictionarySetValue(dict, _CFURLStringKey, path);
			CFRelease(path);
			setIntegerForKey(dict, _CFURLStringTypeKey, kCFURLPOSIXPathStyle);
		}

		if (aliasData) {
			CFDictionarySetValue(dict, _CFURLAliasDataKey, aliasData);
			CFRelease(aliasData);
		}
	} else {
		dict = NULL;
	}

	return dict;
}
