/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Doug Turner <dougt@netscape.com>
 */

#ifndef _NSIDIRENUMERATORIMPL_H_
#define _NSIDIRENUMERATORIMPL_H_

#define NS_IDIRECTORYENUMERATOR_CID {0xe67dcbe0, 0x6a21, 0x11d3, {0x8c, 0x51, 0x00, 0x60, 0x97, 0x92, 0x27, 0x8c}}

// nsXPComInit needs to know about how we are implmented,
// so here we will export it.  Other users should not depend
// on this

#ifdef XP_PC
#include "nsIDirEnumeratorImplWin.h"
#else
#error NOT_IMPLEMENTED
#endif

#endif