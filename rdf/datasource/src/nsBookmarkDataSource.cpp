/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*

  A data source that reads the 4.x "bookmarks.html" file and
  constructs an in-memory data source.

  TO DO

  1) Write the modified bookmarks file back to disk.

 */

#include "nsIRDFDataSource.h"
#include "nsIRDFNode.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsRDFCID.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "prio.h"
#include "rdf.h"
#include "rdfutil.h"

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kIRDFDataSourceIID,        NS_IRDFDATASOURCE_IID);
static NS_DEFINE_IID(kIRDFServiceIID,           NS_IRDFSERVICE_IID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID, NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_CID(kRDFServiceCID,            NS_RDFSERVICE_CID);

static const char kURINC_BookmarksRoot[]         = "NC:BookmarksRoot"; // XXX?
static const char kURINC_PersonalToolbarFolder[] = "NC:PersonalToolbarFolder"; // XXX?

DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, child);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Bookmark);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, BookmarkAddDate);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Description);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Folder);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Name);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, PersonalToolbarFolderCategory);

DEFINE_RDF_VOCAB(WEB_NAMESPACE_URI, WEB, LastVisitDate);
DEFINE_RDF_VOCAB(WEB_NAMESPACE_URI, WEB, LastModifiedDate);

DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, instanceOf);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, type);

static const char kPersonalToolbar[]  = "Personal Toolbar";


////////////////////////////////////////////////////////////////////////

/**
 * The bookmark parser knows how to read <tt>bookmarks.html</tt> and convert it
 * into an RDF graph.
 */
class BookmarkParser {
protected:
    static PRInt32 gRefCnt;

    static const char* kBRString;
    static const char* kCloseDLString;
    static const char* kDDString;
    static const char* kOpenAnchorString;
    static const char* kOpenDLString;
    static const char* kOpenH3String;
    static const char* kOpenTitleString;
    static const char* kSeparatorString;

    static nsIRDFService* gRDFService;
    static nsIRDFResource* kNC_Bookmark;
    static nsIRDFResource* kNC_BookmarkAddDate;
    static nsIRDFResource* kNC_Description;
    static nsIRDFResource* kNC_Folder;
    static nsIRDFResource* kNC_Name;
    static nsIRDFResource* kRDF_type;
    static nsIRDFResource* kWEB_LastModifiedDate;
    static nsIRDFResource* kWEB_LastVisitDate;

    enum BookmarkParserState {
        eBookmarkParserState_Initial,
        eBookmarkParserState_InTitle,
        eBookmarkParserState_InH3,
        eBookmarkParserState_InItemTitle,
        eBookmarkParserState_InItemDescription
    };

    nsIRDFDataSource*      mDataSource;
    nsVoidArray            mStack;
    nsIRDFResource*        mLastItem;
    nsAutoString           mLine;
    PRInt32                mCounter;
    nsAutoString           mFolderDate;
    BookmarkParserState    mState;

    nsresult Tokenize(const char* buf, PRInt32 size);
    nsresult NextToken(void);
    nsresult DoStateTransition(void);
    nsresult CreateBookmark(void);

    nsresult AssertTime(nsIRDFResource* aSubject,
                        nsIRDFResource* aPredicate,
                        const nsString& aTime);

public:
    BookmarkParser(void);
    ~BookmarkParser();

    nsresult Parse(PRFileDesc* file, nsIRDFDataSource* dataSource);
};


const char* BookmarkParser::kBRString         = "<BR>";
const char* BookmarkParser::kCloseDLString    = "</DL>";
const char* BookmarkParser::kDDString         = "<DD>";
const char* BookmarkParser::kOpenAnchorString = "<A";
const char* BookmarkParser::kOpenDLString     = "<DL>";
const char* BookmarkParser::kOpenH3String     = "<H3";
const char* BookmarkParser::kOpenTitleString  = "<TITLE>";
const char* BookmarkParser::kSeparatorString  = "<HR>";

PRInt32 BookmarkParser::gRefCnt;
nsIRDFService* BookmarkParser::gRDFService;
nsIRDFResource* BookmarkParser::kNC_Bookmark;
nsIRDFResource* BookmarkParser::kNC_BookmarkAddDate;
nsIRDFResource* BookmarkParser::kNC_Description;
nsIRDFResource* BookmarkParser::kNC_Folder;
nsIRDFResource* BookmarkParser::kNC_Name;
nsIRDFResource* BookmarkParser::kRDF_type;
nsIRDFResource* BookmarkParser::kWEB_LastModifiedDate;
nsIRDFResource* BookmarkParser::kWEB_LastVisitDate;

BookmarkParser::BookmarkParser(void)
    : mDataSource(nsnull)
{
    if (gRefCnt++ == 0) {
        nsresult rv;
        if (NS_FAILED(rv = nsServiceManager::GetService(kRDFServiceCID,
                                                        kIRDFServiceIID,
                                                        (nsISupports**) &gRDFService))) {
            NS_ERROR("unable to get RDF service");
            return;
        }

        gRDFService->GetResource(kURINC_Bookmark,          &kNC_Bookmark);
        gRDFService->GetResource(kURINC_BookmarkAddDate,   &kNC_BookmarkAddDate);
        gRDFService->GetResource(kURINC_Description,       &kNC_Description);
        gRDFService->GetResource(kURINC_Folder,            &kNC_Folder);
        gRDFService->GetResource(kURINC_Name,              &kNC_Name);
        gRDFService->GetResource(kURIRDF_type,             &kRDF_type);
        gRDFService->GetResource(kURIWEB_LastModifiedDate, &kWEB_LastModifiedDate);
        gRDFService->GetResource(kURIWEB_LastVisitDate,    &kWEB_LastVisitDate);
    }    
}

BookmarkParser::~BookmarkParser(void)
{
    if (--gRefCnt == 0) {
        if (gRDFService)
            nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);

        NS_IF_RELEASE(kNC_Bookmark);
        NS_IF_RELEASE(kNC_BookmarkAddDate);
        NS_IF_RELEASE(kNC_Description);
        NS_IF_RELEASE(kNC_Folder);
        NS_IF_RELEASE(kNC_Name);
        NS_IF_RELEASE(kRDF_type);
        NS_IF_RELEASE(kWEB_LastModifiedDate);
        NS_IF_RELEASE(kWEB_LastVisitDate);
    }
}


nsresult
BookmarkParser::Parse(PRFileDesc* file, nsIRDFDataSource* dataSource)
{
    NS_PRECONDITION(file != nsnull, "null ptr");
    if (! file)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(dataSource != nsnull, "null ptr");
    if (! dataSource)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(gRDFService != nsnull, "not initialized");
    if (! gRDFService)
        return NS_ERROR_NOT_INITIALIZED;

    // Initialize the parser for a run...
    mDataSource = dataSource;
    mState = eBookmarkParserState_Initial;
    mCounter = 0;
    mLastItem = nsnull;
    mLine.Truncate();

    char buf[1024];
    PRInt32 len;

    nsresult rv;
    while ((len = PR_Read(file, buf, sizeof(buf))) > 0) {
        if (NS_FAILED(rv = Tokenize(buf, len))) {
            NS_ERROR("error tokenizing");
            break;
        }
    }

    NS_IF_RELEASE(mLastItem);
    return rv;
}


nsresult
BookmarkParser::Tokenize(const char* buf, PRInt32 size)
{
    nsresult rv;
    for (PRInt32 i = 0; i < size; ++i) {
        char c = buf[i];
        if (c == '<') {
            if (mLine.Length() > 0) {
                rv = NextToken();
                mLine.Truncate();
            }
        }
        mLine.Append(c);
        if (c == '>') {
            if (mLine.Length() > 0) {
                rv = NextToken();
                mLine.Truncate();
            }
        }
    }
    return rv;
}


nsresult
BookmarkParser::NextToken(void)
{
    nsresult rv;

    if (mLine[0] == '<') {
        return DoStateTransition();
    }

    // ok, we have a piece of content. can be the title, or a
    // description
    if ((mState == eBookmarkParserState_InTitle) ||
        (mState == eBookmarkParserState_InH3)) {
        nsIRDFResource* folder;

        if (mStack.Count() > 0) {
            // a regular old folder
            nsAutoString folderURI;

            if (mLine.Find(kPersonalToolbar) == 0) {
                folderURI = kURINC_PersonalToolbarFolder;
            }
            else {
                folderURI = kURINC_BookmarksRoot;
                folderURI.Append('#');
                folderURI.Append(++mCounter, 10);
            }

            if (NS_FAILED(rv = gRDFService->GetUnicodeResource(folderURI, &folder))) {
                NS_ERROR("unable to get resource");
                return rv;
            }

            nsIRDFResource* parent = (nsIRDFResource*) mStack[mStack.Count() - 1];
            rdf_ContainerAppendElement(mDataSource, parent, folder);
        }
        else {
            // it's the root
            if (NS_FAILED(rv = gRDFService->GetResource(kURINC_BookmarksRoot, &folder))) {
                NS_ERROR("unable to get resource");
                return rv;
            }
        }

        rdf_MakeSeq(mDataSource, folder);
        mDataSource->Assert(folder, kRDF_type, kNC_Folder, PR_TRUE);

        if (mFolderDate.Length()) {
            AssertTime(folder, kNC_BookmarkAddDate, mFolderDate);
            mFolderDate.Truncate();
        }

        NS_IF_RELEASE(mLastItem);
        mLastItem = folder;
        // XXX Implied
        //NS_ADDREF(mLastItem);
        //NS_RELEASE(folder); 

        if (mState != eBookmarkParserState_InTitle) {
            nsIRDFLiteral* literal;
            gRDFService->GetLiteral(mLine, &literal);
            mDataSource->Assert(mLastItem, kNC_Name, literal, PR_TRUE);
            NS_RELEASE(literal);
        }
    }
    else if (mState == eBookmarkParserState_InItemTitle) {
        NS_ASSERTION(mLastItem != nsnull, "no last item!");
        if (! mLastItem)
            return NS_ERROR_UNEXPECTED;

        nsIRDFLiteral* literal;
        gRDFService->GetLiteral(mLine, &literal);
        mDataSource->Assert(mLastItem, kNC_Name, literal, PR_TRUE);
        NS_RELEASE(literal);
        NS_RELEASE(mLastItem);
    }
    else if (mState == eBookmarkParserState_InItemDescription) {
        nsIRDFLiteral* literal;
        gRDFService->GetLiteral(mLine, &literal);
        mDataSource->Assert(mLastItem, kNC_Description, literal, PR_TRUE);
        NS_RELEASE(literal);
    }
    return NS_OK;
}



nsresult
BookmarkParser::DoStateTransition(void)
{
    if (mLine.Find(kOpenAnchorString) == 0) {
        CreateBookmark();
        mState = eBookmarkParserState_InItemTitle;
    }
    else if (mLine.Find(kOpenH3String) == 0) {
        PRInt32 start = mLine.Find('"');
        PRInt32 end   = mLine.RFind('"');
        if (start >= 0 && end >= 0)
            mLine.Mid(mFolderDate, start + 1, end - start - 1);
        mState = eBookmarkParserState_InH3;
    }
    else if (mLine.Find(kOpenTitleString) == 0) {
        mState = eBookmarkParserState_InTitle;
    }
    else if (mLine.Find(kDDString) == 0) {
#if 0 // XXX if it doesn't already have a description? Huh?
        if (remoteStoreGetSlotValue(gLocalStore, mLastItem, gWebData->RDF_description, 
                                    RDF_STRING_TYPE, false, true) 
            == nsnull)
#endif
        mState = eBookmarkParserState_InItemDescription;
    }
    else if (mLine.Find(kOpenDLString) == 0) {
        mStack.AppendElement(mLastItem);
        NS_ADDREF(mLastItem);
    }
    else if (mLine.Find(kCloseDLString) == 0) {
        PRInt32 count = mStack.Count();
        if (count) {
            nsIRDFNode* top = NS_STATIC_CAST(nsIRDFNode*, mStack[--count]);
            mStack.RemoveElementAt(count);
            NS_IF_RELEASE(top);
        }
    }
    else if (mLine.Find(kSeparatorString) == 0) {
#if FIXME // separators
        addSlotValue(f, createSeparator(), gCoreVocab->RDF_parent, mStack[mStack.Count() - 1], 
                     RDF_RESOURCE_TYPE, nsnull);
#endif
        mState = eBookmarkParserState_Initial;
    }
    else if ((mState == eBookmarkParserState_InItemDescription) &&
             (mLine.Find(kBRString) == 0)) {
        // XXX in the original bmk2rdf.c, we only added the
        // description in the case that it wasn't set already...why?
        nsIRDFLiteral* literal;
        gRDFService->GetLiteral(mLine, &literal);
        mDataSource->Assert(mLastItem, kNC_Description, literal, PR_TRUE);
        NS_RELEASE(literal);
    }
    else {
        mState = eBookmarkParserState_Initial;
    }
    return NS_OK;
}




nsresult
BookmarkParser::CreateBookmark(void)
{
    nsresult rv;

    enum {
        eBmkAttribute_URL = 0,
        eBmkAttribute_AddDate = 1,
        eBmkAttribute_LastVisit = 2,
        eBmkAttribute_LastModified = 3
    };

    nsAutoString values[4];
    PRInt32 index = 0;
    for (PRInt32 i = 0; i < 4; ++i) {
        PRInt32 start = mLine.Find('"', index);
        if (start == -1)
            break;

        ++start; // past the first quote

        PRInt32 end = mLine.Find('"', start);
        PR_ASSERT(end > 0); // unterminated
        if (end == -1)
            end = mLine.Length();

        mLine.Mid(values[i], start, end - start);
        index = end + 1;
    }

    if (values[eBmkAttribute_URL].Length() == 0)
        return NS_OK;

    nsIRDFResource* bookmark;
    if (NS_FAILED(rv = gRDFService->GetUnicodeResource(values[eBmkAttribute_URL], &bookmark)))
        return rv;

    mDataSource->Assert(bookmark, kRDF_type, kNC_Bookmark, PR_TRUE);

    NS_ASSERTION(mStack.Count() > 0, "no enclosing folder");
    if (! mStack.Count())
        return NS_ERROR_UNEXPECTED;

    nsIRDFResource* parent = (nsIRDFResource*) mStack[mStack.Count() - 1];
    NS_ASSERTION(parent != nsnull, "stack corruption");
    if (! parent)
        return NS_ERROR_UNEXPECTED;

    rdf_ContainerAppendElement(mDataSource, parent, bookmark);

    if (values[eBmkAttribute_AddDate].Length() > 0)
        AssertTime(bookmark, kNC_BookmarkAddDate, values[eBmkAttribute_AddDate]);

    if (values[eBmkAttribute_LastVisit].Length() > 0)
        AssertTime(bookmark, kWEB_LastVisitDate, values[eBmkAttribute_LastVisit]);

    if (values[eBmkAttribute_LastModified].Length() > 0)
        AssertTime(bookmark, kWEB_LastModifiedDate, values[eBmkAttribute_LastModified]);

    NS_IF_RELEASE(mLastItem);
    mLastItem = bookmark;
    // XXX Implied
    //NS_ADDREF(mLastItem);
    //NS_RELEASE(bookmark);

    return NS_OK;
}


nsresult
BookmarkParser::AssertTime(nsIRDFResource* object,
                           nsIRDFResource* predicate,
                           const nsString& time)
{
    // XXX Convert to a time
    nsresult rv;
    nsIRDFLiteral* literal;
    if (NS_FAILED(rv = gRDFService->GetLiteral(time, &literal))) {
        NS_ERROR("unable to get literal for time");
        return rv;
    }

    rv = mDataSource->Assert(object, predicate, literal, PR_TRUE);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to assert time");

    NS_RELEASE(literal);
    return rv;
}

////////////////////////////////////////////////////////////////////////
// BookmarkDataSourceImpl

/**
 * The bookmark data source uses a <tt>BookmarkParser</tt> to read the
 * <tt>bookmarks.html</tt> file from the local disk and present an
 * in-memory RDF graph based on it.
 */
class BookmarkDataSourceImpl : public nsIRDFDataSource {
protected:
    static const char* kBookmarksFilename;

    nsIRDFDataSource* mInner;

    nsresult ReadBookmarks(void);
    nsresult WriteBookmarks(void);
    nsresult AddColumns(void);

public:
    BookmarkDataSourceImpl(void);
    virtual ~BookmarkDataSourceImpl(void);

    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIRDFDataSource
    NS_IMETHOD Init(const char* uri);

    NS_IMETHOD GetURI(const char* *uri) const {
        return mInner->GetURI(uri);
    }

    NS_IMETHOD GetSource(nsIRDFResource* property,
                         nsIRDFNode* target,
                         PRBool tv,
                         nsIRDFResource** source) {
        return mInner->GetSource(property, target, tv, source);
    }

    NS_IMETHOD GetSources(nsIRDFResource* property,
                          nsIRDFNode* target,
                          PRBool tv,
                          nsIRDFAssertionCursor** sources) {
        return mInner->GetSources(property, target, tv, sources);
    }

    NS_IMETHOD GetTarget(nsIRDFResource* source,
                         nsIRDFResource* property,
                         PRBool tv,
                         nsIRDFNode** target) {
        return mInner->GetTarget(source, property, tv, target);
    }

    NS_IMETHOD GetTargets(nsIRDFResource* source,
                          nsIRDFResource* property,
                          PRBool tv,
                          nsIRDFAssertionCursor** targets) {
        return mInner->GetTargets(source, property, tv, targets);
    }

    NS_IMETHOD Assert(nsIRDFResource* source, 
                      nsIRDFResource* property, 
                      nsIRDFNode* target,
                      PRBool tv) {
        return mInner->Assert(source, property, target, tv);
    }

    NS_IMETHOD Unassert(nsIRDFResource* source,
                        nsIRDFResource* property,
                        nsIRDFNode* target) {
        return mInner->Unassert(source, property, target);
    }

    NS_IMETHOD HasAssertion(nsIRDFResource* source,
                            nsIRDFResource* property,
                            nsIRDFNode* target,
                            PRBool tv,
                            PRBool* hasAssertion) {
        return mInner->HasAssertion(source, property, target, tv, hasAssertion);
    }

    NS_IMETHOD AddObserver(nsIRDFObserver* n) {
        return mInner->AddObserver(n);
    }

    NS_IMETHOD RemoveObserver(nsIRDFObserver* n) {
        return mInner->RemoveObserver(n);
    }

    NS_IMETHOD ArcLabelsIn(nsIRDFNode* node,
                           nsIRDFArcsInCursor** labels) {
        return mInner->ArcLabelsIn(node, labels);
    }

    NS_IMETHOD ArcLabelsOut(nsIRDFResource* source,
                            nsIRDFArcsOutCursor** labels) {
        return mInner->ArcLabelsOut(source, labels);
    }

    NS_IMETHOD GetAllResources(nsIRDFResourceCursor** aCursor) {
        return mInner->GetAllResources(aCursor);
    }

    NS_IMETHOD Flush(void);

    NS_IMETHOD IsCommandEnabled(const char* aCommand,
                                nsIRDFResource* aCommandTarget,
                                PRBool* aResult);

    NS_IMETHOD DoCommand(const char* aCommand,
                         nsIRDFResource* aCommandTarget);
};

////////////////////////////////////////////////////////////////////////

// XXX we should get this from prefs.
#if defined(XP_MAC)
const char* BookmarkDataSourceImpl::kBookmarksFilename = "/usr/local/netscape/bin/res/rdf/bookmarks.html";
#elif defined(XP_PC)
const char* BookmarkDataSourceImpl::kBookmarksFilename = "res\\rdf\\bookmarks.html";
#elif defined(XP_UNIX)
const char* BookmarkDataSourceImpl::kBookmarksFilename = "res/rdf/bookmarks.html";
#endif

BookmarkDataSourceImpl::BookmarkDataSourceImpl(void)
{
    // XXX rvg there should be only one instance of this class. 
    // this is actually true of all datasources.
    NS_INIT_REFCNT();
}

BookmarkDataSourceImpl::~BookmarkDataSourceImpl(void)
{
    // unregister this from the RDF service
    nsresult rv;
    nsIRDFService* rdfService;
    if (NS_SUCCEEDED(rv = nsServiceManager::GetService(kRDFServiceCID,
                                                       kIRDFServiceIID,
                                                       (nsISupports**) &rdfService))) {
        rdfService->UnregisterDataSource(this);
        nsServiceManager::ReleaseService(kRDFServiceCID, rdfService);
    }
    Flush();
    NS_RELEASE(mInner);
}

NS_IMPL_ISUPPORTS(BookmarkDataSourceImpl, kIRDFDataSourceIID);

NS_IMETHODIMP
BookmarkDataSourceImpl::Init(const char* uri)
{
    nsresult rv;

    if (NS_FAILED(rv = nsRepository::CreateInstance(kRDFInMemoryDataSourceCID,
                                                    nsnull,
                                                    kIRDFDataSourceIID,
                                                    (void**) &mInner)))
        return rv;

    if (NS_FAILED(rv = mInner->Init(uri)))
        return rv;

    if (NS_FAILED(rv = ReadBookmarks()))
        return rv;

    // register this as a named data source with the RDF service
    nsIRDFService* rdfService;
    if (NS_SUCCEEDED(rv = nsServiceManager::GetService(kRDFServiceCID,
                                                       kIRDFServiceIID,
                                                       (nsISupports**) &rdfService))) {
        rdfService->RegisterDataSource(this);
        nsServiceManager::ReleaseService(kRDFServiceCID, rdfService);
    }

    return NS_OK;
}

NS_IMETHODIMP
BookmarkDataSourceImpl::Flush(void)
{
    return WriteBookmarks();
}


NS_IMETHODIMP
BookmarkDataSourceImpl::IsCommandEnabled(const char* aCommand,
                                         nsIRDFResource* aCommandTarget,
                                         PRBool* aResult)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
BookmarkDataSourceImpl::DoCommand(const char* aCommand,
                                  nsIRDFResource* aCommandTarget)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


nsresult
BookmarkDataSourceImpl::ReadBookmarks(void)
{
    nsresult rv = NS_ERROR_FAILURE;

    PRFileDesc* f;
    if ((f = PR_Open(kBookmarksFilename, PR_RDONLY, 0644)) != nsnull) {
        BookmarkParser parser;
        rv = parser.Parse(f, this);
        PR_Close(f);
    }

    return rv;
}


nsresult
BookmarkDataSourceImpl::WriteBookmarks(void)
{
    //PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

#if FIXME

static const nsString&
rdf_NumericDate(const nsString& url)
{
    nsAutoString result;
    PRInt32 len = url.Length();
    PRInt32 index = 0;

    while (index < len && url[index] < '0' && url[index] > '9')
        ++index;

    if (index >= len)
        return result;

    while (index < len && url[index] >= '0' && url[index] <= '9')
        result.Append(url[index++]);

    result;
}


void
HT_WriteOutAsBookmarks1 (RDF rdf, PRFileDesc *fp, RDF_Resource u, RDF_Resource top, int indent)
{
    RDF_Cursor c = RDF_GetSources(rdf, u, gCoreVocab->RDF_parent, RDF_RESOURCE_TYPE, true);
    RDF_Resource next;
    char *date, *name, *url;
    int loop;

    if (c == nsnull) return;
    if (u == top) {
      name = RDF_GetResourceName(rdf, u);
      ht_rjcprintf(fp, "<!DOCTYPE NETSCAPE-Bookmark-file-1>\n", nsnull);
      ht_rjcprintf(fp, "<!-- This is an automatically generated file.\n", nsnull);
      ht_rjcprintf(fp, "It will be read and overwritten.\n", nsnull);
      ht_rjcprintf(fp, "Do Not Edit! -->\n", nsnull);

      ht_rjcprintf(fp, "<TITLE>%s</TITLE>\n", (name) ? name:"");
      ht_rjcprintf(fp, "<H1>%s</H1>\n<DL><p>\n", (name) ? name:"");
    }
    while ((next = RDF_NextValue(c)) != nsnull) {

      url = resourceID(next);
      if (containerp(next) && (!startsWith("ftp:",url)) && (!startsWith("file:",url))
	    && (!startsWith("IMAP:", url)) && (!startsWith("nes:", url))
	    && (!startsWith("mail:", url)) && (!startsWith("cache:", url))
	    && (!startsWith("ldap:", url))) {
		for (loop=0; loop<indent; loop++)	ht_rjcprintf(fp, "    ", nsnull);

		date = numericDate(resourceID(next));
		ht_rjcprintf(fp, "<DT><H3 ADD_DATE=\"%s\">", (date) ? date:"");
		if (date) freeMem(date);
		name = RDF_GetResourceName(rdf, next);
		ht_rjcprintf(fp, "%s</H3>\n", name);

		for (loop=0; loop<indent; loop++)	ht_rjcprintf(fp, "    ", nsnull);
		ht_rjcprintf(fp, "<DL><p>\n", nsnull);
		HT_WriteOutAsBookmarks1(rdf, fp, next, top, indent+1);

		for (loop=0; loop<indent; loop++)	ht_rjcprintf(fp, "    ", nsnull);

		ht_rjcprintf(fp, "</DL><p>\n", nsnull);
      }
      else if (isSeparator(next)) {
	for (loop=0; loop<indent; loop++)	ht_rjcprintf(fp, "    ", nsnull);
	ht_rjcprintf(fp, "<HR>\n", nsnull);
      }
      else {
	char* bkAddDate = (char*)RDF_GetSlotValue(rdf, next, 
						  gNavCenter->RDF_bookmarkAddDate, 
						  RDF_STRING_TYPE, false, true);

        for (loop=0; loop<indent; loop++)	ht_rjcprintf(fp, "    ", nsnull);

	ht_rjcprintf(fp, "<DT><A HREF=\"%s\" ", resourceID(next));
	date = numericDate(bkAddDate);
	ht_rjcprintf(fp, "ADD_DATE=\"%s\" ", (date) ? date: "");
	if (date) freeMem(date);
	ht_rjcprintf(fp, "LAST_VISIT=\"%s\" ", resourceLastVisitDate(rdf, next));
	ht_rjcprintf(fp, "LAST_MODIFIED=\"%s\">", resourceLastModifiedDate(rdf, next));
	ht_rjcprintf(fp, "%s</A>\n", RDF_GetResourceName(rdf, next));

	if (resourceDescription(rdf, next) != nsnull) {
	  ht_rjcprintf(fp, "<DD>%s\n", resourceDescription(rdf, next));
	}
      }
    }
    RDF_DisposeCursor(c);
    if (u == top) {
      ht_rjcprintf(fp, "</DL>\n", nsnull);
    }
}
#endif

////////////////////////////////////////////////////////////////////////////////

#include "nsXPComFactory.h"

#if defined(TEMPLATES_IN_nsXPComFactory_EXORCISM)
NS_DEF_FACTORY(BookmarkDataSource,BookmarkDataSourceImpl)
#endif

nsresult
NS_NewRDFBookmarkDataSourceFactory(nsIFactory** aResult)
{
    nsresult rv = NS_OK;
#if defined(TEMPLATES_IN_nsXPComFactory_EXORCISM)
    nsIFactory* inst = new nsBookmarkDataSourceFactory;
#else
    nsIFactory* inst = new nsFactory<BookmarkDataSourceImpl>();
#endif
    if (NULL == inst) {
        rv = NS_ERROR_OUT_OF_MEMORY;
    } else {
        NS_ADDREF(inst);
    }
    *aResult = inst;
    return rv;
}

nsresult
NS_NewRDFBookmarkDataSource(nsIRDFDataSource** result)
{
    BookmarkDataSourceImpl* ds = new BookmarkDataSourceImpl();
    if (! ds)
        return NS_ERROR_NULL_POINTER;

    *result = ds;
    NS_ADDREF(*result);
    return NS_OK;
}
