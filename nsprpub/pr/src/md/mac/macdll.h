/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape Portable Runtime (NSPR).
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#ifndef macdll_h__
#define macdll_h__

#include "prtypes.h"

OSErr GetNamedFragmentOffsets(const FSSpec *fileSpec, const char* fragmentName,
							UInt32 *outOffset, UInt32 *outLength);
OSErr GetIndexedFragmentOffsets(const FSSpec *fileSpec, UInt32 fragmentIndex,
							UInt32 *outOffset, UInt32 *outLength, char **outFragmentName);
							
OSErr NSLoadNamedFragment(const FSSpec *fileSpec, const char* fragmentName, CFragConnectionID *outConnectionID);
OSErr NSLoadIndexedFragment(const FSSpec *fileSpec, PRUint32 fragmentIndex,
							char** outFragName, CFragConnectionID *outConnectionID);


OSErr NSGetSharedLibrary(Str255 inLibName, CFragConnectionID* outID, Ptr* outMainAddr);
OSErr NSFindSymbol(CFragConnectionID inID, Str255 inSymName,
							Ptr*	outMainAddr, CFragSymbolClass *outSymClass);

#endif /* macdll_h__ */
