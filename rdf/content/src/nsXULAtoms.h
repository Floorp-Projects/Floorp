/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author(s):
 *   Chris Waterson <waterson@netscape.com>
 *
 * Contributor(s): 
 *
 */

/*

  Header file used to include the commonly-used atoms in the XUL
  code. Actual atom additions and removal should be performed on
  nsXULAtoms.inc.

 */


#ifndef nsXULAtoms_h__
#define nsXULAtoms_h__

#include "nsIAtom.h"

#ifdef NS_XULATOM
#undef NS_XULATOM
#endif

#define NS_XULATOM(__atom) static nsIAtom* __atom

class nsXULAtoms {
protected:
    static nsrefcnt gRefCnt;

public:
    static nsrefcnt AddRef();
    static nsrefcnt Release();

#include "nsXULAtoms.inc"

    static nsIAtom* Template; // XXX because |template| is a keyword
};


#endif
