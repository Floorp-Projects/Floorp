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

#include "nsRDFXMLSerializer.h"

#include "nsIAtom.h"
#include "nsIOutputStream.h"
#include "nsIRDFService.h"
#include "nsIRDFContainerUtils.h"
#include "nsIServiceManager.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsVoidArray.h"
#include "rdf.h"
#include "rdfutil.h"

PRInt32 nsRDFXMLSerializer::gRefCnt = 0;
nsIRDFContainerUtils* nsRDFXMLSerializer::gRDFC;
nsIRDFResource* nsRDFXMLSerializer::kRDF_instanceOf;
nsIRDFResource* nsRDFXMLSerializer::kRDF_type;
nsIRDFResource* nsRDFXMLSerializer::kRDF_nextVal;
nsIRDFResource* nsRDFXMLSerializer::kRDF_Bag;
nsIRDFResource* nsRDFXMLSerializer::kRDF_Seq;
nsIRDFResource* nsRDFXMLSerializer::kRDF_Alt;

NS_IMETHODIMP
nsRDFXMLSerializer::Create(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsRDFXMLSerializer* result = new nsRDFXMLSerializer();
    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(result);

    nsresult rv;
    rv = result->QueryInterface(aIID, aResult);

    if (NS_SUCCEEDED(rv) && (gRefCnt++ == 0)) do {
        nsCOMPtr<nsIRDFService> rdf = do_GetService("@mozilla.org/rdf/rdf-service;1", &rv);
        if (NS_FAILED(rv)) break;

        rv = rdf->GetResource(RDF_NAMESPACE_URI "instanceOf", &kRDF_instanceOf);
        if (NS_FAILED(rv)) break;

        rv = rdf->GetResource(RDF_NAMESPACE_URI "type",       &kRDF_type);
        if (NS_FAILED(rv)) break;

        rv = rdf->GetResource(RDF_NAMESPACE_URI "nextVal",    &kRDF_nextVal);
        if (NS_FAILED(rv)) break;

        rv = rdf->GetResource(RDF_NAMESPACE_URI "Bag",        &kRDF_Bag);
        if (NS_FAILED(rv)) break;

        rv = rdf->GetResource(RDF_NAMESPACE_URI "Seq",        &kRDF_Seq);
        if (NS_FAILED(rv)) break;

        rv = rdf->GetResource(RDF_NAMESPACE_URI "Alt",        &kRDF_Alt);
        if (NS_FAILED(rv)) break;

        rv = nsServiceManager::GetService("@mozilla.org/rdf/container-utils;1",
                                          NS_GET_IID(nsIRDFContainerUtils),
                                          (nsISupports**) &gRDFC);
        if (NS_FAILED(rv)) break;
    } while (0);

    NS_RELEASE(result);

    return rv;
}

nsRDFXMLSerializer::nsRDFXMLSerializer()
{
    NS_INIT_REFCNT();
    MOZ_COUNT_CTOR(nsRDFXMLSerializer);
}

nsRDFXMLSerializer::~nsRDFXMLSerializer()
{
    MOZ_COUNT_DTOR(nsRDFXMLSerializer);

    if (--gRefCnt == 0) {
        NS_IF_RELEASE(kRDF_Bag);
        NS_IF_RELEASE(kRDF_Seq);
        NS_IF_RELEASE(kRDF_Alt);
        NS_IF_RELEASE(kRDF_instanceOf);
        NS_IF_RELEASE(kRDF_type);
        NS_IF_RELEASE(kRDF_nextVal);

        if (gRDFC) {
            nsServiceManager::ReleaseService("@mozilla.org/rdf/container-utils;1", gRDFC);
            gRDFC = nsnull;
        }
    }
}

NS_IMPL_ISUPPORTS2(nsRDFXMLSerializer, nsIRDFXMLSerializer, nsIRDFXMLSource)

NS_IMETHODIMP
nsRDFXMLSerializer::Init(nsIRDFDataSource* aDataSource)
{
    if (! aDataSource)
        return NS_ERROR_NULL_POINTER;

    mDataSource = aDataSource;
    mDataSource->GetURI(getter_Copies(mBaseURLSpec));

    // Add the ``RDF'' prefix, by default.
    nsCOMPtr<nsIAtom> prefix = getter_AddRefs(NS_NewAtom("RDF"));
    AddNameSpace(prefix, NS_LITERAL_STRING("http://www.w3.org/1999/02/22-rdf-syntax-ns#"));
    return NS_OK;
}

NS_IMETHODIMP
nsRDFXMLSerializer::AddNameSpace(nsIAtom* aPrefix, const nsAReadableString& aURI)
{
    mNameSpaces.Put(aURI, aPrefix);
    return NS_OK;
}

static nsresult
rdf_BlockingWrite(nsIOutputStream* stream, const char* buf, PRUint32 size)
{
    PRUint32 written = 0;
    PRUint32 remaining = size;
    while (remaining > 0) {
        nsresult rv;
        PRUint32 cb;

        if (NS_FAILED(rv = stream->Write(buf + written, remaining, &cb)))
            return rv;

        written += cb;
        remaining -= cb;
    }
    return NS_OK;
}

static nsresult
rdf_BlockingWrite(nsIOutputStream* stream, const nsAReadableString& s)
{
    NS_ConvertUCS2toUTF8 utf8(s);
    return rdf_BlockingWrite(stream, utf8.get(), utf8.Length());
}

// This converts a property resource (like
// "http://www.w3.org/TR/WD-rdf-syntax#Description") into a property
// ("Description"), a namespace prefix ("RDF"), and a namespace URI
// ("http://www.w3.org/TR/WD-rdf-syntax#").

PRBool
nsRDFXMLSerializer::MakeQName(nsIRDFResource* aResource,
                              nsString& aProperty,
                              nsString& aNameSpacePrefix,
                              nsString& aNameSpaceURI)
{
    const char* s;
    aResource->GetValueConst(&s);
    NS_ConvertUTF8toUCS2 uri(s);

    nsNameSpaceMap::const_iterator iter = mNameSpaces.GetNameSpaceOf(uri);
    if (iter != mNameSpaces.last()) {
        if (iter->mPrefix)
            iter->mPrefix->ToString(aNameSpacePrefix);
        else
            aNameSpacePrefix.Truncate();

        aNameSpaceURI = iter->mURI;
        uri.Right(aProperty, uri.Length() - aNameSpaceURI.Length());
        return PR_TRUE;
    }
    
    // Okay, so we don't have it in our map. Try to make one up. This
    // is very bogus.
    PRInt32 i = uri.RFindChar('#'); // first try a '#'
    if (i == -1) {
        i = uri.RFindChar('/');
        if (i == -1) {
            // Okay, just punt and assume there is _no_ namespace on
            // this thing...
            aNameSpaceURI.Truncate();
            aNameSpacePrefix.Truncate();
            aProperty = uri;
            return PR_TRUE;
        }
    }

    // Take whatever is to the right of the '#' and call it the
    // property.
    aProperty.Truncate();
    uri.Right(aProperty, uri.Length() - (i + 1));

    // Truncate the namespace URI down to the string up to and
    // including the '#'.
    aNameSpaceURI = uri;
    aNameSpaceURI.Truncate(i + 1);

    // Just generate a random prefix
    static PRInt32 gPrefixID = 0;
    aNameSpacePrefix = NS_LITERAL_STRING("NS");
    aNameSpacePrefix.AppendInt(++gPrefixID, 10);
    return PR_FALSE;
}


PRBool
nsRDFXMLSerializer::IsContainerProperty(nsIRDFResource* aProperty)
{
    // Return `true' if the property is an internal property related
    // to being a container.
    if (aProperty == kRDF_instanceOf)
        return PR_TRUE;

    if (aProperty == kRDF_nextVal)
        return PR_TRUE;

    PRBool isOrdinal = PR_FALSE;
    gRDFC->IsOrdinalProperty(aProperty, &isOrdinal);
    if (isOrdinal)
        return PR_TRUE;

    return PR_FALSE;
} 


// convert '&', '<', and '>' into "&amp;", "&lt;", and "&gt", respectively.
static void
rdf_EscapeAmpersandsAndAngleBrackets(nsString& s)
{
    // XXX this could be re-written using the new string classes to
    // just scan the string once. Maybe we should add a
    // ReplaceSubstrings() function that accepts a list of
    // target/replace pairs, builds a state machine, blah blah
    // blah. I'm too lazy to do that now!
    PRInt32 i;

    // Do ampersands first, so we don't double-escape.
    i = 0;
    while ((i = s.FindChar('&', PR_FALSE, i)) != -1) {
        s.SetCharAt('&', i);
        s.Insert(NS_LITERAL_STRING("amp;"), i + 1);
        i += 4;
    }

    i = 0;
    while ((i = s.FindChar('<', PR_FALSE, i)) != -1) {
        s.SetCharAt('&', i);
        s.Insert(NS_LITERAL_STRING("lt;"), i + 1);
        i += 3;
    }

    i = 0;
    while ((i = s.FindChar('>', PR_FALSE, i)) != -1) {
        s.SetCharAt('&', i);
        s.Insert(NS_LITERAL_STRING("gt;"), i + 1);
        i += 3;
    }
}

// convert '"' to "&quot;"
static void
rdf_EscapeQuotes(nsString& s)
{
    PRInt32 i = 0;
    while ((i = s.FindChar('"', PR_FALSE, i)) != -1) {
        s.SetCharAt('&', i);
        s.Insert(NS_LITERAL_STRING("quot;"), i + 1);
        i += 5;
    }
}

static void
rdf_EscapeAttributeValue(nsString& s)
{
    rdf_EscapeAmpersandsAndAngleBrackets(s);
    rdf_EscapeQuotes(s);
}


nsresult
nsRDFXMLSerializer::SerializeInlineAssertion(nsIOutputStream* aStream,
                                             nsIRDFResource* aResource,
                                             nsIRDFResource* aProperty,
                                             nsIRDFLiteral* aValue)
{
    nsAutoString property, nameSpacePrefix, nameSpaceURI;
    nsAutoString attr;

    PRBool wasDefinedAtGlobalScope =
        MakeQName(aProperty, property, nameSpacePrefix, nameSpaceURI);

    if (nameSpacePrefix.Length()) {
        attr.Append(nameSpacePrefix);
        attr.AppendWithConversion(':');
    }
    attr.Append(property);

    rdf_BlockingWrite(aStream, NS_LITERAL_STRING("\n                   "));

    if (!wasDefinedAtGlobalScope && nameSpacePrefix.Length()) {
        rdf_BlockingWrite(aStream, NS_LITERAL_STRING("xmlns:"));
        rdf_BlockingWrite(aStream, nameSpacePrefix);
        rdf_BlockingWrite(aStream, NS_LITERAL_STRING("=\""));
        rdf_BlockingWrite(aStream, nameSpaceURI);
        rdf_BlockingWrite(aStream, NS_LITERAL_STRING("\" "));
    }

    const PRUnichar* value;
    aValue->GetValueConst(&value);
    nsAutoString s(value);

    rdf_EscapeAttributeValue(s);

    rdf_BlockingWrite(aStream, attr);
    rdf_BlockingWrite(aStream, NS_LITERAL_STRING("=\""));
    rdf_BlockingWrite(aStream, s);
    rdf_BlockingWrite(aStream, NS_LITERAL_STRING("\""));

    return NS_OK;
}

nsresult
nsRDFXMLSerializer::SerializeChildAssertion(nsIOutputStream* aStream,
                                            nsIRDFResource* aResource,
                                            nsIRDFResource* aProperty,
                                            nsIRDFNode* aValue)
{
    nsAutoString property, nameSpacePrefix, nameSpaceURI;
    nsAutoString tag;

    PRBool wasDefinedAtGlobalScope =
        MakeQName(aProperty, property, nameSpacePrefix, nameSpaceURI);

    if (nameSpacePrefix.Length()) {
        tag.Append(nameSpacePrefix);
        tag.AppendWithConversion(':');
    }
    tag.Append(property);

    rdf_BlockingWrite(aStream, "    <", 5);
    rdf_BlockingWrite(aStream, tag);

    if (!wasDefinedAtGlobalScope && nameSpacePrefix.Length()) {
        rdf_BlockingWrite(aStream, " xmlns:", 7);
        rdf_BlockingWrite(aStream, nameSpacePrefix);
        rdf_BlockingWrite(aStream, "=\"", 2);
        rdf_BlockingWrite(aStream, nameSpaceURI);
        rdf_BlockingWrite(aStream, "\"", 1);
    }

    nsCOMPtr<nsIRDFResource> resource;
    nsCOMPtr<nsIRDFLiteral> literal;

    if ((resource = do_QueryInterface(aValue)) != nsnull) {
        const char *s;
        resource->GetValueConst(&s);

        nsAutoString uri = NS_ConvertUTF8toUCS2(s);
        rdf_MakeRelativeRef(NS_ConvertUTF8toUCS2(mBaseURLSpec.get()), uri);
        rdf_EscapeAttributeValue(uri);

static const char kRDFResource1[] = " resource=\"";
static const char kRDFResource2[] = "\"/>\n";
        rdf_BlockingWrite(aStream, kRDFResource1, sizeof(kRDFResource1) - 1);
        rdf_BlockingWrite(aStream, uri);
        rdf_BlockingWrite(aStream, kRDFResource2, sizeof(kRDFResource2) - 1);
    }
    else if ((literal = do_QueryInterface(aValue)) != nsnull) {
        const PRUnichar *value;
        literal->GetValueConst(&value);
        nsAutoString s(value);

        rdf_EscapeAmpersandsAndAngleBrackets(s);

        rdf_BlockingWrite(aStream, ">", 1);
        rdf_BlockingWrite(aStream, s);
        rdf_BlockingWrite(aStream, "</", 2);
        rdf_BlockingWrite(aStream, tag);
        rdf_BlockingWrite(aStream, ">\n", 2);
    }
    else {
        // XXX it doesn't support nsIRDFResource _or_ nsIRDFLiteral???
        // We should serialize nsIRDFInt, nsIRDFDate, etc...
        NS_ERROR("not an nsIRDFResource or nsIRDFLiteral?");
    }

    return NS_OK;
}

nsresult
nsRDFXMLSerializer::SerializeProperty(nsIOutputStream* aStream,
                                      nsIRDFResource* aResource,
                                      nsIRDFResource* aProperty,
                                      PRBool aInline,
                                      PRInt32* aSkipped)
{
    nsresult rv = NS_OK;

    PRInt32 skipped = 0;

    nsCOMPtr<nsISimpleEnumerator> assertions;
    mDataSource->GetTargets(aResource, aProperty, PR_TRUE, getter_AddRefs(assertions));
    if (! assertions)
        return NS_ERROR_FAILURE;

    PRBool multi = PR_FALSE;

    while (1) {
        PRBool hasMore = PR_FALSE;
        assertions->HasMoreElements(&hasMore);
        if (! hasMore)
            break;

        nsCOMPtr<nsISupports> isupports;
        assertions->GetNext(getter_AddRefs(isupports));

        if (! multi) {
            assertions->HasMoreElements(&hasMore);
            if (hasMore)
                multi = PR_TRUE;
        }

        nsCOMPtr<nsIRDFLiteral> literal = do_QueryInterface(isupports);
        if (aInline && (literal && !multi)) {
            rv = SerializeInlineAssertion(aStream, aResource, aProperty, literal);
        }
        else if (!aInline && (!literal || multi)) {
            nsCOMPtr<nsIRDFNode> value = do_QueryInterface(isupports);
            rv = SerializeChildAssertion(aStream, aResource, aProperty, value);
        }
        else {
            ++skipped;
            rv = NS_OK;
        }

        if (NS_FAILED(rv))
            break;
    }

    *aSkipped += skipped;
    return rv;
}


nsresult
nsRDFXMLSerializer::SerializeDescription(nsIOutputStream* aStream,
                                         nsIRDFResource* aResource)
{
static const char kRDFDescriptionOpen[]      = "  <RDF:Description";
static const char kIDAttr[]                  = " ID=\"";
static const char kAboutAttr[]               = " about=\"";
static const char kRDFDescriptionClose[]     = "  </RDF:Description>\n";

    nsresult rv;

    PRBool isTypedNode = PR_FALSE;
    nsAutoString nodeName, nameSpacePrefix, nameSpaceURI;

    nsCOMPtr<nsIRDFNode> typeNode;
    mDataSource->GetTarget(aResource, kRDF_type, PR_TRUE, getter_AddRefs(typeNode));
    if (typeNode) {
        nsCOMPtr<nsIRDFResource> type = do_QueryInterface(typeNode, &rv);
        if (type) {
            // Try to get a namespace prefix.  If none is available,
            // just treat the description as if it weren't a typed node 
            // after all and emit rdf:type as a normal property.  This 
            // seems preferable to using a bogus (invented) prefix.
            isTypedNode = MakeQName(type, nodeName,
                                    nameSpacePrefix, nameSpaceURI);
        }
    }

    const char* s;
    rv = aResource->GetValueConst(&s);
    if (NS_FAILED(rv)) return rv;

    nsAutoString uri = NS_ConvertUTF8toUCS2(s);
    rdf_MakeRelativeRef(NS_ConvertUTF8toUCS2(mBaseURLSpec.get()), uri);
    rdf_EscapeAttributeValue(uri);

    // Emit an open tag and the subject
    if (isTypedNode) {
        rdf_BlockingWrite(aStream, NS_LITERAL_STRING("  <"));
        rdf_BlockingWrite(aStream, nameSpacePrefix);
        rdf_BlockingWrite(aStream, NS_LITERAL_STRING(":"));
        rdf_BlockingWrite(aStream, nodeName);
    }
    else 
        rdf_BlockingWrite(aStream, kRDFDescriptionOpen,
                          sizeof(kRDFDescriptionOpen) - 1);
    if (uri[0] == PRUnichar('#')) {
        uri.Cut(0, 1);
        rdf_BlockingWrite(aStream, kIDAttr, sizeof(kIDAttr) - 1);
    }
    else {
        rdf_BlockingWrite(aStream, kAboutAttr, sizeof(kAboutAttr) - 1);
    }

    rdf_BlockingWrite(aStream, uri);
    rdf_BlockingWrite(aStream, NS_LITERAL_STRING("\""));

    // Any value that's a literal we can write out as an inline
    // attribute on the RDF:Description
    nsAutoVoidArray visited;
    PRInt32 skipped = 0;

    nsCOMPtr<nsISimpleEnumerator> arcs;
    mDataSource->ArcLabelsOut(aResource, getter_AddRefs(arcs));

    if (arcs) {
        // Don't re-serialize rdf:type later on
        if (isTypedNode)
            visited.AppendElement(kRDF_type);

        while (1) {
            PRBool hasMore = PR_FALSE;
            arcs->HasMoreElements(&hasMore);
            if (! hasMore)
                break;

            nsCOMPtr<nsISupports> isupports;
            arcs->GetNext(getter_AddRefs(isupports));

            nsCOMPtr<nsIRDFResource> property = do_QueryInterface(isupports);
            if (! property)
                continue;

            // Ignore properties that pertain to containers; we may be
            // called from SerializeContainer() if the container resource
            // has been assigned non-container properties.
            if (IsContainerProperty(property))
                continue;

            // Only serialize values for the property once.
            if (visited.IndexOf(property.get()) >= 0)
                continue;

            visited.AppendElement(property.get());

            SerializeProperty(aStream, aResource, property, PR_TRUE, &skipped);
        }
    }

    if (skipped) {
        // Close the RDF:Description tag.
        rdf_BlockingWrite(aStream, NS_LITERAL_STRING(">\n"));

        // Now write out resources (which might have their own
        // substructure) as children.
        mDataSource->ArcLabelsOut(aResource, getter_AddRefs(arcs));

        if (arcs) {
            // Forget that we've visited anything
            visited.Clear();
            // ... except for rdf:type
            if (isTypedNode)
                visited.AppendElement(kRDF_type);

            while (1) {
                PRBool hasMore = PR_FALSE;
                arcs->HasMoreElements(&hasMore);
                if (! hasMore)
                    break;

                nsCOMPtr<nsISupports> isupports;
                arcs->GetNext(getter_AddRefs(isupports));

                nsCOMPtr<nsIRDFResource> property = do_QueryInterface(isupports);
                if (! property)
                    continue;

                // Ignore properties that pertain to containers; we may be
                // called from SerializeContainer() if the container
                // resource has been assigned non-container properties.
                if (IsContainerProperty(property))
                    continue;

                // have we already seen this property?  If so, don't write it
                // out again; serialize property will write each instance.
                if (visited.IndexOf(property.get()) >= 0)
                    continue;

                visited.AppendElement(property.get());

                SerializeProperty(aStream, aResource, property, PR_FALSE, &skipped);
            }
        }

        // Emit a proper close-tag.
        if (isTypedNode) {
            rdf_BlockingWrite(aStream,  NS_LITERAL_STRING("  </"));
            rdf_BlockingWrite(aStream, nameSpacePrefix);
            rdf_BlockingWrite(aStream, NS_LITERAL_STRING(":"));
            rdf_BlockingWrite(aStream, nodeName);
            rdf_BlockingWrite(aStream,  NS_LITERAL_STRING(">\n"));
        }
        else
            rdf_BlockingWrite(aStream, kRDFDescriptionClose,
                              sizeof(kRDFDescriptionClose) - 1);
    }
    else {
        // If we saw _no_ child properties, then we can don't need a
        // close-tag.
        rdf_BlockingWrite(aStream, NS_LITERAL_STRING(" />\n"));
    }

    return NS_OK;
}

nsresult
nsRDFXMLSerializer::SerializeMember(nsIOutputStream* aStream,
                                      nsIRDFResource* aContainer,
                                      nsIRDFNode* aMember)
{
    nsresult rv;

    // If it's a resource, then output a "<RDF:li resource=... />"
    // tag, because we'll be dumping the resource separately. (We
    // iterate thru all the resources in the datasource,
    // remember?) Otherwise, output the literal value.

    nsCOMPtr<nsIRDFResource> resource;
    nsCOMPtr<nsIRDFLiteral> literal;

    if ((resource = do_QueryInterface(aMember)) != nsnull) {
        nsXPIDLCString s;
        if (NS_SUCCEEDED(rv = resource->GetValue( getter_Copies(s) ))) {
static const char kRDFLIResource1[] = "    <RDF:li resource=\"";
static const char kRDFLIResource2[] = "\"/>\n";

            nsAutoString uri = NS_ConvertUTF8toUCS2(s);
            rdf_MakeRelativeRef(NS_ConvertUTF8toUCS2(mBaseURLSpec.get()), uri);
            rdf_EscapeAttributeValue(uri);

            rdf_BlockingWrite(aStream, kRDFLIResource1, sizeof(kRDFLIResource1) - 1);
            rdf_BlockingWrite(aStream, uri);
            rdf_BlockingWrite(aStream, kRDFLIResource2, sizeof(kRDFLIResource2) - 1);
        }
    }
    else if ((literal = do_QueryInterface(aMember)) != nsnull) {
        nsXPIDLString value;
        if (NS_SUCCEEDED(rv = literal->GetValue( getter_Copies(value) ))) {
static const char kRDFLILiteral1[] = "    <RDF:li>";
static const char kRDFLILiteral2[] = "</RDF:li>\n";

            rdf_BlockingWrite(aStream, kRDFLILiteral1, sizeof(kRDFLILiteral1) - 1);

            nsAutoString s(value);
            rdf_EscapeAmpersandsAndAngleBrackets(s);

            rdf_BlockingWrite(aStream, s);
            rdf_BlockingWrite(aStream, kRDFLILiteral2, sizeof(kRDFLILiteral2) - 1);
        }
    }
    else {
        NS_ERROR("uhh -- it's not a literal or a resource?");
    }
    return NS_OK;
}


nsresult
nsRDFXMLSerializer::SerializeContainer(nsIOutputStream* aStream,
                                         nsIRDFResource* aContainer)
{
    nsresult rv;
    nsAutoString tag;

    // Decide if it's a sequence, bag, or alternation, and print the
    // appropriate tag-open sequence

    if (IsA(mDataSource, aContainer, kRDF_Bag)) {
        tag = NS_LITERAL_STRING("RDF:Bag");
    }
    else if (IsA(mDataSource, aContainer, kRDF_Seq)) {
        tag = NS_LITERAL_STRING("RDF:Seq");
    }
    else if (IsA(mDataSource, aContainer, kRDF_Alt)) {
        tag = NS_LITERAL_STRING("RDF:Alt");
    }
    else {
        NS_ASSERTION(PR_FALSE, "huh? this is _not_ a container.");
        return NS_ERROR_UNEXPECTED;
    }

    rdf_BlockingWrite(aStream, "  <", 3);
    rdf_BlockingWrite(aStream, tag);


    // Unfortunately, we always need to print out the identity of the
    // resource, even if was constructed "anonymously". We need to do
    // this because we never really know who else might be referring
    // to it...

    const char *s;
    if (NS_SUCCEEDED(aContainer->GetValueConst(&s))) {
        nsAutoString uri = NS_ConvertUTF8toUCS2(s);
        rdf_MakeRelativeRef(NS_ConvertUTF8toUCS2(mBaseURLSpec.get()), uri);

        rdf_EscapeAttributeValue(uri);

        if (uri.First() == PRUnichar('#')) {
            // Okay, it's actually identified as an element in the
            // current document, not trying to decorate some absolute
            // URI. We can use the 'ID=' attribute...
            static const char kIDEquals[] = " ID=\"";

            uri.Cut(0, 1); // chop the '#'
            rdf_BlockingWrite(aStream, kIDEquals, sizeof(kIDEquals) - 1);
        }
        else {
            // We need to cheat and spit out an illegal 'about=' on
            // the sequence. 
            static const char kAboutEquals[] = " about=\"";
            rdf_BlockingWrite(aStream, kAboutEquals, sizeof(kAboutEquals) - 1);
        }

        rdf_BlockingWrite(aStream, uri);
        rdf_BlockingWrite(aStream, "\"", 1);
    }

    rdf_BlockingWrite(aStream, ">\n", 2);

    // First iterate through each of the ordinal elements (the RDF/XML
    // syntax doesn't allow us to place properties on RDF container
    // elements).
    nsCOMPtr<nsISimpleEnumerator> elements;
    rv = NS_NewContainerEnumerator(mDataSource, aContainer, getter_AddRefs(elements));

    if (NS_SUCCEEDED(rv)) {
        while (1) {
            PRBool hasMore;
            rv = elements->HasMoreElements(&hasMore);
            if (NS_FAILED(rv)) break;

            if (! hasMore)
                break;

            nsCOMPtr<nsISupports> isupports;
            elements->GetNext(getter_AddRefs(isupports));

            nsCOMPtr<nsIRDFNode> element = do_QueryInterface(isupports);
            NS_ASSERTION(element != nsnull, "not an nsIRDFNode");
            if (! element)
                continue;

            SerializeMember(aStream, aContainer, element);
        }
    }

    // close the container tag
    rdf_BlockingWrite(aStream, "  </", 4);
    rdf_BlockingWrite(aStream, tag);
    rdf_BlockingWrite(aStream, ">\n", 2);

    // Now, we iterate through _all_ of the arcs, in case someone has
    // applied properties to the bag itself. These'll be placed in a
    // separate RDF:Description element.
    nsCOMPtr<nsISimpleEnumerator> arcs;
    mDataSource->ArcLabelsOut(aContainer, getter_AddRefs(arcs));

    PRBool wroteDescription = PR_FALSE;
    while (! wroteDescription) {
        PRBool hasMore = PR_FALSE;
        rv = arcs->HasMoreElements(&hasMore);
        if (NS_FAILED(rv)) break;

        if (! hasMore)
            break;

        nsIRDFResource* property;
        rv = arcs->GetNext((nsISupports**) &property);
        if (NS_FAILED(rv)) break;

        // If it's a membership property, then output a "LI"
        // tag. Otherwise, output a property.
        if (! IsContainerProperty(property)) {
            rv = SerializeDescription(aStream, aContainer);
            wroteDescription = PR_TRUE;
        }

        NS_RELEASE(property);
        if (NS_FAILED(rv))
            break;
    }

    return NS_OK;
}


nsresult
nsRDFXMLSerializer::SerializePrologue(nsIOutputStream* aStream)
{
static const char kXMLVersion[] = "<?xml version=\"1.0\"?>\n";

    rdf_BlockingWrite(aStream, kXMLVersion, sizeof(kXMLVersion) - 1);

    // global name space declarations
    rdf_BlockingWrite(aStream, NS_LITERAL_STRING("<RDF:RDF "));

    nsNameSpaceMap::const_iterator first = mNameSpaces.first();
    nsNameSpaceMap::const_iterator last = mNameSpaces.last();
    for (nsNameSpaceMap::const_iterator entry = first; entry != last; ++entry) {
        if (entry != first)
            rdf_BlockingWrite(aStream, NS_LITERAL_STRING("\n         "));

        rdf_BlockingWrite(aStream, NS_LITERAL_STRING("xmlns"));

        if (entry->mPrefix) {
            rdf_BlockingWrite(aStream, NS_LITERAL_STRING(":"));
            nsAutoString prefix;
            entry->mPrefix->ToString(prefix);
            rdf_BlockingWrite(aStream, prefix);
        }

        rdf_BlockingWrite(aStream, NS_LITERAL_STRING("=\""));
        rdf_BlockingWrite(aStream, entry->mURI);
        rdf_BlockingWrite(aStream, NS_LITERAL_STRING("\""));
    }

    rdf_BlockingWrite(aStream, NS_LITERAL_STRING(">\n"));
    return NS_OK;
}


nsresult
nsRDFXMLSerializer::SerializeEpilogue(nsIOutputStream* aStream)
{
    rdf_BlockingWrite(aStream, NS_LITERAL_STRING("</RDF:RDF>\n"));
    return NS_OK;
}

nsresult
nsRDFXMLSerializer::CollectNamespaces()
{
    // Iterate through all the resources in the datasource, and all of
    // the arcs-out of each, to collect the set of namespaces in use
    nsCOMPtr<nsISimpleEnumerator> resources;
    mDataSource->GetAllResources(getter_AddRefs(resources));
    if (! resources)
        return NS_ERROR_FAILURE;

    while (1) {
        PRBool hasMore = PR_FALSE;
        resources->HasMoreElements(&hasMore);
        if (! hasMore)
            break;

        nsCOMPtr<nsISupports> isupports;
        resources->GetNext(getter_AddRefs(isupports));

        nsCOMPtr<nsIRDFResource> resource =
            do_QueryInterface(isupports);

        if (! resource)
            continue;

        nsCOMPtr<nsISimpleEnumerator> arcs;
        mDataSource->ArcLabelsOut(resource, getter_AddRefs(arcs));

        if (! arcs)
            continue;

        while (1) {
            hasMore = PR_FALSE;
            arcs->HasMoreElements(&hasMore);
            if (! hasMore)
                break;

            arcs->GetNext(getter_AddRefs(isupports));

            nsCOMPtr<nsIRDFResource> property =
                do_QueryInterface(isupports);

            if (! property)
                continue;

            EnsureNameSpaceFor(property);
        }
    }

    return NS_OK;
}

nsresult
nsRDFXMLSerializer::EnsureNameSpaceFor(nsIRDFResource* aResource)
{
    nsAutoString property;
    nsAutoString nameSpacePrefix;
    nsAutoString nameSpaceURI;

    if (! MakeQName(aResource, property, nameSpacePrefix, nameSpaceURI)) {
#ifdef DEBUG_waterson
        const char* s;
        aResource->GetValueConst(&s);
        printf("*** Creating namespace for %s\n", s);
#endif
        nsCOMPtr<nsIAtom> prefix = getter_AddRefs(NS_NewAtom(nameSpacePrefix));
        mNameSpaces.Put(nameSpaceURI, prefix);
    }

    return NS_OK;
}

//----------------------------------------------------------------------

NS_IMETHODIMP
nsRDFXMLSerializer::Serialize(nsIOutputStream* aStream)
{
    nsresult rv;

    rv = CollectNamespaces();
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsISimpleEnumerator> resources;
    rv = mDataSource->GetAllResources(getter_AddRefs(resources));
    if (NS_FAILED(rv)) return rv;

    rv = SerializePrologue(aStream);
    if (NS_FAILED(rv))
        return rv;

    while (1) {
        PRBool hasMore = PR_FALSE;
        resources->HasMoreElements(&hasMore);
        if (! hasMore)
            break;

        nsCOMPtr<nsISupports> isupports;
        resources->GetNext(getter_AddRefs(isupports));

        nsCOMPtr<nsIRDFResource> resource = do_QueryInterface(isupports);
        if (! resource)
            continue;

        if (IsA(mDataSource, resource, kRDF_Bag) ||
            IsA(mDataSource, resource, kRDF_Seq) ||
            IsA(mDataSource, resource, kRDF_Alt)) {
            rv = SerializeContainer(aStream, resource);
        }
        else {
            rv = SerializeDescription(aStream, resource);
        }

        if (NS_FAILED(rv))
            break;
    }

    return SerializeEpilogue(aStream);
}


PRBool
nsRDFXMLSerializer::IsA(nsIRDFDataSource* aDataSource, nsIRDFResource* aResource, nsIRDFResource* aType)
{
    nsresult rv;

    PRBool result;
    rv = aDataSource->HasAssertion(aResource, kRDF_instanceOf, aType, PR_TRUE, &result);
    if (NS_FAILED(rv)) return PR_FALSE;

    return result;
}
