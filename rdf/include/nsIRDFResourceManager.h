/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsIRDFResourceManager_h__
#define nsIRDFResourceManager_h__

#include "nsISupports.h"
class nsString;
class nsIRDFNode;

// {BFD05261-834C-11d2-8EAC-00805F29F370}
#define NS_IRDFRESOURCEMANAGER_IID \
{ 0xbfd05261, 0x834c, 0x11d2, { 0x8e, 0xac, 0x0, 0x80, 0x5f, 0x29, 0xf3, 0x70 } }


class nsIRDFResourceManager : public nsISupports {
public:
    NS_IMETHOD GetNode(const nsString& uri, nsIRDFNode*& resource) = 0;
};


#endif // nsIRDFResourceManager_h__
