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
#include "nsFileSpec.h"
#include "nsFileStream.h"
#include "nsSpecialSystemDirectory.h"
#include "nsRDFCID.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "prio.h"
#include "rdf.h"
#include "rdfutil.h"
#include "prlog.h"
#include "nsIComponentManager.h"

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
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, URL);

DEFINE_RDF_VOCAB(WEB_NAMESPACE_URI, WEB, LastVisitDate);
DEFINE_RDF_VOCAB(WEB_NAMESPACE_URI, WEB, LastModifiedDate);

DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, instanceOf);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, type);

static const char kPersonalToolbarFolder[]  = "Personal Toolbar Folder";


////////////////////////////////////////////////////////////////////////

/**
 * The bookmark parser knows how to read <tt>bookmarks.html</tt> and convert it
 * into an RDF graph.
 */
class BookmarkParser {
protected:
    static PRInt32 gRefCnt;

    static nsIRDFService* gRDFService;
    static nsIRDFResource* kNC_Bookmark;
    static nsIRDFResource* kNC_BookmarkAddDate;
    static nsIRDFResource* kNC_Description;
    static nsIRDFResource* kNC_Folder;
    static nsIRDFResource* kNC_Name;
    static nsIRDFResource* kNC_PersonalToolbarFolder;
    static nsIRDFResource* kNC_URL;
    static nsIRDFResource* kRDF_type;
    static nsIRDFResource* kWEB_LastModifiedDate;
    static nsIRDFResource* kWEB_LastVisitDate;

    nsInputFileStream&     mStream;
    nsIRDFDataSource*      mDataSource;

    nsresult AssertTime(nsIRDFResource* aSource,
                        nsIRDFResource* aLabel,
                        PRInt32 aTime);

    nsresult ParseBookmark(const nsString& aLine, nsIRDFResource* aContainer);
    nsresult ParseBookmarkHeader(const nsString& aLine, nsIRDFResource* aContainer);
    nsresult ParseBookmarkSeparator(const nsString& aLine);
    nsresult ParseHeaderBegin(const nsString& aLine, nsIRDFResource* aContainer);
    nsresult ParseHeaderEnd(const nsString& aLine);
    nsresult ParseAttribute(const nsString& aLine,
                            const char* aAttribute,
                            PRInt32 aAttributeLen,
                            nsString& aResult);

public:
    BookmarkParser(nsInputFileStream& aStream, nsIRDFDataSource* aDataSource);
    ~BookmarkParser();

    nsresult Parse(nsIRDFResource* aContainer);
};


PRInt32 BookmarkParser::gRefCnt;
nsIRDFService* BookmarkParser::gRDFService;
nsIRDFResource* BookmarkParser::kNC_Bookmark;
nsIRDFResource* BookmarkParser::kNC_BookmarkAddDate;
nsIRDFResource* BookmarkParser::kNC_Description;
nsIRDFResource* BookmarkParser::kNC_Folder;
nsIRDFResource* BookmarkParser::kNC_Name;
nsIRDFResource* BookmarkParser::kNC_PersonalToolbarFolder;
nsIRDFResource* BookmarkParser::kNC_URL;
nsIRDFResource* BookmarkParser::kRDF_type;
nsIRDFResource* BookmarkParser::kWEB_LastModifiedDate;
nsIRDFResource* BookmarkParser::kWEB_LastVisitDate;

BookmarkParser::BookmarkParser(nsInputFileStream& aStream, nsIRDFDataSource* aDataSource)
    : mStream(aStream), mDataSource(aDataSource)
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
        gRDFService->GetResource(kURINC_PersonalToolbarFolder, &kNC_PersonalToolbarFolder);
        gRDFService->GetResource(kURINC_URL,               &kNC_URL);
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
        NS_IF_RELEASE(kNC_PersonalToolbarFolder);
        NS_IF_RELEASE(kNC_URL);
        NS_IF_RELEASE(kRDF_type);
        NS_IF_RELEASE(kWEB_LastModifiedDate);
        NS_IF_RELEASE(kWEB_LastVisitDate);
    }
}

static const char kHREFEquals[]   = "HREF=\"";
static const char kCloseAnchor[] = "</A>";

static const char kOpenHeading[]  = "<H";
static const char kCloseHeading[] = "</H";

static const char kSeparator[]  = "<HR>";

static const char kOpenUL[]     = "<UL>";
static const char kCloseUL[]    = "</UL>";

static const char kOpenMenu[]   = "<MENU>";
static const char kCloseMenu[]  = "</MENU>";

static const char kOpenDL[]     = "<DL>";
static const char kCloseDL[]    = "</DL>";

static const char kTargetEquals[]       = "TARGET=\"";
static const char kAddDateEquals[]      = "ADD_DATE=\"";
static const char kLastVisitEquals[]    = "LAST_VISIT=\"";
static const char kLastModifiedEquals[] = "LAST_MODIFIED=\"";


nsresult
BookmarkParser::Parse(nsIRDFResource* aContainer)
{
	// tokenize the input stream.
	// XXX this needs to handle quotes, etc. it'd be nice to use the real parser for this...
    nsresult rv = NS_OK;
    nsAutoString line;
	while (NS_SUCCEEDED(rv) && !mStream.eof() && !mStream.failed()) {
        char c = mStream.get();
        line.Append(c);

        if (line.Last() != '\n' && !mStream.eof())
            continue; // keep building the string

        PRInt32 offset;

        if ((offset = line.Find(kHREFEquals)) >= 0) {
            rv = ParseBookmark(line, aContainer);
        }
        else if ((offset = line.Find(kOpenHeading)) >= 0 &&
                 nsString::IsDigit(line[offset + 2])) {
            // XXX Ignore <H1> so that bookmarks root _is_ <H1>
            if (line[offset + 2] != PRUnichar('1'))
                rv = ParseBookmarkHeader(line, aContainer);
        }
        else if ((offset = line.Find(kSeparator)) >= 0) {
            rv = ParseBookmarkSeparator(line);
        }
        else if ((offset = line.Find(kCloseUL)) >= 0 ||
                 (offset = line.Find(kCloseMenu)) >= 0 ||
                 (offset = line.Find(kCloseDL)) >= 0) {
            return ParseHeaderEnd(line);
        }
        else if ((offset = line.Find(kOpenUL)) >= 0 ||
                 (offset = line.Find(kOpenMenu)) >= 0 ||
                 (offset = line.Find(kOpenDL)) >= 0) {
            rv = ParseHeaderBegin(line, aContainer);
        }
        else {
            // XXX Discard the line. We should be treating this as the
            // description.
        }
        line.Truncate();
    }
    return rv;
}


nsresult
BookmarkParser::ParseBookmark(const nsString& aLine, nsIRDFResource* aContainer)
{
    NS_PRECONDITION(aContainer != nsnull, "null ptr");
    if (! aContainer)
        return NS_ERROR_NULL_POINTER;

    PRInt32 start = aLine.Find(kHREFEquals);
    NS_ASSERTION(start >= 0, "no 'HREF=\"' string: how'd we get here?");
    if (start < 0)
        return NS_ERROR_UNEXPECTED;

    // 1. Crop out the URL

    // Skip past the first double-quote
    start += (sizeof(kHREFEquals) - 1);

    // ...and find the next so we can chop the URL.
    PRInt32 end = aLine.Find(PRUnichar('"'), start);
    NS_ASSERTION(end >= 0, "unterminated string");
    if (end < 0)
        return NS_ERROR_UNEXPECTED;

    nsAutoString url;
    aLine.Mid(url, start, end - start);

    {
        // Now do properly replace %22's (this is what 4.5 did, anyway...)
        static const char kEscape22[] = "%22";
        PRInt32 offset;
        while ((offset = url.Find(kEscape22)) >= 0) {
            url.SetCharAt(' ',offset);
            url.Cut(offset + 1, sizeof(kEscape22) - 2);
        }
    }

    // XXX At this point, the URL may be relative. 4.5 called into
    // netlib to make an absolute URL, and there was some magic
    // "relative_URL" parameter that got sent down as well. We punt on
    // that stuff.

    // 2. Parse the name

    start = aLine.Find(PRUnichar('>'), end + 1); // 'end' still points to the end of the URL
    NS_ASSERTION(start >= 0, "open anchor tag not terminated");
    if (start < 0)
        return NS_ERROR_UNEXPECTED;

    nsAutoString name;
    aLine.Right(name, aLine.Length() - (start + 1));

    end = name.Find(kCloseAnchor);
    NS_ASSERTION(end >= 0, "anchor tag not terminated");
    if (end < 0)
        return NS_ERROR_UNEXPECTED;

    name.Truncate(end);

    // 3. Parse the target
    nsAutoString target;

    start = aLine.Find(kTargetEquals);
    if (start >= 0) {
        start += (sizeof(kTargetEquals) - 1);
        end = aLine.Find(PRUnichar('"'), start);
        aLine.Mid(target, start, end - start);
    }


    // 4. Parse the addition date
    PRInt32 addDate = 0;

    {
        nsAutoString s;
        ParseAttribute(aLine, kAddDateEquals, sizeof(kAddDateEquals) - 1, s);
        if (s.Length() > 0) {
            PRInt32 err;
            addDate = s.ToInteger(&err); // ignored.
        }
    }

    // 5. Parse the last visit date

    PRInt32 lastVisitDate = 0;

    {
        nsAutoString s;
        ParseAttribute(aLine, kLastVisitEquals, sizeof(kLastVisitEquals) - 1, s);
        if (s.Length() > 0) {
            PRInt32 err;
            lastVisitDate = s.ToInteger(&err); // ignored.
        }
    }

    // 5. Parse the last modified date

    PRInt32 lastModifiedDate;

    {
        nsAutoString s;
        ParseAttribute(aLine, kLastModifiedEquals, sizeof(kLastModifiedEquals) - 1, s);
        if (s.Length() > 0) {
            PRInt32 err;
            lastModifiedDate = s.ToInteger(&err); // ignored.
        }
    }

    // Dunno. 4.5 did it, so will we.
    if (!lastModifiedDate)
        lastModifiedDate = lastVisitDate;

    // XXX There was some other cruft here to deal with aliases, but
    // since I have no clue what those are, I'll punt.


    // Now create the bookmark
    nsresult rv;
    nsCOMPtr<nsIRDFResource> bookmark;
    if (NS_FAILED(rv = gRDFService->GetUnicodeResource(url, getter_AddRefs(bookmark) ))) {
        NS_ERROR("unable to get bookmark resource");
        return rv;
    }

    if (NS_FAILED(rv = mDataSource->Assert(bookmark, kRDF_type, kNC_Bookmark, PR_TRUE))) {
        NS_ERROR("unable to add bookmark to data source");
        return rv;
    }

    nsCOMPtr<nsIRDFLiteral> literal;
    if (NS_FAILED(rv = gRDFService->GetLiteral(name, getter_AddRefs(literal)))) {
        NS_ERROR("unable to create literal for bookmark name");
        return rv;
    }

    if (NS_FAILED(rv = mDataSource->Assert(bookmark, kNC_Name, literal, PR_TRUE))) {
        NS_ERROR("unable to set bookmark name");
        return rv;
    }

    if (NS_FAILED(rv = rdf_ContainerAppendElement(mDataSource, aContainer, bookmark))) {
        NS_ERROR("unable to add bookmark to container");
        return rv;
    }

    AssertTime(bookmark, kNC_BookmarkAddDate, addDate);
    AssertTime(bookmark, kWEB_LastVisitDate, lastVisitDate);
    AssertTime(bookmark, kWEB_LastModifiedDate, lastModifiedDate);

    return NS_OK;
}



nsresult
BookmarkParser::ParseBookmarkHeader(const nsString& aLine, nsIRDFResource* aContainer)
{
    // Snip out the header
    PRInt32 start = aLine.Find(kOpenHeading);
    NS_ASSERTION(start >= 0, "couldn't find '<H'; why am I here?");
    if (start < 0)
        return NS_ERROR_UNEXPECTED;

    start += (sizeof(kOpenHeading) - 1);
    start = aLine.Find(PRUnichar('>'), start); // skip to the end of the '<Hn>' tag

    if (start < 0) {
        NS_WARNING("couldn't find end of header tag");
        return NS_OK;
    }

    nsAutoString name;
    aLine.Right(name, aLine.Length() - (start + 1));

    PRInt32 end = name.Find(kCloseHeading);
    if (end < 0)
        NS_WARNING("No '</H' found to close the heading");

    if (end >= 0)
        name.Truncate(end);

    // Find the add date
    PRInt32 addDate = 0;

    nsAutoString s;
    ParseAttribute(aLine, kAddDateEquals, sizeof(kAddDateEquals) - 1, s);
    if (s.Length() > 0) {
        PRInt32 err;
        addDate = s.ToInteger(&err); // ignored
    }

    // Make the necessary assertions
    nsresult rv;
    nsCOMPtr<nsIRDFResource> folder;
    if (name.Equals(kPersonalToolbarFolder)) {
        folder = do_QueryInterface( kNC_PersonalToolbarFolder );
    }
    else {
        if (NS_FAILED(rv = rdf_CreateAnonymousResource(kURINC_BookmarksRoot, getter_AddRefs(folder)))) {
            NS_ERROR("unable to create anonymous resource for folder");
            return rv;
        }
    }

    nsCOMPtr<nsIRDFLiteral> literal;
    if (NS_FAILED(rv = gRDFService->GetLiteral(name, getter_AddRefs(literal)))) {
        NS_ERROR("unable to create literal for folder name");
        return rv;
    }

    if (NS_FAILED(rv = mDataSource->Assert(folder, kNC_Name, literal, PR_TRUE))) {
        NS_ERROR("unable to set folder name");
        return rv;
    }

    if (NS_FAILED(rv = rdf_ContainerAppendElement(mDataSource, aContainer, folder))) {
        NS_ERROR("unable to add new folder to parent");
        return rv;
    }

    if (NS_FAILED(rv = rdf_MakeSeq(mDataSource, folder))) {
        NS_ERROR("unable to make new folder as sequence");
        return rv;
    }

    if (NS_FAILED(rv = mDataSource->Assert(folder, kRDF_type, kNC_Folder, PR_TRUE))) {
        NS_ERROR("unable to mark new folder as folder");
        return rv;
    }

    if (NS_FAILED(rv = AssertTime(folder, kNC_BookmarkAddDate, addDate))) {
        NS_ERROR("unable to mark add date");
        return rv;
    }

    // And now recursively parse the rest of the file...
    
    if (NS_FAILED(rv = Parse(folder))) {
        NS_ERROR("recursive parse of bookmarks file failed");
        return rv;
    }

    return NS_OK;
}


nsresult
BookmarkParser::ParseBookmarkSeparator(const nsString& aLine)
{
    // XXX Not implemented
    return NS_OK;
}

nsresult
BookmarkParser::ParseHeaderBegin(const nsString& aLine, nsIRDFResource* aContainer)
{
    return NS_OK;
}



nsresult
BookmarkParser::ParseHeaderEnd(const nsString& aLine)
{
    return NS_OK;
}


nsresult
BookmarkParser::ParseAttribute(const nsString& aLine,
                               const char* aAttributeName,
                               PRInt32 aAttributeLen,
                               nsString& aResult)
{
    aResult.Truncate();

    PRInt32 start = aLine.Find(aAttributeName);
    if (start < 0)
        return NS_OK;

    start += aAttributeLen;
    PRInt32 end = aLine.Find(PRUnichar('"'), start);
    aLine.Mid(aResult, start, end - start);

    return NS_OK;
}

nsresult
BookmarkParser::AssertTime(nsIRDFResource* aSource,
                           nsIRDFResource* aLabel,
                           PRInt32 aTime)
{
    // XXX TO DO: Convert to a date literal

    nsAutoString time;
    time.Append(aTime, 10);

    nsresult rv;
    nsIRDFLiteral* literal;
    if (NS_FAILED(rv = gRDFService->GetLiteral(time, &literal))) {
        NS_ERROR("unable to get literal for time");
        return rv;
    }

    rv = mDataSource->Assert(aSource, aLabel, literal, PR_TRUE);
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
private:
    // pseudo-constants
    nsIRDFResource* kNC_URL;
    nsIRDFResource* kNC_BookmarksRoot;
    nsIRDFResource* kNC_Bookmark;
    nsIRDFResource* kRDF_type;

    nsIRDFService* mRDFService;

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
                         nsIRDFNode** target);

    NS_IMETHOD GetTargets(nsIRDFResource* source,
                          nsIRDFResource* property,
                          PRBool tv,
                          nsIRDFAssertionCursor** targets) {
        return mInner->GetTargets(source, property, tv, targets);
    }

    NS_IMETHOD Assert(nsIRDFResource* aSource, 
                      nsIRDFResource* aProperty, 
                      nsIRDFNode* aTarget,
                      PRBool aTruthValue);

    NS_IMETHOD Unassert(nsIRDFResource* aSource,
                        nsIRDFResource* aProperty,
                        nsIRDFNode* aTarget);

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

    NS_IMETHOD GetAllCommands(nsIRDFResource* source,
                              nsIEnumerator/*<nsIRDFResource>*/** commands);

    NS_IMETHOD IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                nsIRDFResource*   aCommand,
                                nsISupportsArray/*<nsIRDFResource>*/* aArguments);

    NS_IMETHOD DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                         nsIRDFResource*   aCommand,
                         nsISupportsArray/*<nsIRDFResource>*/* aArguments);

};

////////////////////////////////////////////////////////////////////////

BookmarkDataSourceImpl::BookmarkDataSourceImpl(void)
{
    // XXX rvg there should be only one instance of this class. 
    // this is actually true of all datasources.
    NS_INIT_REFCNT();

    nsresult rv = nsServiceManager::GetService(kRDFServiceCID,
                                               kIRDFServiceIID,
                                               (nsISupports**) &mRDFService);

    mRDFService->GetResource(kURINC_URL,           &kNC_URL);
    mRDFService->GetResource(kURINC_Bookmark,      &kNC_Bookmark);
    mRDFService->GetResource(kURINC_BookmarksRoot, &kNC_BookmarksRoot);
    mRDFService->GetResource(kURIRDF_type,         &kRDF_type);
}

BookmarkDataSourceImpl::~BookmarkDataSourceImpl(void)
{
    Flush();

    // unregister this from the RDF service
    mRDFService->UnregisterDataSource(this);
    nsServiceManager::ReleaseService(kRDFServiceCID, mRDFService);
    mRDFService = nsnull;

    NS_RELEASE(kNC_URL);
    NS_RELEASE(kNC_BookmarksRoot);

    NS_RELEASE(mInner);
}

NS_IMPL_ISUPPORTS(BookmarkDataSourceImpl, kIRDFDataSourceIID);

NS_IMETHODIMP
BookmarkDataSourceImpl::Init(const char* uri)
{
    nsresult rv;

    if (NS_FAILED(rv = nsComponentManager::CreateInstance(kRDFInMemoryDataSourceCID,
                                                    nsnull,
                                                    kIRDFDataSourceIID,
                                                    (void**) &mInner)))
        return rv;

    if (NS_FAILED(rv = mInner->Init(uri)))
        return rv;

    if (NS_FAILED(rv = ReadBookmarks()))
        return rv;

    // register this as a named data source with the RDF service
    return mRDFService->RegisterDataSource(this);
}

NS_IMETHODIMP
BookmarkDataSourceImpl::GetTarget(nsIRDFResource* aSource,
                                  nsIRDFResource* aProperty,
                                  PRBool aTruthValue,
                                  nsIRDFNode** aTarget)
{
    nsresult rv;

    // If they want the URL...
    if (aTruthValue && aProperty == kNC_URL) {
        // ...and it is in fact a bookmark...
        PRBool hasAssertion;
        if (NS_SUCCEEDED(mInner->HasAssertion(aSource, kRDF_type, kNC_Bookmark, PR_TRUE, &hasAssertion))
            && hasAssertion) {

            const char *uri;
            if (NS_FAILED(rv = aSource->GetValue(&uri))) {
                NS_ERROR("unable to get source's URI");
                return rv;
            }

            nsIRDFLiteral* literal;
            if (NS_FAILED(rv = mRDFService->GetLiteral(nsAutoString(uri), &literal))) {
                NS_ERROR("unable to construct literal for URL");
                return rv;
            }

            *aTarget = (nsIRDFNode*)literal;
            return NS_OK;
        }
    }

    return mInner->GetTarget(aSource, aProperty, aTruthValue, aTarget);
}


NS_IMETHODIMP
BookmarkDataSourceImpl::Assert(nsIRDFResource* aSource,
                               nsIRDFResource* aProperty,
                               nsIRDFNode* aTarget,
                               PRBool aTruthValue)
{
    // XXX TODO: filter out asserts we don't care about
    return mInner->Assert(aSource, aProperty, aTarget, aTruthValue);
}



NS_IMETHODIMP
BookmarkDataSourceImpl::Unassert(nsIRDFResource* aSource,
                                 nsIRDFResource* aProperty,
                                 nsIRDFNode* aTarget)
{
    // XXX TODO: filter out unasserts we don't care about
    return mInner->Unassert(aSource, aProperty, aTarget);
}




NS_IMETHODIMP
BookmarkDataSourceImpl::Flush(void)
{
    return WriteBookmarks();
}


NS_IMETHODIMP
BookmarkDataSourceImpl::GetAllCommands(nsIRDFResource* source,
                                       nsIEnumerator/*<nsIRDFResource>*/** commands)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
BookmarkDataSourceImpl::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                         nsIRDFResource*   aCommand,
                                         nsISupportsArray/*<nsIRDFResource>*/* aArguments)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
BookmarkDataSourceImpl::DoCommand(nsISupportsArray* aSources,
                                  nsIRDFResource*   aCommand,
                                  nsISupportsArray* aArguments)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
BookmarkDataSourceImpl::ReadBookmarks(void)
{
    nsresult rv;
    if (NS_FAILED(rv = rdf_MakeSeq(mInner, kNC_BookmarksRoot))) {
        NS_ERROR("Unaboe to make NC:BookmarksRoot a sequence");
        return rv;
    }

	nsSpecialSystemDirectory bookmarksFile(nsSpecialSystemDirectory::OS_CurrentProcessDirectory);

	// XXX we should get this from prefs.
    bookmarksFile += "res";
    bookmarksFile += "rdf";
    bookmarksFile += "bookmarks.html";

	nsInputFileStream strm(bookmarksFile);

	if (! strm.is_open()) {
		NS_ERROR("unable to open file");
		return NS_ERROR_FAILURE;
	}

	BookmarkParser parser(strm, this);
	parser.Parse(kNC_BookmarksRoot);
	return NS_OK;	
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

NS_DEF_FACTORY(BookmarkDataSource,BookmarkDataSourceImpl)

nsresult
NS_NewRDFBookmarkDataSourceFactory(nsIFactory** aResult)
{
    nsresult rv = NS_OK;
    nsIFactory* inst = new nsBookmarkDataSourceFactory;
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
