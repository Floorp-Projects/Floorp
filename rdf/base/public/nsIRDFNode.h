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

/*

  RDF node interfaces, including nsIRDFNode, nsIRDFResource, and
  nsIRDFLiteral. Nodes are the elements that appear in RDF graphs.

 */

#ifndef nsIRDFNode_h__
#define nsIRDFNode_h__

#if 1 //defined(USE_XPIDL_INTERFACES)
#include "nsRDFInterfaces.h"
#else

#include "nscore.h"
#include "rdf.h"
#include "nsISupports.h"
#include "prtime.h"
// {0F78DA50-8321-11d2-8EAC-00805F29F370}
#define NS_IRDFNODE_IID \
{ 0xf78da50, 0x8321, 0x11d2, { 0x8e, 0xac, 0x0, 0x80, 0x5f, 0x29, 0xf3, 0x70 } }

/**
 *
 * Nodes are created using an instance of nsIRDFService, which
 * should be obtained from the service manager:
 *
 * nsIRDFService* service;
 * if (NS_SUCCEEDED(nsServiceManager::GetService(kCRDFServiceCID,
 *                                               kIRDFServiceIID,
 *                                               (nsISupports**) &service))) {
 *      nsIRDFNode* node;
 *      if (NS_SUCCEEDED(service->GetResource("http://foo.bar/", node))) {
 *          // do something useful here...
 *          NS_IF_RELEASE(node);
 *      }
 *      nsServiceManager::ReleaseService(kCRDFServiceCID, mgr);
 *  }
 *
 */

class NS_RDF nsIRDFNode : public nsISupports {
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IRDFNODE_IID; return iid; }

    /**
     * Determine if two nodes are identical
     */
    NS_IMETHOD EqualsNode(nsIRDFNode* that, PRBool* result) const = 0;
};


// {E0C493D2-9542-11d2-8EB8-00805F29F370}
#define NS_IRDFLITERAL_IID \
{ 0xe0c493d2, 0x9542, 0x11d2, { 0x8e, 0xb8, 0x0, 0x80, 0x5f, 0x29, 0xf3, 0x70 } }

class NS_RDF nsIRDFLiteral : public nsIRDFNode {
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IRDFLITERAL_IID; return iid; }

    /**
     * Get the Unicode string value of the node.
     */
    NS_IMETHOD GetValue(const PRUnichar* *value) const = 0;

    /**
     * Determine if two resources are identical.
     */
    NS_IMETHOD EqualsLiteral(const nsIRDFLiteral* literal, PRBool* result) const = 0;
};

// {E13A24E1-C77A-11d2-80BE-006097B76B8E}
#define NS_IRDFDATE_IID \
{ 0xe13a24e1, 0xc77a, 0x11d2, { 0x80, 0xbe, 0x0, 0x60, 0x97, 0xb7, 0x6b, 0x8e } };


class nsIRDFDate : public nsIRDFNode {
public:
    static const nsIID& IID() { static nsIID iid = NS_IRDFDATE_IID; return iid; }

    /**
     * Get the PRTime value of the node.
     */
    NS_IMETHOD GetValue(PRTime *value) const = 0;

    /**
     * Determine if two ints are identical.
     */
    NS_IMETHOD EqualsDate(const nsIRDFDate* literal, PRBool* result) const = 0;
};

// {E13A24E3-C77A-11d2-80BE-006097B76B8E}
#define NS_IRDFINT_IID \
{ 0xe13a24e3, 0xc77a, 0x11d2, { 0x80, 0xbe, 0x0, 0x60, 0x97, 0xb7, 0x6b, 0x8e } };

class nsIRDFInt : public nsIRDFNode {
public:
    static const nsIID& IID() { static nsIID iid = NS_IRDFINT_IID; return iid; }

    /**
     * Get the int32 value of the node.
     */
    NS_IMETHOD GetValue(int32 *value) const = 0;

    /**
     * Determine if two ints are identical.
     */
    NS_IMETHOD EqualsInt(const nsIRDFInt* literal, PRBool* result) const = 0;
};


// XXX this is here because a lot of files think they can just include nsIRDFNode
#include "nsIRDFResource.h"

#endif

#endif // nsIRDFNode_h__
