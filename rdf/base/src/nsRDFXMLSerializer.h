/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Waterson <waterson@netscape.com>
 *   Axel Hecht <axel@pike.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
