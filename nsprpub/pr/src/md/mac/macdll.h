/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
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
