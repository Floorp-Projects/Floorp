/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "MPL"); you may not use this file except in
 * compliance with the MPL.  You may obtain a copy of the MPL at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the MPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the MPL
 * for the specific language governing rights and limitations under the
 * MPL.
 *
 * The Initial Developer of this code under the MPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributor(s): 
 *   Chris Waterson <waterson@netscape.com>
 */

#ifndef nsRDFXMLSerializer_h__
#define nsRDFXMLSerializer_h__

#include "nsIRDFLiteral.h"
#include "nsIRDFXMLSerializer.h"
#include "nsIRDFXMLSource.h"
#include "nsNameSpaceMap.h"
#include "nsXPIDLString.h"

class nsString;
class nsIOutputStream;
class nsIRDFContainerUtils;

/**
 * A helper class that can serialize RDF/XML from a
 * datasource. Implements both nsIRDFXMLSerializer and
 * nsIRDFXMLSource.
 */
class nsRDFXMLSerializer : public nsIRDFXMLSerializer,
                           public nsIRDFXMLSource
{
public:
    static NS_IMETHODIMP
    Create(nsISupports* aOuter, REFNSIID aIID, void** aResult);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIRDFXMLSERIALIZER
    NS_DECL_NSIRDFXMLSOURCE

protected:
    nsRDFXMLSerializer();
    virtual ~nsRDFXMLSerializer();

    // Implementation methods
    PRBool
    MakeQName(nsIRDFResource* aResource,
              nsString& aPproperty,
              nsString& aNameSpacePrefix,
              nsString& aNameSpaceURI);

    nsresult
    SerializeInlineAssertion(nsIOutputStream* aStream,
                             nsIRDFResource* aResource,
                             nsIRDFResource* aProperty,
                             nsIRDFLiteral* aValue);

    nsresult
    SerializeChildAssertion(nsIOutputStream* aStream,
                            nsIRDFResource* aResource,
                            nsIRDFResource* aProperty,
                            nsIRDFNode* aValue);

    nsresult
    SerializeProperty(nsIOutputStream* aStream,
                      nsIRDFResource* aResource,
                      nsIRDFResource* aProperty,
                      PRBool aInline,
                      PRInt32* aSkipped);

    PRBool
    IsContainerProperty(nsIRDFResource* aProperty);

    nsresult
    SerializeDescription(nsIOutputStream* aStream,
                         nsIRDFResource* aResource);

    nsresult
    SerializeMember(nsIOutputStream* aStream,
                    nsIRDFResource* aContainer,
                    nsIRDFNode* aMember);

    nsresult
    SerializeContainer(nsIOutputStream* aStream,
                       nsIRDFResource* aContainer);

    nsresult
    SerializePrologue(nsIOutputStream* aStream);

    nsresult
    SerializeEpilogue(nsIOutputStream* aStream);

    nsresult
    CollectNamespaces();

    nsresult
    EnsureNameSpaceFor(nsIRDFResource* aResource);

    PRBool
    IsA(nsIRDFDataSource* aDataSource, nsIRDFResource* aResource, nsIRDFResource* aType);

    nsCOMPtr<nsIRDFDataSource> mDataSource;
    nsNameSpaceMap mNameSpaces;
    nsXPIDLCString mBaseURLSpec;

    static PRInt32 gRefCnt;
    static nsIRDFResource* kRDF_instanceOf;
    static nsIRDFResource* kRDF_type;
    static nsIRDFResource* kRDF_nextVal;
    static nsIRDFResource* kRDF_Bag;
    static nsIRDFResource* kRDF_Seq;
    static nsIRDFResource* kRDF_Alt;
    static nsIRDFContainerUtils* gRDFC;
};

#endif // nsRDFXMLSerializer_h__
