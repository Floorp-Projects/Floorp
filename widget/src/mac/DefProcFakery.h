/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *  Mike Pinkerton (pinkerton@netscape.com)
 */

#ifndef DefProcFakery_h__
#define DefProcFakery_h__

#include <Resources.h>

//
// DefProcFakery
//
// There are times where we want to replace the standard defProc for a
// menu or a window, but don't want to go through the hassle of creating
// a stand-alone MDEF/WDEF/etc. This routine will build up a Handle that
// can replace a standard sytem defProc handle and calls into the code
// specified by |inRoutineAddr|. 
//
namespace DefProcFakery
{
    // Create a handle that looks like a code resource and stash |inRoutineAddr| into it
    // so we can divert the defProc into our code.
  Boolean CreateDefProc( RoutineDescriptor* inRoutineAddr, Handle inSystemDefProc, Handle* outDefProcHandle ) ;

    // Retrieve the system defproc stashed away in the create call
  Handle GetSystemDefProc ( Handle inFakedDefProc ) ;

    // Delete the def proc and the routine descriptor associated with it
  void DestroyDefProc ( Handle inFakedDefProc ) ;
  
}


#endif


