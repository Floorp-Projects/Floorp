//
//  CFMutableDictionaryAdditions.c
//  Growl
//
//  Created by Ingmar Stein on 29.05.05.
//  Copyright 2005-2006 The Growl Project. All rights reserved.
//
// This file is under the BSD License, refer to License.txt for details

#include "CFMutableDictionaryAdditions.h"

void setObjectForKey(CFMutableDictionaryRef dict, const void *key, CFTypeRef value) {
	CFDictionarySetValue(dict, key, value);
}

void setIntegerForKey(CFMutableDictionaryRef dict, const void *key, int value) {
	CFNumberRef num = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &value);
	CFDictionarySetValue(dict, key, num);
	CFRelease(num);
}

void setBooleanForKey(CFMutableDictionaryRef dict, const void *key, Boolean value) {
	CFDictionarySetValue(dict, key, value ? kCFBooleanTrue : kCFBooleanFalse);
}
