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

#ifndef nsIRDFResource_h__
#define nsIRDFResource_h__

#include "rdf.h"
#include "nsISupports.h"

// {040F6610-7B35-11d2-8EA9-00805F29F370}
#define NS_IRDFRESOURCE_IID \
{ 0x40f6610, 0x7b35, 0x11d2, { 0x8e, 0xa9, 0x0, 0x80, 0x5f, 0x29, 0xf3, 0x70 } };

/**
 * This interface describes an RDF resource.
 * IT IS GOING TO CHANGE SIGNIFICANTLY ONCE WE XPCOM THE RDF BACK-END.
 * Use at your own risk. Have a nice day.
 */
class nsIRDFResource : public nsISupports {
public:
    NS_IMETHOD GetResource(RDF_Resource& result /* out */) const = 0;
};

#endif nsIRDFResource_h__
