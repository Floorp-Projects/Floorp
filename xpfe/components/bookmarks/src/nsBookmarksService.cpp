/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-file-style: "stroustrup" -*-
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

  The global bookmarks service.

 */

#include "nsCOMPtr.h"
#include "nsFileSpec.h"
#include "nsFileStream.h"
#include "nsIBookmarksService.h"
#include "nsIComponentManager.h"
#include "nsIGenericFactory.h"
#include "nsIProfile.h"
#include "nsIRDFContainer.h"
#include "nsIRDFContainerUtils.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFNode.h"
#include "nsIRDFService.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIServiceManager.h"
#include "nsRDFCID.h"
#include "nsSpecialSystemDirectory.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "nsXPIDLString.h"
#include "prio.h"
#include "prlog.h"
#include "rdf.h"
#include "xp_core.h"
#include "prlong.h"

#include "nsEnumeratorUtils.h"
#include "nsEscape.h"

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_CID(kBookmarksServiceCID,      NS_BOOKMARKS_SERVICE_CID);
static NS_DEFINE_CID(kComponentManagerCID,      NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kGenericFactoryCID,        NS_GENERICFACTORY_CID);
static NS_DEFINE_CID(kProfileCID,               NS_PROFILE_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID, NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_CID(kRDFServiceCID,            NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFContainerCID,          NS_RDFCONTAINER_CID);
static NS_DEFINE_CID(kRDFContainerUtilsCID,     NS_RDFCONTAINERUTILS_CID);
static NS_DEFINE_IID(kISupportsIID,             NS_ISUPPORTS_IID);

static NS_DEFINE_IID(kIRDFResourceIID, NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kIRDFLiteralIID,  NS_IRDFLITERAL_IID);
static NS_DEFINE_IID(kIRDFIntIID,      NS_IRDFINT_IID);
static NS_DEFINE_IID(kIRDFDateIID,     NS_IRDFDATE_IID);

static const char kURINC_BookmarksRoot[]         = "NC:BookmarksRoot"; // XXX?
static const char kURINC_IEFavoritesRoot[]       = "NC:IEFavoritesRoot"; // XXX?
static const char kURINC_PersonalToolbarFolder[] = "NC:PersonalToolbarFolder"; // XXX?

static const char kPersonalToolbarFolder[]  = "Personal Toolbar Folder";

static const char	kBookmarkCommand[] = "http://home.netscape.com/NC-rdf#bookmarkcommand?";

////////////////////////////////////////////////////////////////////////

PRInt32 gRefCnt;
nsIRDFService		*gRDF;
nsIRDFContainerUtils	*gRDFC;

nsIRDFResource		*kNC_Bookmark;
nsIRDFResource		*kNC_BookmarkSeparator;
nsIRDFResource		*kNC_BookmarkAddDate;
nsIRDFResource		*kNC_BookmarksRoot;
nsIRDFResource		*kNC_Description;
nsIRDFResource		*kNC_Folder;
nsIRDFResource		*kNC_IEFavorite;
nsIRDFResource		*kNC_IEFavoritesRoot;
nsIRDFResource		*kNC_Name;
nsIRDFResource		*kNC_PersonalToolbarFolder;
nsIRDFResource		*kNC_ShortcutURL;
nsIRDFResource		*kNC_URL;
nsIRDFResource		*kRDF_type;
nsIRDFResource		*kWEB_LastModifiedDate;
nsIRDFResource		*kWEB_LastVisitDate;

nsIRDFResource		*kNC_Parent;

nsIRDFResource		*kNC_BookmarkCommand_NewBookmark;
nsIRDFResource		*kNC_BookmarkCommand_NewFolder;
nsIRDFResource		*kNC_BookmarkCommand_NewSeparator;
nsIRDFResource		*kNC_BookmarkCommand_DeleteBookmark;
nsIRDFResource		*kNC_BookmarkCommand_DeleteBookmarkFolder;
nsIRDFResource		*kNC_BookmarkCommand_DeleteBookmarkSeparator;



static nsresult
bm_AddRefGlobals()
{
	if (gRefCnt++ == 0) {
		nsresult rv;
		rv = nsServiceManager::GetService(kRDFServiceCID,
						  nsIRDFService::GetIID(),
						  (nsISupports**) &gRDF);

		NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF service");
		if (NS_FAILED(rv)) return rv;

		rv = nsServiceManager::GetService(kRDFContainerUtilsCID,
						  nsIRDFContainerUtils::GetIID(),
						  (nsISupports**) &gRDFC);

		NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF container utils");
		if (NS_FAILED(rv)) return rv;

		gRDF->GetResource(kURINC_BookmarksRoot,         &kNC_BookmarksRoot);
		gRDF->GetResource(kURINC_IEFavoritesRoot,       &kNC_IEFavoritesRoot);
		gRDF->GetResource(kURINC_PersonalToolbarFolder, &kNC_PersonalToolbarFolder);

		gRDF->GetResource(NC_NAMESPACE_URI "Bookmark",          &kNC_Bookmark);
		gRDF->GetResource(NC_NAMESPACE_URI "BookmarkSeparator", &kNC_BookmarkSeparator);
		gRDF->GetResource(NC_NAMESPACE_URI "BookmarkAddDate",   &kNC_BookmarkAddDate);
		gRDF->GetResource(NC_NAMESPACE_URI "Description",       &kNC_Description);
		gRDF->GetResource(NC_NAMESPACE_URI "Folder",            &kNC_Folder);
		gRDF->GetResource(NC_NAMESPACE_URI "IEFavorite",        &kNC_IEFavorite);
		gRDF->GetResource(NC_NAMESPACE_URI "Name",              &kNC_Name);
		gRDF->GetResource(NC_NAMESPACE_URI "ShortcutURL",       &kNC_ShortcutURL);
		gRDF->GetResource(NC_NAMESPACE_URI "URL",               &kNC_URL);
		gRDF->GetResource(RDF_NAMESPACE_URI "type",             &kRDF_type);
		gRDF->GetResource(WEB_NAMESPACE_URI "LastModifiedDate", &kWEB_LastModifiedDate);
		gRDF->GetResource(WEB_NAMESPACE_URI "LastVisitDate",    &kWEB_LastVisitDate);

		gRDF->GetResource(NC_NAMESPACE_URI "parent",            &kNC_Parent);

		gRDF->GetResource(NC_NAMESPACE_URI "bookmarkcommand?newbookmark",    &kNC_BookmarkCommand_NewBookmark);
		gRDF->GetResource(NC_NAMESPACE_URI "bookmarkcommand?newfolder",    &kNC_BookmarkCommand_NewFolder);
		gRDF->GetResource(NC_NAMESPACE_URI "bookmarkcommand?newseparator", &kNC_BookmarkCommand_NewSeparator);
		gRDF->GetResource(NC_NAMESPACE_URI "bookmarkcommand?deletebookmark", &kNC_BookmarkCommand_DeleteBookmark);
		gRDF->GetResource(NC_NAMESPACE_URI "bookmarkcommand?deletebookmarkfolder", &kNC_BookmarkCommand_DeleteBookmarkFolder);
		gRDF->GetResource(NC_NAMESPACE_URI "bookmarkcommand?deletebookmarkseparator", &kNC_BookmarkCommand_DeleteBookmarkSeparator);
	}
	return NS_OK;
}



static void
bm_ReleaseGlobals()
{
	if (--gRefCnt == 0)
	{
		if (gRDF)
		{
			nsServiceManager::ReleaseService(kRDFServiceCID, gRDF);
			gRDF = nsnull;
		}

		if (gRDFC)
		{
			nsServiceManager::ReleaseService(kRDFContainerUtilsCID, gRDFC);
			gRDFC = nsnull;
		}

		NS_IF_RELEASE(kNC_Bookmark);
		NS_IF_RELEASE(kNC_BookmarkSeparator);
		NS_IF_RELEASE(kNC_BookmarkAddDate);
		NS_IF_RELEASE(kNC_BookmarksRoot);
		NS_IF_RELEASE(kNC_Description);
		NS_IF_RELEASE(kNC_Folder);
		NS_IF_RELEASE(kNC_IEFavorite);
		NS_IF_RELEASE(kNC_IEFavoritesRoot);
		NS_IF_RELEASE(kNC_Name);
		NS_IF_RELEASE(kNC_PersonalToolbarFolder);
		NS_IF_RELEASE(kNC_ShortcutURL);
		NS_IF_RELEASE(kNC_URL);
		NS_IF_RELEASE(kRDF_type);
		NS_IF_RELEASE(kWEB_LastModifiedDate);
		NS_IF_RELEASE(kWEB_LastVisitDate);

		NS_IF_RELEASE(kNC_Parent);

		NS_IF_RELEASE(kNC_BookmarkCommand_NewBookmark);
		NS_IF_RELEASE(kNC_BookmarkCommand_NewFolder);
		NS_IF_RELEASE(kNC_BookmarkCommand_NewSeparator);
		NS_IF_RELEASE(kNC_BookmarkCommand_DeleteBookmark);
		NS_IF_RELEASE(kNC_BookmarkCommand_DeleteBookmarkFolder);
		NS_IF_RELEASE(kNC_BookmarkCommand_DeleteBookmarkSeparator);
	}
}



////////////////////////////////////////////////////////////////////////

class	nsBookmarksService;

/**
 * The bookmark parser knows how to read <tt>bookmarks.html</tt> and convert it
 * into an RDF graph.
 */
class BookmarkParser {
private:
	nsInputFileStream      *mStream;
	nsIRDFDataSource       *mDataSource;
	const char             *mIEFavoritesRoot;
	PRBool                 mFoundIEFavoritesRoot;

friend	class nsBookmarksService;

protected:
	nsresult AssertTime(nsIRDFResource* aSource,
			    nsIRDFResource* aLabel,
			    PRInt32 aTime);

	static nsresult CreateAnonymousResource(nsCOMPtr<nsIRDFResource>* aResult);

	nsresult ParseBookmark(const nsString& aLine,
			       nsCOMPtr<nsIRDFContainer>& aContainer,
			       nsIRDFResource *nodeType, nsIRDFResource **bookmarkNode);

	nsresult ParseBookmarkHeader(const nsString& aLine,
				     nsCOMPtr<nsIRDFContainer>& aContainer,
				     nsIRDFResource *nodeType);

	nsresult ParseBookmarkSeparator(const nsString& aLine,
					nsCOMPtr<nsIRDFContainer>& aContainer);

	nsresult ParseHeaderBegin(const nsString& aLine,
				  nsCOMPtr<nsIRDFContainer>& aContainer);

	nsresult ParseHeaderEnd(const nsString& aLine);

	nsresult ParseAttribute(const nsString& aLine,
				const char* aAttribute,
				PRInt32 aAttributeLen,
				nsString& aResult);

public:
	BookmarkParser();
	~BookmarkParser();

	nsresult Init(nsInputFileStream *aStream, nsIRDFDataSource *aDataSource);
	nsresult Parse(nsIRDFResource* aContainer, nsIRDFResource *nodeType);

	nsresult AddBookmark(nsCOMPtr<nsIRDFContainer>& aContainer,
			     const char*      aURL,
			     const PRUnichar* aOptionalTitle,
			     PRInt32          aAddDate,
			     PRInt32          aLastVisitDate,
			     PRInt32          aLastModifiedDate,
			     const char*      aShortcutURL,
			     nsIRDFResource*  aNodeType,
			     nsIRDFResource** bookmarkNode);

	nsresult SetIEFavoritesRoot(const char *IEFavoritesRootURL)
	{
		mIEFavoritesRoot = IEFavoritesRootURL;
		return(NS_OK);
	}
	nsresult ParserFoundIEFavoritesRoot(PRBool *foundIEFavoritesRoot)
	{
		*foundIEFavoritesRoot = mFoundIEFavoritesRoot;
		return(NS_OK);
	}
};



BookmarkParser::BookmarkParser()
{
    bm_AddRefGlobals();
}



nsresult
BookmarkParser::Init(nsInputFileStream *aStream, nsIRDFDataSource *aDataSource)
{
	mStream = aStream;
	mDataSource = aDataSource;
	mIEFavoritesRoot = nsnull;
	mFoundIEFavoritesRoot = PR_FALSE;
	return(NS_OK);
}



BookmarkParser::~BookmarkParser()
{
	bm_ReleaseGlobals();
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

static const char kOpenDD[]     = "<DD>";

static const char kTargetEquals[]       = "TARGET=\"";
static const char kAddDateEquals[]      = "ADD_DATE=\"";
static const char kLastVisitEquals[]    = "LAST_VISIT=\"";
static const char kLastModifiedEquals[] = "LAST_MODIFIED=\"";
static const char kShortcutURLEquals[]  = "SHORTCUTURL=\"";
static const char kIDEquals[]           = "ID=\"";



nsresult
BookmarkParser::Parse(nsIRDFResource* aContainer, nsIRDFResource *nodeType)
{
	// tokenize the input stream.
	// XXX this needs to handle quotes, etc. it'd be nice to use the real parser for this...
	nsRandomAccessInputStream	in(*mStream);
	nsresult			rv;

	nsCOMPtr<nsIRDFContainer> container;
	rv = nsComponentManager::CreateInstance(kRDFContainerCID,
						nsnull,
						nsIRDFContainer::GetIID(),
						getter_AddRefs(container));
	if (NS_FAILED(rv)) return rv;

	rv = container->Init(mDataSource, aContainer);
	if (NS_FAILED(rv)) return rv;

	nsCOMPtr<nsIRDFResource>	bookmarkNode = aContainer;
	nsAutoString			line, description;
	PRBool				inDescription = PR_FALSE;

	while (NS_SUCCEEDED(rv) && !in.eof() && !in.failed())
	{
		line.Truncate();

		while (PR_TRUE)
		{
			char	buf[256];
			PRBool untruncated = in.readline(buf, sizeof(buf));

			// in.readline() return PR_FALSE if there was buffer overflow,
			// or there was a catastrophe. Check to see if we're here
			// because of the latter...
			NS_ASSERTION (! in.failed(), "error reading file");
			if (in.failed()) return NS_ERROR_FAILURE;

			// XXX Bug 5871. What charset conversion should we be
			// applying here?
			line.Append(buf);

			if (untruncated)
				break;
		}

		PRInt32	offset;
		
		if (inDescription == PR_TRUE)
		{
			offset = line.FindChar('<');
			if (offset < 0)
			{
				if (description.Length() > 0)
				{
					description += "\n";
				}
				description += line;
				continue;
			}

			// handle description [convert some HTML-escaped (such as "&lt;") values back]
			
			while ((offset = description.Find("&lt;", PR_TRUE)) > 0)
			{
				description.Cut(offset, 4);
				description.Insert(PRUnichar('<'), offset);
			}
			while ((offset = description.Find("&gt;", PR_TRUE)) > 0)
			{
				description.Cut(offset, 4);
				description.Insert(PRUnichar('>'), offset);
			}
			while ((offset = description.Find("&amp;", PR_TRUE)) > 0)
			{
				description.Cut(offset, 5);
				description.Insert(PRUnichar('&'), offset);
			}

			if (bookmarkNode)
			{
				nsCOMPtr<nsIRDFLiteral>	descLiteral;
				if (NS_SUCCEEDED(rv = gRDF->GetLiteral(description.GetUnicode(), getter_AddRefs(descLiteral))))
				{
					rv = mDataSource->Assert(bookmarkNode, kNC_Description, descLiteral, PR_TRUE);
				}
			}

			inDescription = PR_FALSE;
			description.Truncate();
		}

		if ((offset = line.Find(kHREFEquals, PR_TRUE)) >= 0)
		{
			rv = ParseBookmark(line, container, nodeType, getter_AddRefs(bookmarkNode));
		}
		else if ((offset = line.Find(kOpenHeading, PR_TRUE)) >= 0 &&
			 nsString::IsDigit(line.CharAt(offset + 2)))
		{
			// XXX Ignore <H1> so that bookmarks root _is_ <H1>
			if (line.CharAt(offset + 2) != PRUnichar('1'))
				rv = ParseBookmarkHeader(line, container, nodeType);
		}
		else if ((offset = line.Find(kSeparator, PR_TRUE)) >= 0)
		{
			rv = ParseBookmarkSeparator(line, container);
		}
		else if ((offset = line.Find(kCloseUL, PR_TRUE)) >= 0 ||
			 (offset = line.Find(kCloseMenu, PR_TRUE)) >= 0 ||
			 (offset = line.Find(kCloseDL, PR_TRUE)) >= 0)
		{
			return ParseHeaderEnd(line);
		}
		else if ((offset = line.Find(kOpenUL, PR_TRUE)) >= 0 ||
			 (offset = line.Find(kOpenMenu, PR_TRUE)) >= 0 ||
			 (offset = line.Find(kOpenDL, PR_TRUE)) >= 0)
		{
			rv = ParseHeaderBegin(line, container);
		}
		else if ((offset = line.Find(kOpenDD, PR_TRUE)) >= 0)
		{
			inDescription = PR_TRUE;
			line.Cut(0, offset+sizeof(kOpenDD)-1);
			description = line;
		}
		else
		{
			// XXX Discard the line?
		}
	}
	return(rv);
}



nsresult
BookmarkParser::CreateAnonymousResource(nsCOMPtr<nsIRDFResource>* aResult)
{
	static PRInt32 gNext = 0;
	if (! gNext) {
		LL_L2I(gNext, PR_Now());
	}
	nsAutoString uri(kURINC_BookmarksRoot);
	uri.Append("#$");
	uri.Append(++gNext, 16);

	return gRDF->GetUnicodeResource(uri.GetUnicode(), getter_AddRefs(*aResult));
}



nsresult
BookmarkParser::ParseBookmark(const nsString& aLine, nsCOMPtr<nsIRDFContainer>& aContainer,
				nsIRDFResource *nodeType, nsIRDFResource **bookmarkNode)
{
	NS_PRECONDITION(aContainer != nsnull, "null ptr");
	if (! aContainer)
		return NS_ERROR_NULL_POINTER;

	PRInt32 start = aLine.Find(kHREFEquals, PR_TRUE);
	NS_ASSERTION(start >= 0, "no 'HREF=\"' string: how'd we get here?");
	if (start < 0)
		return NS_ERROR_UNEXPECTED;

	// 1. Crop out the URL

	// Skip past the first double-quote
	start += (sizeof(kHREFEquals) - 1);

	// ...and find the next so we can chop the URL.
	PRInt32 end = aLine.FindChar(PRUnichar('"'), PR_FALSE,start);
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

	start = aLine.FindChar(PRUnichar('>'), PR_FALSE,end + 1); // 'end' still points to the end of the URL
	NS_ASSERTION(start >= 0, "open anchor tag not terminated");
	if (start < 0)
		return NS_ERROR_UNEXPECTED;

	nsAutoString name;
	aLine.Right(name, aLine.Length() - (start + 1));

	end = name.Find(kCloseAnchor, PR_TRUE);
	NS_ASSERTION(end >= 0, "anchor tag not terminated");
	if (end < 0)
		return NS_ERROR_UNEXPECTED;

	name.Truncate(end);

	// 3. Parse the target
	nsAutoString target;

	start = aLine.Find(kTargetEquals, PR_TRUE);
	if (start >= 0) {
		start += (sizeof(kTargetEquals) - 1);
		end = aLine.FindChar(PRUnichar('"'), PR_FALSE,start);
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

	// 6. Parse the last modified date

	PRInt32 lastModifiedDate;

	{
		nsAutoString s;
		ParseAttribute(aLine, kLastModifiedEquals, sizeof(kLastModifiedEquals) - 1, s);
		if (s.Length() > 0) {
			PRInt32 err;
			lastModifiedDate = s.ToInteger(&err); // ignored.
		}
	}

	// 7. Parse the shortcut URL

	nsAutoString	shortcut;
	ParseAttribute(aLine, kShortcutURLEquals, sizeof(kShortcutURLEquals) -1, shortcut);


	// Dunno. 4.5 did it, so will we.
	if (!lastModifiedDate)
		lastModifiedDate = lastVisitDate;

	// XXX There was some other cruft here to deal with aliases, but
	// since I have no clue what those are, I'll punt.

	nsresult rv = NS_ERROR_OUT_OF_MEMORY; // in case ToNewCString() fails

	char *cURL = url.ToNewCString();
	if (cURL) {
		char *cShortcutURL = shortcut.ToNewCString();
		if (cShortcutURL) {
			rv = AddBookmark(aContainer,
					 cURL,
					 name.GetUnicode(),
					 addDate,
					 lastVisitDate,
					 lastModifiedDate,
					 cShortcutURL,
					 nodeType,
					 bookmarkNode);
		}
		delete [] cShortcutURL;
	}
	delete [] cURL;

	return rv;
}



    // Now create the bookmark
nsresult
BookmarkParser::AddBookmark(nsCOMPtr<nsIRDFContainer>&  aContainer,
                            const char*      aURL,
                            const PRUnichar* aOptionalTitle,
                            PRInt32          aAddDate,
                            PRInt32          aLastVisitDate,
                            PRInt32          aLastModifiedDate,
                            const char*      aShortcutURL,
                            nsIRDFResource*  aNodeType,
                            nsIRDFResource** bookmarkNode)
{
	nsresult rv;
	nsCOMPtr<nsIRDFResource> bookmark;

	if (NS_FAILED(rv = gRDF->GetResource(aURL, getter_AddRefs(bookmark) )))
	{
		NS_ERROR("unable to get bookmark resource");
		return rv;
	}

	if (bookmarkNode)
	{
		*bookmarkNode = bookmark;
		NS_ADDREF(*bookmarkNode);
	}

	if (nsnull != mIEFavoritesRoot)
	{
		if (!PL_strcmp(aURL, mIEFavoritesRoot))
		{
			mFoundIEFavoritesRoot = PR_TRUE;
		}
	}

	rv = mDataSource->Assert(bookmark, kRDF_type, aNodeType, PR_TRUE);
	if (rv != NS_RDF_ASSERTION_ACCEPTED)
	{
		NS_ERROR("unable to add bookmark to data source");
		return rv;
	}

	if ((nsnull != aOptionalTitle) && (*aOptionalTitle != PRUnichar('\0')))
	{
		nsCOMPtr<nsIRDFLiteral> literal;
		if (NS_FAILED(rv = gRDF->GetLiteral(aOptionalTitle, getter_AddRefs(literal))))
		{
			NS_ERROR("unable to create literal for bookmark name");
			return rv;
		}

		rv = mDataSource->Assert(bookmark, kNC_Name, literal, PR_TRUE);
		if (rv != NS_RDF_ASSERTION_ACCEPTED)
		{
			NS_ERROR("unable to set bookmark name");
			return rv;
		}
	}

	AssertTime(bookmark, kNC_BookmarkAddDate, aAddDate);
	AssertTime(bookmark, kWEB_LastVisitDate, aLastVisitDate);
	AssertTime(bookmark, kWEB_LastModifiedDate, aLastModifiedDate);

	if ((nsnull != aShortcutURL) && (*aShortcutURL != '\0'))
	{
		nsCOMPtr<nsIRDFLiteral> shortcutLiteral;
		if (NS_FAILED(rv = gRDF->GetLiteral(nsAutoString(aShortcutURL).GetUnicode(),
						    getter_AddRefs(shortcutLiteral))))
		{
			NS_ERROR("unable to get literal for bookmark shortcut URL");
			return(rv);
		}
		if (rv != NS_RDF_NO_VALUE)
		{
			rv = mDataSource->Assert(bookmark,
						 kNC_ShortcutURL,
						 shortcutLiteral,
						 PR_TRUE);

			if (rv != NS_RDF_ASSERTION_ACCEPTED)
			{
				NS_ERROR("unable to set bookmark shortcut URL");
				return(rv);
			}
		}
	}

	// XXX The last thing we do is add the bookmark to the
	// container. This ensures the minimal amount of reflow.
	rv = aContainer->AppendElement(bookmark);
	NS_ASSERTION(NS_SUCCEEDED(rv), "unable to add bookmark to container");
	if (NS_FAILED(rv)) return rv;

	return(NS_OK);
}



nsresult
BookmarkParser::ParseBookmarkHeader(const nsString& aLine,
				    nsCOMPtr<nsIRDFContainer>& aContainer,
				    nsIRDFResource *nodeType)
{
	// Snip out the header
	PRInt32 start = aLine.Find(kOpenHeading, PR_TRUE);
	NS_ASSERTION(start >= 0, "couldn't find '<H'; why am I here?");
	if (start < 0)
		return NS_ERROR_UNEXPECTED;

	start += (sizeof(kOpenHeading) - 1);
	start = aLine.FindChar(PRUnichar('>'), PR_FALSE,start); // skip to the end of the '<Hn>' tag

	if (start < 0) {
		NS_WARNING("couldn't find end of header tag");
		return NS_OK;
	}

	nsAutoString name;
	aLine.Right(name, aLine.Length() - (start + 1));

	PRInt32 end = name.Find(kCloseHeading, PR_TRUE);
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

	// Find the lastmod date
	PRInt32 lastmodDate = 0;

	ParseAttribute(aLine, kLastModifiedEquals, sizeof(kLastModifiedEquals) - 1, s);
	if (s.Length() > 0) {
		PRInt32 err;
		lastmodDate = s.ToInteger(&err); // ignored
	}

	nsAutoString id;
	ParseAttribute(aLine, kIDEquals, sizeof(kIDEquals) - 1, id);

	// Make the necessary assertions
	nsresult rv;
	nsCOMPtr<nsIRDFResource> folder;
	if (id.Length() > 0) {
		// Use the ID attribute, if one is set.
		rv = gRDF->GetUnicodeResource(id.GetUnicode(), getter_AddRefs(folder));
		NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create resource for folder");
		if (NS_FAILED(rv)) return rv;
	}
	else if (name.Equals(kPersonalToolbarFolder)) { // XXX I18n!!!
		folder = dont_QueryInterface( kNC_PersonalToolbarFolder );
	}
	else {
		// We've never seen this folder before. Assign it an anonymous ID
		rv = CreateAnonymousResource(&folder);
		NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create anonymous resource for folder");
		if (NS_FAILED(rv)) return rv;
	}

	nsCOMPtr<nsIRDFLiteral> literal;
	rv = gRDF->GetLiteral(name.GetUnicode(), getter_AddRefs(literal));
	NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create literal for folder name");
	if (NS_FAILED(rv)) return rv;

	rv = mDataSource->Assert(folder, kNC_Name, literal, PR_TRUE);
	if (rv != NS_RDF_ASSERTION_ACCEPTED) {
		NS_ERROR("unable to set folder name");
		return rv;
	}

	rv = gRDFC->MakeSeq(mDataSource, folder, nsnull);
	NS_ASSERTION(NS_SUCCEEDED(rv), "unable to make new folder as sequence");
	if (NS_FAILED(rv)) return rv;

	rv = mDataSource->Assert(folder, kRDF_type, kNC_Folder, PR_TRUE);
	if (rv != NS_RDF_ASSERTION_ACCEPTED) {
		NS_ERROR("unable to mark new folder as folder");
		return rv;
	}

	if (NS_FAILED(rv = AssertTime(folder, kNC_BookmarkAddDate, addDate))) {
		NS_ERROR("unable to mark add date");
		return rv;
	}
	if (NS_FAILED(rv = AssertTime(folder, kWEB_LastModifiedDate, lastmodDate))) {
		NS_ERROR("unable to mark lastmod date");
		return rv;
	}

	rv = aContainer->AppendElement(folder);
	NS_ASSERTION(NS_SUCCEEDED(rv), "unable to add folder to container");
	if (NS_FAILED(rv)) return rv;

	// And now recursively parse the rest of the file...

	if (NS_FAILED(rv = Parse(folder, nodeType))) {
		NS_ERROR("recursive parse of bookmarks file failed");
		return rv;
	}

	return NS_OK;
}



nsresult
BookmarkParser::ParseBookmarkSeparator(const nsString& aLine, nsCOMPtr<nsIRDFContainer>& aContainer)
{
	nsresult			rv;
	nsCOMPtr<nsIRDFResource>	separator;

	if (NS_SUCCEEDED(rv = CreateAnonymousResource(&separator)))
	{
		nsAutoString		defaultSeparatorName("-----");
		nsCOMPtr<nsIRDFLiteral> nameLiteral;
		if (NS_SUCCEEDED(rv = gRDF->GetLiteral(defaultSeparatorName.GetUnicode(), getter_AddRefs(nameLiteral))))
		{
			if (NS_SUCCEEDED(rv = mDataSource->Assert(separator, kNC_Name, nameLiteral, PR_TRUE)))
			{
			}
		}
		if (NS_SUCCEEDED(rv = mDataSource->Assert(separator, kRDF_type, kNC_BookmarkSeparator, PR_TRUE)))
		{
			rv = aContainer->AppendElement(separator);
			if (NS_FAILED(rv)) return rv;
		}
	}
	return(rv);
}



nsresult
BookmarkParser::ParseHeaderBegin(const nsString& aLine, nsCOMPtr<nsIRDFContainer>& aContainer)
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

	PRInt32 start = aLine.Find(aAttributeName, PR_TRUE);
	if (start < 0)
		return NS_OK;

	start += aAttributeLen;
	PRInt32 end = aLine.FindChar(PRUnichar('"'), PR_FALSE,start);
	aLine.Mid(aResult, start, end - start);

	return NS_OK;
}



nsresult
BookmarkParser::AssertTime(nsIRDFResource* aSource,
                           nsIRDFResource* aLabel,
                           PRInt32 aTime)
{
	nsresult	rv = NS_OK;

	if (aTime != 0)
	{
		// Convert to a date literal
		PRInt64		dateVal, temp, million;

		LL_I2L(temp, aTime);
		LL_I2L(million, PR_USEC_PER_SEC);
		LL_MUL(dateVal, temp, million);		// convert from seconds to microseconds (PRTime)

		nsCOMPtr<nsIRDFDate>	dateLiteral;
		if (NS_FAILED(rv = gRDF->GetDateLiteral(dateVal, getter_AddRefs(dateLiteral))))
		{
			NS_ERROR("unable to get date literal for time");
			return(rv);
		}

		rv = mDataSource->Assert(aSource, aLabel, dateLiteral, PR_TRUE);
		NS_ASSERTION(rv == NS_RDF_ASSERTION_ACCEPTED, "unable to assert time");
	}
	return(rv);
}



////////////////////////////////////////////////////////////////////////
// BookmarkDataSourceImpl



class nsBookmarksService : public nsIBookmarksService,
			   public nsIRDFDataSource,
			   public nsIRDFRemoteDataSource
{
protected:
	nsCOMPtr<nsIRDFDataSource> mInner;

	nsresult GetBookmarksFile(nsFileSpec* aResult);
	nsresult ReadBookmarks();
	nsresult WriteBookmarks(nsIRDFDataSource *ds, nsIRDFResource *root);
	nsresult WriteBookmarksContainer(nsIRDFDataSource *ds, nsOutputFileStream strm, nsIRDFResource *container, PRInt32 level);
	nsresult GetTextForNode(nsIRDFNode* aNode, nsString& aResult);
	nsresult UpdateBookmarkLastModifiedDate(nsIRDFResource *aSource);
	nsresult WriteBookmarkProperties(nsIRDFDataSource *ds, nsOutputFileStream strm, nsIRDFResource *node,
					 nsIRDFResource *property, const char *htmlAttrib, PRBool isFirst);
	PRBool CanAccept(nsIRDFResource* aSource, nsIRDFResource* aProperty, nsIRDFNode* aTarget);

	nsresult getArgumentN(nsISupportsArray *arguments, nsIRDFResource *res, PRInt32 offset, nsIRDFResource **argValue);
	nsresult insertBookmarkItem(nsIRDFResource *src, nsISupportsArray *aArguments, PRInt32 parentArgIndex, nsIRDFResource *objType);
	nsresult deleteBookmarkItem(nsIRDFResource *src, nsISupportsArray *aArguments, PRInt32 parentArgIndex, nsIRDFResource *objType);

	nsresult getResourceFromLiteralNode(nsIRDFNode *node, nsIRDFResource **res);

	nsresult ChangeURL(nsIRDFResource* aSource,
                           nsIRDFResource* aProperty,
                           nsIRDFNode* aOldTarget,
                           nsIRDFNode* aNewTarget);

	nsBookmarksService();
	virtual ~nsBookmarksService();
	nsresult Init();

	friend NS_IMETHODIMP
	NS_NewBookmarksService(nsISupports* aOuter, REFNSIID aIID, void** aResult);

public:
	// nsISupports
	NS_DECL_ISUPPORTS

	// nsIBookmarksService
	NS_DECL_NSIBOOKMARKSSERVICE

	// nsIRDFDataSource
	NS_IMETHOD GetURI(char* *uri);

	NS_IMETHOD GetSource(nsIRDFResource* property,
			     nsIRDFNode* target,
			     PRBool tv,
			     nsIRDFResource** source) {
		return mInner->GetSource(property, target, tv, source);
	}

	NS_IMETHOD GetSources(nsIRDFResource* property,
			      nsIRDFNode* target,
			      PRBool tv,
			      nsISimpleEnumerator** sources) {
		return mInner->GetSources(property, target, tv, sources);
	}

	NS_IMETHOD GetTarget(nsIRDFResource* source,
			     nsIRDFResource* property,
			     PRBool tv,
			     nsIRDFNode** target);

	NS_IMETHOD GetTargets(nsIRDFResource* source,
			      nsIRDFResource* property,
			      PRBool tv,
			      nsISimpleEnumerator** targets) {
		return mInner->GetTargets(source, property, tv, targets);
	}

	NS_IMETHOD Assert(nsIRDFResource* aSource,
			  nsIRDFResource* aProperty,
			  nsIRDFNode* aTarget,
			  PRBool aTruthValue);

	NS_IMETHOD Unassert(nsIRDFResource* aSource,
			    nsIRDFResource* aProperty,
			    nsIRDFNode* aTarget);

	NS_IMETHOD Change(nsIRDFResource* aSource,
			  nsIRDFResource* aProperty,
			  nsIRDFNode* aOldTarget,
			  nsIRDFNode* aNewTarget);

	NS_IMETHOD Move(nsIRDFResource* aOldSource,
			nsIRDFResource* aNewSource,
			nsIRDFResource* aProperty,
			nsIRDFNode* aTarget);

	NS_IMETHOD HasAssertion(nsIRDFResource* source,
				nsIRDFResource* property,
				nsIRDFNode* target,
				PRBool tv,
				PRBool* hasAssertion) {
		return mInner->HasAssertion(source, property, target, tv, hasAssertion);
	}

	NS_IMETHOD AddObserver(nsIRDFObserver* aObserver) {
		return mInner->AddObserver(aObserver);
	}

	NS_IMETHOD RemoveObserver(nsIRDFObserver* aObserver) {
		return mInner->RemoveObserver(aObserver);
	}

	NS_IMETHOD ArcLabelsIn(nsIRDFNode* node,
			       nsISimpleEnumerator** labels) {
		return mInner->ArcLabelsIn(node, labels);
	}

	NS_IMETHOD ArcLabelsOut(nsIRDFResource* source,
				nsISimpleEnumerator** labels) {
		return mInner->ArcLabelsOut(source, labels);
	}

	NS_IMETHOD GetAllResources(nsISimpleEnumerator** aResult) {
		return mInner->GetAllResources(aResult);
	}

	NS_IMETHOD GetAllCommands(nsIRDFResource* source,
				  nsIEnumerator/*<nsIRDFResource>*/** commands);

	NS_IMETHOD GetAllCmds(nsIRDFResource* source,
                              nsISimpleEnumerator/*<nsIRDFResource>*/** commands);

	NS_IMETHOD IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
				    nsIRDFResource*   aCommand,
				    nsISupportsArray/*<nsIRDFResource>*/* aArguments,
				    PRBool* aResult);

	NS_IMETHOD DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
			     nsIRDFResource*   aCommand,
			     nsISupportsArray/*<nsIRDFResource>*/* aArguments);

	// nsIRDFRemoteDataSource
	NS_IMETHOD Init(const char* aURI);
	NS_IMETHOD Refresh(PRBool aBlocking);
	NS_IMETHOD Flush();
};



////////////////////////////////////////////////////////////////////////



nsBookmarksService::nsBookmarksService()
{
	NS_INIT_REFCNT();
}



nsBookmarksService::~nsBookmarksService()
{
	Flush();
	bm_ReleaseGlobals();
}



nsresult
nsBookmarksService::Init()
{
	nsresult rv;
	rv = bm_AddRefGlobals();
	if (NS_FAILED(rv)) return rv;

	rv = nsComponentManager::CreateInstance(kRDFInMemoryDataSourceCID,
		nsnull, nsIRDFDataSource::GetIID(), (void**) &mInner);
	if (NS_FAILED(rv)) return rv;

	rv = ReadBookmarks();
	if (NS_FAILED(rv)) return rv;

	// register this as a named data source with the RDF service
	rv = gRDF->RegisterDataSource(this, PR_FALSE);
	if (NS_FAILED(rv)) return rv;

	return NS_OK;
}



NS_IMETHODIMP
NS_NewBookmarksService(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
	NS_PRECONDITION(aResult != nsnull, "null ptr");
	if (! aResult)
		return NS_ERROR_NULL_POINTER;

	NS_PRECONDITION(aOuter == nsnull, "no aggregation");
	if (aOuter)
		return NS_ERROR_NO_AGGREGATION;

	nsresult rv = NS_OK;

	nsBookmarksService* result = new nsBookmarksService();
	if (! result)
		return NS_ERROR_OUT_OF_MEMORY;

	rv = result->Init();
	if (NS_SUCCEEDED(rv))
		rv = result->QueryInterface(aIID, aResult);

	if (NS_FAILED(rv)) {
		delete result;
		*aResult = nsnull;
		return rv;
	}

	return rv;
}



////////////////////////////////////////////////////////////////////////



NS_IMPL_ADDREF(nsBookmarksService);
NS_IMPL_RELEASE(nsBookmarksService);



NS_IMETHODIMP
nsBookmarksService::QueryInterface(REFNSIID aIID, void **aResult)
{
	NS_PRECONDITION(aResult != nsnull, "null ptr");
	if (! aResult)
		return NS_ERROR_NULL_POINTER;

	if (aIID.Equals(nsIBookmarksService::GetIID()) ||
	    aIID.Equals(kISupportsIID))
	{
		*aResult = NS_STATIC_CAST(nsIBookmarksService*, this);
	}
	else if (aIID.Equals(nsIRDFDataSource::GetIID())) {
		*aResult = NS_STATIC_CAST(nsIRDFDataSource*, this);
	}
	else if (aIID.Equals(nsIRDFRemoteDataSource::GetIID())) {
		*aResult = NS_STATIC_CAST(nsIRDFRemoteDataSource*, this);
	}
	else {
		*aResult = nsnull;
		return NS_NOINTERFACE;
	}

	NS_ADDREF(this);
	return NS_OK;
}



////////////////////////////////////////////////////////////////////////
// nsIBookmarksService



NS_IMETHODIMP
nsBookmarksService::AddBookmark(const char *aURI, const PRUnichar *aOptionalTitle)
{
	// XXX For the moment, just add it as a child of
	// BookmarksRoot. Constructing a parser object to do this is so
	// gross. We need to factor AddBookmark() into its own little
	// routine or something.
	BookmarkParser parser;
	parser.Init(nsnull, mInner);

	nsresult rv;

	nsCOMPtr<nsIRDFContainer> container;
	rv = nsComponentManager::CreateInstance(kRDFContainerCID,
						nsnull,
						nsIRDFContainer::GetIID(),
						getter_AddRefs(container));
	if (NS_FAILED(rv)) return rv;

	rv = container->Init(mInner, kNC_BookmarksRoot);
	if (NS_FAILED(rv)) return rv;

	// convert the current date/time from microseconds (PRTime) to seconds
	PRTime		now64 = PR_Now(), million;
	LL_I2L(million, PR_USEC_PER_SEC);
	LL_DIV(now64, now64, million);
	PRInt32		now32;
	LL_L2I(now32, now64);

	rv = parser.AddBookmark(container, aURI, aOptionalTitle,
				now32, 0L, 0L, nsnull, kNC_Bookmark, nsnull);

	if (NS_FAILED(rv)) return rv;

	rv = Flush();
	if (NS_FAILED(rv)) return rv;

	return NS_OK;
}



NS_IMETHODIMP
nsBookmarksService::UpdateBookmarkLastVisitedDate(const char *aURL)
{
	nsCOMPtr<nsIRDFResource>	bookmark;
	nsresult			rv;

	if (NS_SUCCEEDED(rv = gRDF->GetResource(aURL, getter_AddRefs(bookmark) )))
	{
		PRBool			isBookmark = PR_FALSE;

		// Note: always use mInner!! Otherwise, could get into an infinite loop
		// due to Assert/Change calling UpdateBookmarkLastModifiedDate()

		if (NS_SUCCEEDED(rv = mInner->HasAssertion(bookmark, kRDF_type, kNC_Bookmark,
			PR_TRUE, &isBookmark)) && (isBookmark == PR_TRUE))
		{
			nsCOMPtr<nsIRDFDate>	now;

			if (NS_SUCCEEDED(rv = gRDF->GetDateLiteral(PR_Now(), getter_AddRefs(now))))
			{
				nsCOMPtr<nsIRDFNode>	lastMod;

				if (NS_SUCCEEDED(rv = mInner->GetTarget(bookmark, kWEB_LastVisitDate, PR_TRUE,
					getter_AddRefs(lastMod))) && (rv != NS_RDF_NO_VALUE))
				{
					rv = mInner->Change(bookmark, kWEB_LastVisitDate, lastMod, now);
				}
				else
				{
					rv = mInner->Assert(bookmark, kWEB_LastVisitDate, now, PR_TRUE);
				}
				
				// XXX For the moment, do an immediate flush.
				if (NS_SUCCEEDED(rv))
				{
					Flush();
				}
			}
		}
	}
	return(rv);
}



nsresult
nsBookmarksService::UpdateBookmarkLastModifiedDate(nsIRDFResource *aSource)
{
	nsCOMPtr<nsIRDFDate>	now;
	nsresult		rv;

	if (NS_SUCCEEDED(rv = gRDF->GetDateLiteral(PR_Now(), getter_AddRefs(now))))
	{
		nsCOMPtr<nsIRDFNode>	lastMod;

		// Note: always use mInner!! Otherwise, could get into an infinite loop
		// due to Assert/Change calling UpdateBookmarkLastModifiedDate()

		if (NS_SUCCEEDED(rv = mInner->GetTarget(aSource, kWEB_LastModifiedDate, PR_TRUE,
			getter_AddRefs(lastMod))) && (rv != NS_RDF_NO_VALUE))
		{
			rv = mInner->Change(aSource, kWEB_LastModifiedDate, lastMod, now);
		}
		else
		{
			rv = mInner->Assert(aSource, kWEB_LastModifiedDate, now, PR_TRUE);
		}
	}
	return(rv);
}



NS_IMETHODIMP
nsBookmarksService::FindShortcut(const PRUnichar *aUserInput, char **aShortcutURL)
{
	NS_PRECONDITION(aUserInput != nsnull, "null ptr");
	if (! aUserInput)
		return NS_ERROR_NULL_POINTER;

	NS_PRECONDITION(aShortcutURL != nsnull, "null ptr");
	if (! aShortcutURL)
		return NS_ERROR_NULL_POINTER;

	nsresult rv;

	nsCOMPtr<nsIRDFLiteral> literalTarget;
	rv = gRDF->GetLiteral(aUserInput, getter_AddRefs(literalTarget));
	if (NS_FAILED(rv)) return rv;

	if (rv != NS_RDF_NO_VALUE)
	{
		nsCOMPtr<nsIRDFResource> source;
		rv = GetSource(kNC_ShortcutURL, literalTarget,
			       PR_TRUE, getter_AddRefs(source));

		if (NS_FAILED(rv)) return rv;

		if (rv != NS_RDF_NO_VALUE)
		{
			rv = source->GetValue(aShortcutURL);
			if (NS_FAILED(rv)) return rv;

			return NS_OK;
		}
	}

	*aShortcutURL = nsnull;
	return NS_RDF_NO_VALUE;
}



////////////////////////////////////////////////////////////////////////
// nsIRDFDataSource



NS_IMETHODIMP
nsBookmarksService::GetURI(char* *aURI)
{
	*aURI = nsXPIDLCString::Copy("rdf:bookmarks");
	if (! *aURI)
		return NS_ERROR_OUT_OF_MEMORY;

	return NS_OK;
}



static PRBool
isBookmarkCommand(nsIRDFResource *r)
{
	PRBool		isBookmarkCommandFlag = PR_FALSE;
	const char	*uri = nsnull;
	
	if (NS_SUCCEEDED(r->GetValueConst( &uri )) && (uri))
	{
		if (!strncmp(uri, kBookmarkCommand, sizeof(kBookmarkCommand) - 1))
		{
			isBookmarkCommandFlag = PR_TRUE;
		}
	}
	return(isBookmarkCommandFlag);
}



NS_IMETHODIMP
nsBookmarksService::GetTarget(nsIRDFResource* aSource,
			      nsIRDFResource* aProperty,
			      PRBool aTruthValue,
			      nsIRDFNode** aTarget)
{
	nsresult	rv;

	// If they want the URL...
	if (aTruthValue && aProperty == kNC_URL)
	{
		// ...and it is in fact a bookmark...
		PRBool hasAssertion;
		if ((NS_SUCCEEDED(mInner->HasAssertion(aSource, kRDF_type, kNC_Bookmark, PR_TRUE, &hasAssertion))
		    && hasAssertion) ||
		    (NS_SUCCEEDED(mInner->HasAssertion(aSource, kRDF_type, kNC_IEFavorite, PR_TRUE, &hasAssertion))
		    && hasAssertion))
		{
			const char	*uri;
			if (NS_FAILED(rv = aSource->GetValueConst( &uri )))
			{
				NS_ERROR("unable to get source's URI");
				return rv;
			}

			nsAutoString	ncURI(uri);
			if (ncURI.Find("NC:", PR_TRUE) == 0)
			{
				return(NS_RDF_NO_VALUE);
			}

			nsIRDFLiteral* literal;
			if (NS_FAILED(rv = gRDF->GetLiteral(nsAutoString(uri).GetUnicode(), &literal)))
			{
				NS_ERROR("unable to construct literal for URL");
				return rv;
			}

			*aTarget = (nsIRDFNode*)literal;
			return NS_OK;
		}
	}
	else if (aTruthValue && isBookmarkCommand(aSource) && (aProperty == kNC_Name))
	{
		nsAutoString	name;
		if (aSource == kNC_BookmarkCommand_NewBookmark)
		{
			name = "New Bookmark";	// XXX localization
		}
		else if (aSource == kNC_BookmarkCommand_NewFolder)
		{
			name = "New Folder";		// XXX localization
		}
		else if (aSource == kNC_BookmarkCommand_NewSeparator)
		{
			name = "New Separator";	// XXX localization
		}
		else if (aSource == kNC_BookmarkCommand_DeleteBookmark)
		{
			name = "Delete Bookmark";	// XXX localization
		}
		else if (aSource == kNC_BookmarkCommand_DeleteBookmarkFolder)
		{
			name = "Delete Folder";	// XXX localization
		}
		else if (aSource == kNC_BookmarkCommand_DeleteBookmarkSeparator)
		{
			name = "Delete Separator";	// XXX localization
		}

		if (name.Length() > 0)
		{
			nsIRDFLiteral	*literal;
			rv = gRDF->GetLiteral(name.GetUnicode(), &literal);

			rv = literal->QueryInterface(nsIRDFNode::GetIID(), (void**) aTarget);
			NS_RELEASE(literal);

			return rv;
		}
	}

	return mInner->GetTarget(aSource, aProperty, aTruthValue, aTarget);
}



NS_IMETHODIMP
nsBookmarksService::Assert(nsIRDFResource* aSource,
			   nsIRDFResource* aProperty,
			   nsIRDFNode* aTarget,
			   PRBool aTruthValue)
{
	nsresult	rv = NS_RDF_ASSERTION_REJECTED;

	if (CanAccept(aSource, aProperty, aTarget))
	{
		if (NS_SUCCEEDED(rv = mInner->Assert(aSource, aProperty, aTarget, aTruthValue)))
		{
			UpdateBookmarkLastModifiedDate(aSource);
		}
	}
	return(rv);
}



NS_IMETHODIMP
nsBookmarksService::Unassert(nsIRDFResource* aSource,
			     nsIRDFResource* aProperty,
			     nsIRDFNode* aTarget)
{
	nsresult	rv = NS_RDF_ASSERTION_REJECTED;

	if (CanAccept(aSource, aProperty, aTarget))
	{
		if (NS_SUCCEEDED(rv = mInner->Unassert(aSource, aProperty, aTarget)))
		{
			UpdateBookmarkLastModifiedDate(aSource);
		}
	}
	return(rv);
}



nsresult
nsBookmarksService::getResourceFromLiteralNode(nsIRDFNode *node, nsIRDFResource **res)
{
	nsresult	rv;

	nsCOMPtr<nsIRDFLiteral>		newURLLit = do_QueryInterface(node);
	if (!newURLLit)
	{
		return(NS_ERROR_INVALID_ARG);
	}
	const PRUnichar			*newURL = nsnull;
	newURLLit->GetValueConst(&newURL);
	if (!newURL)
	{
		return(NS_ERROR_NULL_POINTER);
	}
	nsAutoString			newURLStr(newURL);
	char				*newURLCStr = newURLStr.ToNewCString();
	if (!newURLCStr)
	{
		return(NS_ERROR_NULL_POINTER);
	}
	rv = gRDF->GetResource(newURLCStr, res);
	delete [] newURLCStr;
	return(rv);
}



nsresult
nsBookmarksService::ChangeURL(nsIRDFResource *aSource, nsIRDFResource *aProperty,
                           nsIRDFNode *aOldTarget, nsIRDFNode *aNewTarget)
{
	nsresult			rv;
	nsCOMPtr<nsIRDFResource>	newURLRes, oldURLRes;

	if (NS_FAILED(rv = getResourceFromLiteralNode(aOldTarget, getter_AddRefs(oldURLRes))))
		return(rv);
	if (NS_FAILED(rv = getResourceFromLiteralNode(aNewTarget, getter_AddRefs(newURLRes))))
		return(rv);
	
	// make all arcs coming out of oldURLRes also come out of newURLRes
	// (if aNewTarget doesn't already have a value for the arc)
	
	nsCOMPtr<nsISimpleEnumerator>	arcsOut;
	if (NS_FAILED(rv = ArcLabelsOut(oldURLRes, getter_AddRefs(arcsOut))))
	{
		return(rv);
	}
	PRBool		hasMoreArcsOut = PR_TRUE;
	while(hasMoreArcsOut == PR_TRUE)
	{
		if (NS_FAILED(rv = arcsOut->HasMoreElements(&hasMoreArcsOut)))
			break;
		if (hasMoreArcsOut == PR_FALSE)	break;

		nsCOMPtr<nsISupports>	aArc;
		if (NS_FAILED(rv = arcsOut->GetNext(getter_AddRefs(aArc))))
			break;
		nsCOMPtr<nsIRDFResource>	aProperty = do_QueryInterface(aArc);
		if (!aProperty)	continue;

		// don't copy URL property as it is special
		if (aProperty.get() == kNC_URL)	continue;

		nsCOMPtr<nsIRDFNode>	aValue;
		if (NS_SUCCEEDED(rv = GetTarget(newURLRes, aProperty, PR_TRUE, getter_AddRefs(aValue)))
			&& (rv == NS_RDF_NO_VALUE))
		{
			if (NS_SUCCEEDED(rv = GetTarget(oldURLRes, aProperty, PR_TRUE, getter_AddRefs(aValue)))
				&& (rv != NS_RDF_NO_VALUE))
			{
				if (NS_FAILED(rv = Assert(newURLRes, aProperty, aValue, PR_TRUE)))
				{
					continue;
				}
			}
		}
	}
	
	// make all arcs pointing to oldURLRes now point to newURLRes

	nsCOMPtr<nsISimpleEnumerator>	arcsIn;
	if (NS_FAILED(rv = ArcLabelsIn(oldURLRes, getter_AddRefs(arcsIn))))
	{
		return(rv);
	}
	PRBool		hasMoreArcsIn = PR_TRUE;
	while(hasMoreArcsIn == PR_TRUE)
	{
		if (NS_FAILED(rv = arcsIn->HasMoreElements(&hasMoreArcsIn)))
			break;
		if (hasMoreArcsIn == PR_FALSE)	break;
		
		nsCOMPtr<nsISupports>	aArc;
		if (NS_FAILED(rv = arcsIn->GetNext(getter_AddRefs(aArc))))
			break;
		nsCOMPtr<nsIRDFResource>	aProperty = do_QueryInterface(aArc);
		if (!aProperty)	continue;

		// don't copy URL property as it is special
		if (aProperty.get() == kNC_URL)	continue;

		nsCOMPtr<nsISimpleEnumerator>	srcList;
		if (NS_FAILED(rv = GetSources(aProperty, oldURLRes, PR_TRUE,
			getter_AddRefs(srcList))))
			break;
		
		PRBool	hasMoreSrcs = PR_TRUE;
		while(hasMoreSrcs == PR_TRUE)
		{
			if (NS_FAILED(rv = srcList->HasMoreElements(&hasMoreSrcs)))
				break;
			if (hasMoreSrcs == PR_FALSE)	break;

			nsCOMPtr<nsISupports>	aSrc;
			if (NS_FAILED(rv = srcList->GetNext(getter_AddRefs(aSrc))))
				break;
			nsCOMPtr<nsIRDFResource>	aSource = do_QueryInterface(aSrc);
			if (!aSource)	continue;

			if (NS_FAILED(rv = Change(aSource, aProperty, oldURLRes, newURLRes)))
				break;
		}
	}

#ifdef	DEBUG
	if (NS_SUCCEEDED(rv))
	{
		printf("ChangeURL success.\n");
	}
#endif

	return(rv);
}



NS_IMETHODIMP
nsBookmarksService::Change(nsIRDFResource* aSource,
			   nsIRDFResource* aProperty,
			   nsIRDFNode* aOldTarget,
			   nsIRDFNode* aNewTarget)
{
	nsresult	rv = NS_RDF_ASSERTION_REJECTED;

	if (CanAccept(aSource, aProperty, aNewTarget))
	{
		if (aProperty == kNC_URL)
		{
			if (NS_SUCCEEDED(rv = ChangeURL(aSource, aProperty, aOldTarget, aNewTarget)))
			{
				// should we update the last modified date on aNewTarget?
			}
		}
		else if (NS_SUCCEEDED(rv = mInner->Change(aSource, aProperty, aOldTarget, aNewTarget)))
		{
			UpdateBookmarkLastModifiedDate(aSource);
		}
	}
	return(rv);
}



NS_IMETHODIMP
nsBookmarksService::Move(nsIRDFResource* aOldSource,
			 nsIRDFResource* aNewSource,
			 nsIRDFResource* aProperty,
			 nsIRDFNode* aTarget)
{
	nsresult	rv = NS_RDF_ASSERTION_REJECTED;

	if (CanAccept(aNewSource, aProperty, aTarget))
	{
		if (NS_SUCCEEDED(rv = mInner->Move(aOldSource, aNewSource, aProperty, aTarget)))
		{
			UpdateBookmarkLastModifiedDate(aOldSource);
			UpdateBookmarkLastModifiedDate(aNewSource);
		}
	}
	return(rv);
}



NS_IMETHODIMP
nsBookmarksService::GetAllCommands(nsIRDFResource* source,
				   nsIEnumerator/*<nsIRDFResource>*/** commands)
{
	NS_NOTYETIMPLEMENTED("write me!");
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
nsBookmarksService::GetAllCmds(nsIRDFResource* source,
			       nsISimpleEnumerator/*<nsIRDFResource>*/** commands)
{
	nsCOMPtr<nsISupportsArray>	cmdArray;
	nsresult			rv;
	rv = NS_NewISupportsArray(getter_AddRefs(cmdArray));
	if (NS_FAILED(rv))	return(rv);

	// determine type
	PRBool	isBookmark = PR_FALSE, isBookmarkFolder = PR_FALSE, isBookmarkSeparator = PR_FALSE;
	mInner->HasAssertion(source, kRDF_type, kNC_Bookmark, PR_TRUE, &isBookmark);
	mInner->HasAssertion(source, kRDF_type, kNC_Folder, PR_TRUE, &isBookmarkFolder);
	mInner->HasAssertion(source, kRDF_type, kNC_BookmarkSeparator, PR_TRUE, &isBookmarkSeparator);

	if (isBookmark || isBookmarkFolder || isBookmarkSeparator)
	{
		cmdArray->AppendElement(kNC_BookmarkCommand_NewBookmark);
		cmdArray->AppendElement(kNC_BookmarkCommand_NewFolder);
		cmdArray->AppendElement(kNC_BookmarkCommand_NewSeparator);
	}
	if (isBookmark)
	{
		cmdArray->AppendElement(kNC_BookmarkCommand_DeleteBookmark);
	}
	if (isBookmarkFolder)
	{
		cmdArray->AppendElement(kNC_BookmarkCommand_DeleteBookmarkFolder);
	}
	if (isBookmarkSeparator)
	{
		cmdArray->AppendElement(kNC_BookmarkCommand_DeleteBookmarkSeparator);
	}

	nsISimpleEnumerator		*result = new nsArrayEnumerator(cmdArray);
	if (!result)
		return(NS_ERROR_OUT_OF_MEMORY);
	NS_ADDREF(result);
	*commands = result;
	return(NS_OK);
}



NS_IMETHODIMP
nsBookmarksService::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                         nsIRDFResource*   aCommand,
                                         nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                         PRBool* aResult)
{
	return(NS_ERROR_NOT_IMPLEMENTED);
}



nsresult
nsBookmarksService::getArgumentN(nsISupportsArray *arguments, nsIRDFResource *res,
				PRInt32 offset, nsIRDFResource **argValue)
{
	nsresult		rv;
	PRUint32		loop, numArguments;

	*argValue = nsnull;

	if (NS_FAILED(rv = arguments->Count(&numArguments)))	return(rv);

	// format is argument, value, argument, value, ... [you get the idea]
	// multiple arguments can be the same, by the way, thus the "offset"
	for (loop = 0; loop < numArguments; loop += 2)
	{
		nsCOMPtr<nsISupports>	aSource = arguments->ElementAt(loop);
		if (!aSource)	return(NS_ERROR_NULL_POINTER);
		nsCOMPtr<nsIRDFResource>	src = do_QueryInterface(aSource);
		if (!src)	return(NS_ERROR_NO_INTERFACE);
		
		if (src.get() == res)
		{
			if (offset > 0)
			{
				--offset;
				continue;
			}

			nsCOMPtr<nsISupports>	aValue = arguments->ElementAt(loop + 1);
			if (!aSource)	return(NS_ERROR_NULL_POINTER);
			nsCOMPtr<nsIRDFResource>	val = do_QueryInterface(aValue);
			if (!val)	return(NS_ERROR_NO_INTERFACE);

			*argValue = val;
			NS_ADDREF(*argValue);
			return(NS_OK);
		}
	}
	return(NS_ERROR_INVALID_ARG);
}



nsresult
nsBookmarksService::insertBookmarkItem(nsIRDFResource *src, nsISupportsArray *aArguments, PRInt32 parentArgIndex, nsIRDFResource *objType)
{
	nsresult			rv;

	nsCOMPtr<nsIRDFResource>	argParent;
	if (NS_FAILED(rv = getArgumentN(aArguments, kNC_Parent,
			parentArgIndex, getter_AddRefs(argParent))))
		return(rv);

	nsCOMPtr<nsIRDFContainer>	container;
	if (NS_FAILED(rv = nsComponentManager::CreateInstance(kRDFContainerCID, nsnull,
			nsIRDFContainer::GetIID(), getter_AddRefs(container))))
		return(rv);
	if (NS_FAILED(rv = container->Init(mInner, argParent)))
		return(rv);

	PRInt32		srcIndex;
	if (NS_FAILED(rv = container->IndexOf(src, &srcIndex)))
		return(rv);

	nsCOMPtr<nsIRDFResource>	newElement;
	if (NS_FAILED(rv = BookmarkParser::CreateAnonymousResource(&newElement)))
		return(rv);

	// set a default name for bookmarks/folders
	nsAutoString			newName("");

	if (objType == kNC_Bookmark)
	{
		newName = "New Bookmark";		// XXX localization
	}
	else if (objType == kNC_Folder)
	{
		if (NS_FAILED(rv = gRDFC->MakeSeq(mInner, newElement, nsnull)))
			return(rv);

		newName = "New Folder";			// XXX localization
	}

	if (newName.Length() > 0)
	{
		nsCOMPtr<nsIRDFLiteral>	nameLiteral;
		if (NS_FAILED(rv = gRDF->GetLiteral(newName.GetUnicode(), getter_AddRefs(nameLiteral))))
			return(rv);
		if (NS_FAILED(rv = mInner->Assert(newElement, kNC_Name, nameLiteral, PR_TRUE)))
			return(rv);
	}

	if (NS_FAILED(rv = mInner->Assert(newElement, kRDF_type, objType, PR_TRUE)))
		return(rv);

	// convert the current date/time from microseconds (PRTime) to seconds
	nsCOMPtr<nsIRDFDate>	dateLiteral;
	if (NS_FAILED(rv = gRDF->GetDateLiteral(PR_Now(), getter_AddRefs(dateLiteral))))
		return(rv);
	if (NS_FAILED(rv = mInner->Assert(newElement, kNC_BookmarkAddDate, dateLiteral, PR_TRUE)))
		return(rv);

	if (NS_FAILED(rv = container->InsertElementAt(newElement, srcIndex + 1, PR_TRUE)))
		return(rv);

	return(rv);
}



nsresult
nsBookmarksService::deleteBookmarkItem(nsIRDFResource *src, nsISupportsArray *aArguments, PRInt32 parentArgIndex, nsIRDFResource *objType)
{
	nsresult			rv;

	nsCOMPtr<nsIRDFResource>	argParent;
	if (NS_FAILED(rv = getArgumentN(aArguments, kNC_Parent,
			parentArgIndex, getter_AddRefs(argParent))))
		return(rv);

	// make sure its an object of the correct type (bookmark, folder, separator, ...)
	PRBool	isCorrectObjectType = PR_FALSE;
	if (NS_FAILED(rv = mInner->HasAssertion(src, kRDF_type,
			objType, PR_TRUE, &isCorrectObjectType)))
		return(rv);
	if (!isCorrectObjectType)	return(NS_ERROR_UNEXPECTED);

	nsCOMPtr<nsIRDFContainer>	container;
	if (NS_FAILED(rv = nsComponentManager::CreateInstance(kRDFContainerCID, nsnull,
			nsIRDFContainer::GetIID(), getter_AddRefs(container))))
		return(rv);
	if (NS_FAILED(rv = container->Init(mInner, argParent)))
		return(rv);

	if (NS_FAILED(rv = container->RemoveElement(src, PR_TRUE)))
		return(rv);

	return(rv);
}



NS_IMETHODIMP
nsBookmarksService::DoCommand(nsISupportsArray *aSources, nsIRDFResource *aCommand,
				nsISupportsArray *aArguments)
{
	nsresult		rv = NS_OK;
	PRInt32			loop;
	PRUint32		numSources;
	if (NS_FAILED(rv = aSources->Count(&numSources)))	return(rv);
	if (numSources < 1)
	{
		return(NS_ERROR_ILLEGAL_VALUE);
	}

	for (loop=((PRInt32)numSources)-1; loop>=0; loop--)
	{
		nsCOMPtr<nsISupports>	aSource = aSources->ElementAt(loop);
		if (!aSource)	return(NS_ERROR_NULL_POINTER);
		nsCOMPtr<nsIRDFResource>	src = do_QueryInterface(aSource);
		if (!src)	return(NS_ERROR_NO_INTERFACE);

		if (aCommand == kNC_BookmarkCommand_NewBookmark)
		{
			if (NS_FAILED(rv = insertBookmarkItem(src, aArguments,
					loop, kNC_Bookmark)))
				return(rv);
		}
		else if (aCommand == kNC_BookmarkCommand_NewFolder)
		{
			if (NS_FAILED(rv = insertBookmarkItem(src, aArguments,
					loop, kNC_Folder)))
				return(rv);
		}
		else if (aCommand == kNC_BookmarkCommand_NewSeparator)
		{
			if (NS_FAILED(rv = insertBookmarkItem(src, aArguments,
					loop, kNC_BookmarkSeparator)))
				return(rv);
		}
		else if (aCommand == kNC_BookmarkCommand_DeleteBookmark)
		{
			if (NS_FAILED(rv = deleteBookmarkItem(src, aArguments,
					loop, kNC_Bookmark)))
				return(rv);
		}
		else if (aCommand == kNC_BookmarkCommand_DeleteBookmarkFolder)
		{
			if (NS_FAILED(rv = deleteBookmarkItem(src, aArguments,
					loop, kNC_Folder)))
				return(rv);
		}
		else if (aCommand == kNC_BookmarkCommand_DeleteBookmarkSeparator)
		{
			if (NS_FAILED(rv = deleteBookmarkItem(src, aArguments,
					loop, kNC_BookmarkSeparator)))
				return(rv);
		}
	}

	Flush();

	return(NS_OK);
}

////////////////////////////////////////////////////////////////////////
// nsIRDFRemoteDataSource



NS_IMETHODIMP
nsBookmarksService::Init(const char* aURI)
{
	return NS_OK;
}



NS_IMETHODIMP
nsBookmarksService::Refresh(PRBool aBlocking)
{
	// XXX re-sync with the bookmarks file, if necessary.
	return NS_OK;
}



NS_IMETHODIMP
nsBookmarksService::Flush()
{
	return WriteBookmarks(mInner, kNC_BookmarksRoot);
}



////////////////////////////////////////////////////////////////////////
// Implementation methods

nsresult
nsBookmarksService::GetBookmarksFile(nsFileSpec* aResult)
{
	nsresult rv;

	// Look for bookmarks.html in the current profile
	// directory. This is as convoluted as it seems because we
	// want to 1) not break viewer (which has no profiles), and 2)
	// still deal reasonably (in the short term) when no
	// bookmarks.html is installed in the profile directory.
	do {
		NS_WITH_SERVICE(nsIProfile, profile, kProfileCID, &rv);
		if (NS_FAILED(rv)) break;

		rv = profile->GetCurrentProfileDir(aResult);
		if (NS_FAILED(rv)) break;

		*aResult += "bookmarks.html";

		// Note: first try "bookmarks.html" and, if that doesn't exist,
		//       fallback to trying "bookmark.htm". Do this due to older
		//       versions of the browser where the name is different per
		//       platform.
		if (! aResult->Exists())
		{
			// XXX should we   NS_RELEASE(*aResult)   ???
		
			rv = profile->GetCurrentProfileDir(aResult);
			if (NS_FAILED(rv)) break;
			*aResult += "bookmark.htm";

			if (! aResult->Exists())
			{
				rv = NS_ERROR_FAILURE;
			}
		}
	} while (0);

#ifdef DEBUG
	if (NS_FAILED(rv)) {
		*aResult = nsSpecialSystemDirectory(nsSpecialSystemDirectory::OS_CurrentProcessDirectory);
		*aResult += "chrome";
		*aResult += "bookmarks";
		*aResult += "content";
		*aResult += "default";
		*aResult += "bookmarks.html";
		rv = NS_OK;
	}
#endif

	return rv;
}

nsresult
nsBookmarksService::ReadBookmarks()
{
	nsresult rv;

	nsFileSpec bookmarksFile;
	rv = GetBookmarksFile(&bookmarksFile);

	// Oh well, couldn't get the bookmarks file. Guess there
	// aren't any bookmarks to read in.
	if (NS_FAILED(rv))
	    return NS_OK;

	rv = gRDFC->MakeSeq(mInner, kNC_BookmarksRoot, nsnull);
	NS_ASSERTION(NS_SUCCEEDED(rv), "Unable to make NC:BookmarksRoot a sequence");
	if (NS_FAILED(rv)) return rv;

	PRBool	foundIERoot = PR_FALSE;

#ifdef	XP_WIN
	nsCOMPtr<nsIRDFResource>	ieFolder;
	char				*ieFavoritesURL = nsnull;
#endif
#ifdef	XP_BEOS
	nsCOMPtr<nsIRDFResource>	netPositiveFolder;
	char				*netPositiveURL = nsnull;
#endif

	{ // <-- scope the stream to get the open/close automatically.
		nsInputFileStream strm(bookmarksFile);

		if (! strm.is_open())
		{
			NS_ERROR("unable to open file");
			return NS_ERROR_FAILURE;
		}

		BookmarkParser parser;
		parser.Init(&strm, mInner);

#ifdef	XP_MAC
		parser.SetIEFavoritesRoot(kURINC_IEFavoritesRoot);
#endif

#ifdef	XP_WIN
		nsSpecialSystemDirectory	ieFavoritesFile(nsSpecialSystemDirectory::Win_Favorites);
		nsFileURL			ieFavoritesURLSpec(ieFavoritesFile);
		const char			*favoritesURL = ieFavoritesURLSpec.GetAsString();
		if (favoritesURL)
		{
			ieFavoritesURL = strdup(favoritesURL);
		}
		parser.SetIEFavoritesRoot(ieFavoritesURL);
#endif

#ifdef	XP_BEOS
		nsSpecialSystemDirectory	netPositiveFile(nsSpecialSystemDirectory::BeOS_SettingsDirectory);
		nsFileURL			netPositiveURLSpec(netPositiveFile);

		// XXX Currently hard-coded; does the BeOS have anything like a
		// system registry which we could use to get this instead?
		netPositiveURLSpec += "NetPositive/Bookmarks/";

		const char			*constNetPositiveURL = netPositiveURLSpec.GetAsString();
		if (constNetPositiveURL)
		{
			netPositiveURL = strdup(constNetPositiveURL);
		}
		parser.SetIEFavoritesRoot(netPositiveURL);
#endif

		parser.Parse(kNC_BookmarksRoot, kNC_Bookmark);

		parser.ParserFoundIEFavoritesRoot(&foundIERoot);
	} // <-- scope the stream to get the open/close automatically.
	
	// look for and import any IE Favorites
	nsAutoString	ieTitle("Imported IE Favorites");			// XXX localization?
#ifdef	XP_BEOS
	nsAutoString	netPositiveTitle("Imported NetPositive Bookmarks");	// XXX localization?
#endif

#ifdef	XP_MAC
	nsSpecialSystemDirectory ieFavoritesFile(nsSpecialSystemDirectory::Mac_PreferencesDirectory);
	ieFavoritesFile += "Explorer";
	ieFavoritesFile += "Favorites.html";

	nsInputFileStream	ieStream(ieFavoritesFile);
	if (ieStream.is_open())
	{
		if (NS_SUCCEEDED(rv = gRDFC->MakeSeq(mInner, kNC_IEFavoritesRoot, nsnull)))
		{
			BookmarkParser parser;
			parser.Init(&ieStream, mInner);
			parser.Parse(kNC_IEFavoritesRoot, kNC_IEFavorite);
				
			nsCOMPtr<nsIRDFLiteral>	ieTitleLiteral;
			rv = gRDF->GetLiteral(ieTitle.GetUnicode(), getter_AddRefs(ieTitleLiteral));
			if (NS_SUCCEEDED(rv) && ieTitleLiteral)
			{
				rv = mInner->Assert(kNC_IEFavoritesRoot, kNC_Name, ieTitleLiteral, PR_TRUE);
			}
				
			// if the IE Favorites root isn't somewhere in bookmarks.html, add it
			if (!foundIERoot)
			{
				nsCOMPtr<nsIRDFContainer> bookmarksRoot;
				rv = nsComponentManager::CreateInstance(kRDFContainerCID,
									nsnull,
									nsIRDFContainer::GetIID(),
									getter_AddRefs(bookmarksRoot));
				if (NS_FAILED(rv)) return rv;

				rv = bookmarksRoot->Init(mInner, kNC_BookmarksRoot);
				if (NS_FAILED(rv)) return rv;

				rv = bookmarksRoot->AppendElement(kNC_IEFavoritesRoot);
				if (NS_FAILED(rv)) return rv;
			}
		}
	}
#endif

#ifdef	XP_WIN
	rv = gRDF->GetResource(ieFavoritesURL, getter_AddRefs(ieFolder));
	if (NS_SUCCEEDED(rv))
	{
		nsCOMPtr<nsIRDFLiteral>	ieTitleLiteral;
		rv = gRDF->GetLiteral(ieTitle.GetUnicode(), getter_AddRefs(ieTitleLiteral));
		if (NS_SUCCEEDED(rv) && ieTitleLiteral)
		{
			rv = mInner->Assert(ieFolder, kNC_Name, ieTitleLiteral, PR_TRUE);
		}

		// if the IE Favorites root isn't somewhere in bookmarks.html, add it
		if (!foundIERoot)
		{
			nsCOMPtr<nsIRDFContainer> container;
			rv = nsComponentManager::CreateInstance(kRDFContainerCID,
								nsnull,
								nsIRDFContainer::GetIID(),
								getter_AddRefs(container));
			if (NS_FAILED(rv)) return rv;

			rv = container->Init(mInner, kNC_BookmarksRoot);
			if (NS_FAILED(rv)) return rv;

			rv = container->AppendElement(ieFolder);
			if (NS_FAILED(rv)) return rv;
		}
	}
	if (ieFavoritesURL)
	{
		free(ieFavoritesURL);
		ieFavoritesURL = nsnull;
	}
#endif

#ifdef	XP_BEOS
	rv = gRDF->GetResource(netPositiveURL, getter_AddRefs(netPositiveFolder));
	if (NS_SUCCEEDED(rv))
	{
		nsCOMPtr<nsIRDFLiteral>	netPositiveTitleLiteral;
		rv = gRDF->GetLiteral(netPositiveTitle.GetUnicode(), getter_AddRefs(netPositiveTitleLiteral));
		if (NS_SUCCEEDED(rv) && netPositiveTitleLiteral)
		{
			rv = mInner->Assert(netPositiveFolder, kNC_Name, netPositiveTitleLiteral, PR_TRUE);
		}

		// if the Favorites root isn't somewhere in bookmarks.html, add it
		if (!foundIERoot)
		{
			nsCOMPtr<nsIRDFContainer> container;
			rv = nsComponentManager::CreateInstance(kRDFContainerCID,
								nsnull,
								nsIRDFContainer::GetIID(),
								getter_AddRefs(container));
			if (NS_FAILED(rv)) return rv;

			rv = container->Init(mInner, kNC_BookmarksRoot);
			if (NS_FAILED(rv)) return rv;

			rv = container->AppendElement(netPositiveFolder);
			if (NS_FAILED(rv)) return rv;
		}
	}
	if (netPositiveURL)
	{
		free(netPositiveURL);
		netPositiveURL = nsnull;
	}
#endif


	return NS_OK;	
}



nsresult
nsBookmarksService::WriteBookmarks(nsIRDFDataSource *ds, nsIRDFResource *root)
{
	nsresult rv;

	nsFileSpec bookmarksFile;
	rv = GetBookmarksFile(&bookmarksFile);

	// Oh well, couldn't get the bookmarks file. Guess there
	// aren't any bookmarks for us to write out.
	if (NS_FAILED(rv))
	    return NS_OK;

	rv = NS_ERROR_FAILURE;
	nsOutputFileStream	strm(bookmarksFile);
	if (strm.is_open())
	{
		strm << "<!DOCTYPE NETSCAPE-Bookmark-file-1>\n";
		strm << "<!-- This is an automatically generated file.\n";
		strm << "It will be read and overwritten.\n";
		strm << "Do Not Edit! -->\n";
		strm << "<TITLE>Bookmarks</TITLE>\n";
		strm << "<H1>Bookmarks</H1>\n\n";
		
		rv = WriteBookmarksContainer(ds, strm, root, 0);
	}
	return(rv);
}



nsresult
nsBookmarksService::WriteBookmarksContainer(nsIRDFDataSource *ds, nsOutputFileStream strm, nsIRDFResource *parent, PRInt32 level)
{
	nsresult	rv = NS_OK;

	nsAutoString	indentationString("");
	for (PRInt32 loop=0; loop<level; loop++)	indentationString += "    ";
	char		*indentation = indentationString.ToNewCString();
	if (nsnull == indentation)	return(NS_ERROR_OUT_OF_MEMORY);

	nsCOMPtr<nsIRDFContainer> container;
	rv = nsComponentManager::CreateInstance(kRDFContainerCID,
											nsnull,
											nsIRDFContainer::GetIID(),
											getter_AddRefs(container));
	if (NS_FAILED(rv)) return rv;

	rv = container->Init(ds, parent);
	if (NS_SUCCEEDED(rv))
	{
		strm << indentation;
		strm << "<DL><p>\n";

		nsCOMPtr<nsISimpleEnumerator>	children;
		if (NS_SUCCEEDED(rv = container->GetElements(getter_AddRefs(children))))
		{
			PRBool	more = PR_TRUE;
			while (more == PR_TRUE)
			{
				if (NS_FAILED(rv = children->HasMoreElements(&more)))	break;
				if (more != PR_TRUE)	break;

				nsCOMPtr<nsISupports>	iSupports;					
				if (NS_FAILED(rv = children->GetNext(getter_AddRefs(iSupports))))	break;

				nsCOMPtr<nsIRDFResource>	child = do_QueryInterface(iSupports);
				if (!child)	break;

				PRBool	isContainer = PR_FALSE;
				if (child.get() != kNC_IEFavoritesRoot)
				{
					rv = gRDFC->IsContainer(ds, child, &isContainer);
					if (NS_FAILED(rv)) break;
				}

				nsCOMPtr<nsIRDFNode>	nameNode;
				nsAutoString		nameString("");
				char			*name = nsnull;
				rv = ds->GetTarget(child, kNC_Name, PR_TRUE, getter_AddRefs(nameNode));
				if (NS_SUCCEEDED(rv) && nameNode)
				{
					nsCOMPtr<nsIRDFLiteral>	nameLiteral = do_QueryInterface(nameNode);
					if (nameLiteral)
					{
						const PRUnichar	*title = nsnull;
						if (NS_SUCCEEDED(rv = nameLiteral->GetValueConst(&title)))
						{
							nameString = title;
							name = nameString.ToNewCString();
						}
					}
				}

				strm << indentation;
				strm << "    ";
				if (isContainer == PR_TRUE)
				{
					strm << "<DT><H3";
					// output ADD_DATE
					WriteBookmarkProperties(ds, strm, child, kNC_BookmarkAddDate, kAddDateEquals, PR_FALSE);

					// output LAST_MODIFIED
					WriteBookmarkProperties(ds, strm, child, kWEB_LastModifiedDate, kLastModifiedEquals, PR_FALSE);

					// output ID
					strm << " " << kIDEquals;
					const char	*id = nsnull;
					rv = child->GetValueConst(&id);
					if (NS_SUCCEEDED(rv) && (id)) {
						strm << (const char*) id;
					}
					strm << "\"";

					strm << ">";

					// output title
					if (name)	strm << name;
					strm << "</H3>\n";

					// output description (if one exists)
					WriteBookmarkProperties(ds, strm, child, kNC_Description, kOpenDD, PR_TRUE);

					rv = WriteBookmarksContainer(ds, strm, child, level+1);
				}
				else
				{
					const char	*url = nsnull;
					if (NS_SUCCEEDED(rv = child->GetValueConst(&url)) && (url))
					{
						nsAutoString	uri(url);

						PRBool		isBookmarkSeparator = PR_FALSE;
						if (NS_SUCCEEDED(mInner->HasAssertion(child, kRDF_type,
							kNC_BookmarkSeparator, PR_TRUE, &isBookmarkSeparator)) &&
							(isBookmarkSeparator == PR_TRUE) )
						{
							// its a separator
							strm << "<HR>\n";
						}
						else
						{
							strm << "<DT><A HREF=\"";
							// output URL
							strm << url;
							strm << "\"";
								
							// output ADD_DATE
							WriteBookmarkProperties(ds, strm, child, kNC_BookmarkAddDate, kAddDateEquals, PR_FALSE);

							// output LAST_VISIT
							WriteBookmarkProperties(ds, strm, child, kWEB_LastVisitDate, kLastVisitEquals, PR_FALSE);
								
							// output LAST_MODIFIED
							WriteBookmarkProperties(ds, strm, child, kWEB_LastModifiedDate, kLastModifiedEquals, PR_FALSE);
								
							// output SHORTCUTURL
							WriteBookmarkProperties(ds, strm, child, kNC_ShortcutURL, kShortcutURLEquals, PR_FALSE);
								
							strm << ">";
							// output title
							if (name)	strm << name;
							strm << "</A>\n";
							
							// output description (if one exists)
							WriteBookmarkProperties(ds, strm, child, kNC_Description, kOpenDD, PR_TRUE);
						}
					}
				}

				if (nsnull != name)
				{
					delete []name;
					name = nsnull;
				}
					
				if (NS_FAILED(rv))	break;
			}

			strm << indentation;
			strm << "</DL><p>\n";
		}
	}
	delete [] indentation;
	return(rv);
}



/*
	Note: this routine is similiar, yet distinctly different from, nsRDFContentUtils::GetTextForNode
*/

nsresult
nsBookmarksService::GetTextForNode(nsIRDFNode* aNode, nsString& aResult)
{
    nsresult		rv;
    nsIRDFResource	*resource;
    nsIRDFLiteral	*literal;
    nsIRDFDate		*dateLiteral;
    nsIRDFInt		*intLiteral;

    if (! aNode) {
        aResult.Truncate();
        rv = NS_OK;
    }
    else if (NS_SUCCEEDED(rv = aNode->QueryInterface(kIRDFResourceIID, (void**) &resource))) {
    	const char	*p = nsnull;
        if (NS_SUCCEEDED(rv = resource->GetValueConst( &p )) && (p)) {
            aResult = p;
        }
        NS_RELEASE(resource);
    }
    else if (NS_SUCCEEDED(rv = aNode->QueryInterface(kIRDFDateIID, (void**) &dateLiteral))) {
	PRInt64		theDate, million;
        if (NS_SUCCEEDED(rv = dateLiteral->GetValue( &theDate ))) {
		LL_I2L(million, PR_USEC_PER_SEC);
		LL_DIV(theDate, theDate, million);			// convert from microseconds (PRTime) to seconds
		PRInt32		now32;
		LL_L2I(now32, theDate);
		aResult.Truncate();
        	aResult.Append(now32, 10);
        }
        NS_RELEASE(dateLiteral);
    }
    else if (NS_SUCCEEDED(rv = aNode->QueryInterface(kIRDFIntIID, (void**) &intLiteral))) {
	PRInt32		theInt;
	aResult.Truncate();
        if (NS_SUCCEEDED(rv = intLiteral->GetValue( &theInt ))) {
        	aResult.Append(theInt, 10);
        }
        NS_RELEASE(intLiteral);
    }
    else if (NS_SUCCEEDED(rv = aNode->QueryInterface(kIRDFLiteralIID, (void**) &literal))) {
	nsXPIDLString	p;
        if (NS_SUCCEEDED(rv = literal->GetValue( getter_Copies(p) ))) {
            aResult = p;
        }
        NS_RELEASE(literal);
    }
    else {
        NS_ERROR("not a resource or a literal");
        rv = NS_ERROR_UNEXPECTED;
    }

    return rv;
}



nsresult
nsBookmarksService::WriteBookmarkProperties(nsIRDFDataSource *ds, nsOutputFileStream strm,
	nsIRDFResource *child, nsIRDFResource *property, const char *htmlAttrib, PRBool isFirst)
{
	nsresult		rv;
	nsCOMPtr<nsIRDFNode>	node;
	if (NS_SUCCEEDED(rv = ds->GetTarget(child, property, PR_TRUE, getter_AddRefs(node))))
	{
		nsAutoString	literalString;
		if (NS_SUCCEEDED(rv = GetTextForNode(node, literalString)))
		{
			char		*attribute = literalString.ToNewCString();
			if (nsnull != attribute)
			{
				if (isFirst == PR_FALSE)
				{
					strm << " ";
				}
				if (property == kNC_Description)
				{
					if (literalString.Length() > 0)
					{
						char *escapedAttrib = nsEscapeHTML(attribute);
						if (escapedAttrib)
						{
							strm << htmlAttrib;
							strm << escapedAttrib;
							strm << "\n";

							delete []escapedAttrib;
							escapedAttrib = nsnull;
						}
					}
				}
				else
				{
					strm << htmlAttrib;
					strm << attribute;
					strm << "\"";
				}
				delete [] attribute;
				attribute = nsnull;
			}
		}
	}
	return(rv);
}



PRBool
nsBookmarksService::CanAccept(nsIRDFResource* aSource,
			      nsIRDFResource* aProperty,
			      nsIRDFNode* aTarget)
{
	// XXX This is really crippled, and needs to be stricter. We want
	// to exclude any property that isn't talking about a known
	// bookmark.
	nsresult rv;

	PRBool isOrdinal;
	rv = gRDFC->IsOrdinalProperty(aProperty, &isOrdinal);
	if (NS_FAILED(rv))
		return PR_FALSE;

	if (isOrdinal) {
		return PR_TRUE;
	}
	else if ((aProperty == kNC_Description) ||
		 (aProperty == kNC_Name) ||
		 (aProperty == kNC_ShortcutURL) ||
		 (aProperty == kNC_URL) ||
		 (aProperty == kWEB_LastModifiedDate) ||
		 (aProperty == kWEB_LastVisitDate) ||
		 (aProperty == kNC_BookmarkAddDate)) {
		return PR_TRUE;
	}
	else {
		return PR_FALSE;
	}
}



////////////////////////////////////////////////////////////////////////
// Component Exports



extern "C" PR_IMPLEMENT(nsresult)
NSGetFactory(nsISupports* aServiceMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
{
	NS_PRECONDITION(aFactory != nsnull, "null ptr");
	if (! aFactory)
		return NS_ERROR_NULL_POINTER;

	nsIGenericFactory::ConstructorProcPtr constructor;

	if (aClass.Equals(kBookmarksServiceCID)) {
		constructor = NS_NewBookmarksService;
	}
	else {
		*aFactory = nsnull;
		return NS_NOINTERFACE; // XXX
	}

	nsresult rv;
	NS_WITH_SERVICE1(nsIComponentManager, compMgr, aServiceMgr, kComponentManagerCID, &rv);
	if (NS_FAILED(rv)) return rv;

	nsCOMPtr<nsIGenericFactory> factory;
	rv = compMgr->CreateInstance(kGenericFactoryCID,
				     nsnull,
				     nsIGenericFactory::GetIID(),
				     getter_AddRefs(factory));

	if (NS_FAILED(rv)) return rv;

	rv = factory->SetConstructor(constructor);
	if (NS_FAILED(rv)) return rv;

	*aFactory = factory;
	NS_ADDREF(*aFactory);
	return NS_OK;
}



extern "C" PR_IMPLEMENT(nsresult)
NSRegisterSelf(nsISupports* aServMgr , const char* aPath)
{
	nsresult rv;

	nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
	if (NS_FAILED(rv)) return rv;

	NS_WITH_SERVICE1(nsIComponentManager, compMgr, servMgr, kComponentManagerCID, &rv);
	if (NS_FAILED(rv)) return rv;

	rv = compMgr->RegisterComponent(kBookmarksServiceCID, "Bookmarks",
					NS_BOOKMARKS_SERVICE_PROGID,
					aPath, PR_TRUE, PR_TRUE);

	rv = compMgr->RegisterComponent(kBookmarksServiceCID, "Bookmarks",
					NS_BOOKMARKS_DATASOURCE_PROGID,
					aPath, PR_TRUE, PR_TRUE);

	return NS_OK;
}



extern "C" PR_IMPLEMENT(nsresult)
NSUnregisterSelf(nsISupports* aServMgr, const char* aPath)
{
	nsresult rv;

	nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
	if (NS_FAILED(rv)) return rv;

	NS_WITH_SERVICE1(nsIComponentManager, compMgr, servMgr, kComponentManagerCID, &rv);
	if (NS_FAILED(rv)) return rv;

	rv = compMgr->UnregisterComponent(kBookmarksServiceCID, aPath);

	return NS_OK;
}
