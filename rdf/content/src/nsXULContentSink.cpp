/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 *
 * Original Authors: Chris Waterson & David Hyatt
 *
 * Contributor(s): 
 */

/*

  An implementation for an NGLayout-style content sink that knows how
  to build an RDF content model from XUL.

  For each container tag, an RDF Sequence object is created that
  contains the order set of children of that container.

  For more information on XUL, see http://www.mozilla.org/xpfe

  TO DO
  -----

*/

#include "nsCOMPtr.h"
#include "nsForwardReference.h"
#include "nsICSSLoader.h"
#include "nsICSSParser.h"
#include "nsICSSStyleSheet.h"
#include "nsIContentSink.h"
#include "nsIDOMDocument.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMXULDocument.h"
#include "nsIDocument.h"
#include "nsIDocumentLoader.h"
#include "nsIFormControl.h"
#include "nsIHTMLContent.h"
#include "nsIHTMLContentContainer.h"
#include "nsINameSpace.h"
#include "nsINameSpaceManager.h"
#include "nsINodeInfo.h"
#include "nsIParser.h"
#include "nsIPresShell.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFContentModelBuilder.h"
#include "nsIRDFService.h"
#include "nsIScriptContext.h"
#include "nsIServiceManager.h"
#include "nsITextContent.h"
#include "nsIURL.h"
#include "nsIViewManager.h"
#include "nsIXMLContent.h"
#include "nsIXULContentSink.h"
#include "nsIXULContentUtils.h"
#include "nsIXULDocument.h"
#include "nsIXULPrototypeDocument.h"
#include "nsIXULPrototypeCache.h"
#include "nsLayoutCID.h"
#include "nsNetUtil.h"
#include "nsRDFCID.h"
#include "nsRDFParserUtils.h"
#include "nsVoidArray.h"
#include "nsWeakPtr.h"
#include "nsXPIDLString.h"
#include "nsXULElement.h"
#include "prlog.h"
#include "prmem.h"
#include "rdfutil.h"
#include "jsapi.h"  // for JSVERSION_*, JS_VersionToString, etc.

#include "nsHTMLTokens.h" // XXX so we can use nsIParserNode::GetTokenType()

static const char kNameSpaceSeparator = ':';
static const char kNameSpaceDef[] = "xmlns";
static const char kXULID[] = "id";

#ifdef PR_LOGGING
static PRLogModuleInfo* gLog;
#endif

// XXX This is sure to change. Copied from mozilla/layout/xul/content/src/nsXULAtoms.cpp
#define XUL_NAMESPACE_URI "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"
static const char kXULNameSpaceURI[] = XUL_NAMESPACE_URI;


static NS_DEFINE_CID(kCSSLoaderCID,              NS_CSS_LOADER_CID);
static NS_DEFINE_CID(kCSSParserCID,              NS_CSSPARSER_CID);
static NS_DEFINE_CID(kNameSpaceManagerCID,       NS_NAMESPACEMANAGER_CID);
static NS_DEFINE_CID(kXULContentUtilsCID,        NS_XULCONTENTUTILS_CID);
static NS_DEFINE_CID(kXULPrototypeCacheCID,      NS_XULPROTOTYPECACHE_CID);

//----------------------------------------------------------------------

class XULContentSinkImpl : public nsIXULContentSink
{
public:
    XULContentSinkImpl(nsresult& aRV);
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
    NS_IMETHOD AddDocTypeDecl(const nsIParserNode& aNode, PRInt32 aMode=0);
    NS_IMETHOD FlushPendingNotifications() { return NS_OK; }

    // nsIXMLContentSink
    NS_IMETHOD AddXMLDecl(const nsIParserNode& aNode);    
    NS_IMETHOD AddCharacterData(const nsIParserNode& aNode);
    NS_IMETHOD AddUnparsedEntity(const nsIParserNode& aNode);
    NS_IMETHOD AddNotation(const nsIParserNode& aNode);
    NS_IMETHOD AddEntityReference(const nsIParserNode& aNode);

    // nsIXULContentSink
    NS_IMETHOD Init(nsIDocument* aDocument, nsIXULPrototypeDocument* aPrototype);

protected:
    // pseudo-constants
    static nsrefcnt               gRefCnt;
    static nsINameSpaceManager*   gNameSpaceManager;
    static nsIXULContentUtils*    gXULUtils;
    static nsIXULPrototypeCache*  gXULCache;

    static nsIAtom* kClassAtom;
    static nsIAtom* kIdAtom;
    static nsIAtom* kScriptAtom;
    static nsIAtom* kStyleAtom;
    static nsIAtom* kTemplateAtom;

    static PRInt32 kNameSpaceID_XUL;

    // Text management
    nsresult FlushText(PRBool aCreateTextNode=PR_TRUE);
    static PRBool IsDataInBuffer(PRUnichar* aBuffer, PRInt32 aLength);

    PRUnichar* mText;
    PRInt32 mTextLength;
    PRInt32 mTextSize;
    PRBool mConstrainSize;

    // namespace management -- XXX combine with ContextStack?
    void PushNameSpacesFrom(const nsIParserNode& aNode);
    void PopNameSpaces(void);
    nsresult GetTopNameSpace(nsCOMPtr<nsINameSpace>* aNameSpace);

    nsVoidArray mNameSpaceStack;

    nsCOMPtr<nsINodeInfoManager> mNodeInfoManager;
    
    // RDF-specific parsing
    nsresult GetXULIDAttribute(const nsIParserNode& aNode, nsString& aID);
    nsresult AddAttributes(const nsIParserNode& aNode, nsXULPrototypeElement* aElement);
    nsresult ParseTag(const nsString& aText, nsINodeInfo*& aNodeInfo);
    nsresult NormalizeAttributeString(const nsAReadableString& aText,
                                      nsINodeInfo*& aNodeInfo);
    nsresult CreateElement(nsINodeInfo *aNodeInfo, nsXULPrototypeElement** aResult);

    nsresult OpenRoot(const nsIParserNode& aNode, nsINodeInfo *aNodeInfo);
    nsresult OpenTag(const nsIParserNode& aNode, nsINodeInfo *aNodeInfo);

    // Script tag handling
    nsresult OpenScript(const nsIParserNode& aNode);

    // Style sheets
    nsresult ProcessStyleLink(nsIContent* aElement,
                              const nsString& aHref,
                              PRBool aAlternate,
                              const nsString& aTitle,
                              const nsString& aType,
                              const nsString& aMedia);
    

    public:
    enum State { eInProlog, eInDocumentElement, eInScript, eInEpilog };
    protected:

    State mState;

    // content stack management
    class ContextStack {
    protected:
        struct Entry {
            nsXULPrototypeNode* mNode;
            nsVoidArray         mChildren;
            State               mState;
            Entry*              mNext;
        };

        Entry* mTop;
        PRInt32 mDepth;

    public:
        ContextStack();
        ~ContextStack();

        PRInt32 Depth() { return mDepth; }

        nsresult Push(nsXULPrototypeNode* aNode, State aState);
        nsresult Pop(State* aState);

        nsresult GetTopNode(nsXULPrototypeNode** aNode);
        nsresult GetTopChildren(nsVoidArray** aChildren);

        PRBool IsInsideXULTemplate();
    };

    friend class ContextStack;
    ContextStack mContextStack;

    nsWeakPtr              mDocument;             // [OWNER]
    nsCOMPtr<nsIURI>       mDocumentURL;          // [OWNER]

    nsCOMPtr<nsIXULPrototypeDocument> mPrototype; // [OWNER]
    nsIParser*             mParser;               // [OWNER] We use regular pointer b/c of funky exports on nsIParser
    
    nsString               mPreferredStyle;
    nsCOMPtr<nsICSSLoader> mCSSLoader;            // [OWNER]
    nsCOMPtr<nsICSSParser> mCSSParser;            // [OWNER]
};

nsrefcnt XULContentSinkImpl::gRefCnt;
nsINameSpaceManager* XULContentSinkImpl::gNameSpaceManager;
nsIXULContentUtils* XULContentSinkImpl::gXULUtils;
nsIXULPrototypeCache* XULContentSinkImpl::gXULCache;

nsIAtom* XULContentSinkImpl::kClassAtom;
nsIAtom* XULContentSinkImpl::kIdAtom;
nsIAtom* XULContentSinkImpl::kScriptAtom;
nsIAtom* XULContentSinkImpl::kStyleAtom;
nsIAtom* XULContentSinkImpl::kTemplateAtom;

PRInt32 XULContentSinkImpl::kNameSpaceID_XUL;

//----------------------------------------------------------------------

XULContentSinkImpl::ContextStack::ContextStack()
    : mTop(nsnull), mDepth(0)
{
}

XULContentSinkImpl::ContextStack::~ContextStack()
{
    while (mTop) {
        Entry* doomed = mTop;
        mTop = mTop->mNext;
        delete doomed;
    }
}

nsresult
XULContentSinkImpl::ContextStack::Push(nsXULPrototypeNode* aNode, State aState)
{
    Entry* entry = new Entry;
    if (! entry)
        return NS_ERROR_OUT_OF_MEMORY;

    entry->mNode  = aNode;
    entry->mState = aState;
    entry->mNext  = mTop;
    mTop = entry;

    ++mDepth;
    return NS_OK;
}

nsresult
XULContentSinkImpl::ContextStack::Pop(State* aState)
{
    if (mDepth == 0)
        return NS_ERROR_UNEXPECTED;

    Entry* entry = mTop;
    mTop = mTop->mNext;
    --mDepth;

    *aState = entry->mState;
    delete entry;

    return NS_OK;
}


nsresult
XULContentSinkImpl::ContextStack::GetTopNode(nsXULPrototypeNode** aNode)
{
    if (mDepth == 0)
        return NS_ERROR_UNEXPECTED;

    *aNode = mTop->mNode;
    return NS_OK;
}


nsresult
XULContentSinkImpl::ContextStack::GetTopChildren(nsVoidArray** aChildren)
{
    if (mDepth == 0)
        return NS_ERROR_UNEXPECTED;

    *aChildren = &(mTop->mChildren);
    return NS_OK;
}


PRBool
XULContentSinkImpl::ContextStack::IsInsideXULTemplate()
{
    if (mDepth) {
        Entry* entry = mTop;
        while (entry) {
            nsXULPrototypeNode* node = entry->mNode;

            if (node->mType == nsXULPrototypeNode::eType_Element) {
                nsXULPrototypeElement* element =
                    NS_REINTERPRET_CAST(nsXULPrototypeElement*, node);

                if (element->mNodeInfo->Equals(kTemplateAtom,
                                               kNameSpaceID_XUL)) {
                    return PR_TRUE;
                }
            }

            entry = entry->mNext;
        }
    }
    return PR_FALSE;
}


//----------------------------------------------------------------------


XULContentSinkImpl::XULContentSinkImpl(nsresult& rv)
    : mText(nsnull),
      mTextLength(0),
      mTextSize(0),
      mConstrainSize(PR_TRUE),
      mState(eInProlog),
      mParser(nsnull)
{
    NS_INIT_REFCNT();

    if (gRefCnt++ == 0) {
        rv = nsComponentManager::CreateInstance(kNameSpaceManagerCID,
                                                nsnull,
                                                NS_GET_IID(nsINameSpaceManager),
                                                (void**) &gNameSpaceManager);

        if (NS_FAILED(rv)) return;


        rv = gNameSpaceManager->RegisterNameSpace(NS_ConvertASCIItoUCS2(kXULNameSpaceURI), kNameSpaceID_XUL);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to register XUL namespace");
        if (NS_FAILED(rv)) return;

        kClassAtom          = NS_NewAtom("class");
        kIdAtom             = NS_NewAtom("id");
        kScriptAtom         = NS_NewAtom("script");
        kStyleAtom          = NS_NewAtom("style");
        kTemplateAtom       = NS_NewAtom("template");

        rv = nsServiceManager::GetService(kXULContentUtilsCID,
                                          NS_GET_IID(nsIXULContentUtils),
                                          (nsISupports**) &gXULUtils);
        if (NS_FAILED(rv)) return;

        rv = nsServiceManager::GetService(kXULPrototypeCacheCID,
                                          NS_GET_IID(nsIXULPrototypeCache),
                                          (nsISupports**) &gXULCache);
    }

#ifdef PR_LOGGING
    if (! gLog)
        gLog = PR_NewLogModule("nsXULContentSink");
#endif

    rv = NS_OK;
}


XULContentSinkImpl::~XULContentSinkImpl()
{
    NS_IF_RELEASE(mParser); // XXX should've been released by now, unless error.

    {
        // There shouldn't be any here except in an error condition
        PRInt32 i = mNameSpaceStack.Count();

        while (0 < i--) {
            nsINameSpace* nameSpace = (nsINameSpace*)mNameSpaceStack[i];

#ifdef PR_LOGGING
            if (PR_LOG_TEST(gLog, PR_LOG_ALWAYS)) {
                nsAutoString uri;
                nsCOMPtr<nsIAtom> prefixAtom;

                nameSpace->GetNameSpaceURI(uri);
                nameSpace->GetNameSpacePrefix(*getter_AddRefs(prefixAtom));

                nsAutoString prefix;
                if (prefixAtom)
                    {
                        const PRUnichar *unicodeString;
                        prefixAtom->GetUnicode(&unicodeString);
                        prefix = unicodeString;
                    }
                else
                    {
                        prefix.AssignWithConversion("<default>");
                    }

                char* prefixStr = prefix.ToNewCString();
                char* uriStr = uri.ToNewCString();

                PR_LOG(gLog, PR_LOG_ALWAYS,
                       ("xul: warning: unclosed namespace '%s' (%s)",
                        prefixStr, uriStr));

                nsCRT::free(prefixStr);
                nsCRT::free(uriStr);
            }
#endif

            NS_RELEASE(nameSpace);
        }
    }

    // Pop all of the elements off of the context stack, and delete
    // any remaining content elements. The context stack _should_ be
    // empty, unless something has gone wrong.
    while (mContextStack.Depth()) {
        nsresult rv;

        nsVoidArray* children;
        rv = mContextStack.GetTopChildren(&children);
        if (NS_SUCCEEDED(rv)) {
            for (PRInt32 i = children->Count() - 1; i >= 0; --i) {
                nsXULPrototypeNode* child =
                    NS_REINTERPRET_CAST(nsXULPrototypeNode*, children->ElementAt(i));

                delete child;
            }
        }

        nsXULPrototypeNode* node;
        rv = mContextStack.GetTopNode(&node);
        if (NS_SUCCEEDED(rv)) delete node;

        State state;
        mContextStack.Pop(&state);
    }

    PR_FREEIF(mText);

    if (--gRefCnt == 0) {
        NS_IF_RELEASE(gNameSpaceManager);

        NS_IF_RELEASE(kClassAtom);
        NS_IF_RELEASE(kIdAtom);
        NS_IF_RELEASE(kScriptAtom);
        NS_IF_RELEASE(kStyleAtom);
        NS_IF_RELEASE(kTemplateAtom);

        if (gXULUtils) {
            nsServiceManager::ReleaseService(kXULContentUtilsCID, gXULUtils);
            gXULUtils = nsnull;
        }

        if (gXULCache) {
            nsServiceManager::ReleaseService(kXULPrototypeCacheCID, gXULCache);
            gXULCache = nsnull;
        }
    }
}

//----------------------------------------------------------------------
// nsISupports interface

NS_IMPL_ISUPPORTS3(XULContentSinkImpl,
                   nsIXULContentSink,
                   nsIXMLContentSink,
                   nsIContentSink)

//----------------------------------------------------------------------
// nsIContentSink interface

NS_IMETHODIMP 
XULContentSinkImpl::WillBuildModel(void)
{
#if FIXME
    if (! mParentContentSink) {
        // If we're _not_ an overlay, then notify the document that
        // the load is beginning.
        mDocument->BeginLoad();
    }
#endif

    return NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::DidBuildModel(PRInt32 aQualityLevel)
{
#if FIXME
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
#endif

    nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocument);
    if (doc) {
        doc->EndLoad();
    }

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
    NS_IF_ADDREF(mParser);
    return NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::OpenContainer(const nsIParserNode& aNode)
{
    // XXX Hopefully the parser will flag this before we get here. If
    // we're in the epilog, there should be no new elements
    NS_PRECONDITION(mState != eInEpilog, "tag in XUL doc epilog");
    if (mState == eInEpilog)
        return NS_ERROR_UNEXPECTED;

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gLog, PR_LOG_DEBUG)) {
        const nsString& text = aNode.GetText();

        nsCAutoString extraWhiteSpace;
        PRInt32 count = mContextStack.Depth();
        while (--count >= 0)
            extraWhiteSpace += "  ";

        nsCAutoString textC;
        textC.AssignWithConversion(text);
        PR_LOG(gLog, PR_LOG_DEBUG,
               ("xul: %.5d. %s<%s>",
                aNode.GetSourceLineNumber(),
                NS_STATIC_CAST(const char*, extraWhiteSpace),
                NS_STATIC_CAST(const char*, textC)));
    }
#endif

    if (mState != eInScript) {
        FlushText();
    }

    // We must register namespace declarations found in the attribute
    // list of an element before creating the element. This is because
    // the namespace prefix for an element might be declared within
    // the attribute list.
    PushNameSpacesFrom(aNode);

    nsresult rv;

    nsCOMPtr<nsINodeInfo> nodeInfo;
    rv = ParseTag(aNode.GetText(), *getter_AddRefs(nodeInfo));
    if (NS_FAILED(rv)) {
#ifdef PR_LOGGING
        nsCAutoString anodeC;
        anodeC.AssignWithConversion(aNode.GetText());
        PR_LOG(gLog, PR_LOG_ALWAYS,
               ("xul: unrecognized namespace on '%s' at line %d",
                NS_STATIC_CAST(const char*,anodeC),
                aNode.GetSourceLineNumber()));
#endif

        return rv;
    }

    switch (mState) {
    case eInProlog:
        // We're the root document element
        rv = OpenRoot(aNode, nodeInfo);
        break;

    case eInDocumentElement:
        rv = OpenTag(aNode, nodeInfo);
        break;

    case eInEpilog:
    case eInScript:
        PR_LOG(gLog, PR_LOG_ALWAYS,
               ("xul: warning: unexpected tags in epilog at line %d",
                aNode.GetSourceLineNumber()));

        rv = NS_ERROR_UNEXPECTED; // XXX
        break;
    }

    return rv;
}

NS_IMETHODIMP 
XULContentSinkImpl::CloseContainer(const nsIParserNode& aNode)
{
    // Never EVER return anything but NS_OK or
    // NS_ERROR_HTMLPARSER_BLOCK from this method. Doing so will blow
    // the parser's little mind all over the planet.
    nsresult rv;

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gLog, PR_LOG_DEBUG)) {
        const nsString& text = aNode.GetText();

        nsCAutoString extraWhiteSpace;
        PRInt32 count = mContextStack.Depth();
        while (--count > 0)
            extraWhiteSpace += "  ";

        nsCAutoString textC;
        textC.AssignWithConversion(text);

        PR_LOG(gLog, PR_LOG_DEBUG,
               ("xul: %.5d. %s</%s>",
                aNode.GetSourceLineNumber(),
                NS_STATIC_CAST(const char*, extraWhiteSpace),
                NS_STATIC_CAST(const char*, textC)));
    }
#endif

    nsXULPrototypeNode* node;
    rv = mContextStack.GetTopNode(&node);

    if (NS_FAILED(rv)) {
#ifdef PR_LOGGING
        if (PR_LOG_TEST(gLog, PR_LOG_ALWAYS)) {
            char* tagStr = aNode.GetText().ToNewCString();
            PR_LOG(gLog, PR_LOG_ALWAYS,
                   ("xul: extra close tag '</%s>' detected at line %d\n",
                    tagStr, aNode.GetSourceLineNumber()));
            nsCRT::free(tagStr);
        }
#endif

        return NS_OK;
    }

    switch (node->mType) {
    case nsXULPrototypeNode::eType_Element: {
        // Flush any text _now_, so that we'll get text nodes created
        // before popping the stack.
        FlushText();

        // Pop the context stack and do prototype hookup.
        nsVoidArray* children;
        rv = mContextStack.GetTopChildren(&children);
        if (NS_FAILED(rv)) return rv;

        nsXULPrototypeElement* element =
            NS_REINTERPRET_CAST(nsXULPrototypeElement*, node);

        PRInt32 count = children->Count();
        if (count) {
            element->mChildren = new nsXULPrototypeNode*[count];
            if (! element->mChildren)
                return NS_ERROR_OUT_OF_MEMORY;

            for (PRInt32 i = count - 1; i >= 0; --i)
                element->mChildren[i] =
                    NS_REINTERPRET_CAST(nsXULPrototypeNode*, children->ElementAt(i));

            element->mNumChildren = count;
        }
    }
    break;

    case nsXULPrototypeNode::eType_Script: {
        nsXULPrototypeScript* script =
            NS_REINTERPRET_CAST(nsXULPrototypeScript*, node);

        // If given a src= attribute, we must ignore script tag content.
        if (! script->mSrcURI) {
            nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocument);

            rv = script->Compile(mText, mTextLength, mDocumentURL,
                                 script->mLineNo, doc, mPrototype);
        }

        FlushText(PR_FALSE);
    }
    break;

    default:
        NS_ERROR("didn't expect that");
        break;
    }

    rv = mContextStack.Pop(&mState);
    NS_ASSERTION(NS_SUCCEEDED(rv), "context stack corrupted");
    if (NS_FAILED(rv)) return rv;

    PopNameSpaces();

    if (mContextStack.Depth() == 0) {
        // The root element should -always- be an element, because
        // it'll have been created via XULContentSinkImpl::OpenRoot().
        NS_ASSERTION(node->mType == nsXULPrototypeNode::eType_Element, "root is not an element");
        if (node->mType != nsXULPrototypeNode::eType_Element)
            return NS_ERROR_UNEXPECTED;

        // Now that we're done parsing, set the prototype document's
        // root element. This transfers ownership of the prototype
        // element tree to the prototype document.
        nsXULPrototypeElement* element =
            NS_REINTERPRET_CAST(nsXULPrototypeElement*, node);

        rv = mPrototype->SetRootElement(element);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to set document root");
        if (NS_FAILED(rv)) return rv;

        mState = eInEpilog;
    }

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
    PR_LOG(gLog, PR_LOG_ALWAYS,
           ("xul: parser error '%s'", aError));

    return NS_OK;
}

// nsIXMLContentSink
NS_IMETHODIMP 
XULContentSinkImpl::AddXMLDecl(const nsIParserNode& aNode)
{
    // XXX We'll ignore it for now
    PR_LOG(gLog, PR_LOG_WARNING,
           ("xul: ignoring XML decl at line %d", aNode.GetSourceLineNumber()));

    return NS_OK;
}


NS_IMETHODIMP 
XULContentSinkImpl::AddComment(const nsIParserNode& aNode)
{
    FlushText();
    nsAutoString text;
    nsresult result = NS_OK;

    text.Assign( aNode.GetText() );

    // XXX add comment here...

    return result;
}

static void SplitMimeType(const nsString& aValue, nsString& aType, nsString& aParams)
{
  aType.Truncate();
  aParams.Truncate();
  PRInt32 semiIndex = aValue.FindChar(PRUnichar(';'));
  if (-1 != semiIndex) {
    aValue.Left(aType, semiIndex);
    aValue.Right(aParams, (aValue.Length() - semiIndex) - 1);
    aParams.StripWhitespace();
  }
  else {
    aType = aValue;
  }
  aType.StripWhitespace();
}


nsresult
XULContentSinkImpl::ProcessStyleLink(nsIContent* aElement,
                                     const nsString& aHref,
                                     PRBool aAlternate,
                                     const nsString& aTitle,
                                     const nsString& aType,
                                     const nsString& aMedia)
{
    static const char kCSSType[] = "text/css";

    nsresult rv = NS_OK;

    if (aAlternate) { // if alternate, does it have title?
        if (0 == aTitle.Length()) { // alternates must have title
            return NS_OK; //return without error, for now
        }
    }

    nsAutoString  mimeType;
    nsAutoString  params;
    SplitMimeType(aType, mimeType, params);

    if ((0 == mimeType.Length()) || mimeType.EqualsIgnoreCase(kCSSType)) {
        nsCOMPtr<nsIURI> url;
        rv = NS_NewURI(getter_AddRefs(url), aHref, mDocumentURL);
        if (NS_OK != rv) {
            return NS_OK; // The URL is bad, move along, don't propagate the error (for now)
        }

        // Add the style sheet reference to the prototype
        mPrototype->AddStyleSheetReference(url);

        // Nope, we need to load it asynchronously
        PRBool blockParser = PR_FALSE;
        if (! aAlternate) {
            if (0 < aTitle.Length()) {  // possibly preferred sheet
                if (0 == mPreferredStyle.Length()) {
                    mPreferredStyle = aTitle;
                    mCSSLoader->SetPreferredSheet(aTitle);
                    nsIAtom* defaultStyle = NS_NewAtom("default-style");
                    if (defaultStyle) {
                        mPrototype->SetHeaderData(defaultStyle, aTitle);
                        NS_RELEASE(defaultStyle);
                    }
                }
            }
            else {  // persistent sheet, block
                blockParser = PR_TRUE;
            }
        }

        nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocument);
        if (! doc)
            return NS_ERROR_FAILURE; // doc went away!

        PRBool doneLoading;
        rv = mCSSLoader->LoadStyleLink(aElement, url, aTitle, aMedia, kNameSpaceID_Unknown,
                                       doc->GetNumberOfStyleSheets(),
                                       ((blockParser) ? mParser : nsnull),
                                       doneLoading, nsnull);
        if (NS_SUCCEEDED(rv) && blockParser && (! doneLoading)) {
            rv = NS_ERROR_HTMLPARSER_BLOCK;
        }
    }

    return rv;
}


NS_IMETHODIMP 
XULContentSinkImpl::AddProcessingInstruction(const nsIParserNode& aNode)
{

    static const char kStyleSheetPI[] = "<?xml-stylesheet";
    static const char kOverlayPI[] = "<?xul-overlay";

    nsresult rv;
    FlushText();

    // XXX For now, we don't add the PI to the content model.
    // We just check for a style sheet PI
    const nsString& text = aNode.GetText();

    if (text.Find(kOverlayPI) == 0) {
        // Load a XUL overlay.
        nsAutoString href;
        rv = nsRDFParserUtils::GetQuotedAttributeValue(text, NS_ConvertASCIItoUCS2("href"), href);
        if (NS_FAILED(rv)) return rv;

        // If there was an error or there's no href, we can't do
        // anything with this PI
        if (0 == href.Length())
            return NS_OK;

        // Add the overlay to our list of overlays that need to be processed.
        nsCOMPtr<nsIURI> url;
        rv = NS_NewURI(getter_AddRefs(url), href, mDocumentURL);
        if (NS_FAILED(rv)) {
            return NS_OK; // The URL is bad, move along. Don't propogate for now.
        }

        rv = mPrototype->AddOverlayReference(url);
        if (NS_FAILED(rv)) return rv;
    }
    // If it's a stylesheet PI...
    else if (text.Find(kStyleSheetPI) == 0) {
        nsAutoString href;
        rv = nsRDFParserUtils::GetQuotedAttributeValue(text, NS_ConvertASCIItoUCS2("href"), href);
        if (NS_FAILED(rv)) return rv;

        // If there was an error or there's no href, we can't do
        // anything with this PI
        if (0 == href.Length())
            return NS_OK;

        nsAutoString type;
        rv = nsRDFParserUtils::GetQuotedAttributeValue(text, NS_ConvertASCIItoUCS2("type"), type);
        if (NS_FAILED(rv)) return rv;

        nsAutoString title;
        rv = nsRDFParserUtils::GetQuotedAttributeValue(text, NS_ConvertASCIItoUCS2("title"), title);
        if (NS_FAILED(rv)) return rv;

        title.CompressWhitespace();

        nsAutoString media;
        rv = nsRDFParserUtils::GetQuotedAttributeValue(text, NS_ConvertASCIItoUCS2("media"), media);
        if (NS_FAILED(rv)) return rv;

        media.ToLowerCase();

        nsAutoString alternate;
        rv = nsRDFParserUtils::GetQuotedAttributeValue(text, NS_ConvertASCIItoUCS2("alternate"), alternate);
        if (NS_FAILED(rv)) return rv;

        rv = ProcessStyleLink(nsnull /* XXX need a node here */,
                              href, alternate.EqualsWithConversion("yes"),  /* XXX ignore case? */
                              title, type, media);

        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::AddDocTypeDecl(const nsIParserNode& aNode, PRInt32 aMode)
{
    PR_LOG(gLog, PR_LOG_WARNING,
           ("xul: ignoring doctype decl at line %d", aNode.GetSourceLineNumber()));

    return NS_OK;
}


NS_IMETHODIMP 
XULContentSinkImpl::AddCharacterData(const nsIParserNode& aNode)
{
    nsAutoString text(aNode.GetText());

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
    PR_LOG(gLog, PR_LOG_WARNING,
           ("xul: ignoring unparsed entity at line %d", aNode.GetSourceLineNumber()));

    return NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::AddNotation(const nsIParserNode& aNode)
{
    PR_LOG(gLog, PR_LOG_WARNING,
           ("xul: ignoring notation at line %d", aNode.GetSourceLineNumber()));

    return NS_OK;
}

NS_IMETHODIMP 
XULContentSinkImpl::AddEntityReference(const nsIParserNode& aNode)
{
    PR_LOG(gLog, PR_LOG_WARNING,
           ("xul: ignoring entity reference at line %d", aNode.GetSourceLineNumber()));

    return NS_OK;
}

//----------------------------------------------------------------------
//
// nsIXULContentSink interface
//

NS_IMETHODIMP
XULContentSinkImpl::Init(nsIDocument* aDocument, nsIXULPrototypeDocument* aPrototype)
{
    NS_PRECONDITION(aDocument != nsnull, "null ptr");
    if (! aDocument)
        return NS_ERROR_NULL_POINTER;
    
    nsresult rv;

    mDocument    = getter_AddRefs(NS_GetWeakReference(aDocument));
    mPrototype   = aPrototype;

    rv = mPrototype->GetURI(getter_AddRefs(mDocumentURL));
    if (NS_FAILED(rv)) return rv;

    // XXX this presumes HTTP header info is already set in document
    // XXX if it isn't we need to set it here...
    nsCOMPtr<nsIAtom> defaultStyle = dont_AddRef( NS_NewAtom("default-style") );
    if (! defaultStyle)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = mPrototype->GetHeaderData(defaultStyle, mPreferredStyle);
    if (NS_FAILED(rv)) return rv;

    // Get the CSS loader from the document so we can load
    // stylesheets.
    nsCOMPtr<nsIHTMLContentContainer> htmlContainer = do_QueryInterface(aDocument);
    NS_ASSERTION(htmlContainer != nsnull, "not an HTML container");
    if (! htmlContainer)
        return NS_ERROR_UNEXPECTED;

    rv = htmlContainer->GetCSSLoader(*getter_AddRefs(mCSSLoader));
    if (NS_FAILED(rv)) return rv;

    rv = aDocument->GetNodeInfoManager(*getter_AddRefs(mNodeInfoManager));
    if (NS_FAILED(rv)) return rv;

    mState = eInProlog;
    return NS_OK;
}


//----------------------------------------------------------------------
//
// Text buffering
//

PRBool
XULContentSinkImpl::IsDataInBuffer(PRUnichar* buffer, PRInt32 length)
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
XULContentSinkImpl::FlushText(PRBool aCreateTextNode)
{
    nsresult rv;

    do {
        // Don't do anything if there's no text to create a node from, or
        // if they've told us not to create a text node
        if (! mTextLength)
            break;

        if (! aCreateTextNode)
            break;

        // Don't bother if there's nothing but whitespace.
        // XXX This could cause problems...
        if (! IsDataInBuffer(mText, mTextLength))
            break;

        // Don't bother if we're not in XUL document body
        if (mState != eInDocumentElement || mContextStack.Depth() == 0)
            break;

        // Trim the leading and trailing whitespace
        nsXULPrototypeText* text = new nsXULPrototypeText(/* line number */ -1);
        if (! text)
            return NS_ERROR_OUT_OF_MEMORY;

        text->mValue.SetCapacity(mTextLength + 1);
        text->mValue.Append(mText, mTextLength);
        text->mValue.Trim(" \t\n\r");

        // hook it up
        nsVoidArray* children;
        rv = mContextStack.GetTopChildren(&children);
        if (NS_FAILED(rv)) return rv;

        // transfer ownership of 'text' to the children array
        children->AppendElement(text);
    } while (0);

    // Reset our text buffer
    mTextLength = 0;
    return NS_OK;
}



//----------------------------------------------------------------------

nsresult
XULContentSinkImpl::GetXULIDAttribute(const nsIParserNode& aNode, nsString& aID)
{
    for (PRInt32 i = aNode.GetAttributeCount(); i >= 0; --i) {
        if (aNode.GetKeyAt(i).EqualsWithConversion("id")) {
            aID = aNode.GetValueAt(i);
            nsRDFParserUtils::StripAndConvert(aID);
            return NS_OK;
        }
    }

    // Otherwise, we couldn't find anything
    aID.Truncate();
    return NS_OK;
}

nsresult
XULContentSinkImpl::AddAttributes(const nsIParserNode& aNode, nsXULPrototypeElement* aElement)
{
    // Add tag attributes to the element
    nsresult rv;
    PRInt32 count = aNode.GetAttributeCount();

    PRBool generateIDAttr = PR_FALSE;
    if (mContextStack.IsInsideXULTemplate()) {
        // Check for an 'id' attribute.  If we're inside a XUL
        // template, then _everything_ needs to have an ID for the
        // 'template' attribute hookup.
        generateIDAttr = PR_TRUE;

        for (PRInt32 i = 0; i < count; i++) {
            if (aNode.GetKeyAt(i).EqualsWithConversion("id")) {
                generateIDAttr = PR_FALSE;
                break;
            }
        }
    }

    // Create storage for the attributes
    PRInt32 numattrs = count;
    if (generateIDAttr)
        ++numattrs;

    nsXULPrototypeAttribute* attrs = nsnull;
    if (numattrs > 0) {
      attrs = new nsXULPrototypeAttribute[numattrs];
      if (! attrs)
        return NS_ERROR_OUT_OF_MEMORY;
    }

    aElement->mAttributes    = attrs;
    aElement->mNumAttributes = numattrs;

    if (generateIDAttr) {
        // Deal with generating an ID attribute for stuff inside a XUL
        // template
        nsAutoString id; id.AssignWithConversion("$");
        id.AppendInt(PRInt32(aElement), 16);

        mNodeInfoManager->GetNodeInfo(kIdAtom, nsnull, kNameSpaceID_None,
                                      *getter_AddRefs(attrs->mNodeInfo));
        NS_ENSURE_TRUE(attrs->mNodeInfo, NS_ERROR_FAILURE);

        attrs->mValue.SetValue( id );

        ++attrs;
    }

    // Copy the attributes into the prototype
    for (PRInt32 i = 0; i < count; i++) {
        const nsString& qname = aNode.GetKeyAt(i);

        rv = NormalizeAttributeString(qname,
                                      *getter_AddRefs(attrs->mNodeInfo));

        if (NS_FAILED(rv)) {
            nsCAutoString qnameC;
            qnameC.AssignWithConversion(qname);
            PR_LOG(gLog, PR_LOG_ALWAYS,
                   ("xul: unable to parse attribute '%s' at line %d",
                    (const char*) qnameC, aNode.GetSourceLineNumber()));

            // Bring it. We'll just fail to copy an attribute that we
            // can't parse. And that's one less attribute to worry
            // about.
            --(aElement->mNumAttributes);
            continue;
        }

        nsAutoString    valueStr;
        valueStr = aNode.GetValueAt(i);

        nsRDFParserUtils::StripAndConvert( valueStr );
        attrs->mValue.SetValue( valueStr );

#ifdef PR_LOGGING
        if (PR_LOG_TEST(gLog, PR_LOG_DEBUG)) {
            nsCAutoString extraWhiteSpace;
            PRInt32 cnt = mContextStack.Depth();
            while (--cnt >= 0)
                extraWhiteSpace += "  ";
            nsCAutoString qnameC,valueC;
            qnameC.AssignWithConversion(qname);
            valueC.AssignWithConversion(valueStr);
            PR_LOG(gLog, PR_LOG_DEBUG,
                   ("xul: %.5d. %s    %s=%s",
                    aNode.GetSourceLineNumber(),
                    NS_STATIC_CAST(const char*, extraWhiteSpace),
                    NS_STATIC_CAST(const char*, qnameC),
                    NS_STATIC_CAST(const char*, valueC)));
        }
#endif

        ++attrs;
    }

    // XUL elements may require some additional work to compute
    // derived information.
    if (aElement->mNodeInfo->NamespaceEquals(kNameSpaceID_XUL)) {
        nsAutoString value;

        // Compute the element's class list if the element has a 'class' attribute.
        rv = aElement->GetAttribute(kNameSpaceID_None, kClassAtom, value);
        if (NS_FAILED(rv)) return rv;

        if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
            rv = nsClassList::ParseClasses(&aElement->mClassList, value);
            if (NS_FAILED(rv)) return rv;
        }

        // Parse the element's 'style' attribute
        rv = aElement->GetAttribute(kNameSpaceID_None, kStyleAtom, value);
        if (NS_FAILED(rv)) return rv;

        if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
            if (! mCSSParser) {
                rv = nsComponentManager::CreateInstance(kCSSParserCID,
                                                        nsnull,
                                                        NS_GET_IID(nsICSSParser),
                                                        getter_AddRefs(mCSSParser));

                if (NS_FAILED(rv)) return rv;
            }

            rv = mCSSParser->ParseDeclarations(value,
                                               mDocumentURL,
                                               *getter_AddRefs(aElement->mInlineStyleRule));

            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to parse style rule");
            if (NS_FAILED(rv)) return rv;
        }
    }

    return NS_OK;
}



nsresult
XULContentSinkImpl::ParseTag(const nsString& aText, nsINodeInfo*& aNodeInfo)
{
    nsresult rv;

    // Split the tag into prefix and tag substrings.
    nsAutoString prefixStr;
    nsAutoString tagStr(aText);
    PRInt32 nsoffset = tagStr.FindChar(kNameSpaceSeparator);
    if (nsoffset >= 0) {
        tagStr.Left(prefixStr, nsoffset);
        tagStr.Cut(0, nsoffset + 1);
    }

    nsCOMPtr<nsIAtom> prefix;
    if (prefixStr.Length() > 0) {
        prefix = dont_AddRef( NS_NewAtom(prefixStr) );
    }

    nsCOMPtr<nsINameSpace> ns;
    rv = GetTopNameSpace(&ns);
    if (NS_FAILED(rv)) return rv;

    PRInt32 namespaceID;
    rv = ns->FindNameSpaceID(prefix, namespaceID);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIAtom> tag(dont_AddRef(NS_NewAtom(tagStr)));
    if (! tag)
        return NS_ERROR_OUT_OF_MEMORY;

    return mNodeInfoManager->GetNodeInfo(tag, prefix, namespaceID, aNodeInfo);
}


nsresult
XULContentSinkImpl::NormalizeAttributeString(const nsAReadableString& aText,
                                             nsINodeInfo*& aNodeInfo)
{
    nsresult rv;
    PRInt32 nameSpaceID = kNameSpaceID_None;

    // Split the attribute into prefix and tag substrings.
    nsAutoString prefixStr;
    nsAutoString attrStr(aText);
    PRInt32 nsoffset = attrStr.FindChar(kNameSpaceSeparator);
    if (nsoffset >= 0) {
        attrStr.Left(prefixStr, nsoffset);
        attrStr.Cut(0, nsoffset + 1);
    }

    nsCOMPtr<nsIAtom> prefix;
    if (prefixStr.Length() > 0) {
        prefix = dont_AddRef( NS_NewAtom(prefixStr) );

        nsCOMPtr<nsINameSpace> ns;
        rv = GetTopNameSpace(&ns);
        if (NS_FAILED(rv)) return rv;

        rv = ns->FindNameSpaceID(prefix, nameSpaceID);
        if (NS_FAILED(rv)) return rv;
    }

    mNodeInfoManager->GetNodeInfo(attrStr, prefixStr, nameSpaceID, aNodeInfo);

    return NS_OK;
}


nsresult
XULContentSinkImpl::CreateElement(nsINodeInfo *aNodeInfo,
                                  nsXULPrototypeElement** aResult)
{
    nsresult rv;

    nsXULPrototypeElement* element = new nsXULPrototypeElement(/* line number */ -1);
    if (! element)
        return NS_ERROR_OUT_OF_MEMORY;

    element->mNodeInfo    = aNodeInfo;
    element->mDocument    = mPrototype;

    nsCOMPtr<nsINameSpace> ns;
    rv = GetTopNameSpace(&ns);
    if (NS_SUCCEEDED(rv)) {
        element->mNameSpace = ns;
    }

    *aResult = element;
    return NS_OK;
}



nsresult
XULContentSinkImpl::OpenRoot(const nsIParserNode& aNode, nsINodeInfo *aNodeInfo)
{
    NS_ASSERTION(mState == eInProlog, "how'd we get here?");
    if (mState != eInProlog)
        return NS_ERROR_UNEXPECTED;

    nsresult rv;

    if (aNodeInfo->Equals(kScriptAtom, kNameSpaceID_HTML) || 
        aNodeInfo->Equals(kScriptAtom, kNameSpaceID_XUL)) {
        PR_LOG(gLog, PR_LOG_ALWAYS,
               ("xul: script tag not allowed as root content element"));

        return NS_ERROR_UNEXPECTED;
    }

    // Create the element
    nsXULPrototypeElement* element;
    rv = CreateElement(aNodeInfo, &element);

    if (NS_FAILED(rv)) {
#ifdef PR_LOGGING
        nsCAutoString anodeC;
        anodeC.AssignWithConversion(aNode.GetText());
        PR_LOG(gLog, PR_LOG_ALWAYS,
               ("xul: unable to create element '%s' at line %d",
                NS_STATIC_CAST(const char*, anodeC),
                aNode.GetSourceLineNumber()));
#endif

        return rv;
    }

    // Push the element onto the context stack, so that child
    // containers will hook up to us as their parent.
    rv = mContextStack.Push(element, mState);
    if (NS_FAILED(rv)) {
        delete element;
        return rv;
    }

    // Add the attributes
    rv = AddAttributes(aNode, element);
    if (NS_FAILED(rv)) return rv;

    mState = eInDocumentElement;
    return NS_OK;
}


nsresult
XULContentSinkImpl::OpenTag(const nsIParserNode& aNode, nsINodeInfo *aNodeInfo)
{
    nsresult rv;

    if (aNodeInfo->Equals(kScriptAtom, kNameSpaceID_HTML) || 
        aNodeInfo->Equals(kScriptAtom, kNameSpaceID_XUL)) {
        // Oops, it's a script!
        return OpenScript(aNode);
    }

    // Create the element
    nsXULPrototypeElement* element;
    rv = CreateElement(aNodeInfo, &element);

    if (NS_FAILED(rv)) {
        nsCAutoString anodeC;
        anodeC.AssignWithConversion(aNode.GetText());
        PR_LOG(gLog, PR_LOG_ALWAYS,
               ("xul: unable to create element '%s' at line %d",
                NS_STATIC_CAST(const char*, anodeC),
                aNode.GetSourceLineNumber()));

        return rv;
    }

    // Link this element to its parent.
    nsVoidArray* children;
    rv = mContextStack.GetTopChildren(&children);
    if (NS_FAILED(rv)) {
        delete element;
        return rv;
    }

    children->AppendElement(element);

    // Push the element onto the context stack, so that child
    // containers will hook up to us as their parent.
    rv = mContextStack.Push(element, mState);
    if (NS_FAILED(rv)) return rv;

    // Add the attributes
    rv = AddAttributes(aNode, element);
    if (NS_FAILED(rv)) return rv;

    mState = eInDocumentElement;
    return NS_OK;
}


nsresult
XULContentSinkImpl::OpenScript(const nsIParserNode& aNode)
{
    nsresult rv = NS_OK;
    PRBool isJavaScript = PR_TRUE;
    const char* jsVersionString = nsnull;
    PRInt32 ac = aNode.GetAttributeCount();

    // Look for SRC attribute and look for a LANGUAGE attribute
    nsAutoString src;
    for (PRInt32 i = 0; i < ac; i++) {
        const nsString& key = aNode.GetKeyAt(i);
        if (key.EqualsIgnoreCase("src")) {
            src.Assign(aNode.GetValueAt(i));
            nsRDFParserUtils::StripAndConvert(src);
        }
        else if (key.EqualsIgnoreCase("type")) {
            nsAutoString  type(aNode.GetValueAt(i));
            nsRDFParserUtils::StripAndConvert(type);
            nsAutoString  mimeType;
            nsAutoString  params;
            SplitMimeType(type, mimeType, params);

            isJavaScript = mimeType.EqualsIgnoreCase("text/javascript");
            if (isJavaScript) {
                JSVersion jsVersion = JSVERSION_DEFAULT;
                if (params.Find("version=", PR_TRUE) == 0) {
                    if (params.Length() != 11 || params[8] != '1' || params[9] != '.')
                        jsVersion = JSVERSION_UNKNOWN;
                    else switch (params[10]) {
                        case '0': jsVersion = JSVERSION_1_0; break;
                        case '1': jsVersion = JSVERSION_1_1; break;
                        case '2': jsVersion = JSVERSION_1_2; break;
                        case '3': jsVersion = JSVERSION_1_3; break;
                        case '4': jsVersion = JSVERSION_1_4; break;
                        case '5': jsVersion = JSVERSION_1_5; break;
                        default:  jsVersion = JSVERSION_UNKNOWN;
                    }
                }
                jsVersionString = JS_VersionToString(jsVersion);
            }
        }
        else if (key.EqualsIgnoreCase("language")) {
            nsAutoString  lang(aNode.GetValueAt(i));
            nsRDFParserUtils::StripAndConvert(lang);
            isJavaScript = nsRDFParserUtils::IsJavaScriptLanguage(lang, &jsVersionString);
        }
    }
  
    // Don't process scripts that aren't JavaScript
    if (isJavaScript) {
        nsXULPrototypeScript* script =
            new nsXULPrototypeScript(aNode.GetSourceLineNumber(),
                                     jsVersionString);
        if (! script)
            return NS_ERROR_OUT_OF_MEMORY;

        nsVoidArray* children;
        rv = mContextStack.GetTopChildren(&children);
        if (NS_FAILED(rv)) {
            delete script;
            return rv;
        }

        children->AppendElement(script);

        // If there is a SRC attribute...
        if (src.Length() > 0) {
            // Use the SRC attribute value to load the URL
            nsCOMPtr<nsIURI> url;
            rv = NS_NewURI(getter_AddRefs(url), src, mDocumentURL);
            if (NS_FAILED(rv)) return rv;

            script->mSrcURI = url;
        }

        mConstrainSize = PR_FALSE;

        mContextStack.Push(script, mState);
        mState = eInScript;
    }

    return NS_OK;
}


//----------------------------------------------------------------------
//
// Namespace management
//

void
XULContentSinkImpl::PushNameSpacesFrom(const nsIParserNode& aNode)
{
    nsINameSpace* nameSpace = nsnull;
    if (0 < mNameSpaceStack.Count()) {
        nameSpace = (nsINameSpace*)mNameSpaceStack[mNameSpaceStack.Count() - 1];
        NS_ADDREF(nameSpace);
    }
    else {
        gNameSpaceManager->CreateRootNameSpace(nameSpace);
    }

    NS_ASSERTION(nameSpace != nsnull, "no parent namespace");
    if (! nameSpace)
        return;

    PRInt32 ac = aNode.GetAttributeCount();
    for (PRInt32 i = 0; i < ac; i++) {
        nsAutoString k(aNode.GetKeyAt(i));

        // Look for "xmlns" at the start of the attribute name
        PRInt32 offset = k.Find(kNameSpaceDef);
        if (0 != offset)
            continue;

        nsAutoString prefix;
        if (k.Length() >= PRInt32(sizeof kNameSpaceDef)) {
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
        nsAutoString uri(aNode.GetValueAt(i));
        nsRDFParserUtils::StripAndConvert(uri);

        // Open a local namespace
        nsIAtom* prefixAtom = ((0 < prefix.Length()) ? NS_NewAtom(prefix) : nsnull);
        nsINameSpace* child = nsnull;
        nameSpace->CreateChildNameSpace(prefixAtom, uri, child);
        if (nsnull != child) {
            NS_RELEASE(nameSpace);
            nameSpace = child;
        }

        NS_IF_RELEASE(prefixAtom);
    }

    // Now push the *last* namespace that we discovered on to the stack.
    mNameSpaceStack.AppendElement(nameSpace);
}

void
XULContentSinkImpl::PopNameSpaces(void)
{
    if (0 < mNameSpaceStack.Count()) {
        PRInt32 i = mNameSpaceStack.Count() - 1;
        nsINameSpace* nameSpace = (nsINameSpace*)mNameSpaceStack[i];
        mNameSpaceStack.RemoveElementAt(i);

        // Releasing the most deeply nested namespace will recursively
        // release intermediate parent namespaces until the next
        // reference is held on the namespace stack.
        NS_RELEASE(nameSpace);
    }
}

nsresult
XULContentSinkImpl::GetTopNameSpace(nsCOMPtr<nsINameSpace>* aNameSpace)
{
    PRInt32 count = mNameSpaceStack.Count();
    if (count == 0)
        return NS_ERROR_UNEXPECTED;

    *aNameSpace = dont_QueryInterface(NS_REINTERPRET_CAST(nsINameSpace*, mNameSpaceStack[count - 1]));
    return NS_OK;
}

//----------------------------------------------------------------------


nsresult
NS_NewXULContentSink(nsIXULContentSink** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;
    XULContentSinkImpl* sink = new XULContentSinkImpl(rv);
    if (! sink)
        return NS_ERROR_OUT_OF_MEMORY;

    if (NS_FAILED(rv)) {
        delete sink;
        return rv;
    }

    NS_ADDREF(sink);
    *aResult = sink;
    return NS_OK;
}
