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

#ifndef NS_STR_UTIL_H
#define NS_STR_UTIL_H

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


#endif  // NSStringUtil
