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

  For more information on RDF, see http://www.w3.org/TR/WDf-rdf-syntax.
  For more information on XUL, see http://www.mozilla.org/xpfe

  This code is based on the working draft "last call", which was
  published on Oct 8, 1998.

  Open Issues ------------------

  1) factoring code with nsXMLContentSink - There's some amount of
     common code between this and the HTML content sink. This will
     increase as we support more and more HTML elements. How can code
     from XML/HTML be factored?

  TO DO ------------------------

  1) Make sure all shortcut syntax works right.

  2) Implement default namespaces. Take a look at how nsIXMLDocument
     does namespaces: should we storing tag/namespace pairs instead of
     the entire URI in the elements?

*/

#include "nsCOMPtr.h"
#include "nsIContentSink.h"
#include "nsINameSpace.h"
#include "nsINameSpaceManager.h"
#include "nsIRDFContentSink.h"
#include "nsIRDFNode.h"
#include "nsIRDFService.h"
#include "nsIRDFXMLDataSource.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsIXMLContentSink.h"
#include "nsLayoutCID.h"
#include "nsRDFCID.h"
#include "nsVoidArray.h"
#include "prlog.h"
#include "prmem.h"
#include "rdfutil.h"

#include "nsHTMLTokens.h" // XXX so we can use nsIParserNode::GetTokenType()

static const char kNameSpaceSeparator[] = ":";
static const char kNameSpaceDef[] = "xmlns";
static const char kXULID[] = "id";

////////////////////////////////////////////////////////////////////////
// RDF core vocabulary

#include "rdf.h"
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, child); // Used to indicate the containment relationship.
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, type); 

////////////////////////////////////////////////////////////////////////
// XPCOM IIDs

static NS_DEFINE_IID(kIContentSinkIID,         NS_ICONTENT_SINK_IID); // XXX grr...
static NS_DEFINE_IID(kINameSpaceManagerIID,    NS_INAMESPACEMANAGER_IID);
static NS_DEFINE_IID(kIRDFContentSinkIID,      NS_IRDFCONTENTSINK_IID);
static NS_DEFINE_IID(kIRDFDataSourceIID,       NS_IRDFDATASOURCE_IID);
static NS_DEFINE_IID(kIRDFServiceIID,          NS_IRDFSERVICE_IID);
static NS_DEFINE_IID(kISupportsIID,            NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIXMLContentSinkIID,      NS_IXMLCONTENT_SINK_IID);

static NS_DEFINE_CID(kNameSpaceManagerCID,      NS_NAMESPACEMANAGER_CID);
static NS_DEFINE_CID(kRDFServiceCID,            NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID, NS_RDFINMEMORYDATASOURCE_CID);

////////////////////////////////////////////////////////////////////////
// Utility routines

// XXX This totally sucks. I wish that mozilla/base had this code.
static PRUnichar
rdf_EntityToUnicode(const char* buf)
{
    if ((buf[0] == 'g' || buf[0] == 'G') &&
        (buf[1] == 't' || buf[1] == 'T'))
        return PRUnichar('>');

    if ((buf[0] == 'l' || buf[0] == 'L') &&
        (buf[1] == 't' || buf[1] == 'T'))
        return PRUnichar('<');

    if ((buf[0] == 'a' || buf[0] == 'A') &&
        (buf[1] == 'm' || buf[1] == 'M') &&
        (buf[2] == 'p' || buf[2] == 'P'))
        return PRUnichar('&');

    NS_NOTYETIMPLEMENTED("this is a named entity that I can't handle...");
    return PRUnichar('?');
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
                PRInt32 ch;

                // XXX Um, here's where we should be converting a
                // named entity. I removed this to avoid a link-time
                // dependency on core raptor.
                ch = rdf_EntityToUnicode(cbuf);

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
                NS_NOTYETIMPLEMENTED("convert a script entity");
            }
        }
    }
}

static nsresult
rdf_GetQuotedAttributeValue(const nsString& aSource, 
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


static void
rdf_FullyQualifyURI(const nsIURL* base, nsString& spec)
{
    // This is a fairly heavy-handed way to do this, but...I don't
    // like typing.
    nsIURL* url;
    if (NS_SUCCEEDED(NS_NewURL(&url, spec, base))) {
        PRUnichar* str;
        url->ToString(&str);
        spec = str;
        delete str;
        url->Release();
    }
}

////////////////////////////////////////////////////////////////////////

typedef enum {
    eXULContentSinkState_InProlog,
    eXULContentSinkState_InDocumentElement,
    eXULContentSinkState_InEpilog
} RDFContentSinkState;


class XULContentSinkImpl : public nsIRDFContentSink
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
    NS_IMETHOD NotifyError(nsresult aErrorResult);

    // nsIXMLContentSink
    NS_IMETHOD AddXMLDecl(const nsIParserNode& aNode);
    NS_IMETHOD AddDocTypeDecl(const nsIParserNode& aNode);
    NS_IMETHOD AddCharacterData(const nsIParserNode& aNode);
    NS_IMETHOD AddUnparsedEntity(const nsIParserNode& aNode);
    NS_IMETHOD AddNotation(const nsIParserNode& aNode);
    NS_IMETHOD AddEntityReference(const nsIParserNode& aNode);

    // nsIRDFContentSink
    NS_IMETHOD Init(nsIURL* aURL, nsINameSpaceManager* aNameSpaceManager);
    NS_IMETHOD SetDataSource(nsIRDFXMLDataSource* ds);
    NS_IMETHOD GetDataSource(nsIRDFXMLDataSource*& ds);

protected:
    static nsrefcnt             gRefCnt;
    static nsIRDFService*       gRDFService;

    // pseudo-constants
    static nsIRDFResource* kRDF_child; // XXX needs to be NC:child (or something else)
    static nsIRDFResource* kRDF_type;

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
    void      PopNameSpaces();

    nsINameSpaceManager* mNameSpaceManager;
    nsVoidArray* mNameSpaceStack;
    
    void SplitQualifiedName(const nsString& aQualifiedName,
                            PRInt32& rNameSpaceID,
                            nsString& rProperty);

    // RDF-specific parsing
    nsresult GetXULIDAttribute(const nsIParserNode& aNode, nsIRDFResource** aResource);
    nsresult AddAttributes(const nsIParserNode& aNode, nsIRDFResource* aSubject);

    virtual nsresult OpenTag(const nsIParserNode& aNode);
    
    // Miscellaneous RDF junk
    nsIRDFXMLDataSource*   mDataSource;
    RDFContentSinkState    mState;

    // content stack management
    PRInt32         PushContext(nsIRDFResource *aContext, RDFContentSinkState aState);
    nsresult        PopContext(nsIRDFResource*& rContext, RDFContentSinkState& rState);
    nsIRDFResource* GetContextElement(PRInt32 ancestor = 0);

    nsVoidArray* mContextStack;

    nsIURL*      mDocumentURL;

    PRBool       mHaveSetRootResource;
};

nsrefcnt             XULContentSinkImpl::gRefCnt = 0;
nsIRDFService*       XULContentSinkImpl::gRDFService = nsnull;

nsIRDFResource*      XULContentSinkImpl::kRDF_child  = nsnull;
nsIRDFResource*      XULContentSinkImpl::kRDF_type   = nsnull;

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
      mHaveSetRootResource(PR_FALSE)
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

        NS_VERIFY(NS_SUCCEEDED(rv = gRDFService->GetResource(kURIRDF_type,  &kRDF_type)),
                  "unalbe to get resource");
    }
}


XULContentSinkImpl::~XULContentSinkImpl()
{
    NS_IF_RELEASE(mNameSpaceManager);
    NS_IF_RELEASE(mDocumentURL);
    NS_IF_RELEASE(mDataSource);

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
            RDFContentSinkState state;
            PopContext(resource, state);
            NS_IF_RELEASE(resource);
        }

        delete mContextStack;
    }
    PR_FREEIF(mText);

    if (--gRefCnt == 0) {
        nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
        NS_IF_RELEASE(kRDF_child);
        NS_IF_RELEASE(kRDF_type);
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
    if (iid.Equals(kIRDFContentSinkIID) ||
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
    NS_VERIFY(NS_SUCCEEDED(rv), "unable to get namespace URI");

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
    return (mDataSource != nsnull) ? mDataSource->BeginLoad() : NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::DidBuildModel(PRInt32 aQualityLevel)
{
    return (mDataSource != nsnull) ? mDataSource->EndLoad() : NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::WillInterrupt(void)
{
    return (mDataSource != nsnull) ? mDataSource->Interrupt() : NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::WillResume(void)
{
    return (mDataSource != nsnull) ? mDataSource->Resume() : NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::SetParser(nsIParser* aParser)
{
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

    FlushText();

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

	NS_ASSERTION(NS_SUCCEEDED(rv), "unexpected content");
    return rv;
}

NS_IMETHODIMP 
XULContentSinkImpl::CloseContainer(const nsIParserNode& aNode)
{
#ifdef DEBUG
    const nsString& text = aNode.GetText();
#endif

    FlushText();

    nsIRDFResource* resource;
    if (NS_FAILED(PopContext(resource, mState))) {
        // XXX parser didn't catch unmatched tags?
        PR_ASSERT(0);
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
XULContentSinkImpl::NotifyError(nsresult aErrorResult)
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
        if (NS_FAILED(rv = rdf_GetQuotedAttributeValue(text, "href", href)))
            return rv;

        // If there was an error or there's no href, we can't do
        // anything with this PI
        if (! href.Length())
            return NS_OK;
    
        nsAutoString type;
        if (NS_FAILED(rv = rdf_GetQuotedAttributeValue(text, "type", type)))
            return rv;
    
        if (! type.Equals(kCSSType))
            return NS_OK;

        nsIURL* url = nsnull;
        nsAutoString absURL;
        nsAutoString emptyURL;
        emptyURL.Truncate();
        if (NS_FAILED(rv = NS_MakeAbsoluteURL(mDocumentURL, emptyURL, href, absURL)))
            return rv;

        if (NS_FAILED(rv = NS_NewURL(&url, absURL)))
            return rv;

        rv = mDataSource->AddCSSStyleSheetURL(url);
        NS_RELEASE(url);
    }

    return rv;
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
        text.Append(rdf_EntityToUnicode(buf));
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

static PRInt32 kNameSpaceID_XUL;

NS_IMETHODIMP
XULContentSinkImpl::Init(nsIURL* aURL, nsINameSpaceManager* aNameSpaceManager)
{
    NS_PRECONDITION((nsnull != aURL) && (nsnull != aNameSpaceManager), "null ptr");
    if ((! aURL) || (! aNameSpaceManager))
        return NS_ERROR_NULL_POINTER;

    mDocumentURL = aURL;
    NS_ADDREF(aURL);

    mNameSpaceManager = aNameSpaceManager;
    NS_ADDREF(aNameSpaceManager);

	nsresult rv;

// XXX This is sure to change. Copied from mozilla/layout/xul/content/src/nsXULAtoms.cpp
static const char kXULNameSpaceURI[]
    = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

    rv = mNameSpaceManager->RegisterNameSpace(kXULNameSpaceURI, kNameSpaceID_XUL);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to register XUL namespace");
    
    if (NS_FAILED(rv = nsServiceManager::GetService(kRDFServiceCID,
                                                    kIRDFServiceIID,
                                                    (nsISupports**) &gRDFService)))
        return rv;

    mState = eXULContentSinkState_InProlog;
    return NS_OK;
}

NS_IMETHODIMP
XULContentSinkImpl::SetDataSource(nsIRDFXMLDataSource* ds)
{
    NS_IF_RELEASE(mDataSource);
    mDataSource = ds;
    NS_IF_ADDREF(mDataSource);
    return NS_OK;
}


NS_IMETHODIMP
XULContentSinkImpl::GetDataSource(nsIRDFXMLDataSource*& ds)
{
    ds = mDataSource;
    NS_IF_ADDREF(mDataSource);
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
    if (! aCreateTextNode)
        return NS_OK;

    // Don't bother if there's nothing but whitespace.
    // XXX This could cause problems...
    if (! rdf_IsDataInBuffer(mText, mTextLength))
        return NS_OK;

    // Don't bother if we're not in XUL document body
    if (mState != eXULContentSinkState_InDocumentElement)
        return NS_OK;

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

    if (NS_FAILED(rv = mDataSource->Assert(GetContextElement(0), kRDF_child, literal, PR_TRUE))) {
        NS_ERROR("unable to make assertion");
        return rv;
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
        SplitQualifiedName(key, nameSpaceID, attr);

		// Look for XUL:ID
        if (nameSpaceID != kNameSpaceID_XUL)
            continue;

        if (attr.EqualsIgnoreCase(kXULID)) {
            nsAutoString uri = aNode.GetValueAt(i);
            rdf_StripAndConvert(uri);

            // XXX Take the URI and make it fully qualified by
            // sticking it into the document's URL. This may not be
            // appropriate...
            rdf_FullyQualifyURI(mDocumentURL, uri);

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
        SplitQualifiedName(key, nameSpaceID, attr);

        if (nameSpaceID == kNameSpaceID_HTML)
            attr.ToUpperCase();

        // Don't add xmlns: declarations, these are really just
        // processing instructions.
        if (nameSpaceID == kNameSpaceID_XMLNS)
            continue;

        // skip xul:id (and potentially others in the future). These
        // are all "special" and should've been dealt with by the
        // caller.
        if ((nameSpaceID == kNameSpaceID_XUL) &&
            (attr.EqualsIgnoreCase(kXULID)))
            continue;

        v = aNode.GetValueAt(i);
        rdf_StripAndConvert(v);

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

    // HTML tags all need to be upper-cased
    if (nameSpaceID == kNameSpaceID_HTML)
        tag.ToUpperCase();

    // Figure out the URI of this object, and create an RDF node for it.
    nsresult rv;

    nsCOMPtr<nsIRDFResource> rdfResource;
    if (NS_FAILED(rv = GetXULIDAttribute(aNode, getter_AddRefs(rdfResource))))
        return rv;

    // Convert the container's namespace/tag pair to a fully qualified
    // URI so that we can specify it as an RDF resource.
    nsCOMPtr<nsIRDFResource> tagResource;
    if (NS_FAILED(rv = MakeResourceFromQualifiedTag(nameSpaceID, tag, getter_AddRefs(tagResource)))) {
        NS_ERROR("unable to construct resource from namespace/tag pair");
        return rv;
    }

    if (NS_FAILED(rv = mDataSource->Assert(rdfResource, kRDF_type, tagResource, PR_TRUE))) {
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
        if (NS_FAILED(rv = mDataSource->SetRootResource(rdfResource))) {
            NS_ERROR("couldn't set root resource");
            return rv;
        }
    }
    else {
        // We now have an RDF node for the container.  Hook it up to
        // its parent container with a "child" relationship.
		if (NS_FAILED(rv = mDataSource->Assert(GetContextElement(0),
                                               kRDF_child,
                                               rdfResource,
                                               PR_TRUE))) {
            NS_ERROR("unable to assert child");
            return rv;
        }
    }

    // Push the element onto the context stack, so that child
    // containers will hook up to us as their parent.
    PushContext(rdfResource, mState);
    mState = eXULContentSinkState_InDocumentElement;
    return NS_OK;
}




////////////////////////////////////////////////////////////////////////
// Content stack management

struct RDFContextStackElement {
    nsIRDFResource*     mResource;
    RDFContentSinkState mState;
};

nsIRDFResource* 
XULContentSinkImpl::GetContextElement(PRInt32 ancestor /* = 0 */)
{
    if ((nsnull == mContextStack) ||
        (ancestor >= mContextStack->Count())) {
        return nsnull;
    }

    RDFContextStackElement* e =
        NS_STATIC_CAST(RDFContextStackElement*, mContextStack->ElementAt(mContextStack->Count()-ancestor-1));

    return e->mResource;
}

PRInt32 
XULContentSinkImpl::PushContext(nsIRDFResource *aResource, RDFContentSinkState aState)
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
XULContentSinkImpl::PopContext(nsIRDFResource*& rResource, RDFContentSinkState& rState)
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

                PRUnichar next = k.CharAt(sizeof(kNameSpaceDef)-1);
                // If the next character is a :, there is a namespace prefix
                if (':' == next) {
                    k.Right(prefix, k.Length()-sizeof(kNameSpaceDef));
                }

                // Get the attribute value (the URI for the namespace)
                uri = aNode.GetValueAt(i);
                rdf_StripAndConvert(uri);

                // Open a local namespace
                nsIAtom* prefixAtom = ((0 < prefix.Length()) ? NS_NewAtom(prefix) : nsnull);
                nsINameSpace* child = nsnull;
                nameSpace->CreateChildNameSpace(prefixAtom, uri, child);
                if (nsnull != child) {
                    NS_RELEASE(nameSpace);
                    nameSpace = child;
                }

                // Add it to the set of namespaces used in the RDF/XML document.
                mDataSource->AddNameSpace(prefixAtom, uri);
      
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
XULContentSinkImpl::PopNameSpaces()
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
NS_NewXULContentSink(nsIRDFContentSink** aResult)
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
