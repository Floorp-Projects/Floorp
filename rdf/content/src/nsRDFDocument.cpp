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

#include "nsIArena.h"
#include "nsIContent.h"
#include "nsIDTD.h"
#include "nsIDocumentObserver.h"
#include "nsIHTMLStyleSheet.h"
#include "nsIParser.h"
#include "nsIRDFContent.h"
#include "nsIRDFCursor.h"
#include "nsIRDFDataBase.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFNode.h"
#include "nsIRDFService.h"
#include "nsIPresShell.h"
#include "nsIScriptContextOwner.h"
#include "nsIServiceManager.h"
#include "nsINameSpaceManager.h"
#include "nsISupportsArray.h"
#include "nsICollection.h"
#include "nsIStreamListener.h"
#include "nsIStyleSet.h"
#include "nsIStyleSheet.h"
#include "nsIURL.h"
#include "nsIURLGroup.h"
#include "nsIWebShell.h"
#include "nsLayoutCID.h"
#include "nsParserCIID.h"
#include "nsRDFCID.h"
#include "nsRDFContentSink.h"
#include "nsRDFDocument.h"
#include "nsITextContent.h"
#include "nsIDTD.h"
#include "nsLayoutCID.h"
#include "rdfutil.h"

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kICollectionIID,         NS_ICOLLECTION_IID);
static NS_DEFINE_IID(kIContentIID,            NS_ICONTENT_IID);
static NS_DEFINE_IID(kIDTDIID,                NS_IDTD_IID);
static NS_DEFINE_IID(kIDocumentIID,           NS_IDOCUMENT_IID);
static NS_DEFINE_IID(kIHTMLStyleSheetIID,     NS_IHTML_STYLE_SHEET_IID);
static NS_DEFINE_IID(kINameSpaceManagerIID,   NS_INAMESPACEMANAGER_IID);
static NS_DEFINE_IID(kIParserIID,             NS_IPARSER_IID);
static NS_DEFINE_IID(kIPresShellIID,          NS_IPRESSHELL_IID);
static NS_DEFINE_IID(kIRDFDataBaseIID,        NS_IRDFDATABASE_IID);
static NS_DEFINE_IID(kIRDFDataSourceIID,      NS_IRDFDATASOURCE_IID);
static NS_DEFINE_IID(kIRDFDocumentIID,        NS_IRDFDOCUMENT_IID);
static NS_DEFINE_IID(kIRDFLiteralIID,         NS_IRDFLITERAL_IID);
static NS_DEFINE_IID(kIRDFResourceIID,        NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kIRDFServiceIID,         NS_IRDFSERVICE_IID);
static NS_DEFINE_IID(kIStreamListenerIID,     NS_ISTREAMLISTENER_IID);
static NS_DEFINE_IID(kISupportsIID,           NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kITextContentIID,        NS_ITEXT_CONTENT_IID); // XXX grr...
static NS_DEFINE_IID(kIWebShellIID,           NS_IWEB_SHELL_IID);
static NS_DEFINE_IID(kIXMLDocumentIID,        NS_IXMLDOCUMENT_IID);

static NS_DEFINE_CID(kHTMLStyleSheetCID,        NS_HTMLSTYLESHEET_CID);
static NS_DEFINE_CID(kNameSpaceManagerCID,      NS_NAMESPACEMANAGER_CID);
static NS_DEFINE_CID(kParserCID,                NS_PARSER_IID); // XXX
static NS_DEFINE_CID(kPresShellCID,             NS_PRESSHELL_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID, NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_CID(kRDFServiceCID,            NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFDataBaseCID,           NS_RDFDATABASE_CID);
static NS_DEFINE_CID(kRangeListCID,             NS_RANGELIST_CID);
static NS_DEFINE_CID(kTextNodeCID,              NS_TEXTNODE_CID);
static NS_DEFINE_CID(kWellFormedDTDCID,         NS_WELLFORMEDDTD_CID);

////////////////////////////////////////////////////////////////////////

nsRDFDocument::nsRDFDocument()
    : mArena(nsnull),
      mDocumentURL(nsnull),
      mDocumentURLGroup(nsnull),
      mRootContent(nsnull),
      mParentDocument(nsnull),
      mScriptContextOwner(nsnull),
      mSelection(nsnull),
      mDisplaySelection(PR_FALSE),
      mNameSpaceManager(nsnull),
      mAttrStyleSheet(nsnull),
      mParser(nsnull),
      mDB(nsnull),
      mRDFService(nsnull),
      mTreeProperties(nsnull)
{
    NS_INIT_REFCNT();

    nsresult rv;

    // construct a selection object
    if (NS_FAILED(rv = nsRepository::CreateInstance(kRangeListCID,
                                                    nsnull,
                                                    kICollectionIID,
                                                    (void**) &mSelection)))
        PR_ASSERT(0);

    
}

nsRDFDocument::~nsRDFDocument()
{
    NS_IF_RELEASE(mParser);

    if (mRDFService) {
        nsServiceManager::ReleaseService(kRDFServiceCID, mRDFService);
        mRDFService = nsnull;
    }

    // mParentDocument is never refcounted
    NS_IF_RELEASE(mSelection);
    NS_IF_RELEASE(mScriptContextOwner);
    NS_IF_RELEASE(mAttrStyleSheet);
    NS_IF_RELEASE(mTreeProperties);
    NS_IF_RELEASE(mRootContent);
    NS_IF_RELEASE(mDB);
    NS_IF_RELEASE(mDocumentURLGroup);
    NS_IF_RELEASE(mDocumentURL);
    NS_IF_RELEASE(mArena);
    NS_IF_RELEASE(mNameSpaceManager);
}

NS_IMETHODIMP 
nsRDFDocument::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    *result = nsnull;
    if (iid.Equals(kIDocumentIID) ||
        iid.Equals(kISupportsIID)) {
        *result = NS_STATIC_CAST(nsIDocument*, this);
        AddRef();
        return NS_OK;
    }
    else if (iid.Equals(kIRDFDocumentIID) ||
             iid.Equals(kIXMLDocumentIID)) {
        *result = NS_STATIC_CAST(nsIRDFDocument*, this);
        AddRef();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsRDFDocument);
NS_IMPL_RELEASE(nsRDFDocument);

////////////////////////////////////////////////////////////////////////
// nsIDocument interface

nsIArena*
nsRDFDocument::GetArena()
{
    NS_IF_ADDREF(mArena);
    return mArena;
}

NS_IMETHODIMP 
nsRDFDocument::StartDocumentLoad(nsIURL *aURL, 
                                 nsIContentViewerContainer* aContainer,
                                 nsIStreamListener **aDocListener,
                                 const char* aCommand)
{
    nsresult rv;

    // Delete references to style sheets - this should be done in superclass...
    PRInt32 index = mStyleSheets.Count();
    while (--index >= 0) {
        nsIStyleSheet* sheet = (nsIStyleSheet*) mStyleSheets.ElementAt(index);
        sheet->SetOwningDocument(nsnull);
        NS_RELEASE(sheet);
    }
    mStyleSheets.Clear();

    NS_IF_RELEASE(mDocumentURL);
    NS_IF_RELEASE(mDocumentURLGroup);
    mDocumentTitle.Truncate();

    mDocumentURL = aURL;
    NS_ADDREF(aURL);

    (void)aURL->GetURLGroup(&mDocumentURLGroup);

    rv = nsRepository::CreateInstance(kParserCID, 
                                      nsnull,
                                      kIParserIID, 
                                      (void**) &mParser);
    PR_ASSERT(NS_SUCCEEDED(rv));
    if (NS_FAILED(rv))
        return rv;

    nsIWebShell* webShell = nsnull;
    nsIRDFContentSink* sink = nsnull;
    nsIRDFDataSource* ds = nsnull;
    nsIHTMLStyleSheet* sheet = nsnull;
    nsIDTD* dtd = nsnull;

    if (NS_FAILED(rv = Init())) {
        PR_ASSERT(0);
        goto done;
    }

    if (NS_FAILED(rv = aContainer->QueryInterface(kIWebShellIID, (void**)&webShell))) {
        PR_ASSERT(0);
        goto done;
    }

    if (NS_FAILED(rv = NS_NewRDFDocumentContentSink(&sink, this, aURL, webShell))) {
        PR_ASSERT(0);
        goto done;
    }

    NS_RELEASE(webShell);

    if (NS_FAILED(rv = nsRepository::CreateInstance(kRDFInMemoryDataSourceCID,
                                                    nsnull,
                                                    kIRDFDataSourceIID,
                                                    (void**) &ds))) {
        PR_ASSERT(0);
        goto done;
    }

    if (NS_FAILED(rv = sink->SetDataSource(ds))) {
        PR_ASSERT(0);
        goto done;
    }

    if (NS_FAILED(rv = mDB->AddDataSource(ds))) {
        PR_ASSERT(0);
        goto done;
    }

    // Create an HTML style sheet for the HTML content.
    if (NS_FAILED(rv = nsRepository::CreateInstance(kHTMLStyleSheetCID,
                                                    nsnull,
                                                    kIHTMLStyleSheetIID,
                                                    (void**) &sheet))) {
        PR_ASSERT(0);
        goto done;
    }

    if (NS_FAILED(rv = sheet->Init(aURL, this))) {
        PR_ASSERT(0);
        goto done;
    }

    mAttrStyleSheet = sheet;
    NS_ADDREF(mAttrStyleSheet);

    AddStyleSheet(mAttrStyleSheet);
      
    // Set the parser as the stream listener for the document loader...
    if (NS_FAILED(rv = mParser->QueryInterface(kIStreamListenerIID, (void**)aDocListener))) {
        PR_ASSERT(0);
        goto done;
    }

    if (NS_FAILED(rv = nsRepository::CreateInstance(kWellFormedDTDCID,
                                                    nsnull,
                                                    kIDTDIID,
                                                    (void**) &dtd))) {
        PR_ASSERT(0);
        goto done;
    }

    mParser->RegisterDTD(dtd);
    mParser->SetCommand(aCommand);
    mParser->SetContentSink(sink);

    if (NS_FAILED(rv = mParser->Parse(aURL))) {
        PR_ASSERT(0);
        goto done;
    }

done:
    NS_IF_RELEASE(dtd);
    NS_IF_RELEASE(sheet);
    NS_IF_RELEASE(ds);
    NS_IF_RELEASE(sink);
    NS_IF_RELEASE(webShell);
    return rv;
}

const nsString*
nsRDFDocument::GetDocumentTitle() const
{
    return &mDocumentTitle;
}

nsIURL* 
nsRDFDocument::GetDocumentURL() const
{
    NS_IF_ADDREF(mDocumentURL);
    return mDocumentURL;
}

nsIURLGroup* 
nsRDFDocument::GetDocumentURLGroup() const
{
    NS_IF_ADDREF(mDocumentURLGroup);
    return mDocumentURLGroup;
}

nsCharSetID 
nsRDFDocument::GetDocumentCharacterSet() const
{
    return mCharSetID;
}

void 
nsRDFDocument::SetDocumentCharacterSet(nsCharSetID aCharSetID)
{
    mCharSetID = aCharSetID;
}

nsresult 
nsRDFDocument::CreateShell(nsIPresContext* aContext,
                           nsIViewManager* aViewManager,
                           nsIStyleSet* aStyleSet,
                           nsIPresShell** aInstancePtrResult)
{
    NS_PRECONDITION(aInstancePtrResult, "null ptr");
    if (! aInstancePtrResult)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    nsIPresShell* shell;
    if (NS_FAILED(rv = nsRepository::CreateInstance(kPresShellCID,
                                                    nsnull,
                                                    kIPresShellIID,
                                                    (void**) &shell)))
        return rv;

    if (NS_FAILED(rv = shell->Init(this, aContext, aViewManager, aStyleSet))) {
        NS_RELEASE(shell);
        return rv;
    }

    mPresShells.AppendElement(shell);
    *aInstancePtrResult = shell;

    return NS_OK;
}

PRBool 
nsRDFDocument::DeleteShell(nsIPresShell* aShell)
{
    return mPresShells.RemoveElement(aShell);
}

PRInt32 
nsRDFDocument::GetNumberOfShells()
{
    return mPresShells.Count();
}

nsIPresShell* 
nsRDFDocument::GetShellAt(PRInt32 aIndex)
{
    nsIPresShell* shell = NS_STATIC_CAST(nsIPresShell*, mPresShells[aIndex]);
    NS_IF_ADDREF(shell);
    return shell;
}

nsIDocument* 
nsRDFDocument::GetParentDocument()
{
    NS_IF_ADDREF(mParentDocument);
    return mParentDocument;
}

void 
nsRDFDocument::SetParentDocument(nsIDocument* aParent)
{
    // Note that we do *not* AddRef our parent because that would
    // create a circular reference.
    mParentDocument = aParent;
}

void 
nsRDFDocument::AddSubDocument(nsIDocument* aSubDoc)
{
    // we don't do subdocs.
    PR_ASSERT(0);
}

PRInt32 
nsRDFDocument::GetNumberOfSubDocuments()
{
    return 0;
}

nsIDocument* 
nsRDFDocument::GetSubDocumentAt(PRInt32 aIndex)
{
    // we don't do subdocs.
    PR_ASSERT(0);
    return nsnull;
}

nsIContent* 
nsRDFDocument::GetRootContent()
{
    NS_IF_ADDREF(mRootContent);
    return mRootContent;
}

void 
nsRDFDocument::SetRootContent(nsIContent* aRoot)
{
    NS_IF_RELEASE(mRootContent);
    mRootContent = aRoot;
    NS_IF_ADDREF(mRootContent);
}

PRInt32 
nsRDFDocument::GetNumberOfStyleSheets()
{
    return mStyleSheets.Count();
}

nsIStyleSheet* 
nsRDFDocument::GetStyleSheetAt(PRInt32 aIndex)
{
    nsIStyleSheet* sheet = NS_STATIC_CAST(nsIStyleSheet*, mStyleSheets[aIndex]);
    NS_IF_ADDREF(sheet);
    return sheet;
}

void 
nsRDFDocument::AddStyleSheet(nsIStyleSheet* aSheet)
{
    NS_PRECONDITION(aSheet, "null arg");
    if (!aSheet)
        return;

    mStyleSheets.AppendElement(aSheet);
    NS_ADDREF(aSheet);

    aSheet->SetOwningDocument(this);

    PRBool enabled;
    aSheet->GetEnabled(enabled);

    if (enabled) {
        PRInt32 count, index;

        count = mPresShells.Count();
        for (index = 0; index < count; index++) {
            nsIPresShell* shell = NS_STATIC_CAST(nsIPresShell*, mPresShells[index]);
            nsIStyleSet* set = shell->GetStyleSet();
            if (nsnull != set) {
                set->InsertDocStyleSheetBefore(aSheet, nsnull); // put it first
                NS_RELEASE(set);
            }
        }

        // XXX should observers be notified for disabled sheets??? I think not, but I could be wrong
        count = mObservers.Count();
        for (index = 0; index < count; index++) {
            nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers.ElementAt(index);
            observer->StyleSheetAdded(this, aSheet);
        }
    }
}

void 
nsRDFDocument::SetStyleSheetDisabledState(nsIStyleSheet* aSheet,
                                          PRBool aDisabled)
{
    NS_PRECONDITION(nsnull != aSheet, "null arg");
    PRInt32 count;
    PRInt32 index = mStyleSheets.IndexOf((void *)aSheet);
    // If we're actually in the document style sheet list
    if (-1 != index) {
        count = mPresShells.Count();
        PRInt32 index;
        for (index = 0; index < count; index++) {
            nsIPresShell* shell = (nsIPresShell*)mPresShells.ElementAt(index);
            nsIStyleSet* set = shell->GetStyleSet();
            if (nsnull != set) {
                if (aDisabled) {
                    set->RemoveDocStyleSheet(aSheet);
                }
                else {
                    set->InsertDocStyleSheetBefore(aSheet, nsnull);  // put it first
                }
                NS_RELEASE(set);
            }
        }
    }  

    count = mObservers.Count();
    for (index = 0; index < count; index++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers.ElementAt(index);
        observer->StyleSheetDisabledStateChanged(this, aSheet, aDisabled);
    }
}

nsIScriptContextOwner *
nsRDFDocument::GetScriptContextOwner()
{
    NS_IF_ADDREF(mScriptContextOwner);
    return mScriptContextOwner;
}

void 
nsRDFDocument::SetScriptContextOwner(nsIScriptContextOwner *aScriptContextOwner)
{
    // XXX HACK ALERT! If the script context owner is null, the document
    // will soon be going away. So tell our content that to lose its
    // reference to the document. This has to be done before we
    // actually set the script context owner to null so that the
    // content elements can remove references to their script objects.
    if (!aScriptContextOwner && !mRootContent)
        mRootContent->SetDocument(nsnull, PR_TRUE);

    NS_IF_RELEASE(mScriptContextOwner);
    mScriptContextOwner = aScriptContextOwner;
    NS_IF_ADDREF(mScriptContextOwner);
}

NS_IMETHODIMP
nsRDFDocument::GetNameSpaceManager(nsINameSpaceManager*& aManager)
{
  aManager = mNameSpaceManager;
  NS_IF_ADDREF(aManager);
  return NS_OK;
}


// Note: We don't hold a reference to the document observer; we assume
// that it has a live reference to the document.
void 
nsRDFDocument::AddObserver(nsIDocumentObserver* aObserver)
{
    // XXX Make sure the observer isn't already in the list
    if (mObservers.IndexOf(aObserver) == -1) {
        mObservers.AppendElement(aObserver);
    }
}

PRBool 
nsRDFDocument::RemoveObserver(nsIDocumentObserver* aObserver)
{
    return mObservers.RemoveElement(aObserver);
}

NS_IMETHODIMP 
nsRDFDocument::BeginLoad()
{
    PRInt32 i, count = mObservers.Count();
    for (i = 0; i < count; i++) {
        nsIDocumentObserver* observer = (nsIDocumentObserver*) mObservers[i];
        observer->BeginLoad(this);
    }
    return NS_OK;
}

NS_IMETHODIMP 
nsRDFDocument::EndLoad()
{
    PRInt32 i, count = mObservers.Count();
    for (i = 0; i < count; i++) {
        nsIDocumentObserver* observer = (nsIDocumentObserver*) mObservers[i];
        observer->EndLoad(this);
    }
    NS_IF_RELEASE(mParser);
    return NS_OK;
}


NS_IMETHODIMP 
nsRDFDocument::ContentChanged(nsIContent* aContent,
                              nsISupports* aSubContent)
{
    PRInt32 count = mObservers.Count();
    for (PRInt32 i = 0; i < count; i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->ContentChanged(this, aContent, aSubContent);
    }
    return NS_OK;
}

NS_IMETHODIMP 
nsRDFDocument::AttributeChanged(nsIContent* aChild,
                                nsIAtom* aAttribute,
                                PRInt32 aHint)
{
    PRInt32 count = mObservers.Count();
    for (PRInt32 i = 0; i < count; i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->AttributeChanged(this, aChild, aAttribute, aHint);
    }
    return NS_OK;
}

NS_IMETHODIMP 
nsRDFDocument::ContentAppended(nsIContent* aContainer,
                               PRInt32 aNewIndexInContainer)
{
    PRInt32 count = mObservers.Count();
    for (PRInt32 i = 0; i < count; i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->ContentAppended(this, aContainer, aNewIndexInContainer);
    }
    return NS_OK;
}

NS_IMETHODIMP 
nsRDFDocument::ContentInserted(nsIContent* aContainer,
                               nsIContent* aChild,
                               PRInt32 aIndexInContainer)
{
    PRInt32 count = mObservers.Count();
    for (PRInt32 i = 0; i < count; i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->ContentInserted(this, aContainer, aChild, aIndexInContainer);
    }
    return NS_OK;
}

NS_IMETHODIMP 
nsRDFDocument::ContentReplaced(nsIContent* aContainer,
                               nsIContent* aOldChild,
                               nsIContent* aNewChild,
                               PRInt32 aIndexInContainer)
{
    PRInt32 count = mObservers.Count();
    for (PRInt32 i = 0; i < count; i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->ContentReplaced(this, aContainer, aOldChild, aNewChild,
                                  aIndexInContainer);
    }
    return NS_OK;
}

NS_IMETHODIMP 
nsRDFDocument::ContentRemoved(nsIContent* aContainer,
                              nsIContent* aChild,
                              PRInt32 aIndexInContainer)
{
    PRInt32 count = mObservers.Count();
    for (PRInt32 i = 0; i < count; i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->ContentRemoved(this, aContainer, 
                                 aChild, aIndexInContainer);
    }
    return NS_OK;
}

NS_IMETHODIMP 
nsRDFDocument::StyleRuleChanged(nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule,
                              PRInt32 aHint)
{
    PRInt32 count = mObservers.Count();
    for (PRInt32 i = 0; i < count; i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->StyleRuleChanged(this, aStyleSheet, aStyleRule, aHint);
    }
    return NS_OK;
}

NS_IMETHODIMP 
nsRDFDocument::StyleRuleAdded(nsIStyleSheet* aStyleSheet,
                            nsIStyleRule* aStyleRule)
{
    PRInt32 count = mObservers.Count();
    for (PRInt32 i = 0; i < count; i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->StyleRuleAdded(this, aStyleSheet, aStyleRule);
    }
    return NS_OK;
}

NS_IMETHODIMP 
nsRDFDocument::StyleRuleRemoved(nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule)
{
    PRInt32 count = mObservers.Count();
    for (PRInt32 i = 0; i < count; i++) {
        nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
        observer->StyleRuleRemoved(this, aStyleSheet, aStyleRule);
    }
    return NS_OK;
}

NS_IMETHODIMP 
nsRDFDocument::GetSelection(nsICollection** aSelection)
{
    if (!mSelection) {
        PR_ASSERT(0);
        *aSelection = nsnull;
        return NS_ERROR_NOT_INITIALIZED;
    }
    NS_ADDREF(mSelection);
    *aSelection = mSelection;
    return NS_OK;
}

NS_IMETHODIMP 
nsRDFDocument::SelectAll()
{

    nsIContent * start = nsnull;
    nsIContent * end   = nsnull;
    nsIContent * body  = nsnull;

    nsString bodyStr("BODY");
    PRInt32 i, n;
    mRootContent->ChildCount(n);
    for (i=0;i<n;i++) {
        nsIContent * child;
        mRootContent->ChildAt(i, child);
        PRBool isSynthetic;
        child->IsSynthetic(isSynthetic);
        if (!isSynthetic) {
            nsIAtom * atom;
            child->GetTag(atom);
            if (bodyStr.EqualsIgnoreCase(atom)) {
                body = child;
                break;
            }

        }
        NS_RELEASE(child);
    }

    if (body == nsnull) {
        return NS_ERROR_FAILURE;
    }

    start = body;
    // Find Very first Piece of Content
    for (;;) {
        start->ChildCount(n);
        if (n <= 0) {
            break;
        }
        nsIContent * child = start;
        child->ChildAt(0, start);
        NS_RELEASE(child);
    }

    end = body;
    // Last piece of Content
    for (;;) {
        end->ChildCount(n);
        if (n <= 0) {
            break;
        }
        nsIContent * child = end;
        child->ChildAt(n-1, end);
        NS_RELEASE(child);
    }

    //NS_RELEASE(start);
    //NS_RELEASE(end);

#if 0 // XXX nsSelectionRange is in another DLL
    nsSelectionRange * range    = mSelection->GetRange();
    nsSelectionPoint * startPnt = range->GetStartPoint();
    nsSelectionPoint * endPnt   = range->GetEndPoint();
    startPnt->SetPoint(start, -1, PR_TRUE);
    endPnt->SetPoint(end, -1, PR_FALSE);
#endif
    SetDisplaySelection(PR_TRUE);

    return NS_OK;
}

NS_IMETHODIMP 
nsRDFDocument::FindNext(const nsString &aSearchStr, PRBool aMatchCase, PRBool aSearchDown, PRBool &aIsFound)
{
    aIsFound = PR_FALSE;
    return NS_ERROR_FAILURE;
}

void 
nsRDFDocument::CreateXIF(nsString & aBuffer, PRBool aUseSelection)
{
    PR_ASSERT(0);
}

void 
nsRDFDocument::ToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode)
{
    PR_ASSERT(0);
}

void 
nsRDFDocument::BeginConvertToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode)
{
    PR_ASSERT(0);
}

void 
nsRDFDocument::ConvertChildrenToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode)
{
    PR_ASSERT(0);
}

void 
nsRDFDocument::FinishConvertToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode)
{
    PR_ASSERT(0);
}

PRBool 
nsRDFDocument::IsInRange(const nsIContent *aStartContent, const nsIContent* aEndContent, const nsIContent* aContent) const
{
    PRBool  result;

    if (aStartContent == aEndContent) {
            return PRBool(aContent == aStartContent);
    }
    else if (aStartContent == aContent || aEndContent == aContent) {
        result = PR_TRUE;
    }
    else {
        result = IsBefore(aStartContent,aContent);
        if (result)
            result = IsBefore(aContent, aEndContent);
    }
    return result;
}

PRBool 
nsRDFDocument::IsBefore(const nsIContent *aNewContent, const nsIContent* aCurrentContent) const
{
    PRBool result = PR_FALSE;

    if (nsnull != aNewContent && nsnull != aCurrentContent && aNewContent != aCurrentContent) {
        nsIContent* test = FindContent(mRootContent,aNewContent,aCurrentContent);
        if (test == aNewContent)
            result = PR_TRUE;

        NS_RELEASE(test);
    }
    return result;
}

PRBool 
nsRDFDocument::IsInSelection(const nsIContent *aContent) const
{
    PRBool  result = PR_FALSE;

    if (mSelection != nsnull) {
#if 0 // XXX can't include this because nsSelectionPoint is in another DLL.
        nsSelectionRange* range = mSelection->GetRange();
        if (range != nsnull) {
            nsSelectionPoint* startPoint = range->GetStartPoint();
            nsSelectionPoint* endPoint = range->GetEndPoint();

            nsIContent* startContent = startPoint->GetContent();
            nsIContent* endContent = endPoint->GetContent();
            result = IsInRange(startContent, endContent, aContent);
            NS_IF_RELEASE(startContent);
            NS_IF_RELEASE(endContent);
        }
#endif
    }
    return result;
}

nsIContent* 
nsRDFDocument::GetPrevContent(const nsIContent *aContent) const
{
    nsIContent* result = nsnull;
 
    // Look at previous sibling

    if (nsnull != aContent) {
        nsIContent* parent; 
        aContent->GetParent(parent);

        if (parent != nsnull && parent != mRootContent) {
            PRInt32 index;
            parent->IndexOf((nsIContent*)aContent, index);
            if (index > 0)
                parent->ChildAt(index-1, result);
            else
                result = GetPrevContent(parent);
        }
        NS_IF_RELEASE(parent);
    }
    return result;
}

nsIContent* 
nsRDFDocument::GetNextContent(const nsIContent *aContent) const
{
    nsIContent* result = nsnull;
   
    if (nsnull != aContent) {
        // Look at next sibling
        nsIContent* parent;
        aContent->GetParent(parent);

        if (parent != nsnull && parent != mRootContent) {
            PRInt32 index;
            parent->IndexOf((nsIContent*)aContent, index);

            PRInt32 count;
            parent->ChildCount(count);
            if (index+1 < count) {
                parent->ChildAt(index+1, result);
                // Get first child down the tree
                for (;;) {
                    PRInt32 n;
                    result->ChildCount(n);
                    if (n <= 0)
                        break;

                    nsIContent * old = result;
                    old->ChildAt(0, result);
                    NS_RELEASE(old);
                    result->ChildCount(n);
                }
            } else {
                result = GetNextContent(parent);
            }
        }
        NS_IF_RELEASE(parent);
    }
    return result;
}

void 
nsRDFDocument::SetDisplaySelection(PRBool aToggle)
{
    mDisplaySelection = aToggle;
}

PRBool 
nsRDFDocument::GetDisplaySelection() const
{
    return mDisplaySelection;
}

NS_IMETHODIMP 
nsRDFDocument::HandleDOMEvent(nsIPresContext& aPresContext, 
                            nsEvent* aEvent, 
                            nsIDOMEvent** aDOMEvent,
                            PRUint32 aFlags,
                            nsEventStatus& aEventStatus)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


////////////////////////////////////////////////////////////////////////
// nsIXMLDocument interface

NS_IMETHODIMP
nsRDFDocument::PrologElementAt(PRInt32 aOffset, nsIContent** aContent)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsRDFDocument::PrologCount(PRInt32* aCount)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsRDFDocument::AppendToProlog(nsIContent* aContent)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRDFDocument::EpilogElementAt(PRInt32 aOffset, nsIContent** aContent)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsRDFDocument::EpilogCount(PRInt32* aCount)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsRDFDocument::AppendToEpilog(nsIContent* aContent)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////
// nsIRDFDocument interface

NS_IMETHODIMP
nsRDFDocument::Init(void)
{
    nsresult rv;
    if (NS_FAILED(rv = NS_NewHeapArena(&mArena, nsnull)))
        return rv;

    rv = nsRepository::CreateInstance(kNameSpaceManagerCID,
                                      nsnull,
                                      kINameSpaceManagerIID,
                                      (void**) &mNameSpaceManager);
    if (NS_FAILED(rv))
        return rv;

    if (NS_FAILED(rv = nsRepository::CreateInstance(kRDFDataBaseCID,
                                                    nsnull,
                                                    kIRDFDataBaseIID,
                                                    (void**) &mDB)))
        return rv;

    if (NS_FAILED(rv = nsServiceManager::GetService(kRDFServiceCID,
                                                    kIRDFServiceIID,
                                                    (nsISupports**) &mRDFService)))
        return rv;

    return NS_OK;
}

NS_IMETHODIMP
nsRDFDocument::SetRootResource(nsIRDFResource* aResource)
{
    nsresult rv;

    nsIRDFContent* root;
    if (NS_FAILED(rv = NS_NewRDFElement(&root)))
        return rv;

    if (NS_FAILED(rv = root->Init(this, "ROOT", aResource, PR_TRUE)))
        return rv;

    SetRootContent(NS_STATIC_CAST(nsIContent*, root));

    NS_RELEASE(root); // release our local reference
    return NS_OK;
}

NS_IMETHODIMP
nsRDFDocument::GetDataBase(nsIRDFDataBase*& result)
{
    if (! mDB)
        return NS_ERROR_NOT_INITIALIZED;

    result = mDB;
    NS_ADDREF(result);

    return NS_OK;
}


NS_IMETHODIMP
nsRDFDocument::CreateChildren(nsIRDFContent* element)
{
    nsresult rv;

    NS_ASSERTION(mDB, "not initialized");
    if (! mDB)
        return NS_ERROR_NOT_INITIALIZED;

    NS_ASSERTION(mRDFService, "not initialized");
    if (! mRDFService)
        return NS_ERROR_NOT_INITIALIZED;


    nsIRDFResource* resource = nsnull;
    nsIRDFArcsOutCursor* properties = nsnull;

    if (NS_FAILED(rv = element->GetResource(resource)))
        goto done;

    // Create a cursor that'll enumerate all of the outbound arcs
    if (NS_FAILED(rv = mDB->ArcLabelsOut(resource, &properties)))
        goto done;

    while (NS_SUCCEEDED(rv = properties->Advance())) {
        nsIRDFResource* property = nsnull;

        if (NS_FAILED(rv = properties->GetValue((nsIRDFNode**)&property)))
            break;

        const char* s;
        if (NS_FAILED(rv = property->GetValue(&s))) {
            NS_RELEASE(property);
            break;
        }

        nsAutoString uri(s);

        // Create a second cursor that'll enumerate all of the values
        // for all of the arcs.
        nsIRDFAssertionCursor* assertions;
        if (NS_FAILED(rv = mDB->GetTargets(resource, property, PR_TRUE, &assertions))) {
            NS_RELEASE(property);
            break;
        }

        while (NS_SUCCEEDED(rv = assertions->Advance())) {
            nsIRDFNode* value;
            if (NS_SUCCEEDED(rv = assertions->GetValue((nsIRDFNode**)&value))) {
                // At this point, the specific nsRDFDocument
                // implementations will create an appropriate child
                // element (or elements).
                rv = AddChild(element, property, value);
                NS_RELEASE(value);
            }

            if (NS_FAILED(rv))
                break;
        }

        NS_RELEASE(assertions);
        NS_RELEASE(property);
    }

    if (rv = NS_ERROR_RDF_CURSOR_EMPTY)
        // This is a normal return code from nsIRDFCursor::Advance()
        rv = NS_OK;

done:
    NS_IF_RELEASE(resource);
    NS_IF_RELEASE(properties);

    return rv;
}

NS_IMETHODIMP
nsRDFDocument::AddTreeProperty(nsIRDFResource* resource)
{
    nsresult rv;
    if (! mTreeProperties) {
        if (NS_FAILED(rv = NS_NewISupportsArray(&mTreeProperties)))
            return rv;
    }

    // ensure uniqueness
    if (mTreeProperties->IndexOf(resource) != -1) {
        PR_ASSERT(0);
        return NS_OK;
    }

    mTreeProperties->AppendElement(resource);
    return NS_OK;
}


NS_IMETHODIMP
nsRDFDocument::RemoveTreeProperty(nsIRDFResource* resource)
{
    if (! mTreeProperties) {
        // XXX no properties have ever been inserted!
        PR_ASSERT(0);
        return NS_OK;
    }

    if (! mTreeProperties->RemoveElement(resource)) {
        // XXX that specific property has never been inserted!
        PR_ASSERT(0);
        return NS_OK;
    }

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// Implementation methods

nsIContent*
nsRDFDocument::FindContent(const nsIContent* aStartNode,
                           const nsIContent* aTest1, 
                           const nsIContent* aTest2) const
{
    PRInt32 count;
    aStartNode->ChildCount(count);

    PRInt32 index;
    for(index = 0; index < count;index++) {
        nsIContent* child;
        aStartNode->ChildAt(index, child);
        nsIContent* content = FindContent(child,aTest1,aTest2);
        if (content != nsnull) {
            NS_IF_RELEASE(child);
            return content;
        }
        if (child == aTest1 || child == aTest2) {
            NS_IF_RELEASE(content);
            return child;
        }
        NS_IF_RELEASE(child);
        NS_IF_RELEASE(content);
    }
    return nsnull;
}


PRBool
nsRDFDocument::IsTreeProperty(const nsIRDFResource* property) const
{
#define TREE_PROPERTY_HACK
#if defined(TREE_PROPERTY_HACK)
    const char* p;
    property->GetValue(&p);
    nsAutoString s(p);
    if (s.Equals("http://home.netscape.com/NC-rdf#child") ||
        s.Equals("http://home.netscape.com/NC-rdf#Folder")) {
        return PR_TRUE;
    }
#endif // defined(TREE_PROPERTY_HACK)
    if (rdf_IsOrdinalProperty(property)) {
        return PR_TRUE;
    }
    if (! mTreeProperties) {
        return PR_FALSE;
    }
    if (mTreeProperties->IndexOf(property) == -1) {
        return PR_FALSE;
    }
    return PR_TRUE;
}

nsresult
nsRDFDocument::NewChild(const nsString& tag,
                        nsIRDFResource* resource,
                        nsIRDFContent*& result,
                        PRBool childrenMustBeGenerated)
{
    nsresult rv;

    nsIRDFContent* child;
    if (NS_FAILED(rv = NS_NewRDFElement(&child)))
        return rv;

    if (NS_FAILED(rv = child->Init(this, tag, resource, childrenMustBeGenerated))) {
        NS_RELEASE(child);
        return rv;
    }

    result = child;
    return NS_OK;
}


nsresult
nsRDFDocument::AttachTextNode(nsIContent* parent,
                              nsIRDFNode* value)
{
    nsresult rv;
    nsAutoString s;
    nsIContent* node = nsnull;
    nsITextContent* text = nsnull;
    nsIDocument* doc = nsnull;
    nsIRDFResource* resource = nsnull;
    nsIRDFLiteral* literal = nsnull;
    
    if (NS_SUCCEEDED(rv = value->QueryInterface(kIRDFResourceIID, (void**) &resource))) {
        const char* p;
        if (NS_FAILED(rv = resource->GetValue(&p)))
            goto error;

        s = p;
    }
    else if (NS_SUCCEEDED(rv = value->QueryInterface(kIRDFLiteralIID, (void**) &literal))) {
        const PRUnichar* p;
        if (NS_FAILED(rv = literal->GetValue(&p)))
            goto error;

        s = p;
    }
    else {
        PR_ASSERT(0);
        goto error;
    }

    if (NS_FAILED(rv = nsRepository::CreateInstance(kTextNodeCID,
                                                    nsnull,
                                                    kIContentIID,
                                                    (void**) &node)))
        goto error;

    if (NS_FAILED(rv = QueryInterface(kIDocumentIID, (void**) &doc)))
        goto error;

    if (NS_FAILED(rv = node->SetDocument(doc, PR_FALSE)))
        goto error;

    if (NS_FAILED(rv = node->QueryInterface(kITextContentIID, (void**) &text)))
        goto error;

    if (NS_FAILED(rv = text->SetText(s.GetUnicode(), s.Length(), PR_FALSE)))
        goto error;

    // hook it up to the child
    if (NS_FAILED(rv = node->SetParent(parent)))
        goto error;

    if (NS_FAILED(rv = parent->AppendChildTo(NS_STATIC_CAST(nsIContent*, node), PR_TRUE)))
        goto error;

error:
    NS_IF_RELEASE(node);
    NS_IF_RELEASE(text);
    NS_IF_RELEASE(doc);
    return rv;
}

