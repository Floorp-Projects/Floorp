/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*

  An implementation for an NGLayout-style content sink that knows how
  to build an RDF content model from XML-serialized RDF.

  For more information on the RDF/XML syntax,
  see http://www.w3.org/TR/REC-rdf-syntax/

  This code is based on the final W3C Recommendation,
  http://www.w3.org/TR/1999/REC-rdf-syntax-19990222.

  Open Issues ------------------

  1) factoring code with nsXMLContentSink - There's some amount of
     common code between this and the HTML content sink. This will
     increase as we support more and more HTML elements. How can code
     from XML/HTML be factored?

  2) We don't support the `parseType' attribute on the Description
     tag; therefore, it is impossible to "inline" raw XML in this
     implemenation.

  3) We don't build the reifications at parse time due to the
     footprint overhead it would incur for large RDF documents. (It
     may be possible to attach a "reification" wrapper datasource that
     would present this information at query-time.) Because of this,
     the `bagID' attribute is not processed correctly.

  4) No attempt is made to `resolve URIs' to a canonical form (the
     specification hints that an implementation should do this). This
     is omitted for the obvious reason that we can ill afford to
     resolve each URI reference.

*/

#include "nsCOMPtr.h"
#include "nsIContentSink.h"
#include "nsIRDFContainer.h"
#include "nsIRDFContainerUtils.h"
#include "nsIRDFContentSink.h"
#include "nsIRDFNode.h"
#include "nsIRDFService.h"
#include "nsIRDFXMLSink.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsIXMLContentSink.h"
#include "nsRDFCID.h"
#include "nsRDFParserUtils.h"
#include "nsVoidArray.h"
#include "nsXPIDLString.h"
#include "prlog.h"
#include "prmem.h"
#include "rdf.h"
#include "rdfutil.h"
#include "nsReadableUtils.h"

#include "nsHTMLTokens.h" // XXX so we can use nsIParserNode::GetTokenType()

////////////////////////////////////////////////////////////////////////

static const char kNameSpaceSeparator = ':';
static const char kNameSpaceDef[] = "xmlns";

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
static NS_DEFINE_CID(kRDFContainerUtilsCID,     NS_RDFCONTAINERUTILS_CID);
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

MOZ_DECL_CTOR_COUNTER(RDFContentSinkImpl::NameSpaceEntry)

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
    NS_IMETHOD AddDocTypeDecl(const nsIParserNode& aNode, PRInt32 aMode=0);
    NS_IMETHOD FlushPendingNotifications() { return NS_OK; }
    NS_IMETHOD SetDocumentCharset(nsAWritableString& aCharset) { return NS_OK; }

    // nsIXMLContentSink
    NS_IMETHOD AddXMLDecl(const nsIParserNode& aNode);    
    NS_IMETHOD AddCharacterData(const nsIParserNode& aNode);
    NS_IMETHOD AddUnparsedEntity(const nsIParserNode& aNode);
    NS_IMETHOD AddNotation(const nsIParserNode& aNode);
    NS_IMETHOD AddEntityReference(const nsIParserNode& aNode);

    // nsIRDFContentSink
    NS_IMETHOD Init(nsIURI* aURL);
    NS_IMETHOD SetDataSource(nsIRDFDataSource* aDataSource);
    NS_IMETHOD GetDataSource(nsIRDFDataSource*& aDataSource);

    // pseudo constants
    static PRInt32 gRefCnt;
    static nsIRDFService* gRDFService;
    static nsIRDFContainerUtils* gRDFContainerUtils;
    static nsIRDFResource* kRDF_type;
    static nsIRDFResource* kRDF_instanceOf; // XXX should be RDF:type
    static nsIRDFResource* kRDF_Alt;
    static nsIRDFResource* kRDF_Bag;
    static nsIRDFResource* kRDF_Seq;
    static nsIRDFResource* kRDF_nextVal;

    static nsIAtom* kAboutAtom;
    static nsIAtom* kIdAtom;
    static nsIAtom* kAboutEachAtom;
    static nsIAtom* kResourceAtom;
    static nsIAtom* kRDFAtom;
    static nsIAtom* kDescriptionAtom;
    static nsIAtom* kBagAtom;
    static nsIAtom* kSeqAtom;
    static nsIAtom* kAltAtom;
    static nsIAtom* kLiAtom;
    static nsIAtom* kXMLNSAtom;

protected:
    // Text management
    nsresult FlushText(PRBool aCreateTextNode=PR_TRUE,
                       PRBool* aDidFlush=nsnull);

    PRUnichar* mText;
    PRInt32 mTextLength;
    PRInt32 mTextSize;
    PRBool mConstrainSize;

    // namespace management
    PRBool IsXMLNSDirective(const nsAReadableString& aAttributeKey, nsIAtom** aPrefix = nsnull);
    nsresult PushNameSpacesFrom(const nsIParserNode& aNode);
    nsresult PopNameSpaces();

    struct NameSpaceEntry {
    public:
        NameSpaceEntry(nsIAtom* aPrefix, const char* aNameSpaceURI)
            : mPrefix(aPrefix), mNext(nsnull) {
            MOZ_COUNT_CTOR(RDFContentSinkImpl::NameSpaceEntry);
            mNameSpaceURI = PL_strdup(aNameSpaceURI);
        }

        ~NameSpaceEntry() {
            MOZ_COUNT_DTOR(RDFContentSinkImpl::NameSpaceEntry);
            PL_strfree(mNameSpaceURI);
        }

        nsCOMPtr<nsIAtom> mPrefix;
        char*             mNameSpaceURI;
        NameSpaceEntry*   mNext;
    };

    NameSpaceEntry* mNameSpaceStack;
    nsAutoVoidArray mNameSpaceScopes;

    nsIAtom*
    CutNameSpacePrefix(nsString& aString);

    nsresult
    GetNameSpaceURI(nsIAtom* aPrefix, const char** aNameSpaceURI);

    nsresult
    ParseTagString(const nsAReadableString& aTagName,
                   const char** aNameSpaceURI,
                   nsIAtom** aTag);

    nsresult
    ParseAttributeString(const nsAReadableString& aAttributeName,
                         const char** aNameSpaceURI,
                         nsIAtom** aAttribute);

    // RDF-specific parsing
    nsresult GetIdAboutAttribute(const nsIParserNode& aNode, nsIRDFResource** aResource, PRBool* aIsAnonymous = nsnull);
    nsresult GetResourceAttribute(const nsIParserNode& aNode, nsIRDFResource** aResource);
    nsresult AddProperties(const nsIParserNode& aNode, nsIRDFResource* aSubject, PRInt32* aCount = nsnull);

    enum eContainerType { eBag, eSeq, eAlt };
    nsresult InitContainer(nsIRDFResource* aContainerType, nsIRDFResource* aContainer);
    nsresult ReinitContainer(nsIRDFResource* aContainerType, nsIRDFResource* aContainer);

    virtual nsresult OpenRDF(const nsIParserNode& aNode);
    virtual nsresult OpenObject(const nsIParserNode& aNode);
    virtual nsresult OpenProperty(const nsIParserNode& aNode);
    virtual nsresult OpenMember(const nsIParserNode& aNode);
    virtual nsresult OpenValue(const nsIParserNode& aNode);

    // The datasource in which we're assigning assertions
    nsCOMPtr<nsIRDFDataSource> mDataSource;

    // The current state of the content sink
    RDFContentSinkState mState;

    // content stack management
    PRInt32         PushContext(nsIRDFResource *aContext, RDFContentSinkState aState);
    nsresult        PopContext(nsIRDFResource*& rContext, RDFContentSinkState& rState);
    nsIRDFResource* GetContextElement(PRInt32 ancestor = 0);

    nsAutoVoidArray* mContextStack;

    nsIURI*      mDocumentURL;
};

PRInt32         RDFContentSinkImpl::gRefCnt = 0;
nsIRDFService*  RDFContentSinkImpl::gRDFService;
nsIRDFContainerUtils* RDFContentSinkImpl::gRDFContainerUtils;
nsIRDFResource* RDFContentSinkImpl::kRDF_type;
nsIRDFResource* RDFContentSinkImpl::kRDF_instanceOf;
nsIRDFResource* RDFContentSinkImpl::kRDF_Alt;
nsIRDFResource* RDFContentSinkImpl::kRDF_Bag;
nsIRDFResource* RDFContentSinkImpl::kRDF_Seq;
nsIRDFResource* RDFContentSinkImpl::kRDF_nextVal;

nsIAtom* RDFContentSinkImpl::kAboutAtom;
nsIAtom* RDFContentSinkImpl::kIdAtom;
nsIAtom* RDFContentSinkImpl::kAboutEachAtom;
nsIAtom* RDFContentSinkImpl::kResourceAtom;
nsIAtom* RDFContentSinkImpl::kRDFAtom;
nsIAtom* RDFContentSinkImpl::kDescriptionAtom;
nsIAtom* RDFContentSinkImpl::kBagAtom;
nsIAtom* RDFContentSinkImpl::kSeqAtom;
nsIAtom* RDFContentSinkImpl::kAltAtom;
nsIAtom* RDFContentSinkImpl::kLiAtom;
nsIAtom* RDFContentSinkImpl::kXMLNSAtom;

////////////////////////////////////////////////////////////////////////


RDFContentSinkImpl::RDFContentSinkImpl()
    : mText(nsnull),
      mTextLength(0),
      mTextSize(0),
      mConstrainSize(PR_TRUE),
      mNameSpaceStack(nsnull),
      mState(eRDFContentSinkState_InProlog),
      mContextStack(nsnull),
      mDocumentURL(nsnull)
{
    NS_INIT_REFCNT();

    if (gRefCnt++ == 0) {
        nsresult rv;
        rv = nsServiceManager::GetService(kRDFServiceCID,
                                          kIRDFServiceIID,
                                          (nsISupports**) &gRDFService);

        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF service");
        if (NS_SUCCEEDED(rv)) {
            rv = gRDFService->GetResource(RDF_NAMESPACE_URI "type",       &kRDF_type);
            rv = gRDFService->GetResource(RDF_NAMESPACE_URI "instanceOf", &kRDF_instanceOf);
            rv = gRDFService->GetResource(RDF_NAMESPACE_URI "Alt",        &kRDF_Alt);
            rv = gRDFService->GetResource(RDF_NAMESPACE_URI "Bag",        &kRDF_Bag);
            rv = gRDFService->GetResource(RDF_NAMESPACE_URI "Seq",        &kRDF_Seq);
            rv = gRDFService->GetResource(RDF_NAMESPACE_URI "nextVal",    &kRDF_nextVal);
        }


        rv = nsServiceManager::GetService(kRDFContainerUtilsCID,
                                          NS_GET_IID(nsIRDFContainerUtils),
                                          (nsISupports**) &gRDFContainerUtils);

        kAboutAtom       = NS_NewAtom("about");
        kIdAtom          = NS_NewAtom("ID");
        kAboutEachAtom   = NS_NewAtom("aboutEach");
        kResourceAtom    = NS_NewAtom("resource");
        kRDFAtom         = NS_NewAtom("RDF");
        kDescriptionAtom = NS_NewAtom("Description");
        kBagAtom         = NS_NewAtom("Bag");
        kSeqAtom         = NS_NewAtom("Seq");
        kAltAtom         = NS_NewAtom("Alt");
        kLiAtom          = NS_NewAtom("li");
        kXMLNSAtom       = NS_NewAtom("xmlns");
    }

#ifdef PR_LOGGING
    if (! gLog)
        gLog = PR_NewLogModule("nsRDFContentSink");
#endif
}


RDFContentSinkImpl::~RDFContentSinkImpl()
{
#ifdef DEBUG_REFS
    --gInstanceCount;
    fprintf(stdout, "%d - RDF: RDFContentSinkImpl\n", gInstanceCount);
#endif

    NS_IF_RELEASE(mDocumentURL);

    if (mNameSpaceStack) {
        // There shouldn't be any here except in an error condition
        PRInt32 i = mNameSpaceScopes.Count();
        while (--i >= 0)
            PopNameSpaces();
    }
    if (mContextStack) {
        PR_LOG(gLog, PR_LOG_ALWAYS,
               ("rdfxml: warning! unclosed tag"));

        // XXX we should never need to do this, but, we'll write the
        // code all the same. If someone left the content stack dirty,
        // pop all the elements off the stack and release them.
        PRInt32 i = mContextStack->Count();
        while (0 < i--) {
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


    if (--gRefCnt == 0) {
        if (gRDFService) {
            nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
            gRDFService = nsnull;
        }


        if (gRDFContainerUtils) {
            nsServiceManager::ReleaseService(kRDFContainerUtilsCID, gRDFContainerUtils);
            gRDFContainerUtils = nsnull;
        }

        NS_IF_RELEASE(kRDF_type);
        NS_IF_RELEASE(kRDF_instanceOf);
        NS_IF_RELEASE(kRDF_Alt);
        NS_IF_RELEASE(kRDF_Bag);
        NS_IF_RELEASE(kRDF_Seq);
        NS_IF_RELEASE(kRDF_nextVal);

        NS_IF_RELEASE(kAboutAtom);
        NS_IF_RELEASE(kIdAtom);
        NS_IF_RELEASE(kAboutEachAtom);
        NS_IF_RELEASE(kResourceAtom);
        NS_IF_RELEASE(kRDFAtom);
        NS_IF_RELEASE(kDescriptionAtom);
        NS_IF_RELEASE(kBagAtom);
        NS_IF_RELEASE(kSeqAtom);
        NS_IF_RELEASE(kAltAtom);
        NS_IF_RELEASE(kLiAtom);
        NS_IF_RELEASE(kXMLNSAtom);
    }
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
    if (mDataSource) {
        nsCOMPtr<nsIRDFXMLSink> sink = do_QueryInterface(mDataSource);
        if (sink) 
            return sink->BeginLoad();
    }
    return NS_OK;
}

NS_IMETHODIMP 
RDFContentSinkImpl::DidBuildModel(PRInt32 aQualityLevel)
{
    if (mDataSource) {
        nsCOMPtr<nsIRDFXMLSink> sink = do_QueryInterface(mDataSource);
        if (sink)
            return sink->EndLoad();
    }
    return NS_OK;
}

NS_IMETHODIMP 
RDFContentSinkImpl::WillInterrupt(void)
{
    if (mDataSource) {
        nsCOMPtr<nsIRDFXMLSink> sink = do_QueryInterface(mDataSource);
        if (sink)
            return sink->Interrupt();
    }
    return NS_OK;
}

NS_IMETHODIMP 
RDFContentSinkImpl::WillResume(void)
{
    if (mDataSource) {
        nsCOMPtr<nsIRDFXMLSink> sink = do_QueryInterface(mDataSource);
        if (sink)
            return sink->Resume();
    }
    return NS_OK;
}

NS_IMETHODIMP 
RDFContentSinkImpl::SetParser(nsIParser* aParser)
{
    return NS_OK;
}

NS_IMETHODIMP 
RDFContentSinkImpl::OpenContainer(const nsIParserNode& aNode)
{
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
        PR_LOG(gLog, PR_LOG_ALWAYS,
               ("rdfxml: unexpected content in epilog at line %d",
                aNode.GetSourceLineNumber()));

        rv = NS_ERROR_UNEXPECTED; // XXX
        break;
    }

    return rv;
}

NS_IMETHODIMP 
RDFContentSinkImpl::CloseContainer(const nsIParserNode& aNode)
{
    FlushText();

    nsIRDFResource* resource;
    if (NS_FAILED(PopContext(resource, mState))) {
        // XXX parser didn't catch unmatched tags?
#ifdef PR_LOGGING
        if (PR_LOG_TEST(gLog, PR_LOG_ALWAYS)) {
            const nsAReadableString& tagStr = aNode.GetText();
            char* tagCStr = ToNewCString(tagStr);

            PR_LOG(gLog, PR_LOG_ALWAYS,
                   ("rdfxml: extra close tag '%s' at line %d",
                    tagCStr, aNode.GetSourceLineNumber()));

            nsCRT::free(tagCStr);
        }
#endif

        return NS_ERROR_UNEXPECTED; // XXX
    }

    // If we've just popped a member or property element, _now_ is the
    // time to add that element to the graph.
    switch (mState) {
    case eRDFContentSinkState_InMemberElement: {
        nsCOMPtr<nsIRDFContainer> container;
        NS_NewRDFContainer(getter_AddRefs(container));
        container->Init(mDataSource, GetContextElement(1));
        container->AppendElement(resource);
    } break;

    case eRDFContentSinkState_InPropertyElement: {
        mDataSource->Assert(GetContextElement(1), GetContextElement(0), resource, PR_TRUE);
    } break;

    default:
        break;
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
    return NS_OK;
}


NS_IMETHODIMP 
RDFContentSinkImpl::AddProcessingInstruction(const nsIParserNode& aNode)
{
    PR_LOG(gLog, PR_LOG_ALWAYS,
           ("rdfxml: ignoring processing instruction at line %d",
            aNode.GetSourceLineNumber()));

    return NS_OK;
}

NS_IMETHODIMP 
RDFContentSinkImpl::AddDocTypeDecl(const nsIParserNode& aNode, PRInt32 aMode)
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
    nsAutoString text(aNode.GetText());

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
        memcpy(&mText[mTextLength], text.get() + offset,
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
RDFContentSinkImpl::Init(nsIURI* aURL)
{
    NS_PRECONDITION(aURL != nsnull, "null ptr");
    if (! aURL)
        return NS_ERROR_NULL_POINTER;

    mDocumentURL = aURL;
    NS_ADDREF(aURL);

    mState = eRDFContentSinkState_InProlog;
    return NS_OK;
}

NS_IMETHODIMP
RDFContentSinkImpl::SetDataSource(nsIRDFDataSource* aDataSource)
{
    mDataSource = dont_QueryInterface(aDataSource);
    return NS_OK;
}


NS_IMETHODIMP
RDFContentSinkImpl::GetDataSource(nsIRDFDataSource*& aDataSource)
{
    aDataSource = mDataSource;
    NS_IF_ADDREF(aDataSource);
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
                if (NS_SUCCEEDED(rv = gRDFService->GetLiteral(value.get(), &literal))) {
                    nsCOMPtr<nsIRDFContainer> container;
                    NS_NewRDFContainer(getter_AddRefs(container));
                    container->Init(mDataSource, GetContextElement(1));
                    container->AppendElement(literal);
                    NS_RELEASE(literal);
                }
            } break;

            case eRDFContentSinkState_InPropertyElement: {
                nsAutoString value;
                value.Append(mText, mTextLength);
                value.Trim(" \t\n\r");

                nsCOMPtr<nsIRDFLiteral> target;
                rv = gRDFService->GetLiteral(value.get(), getter_AddRefs(target));
                if (NS_FAILED(rv)) return rv;

                rv = mDataSource->Assert(GetContextElement(1), GetContextElement(0), target, PR_TRUE);
                if (NS_FAILED(rv)) return rv;
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

nsIAtom*
RDFContentSinkImpl::CutNameSpacePrefix(nsString& aString)
{
    PRInt32 nsoffset = aString.FindChar(kNameSpaceSeparator);

    if (nsoffset >= 0) {
        nsAutoString prefix;
        aString.Left(prefix, nsoffset);
        aString.Cut(0, nsoffset+1);
        return NS_NewAtom(prefix);
    }
    else {
        return nsnull;
    }
}


nsresult
RDFContentSinkImpl::GetNameSpaceURI(nsIAtom* aPrefix, const char** aNameSpaceURI)
{
    // If we're trying to resolve a namespace ID from a prefix, then
    // we'd better have some namespaces open. We don't assert here
    // because the likelihood of bogus files is high, and it doesn't
    // make sense to drop into the debugger.
    for (NameSpaceEntry* ns = mNameSpaceStack; ns != nsnull; ns = ns->mNext) {
        if (ns->mPrefix.get() == aPrefix) {
            *aNameSpaceURI = ns->mNameSpaceURI;
            return NS_OK;
        }
    }

    // Couldn't find the namespace, probably because the prefix
    // was never declared using an 'xmlns' decl.
    *aNameSpaceURI = nsnull;

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gLog, PR_LOG_ALWAYS)) {
        nsAutoString prefixStr;
        if (aPrefix)
            aPrefix->ToString(prefixStr);

        char* prefixCStr = prefixStr.ToNewCString();

        PR_LOG(gLog, PR_LOG_ALWAYS,
               ("rdfxml: undeclared namespace prefix '%s'",
                prefixCStr));

        nsCRT::free(prefixCStr);
    }
#endif

    return NS_ERROR_FAILURE;
}

nsresult
RDFContentSinkImpl::ParseTagString(const nsAReadableString& aTagName,
                                   const char** aNameSpaceURI,
                                   nsIAtom** aTag)
{
    // Split the fully-qualified name into a prefix and a tag part.
    nsAutoString tag(aTagName);
    nsCOMPtr<nsIAtom> prefix = getter_AddRefs(CutNameSpacePrefix(tag));

    GetNameSpaceURI(prefix, aNameSpaceURI);
    *aTag = NS_NewAtom(tag);
    return NS_OK;
}


nsresult
RDFContentSinkImpl::ParseAttributeString(const nsAReadableString& aAttributeName,
                                         const char** aNameSpaceURI,
                                         nsIAtom** aAttribute)
{
    // Split the fully-qualified name into a prefix and a tag part.
    nsAutoString attr(aAttributeName);
    nsCOMPtr<nsIAtom> prefix = getter_AddRefs(CutNameSpacePrefix(attr));

    if (prefix) {
        GetNameSpaceURI(prefix, aNameSpaceURI);
    }
    else {
        *aNameSpaceURI = nsnull;
    }

    *aAttribute = NS_NewAtom(attr);
    return NS_OK;
}


nsresult
RDFContentSinkImpl::GetIdAboutAttribute(const nsIParserNode& aNode,
                                        nsIRDFResource** aResource,
                                        PRBool* aIsAnonymous)
{
    // This corresponds to the dirty work of production [6.5]
    nsAutoString k;
    PRInt32 ac = aNode.GetAttributeCount();
    nsresult rv;

    nsXPIDLCString docURI;
    rv = mDocumentURL->GetSpec(getter_Copies(docURI));
    if (NS_FAILED(rv)) return rv;

    for (PRInt32 i = 0; i < ac; i++) {
        // Get upper-cased key
        const nsAReadableString& key = aNode.GetKeyAt(i);

        const char* nameSpaceURI;
        nsCOMPtr<nsIAtom> attr;
        rv = ParseAttributeString(key, &nameSpaceURI, getter_AddRefs(attr));
        if (NS_FAILED(rv)) return rv;

        // We'll accept either `ID' or `rdf:ID' (ibid with `about' or
        // `rdf:about') in the spirit of being liberal towards the
        // input that we receive.
        if (nameSpaceURI && 0 != PL_strcmp(nameSpaceURI, kRDFNameSpaceURI))
            continue;

        // XXX you can't specify both, but we'll just pick up the
        // first thing that was specified and ignore the other.
        
        if (attr.get() == kAboutAtom) {
            if (aIsAnonymous)
                *aIsAnonymous = PR_FALSE;

            nsAutoString uri(aNode.GetValueAt(i));
            nsRDFParserUtils::StripAndConvert(uri);

            rdf_MakeAbsoluteURI(NS_ConvertUTF8toUCS2(docURI), uri);

            return gRDFService->GetUnicodeResource(uri.get(), aResource);
        }
        else if (attr.get() == kIdAtom) {
            if (aIsAnonymous)
                *aIsAnonymous = PR_FALSE;

            nsAutoString name(aNode.GetValueAt(i));
            nsRDFParserUtils::StripAndConvert(name);

            // In the spirit of leniency, we do not bother trying to
            // enforce that this be a valid "XML Name" (see
            // http://www.w3.org/TR/REC-xml#NT-Nmtoken), as per
            // 6.21. If we wanted to, this would be where to do it.

            // Construct an in-line resource whose URI is the
            // document's URI plus the XML name specified in the ID
            // attribute.
            name.Insert(PRUnichar('#'), 0);
            
            rdf_MakeAbsoluteURI(NS_ConvertUTF8toUCS2(docURI), name);

            return gRDFService->GetUnicodeResource(name.get(), aResource);
        }
        else if (attr.get() == kAboutEachAtom) {
            // XXX we don't deal with aboutEach...
            PR_LOG(gLog, PR_LOG_ALWAYS,
                   ("rdfxml: ignoring aboutEach at line %d",
                    aNode.GetSourceLineNumber()));
        }
    }

    // Otherwise, we couldn't find anything, so just gensym one...
    if (aIsAnonymous)
        *aIsAnonymous = PR_TRUE;

    rv = gRDFService->GetAnonymousResource(aResource);
    return rv;
}


nsresult
RDFContentSinkImpl::GetResourceAttribute(const nsIParserNode& aNode,
                                         nsIRDFResource** aResource)
{
    nsresult rv;
    nsAutoString k;
    PRInt32 ac = aNode.GetAttributeCount();

    for (PRInt32 i = 0; i < ac; i++) {
        // Get upper-cased key
        const nsAReadableString& key = aNode.GetKeyAt(i);

        const char* nameSpaceURI;
        nsCOMPtr<nsIAtom> attr;
        rv = ParseAttributeString(key, &nameSpaceURI, getter_AddRefs(attr));
        if (NS_FAILED(rv)) return rv;

        // We'll accept `resource' or `rdf:resource', under the spirit
        // that we should be liberal towards the input that we
        // receive.
        if (nameSpaceURI && 0 != PL_strcmp(nameSpaceURI, kRDFNameSpaceURI))
            continue;

        // XXX you can't specify both, but we'll just pick up the
        // first thing that was specified and ignore the other.

        if (attr.get() == kResourceAtom) {
            nsAutoString uri(aNode.GetValueAt(i));
            nsRDFParserUtils::StripAndConvert(uri);

            // XXX Take the URI and make it fully qualified by
            // sticking it into the document's URL. This may not be
            // appropriate...
            char* documentURL;
            mDocumentURL->GetSpec(&documentURL);
            rdf_MakeAbsoluteURI(NS_ConvertUTF8toUCS2(documentURL), uri);
            nsCRT::free(documentURL);

            return gRDFService->GetUnicodeResource(uri.get(), aResource);
        }
    }
    return NS_ERROR_FAILURE;
}

nsresult
RDFContentSinkImpl::AddProperties(const nsIParserNode& aNode,
                                  nsIRDFResource* aSubject,
                                  PRInt32* aCount)
{
    // Initialize the out parameter to zero, if they've asked for us
    // to count properties we've added...
    if (aCount)
        *aCount = 0;

    // Add tag attributes to the content attributes
    PRInt32 count = aNode.GetAttributeCount();

    for (PRInt32 i = 0; i < count; i++) {
        // Get upper-cased key
        const nsAReadableString& key = aNode.GetKeyAt(i);

        // skip 'xmlns' directives, these are "meta" information
        if (IsXMLNSDirective(key))
            continue;

        const char* nameSpaceURI;
        nsCOMPtr<nsIAtom> attr;
        ParseAttributeString(key, &nameSpaceURI, getter_AddRefs(attr));

        // skip `about', `ID', and `resource' attributes (either with
        // or without the `rdf:' prefix); these are all "special" and
        // should've been dealt with by the caller.
        if ((!nameSpaceURI || 0 == PL_strcmp(nameSpaceURI, kRDFNameSpaceURI)) &&
            (attr.get() == kAboutAtom ||
             attr.get() == kIdAtom ||
             attr.get() == kResourceAtom))
            continue;

        nsAutoString v(aNode.GetValueAt(i));
        nsRDFParserUtils::StripAndConvert(v);

        nsCAutoString propertyStr(nameSpaceURI);

        const PRUnichar* attrName;
        attr->GetUnicode(&attrName);

        propertyStr += NS_ConvertUCS2toUTF8(attrName);

        // Add the assertion to RDF
        nsCOMPtr<nsIRDFResource> property;
        gRDFService->GetResource(propertyStr, getter_AddRefs(property));

        nsCOMPtr<nsIRDFLiteral> target;
        gRDFService->GetLiteral(v.get(), getter_AddRefs(target));

        mDataSource->Assert(aSubject, property, target, PR_TRUE);

        // If the caller cares, let 'em know that we've added another
        // property.
        if (aCount)
            ++(*aCount);
    }
    return NS_OK;
}

// XXX Wish there was a better macro in nsCom.h...
#if defined(XP_WIN)
#define STDCALL __stdcall
#elif defined(XP_OS2)
#define STDCALL
#else
#define STDCALL
#endif

typedef nsresult (STDCALL nsIRDFContainerUtils::*nsContainerTestFn)(nsIRDFDataSource* aDataSource,
                                                                    nsIRDFResource* aResource,
                                                                    PRBool* aResult);

typedef nsresult (STDCALL nsIRDFContainerUtils::*nsMakeContainerFn)(nsIRDFDataSource* aDataSource,
                                                                    nsIRDFResource* aContainer,
                                                                    nsIRDFContainer** aResult);

struct ContainerInfo {
    nsIRDFResource**  mType;
    nsContainerTestFn mTestFn;
    nsMakeContainerFn mMakeFn;
};

ContainerInfo gContainerInfo[] = {
    { &RDFContentSinkImpl::kRDF_Alt, &nsIRDFContainerUtils::IsAlt, &nsIRDFContainerUtils::MakeAlt },
    { &RDFContentSinkImpl::kRDF_Bag, &nsIRDFContainerUtils::IsBag, &nsIRDFContainerUtils::MakeBag },
    { &RDFContentSinkImpl::kRDF_Seq, &nsIRDFContainerUtils::IsSeq, &nsIRDFContainerUtils::MakeSeq },
    { 0, 0, 0 },
};

nsresult
RDFContentSinkImpl::InitContainer(nsIRDFResource* aContainerType, nsIRDFResource* aContainer)
{
    // Do the right kind of initialization based on the container
    // 'type' resource, and the state of the container (i.e., 'make' a
    // new container vs. 'reinitialize' the container).
    nsresult rv;

    for (ContainerInfo* info = gContainerInfo; info->mType != 0; ++info) {
        if (*info->mType != aContainerType)
            continue;

        PRBool isContainer;
        rv = (gRDFContainerUtils->*(info->mTestFn))(mDataSource, aContainer, &isContainer);
        if (isContainer) {
            rv = ReinitContainer(aContainerType, aContainer);
        }
        else {
            rv = (gRDFContainerUtils->*(info->mMakeFn))(mDataSource, aContainer, nsnull);
        }
        return rv;
    }

    NS_NOTREACHED("not an RDF container type");
    return NS_ERROR_FAILURE;
}



nsresult
RDFContentSinkImpl::ReinitContainer(nsIRDFResource* aContainerType, nsIRDFResource* aContainer)
{
    // Mega-kludge to deal with the fact that Make[Seq|Alt|Bag] is
    // idempotent, and as such, containers will have state (e.g.,
    // RDF:nextVal) maintained in the graph across loads. This
    // re-initializes each container's RDF:nextVal to '1', and 'marks'
    // the container as such.
    nsresult rv;

    nsCOMPtr<nsIRDFLiteral> one;
    rv = gRDFService->GetLiteral(NS_LITERAL_STRING("1").get(), getter_AddRefs(one));
    if (NS_FAILED(rv)) return rv;

    // Re-initialize the 'nextval' property
    nsCOMPtr<nsIRDFNode> nextval;
    rv = mDataSource->GetTarget(aContainer, kRDF_nextVal, PR_TRUE, getter_AddRefs(nextval));
    if (NS_FAILED(rv)) return rv;

    rv = mDataSource->Change(aContainer, kRDF_nextVal, nextval, one);
    if (NS_FAILED(rv)) return rv;

    // Re-mark as a container. XXX should be kRDF_type
    rv = mDataSource->Assert(aContainer, kRDF_instanceOf, aContainerType, PR_TRUE);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to mark container as such");
    if (NS_FAILED(rv)) return rv;

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
    nsresult rv;

    nsCOMPtr<nsIAtom> tag;
    const char* nameSpaceURI;
    rv = ParseTagString(aNode.GetText(), &nameSpaceURI, getter_AddRefs(tag));
    if (NS_FAILED(rv)) return rv;

    if ((nameSpaceURI && 0 != PL_strcmp(nameSpaceURI, kRDFNameSpaceURI))
        || (tag.get() != kRDFAtom)) {
        PR_LOG(gLog, PR_LOG_ALWAYS,
               ("rdfxml: expected RDF:RDF at line %d",
                aNode.GetSourceLineNumber()));

        return NS_ERROR_UNEXPECTED;
    }

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
    nsresult rv;

    nsCOMPtr<nsIAtom> tag;
    const char* nameSpaceURI;
    ParseTagString(aNode.GetText(), &nameSpaceURI, getter_AddRefs(tag));

    // Figure out the URI of this object, and create an RDF node for it.
    nsCOMPtr<nsIRDFResource> source;
    GetIdAboutAttribute(aNode, getter_AddRefs(source));

    // If there is no `ID' or `about', then there's not much we can do.
    if (! source)
        return NS_ERROR_FAILURE;

    // Push the element onto the context stack
    PushContext(source, mState);

    // Now figure out what kind of state transition we need to
    // make. We'll either be going into a mode where we parse a
    // description or a container.
    PRBool isaTypedNode = PR_TRUE;

    if (nameSpaceURI && 0 == PL_strcmp(nameSpaceURI, kRDFNameSpaceURI)) {
        isaTypedNode = PR_FALSE;

        if (tag.get() == kDescriptionAtom) {
            // it's a description
            mState = eRDFContentSinkState_InDescriptionElement;
        }
        else if (tag.get() == kBagAtom) {
            // it's a bag container
            InitContainer(kRDF_Bag, source);
            mState = eRDFContentSinkState_InContainerElement;
        }
        else if (tag.get() == kSeqAtom) {
            // it's a seq container
            InitContainer(kRDF_Seq, source);
            mState = eRDFContentSinkState_InContainerElement;
        }
        else if (tag.get() == kAltAtom) {
            // it's an alt container
            InitContainer(kRDF_Alt, source);
            mState = eRDFContentSinkState_InContainerElement;
        }
        else {
            // heh, that's not *in* the RDF namespace: just treat it
            // like a typed node
            isaTypedNode = PR_TRUE;
        }
    }

    if (isaTypedNode) {
        nsCAutoString typeStr;

        if (nameSpaceURI)
            typeStr = nameSpaceURI;

        const PRUnichar *attrName;
        tag->GetUnicode(&attrName);

        typeStr += NS_ConvertUCS2toUTF8(attrName);

        nsCOMPtr<nsIRDFResource> type;
        rv = gRDFService->GetResource(typeStr, getter_AddRefs(type));
        if (NS_FAILED(rv)) return rv;

        rv = mDataSource->Assert(source, kRDF_type, type, PR_TRUE);
        if (NS_FAILED(rv)) return rv;

        mState = eRDFContentSinkState_InDescriptionElement;
    }

    AddProperties(aNode, source);
    return NS_OK;
}


nsresult
RDFContentSinkImpl::OpenProperty(const nsIParserNode& aNode)
{
    nsresult rv;

    // an "object" non-terminal is either a "description", a "typed
    // node", or a "container", so this change the content sink's
    // state appropriately.
    const char* nameSpaceURI;
    nsCOMPtr<nsIAtom> tag;
    ParseTagString(aNode.GetText(), &nameSpaceURI, getter_AddRefs(tag));

    nsCAutoString propertyStr(nameSpaceURI);

    const PRUnichar *attrName;
    tag->GetUnicode(&attrName);

    propertyStr += NS_ConvertUCS2toUTF8(attrName);

    nsCOMPtr<nsIRDFResource> property;
    rv = gRDFService->GetResource(propertyStr, getter_AddRefs(property));
    if (NS_FAILED(rv)) return rv;

    // See if they've specified a 'resource' attribute, in which case
    // they mean *that* to be the object of this property.
    nsCOMPtr<nsIRDFResource> target;
    GetResourceAttribute(aNode, getter_AddRefs(target));

    PRBool isAnonymous = PR_FALSE;

    if (! target) {
        // See if an 'ID' attribute has been specified, in which case
        // this corresponds to the fourth form of [6.12].

        // XXX strictly speaking, we should reject the RDF/XML as
        // invalid if they've specified both an 'ID' and a 'resource'
        // attribute. Bah.

        // XXX strictly speaking, 'about=' isn't allowed here, but
        // what the hell.
        GetIdAboutAttribute(aNode, getter_AddRefs(target), &isAnonymous);
    }

    if (target) {
        // They specified an inline resource for the value of this
        // property. Create an RDF resource for the inline resource
        // URI, add the properties to it, and attach the inline
        // resource to its parent.
        PRInt32 count;
        rv = AddProperties(aNode, target, &count);
        NS_ASSERTION(NS_SUCCEEDED(rv), "problem adding properties");
        if (NS_FAILED(rv)) return rv;

        if (count || !isAnonymous) {
            // If the resource was "anonymous" (i.e., they hadn't
            // explicitly set an ID or resource attribute), then we'll
            // only assert this property from the context element *if*
            // there were properties specified on the anonymous
            // resource.
            rv = mDataSource->Assert(GetContextElement(0), property, target, PR_TRUE);
            if (NS_FAILED(rv)) return rv;
        }

        // XXX Technically, we should _not_ fall through here and push
        // the element onto the stack: this is supposed to be a closed
        // node. But right now I'm lazy and the code will just Do The
        // Right Thing so long as the RDF is well-formed.
    }

    // Push the element onto the context stack and change state.
    PushContext(property, mState);
    mState = eRDFContentSinkState_InPropertyElement;

    return NS_OK;
}


nsresult
RDFContentSinkImpl::OpenMember(const nsIParserNode& aNode)
{
    // ensure that we're actually reading a member element by making
    // sure that the opening tag is <rdf:li>, where "rdf:" corresponds
    // to whatever they've declared the standard RDF namespace to be.
    nsresult rv;

    const char* nameSpaceURI;
    nsCOMPtr<nsIAtom> tag;
    ParseTagString(aNode.GetText(), &nameSpaceURI, getter_AddRefs(tag));

    if ((0 != PL_strcmp(nameSpaceURI, kRDFNameSpaceURI)) || (tag.get() != kLiAtom)) {
        PR_LOG(gLog, PR_LOG_ALWAYS,
               ("rdfxml: expected RDF:li at line %d",
                aNode.GetSourceLineNumber()));

        return NS_ERROR_UNEXPECTED;
    }

    // The parent element is the container.
    nsIRDFResource* container = GetContextElement(0);
    if (! container)
        return NS_ERROR_NULL_POINTER;

    nsIRDFResource* resource;
    if (NS_SUCCEEDED(rv = GetResourceAttribute(aNode, &resource))) {
        // Okay, this node has an RDF:resource="..." attribute. That
        // means that it's a "referenced item," as covered in [6.29].
        nsCOMPtr<nsIRDFContainer> c;
        NS_NewRDFContainer(getter_AddRefs(c));
        c->Init(mDataSource, container);
        c->AppendElement(resource);

        // XXX Technically, we should _not_ fall through here and push
        // the element onto the stack: this is supposed to be a closed
        // node. But right now I'm lazy and the code will just Do The
        // Right Thing so long as the RDF is well-formed.
        NS_RELEASE(resource);
    }

    // Change state. Pushing a null context element is a bit weird,
    // but the idea is that there really is _no_ context "property".
    // The contained element will use nsIRDFContainer::AppendElement() to add
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
        mContextStack = new nsAutoVoidArray();
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

    PRInt32 i = mContextStack->Count() - 1;
    e = NS_STATIC_CAST(RDFContextStackElement*, mContextStack->ElementAt(i));
    mContextStack->RemoveElementAt(i);

    // don't bother Release()-ing: call it our implicit AddRef().
    rResource = e->mResource;
    rState    = e->mState;

    delete e;
    return NS_OK;
}
 

////////////////////////////////////////////////////////////////////////
// Namespace management

PRBool
RDFContentSinkImpl::IsXMLNSDirective(const nsAReadableString& aAttributeKey, nsIAtom** aPrefix)
{
    nsAutoString attr(aAttributeKey);

    // Look for `xmlns' at the start of the attribute name
    PRInt32 offset = attr.Find(kNameSpaceDef);
    if (offset != 0)
        return PR_FALSE;

    PRInt32 prefixLen = attr.Length() - sizeof(kNameSpaceDef);
    if (prefixLen <= 0) {
        // they're setting the default namespace; leave `prefix'
        // as nsnull.
    }
    else {
        // make sure there's a `:' character
        if (attr[sizeof(kNameSpaceDef) - 1] != kNameSpaceSeparator)
            return PR_FALSE;

        // if the caller wants the prefix back, compute it for them.
        if (aPrefix) {
            nsAutoString prefixStr;
            attr.Right(prefixStr, prefixLen);

            *aPrefix = NS_NewAtom(prefixStr);
        }
    }

    return PR_TRUE;
}

nsresult
RDFContentSinkImpl::PushNameSpacesFrom(const nsIParserNode& aNode)
{
    // Remember the current top of the stack as the namespace
    // scope. When popping namespaces, we'll remove stack elements
    // until we hit this.
    mNameSpaceScopes.AppendElement(mNameSpaceStack);

    PRInt32 count = aNode.GetAttributeCount();
    for (PRInt32 i = 0; i < count; ++i) {
        const nsAReadableString& key = aNode.GetKeyAt(i);

        nsCOMPtr<nsIAtom> prefix;
        if (! IsXMLNSDirective(key, getter_AddRefs(prefix)))
            continue;

        nsAutoString uri(aNode.GetValueAt(i));
        nsRDFParserUtils::StripAndConvert(uri);

        // Open a local namespace
        NameSpaceEntry* ns = new NameSpaceEntry(prefix, NS_ConvertUCS2toUTF8(uri).get());
        if (! ns)
            return NS_ERROR_OUT_OF_MEMORY;

        ns->mNext = mNameSpaceStack;
        mNameSpaceStack = ns;

        // Add it to the set of namespaces used in the RDF/XML document.
        nsCOMPtr<nsIRDFXMLSink> sink = do_QueryInterface(mDataSource);
        if (sink)
            sink->AddNameSpace(prefix, uri);
    }

    return NS_OK;
}

nsresult
RDFContentSinkImpl::PopNameSpaces()
{
    // Close a namespace scope by removing the topmost entries from
    // the namespace stack.
    PRInt32 top = mNameSpaceScopes.Count() - 1;
    if (top < 0)
        return NS_ERROR_UNEXPECTED; // XXX huh?

    NameSpaceEntry* ns = NS_STATIC_CAST(NameSpaceEntry*, mNameSpaceScopes[top]);
    mNameSpaceScopes.RemoveElementAt(top);

    while (mNameSpaceStack && mNameSpaceStack != ns) {
        NameSpaceEntry* doomed = mNameSpaceStack;
        mNameSpaceStack = mNameSpaceStack->mNext;
        delete doomed;
    }

    return NS_OK;
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
