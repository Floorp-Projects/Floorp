/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsRDFXMLSerializer_h__
#define nsRDFXMLSerializer_h__

#include "nsIRDFLiteral.h"
#include "nsIRDFXMLSerializer.h"
#include "nsIRDFXMLSource.h"
#include "nsNameSpaceMap.h"
#include "nsXPIDLString.h"

#include "nsDataHashtable.h"
#include "rdfITripleVisitor.h"

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
    static nsresult
    Create(nsISupports* aOuter, REFNSIID aIID, void** aResult);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIRDFXMLSERIALIZER
    NS_DECL_NSIRDFXMLSOURCE

protected:
    nsRDFXMLSerializer();
    virtual ~nsRDFXMLSerializer();

    // Implementation methods
    nsresult
    RegisterQName(nsIRDFResource* aResource);
    nsresult
    GetQName(nsIRDFResource* aResource, nsCString& aQName);
    already_AddRefed<nsIAtom>
    EnsureNewPrefix();

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
                      bool aInline,
                      PRInt32* aSkipped);

    bool
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

    bool
    IsA(nsIRDFDataSource* aDataSource, nsIRDFResource* aResource, nsIRDFResource* aType);

    nsCOMPtr<nsIRDFDataSource> mDataSource;
    nsNameSpaceMap mNameSpaces;
    nsXPIDLCString mBaseURLSpec;

    // hash mapping resources to utf8-encoded QNames
    nsDataHashtable<nsISupportsHashKey, nsCString> mQNames;
    friend class QNameCollector;

    PRUint32 mPrefixID;

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
