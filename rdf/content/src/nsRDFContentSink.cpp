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
     from XML/HTML be factored?

  TO DO ------------------------

  1) Make sure all shortcut syntax works right.

  2) Implement default namespaces. Take a look at how nsIXMLDocument
     does namespaces: should we storing tag/namespace pairs instead of
     the entire URI in the elements?

  3) Write a test harness.

*/

#include "nsCRT.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFNode.h"
#include "nsIRDFResourceManager.h"
#include "nsRDFBaseCID.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsRDFContentSink.h"
#include "nsVoidArray.h"
#include "nsINameSpaceManager.h"
#include "nsINameSpace.h"
#include "prlog.h"
#include "prmem.h"
#include "rdfutil.h"

static const char kNameSpaceSeparator[] = ":";
static const char kNameSpaceDef[] = "xmlns";

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
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, type);

static const char kRDFNameSpaceURI[] = RDF_NAMESPACE_URI;

////////////////////////////////////////////////////////////////////////
// XPCOM IIDs

static NS_DEFINE_IID(kIContentSinkIID,         NS_ICONTENT_SINK_IID); // XXX grr...
static NS_DEFINE_IID(kIRDFDataSourceIID,       NS_IRDFDATASOURCE_IID);
static NS_DEFINE_IID(kIRDFResourceManagerIID,  NS_IRDFRESOURCEMANAGER_IID);
static NS_DEFINE_IID(kISupportsIID,            NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIXMLContentSinkIID,      NS_IXMLCONTENT_SINK_IID);
static NS_DEFINE_IID(kIRDFContentSinkIID,      NS_IRDFCONTENTSINK_IID);

static NS_DEFINE_CID(kRDFResourceManagerCID,   NS_RDFRESOURCEMANAGER_CID);
static NS_DEFINE_CID(kRDFMemoryDataSourceCID,  NS_RDFMEMORYDATASOURCE_CID);

////////////////////////////////////////////////////////////////////////
// Utility routines

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
#if 0
                ch = NS_EntityToUnicode(cbuf);
#endif
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
    if (NS_SUCCEEDED(NS_NewURL(&url, spec, base))) {
        PRUnichar* str;
        url->ToString(&str);
        spec = str;
        delete str;
        url->Release();
    }
}

////////////////////////////////////////////////////////////////////////

nsRDFContentSink::nsRDFContentSink()
    : mDocumentURL(nsnull),
      mResourceMgr(nsnull),
      mDataSource(nsnull),
      mGenSym(0),
      mNameSpaceManager(nsnull),
      mNameSpaceStack(nsnull),
      mRDFNameSpaceID(kNameSpaceID_Unknown),
      mContextStack(nsnull),
      mText(nsnull),
      mTextLength(0),
      mTextSize(0),
      mConstrainSize(PR_TRUE)
{
    NS_INIT_REFCNT();
}


nsRDFContentSink::~nsRDFContentSink()
{
    NS_IF_RELEASE(mDocumentURL);

    if (mResourceMgr)
        nsServiceManager::ReleaseService(kRDFResourceManagerCID, mResourceMgr);

    NS_IF_RELEASE(mDataSource);

    NS_IF_RELEASE(mNameSpaceManager);
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
}

////////////////////////////////////////////////////////////////////////

nsresult
nsRDFContentSink::Init(nsIURL* aURL, nsINameSpaceManager* aNameSpaceManager)
{
    NS_PRECONDITION((nsnull != aURL) && (nsnull != aNameSpaceManager), "null ptr");
    if ((! aURL) || (! aNameSpaceManager))
        return NS_ERROR_NULL_POINTER;

    mDocumentURL = aURL;
    NS_ADDREF(aURL);

    mNameSpaceManager = aNameSpaceManager;
    NS_ADDREF(mNameSpaceManager);

    nsresult rv;
    if (NS_FAILED(rv = nsServiceManager::GetService(kRDFResourceManagerCID,
                                                    kIRDFResourceManagerIID,
                                                    (nsISupports**) &mResourceMgr)))
        return rv;

    mState = eRDFContentSinkState_InProlog;
    return NS_OK;
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
nsRDFContentSink::WillBuildModel(void)
{
    return NS_OK;
}

NS_IMETHODIMP 
nsRDFContentSink::DidBuildModel(PRInt32 aQualityLevel)
{
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
nsRDFContentSink::SetParser(nsIParser* aParser)
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
    PushNameSpacesFrom(aNode);

    nsresult rv;

    RDFContentSinkState lastState = mState;
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

    return rv;
}

NS_IMETHODIMP 
nsRDFContentSink::CloseContainer(const nsIParserNode& aNode)
{
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
    nsresult result = NS_OK;

    text = aNode.GetText();

    // XXX add comment here...

    return result;
}


NS_IMETHODIMP 
nsRDFContentSink::AddProcessingInstruction(const nsIParserNode& aNode)
{
    FlushText();
    return NS_OK;
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
// nsIRDFContentSink interface

NS_IMETHODIMP
nsRDFContentSink::SetDataSource(nsIRDFDataSource* ds)
{
    NS_IF_RELEASE(mDataSource);
    mDataSource = ds;
    NS_IF_ADDREF(mDataSource);
    return NS_OK;
}


NS_IMETHODIMP
nsRDFContentSink::GetDataSource(nsIRDFDataSource*& ds)
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
nsRDFContentSink::FlushText(PRBool aCreateTextNode, PRBool* aDidFlush)
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

                nsIRDFLiteral* literal;
                if (NS_SUCCEEDED(rv = mResourceMgr->GetLiteral(value, &literal))) {
                    rv = rdf_ContainerAddElement(mResourceMgr,
                                                 mDataSource,
                                                 GetContextElement(0),
                                                 literal);
                    NS_RELEASE(literal);
                }
            } break;

            case eRDFContentSinkState_InPropertyElement: {
                nsAutoString value;
                value.Append(mText, mTextLength);

                rv = rdf_Assert(mResourceMgr,
                                mDataSource,
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
nsRDFContentSink::SplitQualifiedName(const nsString& aQualifiedName,
                                     PRInt32& rNameSpaceID,
                                     nsString& rProperty)
{
    rProperty = aQualifiedName;
    nsIAtom* prefix = CutNameSpacePrefix(rProperty);
    rNameSpaceID = GetNameSpaceID(prefix);
    NS_IF_RELEASE(prefix);
}


nsresult
nsRDFContentSink::GetIdAboutAttribute(const nsIParserNode& aNode,
                                      nsString& rResource)
{
    // This corresponds to the dirty work of production [6.5]
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

        if (attr.Equals(kTagRDF_about)) {
            rResource = aNode.GetValueAt(i);
            rdf_StripAndConvert(rResource);

            return NS_OK;
        }

        if (attr.Equals(kTagRDF_ID)) {
            PRUnichar* str;
            mDocumentURL->ToString(&str);
            rResource = str;
            delete str;
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
    PRUnichar* str;
    mDocumentURL->ToString(&str);
    rResource = str;
    delete str;
    rResource.Append("#anonymous$");
    rResource.Append(mGenSym++, 10);
    return NS_OK;
}


nsresult
nsRDFContentSink::GetResourceAttribute(const nsIParserNode& aNode,
                                       nsString& rResource)
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
        rdf_StripAndConvert(v);

        GetNameSpaceURI(nameSpaceID, k);
        k.Append(attr);

        // Add the attribute to RDF
        rdf_Assert(mResourceMgr, mDataSource, aSubject, k, v);
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
nsRDFContentSink::OpenObject(const nsIParserNode& aNode)
{
    // an "object" non-terminal is either a "description", a "typed
    // node", or a "container", so this change the content sink's
    // state appropriately.

    if (! mResourceMgr)
        return NS_ERROR_NOT_INITIALIZED;

    nsAutoString tag;
    PRInt32 nameSpaceID;

    SplitQualifiedName(aNode.GetText(), nameSpaceID, tag);

    // Figure out the URI of this object, and create an RDF node for it.
    nsresult rv;
    nsAutoString uri;
    if (NS_FAILED(rv = GetIdAboutAttribute(aNode, uri)))
        return rv;

    nsIRDFResource* rdfResource;
    if (NS_FAILED(rv = mResourceMgr->GetUnicodeResource(uri, &rdfResource)))
        return rv;

    // If we're in a member or property element, then this is the cue
    // that we need to hook the object up into the graph via the
    // member/property.
    switch (mState) {
    case eRDFContentSinkState_InMemberElement: {
        rdf_ContainerAddElement(mResourceMgr, mDataSource, GetContextElement(0), rdfResource);
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
            rdf_MakeBag(mResourceMgr, mDataSource, rdfResource);
            mState = eRDFContentSinkState_InContainerElement;
        }
        else if (tag.Equals(kTagRDF_Seq)) {
            // it's a seq container
            rdf_MakeSeq(mResourceMgr, mDataSource, rdfResource);
            mState = eRDFContentSinkState_InContainerElement;
        }
        else if (tag.Equals(kTagRDF_Alt)) {
            // it's an alt container
            rdf_MakeAlt(mResourceMgr, mDataSource, rdfResource);
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
        rdf_Assert(mResourceMgr, mDataSource, rdfResource, kURIRDF_type, nameSpace);

        mState = eRDFContentSinkState_InDescriptionElement;
    }

    AddProperties(aNode, rdfResource);
    NS_RELEASE(rdfResource);

    return NS_OK;
}


nsresult
nsRDFContentSink::OpenProperty(const nsIParserNode& aNode)
{
    if (! mResourceMgr)
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
    if (NS_FAILED(rv = mResourceMgr->GetUnicodeResource(ns, &rdfProperty)))
        return rv;

    nsAutoString resourceURI;
    if (NS_SUCCEEDED(GetResourceAttribute(aNode, resourceURI))) {
        // They specified an inline resource for the value of this
        // property. Create an RDF resource for the inline resource
        // URI, add the properties to it, and attach the inline
        // resource to its parent.
        nsIRDFResource* rdfResource;
        if (NS_SUCCEEDED(rv = mResourceMgr->GetUnicodeResource(resourceURI, &rdfResource))) {
            if (NS_SUCCEEDED(rv = AddProperties(aNode, rdfResource))) {
                rv = rdf_Assert(mResourceMgr,
                                mDataSource,
                                GetContextElement(0),
                                rdfProperty,
                                rdfResource);
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
    nsAutoString resourceURI;
    if (NS_SUCCEEDED(rv = GetResourceAttribute(aNode, resourceURI))) {
        // Okay, this node has an RDF:resource="..." attribute. That
        // means that it's a "referenced item," as covered in [6.29].

        nsIRDFResource* resource;
        if (NS_SUCCEEDED(rv = mResourceMgr->GetUnicodeResource(resourceURI, &resource))) {
            rv = rdf_ContainerAddElement(mResourceMgr, mDataSource, container, resource);
        }

        // XXX Technically, we should _not_ fall through here and push
        // the element onto the stack: this is supposed to be a closed
        // node. But right now I'm lazy and the code will just Do The
        // Right Thing so long as the RDF is well-formed.
    }

    // Change state.
    mState = eRDFContentSinkState_InMemberElement;
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
// Content stack management

struct RDFContextStackElement {
    nsIRDFResource*     mResource;
    RDFContentSinkState mState;
};

nsIRDFResource* 
nsRDFContentSink::GetContextElement(PRInt32 ancestor /* = 0 */)
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
nsRDFContentSink::PushContext(nsIRDFResource *aResource, RDFContentSinkState aState)
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
nsRDFContentSink::PopContext(nsIRDFResource*& rResource, RDFContentSinkState& rState)
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
nsRDFContentSink::PushNameSpacesFrom(const nsIParserNode& aNode)
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
                PRUnichar next = k.CharAt(sizeof(kNameSpaceDef)-1);
                // If the next character is a :, there is a namespace prefix
                if (':' == next) {
                    k.Right(prefix, k.Length()-sizeof(kNameSpaceDef));
                }
                else {
                    prefix.Truncate();
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
                NS_IF_RELEASE(prefixAtom);
            }
        }
        if (nsnull == mNameSpaceStack) {
            mNameSpaceStack = new nsVoidArray();
        }
        mNameSpaceStack->AppendElement(nameSpace);
    }
}

nsIAtom* 
nsRDFContentSink::CutNameSpacePrefix(nsString& aString)
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
nsRDFContentSink::GetNameSpaceID(nsIAtom* aPrefix)
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
nsRDFContentSink::GetNameSpaceURI(PRInt32 aID, nsString& aURI)
{
  mNameSpaceManager->GetNameSpaceURI(aID, aURI);
}

void
nsRDFContentSink::PopNameSpaces()
{
  if ((nsnull != mNameSpaceStack) && (0 < mNameSpaceStack->Count())) {
    PRInt32 index = mNameSpaceStack->Count() - 1;
    nsINameSpace* nameSpace = (nsINameSpace*)mNameSpaceStack->ElementAt(index);
    mNameSpaceStack->RemoveElementAt(index);
    NS_RELEASE(nameSpace);
  }
}

