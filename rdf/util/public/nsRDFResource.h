/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsRDFResource_h__
#define nsRDFResource_h__

#include "nsIRDFNode.h"
#include "nscore.h"
#include "rdf.h"
class nsIRDFService;

/**
 * This simple base class implements nsIRDFResource, and can be used as a
 * superclass for more sophisticated resource implementations.
 */
class NS_RDF nsRDFResource : public nsIRDFResource {
public:

    NS_DECL_ISUPPORTS

    // nsIRDFNode methods:
    NS_IMETHOD EqualsNode(nsIRDFNode* node, PRBool* result);

    // nsIRDFResource methods:
    NS_IMETHOD Init(const char* uri);
    NS_IMETHOD GetValue(char* *uri);
    NS_IMETHOD EqualsResource(nsIRDFResource* resource, PRBool* result);
    NS_IMETHOD EqualsString(const char* uri, PRBool* result);

    // nsRDFResource methods:
    nsRDFResource(void);
    virtual ~nsRDFResource(void);

protected:
    char* mURI;
};

#endif // nsRDFResource_h__
