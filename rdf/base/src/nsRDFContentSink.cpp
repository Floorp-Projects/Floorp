/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

#include "mozilla/Util.h"

#include "nsCOMPtr.h"
#include "nsInterfaceHashtable.h"
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
#include "nsTArray.h"
#include "nsXPIDLString.h"
#include "prlog.h"
#include "rdf.h"
#include "rdfutil.h"
#include "nsReadableUtils.h"
#include "nsIExpatSink.h"
#include "nsCRT.h"
#include "nsIAtom.h"
#include "nsStaticAtom.h"
#include "nsIScriptError.h"
#include "nsIDTD.h"

using namespace mozilla;

////////////////////////////////////////////////////////////////////////
// XPCOM IIDs

static NS_DEFINE_IID(kIContentSinkIID,         NS_ICONTENT_SINK_IID); // XXX grr...
static NS_DEFINE_IID(kIExpatSinkIID,           NS_IEXPATSINK_IID);
static NS_DEFINE_IID(kIRDFServiceIID,          NS_IRDFSERVICE_IID);
static NS_DEFINE_IID(kISupportsIID,            NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIXMLContentSinkIID,      NS_IXMLCONTENT_SINK_IID);
static NS_DEFINE_IID(kIRDFContentSinkIID,      NS_IRDFCONTENTSINK_IID);

static NS_DEFINE_CID(kRDFServiceCID,            NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFContainerUtilsCID,     NS_RDFCONTAINERUTILS_CID);

////////////////////////////////////////////////////////////////////////

#ifdef PR_LOGGING
static PRLogModuleInfo* gLog;
#endif

///////////////////////////////////////////////////////////////////////

enum RDFContentSinkState {
    eRDFContentSinkState_InProlog,
    eRDFContentSinkState_InDocumentElement,
    eRDFContentSinkState_InDescriptionElement,
    eRDFContentSinkState_InContainerElement,
    eRDFContentSinkState_InPropertyElement,
    eRDFContentSinkState_InMemberElement,
    eRDFContentSinkState_InEpilog
};

enum RDFContentSinkParseMode {
    eRDFContentSinkParseMode_Resource,
    eRDFContentSinkParseMode_Literal,
    eRDFContentSinkParseMode_Int,
    eRDFContentSinkParseMode_Date
};

typedef
NS_STDCALL_FUNCPROTO(nsresult,
                     nsContainerTestFn,
                     nsIRDFContainerUtils, IsAlt,
                     (nsIRDFDataSource*, nsIRDFResource*, bool*));

typedef
NS_STDCALL_FUNCPROTO(nsresult,
                     nsMakeContainerFn,
                     nsIRDFContainerUtils, MakeAlt,
                     (nsIRDFDataSource*, nsIRDFResource*, nsIRDFContainer**));

class RDFContentSinkImpl : public nsIRDFContentSink,
                           public nsIExpatSink
{
public:
    RDFContentSinkImpl();
    virtual ~RDFContentSinkImpl();

    // nsISupports
    NS_DECL_ISUPPORTS
    NS_DECL_NSIEXPATSINK

    // nsIContentSink
    NS_IMETHOD WillParse(void);
    NS_IMETHOD WillBuildModel(nsDTDMode aDTDMode);
    NS_IMETHOD DidBuildModel(bool aTerminated);
    NS_IMETHOD WillInterrupt(void);
    NS_IMETHOD WillResume(void);
    NS_IMETHOD SetParser(nsParserBase* aParser);
    virtual void FlushPendingNotifications(mozFlushType aType) { }
    NS_IMETHOD SetDocumentCharset(nsACString& aCharset) { return NS_OK; }
    virtual nsISupports *GetTarget() { return nullptr; }

    // nsIRDFContentSink
    NS_IMETHOD Init(nsIURI* aURL);
    NS_IMETHOD SetDataSource(nsIRDFDataSource* aDataSource);
    NS_IMETHOD GetDataSource(nsIRDFDataSource*& aDataSource);

    // pseudo constants
    static int32_t gRefCnt;
    static nsIRDFService* gRDFService;
    static nsIRDFContainerUtils* gRDFContainerUtils;
    static nsIRDFResource* kRDF_type;
    static nsIRDFResource* kRDF_instanceOf; // XXX should be RDF:type
    static nsIRDFResource* kRDF_Alt;
    static nsIRDFResource* kRDF_Bag;
    static nsIRDFResource* kRDF_Seq;
    static nsIRDFResource* kRDF_nextVal;

#define RDF_ATOM(name_, value_) static nsIAtom* name_;
#include "nsRDFContentSinkAtomList.h"
#undef RDF_ATOM

    typedef struct ContainerInfo {
        nsIRDFResource**  mType;
        nsContainerTestFn mTestFn;
        nsMakeContainerFn mMakeFn;
    } ContainerInfo;

protected:
    // Text management
    void ParseText(nsIRDFNode **aResult);

    nsresult FlushText();
    nsresult AddText(const PRUnichar* aText, int32_t aLength);

    // RDF-specific parsing
    nsresult OpenRDF(const PRUnichar* aName);
    nsresult OpenObject(const PRUnichar* aName ,const PRUnichar** aAttributes);
    nsresult OpenProperty(const PRUnichar* aName, const PRUnichar** aAttributes);
    nsresult OpenMember(const PRUnichar* aName, const PRUnichar** aAttributes);
    nsresult OpenValue(const PRUnichar* aName, const PRUnichar** aAttributes);
    
    nsresult GetIdAboutAttribute(const PRUnichar** aAttributes, nsIRDFResource** aResource, bool* aIsAnonymous = nullptr);
    nsresult GetResourceAttribute(const PRUnichar** aAttributes, nsIRDFResource** aResource);
    nsresult AddProperties(const PRUnichar** aAttributes, nsIRDFResource* aSubject, int32_t* aCount = nullptr);
    void SetParseMode(const PRUnichar **aAttributes);

    PRUnichar* mText;
    int32_t mTextLength;
    int32_t mTextSize;

    /**
     * From the set of given attributes, this method extracts the 
     * namespace definitions and feeds them to the datasource.
     * These can then be suggested to the serializer to be used again.
     * Hopefully, this will keep namespace definitions intact in a 
     * parse - serialize cycle.
     */
    void RegisterNamespaces(const PRUnichar **aAttributes);

    /**
     * Extracts the localname from aExpatName, the name that the Expat parser
     * passes us.
     * aLocalName will contain the localname in aExpatName.
     * The return value is a dependent string containing just the namespace.
     */
    const nsDependentSubstring SplitExpatName(const PRUnichar *aExpatName,
                                              nsIAtom **aLocalName);

    enum eContainerType { eBag, eSeq, eAlt };
    nsresult InitContainer(nsIRDFResource* aContainerType, nsIRDFResource* aContainer);
    nsresult ReinitContainer(nsIRDFResource* aContainerType, nsIRDFResource* aContainer);

    // The datasource in which we're assigning assertions
    nsCOMPtr<nsIRDFDataSource> mDataSource;

    // A hash of all the node IDs referred to
    nsInterfaceHashtable<nsStringHashKey, nsIRDFResource> mNodeIDMap;

    // The current state of the content sink
    RDFContentSinkState mState;
    RDFContentSinkParseMode mParseMode;

    // content stack management
    int32_t         
    PushContext(nsIRDFResource *aContext,
                RDFContentSinkState aState,
                RDFContentSinkParseMode aParseMode);

    nsresult
    PopContext(nsIRDFResource         *&aContext,
               RDFContentSinkState     &aState,
               RDFContentSinkParseMode &aParseMode);

    nsIRDFResource* GetContextElement(int32_t ancestor = 0);


    struct RDFContextStackElement {
        nsCOMPtr<nsIRDFResource> mResource;
        RDFContentSinkState      mState;
        RDFContentSinkParseMode  mParseMode;
    };

    nsAutoTArray<RDFContextStackElement, 8>* mContextStack;

    nsIURI*      mDocumentURL;
};

int32_t         RDFContentSinkImpl::gRefCnt = 0;
nsIRDFService*  RDFContentSinkImpl::gRDFService;
nsIRDFContainerUtils* RDFContentSinkImpl::gRDFContainerUtils;
nsIRDFResource* RDFContentSinkImpl::kRDF_type;
nsIRDFResource* RDFContentSinkImpl::kRDF_instanceOf;
nsIRDFResource* RDFContentSinkImpl::kRDF_Alt;
nsIRDFResource* RDFContentSinkImpl::kRDF_Bag;
nsIRDFResource* RDFContentSinkImpl::kRDF_Seq;
nsIRDFResource* RDFContentSinkImpl::kRDF_nextVal;

////////////////////////////////////////////////////////////////////////

#define RDF_ATOM(name_, value_) nsIAtom* RDFContentSinkImpl::name_;
#include "nsRDFContentSinkAtomList.h"
#undef RDF_ATOM

#define RDF_ATOM(name_, value_) NS_STATIC_ATOM_BUFFER(name_##_buffer, value_)
#include "nsRDFContentSinkAtomList.h"
#undef RDF_ATOM

static const nsStaticAtom rdf_atoms[] = {
#define RDF_ATOM(name_, value_) NS_STATIC_ATOM(name_##_buffer, &RDFContentSinkImpl::name_),
#include "nsRDFContentSinkAtomList.h"
#undef RDF_ATOM
};

RDFContentSinkImpl::RDFContentSinkImpl()
    : mText(nullptr),
      mTextLength(0),
      mTextSize(0),
      mState(eRDFContentSinkState_InProlog),
      mParseMode(eRDFContentSinkParseMode_Literal),
      mContextStack(nullptr),
      mDocumentURL(nullptr)
{
    if (gRefCnt++ == 0) {
        nsresult rv = CallGetService(kRDFServiceCID, &gRDFService);

        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF service");
        if (NS_SUCCEEDED(rv)) {
            rv = gRDFService->GetResource(NS_LITERAL_CSTRING(RDF_NAMESPACE_URI "type"),
                                          &kRDF_type);
            rv = gRDFService->GetResource(NS_LITERAL_CSTRING(RDF_NAMESPACE_URI "instanceOf"),
                                          &kRDF_instanceOf);
            rv = gRDFService->GetResource(NS_LITERAL_CSTRING(RDF_NAMESPACE_URI "Alt"),
                                          &kRDF_Alt);
            rv = gRDFService->GetResource(NS_LITERAL_CSTRING(RDF_NAMESPACE_URI "Bag"),
                                          &kRDF_Bag);
            rv = gRDFService->GetResource(NS_LITERAL_CSTRING(RDF_NAMESPACE_URI "Seq"),
                                          &kRDF_Seq);
            rv = gRDFService->GetResource(NS_LITERAL_CSTRING(RDF_NAMESPACE_URI "nextVal"),
                                          &kRDF_nextVal);
        }


        rv = CallGetService(kRDFContainerUtilsCID, &gRDFContainerUtils);

        NS_RegisterStaticAtoms(rdf_atoms);
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

    if (mContextStack) {
        PR_LOG(gLog, PR_LOG_WARNING,
               ("rdfxml: warning! unclosed tag"));

        // XXX we should never need to do this, but, we'll write the
        // code all the same. If someone left the content stack dirty,
        // pop all the elements off the stack and release them.
        int32_t i = mContextStack->Length();
        while (0 < i--) {
            nsIRDFResource* resource = nullptr;
            RDFContentSinkState state;
            RDFContentSinkParseMode parseMode;
            PopContext(resource, state, parseMode);

#ifdef PR_LOGGING
            // print some fairly useless debugging info
            // XXX we should save line numbers on the context stack: this'd
            // be about 1000x more helpful.
            if (resource) {
                nsXPIDLCString uri;
                resource->GetValue(getter_Copies(uri));
                PR_LOG(gLog, PR_LOG_NOTICE,
                       ("rdfxml:   uri=%s", (const char*) uri));
            }
#endif

            NS_IF_RELEASE(resource);
        }

        delete mContextStack;
    }
    moz_free(mText);


    if (--gRefCnt == 0) {
        NS_IF_RELEASE(gRDFService);
        NS_IF_RELEASE(gRDFContainerUtils);
        NS_IF_RELEASE(kRDF_type);
        NS_IF_RELEASE(kRDF_instanceOf);
        NS_IF_RELEASE(kRDF_Alt);
        NS_IF_RELEASE(kRDF_Bag);
        NS_IF_RELEASE(kRDF_Seq);
        NS_IF_RELEASE(kRDF_nextVal);
    }
}

////////////////////////////////////////////////////////////////////////
// nsISupports interface

NS_IMPL_ADDREF(RDFContentSinkImpl)
NS_IMPL_RELEASE(RDFContentSinkImpl)

NS_IMETHODIMP
RDFContentSinkImpl::QueryInterface(REFNSIID iid, void** result)
{
    NS_PRECONDITION(result, "null ptr");
    if (! result)
        return NS_ERROR_NULL_POINTER;

    *result = nullptr;
    if (iid.Equals(kIRDFContentSinkIID) ||
        iid.Equals(kIXMLContentSinkIID) ||
        iid.Equals(kIContentSinkIID) ||
        iid.Equals(kISupportsIID)) {
        *result = static_cast<nsIXMLContentSink*>(this);
        AddRef();
        return NS_OK;
    }
    else if (iid.Equals(kIExpatSinkIID)) {
      *result = static_cast<nsIExpatSink*>(this);
       AddRef();
       return NS_OK;
    }
    return NS_NOINTERFACE;
}

NS_IMETHODIMP 
RDFContentSinkImpl::HandleStartElement(const PRUnichar *aName, 
                                       const PRUnichar **aAtts, 
                                       uint32_t aAttsCount, 
                                       int32_t aIndex, 
                                       uint32_t aLineNumber)
{
  FlushText();

  nsresult rv = NS_ERROR_UNEXPECTED; // XXX

  RegisterNamespaces(aAtts);

  switch (mState) {
  case eRDFContentSinkState_InProlog:
      rv = OpenRDF(aName);
      break;

  case eRDFContentSinkState_InDocumentElement:
      rv = OpenObject(aName,aAtts);
      break;

  case eRDFContentSinkState_InDescriptionElement:
      rv = OpenProperty(aName,aAtts);
      break;

  case eRDFContentSinkState_InContainerElement:
      rv = OpenMember(aName,aAtts);
      break;

  case eRDFContentSinkState_InPropertyElement:
  case eRDFContentSinkState_InMemberElement:
      rv = OpenValue(aName,aAtts);
      break;

  case eRDFContentSinkState_InEpilog:
      PR_LOG(gLog, PR_LOG_WARNING,
             ("rdfxml: unexpected content in epilog at line %d",
              aLineNumber));
      break;
  }

  return rv;
}

NS_IMETHODIMP 
RDFContentSinkImpl::HandleEndElement(const PRUnichar *aName)
{
  FlushText();

  nsIRDFResource* resource;
  if (NS_FAILED(PopContext(resource, mState, mParseMode))) {
      // XXX parser didn't catch unmatched tags?
#ifdef PR_LOGGING
      if (PR_LOG_TEST(gLog, PR_LOG_WARNING)) {
          nsAutoString tagStr(aName);
          char* tagCStr = ToNewCString(tagStr);

          PR_LogPrint
                 ("rdfxml: extra close tag '%s' at line %d",
                  tagCStr, 0/*XXX fix me */);

          NS_Free(tagCStr);
      }
#endif

      return NS_ERROR_UNEXPECTED; // XXX
  }

  // If we've just popped a member or property element, _now_ is the
  // time to add that element to the graph.
  switch (mState) {
    case eRDFContentSinkState_InMemberElement: 
      {
        nsCOMPtr<nsIRDFContainer> container;
        NS_NewRDFContainer(getter_AddRefs(container));
        container->Init(mDataSource, GetContextElement(1));
        container->AppendElement(resource);
      } 
      break;

    case eRDFContentSinkState_InPropertyElement: 
      {
        mDataSource->Assert(GetContextElement(1), GetContextElement(0), resource, true);                                          
      } break;
    default:
      break;
  }
  
  if (mContextStack->IsEmpty())
      mState = eRDFContentSinkState_InEpilog;

  NS_IF_RELEASE(resource);
  return NS_OK;
}
 
NS_IMETHODIMP 
RDFContentSinkImpl::HandleComment(const PRUnichar *aName)
{
    return NS_OK;
}

NS_IMETHODIMP 
RDFContentSinkImpl::HandleCDataSection(const PRUnichar *aData, 
                                       uint32_t aLength)
{
  return aData ?  AddText(aData, aLength) : NS_OK;
}

NS_IMETHODIMP 
RDFContentSinkImpl::HandleDoctypeDecl(const nsAString & aSubset, 
                                      const nsAString & aName, 
                                      const nsAString & aSystemId, 
                                      const nsAString & aPublicId,
                                      nsISupports* aCatalogData)
{
    return NS_OK;
}

NS_IMETHODIMP 
RDFContentSinkImpl::HandleCharacterData(const PRUnichar *aData, 
                                        uint32_t aLength)
{
  return aData ?  AddText(aData, aLength) : NS_OK;
}

NS_IMETHODIMP 
RDFContentSinkImpl::HandleProcessingInstruction(const PRUnichar *aTarget, 
                                                const PRUnichar *aData)
{
    return NS_OK;
}

NS_IMETHODIMP 
RDFContentSinkImpl::HandleXMLDeclaration(const PRUnichar *aVersion,
                                         const PRUnichar *aEncoding,
                                         int32_t aStandalone)
{
    return NS_OK;
}

NS_IMETHODIMP
RDFContentSinkImpl::ReportError(const PRUnichar* aErrorText, 
                                const PRUnichar* aSourceText,
                                nsIScriptError *aError,
                                bool *_retval)
{
  NS_PRECONDITION(aError && aSourceText && aErrorText, "Check arguments!!!");

  // The expat driver should report the error.
  *_retval = true;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsIContentSink interface

NS_IMETHODIMP 
RDFContentSinkImpl::WillParse(void)
{
    return NS_OK;
}


NS_IMETHODIMP 
RDFContentSinkImpl::WillBuildModel(nsDTDMode)
{
    if (mDataSource) {
        nsCOMPtr<nsIRDFXMLSink> sink = do_QueryInterface(mDataSource);
        if (sink) 
            return sink->BeginLoad();
    }
    return NS_OK;
}

NS_IMETHODIMP 
RDFContentSinkImpl::DidBuildModel(bool aTerminated)
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
RDFContentSinkImpl::SetParser(nsParserBase* aParser)
{
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsIRDFContentSink interface

NS_IMETHODIMP
RDFContentSinkImpl::Init(nsIURI* aURL)
{
    NS_PRECONDITION(aURL != nullptr, "null ptr");
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
    NS_PRECONDITION(aDataSource != nullptr, "SetDataSource null ptr");
    mDataSource = aDataSource;
    NS_ASSERTION(mDataSource != nullptr,"Couldn't QI RDF DataSource");
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

static bool
rdf_IsDataInBuffer(PRUnichar* buffer, int32_t length)
{
    for (int32_t i = 0; i < length; ++i) {
        if (buffer[i] == ' ' ||
            buffer[i] == '\t' ||
            buffer[i] == '\n' ||
            buffer[i] == '\r')
            continue;

        return true;
    }
    return false;
}

void
RDFContentSinkImpl::ParseText(nsIRDFNode **aResult)
{
    // XXXwaterson wasteful, but we'd need to make a copy anyway to be
    // able to call nsIRDFService::Get[Resource|Literal|...]().
    nsAutoString value;
    value.Append(mText, mTextLength);
    value.Trim(" \t\n\r");

    switch (mParseMode) {
    case eRDFContentSinkParseMode_Literal:
        {
            nsIRDFLiteral *result;
            gRDFService->GetLiteral(value.get(), &result);
            *aResult = result;
        }
        break;

    case eRDFContentSinkParseMode_Resource:
        {
            nsIRDFResource *result;
            gRDFService->GetUnicodeResource(value, &result);
            *aResult = result;
        }
        break;

    case eRDFContentSinkParseMode_Int:
        {
            nsresult err;
            int32_t i = value.ToInteger(&err);
            nsIRDFInt *result;
            gRDFService->GetIntLiteral(i, &result);
            *aResult = result;
        }
        break;

    case eRDFContentSinkParseMode_Date:
        {
            PRTime t = rdf_ParseDate(nsDependentCString(NS_LossyConvertUTF16toASCII(value).get(), value.Length()));
            nsIRDFDate *result;
            gRDFService->GetDateLiteral(t, &result);
            *aResult = result;
        }
        break;

    default:
        NS_NOTREACHED("unknown parse type");
        break;
    }
}

nsresult
RDFContentSinkImpl::FlushText()
{
    nsresult rv = NS_OK;
    if (0 != mTextLength) {
        if (rdf_IsDataInBuffer(mText, mTextLength)) {
            // XXX if there's anything but whitespace, then we'll
            // create a text node.

            switch (mState) {
            case eRDFContentSinkState_InMemberElement: {
                nsCOMPtr<nsIRDFNode> node;
                ParseText(getter_AddRefs(node));

                nsCOMPtr<nsIRDFContainer> container;
                NS_NewRDFContainer(getter_AddRefs(container));
                container->Init(mDataSource, GetContextElement(1));

                container->AppendElement(node);
            } break;

            case eRDFContentSinkState_InPropertyElement: {
                nsCOMPtr<nsIRDFNode> node;
                ParseText(getter_AddRefs(node));

                mDataSource->Assert(GetContextElement(1), GetContextElement(0), node, true);
            } break;

            default:
                // just ignore it
                break;
            }
        }
        mTextLength = 0;
    }
    return rv;
}


nsresult
RDFContentSinkImpl::AddText(const PRUnichar* aText, int32_t aLength)
{
    // Create buffer when we first need it
    if (0 == mTextSize) {
        mText = (PRUnichar *) moz_malloc(sizeof(PRUnichar) * 4096);
        if (!mText) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
        mTextSize = 4096;
    }

    // Copy data from string into our buffer; grow the buffer as needed.
    // It never shrinks, but since the content sink doesn't stick around,
    // this shouldn't be a bloat issue.
    int32_t amount = mTextSize - mTextLength;
    if (amount < aLength) {
        // Grow the buffer by at least a factor of two to prevent thrashing.
        // Since PR_REALLOC will leave mText intact if the call fails,
        // don't clobber mText or mTextSize until the new mem is allocated.
        int32_t newSize = (2 * mTextSize > (mTextSize + aLength)) ?
                          (2 * mTextSize) : (mTextSize + aLength);
        PRUnichar* newText = 
            (PRUnichar *) moz_realloc(mText, sizeof(PRUnichar) * newSize);
        if (!newText)
            return NS_ERROR_OUT_OF_MEMORY;
        mTextSize = newSize;
        mText = newText;
    }
    memcpy(&mText[mTextLength], aText, sizeof(PRUnichar) * aLength);
    mTextLength += aLength;

    return NS_OK;
}

bool
rdf_RequiresAbsoluteURI(const nsString& uri)
{
    // cheap shot at figuring out if this requires an absolute url translation
    return !(StringBeginsWith(uri, NS_LITERAL_STRING("urn:")) ||
             StringBeginsWith(uri, NS_LITERAL_STRING("chrome:")));
}

nsresult
RDFContentSinkImpl::GetIdAboutAttribute(const PRUnichar** aAttributes,
                                        nsIRDFResource** aResource,
                                        bool* aIsAnonymous)
{
    // This corresponds to the dirty work of production [6.5]
    nsresult rv = NS_OK;

    nsAutoString nodeID;

    nsCOMPtr<nsIAtom> localName;
    for (; *aAttributes; aAttributes += 2) {
        const nsDependentSubstring& nameSpaceURI =
            SplitExpatName(aAttributes[0], getter_AddRefs(localName));

        // We'll accept either `ID' or `rdf:ID' (ibid with `about' or
        // `rdf:about') in the spirit of being liberal towards the
        // input that we receive.
        if (!nameSpaceURI.IsEmpty() &&
            !nameSpaceURI.EqualsLiteral(RDF_NAMESPACE_URI)) {
          continue;
        }

        // XXX you can't specify both, but we'll just pick up the
        // first thing that was specified and ignore the other.
      
        if (localName == kAboutAtom) {
            if (aIsAnonymous)
                *aIsAnonymous = false;

            nsAutoString relURI(aAttributes[1]);
            if (rdf_RequiresAbsoluteURI(relURI)) {
                nsAutoCString uri;
                rv = mDocumentURL->Resolve(NS_ConvertUTF16toUTF8(aAttributes[1]), uri);
                if (NS_FAILED(rv)) return rv;
                
                return gRDFService->GetResource(uri, 
                                                aResource);
            } 
            return gRDFService->GetResource(NS_ConvertUTF16toUTF8(aAttributes[1]), 
                                            aResource);
        }
        else if (localName == kIdAtom) {
            if (aIsAnonymous)
                *aIsAnonymous = false;
            // In the spirit of leniency, we do not bother trying to
            // enforce that this be a valid "XML Name" (see
            // http://www.w3.org/TR/REC-xml#NT-Nmtoken), as per
            // 6.21. If we wanted to, this would be where to do it.

            // Construct an in-line resource whose URI is the
            // document's URI plus the XML name specified in the ID
            // attribute.
            nsAutoCString name;
            nsAutoCString ref('#');
            AppendUTF16toUTF8(aAttributes[1], ref);

            rv = mDocumentURL->Resolve(ref, name);
            if (NS_FAILED(rv)) return rv;

            return gRDFService->GetResource(name, aResource);
        }
        else if (localName == kNodeIdAtom) {
            nodeID.Assign(aAttributes[1]);
        }
        else if (localName == kAboutEachAtom) {
            // XXX we don't deal with aboutEach...
            //PR_LOG(gLog, PR_LOG_WARNING,
            //       ("rdfxml: ignoring aboutEach at line %d",
            //        aNode.GetSourceLineNumber()));
        }
    }

    // Otherwise, we couldn't find anything, so just gensym one...
    if (aIsAnonymous)
        *aIsAnonymous = true;

    // If nodeID is present, check if we already know about it. If we've seen
    // the nodeID before, use the same resource, otherwise generate a new one.
    if (!nodeID.IsEmpty()) {
        mNodeIDMap.Get(nodeID,aResource);

        if (!*aResource) {
            rv = gRDFService->GetAnonymousResource(aResource);
            mNodeIDMap.Put(nodeID,*aResource);
        }
    }
    else {
        rv = gRDFService->GetAnonymousResource(aResource);
    }

    return rv;
}

nsresult
RDFContentSinkImpl::GetResourceAttribute(const PRUnichar** aAttributes,
                                         nsIRDFResource** aResource)
{
  nsCOMPtr<nsIAtom> localName;

  nsAutoString nodeID;

  for (; *aAttributes; aAttributes += 2) {
      const nsDependentSubstring& nameSpaceURI =
          SplitExpatName(aAttributes[0], getter_AddRefs(localName));

      // We'll accept `resource' or `rdf:resource', under the spirit
      // that we should be liberal towards the input that we
      // receive.
      if (!nameSpaceURI.IsEmpty() &&
          !nameSpaceURI.EqualsLiteral(RDF_NAMESPACE_URI)) {
          continue;
      }

      // XXX you can't specify both, but we'll just pick up the
      // first thing that was specified and ignore the other.

      if (localName == kResourceAtom) {
          // XXX Take the URI and make it fully qualified by
          // sticking it into the document's URL. This may not be
          // appropriate...
          nsAutoString relURI(aAttributes[1]);
          if (rdf_RequiresAbsoluteURI(relURI)) {
              nsresult rv;
              nsAutoCString uri;

              rv = mDocumentURL->Resolve(NS_ConvertUTF16toUTF8(aAttributes[1]), uri);
              if (NS_FAILED(rv)) return rv;

              return gRDFService->GetResource(uri, aResource);
          } 
          return gRDFService->GetResource(NS_ConvertUTF16toUTF8(aAttributes[1]), 
                                          aResource);
      }
      else if (localName == kNodeIdAtom) {
          nodeID.Assign(aAttributes[1]);
      }
  }
    
  // If nodeID is present, check if we already know about it. If we've seen
  // the nodeID before, use the same resource, otherwise generate a new one.
  if (!nodeID.IsEmpty()) {
      mNodeIDMap.Get(nodeID,aResource);

      if (!*aResource) {
          nsresult rv;
          rv = gRDFService->GetAnonymousResource(aResource);
          if (NS_FAILED(rv)) {
              return rv;
          }
          mNodeIDMap.Put(nodeID,*aResource);
      }
      return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

nsresult
RDFContentSinkImpl::AddProperties(const PRUnichar** aAttributes,
                                  nsIRDFResource* aSubject,
                                  int32_t* aCount)
{
  if (aCount)
      *aCount = 0;

  nsCOMPtr<nsIAtom> localName;
  for (; *aAttributes; aAttributes += 2) {
      const nsDependentSubstring& nameSpaceURI =
          SplitExpatName(aAttributes[0], getter_AddRefs(localName));

      // skip 'xmlns' directives, these are "meta" information
      if (nameSpaceURI.EqualsLiteral("http://www.w3.org/2000/xmlns/")) {
        continue;
      }

      // skip `about', `ID', `resource', and 'nodeID' attributes (either with or
      // without the `rdf:' prefix); these are all "special" and
      // should've been dealt with by the caller.
      if (localName == kAboutAtom || localName == kIdAtom ||
          localName == kResourceAtom || localName == kNodeIdAtom) {
          if (nameSpaceURI.IsEmpty() ||
              nameSpaceURI.EqualsLiteral(RDF_NAMESPACE_URI))
              continue;
      }

      // Skip `parseType', `RDF:parseType', and `NC:parseType'. This
      // is meta-information that will be handled in SetParseMode.
      if (localName == kParseTypeAtom) {
          if (nameSpaceURI.IsEmpty() ||
              nameSpaceURI.EqualsLiteral(RDF_NAMESPACE_URI) ||
              nameSpaceURI.EqualsLiteral(NC_NAMESPACE_URI)) {
              continue;
          }
      }

      NS_ConvertUTF16toUTF8 propertyStr(nameSpaceURI);    
      propertyStr.Append(nsAtomCString(localName));

      // Add the assertion to RDF
      nsCOMPtr<nsIRDFResource> property;
      gRDFService->GetResource(propertyStr, getter_AddRefs(property));

      nsCOMPtr<nsIRDFLiteral> target;
      gRDFService->GetLiteral(aAttributes[1], 
                              getter_AddRefs(target));

      mDataSource->Assert(aSubject, property, target, true);
  }
  return NS_OK;
}

void
RDFContentSinkImpl::SetParseMode(const PRUnichar **aAttributes)
{
    nsCOMPtr<nsIAtom> localName;
    for (; *aAttributes; aAttributes += 2) {
        const nsDependentSubstring& nameSpaceURI =
            SplitExpatName(aAttributes[0], getter_AddRefs(localName));

        if (localName == kParseTypeAtom) {
            nsDependentString v(aAttributes[1]);

            if (nameSpaceURI.IsEmpty() ||
                nameSpaceURI.EqualsLiteral(RDF_NAMESPACE_URI)) {
                if (v.EqualsLiteral("Resource"))
                    mParseMode = eRDFContentSinkParseMode_Resource;

                break;
            }
            else if (nameSpaceURI.EqualsLiteral(NC_NAMESPACE_URI)) {
                if (v.EqualsLiteral("Date"))
                    mParseMode = eRDFContentSinkParseMode_Date;
                else if (v.EqualsLiteral("Integer"))
                    mParseMode = eRDFContentSinkParseMode_Int;

                break;
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////
// RDF-specific routines used to build the model

nsresult
RDFContentSinkImpl::OpenRDF(const PRUnichar* aName)
{
    // ensure that we're actually reading RDF by making sure that the
    // opening tag is <rdf:RDF>, where "rdf:" corresponds to whatever
    // they've declared the standard RDF namespace to be.
    nsCOMPtr<nsIAtom> localName;
    const nsDependentSubstring& nameSpaceURI =
        SplitExpatName(aName, getter_AddRefs(localName));

    if (!nameSpaceURI.EqualsLiteral(RDF_NAMESPACE_URI) || localName != kRDFAtom) {
       // PR_LOG(gLog, PR_LOG_ALWAYS,
       //        ("rdfxml: expected RDF:RDF at line %d",
       //         aNode.GetSourceLineNumber()));

        return NS_ERROR_UNEXPECTED;
    }

    PushContext(nullptr, mState, mParseMode);
    mState = eRDFContentSinkState_InDocumentElement;
    return NS_OK;
}

nsresult
RDFContentSinkImpl::OpenObject(const PRUnichar* aName, 
                               const PRUnichar** aAttributes)
{
    // an "object" non-terminal is either a "description", a "typed
    // node", or a "container", so this change the content sink's
    // state appropriately.
    nsCOMPtr<nsIAtom> localName;
    const nsDependentSubstring& nameSpaceURI =
        SplitExpatName(aName, getter_AddRefs(localName));

    // Figure out the URI of this object, and create an RDF node for it.
    nsCOMPtr<nsIRDFResource> source;
    GetIdAboutAttribute(aAttributes, getter_AddRefs(source));

    // If there is no `ID' or `about', then there's not much we can do.
    if (! source)
        return NS_ERROR_FAILURE;

    // Push the element onto the context stack
    PushContext(source, mState, mParseMode);

    // Now figure out what kind of state transition we need to
    // make. We'll either be going into a mode where we parse a
    // description or a container.
    bool isaTypedNode = true;

    if (nameSpaceURI.EqualsLiteral(RDF_NAMESPACE_URI)) {
        isaTypedNode = false;

        if (localName == kDescriptionAtom) {
            // it's a description
            mState = eRDFContentSinkState_InDescriptionElement;
        }
        else if (localName == kBagAtom) {
            // it's a bag container
            InitContainer(kRDF_Bag, source);
            mState = eRDFContentSinkState_InContainerElement;
        }
        else if (localName == kSeqAtom) {
            // it's a seq container
            InitContainer(kRDF_Seq, source);
            mState = eRDFContentSinkState_InContainerElement;
        }
        else if (localName == kAltAtom) {
            // it's an alt container
            InitContainer(kRDF_Alt, source);
            mState = eRDFContentSinkState_InContainerElement;
        }
        else {
            // heh, that's not *in* the RDF namespace: just treat it
            // like a typed node
            isaTypedNode = true;
        }
    }

    if (isaTypedNode) {
        NS_ConvertUTF16toUTF8 typeStr(nameSpaceURI);
        typeStr.Append(nsAtomCString(localName));

        nsCOMPtr<nsIRDFResource> type;
        nsresult rv = gRDFService->GetResource(typeStr, getter_AddRefs(type));
        if (NS_FAILED(rv)) return rv;

        rv = mDataSource->Assert(source, kRDF_type, type, true);
        if (NS_FAILED(rv)) return rv;

        mState = eRDFContentSinkState_InDescriptionElement;
    }

    AddProperties(aAttributes, source);
    return NS_OK;
}

nsresult
RDFContentSinkImpl::OpenProperty(const PRUnichar* aName, const PRUnichar** aAttributes)
{
    nsresult rv;

    // an "object" non-terminal is either a "description", a "typed
    // node", or a "container", so this change the content sink's
    // state appropriately.
    nsCOMPtr<nsIAtom> localName;
    const nsDependentSubstring& nameSpaceURI =
        SplitExpatName(aName, getter_AddRefs(localName));

    NS_ConvertUTF16toUTF8 propertyStr(nameSpaceURI);
    propertyStr.Append(nsAtomCString(localName));

    nsCOMPtr<nsIRDFResource> property;
    rv = gRDFService->GetResource(propertyStr, getter_AddRefs(property));
    if (NS_FAILED(rv)) return rv;

    // See if they've specified a 'resource' attribute, in which case
    // they mean *that* to be the object of this property.
    nsCOMPtr<nsIRDFResource> target;
    GetResourceAttribute(aAttributes, getter_AddRefs(target));

    bool isAnonymous = false;

    if (! target) {
        // See if an 'ID' attribute has been specified, in which case
        // this corresponds to the fourth form of [6.12].

        // XXX strictly speaking, we should reject the RDF/XML as
        // invalid if they've specified both an 'ID' and a 'resource'
        // attribute. Bah.

        // XXX strictly speaking, 'about=' isn't allowed here, but
        // what the hell.
        GetIdAboutAttribute(aAttributes, getter_AddRefs(target), &isAnonymous);
    }

    if (target) {
        // They specified an inline resource for the value of this
        // property. Create an RDF resource for the inline resource
        // URI, add the properties to it, and attach the inline
        // resource to its parent.
        int32_t count;
        rv = AddProperties(aAttributes, target, &count);
        NS_ASSERTION(NS_SUCCEEDED(rv), "problem adding properties");
        if (NS_FAILED(rv)) return rv;

        if (count || !isAnonymous) {
            // If the resource was "anonymous" (i.e., they hadn't
            // explicitly set an ID or resource attribute), then we'll
            // only assert this property from the context element *if*
            // there were properties specified on the anonymous
            // resource.
            rv = mDataSource->Assert(GetContextElement(0), property, target, true);
            if (NS_FAILED(rv)) return rv;
        }

        // XXX Technically, we should _not_ fall through here and push
        // the element onto the stack: this is supposed to be a closed
        // node. But right now I'm lazy and the code will just Do The
        // Right Thing so long as the RDF is well-formed.
    }

    // Push the element onto the context stack and change state.
    PushContext(property, mState, mParseMode);
    mState = eRDFContentSinkState_InPropertyElement;
    SetParseMode(aAttributes);

    return NS_OK;
}

nsresult
RDFContentSinkImpl::OpenMember(const PRUnichar* aName, 
                               const PRUnichar** aAttributes)
{
    // ensure that we're actually reading a member element by making
    // sure that the opening tag is <rdf:li>, where "rdf:" corresponds
    // to whatever they've declared the standard RDF namespace to be.
    nsresult rv;

    nsCOMPtr<nsIAtom> localName;
    const nsDependentSubstring& nameSpaceURI =
        SplitExpatName(aName, getter_AddRefs(localName));

    if (!nameSpaceURI.EqualsLiteral(RDF_NAMESPACE_URI) ||
        localName != kLiAtom) {
        PR_LOG(gLog, PR_LOG_ALWAYS,
               ("rdfxml: expected RDF:li at line %d",
                -1)); // XXX pass in line number

        return NS_ERROR_UNEXPECTED;
    }

    // The parent element is the container.
    nsIRDFResource* container = GetContextElement(0);
    if (! container)
        return NS_ERROR_NULL_POINTER;

    nsIRDFResource* resource;
    if (NS_SUCCEEDED(rv = GetResourceAttribute(aAttributes, &resource))) {
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
    PushContext(nullptr, mState, mParseMode);
    mState = eRDFContentSinkState_InMemberElement;
    SetParseMode(aAttributes);

    return NS_OK;
}


nsresult
RDFContentSinkImpl::OpenValue(const PRUnichar* aName, const PRUnichar** aAttributes)
{
    // a "value" can either be an object or a string: we'll only get
    // *here* if it's an object, as raw text is added as a leaf.
    return OpenObject(aName,aAttributes);
}

////////////////////////////////////////////////////////////////////////
// namespace resolution
void
RDFContentSinkImpl::RegisterNamespaces(const PRUnichar **aAttributes)
{
    nsCOMPtr<nsIRDFXMLSink> sink = do_QueryInterface(mDataSource);
    if (!sink) {
        return;
    }
    NS_NAMED_LITERAL_STRING(xmlns, "http://www.w3.org/2000/xmlns/");
    for (; *aAttributes; aAttributes += 2) {
        // check the namespace
        const PRUnichar* attr = aAttributes[0];
        const PRUnichar* xmlnsP = xmlns.BeginReading();
        while (*attr ==  *xmlnsP) {
            ++attr;
            ++xmlnsP;
        }
        if (*attr != 0xFFFF ||
            xmlnsP != xmlns.EndReading()) {
            continue;
        }
        // get the localname (or "xmlns" for the default namespace)
        const PRUnichar* endLocal = ++attr;
        while (*endLocal && *endLocal != 0xFFFF) {
            ++endLocal;
        }
        nsDependentSubstring lname(attr, endLocal);
        nsCOMPtr<nsIAtom> preferred = do_GetAtom(lname);
        if (preferred == kXMLNSAtom) {
            preferred = nullptr;
        }
        sink->AddNameSpace(preferred, nsDependentString(aAttributes[1]));
    }
}

////////////////////////////////////////////////////////////////////////
// Qualified name resolution

const nsDependentSubstring
RDFContentSinkImpl::SplitExpatName(const PRUnichar *aExpatName,
                                   nsIAtom **aLocalName)
{
    /**
     *  Expat can send the following:
     *    localName
     *    namespaceURI<separator>localName
     *    namespaceURI<separator>localName<separator>prefix
     *
     *  and we use 0xFFFF for the <separator>.
     *
     */

    const PRUnichar *uriEnd = aExpatName;
    const PRUnichar *nameStart = aExpatName;
    const PRUnichar *pos;
    for (pos = aExpatName; *pos; ++pos) {
        if (*pos == 0xFFFF) {
            if (uriEnd != aExpatName) {
                break;
            }

            uriEnd = pos;
            nameStart = pos + 1;
        }
    }

    const nsDependentSubstring& nameSpaceURI = Substring(aExpatName, uriEnd);
    *aLocalName = NS_NewAtom(Substring(nameStart, pos)).get();
    return nameSpaceURI;
}

nsresult
RDFContentSinkImpl::InitContainer(nsIRDFResource* aContainerType, nsIRDFResource* aContainer)
{
    // Do the right kind of initialization based on the container
    // 'type' resource, and the state of the container (i.e., 'make' a
    // new container vs. 'reinitialize' the container).
    nsresult rv;

    static const ContainerInfo gContainerInfo[] = {
        { &RDFContentSinkImpl::kRDF_Alt, &nsIRDFContainerUtils::IsAlt, &nsIRDFContainerUtils::MakeAlt },
        { &RDFContentSinkImpl::kRDF_Bag, &nsIRDFContainerUtils::IsBag, &nsIRDFContainerUtils::MakeBag },
        { &RDFContentSinkImpl::kRDF_Seq, &nsIRDFContainerUtils::IsSeq, &nsIRDFContainerUtils::MakeSeq },
        { 0, 0, 0 },
    };

    for (const ContainerInfo* info = gContainerInfo; info->mType != 0; ++info) {
        if (*info->mType != aContainerType)
            continue;

        bool isContainer;
        rv = (gRDFContainerUtils->*(info->mTestFn))(mDataSource, aContainer, &isContainer);
        if (isContainer) {
            rv = ReinitContainer(aContainerType, aContainer);
        }
        else {
            rv = (gRDFContainerUtils->*(info->mMakeFn))(mDataSource, aContainer, nullptr);
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
    rv = mDataSource->GetTarget(aContainer, kRDF_nextVal, true, getter_AddRefs(nextval));
    if (NS_FAILED(rv)) return rv;

    rv = mDataSource->Change(aContainer, kRDF_nextVal, nextval, one);
    if (NS_FAILED(rv)) return rv;

    // Re-mark as a container. XXX should be kRDF_type
    rv = mDataSource->Assert(aContainer, kRDF_instanceOf, aContainerType, true);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to mark container as such");
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// Content stack management

nsIRDFResource* 
RDFContentSinkImpl::GetContextElement(int32_t ancestor /* = 0 */)
{
    if ((nullptr == mContextStack) ||
        (uint32_t(ancestor) >= mContextStack->Length())) {
        return nullptr;
    }

    return mContextStack->ElementAt(
           mContextStack->Length()-ancestor-1).mResource;
}

int32_t 
RDFContentSinkImpl::PushContext(nsIRDFResource         *aResource,
                                RDFContentSinkState     aState,
                                RDFContentSinkParseMode aParseMode)
{
    if (! mContextStack) {
        mContextStack = new nsAutoTArray<RDFContextStackElement, 8>();
        if (! mContextStack)
            return 0;
    }

    RDFContextStackElement* e = mContextStack->AppendElement();
    if (! e)
        return mContextStack->Length();

    e->mResource  = aResource;
    e->mState     = aState;
    e->mParseMode = aParseMode;
  
    return mContextStack->Length();
}
 
nsresult
RDFContentSinkImpl::PopContext(nsIRDFResource         *&aResource,
                               RDFContentSinkState     &aState,
                               RDFContentSinkParseMode &aParseMode)
{
    if ((nullptr == mContextStack) ||
        (mContextStack->IsEmpty())) {
        return NS_ERROR_NULL_POINTER;
    }

    uint32_t i = mContextStack->Length() - 1;
    RDFContextStackElement &e = mContextStack->ElementAt(i);

    aResource  = e.mResource;
    NS_IF_ADDREF(aResource);
    aState     = e.mState;
    aParseMode = e.mParseMode;

    mContextStack->RemoveElementAt(i);
    return NS_OK;
}
 

////////////////////////////////////////////////////////////////////////

nsresult
NS_NewRDFContentSink(nsIRDFContentSink** aResult)
{
    NS_PRECONDITION(aResult != nullptr, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    RDFContentSinkImpl* sink = new RDFContentSinkImpl();
    if (! sink)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(sink);
    *aResult = sink;
    return NS_OK;
}
