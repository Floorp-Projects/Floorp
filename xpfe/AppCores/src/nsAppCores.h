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
 */
#ifndef nsAppCores_h___
#define nsAppCores_h___

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nscore.h"
#include "prio.h"

////////////////////////////////////////////////////////////////////////////////
// DLL Entry Points:
////////////////////////////////////////////////////////////////////////////////

extern "C" void
IncInstanceCount();

extern "C" void
IncLockCount();

extern "C" void
DecInstanceCount();

extern "C" void
DecLockCount();












#endif 
