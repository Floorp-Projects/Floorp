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

  A package of routines shared by the RDF content code.

 */


#include "nsCOMPtr.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIRDFNode.h"
#include "nsINameSpace.h"
#include "nsINameSpaceManager.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsITextContent.h"
#include "nsIURL.h"
#include "nsIXMLContent.h"
#include "nsLayoutCID.h"
#include "nsRDFCID.h"
#include "nsRDFContentUtils.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "prlog.h"
#include "rdf.h"
#include "rdfutil.h"

static NS_DEFINE_IID(kIContentIID,     NS_ICONTENT_IID);
static NS_DEFINE_IID(kIRDFResourceIID, NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kIRDFLiteralIID,  NS_IRDFLITERAL_IID);
static NS_DEFINE_IID(kITextContentIID, NS_ITEXT_CONTENT_IID); // XXX grr...
static NS_DEFINE_CID(kTextNodeCID,     NS_TEXTNODE_CID);
static NS_DEFINE_CID(kRDFServiceCID,   NS_RDFSERVICE_CID);



nsresult
nsRDFContentUtils::AttachTextNode(nsIContent* parent, nsIRDFNode* value)
{
    nsresult rv;

    nsAutoString s;
    rv = GetTextForNode(value, s);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsITextContent> text;
    rv = nsComponentManager::CreateInstance(kTextNodeCID,
                                            nsnull,
                                            nsITextContent::GetIID(),
                                            getter_AddRefs(text));
    if (NS_FAILED(rv)) return rv;


    rv = text->SetText(s.GetUnicode(), s.Length(), PR_FALSE);
    if (NS_FAILED(rv)) return rv;

    // hook it up to the child
    rv = parent->AppendChildTo(nsCOMPtr<nsIContent>( do_QueryInterface(text) ), PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


nsresult
nsRDFContentUtils::FindChildByTag(nsIContent* aElement,
                                  PRInt32 aNameSpaceID,
                                  nsIAtom* aTag,
                                  nsIContent** aResult)
{
    nsresult rv;

    PRInt32 count;
    if (NS_FAILED(rv = aElement->ChildCount(count)))
        return rv;

    for (PRInt32 i = 0; i < count; ++i) {
        nsCOMPtr<nsIContent> kid;
        if (NS_FAILED(rv = aElement->ChildAt(i, *getter_AddRefs(kid))))
            return rv; // XXX fatal

        PRInt32 nameSpaceID;
        if (NS_FAILED(rv = kid->GetNameSpaceID(nameSpaceID)))
            return rv; // XXX fatal

        if (nameSpaceID != aNameSpaceID)
            continue; // wrong namespace

        nsCOMPtr<nsIAtom> kidTag;
        if (NS_FAILED(rv = kid->GetTag(*getter_AddRefs(kidTag))))
            return rv; // XXX fatal

        if (kidTag.get() != aTag)
            continue;

        *aResult = kid;
        NS_ADDREF(*aResult);
        return NS_OK;
    }

    return NS_RDF_NO_VALUE; // not found
}


nsresult
nsRDFContentUtils::FindChildByTagAndResource(nsIContent* aElement,
                                             PRInt32 aNameSpaceID,
                                             nsIAtom* aTag,
                                             nsIRDFResource* aResource,
                                             nsIContent** aResult)
{
    nsresult rv;

    PRInt32 count;
    if (NS_FAILED(rv = aElement->ChildCount(count)))
        return rv;

    for (PRInt32 i = 0; i < count; ++i) {
        nsCOMPtr<nsIContent> kid;
        if (NS_FAILED(rv = aElement->ChildAt(i, *getter_AddRefs(kid))))
            return rv; // XXX fatal

        // Make sure it's a <xul:treecell>
        PRInt32 nameSpaceID;
        if (NS_FAILED(rv = kid->GetNameSpaceID(nameSpaceID)))
            return rv; // XXX fatal

        if (nameSpaceID != aNameSpaceID)
            continue; // wrong namespace

        nsCOMPtr<nsIAtom> tag;
        if (NS_FAILED(rv = kid->GetTag(*getter_AddRefs(tag))))
            return rv; // XXX fatal

        if (tag.get() != aTag)
            continue; // wrong tag

        // Now get the resource ID from the RDF:ID attribute. We do it
        // via the content model, because you're never sure who
        // might've added this stuff in...
        nsCOMPtr<nsIRDFResource> resource;
        rv = GetElementResource(kid, getter_AddRefs(resource));
        NS_ASSERTION(NS_SUCCEEDED(rv), "severe error retrieving resource");
        if (NS_FAILED(rv)) return rv;

        if (resource.get() != aResource)
            continue; // not the resource we want

        // Fount it!
        *aResult = kid;
        NS_ADDREF(*aResult);
        return NS_OK;
    }

    return NS_RDF_NO_VALUE; // not found
}


nsresult
nsRDFContentUtils::GetElementResource(nsIContent* aElement, nsIRDFResource** aResult)
{
    // Perform a reverse mapping from an element in the content model
    // to an RDF resource.
    nsresult rv;
    nsAutoString uri;

    nsCOMPtr<nsIAtom> kIdAtom( dont_QueryInterface(NS_NewAtom("id")) );
    rv = aElement->GetAttribute(kNameSpaceID_None, kIdAtom, uri);
    NS_ASSERTION(NS_SUCCEEDED(rv), "severe error retrieving attribute");
    if (NS_FAILED(rv)) return rv;

    if (rv != NS_CONTENT_ATTR_HAS_VALUE)
        return NS_ERROR_FAILURE;

    // Since the element will store its ID attribute as a document-relative value,
    // we may need to qualify it first...
    nsCOMPtr<nsIDocument> doc;
    rv = aElement->GetDocument(*getter_AddRefs(doc));
    if (NS_FAILED(rv)) return rv;

    NS_ASSERTION(doc != nsnull, "element is not in any document");
    if (doc) {
        nsIURL* docURL = nsnull;
        doc->GetBaseURL(docURL);
        if (docURL) {
            const char* url;
            docURL->GetSpec(&url);
            rdf_PossiblyMakeAbsolute(url, uri);
            NS_RELEASE(docURL);
        }
    }

    NS_WITH_SERVICE(nsIRDFService, rdf, kRDFServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = rdf->GetUnicodeResource(uri.GetUnicode(), aResult);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create resource");
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}



nsresult
nsRDFContentUtils::GetTextForNode(nsIRDFNode* aNode, nsString& aResult)
{
    nsresult rv;
    nsIRDFResource* resource;
    nsIRDFLiteral* literal;
    
    if (NS_SUCCEEDED(rv = aNode->QueryInterface(kIRDFResourceIID, (void**) &resource))) {
        nsXPIDLCString p;
        if (NS_SUCCEEDED(rv = resource->GetValue( getter_Copies(p) ))) {
            aResult = p;
        }
        NS_RELEASE(resource);
    }
    else if (NS_SUCCEEDED(rv = aNode->QueryInterface(kIRDFLiteralIID, (void**) &literal))) {
        nsXPIDLString p;
        if (NS_SUCCEEDED(rv = literal->GetValue( getter_Copies(p) ))) {
            aResult = p;
        }
        NS_RELEASE(literal);
    }
    else {
        NS_ERROR("not a resource or a literal");
        rv = NS_ERROR_UNEXPECTED;
    }

    return rv;
}

nsresult
nsRDFContentUtils::GetElementLogString(nsIContent* aElement, nsString& aResult)
{
    nsresult rv;

    aResult = '<';

    nsCOMPtr<nsINameSpace> ns;

    PRInt32 elementNameSpaceID;
    rv = aElement->GetNameSpaceID(elementNameSpaceID);
    if (NS_FAILED(rv)) return rv;

    if (kNameSpaceID_HTML == elementNameSpaceID) {
        aResult.Append("html:");
    }
    else {
        nsCOMPtr<nsIXMLContent> xml( do_QueryInterface(aElement) );
        NS_ASSERTION(xml != nsnull, "not an XML or HTML element");
        if (! xml) return NS_ERROR_UNEXPECTED;

        rv = xml->GetContainingNameSpace(*getter_AddRefs(ns));
        if (NS_FAILED(rv)) return rv;
        
        nsCOMPtr<nsIAtom> prefix;
        rv = ns->FindNameSpacePrefix(elementNameSpaceID, *getter_AddRefs(prefix));
        if (NS_SUCCEEDED(rv) && (prefix != nsnull)) {
            nsAutoString prefixStr;
            prefix->ToString(prefixStr);
            if (prefixStr.Length()) {
                aResult.Append(prefix->GetUnicode());
                aResult.Append(':');
            }
        }
    }

    nsCOMPtr<nsIAtom> tag;
    rv = aElement->GetTag(*getter_AddRefs(tag));
    if (NS_FAILED(rv)) return rv;

    aResult.Append(tag->GetUnicode());

    PRInt32 count;
    rv = aElement->GetAttributeCount(count);
    if (NS_FAILED(rv)) return rv;

    for (PRInt32 i = 0; i < count; ++i) {
        aResult.Append(' ');

        PRInt32 nameSpaceID;
        nsCOMPtr<nsIAtom> name;
        rv = aElement->GetAttributeNameAt(i, nameSpaceID, *getter_AddRefs(name));
        if (NS_FAILED(rv)) return rv;

        nsAutoString attr;
        nsRDFContentUtils::GetAttributeLogString(aElement, nameSpaceID, name, attr);

        aResult.Append(attr);
        aResult.Append("=\"");

        nsAutoString value;
        rv = aElement->GetAttribute(nameSpaceID, name, value);
        if (NS_FAILED(rv)) return rv;

        aResult.Append(value);
        aResult.Append("\"");
    }

    aResult.Append('>');
    return NS_OK;
}


nsresult
nsRDFContentUtils::GetAttributeLogString(nsIContent* aElement, PRInt32 aNameSpaceID, nsIAtom* aTag, nsString& aResult)
{
    nsresult rv;

    PRInt32 elementNameSpaceID;
    rv = aElement->GetNameSpaceID(elementNameSpaceID);
    if (NS_FAILED(rv)) return rv;

    if ((kNameSpaceID_HTML == elementNameSpaceID) ||
        (kNameSpaceID_None == aNameSpaceID)) {
        aResult.Truncate();
    }
    else {
        // we may have a namespace prefix on the attribute
        nsCOMPtr<nsIXMLContent> xml( do_QueryInterface(aElement) );
        NS_ASSERTION(xml != nsnull, "not an XML or HTML element");
        if (! xml) return NS_ERROR_UNEXPECTED;

        nsCOMPtr<nsINameSpace> ns;
        rv = xml->GetContainingNameSpace(*getter_AddRefs(ns));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIAtom> prefix;
        rv = ns->FindNameSpacePrefix(aNameSpaceID, *getter_AddRefs(prefix));
        if (NS_SUCCEEDED(rv) && (prefix != nsnull)) {
            nsAutoString prefixStr;
            prefix->ToString(prefixStr);
            if (prefixStr.Length()) {
                aResult.Append(prefix->GetUnicode());
                aResult.Append(':');
            }
        }
    }

    aResult.Append(aTag->GetUnicode());
    return NS_OK;
}
