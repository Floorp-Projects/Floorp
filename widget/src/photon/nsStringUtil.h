/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

// Convience macros for converting nsString's to chars +
// creating temporary char[] bufs.
#include "nsReadableUtils.h"

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
    varName = ToNewCString(strName);                          \
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
