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
#include "nsNeckoUtil.h"
#include "nsRDFCID.h"
#include "nsRDFContentUtils.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "prlog.h"
#include "prtime.h"
#include "rdf.h"
#include "rdfutil.h"

#include "nsILocale.h"
#include "nsLocaleCID.h"
#include "nsILocaleFactory.h"

#include "nsIDateTimeFormat.h"
#include "nsDateTimeFormatCID.h"
#include "nsIScriptableDateFormat.h"

static NS_DEFINE_IID(kIContentIID,     NS_ICONTENT_IID);
static NS_DEFINE_IID(kIRDFResourceIID, NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kIRDFLiteralIID,  NS_IRDFLITERAL_IID);
static NS_DEFINE_IID(kIRDFIntIID,      NS_IRDFINT_IID);
static NS_DEFINE_IID(kIRDFDateIID,     NS_IRDFDATE_IID);
static NS_DEFINE_IID(kITextContentIID, NS_ITEXT_CONTENT_IID); // XXX grr...
static NS_DEFINE_CID(kTextNodeCID,     NS_TEXTNODE_CID);
static NS_DEFINE_CID(kRDFServiceCID,   NS_RDFSERVICE_CID);

static NS_DEFINE_CID(kLocaleFactoryCID, NS_LOCALEFACTORY_CID);
static NS_DEFINE_IID(kILocaleFactoryIID, NS_ILOCALEFACTORY_IID);
static NS_DEFINE_CID(kLocaleCID, NS_LOCALE_CID);
static NS_DEFINE_IID(kILocaleIID, NS_ILOCALE_IID);
static NS_DEFINE_CID(kNameSpaceManagerCID, NS_NAMESPACEMANAGER_CID);

static NS_DEFINE_CID(kDateTimeFormatCID, NS_DATETIMEFORMAT_CID);
static NS_DEFINE_CID(kDateTimeFormatIID, NS_IDATETIMEFORMAT_IID);

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
    rv = parent->AppendChildTo(nsCOMPtr<nsIContent>( do_QueryInterface(text) ), PR_FALSE);
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
nsRDFContentUtils::FindChildByResource(nsIContent* aElement,
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

        // Now get the resource ID from the RDF:ID attribute. We do it
        // via the content model, because you're never sure who
        // might've added this stuff in...
        nsCOMPtr<nsIRDFResource> resource;
        rv = GetElementResource(kid, getter_AddRefs(resource));
        if (NS_FAILED(rv)) continue;

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
    nsAutoString id;

    nsCOMPtr<nsIAtom> kIdAtom( dont_AddRef(NS_NewAtom("id")) );
    rv = aElement->GetAttribute(kNameSpaceID_None, kIdAtom, id);
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
    if (! doc)
        return NS_ERROR_FAILURE;

    rv = nsRDFContentUtils::MakeElementResource(doc, id, aResult);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


nsresult
nsRDFContentUtils::GetElementRefResource(nsIContent* aElement, nsIRDFResource** aResult)
{
    // Perform a reverse mapping from an element in the content model
    // to an RDF resource. Check for a "ref" attribute first, then
    // fallback on an "id" attribute.
    nsresult rv;
    nsAutoString uri;

    nsCOMPtr<nsIAtom> kIdAtom( dont_AddRef(NS_NewAtom("ref")) );
    rv = aElement->GetAttribute(kNameSpaceID_None, kIdAtom, uri);
    NS_ASSERTION(NS_SUCCEEDED(rv), "severe error retrieving attribute");
    if (NS_FAILED(rv)) return rv;

    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
        // We'll use rdf_MakeAbsolute() to translate this to a URL.
        nsCOMPtr<nsIDocument> doc;
        rv = aElement->GetDocument(*getter_AddRefs(doc));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIURI> url = dont_AddRef( doc->GetDocumentURL() );
        NS_ASSERTION(url != nsnull, "element has no document");
        if (! url)
            return NS_ERROR_UNEXPECTED;

        rv = rdf_MakeAbsoluteURI(url, uri);
        if (NS_FAILED(rv)) return rv;

        NS_WITH_SERVICE(nsIRDFService, rdf, kRDFServiceCID, &rv);
        if (NS_FAILED(rv)) return rv;

        rv = rdf->GetUnicodeResource(uri.GetUnicode(), aResult);
    }
    else {
        rv = GetElementResource(aElement, aResult);
    }

    return rv;
}



/*
	Note: this routine is similiar, yet distinctly different from, nsBookmarksService::GetTextForNode
*/

nsresult
nsRDFContentUtils::GetTextForNode(nsIRDFNode* aNode, nsString& aResult)
{
    nsresult		rv;
    nsCOMPtr <nsIRDFResource>	resource;
    nsCOMPtr <nsIRDFLiteral>	literal;
    nsCOMPtr <nsIRDFDate>	dateLiteral;
    nsCOMPtr <nsIRDFInt>	intLiteral;

    if (! aNode) {
        aResult.Truncate();
        rv = NS_OK;
    }
    else if (NS_SUCCEEDED(rv = aNode->QueryInterface(kIRDFResourceIID, getter_AddRefs(resource)))) {
        nsXPIDLCString p;
        if (NS_SUCCEEDED(rv = resource->GetValue( getter_Copies(p) ))) {
            aResult = p;
        }
    }
    else if (NS_SUCCEEDED(rv = aNode->QueryInterface(kIRDFDateIID, getter_AddRefs(dateLiteral)))) {
	PRInt64		theDate;
        if (NS_SUCCEEDED(rv = dateLiteral->GetValue( &theDate ))) {
		// XXX can we cache this somehow instead of creating/destroying it all the time?
		nsCOMPtr <nsIDateTimeFormat> aDateTimeFormat;
		rv = nsComponentManager::CreateInstance(kDateTimeFormatCID, NULL,
			      nsIDateTimeFormat::GetIID(), getter_AddRefs(aDateTimeFormat));
		if (NS_SUCCEEDED(rv) && aDateTimeFormat)
		{
			rv = aDateTimeFormat->FormatPRTime(nsnull /* nsILocale* locale */, kDateFormatShort, kTimeFormatSeconds, (PRTime)theDate, aResult);
			if (NS_FAILED(rv)) {
				aResult.Truncate();
			}
		}
        }
    }
    else if (NS_SUCCEEDED(rv = aNode->QueryInterface(kIRDFIntIID, getter_AddRefs(intLiteral)))) {
	PRInt32		theInt;
	aResult.Truncate();
        if (NS_SUCCEEDED(rv = intLiteral->GetValue( &theInt ))) {
        	aResult.Append(theInt, 10);
        }
    }
    else if (NS_SUCCEEDED(rv = aNode->QueryInterface(kIRDFLiteralIID, getter_AddRefs(literal)))) {
        nsXPIDLString p;
        if (NS_SUCCEEDED(rv = literal->GetValue( getter_Copies(p) ))) {
            aResult = p;
        }
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
                PRUnichar *unicodeString;
                prefix->GetUnicode(&unicodeString);
                aResult.Append(unicodeString);
                aResult.Append(':');
            }
        }
    }

    nsCOMPtr<nsIAtom> tag;
    rv = aElement->GetTag(*getter_AddRefs(tag));
    if (NS_FAILED(rv)) return rv;

    PRUnichar *unicodeString;
    tag->GetUnicode(&unicodeString);
    aResult.Append(unicodeString);

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
                PRUnichar *unicodeString;
                prefix->GetUnicode(&unicodeString);
                aResult.Append(unicodeString);
                aResult.Append(':');
            }
        }
    }

    PRUnichar *unicodeString;
    aTag->GetUnicode(&unicodeString);
    aResult.Append(unicodeString);
    return NS_OK;
}


nsresult
nsRDFContentUtils::MakeElementURI(nsIDocument* aDocument, const nsString& aElementID, nsCString& aURI)
{
    // Convert an element's ID to a URI that can be used to refer to
    // the element in the XUL graph.

    if (aElementID.FindChar(':') > 0) {
        // Assume it's absolute already. Use as is.
        aURI = aElementID;
    }
    else {
        nsresult rv;

        nsCOMPtr<nsIURI> docURL;
        rv = aDocument->GetBaseURL(*getter_AddRefs(docURL));
        if (NS_FAILED(rv)) return rv;

        // XXX Urgh. This is so broken; I'd really just like to use
        // NS_MakeAbsolueURI(). Unfortunatly, doing that breaks
        // MakeElementID in some cases that I haven't yet been able to
        // figure out.
#define USE_BROKEN_RELATIVE_PARSING
#ifdef USE_BROKEN_RELATIVE_PARSING
        nsXPIDLCString spec;
        docURL->GetSpec(getter_Copies(spec));
        if (! spec)
            return NS_ERROR_FAILURE;

        aURI = spec;

        if (aElementID.First() != '#') {
            aURI.Append('#');
        }
        aURI.Append(nsCAutoString(aElementID));
#else
        nsXPIDLCString spec;
        rv = NS_MakeAbsoluteURI(nsCAutoString(aElementID), docURL, getter_Copies(spec));
        if (NS_SUCCEEDED(rv)) {
            aURI = spec;
        }
        else {
            NS_WARNING("MakeElementURI: NS_MakeAbsoluteURI failed");
            aURI = aElementID;
        }
#endif
    }

    return NS_OK;
}


nsresult
nsRDFContentUtils::MakeElementResource(nsIDocument* aDocument, const nsString& aID, nsIRDFResource** aResult)
{
    nsresult rv;

    char buf[256];
    nsCAutoString uri(CBufDescriptor(buf, PR_TRUE, sizeof(buf)));
    rv = MakeElementURI(aDocument, aID, uri);
    if (NS_FAILED(rv)) return rv;

    NS_WITH_SERVICE(nsIRDFService, rdf, kRDFServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = rdf->GetResource(uri, aResult);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create resource");
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}



nsresult
nsRDFContentUtils::MakeElementID(nsIDocument* aDocument, const nsString& aURI, nsString& aElementID)
{
    // Convert a URI into an element ID that can be accessed from the
    // DOM APIs.
    nsresult rv;

    nsCOMPtr<nsIURI> docURL;
    rv = aDocument->GetBaseURL(*getter_AddRefs(docURL));
    if (NS_FAILED(rv)) return rv;

    nsXPIDLCString spec;
    docURL->GetSpec(getter_Copies(spec));
    if (! spec)
        return NS_ERROR_FAILURE;

    if (aURI.Find(spec) == 0) {
#ifdef USE_BROKEN_RELATIVE_PARSING
        static const PRInt32 kFudge = 1;  // XXX assume '#'
#else
        static const PRInt32 kFudge = 0;
#endif
        PRInt32 len = PL_strlen(spec);
        aURI.Right(aElementID, aURI.Length() - (len + kFudge));
    }
    else {
        aElementID = aURI;
    }

    return NS_OK;
}

PRBool
nsRDFContentUtils::IsContainedBy(nsIContent* aElement, nsIContent* aContainer)
{
    nsCOMPtr<nsIContent> element( dont_QueryInterface(aElement) );
    while (element) {
        nsresult rv;

        nsCOMPtr<nsIContent> parent;
        rv = element->GetParent(*getter_AddRefs(parent));
        if (NS_FAILED(rv)) return PR_FALSE;

        if (parent.get() == aContainer)
            return PR_TRUE;

        element = parent;
    }

    return PR_FALSE;
}


nsresult
nsRDFContentUtils::GetResource(PRInt32 aNameSpaceID, nsIAtom* aAttribute, nsIRDFResource** aResult)
{
    // construct a fully-qualified URI from the namespace/tag pair.
    NS_PRECONDITION(aAttribute != nsnull, "null ptr");
    if (! aAttribute)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    nsAutoString attr;
    rv = aAttribute->ToString(attr);
    if (NS_FAILED(rv)) return rv;

    return GetResource(aNameSpaceID, attr, aResult);
}


nsresult
nsRDFContentUtils::GetResource(PRInt32 aNameSpaceID, const nsString& aAttribute, nsIRDFResource** aResult)
{
    // construct a fully-qualified URI from the namespace/tag pair.

    // XXX should we allow nodes with no namespace???
    NS_PRECONDITION(aNameSpaceID != kNameSpaceID_Unknown, "no namespace");
    if (aNameSpaceID == kNameSpaceID_Unknown)
        return NS_ERROR_UNEXPECTED;

    nsresult rv;

    NS_WITH_SERVICE(nsINameSpaceManager, nsmgr, kNameSpaceManagerCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsAutoString uri;
    rv = nsmgr->GetNameSpaceURI(aNameSpaceID, uri);
    // XXX ignore failure; treat as "no namespace"

    // XXX check to see if we need to insert a '/' or a '#'
    if (0 < uri.Length() && uri.Last() != '#' && uri.Last() != '/' && aAttribute.First() != '#')
        uri.Append('#');

    uri.Append(aAttribute);

    NS_WITH_SERVICE(nsIRDFService, rdf, kRDFServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = rdf->GetResource(nsCAutoString(uri), aResult);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get resource");
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}
