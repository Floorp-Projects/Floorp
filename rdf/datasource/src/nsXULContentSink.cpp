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

  An implementation for an NGLayout-style content sink that knows how
  to build an RDF content model from XUL.

  For each container tag, an RDF Sequence object is created that
  contains the order set of children of that container.

  For more information on XUL, see http://www.mozilla.org/xpfe

*/

#include "nsCOMPtr.h"
#include "nsIContent.h"
#include "nsIContentSink.h"
#include "nsICSSParser.h"
#include "nsICSSStyleSheet.h"
#include "nsIDOMNode.h"
#include "nsIDocument.h"
#include "nsINameSpace.h"
#include "nsINameSpaceManager.h"
#include "nsIParser.h"
#include "nsIPresShell.h"
#include "nsIRDFDocument.h"
#include "nsIRDFNode.h"
#include "nsIRDFService.h"
#include "nsIRDFXMLDataSource.h"
#include "nsIScriptContext.h"
#include "nsIScriptContextOwner.h"
#include "nsIServiceManager.h"
#include "nsIUnicharStreamLoader.h"
#include "nsIURL.h"
#include "nsIURLGroup.h"
#include "nsIViewManager.h"
#include "nsIXULContentSink.h"
#include "nsLayoutCID.h"
#include "nsRDFCID.h"
#include "nsRDFParserUtils.h"
#include "nsVoidArray.h"
#include "prlog.h"
#include "prmem.h"
#include "rdfutil.h"
#include "nsIXULChildDocument.h"

#include "nsHTMLTokens.h" // XXX so we can use nsIParserNode::GetTokenType()

static const char kNameSpaceSeparator[] = ":";
static const char kNameSpaceDef[] = "xmlns";
static const char kXULID[] = "id";

////////////////////////////////////////////////////////////////////////
// RDF core vocabulary

#include "rdf.h"
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, child); // Used to indicate the containment relationship.
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, instanceOf);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, type); 


// XXX This is sure to change. Copied from mozilla/layout/xul/content/src/nsXULAtoms.cpp
#define XUL_NAMESPACE_URI "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"
static const char kXULNameSpaceURI[] = XUL_NAMESPACE_URI;

#define XUL_NAMESPACE_URI_PREFIX XUL_NAMESPACE_URI "#"
DEFINE_RDF_VOCAB(XUL_NAMESPACE_URI_PREFIX, XUL, element);

////////////////////////////////////////////////////////////////////////
// XPCOM IIDs

static NS_DEFINE_IID(kIContentSinkIID,         NS_ICONTENT_SINK_IID); // XXX grr...
static NS_DEFINE_IID(kINameSpaceManagerIID,    NS_INAMESPACEMANAGER_IID);
static NS_DEFINE_IID(kIRDFDataSourceIID,       NS_IRDFDATASOURCE_IID);
static NS_DEFINE_IID(kIRDFServiceIID,          NS_IRDFSERVICE_IID);
static NS_DEFINE_IID(kISupportsIID,            NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIXMLContentSinkIID,      NS_IXMLCONTENT_SINK_IID);
static NS_DEFINE_IID(kIXULContentSinkIID,      NS_IXULCONTENTSINK_IID);

static NS_DEFINE_CID(kCSSParserCID,             NS_CSSPARSER_CID);
static NS_DEFINE_CID(kNameSpaceManagerCID,      NS_NAMESPACEMANAGER_CID);
static NS_DEFINE_CID(kRDFServiceCID,            NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID, NS_RDFINMEMORYDATASOURCE_CID);

////////////////////////////////////////////////////////////////////////
// Utility routines

////////////////////////////////////////////////////////////////////////

typedef enum {
    eXULContentSinkState_InProlog,
    eXULContentSinkState_InDocumentElement,
    eXULContentSinkState_InScript,
    eXULContentSinkState_InEpilog
} XULContentSinkState;


class XULContentSinkImpl : public nsIXULContentSink
{
public:
    XULContentSinkImpl();
    virtual ~XULContentSinkImpl();

    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIContentSink
    NS_IMETHOD WillBuildModel(void);
    NS_IMETHOD DidBuildModel(PRInt32 aQualityLevel);
    NS_IMETHOD WillInterrupt(void);
    NS_IMETHOD WillResume(void);
    NS_IMETHOD SetParser(nsIParser* aParser);  
    NS_IMETHOD OpenContainer(const nsIParserNode& aNode);
    NS_IMETHOD CloseContainer(const nsIParserNode& aNode);
    NS_IMETHOD AddLeaf(const nsIParserNode& aNode);
    NS_IMETHOD AddComment(const nsIParserNode& aNode);
    NS_IMETHOD AddProcessingInstruction(const nsIParserNode& aNode);
    NS_IMETHOD NotifyError(const nsParserError* aError);

    // nsIXMLContentSink
    NS_IMETHOD AddXMLDecl(const nsIParserNode& aNode);
    NS_IMETHOD AddDocTypeDecl(const nsIParserNode& aNode);
    NS_IMETHOD AddCharacterData(const nsIParserNode& aNode);
    NS_IMETHOD AddUnparsedEntity(const nsIParserNode& aNode);
    NS_IMETHOD AddNotation(const nsIParserNode& aNode);
    NS_IMETHOD AddEntityReference(const nsIParserNode& aNode);

    // nsIXULContentSink
    NS_IMETHOD Init(nsIDocument* aDocument, nsIRDFDataSource* aDataSource);

protected:
    static nsrefcnt             gRefCnt;
    static nsIRDFService*       gRDFService;

    // pseudo-constants
    PRInt32 kNameSpaceID_XUL; // XXX per-instance member variable

    static nsIRDFResource* kRDF_child; // XXX needs to be NC:child (or something else)
    static nsIRDFResource* kRDF_instanceOf;
    static nsIRDFResource* kRDF_type;
    static nsIRDFResource* kXUL_element;

    nsresult
    MakeResourceFromQualifiedTag(PRInt32 aNameSpaceID,
                                 const nsString& aTag,
                                 nsIRDFResource** aResource);

    // Text management
    nsresult FlushText(PRBool aCreateTextNode=PR_TRUE,
                       PRBool* aDidFlush=nsnull);

    PRUnichar* mText;
    PRInt32 mTextLength;
    PRInt32 mTextSize;
    PRBool mConstrainSize;

    // namespace management
    void      PushNameSpacesFrom(const nsIParserNode& aNode);
    nsIAtom*  CutNameSpacePrefix(nsString& aString);
    PRInt32   GetNameSpaceID(nsIAtom* aPrefix);
    void      PopNameSpaces(void);

    nsINameSpaceManager* mNameSpaceManager;
    nsVoidArray* mNameSpaceStack;
    
    void SplitQualifiedName(const nsString& aQualifiedName,
                            PRInt32& rNameSpaceID,
                            nsString& rProperty);

    // RDF-specific parsing
    nsresult GetXULIDAttribute(const nsIParserNode& aNode, nsIRDFResource** aResource);
    nsresult AddAttributes(const nsIParserNode& aNode, nsIRDFResource* aSubject);

    nsresult OpenTag(const nsIParserNode& aNode);

    // Script tag handling
    nsresult OpenScript(const nsIParserNode& aNode);

    static void DoneLoadingScript(nsIUnicharStreamLoader* aLoader,
                                  nsString& aData,
                                  void* aRef,
                                  nsresult aStatus);

    nsresult EvaluateScript(nsString& aScript, PRUint32 aLineNo);

    nsresult CloseScript(const nsIParserNode& aNode);

    PRBool mInScript;
    PRUint32 mScriptLineNo;


    // Style sheets
    nsresult
    LoadStyleSheet(nsIURL* aURL,
                   nsIUnicharInputStream* aUIN,
                   PRBool aActive,
                   const nsString& aTitle,
                   const nsString& aMedia,
                   nsIContent* aOwner); 

    static void
    DoneLoadingStyle(nsIUnicharStreamLoader* aLoader,
                     nsString& aData,
                     void* aRef,
                     nsresult aStatus);
    
    // Miscellaneous RDF junk
    nsIRDFDataSource*      mDataSource;
    XULContentSinkState    mState;

    // content stack management
    PRInt32         PushResourceAndState(nsIRDFResource *aContext, XULContentSinkState aState);
    nsresult        PopResourceAndState(nsIRDFResource*& rContext, XULContentSinkState& rState);
    nsIRDFResource* GetTopResource(void);

    nsVoidArray* mContextStack;

    nsIURL*      mDocumentURL;

    PRBool       mHaveSetRootResource;

    nsIDocument* mDocument;
    nsIParser*   mParser;
    
    nsIRDFResource*  mFragmentRoot;

};

nsrefcnt             XULContentSinkImpl::gRefCnt = 0;
nsIRDFService*       XULContentSinkImpl::gRDFService = nsnull;

nsIRDFResource*      XULContentSinkImpl::kRDF_child;
nsIRDFResource*      XULContentSinkImpl::kRDF_instanceOf;
nsIRDFResource*      XULContentSinkImpl::kRDF_type;
nsIRDFResource*      XULContentSinkImpl::kXUL_element;

////////////////////////////////////////////////////////////////////////

XULContentSinkImpl::XULContentSinkImpl()
    : mDocumentURL(nsnull),
      mDataSource(nsnull),
      mNameSpaceStack(nsnull),
      mContextStack(nsnull),
      mText(nsnull),
      mTextLength(0),
      mTextSize(0),
      mConstrainSize(PR_TRUE),
      mHaveSetRootResource(PR_FALSE),
      mDocument(nsnull),
      mParser(nsnull),
      mFragmentRoot(nsnull)
{
    NS_INIT_REFCNT();

    if (gRefCnt++ == 0) {
        nsresult rv;

        rv = nsServiceManager::GetService(kRDFServiceCID,
                                          kIRDFServiceIID,
                                          (nsISupports**) &gRDFService);

        NS_VERIFY(NS_SUCCEEDED(rv), "unable to get RDF service");

        NS_VERIFY(NS_SUCCEEDED(rv = gRDFService->GetResource(kURIRDF_child, &kRDF_child)),
                  "unalbe to get resource");

        NS_VERIFY(NS_SUCCEEDED(rv = gRDFService->GetResource(kURIRDF_instanceOf, &kRDF_instanceOf)),
                  "unalbe to get resource");

        NS_VERIFY(NS_SUCCEEDED(rv = gRDFService->GetResource(kURIRDF_type, &kRDF_type)),
                  "unalbe to get resource");

        NS_VERIFY(NS_SUCCEEDED(rv = gRDFService->GetResource(kURIXUL_element, &kXUL_element)),
                  "unalbe to get resource");
    }
}


XULContentSinkImpl::~XULContentSinkImpl()
{
    NS_IF_RELEASE(mNameSpaceManager);
    NS_IF_RELEASE(mDocumentURL);
    NS_IF_RELEASE(mDataSource);
    NS_IF_RELEASE(mDocument);
    NS_IF_RELEASE(mParser);
    NS_IF_RELEASE(mFragmentRoot);

    if (mNameSpaceStack) {
        NS_PRECONDITION(0 == mNameSpaceStack->Count(), "namespace stack not empty");

        // There shouldn't be any here except in an error condition
        PRInt32 index = mNameSpaceStack->Count();

        while (0 < index--) {
            nsINameSpace* nameSpace = (nsINameSpace*)mNameSpaceStack->ElementAt(index);
            NS_RELEASE(nameSpace);
        }
        delete mNameSpaceStack;
    }
    if (mContextStack) {
        NS_PRECONDITION(0 == mContextStack->Count(), "content stack not empty");

        // XXX we should never need to do this, but, we'll write the
        // code all the same. If someone left the content stack dirty,
        // pop all the elements off the stack and release them.
        PRInt32 index = mContextStack->Count();
        while (0 < index--) {
            nsIRDFResource* resource;
            XULContentSinkState state;
            PopResourceAndState(resource, state);
            NS_IF_RELEASE(resource);
        }

        delete mContextStack;
    }
    PR_FREEIF(mText);

    if (--gRefCnt == 0) {
        nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
        NS_IF_RELEASE(kRDF_child);
        NS_IF_RELEASE(kRDF_instanceOf);
        NS_IF_RELEASE(kRDF_type);
        NS_IF_RELEASE(kXUL_element);
    }
}

////////////////////////////////////////////////////////////////////////
// nsISupports interface

NS_IMPL_ADDREF(XULContentSinkImpl);
NS_IMPL_RELEASE(XULContentSinkImpl);

NS_IMETHODIMP
XULContentSinkImpl::QueryInterface(REFNSIID iid, void** result)
{
    NS_PRECONDITION(result, "null ptr");
    if (! result)
        return NS_ERROR_NULL_POINTER;

    *result = nsnull;
    if (iid.Equals(kIXULContentSinkIID) ||
        iid.Equals(kIXMLContentSinkIID) ||
        iid.Equals(kIContentSinkIID) ||
        iid.Equals(kISupportsIID)) {
        *result = NS_STATIC_CAST(nsIXMLContentSink*, this);
        AddRef();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}

////////////////////////////////////////////////////////////////////////

nsresult
XULContentSinkImpl::MakeResourceFromQualifiedTag(PRInt32 aNameSpaceID,
                                                 const nsString& aTag,
                                                 nsIRDFResource** aResource)
{
    nsresult rv;
    nsAutoString uri;

    rv = mNameSpaceManager->GetNameSpaceURI(aNameSpaceID, uri);
    //NS_VERIFY(NS_SUCCEEDED(rv), "unable to get namespace URI");

    // some hacky logic to try to construct an appopriate URI for the
    // namespace/tag pair.
    if ((uri.Last() != '#' || uri.Last() != '/') &&
        (aTag.First() != '#')) {
        uri.Append('#');
    }

    uri.Append(aTag);

    rv = gRDFService->GetUnicodeResource(uri, aResource);
    NS_VERIFY(NS_SUCCEEDED(rv), "unable to get resource");

    return rv;
}


////////////////////////////////////////////////////////////////////////
// nsIContentSink interface

NS_IMETHODIMP 
XULContentSinkImpl::WillBuildModel(void)
{
    mDocument->BeginLoad();
    return NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::DidBuildModel(PRInt32 aQualityLevel)
{
    // XXX this is silly; who cares?
    PRInt32 i, ns = mDocument->GetNumberOfShells();
    for (i = 0; i < ns; i++) {
        nsIPresShell* shell = mDocument->GetShellAt(i);
        if (nsnull != shell) {
            nsIViewManager* vm;
            shell->GetViewManager(&vm);
            if(vm) {
                vm->SetQuality(nsContentQuality(aQualityLevel));
            }
            NS_RELEASE(vm);
            NS_RELEASE(shell);
        }
    }

    mDocument->EndLoad();

    // Drop our reference to the parser to get rid of a circular
    // reference.
    NS_IF_RELEASE(mParser);
    return NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::WillInterrupt(void)
{
    // XXX Notify the webshell, if necessary
    return NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::WillResume(void)
{
    // XXX Notify the webshell, if necessary
    return NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::SetParser(nsIParser* aParser)
{
    NS_IF_RELEASE(mParser);
    mParser = aParser;
    NS_IF_ADDREF(aParser);
    return NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::OpenContainer(const nsIParserNode& aNode)
{
    // XXX Hopefully the parser will flag this before we get here. If
    // we're in the epilog, there should be no new elements
    NS_PRECONDITION(mState != eXULContentSinkState_InEpilog, "tag in XUL doc epilog");

#ifdef DEBUG
    const nsString& text = aNode.GetText();
#endif

    if (mState != eXULContentSinkState_InScript) {
        FlushText();
    }

    // We must register namespace declarations found in the attribute
    // list of an element before creating the element. This is because
    // the namespace prefix for an element might be declared within
    // the attribute list.
    PushNameSpacesFrom(aNode);

    nsresult rv;

    switch (mState) {
    case eXULContentSinkState_InProlog:
    case eXULContentSinkState_InDocumentElement:
        rv = OpenTag(aNode);
        break;

    case eXULContentSinkState_InEpilog:
        PR_ASSERT(0);
        rv = NS_ERROR_UNEXPECTED; // XXX
        break;
    }

	//NS_ASSERTION(NS_SUCCEEDED(rv), "unexpected content");
    return rv;
}

NS_IMETHODIMP 
XULContentSinkImpl::CloseContainer(const nsIParserNode& aNode)
{
    nsresult rv;

#ifdef DEBUG
    const nsString& text = aNode.GetText();
#endif

    if (mState == eXULContentSinkState_InScript) {
        if (NS_FAILED(rv = CloseScript(aNode)))
            return rv;
    }
    else {
        FlushText();
    }

    nsIRDFResource* resource;
    if (NS_FAILED(PopResourceAndState(resource, mState))) {
        NS_ERROR("parser didn't catch unmatched tags?");
        return NS_ERROR_UNEXPECTED; // XXX
    }

    PRInt32 nestLevel = mContextStack->Count();
    if (nestLevel == 0)
        mState = eXULContentSinkState_InEpilog;

    PopNameSpaces();
      
    NS_IF_RELEASE(resource);
    return NS_OK;
}


NS_IMETHODIMP 
XULContentSinkImpl::AddLeaf(const nsIParserNode& aNode)
{
    // XXX For now, all leaf content is character data
    AddCharacterData(aNode);
    return NS_OK;
}

NS_IMETHODIMP
XULContentSinkImpl::NotifyError(const nsParserError* aError)
{
    printf("XULContentSinkImpl::NotifyError\n");
    return NS_OK;
}

// nsIXMLContentSink
NS_IMETHODIMP 
XULContentSinkImpl::AddXMLDecl(const nsIParserNode& aNode)
{
    // XXX We'll ignore it for now
    printf("XULContentSinkImpl::AddXMLDecl\n");
    return NS_OK;
}


NS_IMETHODIMP 
XULContentSinkImpl::AddComment(const nsIParserNode& aNode)
{
    FlushText();
    nsAutoString text;
    nsresult result = NS_OK;

    text = aNode.GetText();

    // XXX add comment here...

    return result;
}

nsresult
XULContentSinkImpl::LoadStyleSheet(nsIURL* aURL,
                                   nsIUnicharInputStream* aUIN,
                                   PRBool aActive,
                                   const nsString& aTitle,
                                   const nsString& aMedia,
                                   nsIContent* aOwner)
{
    nsresult rv;
    nsCOMPtr<nsICSSParser> parser;
    if (NS_FAILED(rv = nsComponentManager::CreateInstance(kCSSParserCID,
                                                    nsnull,
                                                    nsICSSParser::GetIID(),
                                                    (void**) getter_AddRefs(parser)))) {
        NS_ERROR("unable to create CSS parser");
        return rv;
    }

    nsCOMPtr<nsICSSStyleSheet> sheet;

    // XXX note: we are ignoring rv until the error code stuff in the
    // input routines is converted to use nsresult's
    parser->SetCaseSensitive(PR_TRUE);
    parser->Parse(aUIN, aURL, *getter_AddRefs(sheet));

    if (! sheet)
        return NS_ERROR_OUT_OF_MEMORY; // XXX

    sheet->SetTitle(aTitle);
    sheet->SetEnabled(aActive);
    mDocument->AddStyleSheet(sheet);

    if (nsnull != aOwner) {
        nsIDOMNode* domNode;
        if (NS_SUCCEEDED(aOwner->QueryInterface(nsIDOMNode::GetIID(), (void**)&domNode))) {
            sheet->SetOwningNode(domNode);
            NS_RELEASE(domNode);
        }
    }

    return NS_OK;
}

struct AsyncStyleProcessingData {
    nsString            mTitle;
    nsString            mMedia;
    PRBool              mIsActive;
    nsIURL*             mURL;
    nsIContent*         mElement;
    XULContentSinkImpl* mSink;
};

void
XULContentSinkImpl::DoneLoadingStyle(nsIUnicharStreamLoader* aLoader,
                                     nsString& aData,
                                     void* aRef,
                                     nsresult aStatus)
{
    nsresult rv = NS_OK;
    AsyncStyleProcessingData* d = (AsyncStyleProcessingData*)aRef;
    nsIUnicharInputStream* uin = nsnull;

    if ((NS_OK == aStatus) && (0 < aData.Length())) {
        // wrap the string with the CSS data up in a unicode
        // input stream.
        rv = NS_NewStringUnicharInputStream(&uin, new nsString(aData));
        if (NS_OK == rv) {
            // XXX We have no way of indicating failure. Silently fail?
            rv = d->mSink->LoadStyleSheet(d->mURL, uin, d->mIsActive, 
                                          d->mTitle, d->mMedia, d->mElement);
            NS_RELEASE(uin);
        }
    }
    
    if (d->mSink->mParser) {
        d->mSink->mParser->EnableParser(PR_TRUE);
    }

    NS_RELEASE(d->mURL);
    NS_IF_RELEASE(d->mElement);
    NS_RELEASE(d->mSink);
    delete d;

    // We added a reference when the loader was created. This
    // release should destroy it.
    NS_RELEASE(aLoader);
}


NS_IMETHODIMP 
XULContentSinkImpl::AddProcessingInstruction(const nsIParserNode& aNode)
{

    static const char kStyleSheetPI[] = "<?xml-stylesheet";
    static const char kCSSType[] = "text/css";

    nsresult rv;
    FlushText();

    if (! mDataSource)
        return NS_OK;

    // XXX For now, we don't add the PI to the content model.
    // We just check for a style sheet PI
    const nsString& text = aNode.GetText();

    // If it's a stylesheet PI...
    if (0 == text.Find(kStyleSheetPI)) {
        nsAutoString href;
        if (NS_FAILED(rv = nsRDFParserUtils::GetQuotedAttributeValue(text, "href", href)))
            return rv;

        // If there was an error or there's no href, we can't do
        // anything with this PI
        if (0 == href.Length())
            return NS_OK;

        nsAutoString type;
        if (NS_FAILED(rv = nsRDFParserUtils::GetQuotedAttributeValue(text, "type", type)))
            return rv;

        nsAutoString title;
        if (NS_FAILED(rv = nsRDFParserUtils::GetQuotedAttributeValue(text, "title", title)))
            return rv;

        title.CompressWhitespace();

        nsAutoString media;
        if (NS_FAILED(rv =  nsRDFParserUtils::GetQuotedAttributeValue(text, "media", media)))
            return rv;

        media.ToUpperCase();

        if (type.Equals(kCSSType)) {
            // Use the SRC attribute value to load the URL
            nsCOMPtr<nsIURL> url;
            nsAutoString absURL;

            nsCOMPtr<nsIURLGroup> urlGroup; 
            if (NS_SUCCEEDED(rv = mDocumentURL->GetURLGroup(getter_AddRefs(urlGroup)))) {
                if (NS_FAILED(rv = urlGroup->CreateURL(getter_AddRefs(url),
                                                       mDocumentURL,
                                                       href,
                                                       nsnull))) {
                    NS_ERROR("unable to create URL for style sheet");
                    return rv;
                }
            }
            else {
                if (NS_FAILED(rv = NS_NewURL(getter_AddRefs(url), absURL))) {
                    NS_ERROR("unable to create URL for style sheet");
                    return rv;
                }
            }

            AsyncStyleProcessingData* d = new AsyncStyleProcessingData;
            if (! d)
                return NS_ERROR_OUT_OF_MEMORY;

            d->mTitle.SetString(title);
            d->mMedia.SetString(media);
            d->mIsActive = PR_TRUE;
            d->mURL      = url;
            NS_ADDREF(d->mURL);
            // XXX Need to create PI node
            d->mElement  = nsnull;
            d->mSink     = this;
            NS_ADDREF(this);

            nsIUnicharStreamLoader* loader;
            if (NS_FAILED(rv = NS_NewUnicharStreamLoader(&loader,
                                                         url, 
                                                         (nsStreamCompleteFunc)DoneLoadingStyle, 
                                                         (void *)d))) {
                return rv;
            }

            return NS_ERROR_HTMLPARSER_BLOCK;
        }
    }

    return NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::AddDocTypeDecl(const nsIParserNode& aNode)
{
    printf("XULContentSinkImpl::AddDocTypeDecl\n");
    return NS_OK;
}


NS_IMETHODIMP 
XULContentSinkImpl::AddCharacterData(const nsIParserNode& aNode)
{
    nsAutoString text = aNode.GetText();

    if (aNode.GetTokenType() == eToken_entity) {
        char buf[12];
        text.ToCString(buf, sizeof(buf));
        text.Truncate();
        text.Append(nsRDFParserUtils::EntityToUnicode(buf));
    }

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
XULContentSinkImpl::AddUnparsedEntity(const nsIParserNode& aNode)
{
    printf("XULContentSinkImpl::AddUnparsedEntity\n");
    return NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::AddNotation(const nsIParserNode& aNode)
{
    printf("XULContentSinkImpl::AddNotation\n");
    return NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::AddEntityReference(const nsIParserNode& aNode)
{
    printf("XULContentSinkImpl::AddEntityReference\n");
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsIRDFContentSink interface

NS_IMETHODIMP
XULContentSinkImpl::Init(nsIDocument* aDocument, nsIRDFDataSource* aDataSource)
{
    nsresult rv;

    NS_PRECONDITION(aDocument != nsnull, "null ptr");
    if (! aDocument)
        return NS_ERROR_NULL_POINTER;
    
    nsCOMPtr<nsIXULChildDocument> childDocument;
    childDocument = do_QueryInterface(aDocument);
    childDocument->GetFragmentRoot(&mFragmentRoot);
    if (mFragmentRoot) {
        // We're totally a subdocument. Find the root document's
        // data source and make assertions there.
      
        // First of all, find the root document.
        nsIDocument* rootDocument = nsnull; 
        nsIDocument* currDocument; 
        currDocument = aDocument; 
        NS_ADDREF(currDocument); 
        while (currDocument != nsnull) { 
            NS_IF_RELEASE(rootDocument); 
            rootDocument = currDocument; 
            currDocument = rootDocument->GetParentDocument(); 
        } 

        // Retrieve the root data source. 
        nsCOMPtr<nsIRDFDocument> rdfRootDoc; 
        rdfRootDoc = do_QueryInterface(rootDocument); 
        if (rdfRootDoc == nsnull) { 
            NS_ERROR("Root document of a XUL fragment is not an RDF doc."); 
            NS_RELEASE(rootDocument); 
            return NS_ERROR_INVALID_ARG; 
        } 

        nsCOMPtr<nsIRDFDataSource> docDataSource; 
        if (NS_FAILED(rv = rdfRootDoc->GetDocumentDataSource(getter_AddRefs(docDataSource)))) { 
            NS_ERROR("Unable to retrieve an RDF document's data source."); 
            NS_RELEASE(rootDocument); 
            return rv; 
        } 

        NS_IF_RELEASE(mDataSource);
        mDataSource = docDataSource.get();
        NS_ADDREF(mDataSource);
    }
    else {

        NS_PRECONDITION(aDataSource != nsnull, "null ptr");
        if (! aDataSource)
            return NS_ERROR_NULL_POINTER;

        NS_IF_RELEASE(mDataSource);
        mDataSource = aDataSource;
        NS_ADDREF(aDataSource);
    }
    
    
    NS_IF_RELEASE(mDocument);
    mDocument = aDocument;
    NS_ADDREF(mDocument);

    NS_IF_RELEASE(mDocumentURL);
    mDocumentURL = mDocument->GetDocumentURL();

    if (NS_FAILED(rv = mDocument->GetNameSpaceManager(mNameSpaceManager))) {
        NS_ERROR("unable to get document namespace manager");
        return rv;
    }

    if (mFragmentRoot == nsnull) {
        // XUL Namespace isn't registered if we're a root document.
        // We need to register it.
        rv = mNameSpaceManager->RegisterNameSpace(kXULNameSpaceURI, kNameSpaceID_XUL);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to register XUL namespace");
    }

    mState = eXULContentSinkState_InProlog;

    return NS_OK;
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
XULContentSinkImpl::FlushText(PRBool aCreateTextNode, PRBool* aDidFlush)
{
    if (aDidFlush)
        *aDidFlush = PR_FALSE;

    // Don't do anything if there's no text to create a node from.
    if (! mTextLength)
        return NS_OK;

    // Don't do anything if they've told us not to create a text node
    if (aCreateTextNode) {
      
      // Don't bother if there's nothing but whitespace.
      // XXX This could cause problems...
      if (rdf_IsDataInBuffer(mText, mTextLength)) {

        // Don't bother if we're not in XUL document body
        if (mState == eXULContentSinkState_InDocumentElement) {
          // Trim the leading and trailing whitespace, create an RDF
          // literal, and put it into the graph.
          nsAutoString value;
          value.Append(mText, mTextLength);
          value.Trim(" \t\n\r");

          nsresult rv;
          nsCOMPtr<nsIRDFLiteral> literal;
          if (NS_FAILED(rv = gRDFService->GetLiteral(value, getter_AddRefs(literal)))) {
              NS_ERROR("unable to create RDF literal");
              return rv;
          }

          if (NS_FAILED(rv = rdf_ContainerAppendElement(mDataSource, GetTopResource(), literal))) {
              NS_ERROR("unable to add text to container");
              return rv;
          }
        }
      }
    }

    // Reset our text buffer
    mTextLength = 0;

    if (aDidFlush)
        *aDidFlush = PR_TRUE;

    return NS_OK;
}



////////////////////////////////////////////////////////////////////////
// Qualified name resolution

void
XULContentSinkImpl::SplitQualifiedName(const nsString& aQualifiedName,
                                       PRInt32& rNameSpaceID,
                                       nsString& rProperty)
{
    rProperty = aQualifiedName;
    nsIAtom* prefix = CutNameSpacePrefix(rProperty);
    rNameSpaceID = GetNameSpaceID(prefix);
    NS_IF_RELEASE(prefix);
}


nsresult
XULContentSinkImpl::GetXULIDAttribute(const nsIParserNode& aNode,
                                      nsIRDFResource** aResource)
{
    nsAutoString k;
    nsAutoString attr;
    PRInt32 nameSpaceID;
    PRInt32 ac = aNode.GetAttributeCount();

	
    for (PRInt32 i = 0; i < ac; i++) {
        // Get upper-cased key
        const nsString& key = aNode.GetKeyAt(i);
        attr = key;
        nsIAtom* prefix = CutNameSpacePrefix(attr);
        if (prefix != nsnull)
        {
            SplitQualifiedName(key, nameSpaceID, attr);
            NS_RELEASE(prefix);
        }
        else
            nameSpaceID = kNameSpaceID_None; // Unqualified attributes have a namespace of none (always)

		// Look for XUL:ID
        if (nameSpaceID != kNameSpaceID_None)
            continue;

        if (attr == kXULID) {
            nsAutoString uri = aNode.GetValueAt(i);
            nsRDFParserUtils::StripAndConvert(uri);

            // XXX Take the URI and make it fully qualified by
            // sticking it into the document's URL. This may not be
            // appropriate...
            const char* documentURL;
            mDocumentURL->GetSpec(&documentURL);
            rdf_PossiblyMakeAbsolute(documentURL, uri);

            return gRDFService->GetUnicodeResource(uri, aResource);
        }
    }

    // Otherwise, we couldn't find anything, so just gensym one...
    const char* url;
    mDocumentURL->GetSpec(&url);
    return rdf_CreateAnonymousResource(url, aResource);
}

nsresult
XULContentSinkImpl::AddAttributes(const nsIParserNode& aNode,
                                  nsIRDFResource* aSubject)
{
    // Add tag attributes to the content attributes
    nsAutoString k, v;
    nsAutoString attr;
    PRInt32 nameSpaceID;
    PRInt32 ac = aNode.GetAttributeCount();

    for (PRInt32 i = 0; i < ac; i++) {
        // Get upper-cased key
        const nsString& key = aNode.GetKeyAt(i);

        attr = key;
        nsIAtom* prefix = CutNameSpacePrefix(attr);
        if (prefix != nsnull)
        {
            SplitQualifiedName(key, nameSpaceID, attr);
            NS_RELEASE(prefix);
        }
        else
            nameSpaceID = kNameSpaceID_None; // Unqualified attributes have a namespace of none (always)

        // Don't add xmlns: declarations, these are really just
        // processing instructions.
        if (nameSpaceID == kNameSpaceID_XMLNS)
            continue;

        // skip xul:id (and potentially others in the future). These
        // are all "special" and should've been dealt with by the
        // caller.
        if ((nameSpaceID == kNameSpaceID_None) &&
            (attr == kXULID))
            continue;

        v = aNode.GetValueAt(i);
        nsRDFParserUtils::StripAndConvert(v);

        //if (nameSpaceID == kNameSpaceID_HTML)
        //    attr.ToLowerCase(); // Not our problem. You'd better be lowercase.

        // Get the URI for the namespace, so we can construct a
        // fully-qualified property name.
        mNameSpaceManager->GetNameSpaceURI(nameSpaceID, k);

        // Insert a '#' if the namespace doesn't end with one, or the
        // attribute doesn't start with one.
        if (! ((attr.First() == '#') &&
               (k.Last() == '#' || k.Last() == '/'))) {
            k.Append('#');
        }
        k.Append(attr);

        // Add the attribute to RDF
        rdf_Assert(mDataSource, aSubject, k, v);
    }
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// RDF-specific routines used to build the graph representation of the XUL content model

nsresult
XULContentSinkImpl::OpenTag(const nsIParserNode& aNode)
{
    // an "object" non-terminal is a "container".  Change the content sink's
    // state appropriately.
    nsAutoString tag;
    PRInt32 nameSpaceID;

    SplitQualifiedName(aNode.GetText(), nameSpaceID, tag);

    // HTML tags must be lowercase
    if (nameSpaceID == kNameSpaceID_HTML) {
        if (tag.Equals("script")) {
            return OpenScript(aNode);
        }
    }

    // Figure out the URI of this object, and create an RDF node for it.
    nsresult rv;

    nsCOMPtr<nsIRDFResource> rdfResource;
    if (NS_FAILED(rv = GetXULIDAttribute(aNode, getter_AddRefs(rdfResource)))) {
        NS_ERROR("unable to parser XUL ID from tag");
        return rv;
    }

    if (NS_FAILED(rv = rdf_MakeSeq(mDataSource, rdfResource))) {
        NS_ERROR("unable to create sequence for tag");
        return rv;
    }

    if (NS_FAILED(rv = rdf_Assert(mDataSource, rdfResource, kRDF_instanceOf, kXUL_element))) {
        NS_ERROR("unable to mark resource as XUL element");
        return rv;
    }

    // Convert the container's namespace/tag pair to a fully qualified
    // URI so that we can specify it as an RDF resource.
    nsCOMPtr<nsIRDFResource> tagResource;
    if (NS_FAILED(rv = MakeResourceFromQualifiedTag(nameSpaceID, tag, getter_AddRefs(tagResource)))) {
        NS_ERROR("unable to construct resource from namespace/tag pair");
        return rv;
    }

    if (NS_FAILED(rv = rdf_Assert(mDataSource, rdfResource, kRDF_type, tagResource))) {
        NS_ERROR("unable to assert tag type");
        return rv;
    }

	// Make arcs from us to all of our attribute values (with the attribute names
	// as the labels of the arcs).
    if (NS_FAILED(rv = AddAttributes(aNode, rdfResource))) {
        NS_ERROR("problem adding properties to node");
        return rv;
    }

    if (!mHaveSetRootResource) {
        // This code sets the root (happens if we're the first open
        // container).
        mHaveSetRootResource = PR_TRUE;

        if (mFragmentRoot) {
            // We're a subdocument.  We need to take this fragment
            // node (which is the root of the fragment) and completely
            // discard it.  The fragment root's resource is actually what
            // should become the root of this subtree.
            rdfResource = dont_QueryInterface(mFragmentRoot);
        }
        else {
            nsCOMPtr<nsIRDFDocument> rdfDoc;
            if (NS_SUCCEEDED(mDocument->QueryInterface(nsIRDFDocument::GetIID(),
                                                       (void**) getter_AddRefs(rdfDoc)))) {
                if (NS_FAILED(rv = rdfDoc->SetRootResource(rdfResource))) {
                    NS_ERROR("couldn't set root resource");
                    return rv;
                }
            }
        }
    }
    else {
        // We now have an RDF node for the container.  Hook it up to
        // its parent container with a "child" relationship.
        if (NS_FAILED(rv = rdf_ContainerAppendElement(mDataSource,
                                                   GetTopResource(),
                                                   rdfResource))) {
            NS_ERROR("unable to add child to container");
            return rv;
        }
    }

    // Push the element onto the context stack, so that child
    // containers will hook up to us as their parent.
    PushResourceAndState(rdfResource, mState);
    mState = eXULContentSinkState_InDocumentElement;
    return NS_OK;
}



nsresult
XULContentSinkImpl::OpenScript(const nsIParserNode& aNode)
{
    nsresult rv = NS_OK;
    PRBool   isJavaScript = PR_TRUE;
    PRInt32 i, ac = aNode.GetAttributeCount();

    // Look for SRC attribute and look for a LANGUAGE attribute
    nsAutoString src;
    for (i = 0; i < ac; i++) {
        const nsString& key = aNode.GetKeyAt(i);
        if (key.EqualsIgnoreCase("src")) {
            src = aNode.GetValueAt(i);
            nsRDFParserUtils::StripAndConvert(src);
        }
        else if (key.EqualsIgnoreCase("type")) {
            nsAutoString  type(aNode.GetValueAt(i));
            nsRDFParserUtils::StripAndConvert(type);
            isJavaScript = type.EqualsIgnoreCase("text/javascript");
        }
        else if (key.EqualsIgnoreCase("language")) {
            nsAutoString  lang(aNode.GetValueAt(i));
            nsRDFParserUtils::StripAndConvert(lang);
            isJavaScript = nsRDFParserUtils::IsJavaScriptLanguage(lang);
        }
    }
  
    // Don't process scripts that aren't JavaScript
    if (isJavaScript) {
        nsAutoString script;

        // If there is a SRC attribute...
        if (src.Length() > 0) {
            // Use the SRC attribute value to load the URL
            nsIURL* url = nsnull;
            nsAutoString absURL;
            nsIURLGroup* urlGroup;

            rv = mDocumentURL->GetURLGroup(&urlGroup);
      
            if ((NS_OK == rv) && urlGroup) {
                rv = urlGroup->CreateURL(&url, mDocumentURL, src, nsnull);
                NS_RELEASE(urlGroup);
            }
            else {
                rv = NS_NewURL(&url, absURL);
            }
            if (NS_OK != rv) {
                return rv;
            }

            // Add a reference to this since the url loader is holding
            // onto it as opaque data.
            NS_ADDREF(this);

            nsIUnicharStreamLoader* loader;
            rv = NS_NewUnicharStreamLoader(&loader,
                                           url, 
                                           (nsStreamCompleteFunc)DoneLoadingScript, 
                                           (void *)this);
            NS_RELEASE(url);
            if (NS_OK == rv) {
                rv = NS_ERROR_HTMLPARSER_BLOCK;
            }
        }

        mInScript = PR_TRUE;
        mConstrainSize = PR_FALSE;
        mScriptLineNo = (PRUint32)aNode.GetSourceLineNumber();

        PushResourceAndState(nsnull, mState);
        mState = eXULContentSinkState_InScript;
    }

    return rv;
}

void
XULContentSinkImpl::DoneLoadingScript(nsIUnicharStreamLoader* aLoader,
                                      nsString& aData,
                                      void* aRef,
                                      nsresult aStatus)
{
    XULContentSinkImpl* sink = (XULContentSinkImpl*)aRef;

    if (NS_OK == aStatus) {
        // XXX We have no way of indicating failure. Silently fail?
        sink->EvaluateScript(aData, 0);
    }
    else {
        NS_ERROR("error loading script");
    }

    if (sink->mParser) {
        sink->mParser->EnableParser(PR_TRUE);
    }

    // The url loader held a reference to the sink
    NS_RELEASE(sink);

    // We added a reference when the loader was created. This
    // release should destroy it.
    NS_RELEASE(aLoader);
}


nsresult
XULContentSinkImpl::EvaluateScript(nsString& aScript, PRUint32 aLineNo)
{
    nsresult rv = NS_OK;

    if (0 < aScript.Length()) {
        nsIScriptContextOwner *owner;
        nsIScriptContext *context;
        owner = mDocument->GetScriptContextOwner();
        if (nsnull != owner) {
      
            rv = owner->GetScriptContext(&context);
            if (rv != NS_OK) {
                NS_RELEASE(owner);
                return rv;
            }
        
            nsIURL* docURL = mDocument->GetDocumentURL();
            const char* url;
            if (docURL) {
                (void)docURL->GetSpec(&url);
            }

            nsAutoString val;
            PRBool isUndefined;

            PRBool result = context->EvaluateString(aScript, url, aLineNo, 
                                                    val, &isUndefined);
      
            NS_IF_RELEASE(docURL);
      
            NS_RELEASE(context);
            NS_RELEASE(owner);
        }
    }

    return NS_OK;
}

nsresult
XULContentSinkImpl::CloseScript(const nsIParserNode& aNode)
{
    nsresult result = NS_OK;
    if (mInScript) {
        nsAutoString script;
        script.SetString(mText, mTextLength);
        result = EvaluateScript(script, mScriptLineNo);
        FlushText(PR_FALSE);
        mInScript = PR_FALSE;
    }

    return result;
}


////////////////////////////////////////////////////////////////////////
// Content stack management

struct RDFContextStackElement {
    nsIRDFResource*     mResource;
    XULContentSinkState mState;
};

nsIRDFResource* 
XULContentSinkImpl::GetTopResource(void)
{
    if ((nsnull == mContextStack) ||
        (0 >= mContextStack->Count())) {
        NS_ERROR("no context element");
        return nsnull;
    }

    RDFContextStackElement* e =
        NS_STATIC_CAST(RDFContextStackElement*,
                       mContextStack->ElementAt(mContextStack->Count() - 1));

    return e->mResource;
}

PRInt32 
XULContentSinkImpl::PushResourceAndState(nsIRDFResource* aResource,
                                         XULContentSinkState aState)
{
    if (! mContextStack) {
        mContextStack = new nsVoidArray();
        if (! mContextStack)
            return 0;
    }

    RDFContextStackElement* e = new RDFContextStackElement;
    if (! e)
        return mContextStack->Count();

    NS_IF_ADDREF(aResource);
    e->mResource = aResource;
    e->mState    = aState;
  
    mContextStack->AppendElement(NS_STATIC_CAST(void*, e));
    return mContextStack->Count();
}
 
nsresult
XULContentSinkImpl::PopResourceAndState(nsIRDFResource*& rResource,
                                        XULContentSinkState& rState)
{
    RDFContextStackElement* e;
    if ((nsnull == mContextStack) ||
        (0 == mContextStack->Count())) {
        return NS_ERROR_NULL_POINTER;
    }

    PRInt32 index = mContextStack->Count() - 1;
    e = NS_STATIC_CAST(RDFContextStackElement*, mContextStack->ElementAt(index));
    mContextStack->RemoveElementAt(index);

    // don't bother Release()-ing: call it our implicit AddRef().
    rResource = e->mResource;
    rState    = e->mState;

    delete e;
    return NS_OK;
}
 

////////////////////////////////////////////////////////////////////////
// Namespace management

void
XULContentSinkImpl::PushNameSpacesFrom(const nsIParserNode& aNode)
{
    nsAutoString k, uri, prefix;
    PRInt32 ac = aNode.GetAttributeCount();
    PRInt32 offset;
    nsresult result = NS_OK;
    nsINameSpace* nameSpace = nsnull;

    if ((nsnull != mNameSpaceStack) && (0 < mNameSpaceStack->Count())) {
        nameSpace = (nsINameSpace*)mNameSpaceStack->ElementAt(mNameSpaceStack->Count() - 1);
        NS_ADDREF(nameSpace);
    }
    else {
        mNameSpaceManager->CreateRootNameSpace(nameSpace);
    }

    if (nsnull != nameSpace) {
        for (PRInt32 i = 0; i < ac; i++) {
            const nsString& key = aNode.GetKeyAt(i);
            k.Truncate();
            k.Append(key);
            // Look for "xmlns" at the start of the attribute name
            offset = k.Find(kNameSpaceDef);
            if (0 == offset) {
                prefix.Truncate();

                if (k.Length() >= sizeof(kNameSpaceDef)) {
                    // If the next character is a :, there is a namespace prefix
                    PRUnichar next = k.CharAt(sizeof(kNameSpaceDef)-1);
                    if (':' == next) {
                        k.Right(prefix, k.Length()-sizeof(kNameSpaceDef));
                    }
                    else {
                        continue; // it's not "xmlns:"
                    }
                }

                // Get the attribute value (the URI for the namespace)
                uri = aNode.GetValueAt(i);
                nsRDFParserUtils::StripAndConvert(uri);

                // Open a local namespace
                nsIAtom* prefixAtom = ((0 < prefix.Length()) ? NS_NewAtom(prefix) : nsnull);
                nsINameSpace* child = nsnull;
                nameSpace->CreateChildNameSpace(prefixAtom, uri, child);
                if (nsnull != child) {
                    NS_RELEASE(nameSpace);
                    nameSpace = child;
                }

                // XXX I don't think that this is important for XUL,
                // since we won't be streaming it back out.
                // 
                // Add it to the set of namespaces used in the RDF/XML
                // document.
                //mDataSource->AddNameSpace(prefixAtom, uri);
      
                NS_IF_RELEASE(prefixAtom);
            }
        }

        // Now push the *last* namespace that we discovered on to the stack.
        if (nsnull == mNameSpaceStack) {
            mNameSpaceStack = new nsVoidArray();
        }
        mNameSpaceStack->AppendElement(nameSpace);
    }
}

nsIAtom* 
XULContentSinkImpl::CutNameSpacePrefix(nsString& aString)
{
    nsAutoString  prefix;
    PRInt32 nsoffset = aString.Find(kNameSpaceSeparator);
    if (-1 != nsoffset) {
        aString.Left(prefix, nsoffset);
        aString.Cut(0, nsoffset+1);
    }
    if (0 < prefix.Length()) {
        return NS_NewAtom(prefix);
    }
    return nsnull;
}

PRInt32 
XULContentSinkImpl::GetNameSpaceID(nsIAtom* aPrefix)
{
    PRInt32 id = kNameSpaceID_Unknown;
  
    if ((nsnull != mNameSpaceStack) && (0 < mNameSpaceStack->Count())) {
        PRInt32 index = mNameSpaceStack->Count() - 1;
        nsINameSpace* nameSpace = (nsINameSpace*)mNameSpaceStack->ElementAt(index);
        nameSpace->FindNameSpaceID(aPrefix, id);
    }

    return id;
}

void
XULContentSinkImpl::PopNameSpaces(void)
{
    if ((nsnull != mNameSpaceStack) && (0 < mNameSpaceStack->Count())) {
        PRInt32 index = mNameSpaceStack->Count() - 1;
        nsINameSpace* nameSpace = (nsINameSpace*)mNameSpaceStack->ElementAt(index);
        mNameSpaceStack->RemoveElementAt(index);

        // Releasing the most deeply nested namespace will recursively
        // release intermediate parent namespaces until the next
        // reference is held on the namespace stack.
        NS_RELEASE(nameSpace);
    }
}

////////////////////////////////////////////////////////////////////////

nsresult
NS_NewXULContentSink(nsIXULContentSink** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    XULContentSinkImpl* sink = new XULContentSinkImpl();
    if (! sink)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(sink);
    *aResult = sink;
    return NS_OK;
}
