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

#if 1 //defined(USE_XPIDL_INTERFACES)
#include "nsRDFInterfaces.h"
#else

#include "nsIRDFNode.h"
#include "prtypes.h"

struct JSContext;
struct JSObject;

/**
 * A resource node, which has unique object identity.
 */

// {E0C493D1-9542-11d2-8EB8-00805F29F370}
#define NS_IRDFRESOURCE_IID \
{ 0xe0c493d1, 0x9542, 0x11d2, { 0x8e, 0xb8, 0x0, 0x80, 0x5f, 0x29, 0xf3, 0x70 } }

class NS_RDF nsIRDFResource : public nsIRDFNode {
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IRDFRESOURCE_IID; return iid; }

    /**
     * Called by nsIRDFService after constructing a resource object to
     * initialize it's URI.
     */
    NS_IMETHOD Init(const char* uri) = 0;

    /**
     * Get the 8-bit string value of the node.
     */
    NS_IMETHOD GetValue(const char* *uri) const = 0;

    /**
     * Determine if two resources are identical.
     */
    NS_IMETHOD EqualsResource(const nsIRDFResource* resource, PRBool* result) const = 0;

    /**
     * Determine if two resources are identical.
     */
    NS_IMETHOD EqualsString(const char* uri, PRBool* result) const = 0;


    // XXX This is a kludge that's here until we get XPIDL scriptability working.
    static JSObject*
    GetJSObject(JSContext* aContext, nsIRDFResource* aPrivate) {
        NS_NOTREACHED("sorry, you script RDF resources yet");
        return nsnull;
    }
};

#endif

#endif // nsIRDFResource_h__

