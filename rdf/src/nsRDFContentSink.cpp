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

  This is an NGLayout-style content sink that knows how to build an
  RDF content model from XML-serialized RDF.

  For more information on RDF, see http://www.w3.org/TR/WDf-rdf-syntax.

  This code is based on the working draft "last call", which was
  published on Oct 8, 1998.

  Open Issues ------------------

  1) factoring code with nsXMLContentSink - There's some amount of
     common code between this and the HTML content sink. This will
     increase as we support more and more HTML elements. How can code
     from the code be factored?

  TO DO ------------------------

  1) Using the nsIRDFContent and nsIRDFContainerContent interfaces to
     actually build the content model is a red herring. What I
     *really* want to do is just build an RDF graph, then let the
     presentation actually generate the "content model" on demand.

  2) Handle resource references in properties

  3) Make sure all shortcut syntax works right.

*/

#include "nsCRT.h"
#include "nsHTMLEntities.h" // XXX for NS_EntityToUnicode()
#include "nsICSSParser.h"
#include "nsIContent.h"
#include "nsIDOMComment.h"
#include "nsIDocument.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIRDFContent.h"
#include "nsIRDFContent.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFDocument.h"
#include "nsIRDFNode.h"
#include "nsIRDFResourceManager.h"
#include "nsIScriptContext.h"
#include "nsIScriptContextOwner.h"
#include "nsIScriptObjectOwner.h"
#include "nsIServiceManager.h"
#include "nsICSSStyleSheet.h"
#include "nsITextContent.h"
#include "nsIURL.h"
#include "nsIUnicharInputStream.h"
#include "nsIViewManager.h"
#include "nsIWebShell.h"
#include "nsIXMLContent.h"
#include "nsIXMLDocument.h"
#include "nsLayoutCID.h"
#include "nsRDFCID.h"
#include "nsRDFContentSink.h"
#include "nsVoidArray.h"
#include "prlog.h"
#include "prmem.h"
#include "prtime.h"

static const char kNameSpaceSeparator[] = ":";
static const char kNameSpaceDef[] = "xmlns";
static const char kStyleSheetPI[] = "<?xml-stylesheet";
static const char kCSSType[] = "text/css";
static const char kQuote = '\"';
static const char kApostrophe = '\'';

////////////////////////////////////////////////////////////////////////
// RDF core vocabulary

#include "rdf.h"
#define RDF_NAMESPACE_URI  "http://www.w3.org/TR/WD-rdf-syntax#"
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, Alt);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, Bag);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, Description);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, ID);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, RDF);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, Seq);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, about);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, aboutEach);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, bagID);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, instanceOf);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, li);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, resource);

static const char kRDFNameSpaceURI[] = RDF_NAMESPACE_URI;

////////////////////////////////////////////////////////////////////////
// XPCOM IIDs

static NS_DEFINE_IID(kICSSParserIID,           NS_ICSS_PARSER_IID); // XXX grr..
static NS_DEFINE_IID(kIContentSinkIID,         NS_ICONTENT_SINK_IID); // XXX grr...
static NS_DEFINE_IID(kIDOMCommentIID,          NS_IDOMCOMMENT_IID);
static NS_DEFINE_IID(kIRDFContentSinkIID,      NS_IRDFCONTENTSINK_IID);
static NS_DEFINE_IID(kIRDFDataSourceIID,       NS_IRDFDATASOURCE_IID);
static NS_DEFINE_IID(kIRDFDocumentIID,         NS_IRDFDOCUMENT_IID);
static NS_DEFINE_IID(kIRDFResourceManagerIID,  NS_IRDFRESOURCEMANAGER_IID);
static NS_DEFINE_IID(kIScrollableViewIID,      NS_ISCROLLABLEVIEW_IID);
static NS_DEFINE_IID(kISupportsIID,            NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIXMLContentSinkIID,      NS_IXMLCONTENT_SINK_IID);
static NS_DEFINE_IID(kIXMLDocumentIID,         NS_IXMLDOCUMENT_IID);

static NS_DEFINE_CID(kCSSParserCID,            NS_CSSPARSER_CID);
static NS_DEFINE_CID(kRDFResourceManagerCID,   NS_RDFRESOURCEMANAGER_CID);
static NS_DEFINE_CID(kRDFMemoryDataSourceCID,  NS_RDFMEMORYDATASOURCE_CID);


////////////////////////////////////////////////////////////////////////
// Utility routines

static nsresult
rdf_GetQuotedAttributeValue(nsString& aSource, 
                            const nsString& aAttribute,
                            nsString& aValue)
{
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

// XXX Code copied from nsHTMLContentSink. It should be shared.
static void
rdf_StripAndConvert(nsString& aResult)
{
    // Strip quotes if present
    PRUnichar first = aResult.First();
    if ((first == '"') || (first == '\'')) {
        if (aResult.Last() == first) {
            aResult.Cut(0, 1);
            PRInt32 pos = aResult.Length() - 1;
            if (pos >= 0) {
                aResult.Cut(pos, 1);
            }
        } else {
            // Mismatched quotes - leave them in
        }
    }

    // Reduce any entities
    // XXX Note: as coded today, this will only convert well formed
    // entities.  This may not be compatible enough.
    // XXX there is a table in navigator that translates some numeric entities
    // should we be doing that? If so then it needs to live in two places (bad)
    // so we should add a translate numeric entity method from the parser...
    char cbuf[100];
    PRInt32 index = 0;
    while (index < aResult.Length()) {
        // If we have the start of an entity (and it's not at the end of
        // our string) then translate the entity into it's unicode value.
        if ((aResult.CharAt(index++) == '&') && (index < aResult.Length())) {
            PRInt32 start = index - 1;
            PRUnichar e = aResult.CharAt(index);
            if (e == '#') {
                // Convert a numeric character reference
                index++;
                char* cp = cbuf;
                char* limit = cp + sizeof(cbuf) - 1;
                PRBool ok = PR_FALSE;
                PRInt32 slen = aResult.Length();
                while ((index < slen) && (cp < limit)) {
                    PRUnichar e = aResult.CharAt(index);
                    if (e == ';') {
                        index++;
                        ok = PR_TRUE;
                        break;
                    }
                    if ((e >= '0') && (e <= '9')) {
                        *cp++ = char(e);
                        index++;
                        continue;
                    }
                    break;
                }
                if (!ok || (cp == cbuf)) {
                    continue;
                }
                *cp = '\0';
                if (cp - cbuf > 5) {
                    continue;
                }
                PRInt32 ch = PRInt32( ::atoi(cbuf) );
                if (ch > 65535) {
                    continue;
                }

                // Remove entity from string and replace it with the integer
                // value.
                aResult.Cut(start, index - start);
                aResult.Insert(PRUnichar(ch), start);
                index = start + 1;
            }
            else if (((e >= 'A') && (e <= 'Z')) ||
                     ((e >= 'a') && (e <= 'z'))) {
                // Convert a named entity
                index++;
                char* cp = cbuf;
                char* limit = cp + sizeof(cbuf) - 1;
                *cp++ = char(e);
                PRBool ok = PR_FALSE;
                PRInt32 slen = aResult.Length();
                while ((index < slen) && (cp < limit)) {
                    PRUnichar e = aResult.CharAt(index);
                    if (e == ';') {
                        index++;
                        ok = PR_TRUE;
                        break;
                    }
                    if (((e >= '0') && (e <= '9')) ||
                        ((e >= 'A') && (e <= 'Z')) ||
                        ((e >= 'a') && (e <= 'z'))) {
                        *cp++ = char(e);
                        index++;
                        continue;
                    }
                    break;
                }
                if (!ok || (cp == cbuf)) {
                    continue;
                }
                *cp = '\0';
                PRInt32 ch = NS_EntityToUnicode(cbuf);
                if (ch < 0) {
                    continue;
                }

                // Remove entity from string and replace it with the integer
                // value.
                aResult.Cut(start, index - start);
                aResult.Insert(PRUnichar(ch), start);
                index = start + 1;
            }
            else if (e == '{') {
                // Convert a script entity
                // XXX write me!
            }
        }
    }
}


static void
rdf_FullyQualifyURI(const nsIURL* base, nsString& spec)
{
    // This is a fairly heavy-handed way to do this, but...I don't
    // like typing.
    nsIURL* url;
    if (NS_SUCCEEDED(NS_NewURL(&url, base, spec))) {
        url->ToString(spec);
        url->Release();
    }
}

////////////////////////////////////////////////////////////////////////
// Factory method

nsresult
NS_NewRDFContentSink(nsIRDFContentSink** aResult,
                     nsIDocument* aDoc,
                     nsIURL* aURL,
                     nsIWebShell* aWebShell)
{
    NS_PRECONDITION(nsnull != aResult, "null ptr");
    if (nsnull == aResult) {
        return NS_ERROR_NULL_POINTER;
    }
    nsRDFContentSink* it;
    NS_NEWXPCOM(it, nsRDFContentSink);
    if (nsnull == it) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    nsresult rv = it->Init(aDoc, aURL, aWebShell);
    if (NS_OK != rv) {
        delete it;
        return rv;
    }
    return it->QueryInterface(kIRDFContentSinkIID, (void **)aResult);
}


////////////////////////////////////////////////////////////////////////


nsRDFContentSink::nsRDFContentSink()
{
    NS_INIT_REFCNT();
    mDocument = nsnull;
    mDocumentURL = nsnull;
    mWebShell = nsnull;
    mRootElement = nsnull;
    mRDFResourceManager = nsnull;
    mDataSource = nsnull;
    mGenSym = 0;
    mNameSpaces = nsnull;
    mNestLevel = 0;
    mContextStack = nsnull;
    mText = nsnull;
    mTextLength = 0;
    mTextSize = 0;
    mConstrainSize = PR_TRUE;
}

nsRDFContentSink::~nsRDFContentSink()
{
    NS_IF_RELEASE(mDocument);
    NS_IF_RELEASE(mDocumentURL);
    NS_IF_RELEASE(mWebShell);
    NS_IF_RELEASE(mRootElement);
    if (mRDFResourceManager) {
        nsServiceManager::ReleaseService(kRDFResourceManagerCID, mRDFResourceManager);
    }
    NS_IF_RELEASE(mDataSource);
    if (mNameSpaces) {
        // There shouldn't be any here except in an error condition
        PRInt32 i, count = mNameSpaces->Count();
    
        for (i=0; i < count; i++) {
            NameSpaceStruct *ns = (NameSpaceStruct *)mNameSpaces->ElementAt(i);
      
            if (nsnull != ns) {
                NS_IF_RELEASE(ns->mPrefix);
                delete ns;
            }
        }
        delete mNameSpaces;
    }
    if (mContextStack) {
        NS_PRECONDITION(GetCurrentNestLevel() == 0, "content stack not empty");

        // XXX we should never need to do this, but, we'll write the
        // code all the same. If someone left the content stack dirty,
        // pop all the elements off the stack and release them.
        while (GetCurrentNestLevel() > 0) {
            nsIRDFNode* resource;
            RDFContentSinkState state;
            PopContext(resource, state);
            NS_IF_RELEASE(resource);
        }

        delete mContextStack;
    }
    PR_FREEIF(mText);
}

////////////////////////////////////////////////////////////////////////

nsresult
nsRDFContentSink::Init(nsIDocument* aDoc,
                       nsIURL* aURL,
                       nsIWebShell* aContainer)
{
    NS_PRECONDITION(nsnull != aDoc, "null ptr");
    NS_PRECONDITION(nsnull != aURL, "null ptr");
    NS_PRECONDITION(nsnull != aContainer, "null ptr");
    if ((nsnull == aDoc) || (nsnull == aURL) || (nsnull == aContainer)) {
        return NS_ERROR_NULL_POINTER;
    }

    mDocument = aDoc;
    NS_ADDREF(aDoc);

    mDocumentURL = aURL;
    NS_ADDREF(aURL);
    mWebShell = aContainer;
    NS_ADDREF(aContainer);

    nsresult rv;
    if (NS_FAILED(rv = nsServiceManager::GetService(kRDFResourceManagerCID,
                                                    kIRDFResourceManagerIID,
                                                    (nsISupports**) &mRDFResourceManager)))
        return rv;

    if (NS_FAILED(rv = nsRepository::CreateInstance(kRDFMemoryDataSourceCID,
                                                    NULL,
                                                    kIRDFDataSourceIID,
                                                    (void**) &mDataSource)))
        return rv;

    nsIRDFDocument* rdfDoc;
    if (NS_FAILED(rv = mDocument->QueryInterface(kIRDFDocumentIID, (void**) &rdfDoc)))
        return rv;

    rv = rdfDoc->SetDataSource(mDataSource);
    NS_RELEASE(rdfDoc);

    mState = eRDFContentSinkState_InProlog;
    return rv;
}

////////////////////////////////////////////////////////////////////////
// nsISupports interface

NS_IMPL_ADDREF(nsRDFContentSink);
NS_IMPL_RELEASE(nsRDFContentSink);

NS_IMETHODIMP
nsRDFContentSink::QueryInterface(REFNSIID iid, void** result)
{
    NS_PRECONDITION(result, "null ptr");
    if (! result)
        return NS_ERROR_NULL_POINTER;

    *result = NULL;
    if (iid.Equals(kIRDFContentSinkIID) ||
        iid.Equals(kIXMLContentSinkIID) ||
        iid.Equals(kIContentSinkIID) ||
        iid.Equals(kISupportsIID)) {
        *result = static_cast<nsIRDFContentSink*>(this);
        AddRef();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}


////////////////////////////////////////////////////////////////////////
// nsIContentSink interface

NS_IMETHODIMP 
nsRDFContentSink::WillBuildModel(void)
{
    // Notify document that the load is beginning
    mDocument->BeginLoad();
    nsresult result = NS_OK;

    return result;
}

NS_IMETHODIMP 
nsRDFContentSink::DidBuildModel(PRInt32 aQualityLevel)
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

NS_IMETHODIMP 
nsRDFContentSink::WillInterrupt(void)
{
    return NS_OK;
}

NS_IMETHODIMP 
nsRDFContentSink::WillResume(void)
{
    return NS_OK;
}



NS_IMETHODIMP 
nsRDFContentSink::OpenContainer(const nsIParserNode& aNode)
{
    // XXX Hopefully the parser will flag this before we get here. If
    // we're in the epilog, there should be no new elements
    NS_PRECONDITION(mState != eRDFContentSinkState_InEpilog, "tag in RDF doc epilog");

    FlushText();

    // We must register namespace declarations found in the attribute
    // list of an element before creating the element. This is because
    // the namespace prefix for an element might be declared within
    // the attribute list.
    FindNameSpaceAttributes(aNode);

    nsIContent* content = NULL;
    nsresult result;

    RDFContentSinkState lastState = mState;
    switch (mState) {
    case eRDFContentSinkState_InProlog:
        result = OpenRDF(aNode);
        break;

    case eRDFContentSinkState_InDocumentElement:
        result = OpenObject(aNode);
        break;

    case eRDFContentSinkState_InDescriptionElement:
        result = OpenProperty(aNode);
        break;

    case eRDFContentSinkState_InContainerElement:
        result = OpenMember(aNode);
        break;

    case eRDFContentSinkState_InPropertyElement:
        result = OpenValue(aNode);
        break;

    case eRDFContentSinkState_InMemberElement:
        result = OpenValue(aNode);
        break;

    case eRDFContentSinkState_InEpilog:
        PR_ASSERT(0);
        result = NS_ERROR_UNEXPECTED; // XXX
        break;
    }

    return result;
}

NS_IMETHODIMP 
nsRDFContentSink::CloseContainer(const nsIParserNode& aNode)
{
    FlushText();

    nsIRDFNode* resource;
    if (NS_FAILED(PopContext(resource, mState))) {
        // XXX parser didn't catch unmatched tags?
        PR_ASSERT(0);
        return NS_ERROR_UNEXPECTED; // XXX
    }

    PRInt32 nestLevel = GetCurrentNestLevel();
    if (nestLevel == 0)
        mState = eRDFContentSinkState_InEpilog;

    CloseNameSpacesAtNestLevel(nestLevel);
      
    NS_IF_RELEASE(resource);
    return NS_OK;
}


NS_IMETHODIMP 
nsRDFContentSink::AddLeaf(const nsIParserNode& aNode)
{
    // XXX For now, all leaf content is character data
    AddCharacterData(aNode);
    return NS_OK;
}

NS_IMETHODIMP
nsRDFContentSink::NotifyError(nsresult aErrorResult)
{
    printf("nsRDFContentSink::NotifyError\n");
    return NS_OK;
}

// nsIXMLContentSink
NS_IMETHODIMP 
nsRDFContentSink::AddXMLDecl(const nsIParserNode& aNode)
{
    // XXX We'll ignore it for now
    printf("nsRDFContentSink::AddXMLDecl\n");
    return NS_OK;
}


NS_IMETHODIMP 
nsRDFContentSink::AddComment(const nsIParserNode& aNode)
{
    FlushText();
    nsAutoString text;
    //nsIDOMComment *domComment;
    nsresult result = NS_OK;

    text = aNode.GetText();

    // XXX add comment here...

    return result;
}


NS_IMETHODIMP 
nsRDFContentSink::AddProcessingInstruction(const nsIParserNode& aNode)
{
    FlushText();

    // XXX For now, we don't add the PI to the content model.
    // We just check for a style sheet PI
    nsAutoString text, type, href;
    PRInt32 offset;
    nsresult result = NS_OK;

    text = aNode.GetText();

    offset = text.Find(kStyleSheetPI);
    // If it's a stylesheet PI...
    if (0 == offset) {
        result = rdf_GetQuotedAttributeValue(text, "href", href);
        // If there was an error or there's no href, we can't do
        // anything with this PI
        if ((NS_OK != result) || (0 == href.Length())) {
            return result;
        }
    
        result = rdf_GetQuotedAttributeValue(text, "type", type);
        if (NS_OK != result) {
            return result;
        }
    
        if (type.Equals(kCSSType)) {
            nsIURL* url = nsnull;
            nsIUnicharInputStream* uin = nsnull;
            nsAutoString absURL;
            nsIURL* docURL = mDocument->GetDocumentURL();
            nsAutoString emptyURL;
            emptyURL.Truncate();
            result = NS_MakeAbsoluteURL(docURL, emptyURL, href, absURL);
            if (NS_OK != result) {
                return result;
            }
            NS_RELEASE(docURL);
            result = NS_NewURL(&url, nsnull, absURL);
            if (NS_OK != result) {
                return result;
            }
            PRInt32 ec;
            nsIInputStream* iin = url->Open(&ec);
            if (nsnull == iin) {
                NS_RELEASE(url);
                return (nsresult) ec;/* XXX fix url->Open */
            }
            result = NS_NewConverterStream(&uin, nsnull, iin);
            NS_RELEASE(iin);
            if (NS_OK != result) {
                NS_RELEASE(url);
                return result;
            }
      
            result = LoadStyleSheet(url, uin);
            NS_RELEASE(uin);
            NS_RELEASE(url);
        }
    }

    return result;
}

NS_IMETHODIMP 
nsRDFContentSink::AddDocTypeDecl(const nsIParserNode& aNode)
{
    printf("nsRDFContentSink::AddDocTypeDecl\n");
    return NS_OK;
}


NS_IMETHODIMP 
nsRDFContentSink::AddCharacterData(const nsIParserNode& aNode)
{
    nsAutoString text = aNode.GetText();

    PRInt32 addLen = text.Length();
    if (0 == addLen) {
        return NS_OK;
    }

    // Create buffer when we first need it
    if (0 == mTextSize) {
        mText = (PRUnichar *) PR_MALLOC(sizeof(PRUnichar) * 4096);
        if (nsnull == mText) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
        mTextSize = 4096;
    }

    // Copy data from string into our buffer; flush buffer when it fills up
    PRInt32 offset = 0;
    while (0 != addLen) {
        PRInt32 amount = mTextSize - mTextLength;
        if (amount > addLen) {
            amount = addLen;
        }
        if (0 == amount) {
            if (mConstrainSize) {
                nsresult rv = FlushText();
                if (NS_OK != rv) {
                    return rv;
                }
            }
            else {
                mTextSize += addLen;
                mText = (PRUnichar *) PR_REALLOC(mText, sizeof(PRUnichar) * mTextSize);
                if (nsnull == mText) {
                    return NS_ERROR_OUT_OF_MEMORY;
                }
            }
        }
        memcpy(&mText[mTextLength], text.GetUnicode() + offset,
               sizeof(PRUnichar) * amount);
        mTextLength += amount;
        offset += amount;
        addLen -= amount;
    }

    return NS_OK;
}

NS_IMETHODIMP 
nsRDFContentSink::AddUnparsedEntity(const nsIParserNode& aNode)
{
    printf("nsRDFContentSink::AddUnparsedEntity\n");
    return NS_OK;
}

NS_IMETHODIMP 
nsRDFContentSink::AddNotation(const nsIParserNode& aNode)
{
    printf("nsRDFContentSink::AddNotation\n");
    return NS_OK;
}

NS_IMETHODIMP 
nsRDFContentSink::AddEntityReference(const nsIParserNode& aNode)
{
    printf("nsRDFContentSink::AddEntityReference\n");
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// Implementation methods

void
nsRDFContentSink::StartLayout()
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
    const char* ref = mDocumentURL->GetRef();
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
                            if (topLevelFrameset)
                                mOriginalScrollPreference = nsScrollPreference_kNeverScroll;
                            else
                                sview->GetScrollPreference(mOriginalScrollPreference);
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
nsRDFContentSink::LoadStyleSheet(nsIURL* aURL,
                                 nsIUnicharInputStream* aUIN)
{
    /* XXX use repository */
    nsresult rv;
    nsICSSParser* parser;
    rv = nsRepository::CreateInstance(kCSSParserCID,
                                      NULL,
                                      kICSSParserIID,
                                      (void**) &parser);

    if (NS_SUCCEEDED(rv)) {
        nsICSSStyleSheet* sheet = nsnull;
        // XXX note: we are ignoring rv until the error code stuff in the
        // input routines is converted to use nsresult's
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
// Text buffering

static PRBool
rdf_IsDataInBuffer(PRUnichar* buffer, PRInt32 length)
{
    for (PRInt32 i = 0; i < length; ++i) {
        if (buffer[i] == ' ' ||
            buffer[i] == '\t' ||
            buffer[i] == '\n' ||
            buffer[i] == '\r')
            continue;

        return PR_TRUE;
    }
    return PR_FALSE;
}


nsresult
nsRDFContentSink::FlushText(PRBool aCreateTextNode, PRBool* aDidFlush)
{
    nsresult rv = NS_OK;
    PRBool didFlush = PR_FALSE;
    if (0 != mTextLength) {
        if (aCreateTextNode && rdf_IsDataInBuffer(mText, mTextLength)) {
            // XXX if there's anything but whitespace, then we'll
            // create a text node.

            switch (mState) {
            case eRDFContentSinkState_InMemberElement:
            case eRDFContentSinkState_InPropertyElement: {
                nsAutoString value;
                value.Append(mText, mTextLength);

                rv = Assert(GetContextElement(1),
                            GetContextElement(0),
                            value);

                } break;

            default:
                // just ignore it
                break;
            }
        }
        mTextLength = 0;
        didFlush = PR_TRUE;
    }
    if (nsnull != aDidFlush) {
        *aDidFlush = didFlush;
    }
    return rv;
}



////////////////////////////////////////////////////////////////////////
// Qualified name resolution

nsresult
nsRDFContentSink::SplitQualifiedName(const nsString& aQualifiedName,
                                     nsString& rNameSpaceURI,
                                     nsString& rProperty)
{
    rProperty = aQualifiedName;

    nsAutoString nameSpace;
    PRInt32 nsoffset = rProperty.Find(kNameSpaceSeparator);
    if (-1 != nsoffset) {
        rProperty.Left(nameSpace, nsoffset);
        rProperty.Cut(0, nsoffset+1);
    }
    else {
        nameSpace.Truncate(); // XXX isn't it empty already?
    }

    nsresult rv;
    nsIXMLDocument* xmlDoc;
    if (NS_FAILED(rv = mDocument->QueryInterface(kIXMLDocumentIID, (void**) &xmlDoc)))
        return rv;

    PRInt32 nameSpaceId = GetNameSpaceId(nameSpace);
    rv = xmlDoc->GetNameSpaceURI(nameSpaceId, rNameSpaceURI);
    NS_RELEASE(xmlDoc);

    return rv;
}


nsresult
nsRDFContentSink::GetIdAboutAttribute(const nsIParserNode& aNode,
                                      nsString& rResource)
{
    // This corresponds to the dirty work of production [6.5]
    nsAutoString k;
    nsAutoString ns, attr;
    PRInt32 ac = aNode.GetAttributeCount();

    for (PRInt32 i = 0; i < ac; i++) {
        // Get upper-cased key
        const nsString& key = aNode.GetKeyAt(i);
        if (NS_FAILED(SplitQualifiedName(key, ns, attr)))
            continue;

        if (! ns.Equals(kRDFNameSpaceURI))
            continue;

        // XXX you can't specify both, but we'll just pick up the
        // first thing that was specified and ignore the other.

        if (attr.Equals(kTagRDF_about)) {
            rResource = aNode.GetValueAt(i);
            rdf_StripAndConvert(rResource);

            return NS_OK;
        }

        if (attr.Equals(kTagRDF_ID)) {
            mDocumentURL->ToString(rResource);
            nsAutoString tag = aNode.GetValueAt(i);
            rdf_StripAndConvert(tag);

            if (rResource.Last() != '#' && tag.First() != '#')
                rResource.Append('#');

            rResource.Append(tag);
            return NS_OK;
        }

        // XXX we don't deal with aboutEach...
    }

    // Otherwise, we couldn't find anything, so just gensym one...
    mDocumentURL->ToString(rResource);
    rResource.Append("#anonymous$");
    rResource.Append(mGenSym++, 10);
    return NS_OK;
}


nsresult
nsRDFContentSink::GetResourceAttribute(const nsIParserNode& aNode,
                                       nsString& rResource)
{
    nsAutoString k;
    nsAutoString ns, attr;
    PRInt32 ac = aNode.GetAttributeCount();

    for (PRInt32 i = 0; i < ac; i++) {
        // Get upper-cased key
        const nsString& key = aNode.GetKeyAt(i);
        if (NS_FAILED(SplitQualifiedName(key, ns, attr)))
            continue;

        if (! ns.Equals(kRDFNameSpaceURI))
            continue;

        // XXX you can't specify both, but we'll just pick up the
        // first thing that was specified and ignore the other.

        if (attr.Equals(kTagRDF_resource)) {
            rResource = aNode.GetValueAt(i);
            rdf_StripAndConvert(rResource);
            rdf_FullyQualifyURI(mDocumentURL, rResource);
            return NS_OK;
        }
    }
    return NS_ERROR_FAILURE;
}

nsresult
nsRDFContentSink::AddProperties(const nsIParserNode& aNode,
                                nsIRDFNode* aSubject)
{
    // Add tag attributes to the content attributes
    nsAutoString k, v;
    nsAutoString ns, attr;
    PRInt32 ac = aNode.GetAttributeCount();

    for (PRInt32 i = 0; i < ac; i++) {
        // Get upper-cased key
        const nsString& key = aNode.GetKeyAt(i);
        if (NS_FAILED(SplitQualifiedName(key, ns, attr)))
            continue;

        // skip rdf:about, rdf:ID, and rdf:resource attributes; these
        // are all "special" and should've been dealt with by the
        // caller.
        if (ns.Equals(kRDFNameSpaceURI) &&
            (attr.Equals(kTagRDF_about) ||
             attr.Equals(kTagRDF_ID) ||
             attr.Equals(kTagRDF_resource)))
            continue;

        v = aNode.GetValueAt(i);
        rdf_StripAndConvert(v);

        k.Truncate();
        k.Append(ns);
        k.Append(attr);

        // Add the attribute to RDF
        Assert(aSubject, k, v);
    }
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// RDF-specific routines used to build the model

nsresult
nsRDFContentSink::OpenRDF(const nsIParserNode& aNode)
{
    // ensure that we're actually reading RDF by making sure that the
    // opening tag is <rdf:RDF>, where "rdf:" corresponds to whatever
    // they've declared the standard RDF namespace to be.
    nsAutoString ns, tag;
    
    if (NS_FAILED(SplitQualifiedName(aNode.GetText(), ns, tag)))
        return NS_ERROR_UNEXPECTED;

    if (! ns.Equals(kRDFNameSpaceURI))
        return NS_ERROR_UNEXPECTED;

    if (! tag.Equals(kTagRDF_RDF))
        return NS_ERROR_UNEXPECTED;

    PushContext(NULL, mState);
    mState = eRDFContentSinkState_InDocumentElement;
    return NS_OK;
}


nsresult
nsRDFContentSink::OpenObject(const nsIParserNode& aNode)
{
    // an "object" non-terminal is either a "description", a "typed
    // node", or a "container", so this change the content sink's
    // state appropriately.

    if (! mRDFResourceManager)
        return NS_ERROR_NOT_INITIALIZED;

    nsAutoString ns, tag;
    nsresult rv;
    if (NS_FAILED(rv = SplitQualifiedName(aNode.GetText(), ns, tag)))
        return rv;

    // Figure out the URI of this object, and create an RDF node for it.
    nsAutoString uri;
    if (NS_FAILED(rv = GetIdAboutAttribute(aNode, uri)))
        return rv;

    nsIRDFNode* rdfResource;
    if (NS_FAILED(rv = mRDFResourceManager->GetNode(uri, rdfResource)))
        return rv;

    // Arbitrarily make the document root be the first container
    // element in the RDF.
    if (! mRootElement) {
        nsIRDFContent* rdfElement;
        if (NS_FAILED(rv = NS_NewRDFElement(&rdfElement)))
            return rv;

        if (NS_FAILED(rv = rdfElement->SetDocument(mDocument, PR_FALSE))) {
            NS_RELEASE(rdfElement);
            return rv;
        }

        if (NS_FAILED(rv = rdfElement->SetResource(uri))) {
            NS_RELEASE(rdfElement);
            return rv;
        }

        mRootElement = static_cast<nsIContent*>(rdfElement);
        mDocument->SetRootContent(mRootElement);

        // don't release the rdfElement since we're keeping
        // a reference to it in mRootElement
    }

    // If we're in a member or property element, then this is the cue
    // that we need to hook the object up into the graph via the
    // member/property.
    switch (mState) {
    case eRDFContentSinkState_InMemberElement:
    case eRDFContentSinkState_InPropertyElement: {
        Assert(GetContextElement(1),
               GetContextElement(0),
               rdfResource);
    } break;

    default:
        break;
    }
    
    // Push the element onto the context stack
    PushContext(rdfResource, mState);

    // Now figure out what kind of state transition we need to
    // make. We'll either be going into a mode where we parse a
    // description or a container.
    PRBool isaTypedNode = PR_TRUE;

    if (ns.Equals(kRDFNameSpaceURI)) {
        isaTypedNode = PR_FALSE;

        if (tag.Equals(kTagRDF_Description)) {
            // it's a description
            mState = eRDFContentSinkState_InDescriptionElement;
        }
        else if (tag.Equals(kTagRDF_Bag)) {
            // it's a bag container
            Assert(rdfResource, kURIRDF_instanceOf, kURIRDF_Bag);
            mState = eRDFContentSinkState_InContainerElement;
        }
        else if (tag.Equals(kTagRDF_Seq)) {
            // it's a seq container
            Assert(rdfResource, kURIRDF_instanceOf, kURIRDF_Seq);
            mState = eRDFContentSinkState_InContainerElement;
        }
        else if (tag.Equals(kTagRDF_Alt)) {
            // it's an alt container
            Assert(rdfResource, kURIRDF_instanceOf, kURIRDF_Alt);
            mState = eRDFContentSinkState_InContainerElement;
        }
        else {
            // heh, that's not *in* the RDF namespace: just treat it
            // like a typed node
            isaTypedNode = PR_TRUE;
        }
    }
    if (isaTypedNode) {
        // XXX destructively alter "ns" to contain the fully qualified
        // tag name. We can do this 'cause we don't need it anymore...
        ns.Append(tag);
        Assert(rdfResource, kURIRDF_instanceOf, ns);

        mState = eRDFContentSinkState_InDescriptionElement;
    }

    AddProperties(aNode, rdfResource);
    NS_RELEASE(rdfResource);

    return NS_OK;
}


nsresult
nsRDFContentSink::OpenProperty(const nsIParserNode& aNode)
{
    if (! mRDFResourceManager)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv;

    // an "object" non-terminal is either a "description", a "typed
    // node", or a "container", so this change the content sink's
    // state appropriately.
    nsAutoString ns, tag;

    if (NS_FAILED(rv = SplitQualifiedName(aNode.GetText(), ns, tag)))
        return rv;

    // destructively alter "ns" to contain the fully qualified tag
    // name. We can do this 'cause we don't need it anymore...
    ns.Append(tag);
    nsIRDFNode* rdfProperty;
    if (NS_FAILED(rv = mRDFResourceManager->GetNode(ns, rdfProperty)))
        return rv;

    nsAutoString resourceURI;
    if (NS_SUCCEEDED(GetResourceAttribute(aNode, resourceURI))) {
        // They specified an inline resource for the value of this
        // property. Create an RDF resource for the inline resource
        // URI, add the properties to it, and attach the inline
        // resource to its parent.
        nsIRDFNode* rdfResource;
        if (NS_SUCCEEDED(rv = mRDFResourceManager->GetNode(resourceURI, rdfResource))) {
            if (NS_SUCCEEDED(rv = AddProperties(aNode, rdfResource))) {
                rv = Assert(GetContextElement(0), rdfProperty, rdfResource);
            }
        }

        // XXX ignore any failure from above...
        PR_ASSERT(rv == NS_OK);

        // XXX Technically, we should _not_ fall through here and push
        // the element onto the stack: this is supposed to be a closed
        // node. But right now I'm lazy and the code will just Do The
        // Right Thing so long as the RDF is well-formed.
    }

    // Push the element onto the context stack and change state.
    PushContext(rdfProperty, mState);
    mState = eRDFContentSinkState_InPropertyElement;

    rdfProperty->Release();
    return NS_OK;
}


nsresult
nsRDFContentSink::OpenMember(const nsIParserNode& aNode)
{
    // ensure that we're actually reading a member element by making
    // sure that the opening tag is <rdf:li>, where "rdf:" corresponds
    // to whatever they've declared the standard RDF namespace to be.
    nsAutoString ns, tag;

    if (NS_FAILED(SplitQualifiedName(aNode.GetText(), ns, tag)))
        return NS_ERROR_UNEXPECTED;

    if (! ns.Equals(kRDFNameSpaceURI))
        return NS_ERROR_UNEXPECTED;

    if (! tag.Equals(kTagRDF_li))
        return NS_ERROR_UNEXPECTED;

    // The parent element is the container.
    nsIRDFNode* contextResource = GetContextElement(0);
    if (! contextResource)
        return NS_ERROR_NULL_POINTER;

    // XXX err...figure out where we're at
    PRUint32 count = 1;

    nsAutoString ordinalURI(kRDFNameSpaceURI);
    ordinalURI.Append('_');
    ordinalURI.Append(count + 1, 10);

    nsresult rv;
    nsIRDFNode* ordinalResource;
    if (NS_FAILED(rv = mRDFResourceManager->GetNode(ordinalURI, ordinalResource)))
        return rv;

    nsAutoString resourceURI;
    if (NS_SUCCEEDED(GetResourceAttribute(aNode, resourceURI))) {
        // Okay, this node has an RDF:resource="..." attribute. That
        // means that it's a "referenced item," as covered in [6.29].
        rv = Assert(contextResource, ordinalResource, resourceURI);

        // XXX Technically, we should _not_ fall through here and push
        // the element onto the stack: this is supposed to be a closed
        // node. But right now I'm lazy and the code will just Do The
        // Right Thing so long as the RDF is well-formed.
    }

    // Push it on to the content stack and change state.
    PushContext(ordinalResource, mState);
    mState = eRDFContentSinkState_InMemberElement;

    ordinalResource->Release();
    return rv;
}


nsresult
nsRDFContentSink::OpenValue(const nsIParserNode& aNode)
{
    // a "value" can either be an object or a string: we'll only get
    // *here* if it's an object, as raw text is added as a leaf.
    return OpenObject(aNode);
}

////////////////////////////////////////////////////////////////////////
// RDF Helper Methods

nsresult
nsRDFContentSink::Assert(nsIRDFNode* subject, nsIRDFNode* predicate, nsIRDFNode* object)
{
    if (!mDataSource)
        return NS_ERROR_NOT_INITIALIZED;

#ifdef DEBUG_waterson
    char buf[256];
    nsAutoString s;

    printf("assert [\n");

    subject->GetStringValue(s);
    printf("  %s\n", s.ToCString(buf, sizeof(buf)));

    predicate->GetStringValue(s);
    printf("  %s\n", s.ToCString(buf, sizeof(buf)));

    object->GetStringValue(s);
    printf("  %s\n", s.ToCString(buf, sizeof(buf)));

    printf("]\n");
#endif

    return mDataSource->Assert(subject, predicate, object);
}


nsresult
nsRDFContentSink::Assert(nsIRDFNode* subject, nsIRDFNode* predicate, const nsString& objectLiteral)
{
    if (!mRDFResourceManager)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv;
    nsIRDFNode* object;
    if (NS_FAILED(rv = mRDFResourceManager->GetNode(objectLiteral, object)))
        return rv;

    rv = Assert(subject, predicate, object);
    NS_RELEASE(object);

    return rv;
}

nsresult
nsRDFContentSink::Assert(nsIRDFNode* subject, const nsString& predicateURI, const nsString& objectLiteral)
{
    if (!mRDFResourceManager)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv;
    nsIRDFNode* predicate;
    if (NS_FAILED(rv = mRDFResourceManager->GetNode(predicateURI, predicate)))
        return rv;

    rv = Assert(subject, predicate, objectLiteral);
    NS_RELEASE(predicate);

    return rv;
}


////////////////////////////////////////////////////////////////////////
// Content stack management

struct RDFContextStackElement {
    nsIRDFNode*         mResource;
    RDFContentSinkState mState;
};

nsIRDFNode* 
nsRDFContentSink::GetContextElement(PRInt32 ancestor /* = 0 */)
{
    if ((nsnull == mContextStack) ||
        (ancestor >= mNestLevel)) {
        return nsnull;
    }

    RDFContextStackElement* e =
        static_cast<RDFContextStackElement*>(mContextStack->ElementAt(mNestLevel-ancestor-1));

    return e->mResource;
}

PRInt32 
nsRDFContentSink::PushContext(nsIRDFNode *aResource, RDFContentSinkState aState)
{
    if (! mContextStack) {
        mContextStack = new nsVoidArray();
        if (! mContextStack)
            return 0;
    }

    RDFContextStackElement* e = new RDFContextStackElement;
    if (! e)
        return mNestLevel;

    NS_IF_ADDREF(aResource);
    e->mResource = aResource;
    e->mState    = aState;
  
    mContextStack->AppendElement(static_cast<void*>(e));
    return ++mNestLevel;
}
 
nsresult
nsRDFContentSink::PopContext(nsIRDFNode*& rResource, RDFContentSinkState& rState)
{
    RDFContextStackElement* e;
    if ((nsnull == mContextStack) ||
        (0 == mNestLevel)) {
        return NS_ERROR_NULL_POINTER;
    }
  
    --mNestLevel;
    e = static_cast<RDFContextStackElement*>(mContextStack->ElementAt(mNestLevel));
    mContextStack->RemoveElementAt(mNestLevel);

    // don't bother Release()-ing: call it our implicit AddRef().
    rResource = e->mResource;
    rState    = e->mState;

    delete e;
    return NS_OK;
}
 
PRInt32 
nsRDFContentSink::GetCurrentNestLevel()
{
    return mNestLevel;
}


////////////////////////////////////////////////////////////////////////
// Namespace management

void
nsRDFContentSink::FindNameSpaceAttributes(const nsIParserNode& aNode)
{
    nsAutoString k, uri, prefix;
    PRInt32 ac = aNode.GetAttributeCount();
    PRInt32 offset;
    nsresult result = NS_OK;

    for (PRInt32 i = 0; i < ac; i++) {
        const nsString& key = aNode.GetKeyAt(i);
        k.Truncate();
        k.Append(key);
        // Look for "xmlns" at the start of the attribute name
        offset = k.Find(kNameSpaceDef);
        if (0 == offset) {
            prefix.Truncate();

            PRUnichar next = k.CharAt(sizeof(kNameSpaceDef)-1);
            // If the next character is a :, there is a namespace prefix
            if (':' == next) {
                k.Right(prefix, k.Length()-sizeof(kNameSpaceDef));
            }

            // Get the attribute value (the URI for the namespace)
            uri = aNode.GetValueAt(i);
            rdf_StripAndConvert(uri);
      
            // Open a local namespace
            OpenNameSpace(prefix, uri);
        }
    }
}

PRInt32 
nsRDFContentSink::OpenNameSpace(const nsString& aPrefix, const nsString& aURI)
{
    nsIAtom *nameSpaceAtom = nsnull;
    PRInt32 id = gNameSpaceId_Unknown;

    nsIXMLDocument *xmlDoc;
    nsresult result = mDocument->QueryInterface(kIXMLDocumentIID, 
                                                (void **)&xmlDoc);
    if (NS_OK != result)
        return id;

    if (0 < aPrefix.Length())
        nameSpaceAtom = NS_NewAtom(aPrefix);
  
    result = xmlDoc->RegisterNameSpace(nameSpaceAtom, aURI, id);
    if (NS_OK == result) {
        NameSpaceStruct *ns;
    
        ns = new NameSpaceStruct;
        if (nsnull != ns) {
            ns->mPrefix = nameSpaceAtom;
            NS_IF_ADDREF(nameSpaceAtom);
            ns->mId = id;
            ns->mNestLevel = GetCurrentNestLevel();
      
            if (nsnull == mNameSpaces)
                mNameSpaces = new nsVoidArray();

            // XXX Should check for duplication
            mNameSpaces->AppendElement((void *)ns);
        }
    }

    NS_IF_RELEASE(nameSpaceAtom);
    NS_RELEASE(xmlDoc);

    return id;
}

PRInt32 
nsRDFContentSink::GetNameSpaceId(const nsString& aPrefix)
{
    nsIAtom *nameSpaceAtom = nsnull;
    PRInt32 id = gNameSpaceId_Unknown;
    PRInt32 i, count;
  
    if (nsnull == mNameSpaces)
        return id;

    if (0 < aPrefix.Length())
        nameSpaceAtom = NS_NewAtom(aPrefix);

    count = mNameSpaces->Count();
    for (i = 0; i < count; i++) {
        NameSpaceStruct *ns = (NameSpaceStruct *)mNameSpaces->ElementAt(i);
    
        if ((nsnull != ns) && (ns->mPrefix == nameSpaceAtom)) {
            id = ns->mId;
            break;
        }
    }

    NS_IF_RELEASE(nameSpaceAtom);
    return id;
}

void    
nsRDFContentSink::CloseNameSpacesAtNestLevel(PRInt32 mNestLevel)
{
    PRInt32 nestLevel = GetCurrentNestLevel();

    if (nsnull == mNameSpaces) {
        return;
    }

    PRInt32 i, count;
    count = mNameSpaces->Count();
    // Go backwards so that we can delete as we go along
    for (i = count; i >= 0; i--) {
        NameSpaceStruct *ns = (NameSpaceStruct *)mNameSpaces->ElementAt(i);
    
        if ((nsnull != ns) && (ns->mNestLevel == nestLevel)) {
            NS_IF_RELEASE(ns->mPrefix);
            mNameSpaces->RemoveElementAt(i);
            delete ns;
        }
    }
}
