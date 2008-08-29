//
//  CFURLDefines.h
//  Growl
//
//  Created by Ingmar Stein on Fri Sep 16 2005.
//  Copyright 2005-2006 The Growl Project. All rights reserved.
//
// This file is under the BSD License, refer to License.txt for details

#ifndef HAVE_CFGROWLDEFINES_H
#define HAVE_CFGROWLDEFINES_H

#ifdef __OBJC__
#	define DATA_TYPE				NSData *
#	define DATE_TYPE				NSDate *
#	define DICTIONARY_TYPE			NSDictionary *
#	define MUTABLE_DICTIONARY_TYPE	NSMutableDictionary *
#	define STRING_TYPE				NSString *
#	define ARRAY_TYPE				NSArray *
#	define URL_TYPE					NSURL *
#	define PLIST_TYPE				NSObject *
#	define OBJECT_TYPE				id
#	define BOOL_TYPE				BOOL
#else
#	include <CoreFoundation/CoreFoundation.h>
#	define DATA_TYPE				CFDataRef
#	define DATE_TYPE				CFDateRef
#	define DICTIONARY_TYPE			CFDictionaryRef
#	define MUTABLE_DICTIONARY_TYPE	CFMutableDictionaryRef
#	define STRING_TYPE				CFStringRef
#	define ARRAY_TYPE				CFArrayRef
#	define URL_TYPE					CFURLRef
#	define PLIST_TYPE				CFPropertyListRef
#	define OBJECT_TYPE				CFTypeRef
#	define BOOL_TYPE				Boolean
#endif

#endif
