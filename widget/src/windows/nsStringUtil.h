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

// nsString to temporary char[] macro

// Convience MACROS to convert an nsString to a char * which use a 
// static char array if possible to reduce memory fragmentation, 
// otherwise they allocate a char[] which must be freed.
// REMEMBER to always use the NS_FREE_STR_BUF after using the 
// NS_ALLOC_STR_BUF. You can not nest NS_ALLOC_STR_BUF's. 

#define NS_ALLOC_STR_BUF(varName, strName, tempSize)          \
  static const int _ns_kSmallBufferSize = tempSize;           \
  char* varName = 0;                                          \
  int _ns_smallBufUsed = 0;                                   \
  char _ns_smallBuffer[_ns_kSmallBufferSize];                 \
  if (strName.Length() < (_ns_kSmallBufferSize - 1)) {        \
    strName.ToCString(_ns_smallBuffer, _ns_kSmallBufferSize); \
    _ns_smallBuffer[_ns_kSmallBufferSize - 1] = '\0';         \
    _ns_smallBufUsed = 1;                                     \
    varName = _ns_smallBuffer;                                \
  }                                                           \
  else {                                                      \
    varName = strName.ToNewCString();                         \
  }

#define NS_FREE_STR_BUF(varName)                              \
 if (! _ns_smallBufUsed)                                      \
    delete[] varName;

// Create temporary char[] macro
//
// Create a temporary buffer for storing chars.
// If the actual size is > size then the buffer
// is allocated from the heap, otherwise the buffer
// is a stack variable. REMEMBER: use NS_FREE_BUF
// when finished with the buffer allocated, and do
// NOT nest INSERT_BUF'S.

#define NS_ALLOC_CHAR_BUF(aBuf, aSize, aActualSize)   \
  char *aBuf;                                         \
  int _ns_smallBufUsed = 0;                           \
  static const int _ns_kSmallBufferSize = aSize;      \
  if (aActualSize < _ns_kSmallBufferSize) {           \
    char _ns_smallBuffer[_ns_kSmallBufferSize];       \
    aBuf = _ns_smallBuffer;                           \
    _ns_smallBufUsed = 1;                             \
  }                                                   \
  else {                                              \
    aBuf = new char[aActualSize];                      \
  } 

#define NS_FREE_CHAR_BUF(aBuf) \
if (! _ns_smallBufUsed) \
    delete[] aBuf; 


#endif  // NSStringUtil
