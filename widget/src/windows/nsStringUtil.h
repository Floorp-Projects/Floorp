/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

// Convience macros for converting nsString's to chars +
// creating temporary char[] bufs.

#ifndef NS_STR_UTIL_H
#define NS_STR_UTIL_H
#include "nsString.h"

// nsString to temporary char[] macro

// Convience MACROS to convert an nsString to a char * which use a 
// static char array if possible to reduce memory fragmentation, 
// otherwise they allocate a char[] which must be freed.
// REMEMBER to always use the NS_FREE_STR_BUF after using the 
// NS_ALLOC_STR_BUF. You can not nest NS_ALLOC_STR_BUF's. 

#define NS_ALLOC_CHAR_BUF(aBuf, aSize, aActualSize) \
  int _ns_tmpActualSize = aActualSize;              \
  char _ns_smallBuffer[aSize];                      \
  char * const aBuf = _ns_tmpActualSize <= aSize ? _ns_smallBuffer : new char[_ns_tmpActualSize]; 

#define NS_FREE_CHAR_BUF(aBuf)   \
  if (aBuf != _ns_smallBuffer) \
   delete[] aBuf;

#define NS_ALLOC_STR_BUF(aBuf, aStrName, aTempSize) \
  NS_ALLOC_CHAR_BUF(aBuf, aTempSize, aStrName.Length()+1); \
  aStrName.ToCString(aBuf, aStrName.Length()+1);

#define NS_FREE_STR_BUF(aBuf)  \
  NS_FREE_CHAR_BUF(aBuf)

char* GetACPString(nsString&);

#endif  // NSStringUtil
