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

#ifndef nsIToolbarDataModel_h___
#define nsIToolbarDataModel_h___

#include "prthread.h"
#include "nsString.h"

class nsIToolbarItem;

// {2AAB8D81-7956-11d2-BF86-00105A1B0627}
#define NS_ITOOLBARDATAMODEL_IID \
{ 0x2aab8d81, 0x7956, 0x11d2, { 0xbf, 0x86, 0x0, 0x10, 0x5a, 0x1b, 0x6, 0x27 } }

class nsIToolbarDataModel : public nsISupports
{

public:
	// There will be methods for toolbar specific stuff here.

};

#endif /* nsIToolbarDataModel_h___ */

