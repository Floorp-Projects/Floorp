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

#include "nsICSSParser.h"
#include "nsIContent.h"
#include "nsIDOMComment.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsICSSStyleSheet.h"
#include "nsIRDFContent.h"
#include "nsIRDFDocument.h"
#include "nsIRDFNode.h"
#include "nsIRDFResourceManager.h"
#include "nsIURL.h"
#include "nsIWebShell.h"
#include "nsLayoutCID.h"
#include "nsRDFContentSink.h"
#include "nsINameSpaceManager.h"

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kICSSParserIID,           NS_ICSS_PARSER_IID); // XXX grr..
static NS_DEFINE_IID(kIDOMCommentIID,          NS_IDOMCOMMENT_IID);
static NS_DEFINE_IID(kIRDFContentSinkIID,      NS_IRDFCONTENTSINK_IID);
static NS_DEFINE_IID(kIScrollableViewIID,      NS_ISCROLLABLEVIEW_IID);
static NS_DEFINE_IID(kIRDFDocumentIID,         NS_IRDFDOCUMENT_IID);

static NS_DEFINE_CID(kCSSParserCID,            NS_CSSPARSER_CID);


class nsRDFDocumentContentSink : public nsRDFContentSink {
public:
    nsRDFDocumentContentSink(void);
    virtual ~nsRDFDocumentContentSink(void);

    virtual nsresult Init(nsIDocument* aDoc,
                          nsIURL* aURL,
                          nsIWebShell* aContainer);

    // nsIContentSink
    NS_IMETHOD WillBuildModel(void);
    NS_IMETHOD DidBuildModel(PRInt32 aQualityLevel);
    NS_IMETHOD AddProcessingInstruction(const nsIParserNode& aNode);

protected:
    void StartLayout(void);

    virtual nsresult OpenObject(const nsIParserNode& aNode);

    // Style sheets
    nsresult LoadStyleSheet(nsIURL* aURL,
                            nsIUnicharInputStream* aUIN);

    nsIStyleSheet* mStyleSheet;

    // Document, webshell, etc.
    nsIDocument* mDocument;
    nsIContent*  mRootElement;
    nsIWebShell* mWebShell;
};

////////////////////////////////////////////////////////////////////////

nsRDFDocumentContentSink::nsRDFDocumentContentSink(void)
{
    mDocument = nsnull;
    mWebShell = nsnull;
    mRootElement = nsnull;
}


nsRDFDocumentContentSink::~nsRDFDocumentContentSink(void)
{
    NS_IF_RELEASE(mDocument);
    NS_IF_RELEASE(mWebShell);
    NS_IF_RELEASE(mRootElement);
}


nsresult
nsRDFDocumentContentSink::Init(nsIDocument* aDoc,
                               nsIURL* aURL,
                               nsIWebShell* aContainer)
{
    NS_PRECONDITION(aDoc && aContainer, "null ptr");
    if (!aDoc || !aContainer)
        return NS_ERROR_NULL_POINTER;

    nsINameSpaceManager* nameSpaceManager = nsnull;
    nsresult rv = aDoc->GetNameSpaceManager(nameSpaceManager);

    if (NS_SUCCEEDED(rv)) {
      rv = nsRDFContentSink::Init(aURL, nameSpaceManager);
      if (NS_SUCCEEDED(rv)) {
        mDocument = aDoc;
        NS_ADDREF(aDoc);

        mWebShell = aContainer;
        NS_ADDREF(aContainer);
      }
      NS_RELEASE(nameSpaceManager);
    }

    return rv;
}


void
nsRDFDocumentContentSink::StartLayout(void)
{
    PRInt32 i, ns = mDocument->GetNumberOfShells();
    for (i = 0; i < ns; i++) {
        nsIPresShell* shell = mDocument->GetShellAt(i);
        if (nsnull != shell) {
            // Make shell an observer for next time
            shell->BeginObservingDocument();

            // Resize-reflow this time
            nsIPresContext* cx = shell->GetPresContext();
            nsRect r;
            cx->GetVisibleArea(r);
            shell->InitialReflow(r.width, r.height);
            NS_RELEASE(cx);

            // Now trigger a refresh
            nsIViewManager* vm = shell->GetViewManager();
            if (nsnull != vm) {
                vm->EnableRefresh();
                NS_RELEASE(vm);
            }

            NS_RELEASE(shell);
        }
    }

    // If the document we are loading has a reference or it is a top level
    // frameset document, disable the scroll bars on the views.
    const char* ref;
    (void)mDocumentURL->GetRef(&ref);
    PRBool topLevelFrameset = PR_FALSE;
    if (mWebShell) {
        nsIWebShell* rootWebShell;
        mWebShell->GetRootWebShell(rootWebShell);
        if (mWebShell == rootWebShell) {
            topLevelFrameset = PR_TRUE;
        }
        NS_IF_RELEASE(rootWebShell);
    }

    if ((nsnull != ref) || topLevelFrameset) {
        // XXX support more than one presentation-shell here

        // Get initial scroll preference and save it away; disable the
        // scroll bars.
        PRInt32 i, ns = mDocument->GetNumberOfShells();
        for (i = 0; i < ns; i++) {
            nsIPresShell* shell = mDocument->GetShellAt(i);
            if (nsnull != shell) {
                nsIViewManager* vm = shell->GetViewManager();
                if (nsnull != vm) {
                    nsIView* rootView = nsnull;
                    vm->GetRootView(rootView);
                    if (nsnull != rootView) {
                        nsIScrollableView* sview = nsnull;
                        rootView->QueryInterface(kIScrollableViewIID, (void**) &sview);
                        if (nsnull != sview) {
#if 0
                            if (topLevelFrameset)
                                mOriginalScrollPreference = nsScrollPreference_kNeverScroll;
                            else
                                sview->GetScrollPreference(mOriginalScrollPreference);
#endif
                            sview->SetScrollPreference(nsScrollPreference_kNeverScroll);
                        }
                    }
                    NS_RELEASE(vm);
                }
                NS_RELEASE(shell);
            }
        }
    }
}


// XXX Borrowed from HTMLContentSink. Should be shared.
nsresult
nsRDFDocumentContentSink::LoadStyleSheet(nsIURL* aURL,
                                         nsIUnicharInputStream* aUIN)
{
    nsresult rv;
    nsICSSParser* parser;
    rv = nsRepository::CreateInstance(kCSSParserCID,
                                      nsnull,
                                      kICSSParserIID,
                                      (void**) &parser);

    if (NS_SUCCEEDED(rv)) {
        nsICSSStyleSheet* sheet = nsnull;
        // XXX note: we are ignoring rv until the error code stuff in the
        // input routines is converted to use nsresult's
        parser->SetCaseSensative(PR_TRUE);
        parser->Parse(aUIN, aURL, sheet);
        if (nsnull != sheet) {
            mDocument->AddStyleSheet(sheet);
            NS_RELEASE(sheet);
            rv = NS_OK;
        } else {
            rv = NS_ERROR_OUT_OF_MEMORY;/* XXX */
        }
        NS_RELEASE(parser);
    }
    return rv;
}


////////////////////////////////////////////////////////////////////////
// nsIContentSink interface

NS_IMETHODIMP 
nsRDFDocumentContentSink::WillBuildModel(void)
{
    // Notify document that the load is beginning
    mDocument->BeginLoad();
    nsresult result = NS_OK;

    return result;
}

NS_IMETHODIMP 
nsRDFDocumentContentSink::DidBuildModel(PRInt32 aQualityLevel)
{
    // XXX this is silly; who cares?
    PRInt32 i, ns = mDocument->GetNumberOfShells();
    for (i = 0; i < ns; i++) {
        nsIPresShell* shell = mDocument->GetShellAt(i);
        if (nsnull != shell) {
            nsIViewManager* vm = shell->GetViewManager();
            if(vm) {
                vm->SetQuality(nsContentQuality(aQualityLevel));
            }
            NS_RELEASE(vm);
            NS_RELEASE(shell);
        }
    }

    StartLayout();

    // XXX Should scroll to ref when that makes sense
    // ScrollToRef();

    mDocument->EndLoad();
    return NS_OK;
}


static nsresult
rdf_GetQuotedAttributeValue(nsString& aSource, 
                            const nsString& aAttribute,
                            nsString& aValue)
{
static const char kQuote = '\"';
static const char kApostrophe = '\'';

    PRInt32 offset;
    PRInt32 endOffset = -1;
    nsresult result = NS_OK;

    offset = aSource.Find(aAttribute);
    if (-1 != offset) {
        offset = aSource.Find('=', offset);

        PRUnichar next = aSource.CharAt(++offset);
        if (kQuote == next) {
            endOffset = aSource.Find(kQuote, ++offset);
        }
        else if (kApostrophe == next) {
            endOffset = aSource.Find(kApostrophe, ++offset);	  
        }
  
        if (-1 != endOffset) {
            aSource.Mid(aValue, offset, endOffset-offset);
        }
        else {
            // Mismatched quotes - return an error
            result = NS_ERROR_FAILURE;
        }
    }
    else {
        aValue.Truncate();
    }

    return result;
}



NS_IMETHODIMP 
nsRDFDocumentContentSink::AddProcessingInstruction(const nsIParserNode& aNode)
{
static const char kStyleSheetPI[] = "<?xml-stylesheet";
static const char kCSSType[] = "text/css";

    nsresult rv;
    if (NS_FAILED(rv = nsRDFContentSink::AddProcessingInstruction(aNode)))
        return rv;

    // XXX For now, we don't add the PI to the content model.
    // We just check for a style sheet PI
    nsAutoString text, type, href;
    PRInt32 offset;

    text = aNode.GetText();

    offset = text.Find(kStyleSheetPI);

    // If it's a stylesheet PI...
    if (0 == offset) {
        rv = rdf_GetQuotedAttributeValue(text, "href", href);
        // If there was an error or there's no href, we can't do
        // anything with this PI
        if ((NS_OK != rv) || (0 == href.Length())) {
            return rv;
        }
    
        rv = rdf_GetQuotedAttributeValue(text, "type", type);
        if (NS_OK != rv) {
            return rv;
        }
    
        if (type.Equals(kCSSType)) {
            nsIURL* url = nsnull;
            nsIUnicharInputStream* uin = nsnull;
            nsAutoString absURL;
            nsIURL* docURL = mDocument->GetDocumentURL();
            nsAutoString emptyURL;
            emptyURL.Truncate();
            rv = NS_MakeAbsoluteURL(docURL, emptyURL, href, absURL);
            if (NS_OK != rv) {
                return rv;
            }
            NS_RELEASE(docURL);
            rv = NS_NewURL(&url, absURL);
            if (NS_OK != rv) {
                return rv;
            }
            nsIInputStream* iin;
            rv = NS_OpenURL(url, &iin);
            if (NS_OK != rv) {
                NS_RELEASE(url);
                return rv;
            }
            rv = NS_NewConverterStream(&uin, nsnull, iin);
            NS_RELEASE(iin);
            if (NS_OK != rv) {
                NS_RELEASE(url);
                return rv;
            }
      
            rv = LoadStyleSheet(url, uin);
            NS_RELEASE(uin);
            NS_RELEASE(url);
        }
    }

    return rv;
}


nsresult
nsRDFDocumentContentSink::OpenObject(const nsIParserNode& aNode)
{
    nsresult rv;

    if (NS_FAILED(rv = nsRDFContentSink::OpenObject(aNode)))
        return rv;

    // Arbitrarily make the document root be the first container
    // element in the RDF.
    if (! mRootElement) {
        nsAutoString uri;
        if (NS_FAILED(rv = GetIdAboutAttribute(aNode, uri)))
            return rv;

        nsIRDFNode* resource;
        if (NS_FAILED(rv = mRDFResourceManager->GetNode(uri, resource)))
            return rv;

        nsIRDFDocument* rdfDoc;
        if (NS_SUCCEEDED(rv = mDocument->QueryInterface(kIRDFDocumentIID, (void**) &rdfDoc))) {
            if (NS_SUCCEEDED(rv = rdfDoc->SetRootResource(resource))) {
                mRootElement = mDocument->GetRootContent();
            }
            NS_RELEASE(rdfDoc);
        }

        NS_RELEASE(resource);

        // don't release the rdfElement since we're keeping
        // a reference to it in mRootElement
    }
    return rv;
}


////////////////////////////////////////////////////////////////////////

nsresult
NS_NewRDFDocumentContentSink(nsIRDFContentSink** aResult,
                             nsIDocument* aDoc,
                             nsIURL* aURL,
                             nsIWebShell* aWebShell)
{
    NS_PRECONDITION(aResult, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    nsRDFDocumentContentSink* it;
    NS_NEWXPCOM(it, nsRDFDocumentContentSink);
    if (! it)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = it->Init(aDoc, aURL, aWebShell);
    if (NS_FAILED(rv)) {
        delete it;
        return rv;
    }
    return it->QueryInterface(kIRDFContentSinkIID, (void **)aResult);
}


