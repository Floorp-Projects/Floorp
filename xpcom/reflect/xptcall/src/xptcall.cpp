/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 */

/* entry point wrappers. */

#if defined(XP_MAC)
#pragma export on
#endif

#include "xptcprivate.h"

// This method is never called and is only here so the compiler
// will generate a vtbl for this class. 
// *Needed by the Irix implementation.*
NS_IMETHODIMP nsXPTCStubBase::QueryInterface(REFNSIID aIID,
                                             void** aInstancePtr)
{
   NS_ASSERTION(0,"wowa! nsXPTCStubBase::QueryInterface called");
   return NS_ERROR_FAILURE;
}
#if defined(XP_MAC)
#pragma export off
#endif

void
xptc_dummy2()
{
}
