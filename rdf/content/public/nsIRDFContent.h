/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

/*

  An interface for RDF content model elements.

 */

#ifndef nsIRDFContent_h___
#define nsIRDFContent_h___

#include "nsISupports.h"
#include "nsIXMLContent.h"

class nsIRDFResource;

// {954F0810-81DC-11d2-B52A-000000000000}
#define NS_IRDFCONTENT_IID \
{ 0x954f0810, 0x81dc, 0x11d2, { 0xb5, 0x2a, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } }

/**
 * RDF content extensions to nsIContent
 */
class nsIRDFContent : public nsIContent {
public:
    static const nsIID& IID() { static nsIID iid = NS_IRDFCONTENT_IID; return iid; }

    /**
     * Return the RDF resource on which this RDF content element was based.
     */
    NS_IMETHOD GetResource(nsIRDFResource*& aResource) const = 0;

    /**
     * Mark a an element as a "container element" so that its contents
     * will be generated on-demand.
     */
    NS_IMETHOD SetContainer(PRBool aIsContainer) = 0;
};

nsresult
NS_NewRDFResourceElement(nsIRDFContent** aResult,
                         nsIRDFResource* aResource,
                         PRInt32 aNameSpaceID,
                         nsIAtom* aTag);

nsresult
NS_NewRDFGenericElement(nsIContent** aResult,
                        PRInt32 aNameSpaceID,
                        nsIAtom* aTag);


#endif // nsIRDFContent_h___
