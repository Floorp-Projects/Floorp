/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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

/* 
 * nsDiskModule
 *
 * Gagan Saksena 02/02/98
 * 
 */

#include <prtypes.h>
#include "nsDiskModule.h"
#include "nsCacheObject.h"
//
// Constructor: nsDiskModule
//
nsDiskModule::nsDiskModule()
{

}

nsDiskModule::~nsDiskModule()
{

}

nsCacheObject* nsDiskModule::GetObject(PRUint32 i_index) const
{
	return 0;
}

PRBool nsDiskModule::AddObject(nsCacheObject* i_pObject)
{
	return PR_FALSE;
}
