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
  to build an RDF content model from XML-serialized RDF.

  For more information on RDF, see http://www.w3.org/TR/WDf-rdf-syntax.

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
#include "nsRDFCID.h"
#include "nsRDFParserUtils.h"
#include "nsVoidArray.h"
#include "nsXPIDLString.h"
#include "prlog.h"
#include "prmem.h"
#include "rdfutil.h"

#include "nsHTMLTokens.h" // XXX so we can use nsIParserNode::GetTokenType()

static const char kNameSpaceSeparator[] = ":";
static const char kNameSpaceDef[] = "xmlns";

////////////////////////////////////////////////////////////////////////
// RDF core vocabulary

#include "rdf.h"
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
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, type);

static const char kRDFNameSpaceURI[] = RDF_NAMESPACE_URI;

////////////////////////////////////////////////////////////////////////
// XPCOM IIDs

static NS_DEFINE_IID(kIContentSinkIID,         NS_ICONTENT_SINK_IID); // XXX grr...
static NS_DEFINE_IID(kIRDFDataSourceIID,       NS_IRDFDATASOURCE_IID);
static NS_DEFINE_IID(kIRDFServiceIID,          NS_IRDFSERVICE_IID);
static NS_DEFINE_IID(kISupportsIID,            NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIXMLContentSinkIID,      NS_IXMLCONTENT_SINK_IID);
static NS_DEFINE_IID(kIRDFContentSinkIID,      NS_IRDFCONTENTSINK_IID);

static NS_DEFINE_CID(kRDFServiceCID,            NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID, NS_RDFINMEMORYDATASOURCE_CID);

////////////////////////////////////////////////////////////////////////

#ifdef PR_LOGGING
static PRLogModuleInfo* gLog;
#endif

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


///////////////////////////////////////////////////////////////////////

typedef enum {
    eRDFContentSinkState_InProlog,
    eRDFContentSinkState_InDocumentElement,
    eRDFContentSinkState_InDescriptionElement,
    eRDFContentSinkState_InContainerElement,
    eRDFContentSinkState_InPropertyElement,
    eRDFContentSinkState_InMemberElement,
    eRDFContentSinkState_InEpilog
} RDFContentSinkState;


class RDFContentSinkImpl : public nsIRDFContentSink
{
public:
    RDFContentSinkImpl();
    virtual ~RDFContentSinkImpl();

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

    // nsIRDFContentSink
    NS_IMETHOD Init(nsIURL* aURL, nsINameSpaceManager* aNameSpaceManager);
    NS_IMETHOD SetDataSource(nsIRDFXMLDataSource* ds);
    NS_IMETHOD GetDataSource(nsIRDFXMLDataSource*& ds);

protected:
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
    void      GetNameSpaceURI(PRInt32 aID, nsString& aURI);
    void      PopNameSpaces();

    nsINameSpaceManager*  mNameSpaceManager;
    nsVoidArray* mNameSpaceStack;
    PRInt32      mRDFNameSpaceID;

    void SplitQualifiedName(const nsString& aQualifiedName,
                            PRInt32& rNameSpaceID,
                            nsString& rProperty);

    // RDF-specific parsing
    nsresult GetIdAboutAttribute(const nsIParserNode& aNode, nsIRDFResource** aResource);
    nsresult GetResourceAttribute(const nsIParserNode& aNode, nsIRDFResource** aResource);
    nsresult AddProperties(const nsIParserNode& aNode, nsIRDFResource* aSubject);

    virtual nsresult OpenRDF(const nsIParserNode& aNode);
    virtual nsresult OpenObject(const nsIParserNode& aNode);
    virtual nsresult OpenProperty(const nsIParserNode& aNode);
    virtual nsresult OpenMember(const nsIParserNode& aNode);
    virtual nsresult OpenValue(const nsIParserNode& aNode);

    // Miscellaneous RDF junk
    nsIRDFService*         mRDFService;
    nsIRDFXMLDataSource*   mDataSource;
    RDFContentSinkState    mState;

    // content stack management
    PRInt32         PushContext(nsIRDFResource *aContext, RDFContentSinkState aState);
    nsresult        PopContext(nsIRDFResource*& rContext, RDFContentSinkState& rState);
    nsIRDFResource* GetContextElement(PRInt32 ancestor = 0);

    nsVoidArray* mContextStack;

    nsIURL*      mDocumentURL;
    PRUint32     mGenSym; // for generating anonymous resources

    PRBool       mHaveSetRootResource;
};

////////////////////////////////////////////////////////////////////////

RDFContentSinkImpl::RDFContentSinkImpl()
    : mDocumentURL(nsnull),
      mRDFService(nsnull),
      mDataSource(nsnull),
      mGenSym(0),
      mNameSpaceManager(nsnull),
      mNameSpaceStack(nsnull),
      mRDFNameSpaceID(kNameSpaceID_Unknown),
      mContextStack(nsnull),
      mText(nsnull),
      mTextLength(0),
      mTextSize(0),
      mConstrainSize(PR_TRUE),
      mHaveSetRootResource(PR_FALSE)
{
    NS_INIT_REFCNT();

#ifdef PR_LOGGING
    if (! gLog)
        gLog = PR_NewLogModule("nsRDFContentSink");
#endif
}


RDFContentSinkImpl::~RDFContentSinkImpl()
{
    NS_IF_RELEASE(mDocumentURL);

    if (mRDFService)
        nsServiceManager::ReleaseService(kRDFServiceCID, mRDFService);

    NS_IF_RELEASE(mDataSource);

    NS_IF_RELEASE(mNameSpaceManager);
    if (mNameSpaceStack) {
        // There shouldn't be any here except in an error condition
        PRInt32 index = mNameSpaceStack->Count();

        while (0 < index--) {
            nsINameSpace* nameSpace = (nsINameSpace*)mNameSpaceStack->ElementAt(index);
            NS_RELEASE(nameSpace);
        }
        delete mNameSpaceStack;
    }
    if (mContextStack) {
        PR_LOG(gLog, PR_LOG_ALWAYS,
               ("rdfxml: warning: unclosed tag"));

        // XXX we should never need to do this, but, we'll write the
        // code all the same. If someone left the content stack dirty,
        // pop all the elements off the stack and release them.
        PRInt32 index = mContextStack->Count();
        while (0 < index--) {
            nsIRDFResource* resource;
            RDFContentSinkState state;
            PopContext(resource, state);

#ifdef PR_LOGGING
            // print some fairly useless debugging info
            // XXX we should save line numbers on the context stack: this'd
            // be about 1000x more helpful.
            if (resource) {
                nsXPIDLCString uri;
                resource->GetValue(getter_Copies(uri));
                PR_LOG(gLog, PR_LOG_ALWAYS,
                       ("rdfxml:   uri=%s", (const char*) uri));
            }
#endif

            NS_IF_RELEASE(resource);
        }

        delete mContextStack;
    }
    PR_FREEIF(mText);
}

////////////////////////////////////////////////////////////////////////
// nsISupports interface

NS_IMPL_ADDREF(RDFContentSinkImpl);
NS_IMPL_RELEASE(RDFContentSinkImpl);

NS_IMETHODIMP
RDFContentSinkImpl::QueryInterface(REFNSIID iid, void** result)
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
// nsIContentSink interface

NS_IMETHODIMP 
RDFContentSinkImpl::WillBuildModel(void)
{
    return (mDataSource != nsnull) ? mDataSource->BeginLoad() : NS_OK;
}

NS_IMETHODIMP 
RDFContentSinkImpl::DidBuildModel(PRInt32 aQualityLevel)
{
    return (mDataSource != nsnull) ? mDataSource->EndLoad() : NS_OK;
}

NS_IMETHODIMP 
RDFContentSinkImpl::WillInterrupt(void)
{
    return (mDataSource != nsnull) ? mDataSource->Interrupt() : NS_OK;
}

NS_IMETHODIMP 
RDFContentSinkImpl::WillResume(void)
{
    return (mDataSource != nsnull) ? mDataSource->Resume() : NS_OK;
}

NS_IMETHODIMP 
RDFContentSinkImpl::SetParser(nsIParser* aParser)
{
    return NS_OK;
}

NS_IMETHODIMP 
RDFContentSinkImpl::OpenContainer(const nsIParserNode& aNode)
{
    // XXX Hopefully the parser will flag this before we get here. If
    // we're in the epilog, there should be no new elements
    NS_PRECONDITION(mState != eRDFContentSinkState_InEpilog, "tag in RDF doc epilog");

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
    case eRDFContentSinkState_InProlog:
        rv = OpenRDF(aNode);
        break;

    case eRDFContentSinkState_InDocumentElement:
        rv = OpenObject(aNode);
        break;

    case eRDFContentSinkState_InDescriptionElement:
        rv = OpenProperty(aNode);
        break;

    case eRDFContentSinkState_InContainerElement:
        rv = OpenMember(aNode);
        break;

    case eRDFContentSinkState_InPropertyElement:
        rv = OpenValue(aNode);
        break;

    case eRDFContentSinkState_InMemberElement:
        rv = OpenValue(aNode);
        break;

    case eRDFContentSinkState_InEpilog:
        PR_ASSERT(0);
        rv = NS_ERROR_UNEXPECTED; // XXX
        break;
    }

	NS_ASSERTION(NS_SUCCEEDED(rv), "unexpected content");
    return rv;
}

NS_IMETHODIMP 
RDFContentSinkImpl::CloseContainer(const nsIParserNode& aNode)
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
        mState = eRDFContentSinkState_InEpilog;

    PopNameSpaces();
      
    NS_IF_RELEASE(resource);
    return NS_OK;
}


NS_IMETHODIMP 
RDFContentSinkImpl::AddLeaf(const nsIParserNode& aNode)
{
    // XXX For now, all leaf content is character data
    AddCharacterData(aNode);
    return NS_OK;
}

NS_IMETHODIMP
RDFContentSinkImpl::NotifyError(const nsParserError* aError)
{
    PR_LOG(gLog, PR_LOG_ALWAYS,
           ("rdfxml: %s", aError));
    return NS_OK;
}

// nsIXMLContentSink
NS_IMETHODIMP 
RDFContentSinkImpl::AddXMLDecl(const nsIParserNode& aNode)
{
    // XXX We'll ignore it for now
    PR_LOG(gLog, PR_LOG_ALWAYS,
           ("rdfxml: ignoring XML decl at line %d",
            aNode.GetSourceLineNumber()));

    return NS_OK;
}


NS_IMETHODIMP 
RDFContentSinkImpl::AddComment(const nsIParserNode& aNode)
{
    FlushText();
    nsAutoString text;
    nsresult result = NS_OK;

    text = aNode.GetText();

    // XXX add comment here...

    return result;
}


NS_IMETHODIMP 
RDFContentSinkImpl::AddProcessingInstruction(const nsIParserNode& aNode)
{

static const char kStyleSheetPI[] = "<?xml-stylesheet";
static const char kCSSType[] = "text/css";

static const char kDataSourcePI[] = "<?rdf-datasource";
static const char kContentModelBuilderPI[] = "<?rdf-builder";

    nsresult rv = NS_OK;
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
        if (! href.Length())
            return NS_OK;
    
        nsAutoString type;
        if (NS_FAILED(rv = nsRDFParserUtils::GetQuotedAttributeValue(text, "type", type)))
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
    else if (0 == text.Find(kDataSourcePI)) {
        nsAutoString href;
        rv = nsRDFParserUtils::GetQuotedAttributeValue(text, "href", href);
        if (NS_FAILED(rv) || (0 == href.Length()))
            return rv;

        char uri[256];
        href.ToCString(uri, sizeof(uri));

        rv = mDataSource->AddNamedDataSourceURI(uri);
    }

    return rv;
}

NS_IMETHODIMP 
RDFContentSinkImpl::AddDocTypeDecl(const nsIParserNode& aNode)
{
    // XXX We'll ignore it for now
    PR_LOG(gLog, PR_LOG_ALWAYS,
           ("rdfxml: ignoring doc type decl at line %d",
            aNode.GetSourceLineNumber()));

    return NS_OK;
}


NS_IMETHODIMP 
RDFContentSinkImpl::AddCharacterData(const nsIParserNode& aNode)
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
RDFContentSinkImpl::AddUnparsedEntity(const nsIParserNode& aNode)
{
    // XXX We'll ignore it for now
    PR_LOG(gLog, PR_LOG_ALWAYS,
           ("rdfxml: ignoring unparsed entity at line %d",
            aNode.GetSourceLineNumber()));


    return NS_OK;
}

NS_IMETHODIMP 
RDFContentSinkImpl::AddNotation(const nsIParserNode& aNode)
{
    // XXX We'll ignore it for now
    PR_LOG(gLog, PR_LOG_ALWAYS,
           ("rdfxml: ignoring notation at line %d",
            aNode.GetSourceLineNumber()));


    return NS_OK;
}

NS_IMETHODIMP 
RDFContentSinkImpl::AddEntityReference(const nsIParserNode& aNode)
{
    // XXX We'll ignore it for now
    PR_LOG(gLog, PR_LOG_ALWAYS,
           ("rdfxml: ignoring entity reference at line %d",
            aNode.GetSourceLineNumber()));

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsIRDFContentSink interface

NS_IMETHODIMP
RDFContentSinkImpl::Init(nsIURL* aURL, nsINameSpaceManager* aNameSpaceManager)
{
    NS_PRECONDITION((nsnull != aURL) && (nsnull != aNameSpaceManager), "null ptr");
    if ((! aURL) || (! aNameSpaceManager))
        return NS_ERROR_NULL_POINTER;

    mDocumentURL = aURL;
    NS_ADDREF(aURL);

    mNameSpaceManager = aNameSpaceManager;
    NS_ADDREF(mNameSpaceManager);

    nsresult rv;
    if (NS_FAILED(rv = nsServiceManager::GetService(kRDFServiceCID,
                                                    kIRDFServiceIID,
                                                    (nsISupports**) &mRDFService)))
        return rv;

    mState = eRDFContentSinkState_InProlog;
    return NS_OK;
}

NS_IMETHODIMP
RDFContentSinkImpl::SetDataSource(nsIRDFXMLDataSource* ds)
{
    NS_IF_RELEASE(mDataSource);
    mDataSource = ds;
    NS_IF_ADDREF(mDataSource);
    return NS_OK;
}


NS_IMETHODIMP
RDFContentSinkImpl::GetDataSource(nsIRDFXMLDataSource*& ds)
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
RDFContentSinkImpl::FlushText(PRBool aCreateTextNode, PRBool* aDidFlush)
{
    nsresult rv = NS_OK;
    PRBool didFlush = PR_FALSE;
    if (0 != mTextLength) {
        if (aCreateTextNode && rdf_IsDataInBuffer(mText, mTextLength)) {
            // XXX if there's anything but whitespace, then we'll
            // create a text node.

            switch (mState) {
            case eRDFContentSinkState_InMemberElement: {
                nsAutoString value;
                value.Append(mText, mTextLength);
                value.Trim(" \t\n\r");

                nsIRDFLiteral* literal;
                if (NS_SUCCEEDED(rv = mRDFService->GetLiteral(value, &literal))) {
                    rv = rdf_ContainerAppendElement(mDataSource,
                                                    GetContextElement(1),
                                                    literal);
                    NS_RELEASE(literal);
                }
            } break;

            case eRDFContentSinkState_InPropertyElement: {
                nsAutoString value;
                value.Append(mText, mTextLength);
                value.Trim(" \t\n\r");

                rv = rdf_Assert(mDataSource,
                                GetContextElement(1),
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

void
RDFContentSinkImpl::SplitQualifiedName(const nsString& aQualifiedName,
                                       PRInt32& rNameSpaceID,
                                       nsString& rProperty)
{
    rProperty = aQualifiedName;
    nsIAtom* prefix = CutNameSpacePrefix(rProperty);
    rNameSpaceID = GetNameSpaceID(prefix);
    NS_IF_RELEASE(prefix);
}


nsresult
RDFContentSinkImpl::GetIdAboutAttribute(const nsIParserNode& aNode,
                                        nsIRDFResource** aResource)
{
    // This corresponds to the dirty work of production [6.5]
    nsAutoString k;
    nsAutoString attr;
    PRInt32 nameSpaceID;
    PRInt32 ac = aNode.GetAttributeCount();
    nsresult rv;

    for (PRInt32 i = 0; i < ac; i++) {
        // Get upper-cased key
        const nsString& key = aNode.GetKeyAt(i);
        SplitQualifiedName(key, nameSpaceID, attr);

        if (nameSpaceID != mRDFNameSpaceID)
            continue;

        // XXX you can't specify both, but we'll just pick up the
        // first thing that was specified and ignore the other.
        
        if (attr.Equals(kTagRDF_about)) {
            nsAutoString uri = aNode.GetValueAt(i);
            nsRDFParserUtils::StripAndConvert(uri);

            return mRDFService->GetUnicodeResource(uri, aResource);
        }

        if (attr.Equals(kTagRDF_ID)) {
            const char* docURI;
            mDocumentURL->GetSpec(&docURI);

            if (NS_FAILED(rv = mRDFService->GetResource(docURI, aResource)))
                return rv;

            nsAutoString tag = aNode.GetValueAt(i);
            nsRDFParserUtils::StripAndConvert(tag);

            rdf_PossiblyMakeAbsolute(docURI, tag);

            return mRDFService->GetUnicodeResource(tag, aResource);
        }

        if (attr.Equals(kTagRDF_aboutEach)) {
            // XXX we don't deal with aboutEach...
            NS_NOTYETIMPLEMENTED("RDF:aboutEach is not understood at this time");
        }
    }

    // Otherwise, we couldn't find anything, so just gensym one...
    const char* url;
    mDocumentURL->GetSpec(&url);
    return rdf_CreateAnonymousResource(url, aResource);
}


nsresult
RDFContentSinkImpl::GetResourceAttribute(const nsIParserNode& aNode,
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

        if (nameSpaceID != mRDFNameSpaceID)
            continue;

        // XXX you can't specify both, but we'll just pick up the
        // first thing that was specified and ignore the other.

        if (attr.Equals(kTagRDF_resource)) {
            nsAutoString uri = aNode.GetValueAt(i);
            nsRDFParserUtils::StripAndConvert(uri);

            // XXX Take the URI and make it fully qualified by
            // sticking it into the document's URL. This may not be
            // appropriate...
            const char* documentURL;
            mDocumentURL->GetSpec(&documentURL);
            rdf_PossiblyMakeAbsolute(documentURL, uri);

            return mRDFService->GetUnicodeResource(uri, aResource);
        }
    }
    return NS_ERROR_FAILURE;
}

nsresult
RDFContentSinkImpl::AddProperties(const nsIParserNode& aNode,
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

        // skip rdf:about, rdf:ID, and rdf:resource attributes; these
        // are all "special" and should've been dealt with by the
        // caller.
        if ((nameSpaceID == mRDFNameSpaceID) &&
            (attr.Equals(kTagRDF_about) ||
             attr.Equals(kTagRDF_ID) ||
             attr.Equals(kTagRDF_resource)))
            continue;

        v = aNode.GetValueAt(i);
        nsRDFParserUtils::StripAndConvert(v);

        GetNameSpaceURI(nameSpaceID, k);
        k.Append(attr);

        // Add the attribute to RDF
        rdf_Assert(mDataSource, aSubject, k, v);
    }
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// RDF-specific routines used to build the model

nsresult
RDFContentSinkImpl::OpenRDF(const nsIParserNode& aNode)
{
    // ensure that we're actually reading RDF by making sure that the
    // opening tag is <rdf:RDF>, where "rdf:" corresponds to whatever
    // they've declared the standard RDF namespace to be.
    nsAutoString tag;
    PRInt32 nameSpaceID;
    
    SplitQualifiedName(aNode.GetText(), nameSpaceID, tag);

    if (nameSpaceID != mRDFNameSpaceID)
        return NS_ERROR_UNEXPECTED;

    if (! tag.Equals(kTagRDF_RDF))
        return NS_ERROR_UNEXPECTED;

    PushContext(nsnull, mState);
    mState = eRDFContentSinkState_InDocumentElement;
    return NS_OK;
}


nsresult
RDFContentSinkImpl::OpenObject(const nsIParserNode& aNode)
{
    // an "object" non-terminal is either a "description", a "typed
    // node", or a "container", so this change the content sink's
    // state appropriately.

    if (! mRDFService)
        return NS_ERROR_NOT_INITIALIZED;

    nsAutoString tag;
    PRInt32 nameSpaceID;

    SplitQualifiedName(aNode.GetText(), nameSpaceID, tag);

    // Figure out the URI of this object, and create an RDF node for it.
    nsresult rv;
    nsIRDFResource* rdfResource;
    if (NS_FAILED(rv = GetIdAboutAttribute(aNode, &rdfResource)))
        return rv;

    // If we're in a member or property element, then this is the cue
    // that we need to hook the object up into the graph via the
    // member/property.
    switch (mState) {
    case eRDFContentSinkState_InMemberElement: {
        rdf_ContainerAppendElement(mDataSource, GetContextElement(1), rdfResource);
    } break;

    case eRDFContentSinkState_InPropertyElement: {
        mDataSource->Assert(GetContextElement(1), GetContextElement(0), rdfResource, PR_TRUE);
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

    if (nameSpaceID == mRDFNameSpaceID) {
        isaTypedNode = PR_FALSE;

        if (tag.Equals(kTagRDF_Description)) {
            // it's a description
            mState = eRDFContentSinkState_InDescriptionElement;
        }
        else if (tag.Equals(kTagRDF_Bag)) {
            // it's a bag container
            rdf_MakeBag(mDataSource, rdfResource);
            mState = eRDFContentSinkState_InContainerElement;
        }
        else if (tag.Equals(kTagRDF_Seq)) {
            // it's a seq container
            rdf_MakeSeq(mDataSource, rdfResource);
            mState = eRDFContentSinkState_InContainerElement;
        }
        else if (tag.Equals(kTagRDF_Alt)) {
            // it's an alt container
            rdf_MakeAlt(mDataSource, rdfResource);
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
        nsAutoString nameSpace;
        GetNameSpaceURI(nameSpaceID, nameSpace);  // XXX append ':' too?
        nameSpace.Append(tag);
        rdf_Assert(mDataSource, rdfResource, kURIRDF_type, nameSpace);

        mState = eRDFContentSinkState_InDescriptionElement;
    }

    AddProperties(aNode, rdfResource);

    if (mDataSource && !mHaveSetRootResource) {
        mHaveSetRootResource = PR_TRUE;
        mDataSource->SetRootResource(rdfResource);
    }

    NS_RELEASE(rdfResource);

    return NS_OK;
}


nsresult
RDFContentSinkImpl::OpenProperty(const nsIParserNode& aNode)
{
    if (! mRDFService)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv;

    // an "object" non-terminal is either a "description", a "typed
    // node", or a "container", so this change the content sink's
    // state appropriately.
    nsAutoString tag;
    PRInt32 nameSpaceID;

    SplitQualifiedName(aNode.GetText(), nameSpaceID, tag);

    nsAutoString  ns;
    GetNameSpaceURI(nameSpaceID, ns);

    // destructively alter "ns" to contain the fully qualified tag
    // name. We can do this 'cause we don't need it anymore...
    ns.Append(tag);
    nsIRDFResource* rdfProperty;
    if (NS_FAILED(rv = mRDFService->GetUnicodeResource(ns, &rdfProperty)))
        return rv;

    nsIRDFResource* rdfResource;
    if (NS_SUCCEEDED(GetResourceAttribute(aNode, &rdfResource))) {
        // They specified an inline resource for the value of this
        // property. Create an RDF resource for the inline resource
        // URI, add the properties to it, and attach the inline
        // resource to its parent.
        if (NS_SUCCEEDED(rv = AddProperties(aNode, rdfResource))) {
            rv = rdf_Assert(mDataSource,
                            GetContextElement(0),
                            rdfProperty,
                            rdfResource);
        }

        // XXX ignore any failure from above...
        NS_ASSERTION(NS_SUCCEEDED(rv), "problem adding properties");

        // XXX Technically, we should _not_ fall through here and push
        // the element onto the stack: this is supposed to be a closed
        // node. But right now I'm lazy and the code will just Do The
        // Right Thing so long as the RDF is well-formed.
        NS_RELEASE(rdfResource);
    }

    // Push the element onto the context stack and change state.
    PushContext(rdfProperty, mState);
    mState = eRDFContentSinkState_InPropertyElement;

    rdfProperty->Release();
    return NS_OK;
}


nsresult
RDFContentSinkImpl::OpenMember(const nsIParserNode& aNode)
{
    // ensure that we're actually reading a member element by making
    // sure that the opening tag is <rdf:li>, where "rdf:" corresponds
    // to whatever they've declared the standard RDF namespace to be.
    nsAutoString tag;
    PRInt32 nameSpaceID;

    SplitQualifiedName(aNode.GetText(), nameSpaceID, tag);

    if (nameSpaceID != mRDFNameSpaceID)
        return NS_ERROR_UNEXPECTED;

    if (! tag.Equals(kTagRDF_li))
        return NS_ERROR_UNEXPECTED;

    // The parent element is the container.
    nsIRDFResource* container = GetContextElement(0);
    if (! container)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;
    nsIRDFResource* resource;
    if (NS_SUCCEEDED(rv = GetResourceAttribute(aNode, &resource))) {
        // Okay, this node has an RDF:resource="..." attribute. That
        // means that it's a "referenced item," as covered in [6.29].
        rv = rdf_ContainerAppendElement(mDataSource, container, resource);

        // XXX Technically, we should _not_ fall through here and push
        // the element onto the stack: this is supposed to be a closed
        // node. But right now I'm lazy and the code will just Do The
        // Right Thing so long as the RDF is well-formed.
        NS_RELEASE(resource);
    }

    // Change state. Pushing a null context element is a bit weird,
    // but the idea is that there really is _no_ context "property".
    // The contained element will use rdf_ContainerAppendElement() to add
    // the element to the container, which requires only the container
    // and the element to be added.
    PushContext(nsnull, mState);
    mState = eRDFContentSinkState_InMemberElement;
    return NS_OK;
}


nsresult
RDFContentSinkImpl::OpenValue(const nsIParserNode& aNode)
{
    // a "value" can either be an object or a string: we'll only get
    // *here* if it's an object, as raw text is added as a leaf.
    return OpenObject(aNode);
}


////////////////////////////////////////////////////////////////////////
// Content stack management

struct RDFContextStackElement {
    nsIRDFResource*     mResource;
    RDFContentSinkState mState;
};

nsIRDFResource* 
RDFContentSinkImpl::GetContextElement(PRInt32 ancestor /* = 0 */)
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
RDFContentSinkImpl::PushContext(nsIRDFResource *aResource, RDFContentSinkState aState)
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
RDFContentSinkImpl::PopContext(nsIRDFResource*& rResource, RDFContentSinkState& rState)
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
RDFContentSinkImpl::PushNameSpacesFrom(const nsIParserNode& aNode)
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
        mNameSpaceManager->RegisterNameSpace(kRDFNameSpaceURI, mRDFNameSpaceID);
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
RDFContentSinkImpl::CutNameSpacePrefix(nsString& aString)
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
RDFContentSinkImpl::GetNameSpaceID(nsIAtom* aPrefix)
{
    PRInt32 id = kNameSpaceID_Unknown;
  
    if ((nsnull != mNameSpaceStack) && (0 < mNameSpaceStack->Count())) {
        PRInt32 index = mNameSpaceStack->Count() - 1;
        nsINameSpace* nameSpace = (nsINameSpace*)mNameSpaceStack->ElementAt(index);
        nameSpace->FindNameSpaceID(aPrefix, id);
    }

    NS_ASSERTION(kNameSpaceID_Unknown != mRDFNameSpaceID, "failed to register RDF nameSpace");
    return id;
}

void
RDFContentSinkImpl::GetNameSpaceURI(PRInt32 aID, nsString& aURI)
{
  mNameSpaceManager->GetNameSpaceURI(aID, aURI);
}

void
RDFContentSinkImpl::PopNameSpaces()
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
NS_NewRDFContentSink(nsIRDFContentSink** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    RDFContentSinkImpl* sink = new RDFContentSinkImpl();
    if (! sink)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(sink);
    *aResult = sink;
    return NS_OK;
}
