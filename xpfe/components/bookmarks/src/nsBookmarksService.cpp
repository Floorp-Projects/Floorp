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
#include "prtime.h"
#include "nsEnumeratorUtils.h"
#include "nsEscape.h"
#include "nsITimer.h"
#include "nsIAtom.h"

//#include "nsISound.h"
//#include "nsICommonDialogs.h"
#include "nsINetSupportDialogService.h"
#include "nsIPrompt.h"
#include "nsAppShellCIDs.h"
#include "nsIAppShellService.h"
#include "nsIWebShellWindow.h"
#include "nsIWebShell.h"
#include "nsIBrowserWindow.h"
#include "nsWidgetsCID.h"
#include "nsIAppShell.h"

#include "nsIURL.h"
#include "nsNeckoUtil.h"
#include "nsIIOService.h"
#include "nsIChannel.h"
#include "nsIHTTPChannel.h"
#include "nsHTTPEnums.h"
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

#include "nsIBuffer.h"
#include "nsIInputStream.h"
#include "nsIBufferInputStream.h"
#include "nsIStreamListener.h"
#include "nsIHTTPHeader.h"

#define	BOOKMARK_TIMEOUT		15000		// fire every 15 seconds
// #define	DEBUG_BOOKMARK_PING_OUTPUT	1

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

//static NS_DEFINE_CID(kNSCOMMONDIALOGSCID,       NS_CommonDialog_CID);
//static NS_DEFINE_IID(kNSCOMMONDIALOGSIID,       NS_ICOMMONDIALOGS_IID);
static NS_DEFINE_CID(kNetSupportDialogCID,      NS_NETSUPPORTDIALOG_CID);
static NS_DEFINE_CID(kAppShellServiceCID,       NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_IID(kAppShellCID,              NS_APPSHELL_CID);
static NS_DEFINE_IID(kIAppShellIID,             NS_IAPPSHELL_IID);


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
nsIRDFResource		*kWEB_Schedule;
nsIRDFResource		*kWEB_Status;
nsIRDFResource		*kWEB_LastPingDate;
nsIRDFResource		*kWEB_LastPingETag;
nsIRDFResource		*kWEB_LastPingModDate;
nsIRDFResource		*kWEB_LastPingContentLen;

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

		gRDF->GetResource(kURINC_BookmarksRoot,                   &kNC_BookmarksRoot);
		gRDF->GetResource(kURINC_IEFavoritesRoot,                 &kNC_IEFavoritesRoot);
		gRDF->GetResource(kURINC_PersonalToolbarFolder,           &kNC_PersonalToolbarFolder);

		gRDF->GetResource(NC_NAMESPACE_URI "Bookmark",            &kNC_Bookmark);
		gRDF->GetResource(NC_NAMESPACE_URI "BookmarkSeparator",   &kNC_BookmarkSeparator);
		gRDF->GetResource(NC_NAMESPACE_URI "BookmarkAddDate",     &kNC_BookmarkAddDate);
		gRDF->GetResource(NC_NAMESPACE_URI "Description",         &kNC_Description);
		gRDF->GetResource(NC_NAMESPACE_URI "Folder",              &kNC_Folder);
		gRDF->GetResource(NC_NAMESPACE_URI "IEFavorite",          &kNC_IEFavorite);
		gRDF->GetResource(NC_NAMESPACE_URI "Name",                &kNC_Name);
		gRDF->GetResource(NC_NAMESPACE_URI "ShortcutURL",         &kNC_ShortcutURL);
		gRDF->GetResource(NC_NAMESPACE_URI "URL",                 &kNC_URL);
		gRDF->GetResource(RDF_NAMESPACE_URI "type",               &kRDF_type);
		gRDF->GetResource(WEB_NAMESPACE_URI "LastModifiedDate",   &kWEB_LastModifiedDate);
		gRDF->GetResource(WEB_NAMESPACE_URI "LastVisitDate",      &kWEB_LastVisitDate);

		gRDF->GetResource(WEB_NAMESPACE_URI "Schedule",           &kWEB_Schedule);
		gRDF->GetResource(WEB_NAMESPACE_URI "status",             &kWEB_Status);
		gRDF->GetResource(WEB_NAMESPACE_URI "LastPingDate",       &kWEB_LastPingDate);
		gRDF->GetResource(WEB_NAMESPACE_URI "LastPingETag",       &kWEB_LastPingETag);
		gRDF->GetResource(WEB_NAMESPACE_URI "LastPingModDate",    &kWEB_LastPingModDate);
		gRDF->GetResource(WEB_NAMESPACE_URI "LastPingContentLen", &kWEB_LastPingContentLen);

		gRDF->GetResource(NC_NAMESPACE_URI "parent",              &kNC_Parent);

		gRDF->GetResource(NC_NAMESPACE_URI "bookmarkcommand?newbookmark",             &kNC_BookmarkCommand_NewBookmark);
		gRDF->GetResource(NC_NAMESPACE_URI "bookmarkcommand?newfolder",               &kNC_BookmarkCommand_NewFolder);
		gRDF->GetResource(NC_NAMESPACE_URI "bookmarkcommand?newseparator",            &kNC_BookmarkCommand_NewSeparator);
		gRDF->GetResource(NC_NAMESPACE_URI "bookmarkcommand?deletebookmark",          &kNC_BookmarkCommand_DeleteBookmark);
		gRDF->GetResource(NC_NAMESPACE_URI "bookmarkcommand?deletebookmarkfolder",    &kNC_BookmarkCommand_DeleteBookmarkFolder);
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
		NS_IF_RELEASE(kWEB_Schedule);
		NS_IF_RELEASE(kWEB_Status);
		NS_IF_RELEASE(kWEB_LastPingDate);
		NS_IF_RELEASE(kWEB_LastPingETag);
		NS_IF_RELEASE(kWEB_LastPingModDate);
		NS_IF_RELEASE(kWEB_LastPingContentLen);
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

static const char kTargetEquals[]          = "TARGET=\"";
static const char kAddDateEquals[]         = "ADD_DATE=\"";
static const char kLastVisitEquals[]       = "LAST_VISIT=\"";
static const char kLastModifiedEquals[]    = "LAST_MODIFIED=\"";
static const char kShortcutURLEquals[]     = "SHORTCUTURL=\"";
static const char kScheduleEquals[]        = "SCHEDULE=\"";
static const char kLastPingEquals[]        = "LAST_PING=\"";
static const char kPingETagEquals[]        = "PING_ETAG=\"";
static const char kPingLastModEquals[]     = "PING_LAST_MODIFIED=\"";
static const char kPingContentLenEquals[]  = "PING_CONTENT_LEN=\"";
static const char kPingStatusEquals[]      = "PING_STATUS=\"";
static const char kIDEquals[]              = "ID=\"";



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
	PRInt32 lastModifiedDate = 0;
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

	// 8. Parse the schedule
	nsAutoString	schedule;
	ParseAttribute(aLine, kScheduleEquals, sizeof(kScheduleEquals) -1, schedule);

	// 9. Parse the last ping date
	PRInt32 lastPingDate = 0;
	{
		nsAutoString s;
		ParseAttribute(aLine, kLastPingEquals, sizeof(kLastPingEquals) - 1, s);
		if (s.Length() > 0) {
			PRInt32 err;
			lastPingDate = s.ToInteger(&err); // ignored.
		}
	}

	// 10. Parse the ping ETag
	nsAutoString	pingETag;
	ParseAttribute(aLine, kPingETagEquals, sizeof(kPingETagEquals) -1, pingETag);

	// 11. Parse the ping LastMod date
	nsAutoString	pingLastMod;
	ParseAttribute(aLine, kPingLastModEquals, sizeof(kPingLastModEquals) -1, pingLastMod);

	// 12. Parse the Ping Content Length
	nsAutoString	pingContentLength;
	ParseAttribute(aLine, kPingContentLenEquals, sizeof(kPingContentLenEquals) -1, pingContentLength);

	// 13. Parse the Ping Status
	nsAutoString	pingStatus;
	ParseAttribute(aLine, kPingStatusEquals, sizeof(kPingStatusEquals) -1, pingStatus);

	// Dunno. 4.5 did it, so will we.
	if (!lastModifiedDate)
		lastModifiedDate = lastVisitDate;

	// There was some other cruft here to deal with aliases, but we ignore them thanks for RDF

	nsresult rv = NS_ERROR_OUT_OF_MEMORY; // in case ToNewCString() fails

	char *cURL = url.ToNewCString();
	if (cURL)
	{
		char *cShortcutURL = shortcut.ToNewCString();	// Note: can be null

		rv = AddBookmark(aContainer, cURL, name.GetUnicode(), addDate, lastVisitDate,
				lastModifiedDate, cShortcutURL, nodeType, bookmarkNode);

		if (NS_SUCCEEDED(rv))
		{
			// save schedule
			if (schedule.Length() > 0)
			{
				nsCOMPtr<nsIRDFLiteral>	scheduleLiteral;
				if (NS_SUCCEEDED(rv = gRDF->GetLiteral(schedule.GetUnicode(),
								    getter_AddRefs(scheduleLiteral))))
				{
					rv = mDataSource->Assert(*bookmarkNode, kWEB_Schedule, scheduleLiteral, PR_TRUE);
					if (rv != NS_RDF_ASSERTION_ACCEPTED)
					{
						NS_ERROR("unable to set bookmark schedule");
					}
				}
				else
				{
					NS_ERROR("unable to get literal for bookmark schedule");
				}
			}

			// last ping date
			AssertTime(*bookmarkNode, kWEB_LastPingDate, lastPingDate);
				
			// save ping ETag
			if (pingETag.Length() > 0)
			{
				nsCOMPtr<nsIRDFLiteral>	pingLiteral;
				if (NS_SUCCEEDED(rv = gRDF->GetLiteral(pingETag.GetUnicode(),
								    getter_AddRefs(pingLiteral))))
				{
					rv = mDataSource->Assert(*bookmarkNode, kWEB_LastPingETag, pingLiteral, PR_TRUE);
					if (rv != NS_RDF_ASSERTION_ACCEPTED)
					{
						NS_ERROR("unable to set ping etag");
					}
				}
				else
				{
					NS_ERROR("unable to get literal for ping etag");
				}
			}

			// save ping Last Mod date
			if (pingLastMod.Length() > 0)
			{
				nsCOMPtr<nsIRDFLiteral>	pingLastModLiteral;
				if (NS_SUCCEEDED(rv = gRDF->GetLiteral(pingLastMod.GetUnicode(),
								    getter_AddRefs(pingLastModLiteral))))
				{
					rv = mDataSource->Assert(*bookmarkNode, kWEB_LastPingModDate, pingLastModLiteral, PR_TRUE);
					if (rv != NS_RDF_ASSERTION_ACCEPTED)
					{
						NS_ERROR("unable to set ping last mod");
					}
				}
				else
				{
					NS_ERROR("unable to get literal for ping last mod");
				}
			}

			// save ping Content Length date
			if (pingContentLength.Length() > 0)
			{
				nsCOMPtr<nsIRDFLiteral>	pingContentLengthLiteral;
				if (NS_SUCCEEDED(rv = gRDF->GetLiteral(pingContentLength.GetUnicode(),
								    getter_AddRefs(pingContentLengthLiteral))))
				{
					rv = mDataSource->Assert(*bookmarkNode, kWEB_LastPingContentLen, pingContentLengthLiteral, PR_TRUE);
					if (rv != NS_RDF_ASSERTION_ACCEPTED)
					{
						NS_ERROR("unable to set ping content length");
					}
				}
				else
				{
					NS_ERROR("unable to get literal for ping content length");
				}
			}

			// save ping status
			if (pingStatus.Length() > 0)
			{
				nsCOMPtr<nsIRDFLiteral>	pingStatusLiteral;
				if (NS_SUCCEEDED(rv = gRDF->GetLiteral(pingStatus.GetUnicode(),
								    getter_AddRefs(pingStatusLiteral))))
				{
					rv = mDataSource->Assert(*bookmarkNode, kWEB_Status, pingStatusLiteral, PR_TRUE);
					if (rv != NS_RDF_ASSERTION_ACCEPTED)
					{
						NS_ERROR("unable to set ping status");
					}
				}
				else
				{
					NS_ERROR("unable to get literal for ping status");
				}
			}
		}
		if (cShortcutURL)
		{
			nsCRT::free(cShortcutURL);
		}
		nsCRT::free(cURL);
	}
	return(rv);
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
		return(rv);
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
		return(rv);
	}

	if ((nsnull != aOptionalTitle) && (*aOptionalTitle != PRUnichar('\0')))
	{
		nsCOMPtr<nsIRDFLiteral> literal;
		if (NS_FAILED(rv = gRDF->GetLiteral(aOptionalTitle, getter_AddRefs(literal))))
		{
			NS_ERROR("unable to create literal for bookmark name");
		}
		if (NS_SUCCEEDED(rv))
		{
			rv = mDataSource->Assert(bookmark, kNC_Name, literal, PR_TRUE);
			if (rv != NS_RDF_ASSERTION_ACCEPTED)
			{
				NS_ERROR("unable to set bookmark name");
			}
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
		}
		if (NS_SUCCEEDED(rv) && (rv != NS_RDF_NO_VALUE))
		{
			rv = mDataSource->Assert(bookmark,
						 kNC_ShortcutURL,
						 shortcutLiteral,
						 PR_TRUE);

			if (rv != NS_RDF_ASSERTION_ACCEPTED)
			{
				NS_ERROR("unable to set bookmark shortcut URL");
			}
		}
	}

	// The last thing we do is add the bookmark to the container. This ensures the minimal amount of reflow.
	rv = aContainer->AppendElement(bookmark);
	NS_ASSERTION(NS_SUCCEEDED(rv), "unable to add bookmark to container");
	return(rv);
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
		nsCOMPtr<nsIRDFNode>	currentNode;
		if (NS_SUCCEEDED(rv = mDataSource->GetTarget(aSource, aLabel, PR_TRUE,
			getter_AddRefs(currentNode))) && (rv != NS_RDF_NO_VALUE))
		{
			rv = mDataSource->Change(aSource, aLabel, currentNode, dateLiteral);
		}
		else
		{
			rv = mDataSource->Assert(aSource, aLabel, dateLiteral, PR_TRUE);
		}
		NS_ASSERTION(rv == NS_RDF_ASSERTION_ACCEPTED, "unable to assert new time");
	}
	return(rv);
}



////////////////////////////////////////////////////////////////////////
// BookmarkDataSourceImpl



class nsBookmarksService : public nsIBookmarksService,
			   public nsIRDFDataSource,
			   public nsIRDFRemoteDataSource,
			   public nsIStreamListener
{
protected:
	nsCOMPtr<nsIRDFDataSource>	mInner;
	nsCOMPtr<nsITimer>		mTimer;
	PRBool				busySchedule;
	nsCOMPtr<nsIRDFResource>	busyResource;
	PRUint32			htmlSize;

static	void	FireTimer(nsITimer* aTimer, void* aClosure);
nsresult	ExamineBookmarkSchedule(nsIRDFResource *theBookmark, PRBool & examineFlag);
nsresult	GetBookmarkToPing(nsIRDFResource **theBookmark);

	nsresult GetBookmarksFile(nsFileSpec* aResult);
	nsresult ReadBookmarks();
	nsresult WriteBookmarks(nsIRDFDataSource *ds, nsIRDFResource *root);
	nsresult WriteBookmarksContainer(nsIRDFDataSource *ds, nsOutputFileStream strm, nsIRDFResource *container, PRInt32 level);
	nsresult GetTextForNode(nsIRDFNode* aNode, nsString& aResult);
	nsresult UpdateBookmarkLastModifiedDate(nsIRDFResource *aSource);
	nsresult WriteBookmarkProperties(nsIRDFDataSource *ds, nsOutputFileStream strm, nsIRDFResource *node,
					 nsIRDFResource *property, const char *htmlAttrib, PRBool isFirst);
	PRBool   CanAccept(nsIRDFResource* aSource, nsIRDFResource* aProperty, nsIRDFNode* aTarget);

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

	// nsIStreamObserver methods:
	NS_DECL_NSISTREAMOBSERVER

	// nsIStreamListener methods:
	NS_DECL_NSISTREAMLISTENER

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
	NS_DECL_NSIRDFREMOTEDATASOURCE
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

	busyResource = null_nsCOMPtr();

	if (!mTimer)
	{
		busySchedule = PR_FALSE;

		rv = NS_NewTimer(getter_AddRefs(mTimer));
		if (NS_FAILED(rv)) return rv;

		// by default, fire the timer once a minute (unit is milliseconds)
		mTimer->Init(nsBookmarksService::FireTimer, this, /* repeat, */ BOOKMARK_TIMEOUT);
		// the timer will hold a reference to the bookmark service, so AddRef
		NS_ADDREF(this);
	}

	return NS_OK;
}



nsresult
nsBookmarksService::ExamineBookmarkSchedule(nsIRDFResource *theBookmark, PRBool & examineFlag)
{
	examineFlag = PR_FALSE;
	
	nsresult	rv = NS_OK;

	nsCOMPtr<nsIRDFNode>	scheduleNode;
	if (NS_FAILED(rv = mInner->GetTarget(theBookmark, kWEB_Schedule, PR_TRUE,
		getter_AddRefs(scheduleNode))) || (rv == NS_RDF_NO_VALUE))
		return(rv);
	
	nsCOMPtr<nsIRDFLiteral>	scheduleLiteral = do_QueryInterface(scheduleNode);
	if (!scheduleLiteral)	return(NS_ERROR_NO_INTERFACE);
	
	const PRUnichar		*scheduleUni = nsnull;
	if (NS_FAILED(rv = scheduleLiteral->GetValueConst(&scheduleUni)))
		return(rv);
	if (!scheduleUni)	return(NS_ERROR_NULL_POINTER);

	nsAutoString		schedule(scheduleUni);
	if (schedule.Length() < 1)	return(NS_ERROR_UNEXPECTED);

	// convert the current date/time from microseconds (PRTime) to seconds
	// Note: don't change now64, as its used later in the function
	PRTime		now64 = PR_Now(), temp64, million;
	LL_I2L(million, PR_USEC_PER_SEC);
	LL_DIV(temp64, now64, million);
	PRInt32		now32;
	LL_L2I(now32, temp64);

	PRExplodedTime	nowInfo;
	PR_ExplodeTime(now64, PR_LocalTimeParameters, &nowInfo);
	
	// XXX Do we need to do this?
	PR_NormalizeTime(&nowInfo, PR_LocalTimeParameters);

	nsAutoString	dayNum;
	dayNum.Append(nowInfo.tm_wday, 10);

	// a schedule string has the following format:
	// Check Monday, Tuesday, and Friday | 9 AM thru 5 PM | every five minutes | change bookmark icon
	// 125|9-17|5|icon

	nsAutoString	notificationMethod;
	PRInt32		startHour = -1, endHour = -1, duration = -1;

	// should we be checking today?
	PRInt32		slashOffset;
	if ((slashOffset = schedule.FindChar(PRUnichar('|'))) >= 0)
	{
		nsAutoString	daySection;
		schedule.Left(daySection, slashOffset);
		schedule.Cut(0, slashOffset+1);
		if (daySection.Find(dayNum) >= 0)
		{
			// ok, we should be checking today.  Within hour range?
			if ((slashOffset = schedule.FindChar(PRUnichar('|'))) >= 0)
			{
				nsAutoString	hourRange;
				schedule.Left(hourRange, slashOffset);
				schedule.Cut(0, slashOffset+1);

				// now have the "hour-range" segment of the string
				// such as "9-17" or "9-12" from the examples above
				PRInt32		dashOffset;
				if ((dashOffset = hourRange.FindChar(PRUnichar('-'))) >= 1)
				{
					nsAutoString	startStr, endStr;

					hourRange.Right(endStr, hourRange.Length() - dashOffset - 1);
					hourRange.Left(startStr, dashOffset);

					PRInt32		errorCode2 = 0;
					startHour = startStr.ToInteger(&errorCode2);
					if (errorCode2)	startHour = -1;
					endHour = endStr.ToInteger(&errorCode2);
					if (errorCode2)	endHour = -1;
					
					if ((startHour >=0) && (endHour >=0))
					{
						if ((slashOffset = schedule.FindChar(PRUnichar('|'))) >= 0)
						{
							nsAutoString	durationStr;
							schedule.Left(durationStr, slashOffset);
							schedule.Cut(0, slashOffset+1);

							// get duration
							PRInt32		errorCode = 0;
							duration = durationStr.ToInteger(&errorCode);
							if (errorCode)	duration = -1;
							
							// what's left is the notification options
							notificationMethod = schedule;
						}
					}
				}
			}
		}
	}
		

#ifdef	DEBUG_BOOKMARK_PING_OUTPUT
	char *methodStr = notificationMethod.ToNewCString();
	if (methodStr)
	{
		printf("Start Hour: %d    End Hour: %d    Duration: %d mins    Method: '%s'\n",
			startHour, endHour, duration, methodStr);
		delete [] methodStr;
		methodStr = nsnull;
	}
#endif

	if ((startHour <= nowInfo.tm_hour) && (endHour >= nowInfo.tm_hour) &&
		(duration >= 1) && (notificationMethod.Length() > 0))
	{
		// OK, we're with the start/end time range, check the duration
		// against the last time we've "pinged" the server (if ever)

		examineFlag = PR_TRUE;

		nsCOMPtr<nsIRDFNode>	pingNode;
		if (NS_SUCCEEDED(rv = mInner->GetTarget(theBookmark, kWEB_LastPingDate,
			PR_TRUE, getter_AddRefs(pingNode))) && (rv != NS_RDF_NO_VALUE))
		{
			nsCOMPtr<nsIRDFDate>	pingLiteral = do_QueryInterface(pingNode);
			if (pingLiteral)
			{
				PRInt64		lastPing;
				if (NS_SUCCEEDED(rv = pingLiteral->GetValue(&lastPing)))
				{
					PRInt64		diff64, sixty;
					LL_SUB(diff64, now64, lastPing);
					
					// convert from milliseconds to seconds
					LL_DIV(diff64, diff64, million);
					// convert from seconds to minutes
					LL_I2L(sixty, 60L);
					LL_DIV(diff64, diff64, sixty);

					PRInt32		diff32;
					LL_L2I(diff32, diff64);
					if (diff32 < duration)
					{
						examineFlag = PR_FALSE;

#ifdef	DEBUG_BOOKMARK_PING_OUTPUT
						printf("Skipping URL, its too soon.\n");
#endif
					}
				}
			}
		}
	}
	return(rv);
}



nsresult
nsBookmarksService::GetBookmarkToPing(nsIRDFResource **theBookmark)
{
	nsresult	rv = NS_OK;

	*theBookmark = nsnull;

	nsCOMPtr<nsISimpleEnumerator>	srcList;
	if (NS_FAILED(rv = GetSources(kRDF_type, kNC_Bookmark, PR_TRUE, getter_AddRefs(srcList))))
		return(rv);

	nsCOMPtr<nsISupportsArray>	bookmarkList;
	if (NS_FAILED(rv = NS_NewISupportsArray(getter_AddRefs(bookmarkList))))
		return(rv);

	// build up a list of potential bookmarks to check
	PRBool	hasMoreSrcs = PR_TRUE;
	while(NS_SUCCEEDED(rv = srcList->HasMoreElements(&hasMoreSrcs))
		&& (hasMoreSrcs == PR_TRUE))
	{
		nsCOMPtr<nsISupports>	aSrc;
		if (NS_FAILED(rv = srcList->GetNext(getter_AddRefs(aSrc))))
			break;
		nsCOMPtr<nsIRDFResource>	aSource = do_QueryInterface(aSrc);
		if (!aSource)	continue;

		// does the bookmark have a schedule, and if so,
		// are we within its bounds for checking the URL?

		PRBool	examineFlag = PR_FALSE;
		if (NS_FAILED(rv = ExamineBookmarkSchedule(aSource, examineFlag))
			|| (examineFlag == PR_FALSE))	continue;

		bookmarkList->AppendElement(aSource);
	}

	// pick a random entry from the list of bookmarks to check
	PRUint32	numBookmarks;
	if (NS_SUCCEEDED(rv = bookmarkList->Count(&numBookmarks)) && (numBookmarks > 0))
	{
		PRInt32		randomNum;
		LL_L2I(randomNum, PR_Now());
		PRUint32	randomBookmark = (numBookmarks-1) % randomNum;

		nsCOMPtr<nsISupports>	iSupports;
		if (NS_SUCCEEDED(rv = bookmarkList->GetElementAt(randomBookmark,
			getter_AddRefs(iSupports))))
		{
			nsCOMPtr<nsIRDFResource>	aBookmark = do_QueryInterface(iSupports);
			if (aBookmark)
			{
				*theBookmark = aBookmark;
				NS_ADDREF(*theBookmark);
			}
		}
	}
	return(rv);
}



void
nsBookmarksService::FireTimer(nsITimer* aTimer, void* aClosure)
{
	// XXX Any lifetime issues we need to deal with here???
	nsBookmarksService *bmks = NS_STATIC_CAST(nsBookmarksService *, aClosure);
	if (!bmks)	return;

	bmks->mTimer = nsnull;

	if (bmks->busySchedule == PR_FALSE)
	{
		nsresult			rv;
		nsCOMPtr<nsIRDFResource>	bookmark;
		if (NS_SUCCEEDED(rv = bmks->GetBookmarkToPing(getter_AddRefs(bookmark))) && (bookmark))
		{
			bmks->busyResource = bookmark;
			const char		*url = nsnull;
			bookmark->GetValueConst(&url);

#ifdef	DEBUG_BOOKMARK_PING_OUTPUT
			printf("nsBookmarksService::FireTimer - Pinging '%s'\n", url);
#endif

			nsresult		rv;
			nsCOMPtr<nsIURI>	uri;
			if (NS_SUCCEEDED(rv = NS_NewURI(getter_AddRefs(uri), url)))
			{
#if 0
				rv = NS_OpenURI(NS_STATIC_CAST(nsIStreamListener *, bmks), nsnull, uri, nsnull);
#else
				nsCOMPtr<nsIChannel>	channel;
				if (NS_SUCCEEDED(rv = NS_OpenURI(getter_AddRefs(channel), uri, nsnull)))
				{
					nsCOMPtr<nsIHTTPChannel>	httpChannel = do_QueryInterface(channel);
					if (httpChannel)
					{
						bmks->busySchedule = PR_TRUE;
						bmks->htmlSize = 0;

//						httpChannel->SetRequestMethod(HM_HEAD);
						httpChannel->SetRequestMethod(HM_GET);
						rv = channel->AsyncRead(0, -1, nsnull, bmks);
					}
				}
#endif
			}
		}
	}
#ifdef	DEBUG_BOOKMARK_PING_OUTPUT
else
	{
	printf("nsBookmarksService::FireTimer - busy pinging.\n");
	}
#endif


	// reschedule the timer unless bookmarks service is going away
	nsrefcnt	refcnt;
	NS_RELEASE2(bmks, refcnt);
	if (0 != refcnt)
	{
		nsresult rv = NS_NewTimer(getter_AddRefs(bmks->mTimer));
		if (NS_FAILED(rv)) return;

		// by default, fire the timer once a minute (unit is milliseconds)
		bmks->mTimer->Init(nsBookmarksService::FireTimer, bmks, /* repeat, */ BOOKMARK_TIMEOUT);
		// the timer will hold a reference to the bookmark service, so AddRef
		NS_ADDREF(bmks);
	}
}



// stream observer methods



NS_IMETHODIMP
nsBookmarksService::OnStartRequest(nsIChannel* channel, nsISupports *ctxt)
{
	return(NS_OK);
}



NS_IMETHODIMP
nsBookmarksService::OnDataAvailable(nsIChannel* channel, nsISupports *ctxt, nsIInputStream *aIStream,
					  PRUint32 sourceOffset, PRUint32 aLength)
{
	// calculate html page size if server doesn't tell us in headers
	htmlSize += aLength;

	return(NS_OK);
}



NS_IMETHODIMP
nsBookmarksService::OnStopRequest(nsIChannel* channel, nsISupports *ctxt,
					nsresult status, const PRUnichar *errorMsg) 
{
	nsresult		rv;

	const char		*uri = nsnull;
	if (NS_SUCCEEDED(rv = busyResource->GetValueConst(&uri)) && (uri))
	{
#ifdef	DEBUG_BOOKMARK_PING_OUTPUT
		printf("Finished polling '%s'\n", uri);
#endif
	}

	nsCOMPtr<nsIHTTPChannel>	httpChannel = do_QueryInterface(channel);
	if (httpChannel)
	{
		nsAutoString			eTagValue, lastModValue, contentLengthValue;
		nsCOMPtr<nsISimpleEnumerator>	enumerator;
		if (NS_SUCCEEDED(rv = httpChannel->GetResponseHeaderEnumerator(getter_AddRefs(enumerator))))
		{
			PRBool			bMoreHeaders;

			while (NS_SUCCEEDED(rv = enumerator->HasMoreElements(&bMoreHeaders))
				&& (bMoreHeaders == PR_TRUE))
			{
				nsCOMPtr<nsISupports>   item;
				enumerator->GetNext(getter_AddRefs(item));
				nsCOMPtr<nsIHTTPHeader>	header = do_QueryInterface(item);
				NS_ASSERTION(header, "nsBookmarksService::OnStopRequest - Bad HTTP header.");
				if (header)
				{
					nsCOMPtr<nsIAtom>       headerAtom;
					header->GetField(getter_AddRefs(headerAtom));
					nsAutoString		headerStr;
					headerAtom->ToString(headerStr);

					char	*val = nsnull;
					
					if (headerStr.EqualsIgnoreCase("eTag"))
					{
						header->GetValue(&val);
						if (val)
						{
							eTagValue = val;
							nsCRT::free(val);
						}
					}
					else if (headerStr.EqualsIgnoreCase("Last-Modified"))
					{
						header->GetValue(&val);
						if (val)
						{
							lastModValue = val;
							nsCRT::free(val);
						}
					}
					else if (headerStr.EqualsIgnoreCase("Content-Length"))
					{
						header->GetValue(&val);
						if (val)
						{
							contentLengthValue = val;
							nsCRT::free(val);
						}
					}
				}
			}
		}

		PRBool		changedFlag = PR_FALSE;

		PRUint32	respStatus;
		if (NS_SUCCEEDED(rv = httpChannel->GetResponseStatus(&respStatus)))
		{
			if ((respStatus >= 200) && (respStatus <= 299))
			{
				if (eTagValue.Length() > 0)
				{
#ifdef	DEBUG_BOOKMARK_PING_OUTPUT
					const char *eTagVal = nsCAutoString(eTagValue);
					printf("eTag: '%s'\n", eTagVal);
#endif
					nsCOMPtr<nsIRDFNode>	currentETagNode;
					if (NS_SUCCEEDED(rv = mInner->GetTarget(busyResource, kWEB_LastPingETag,
						PR_TRUE, getter_AddRefs(currentETagNode))) && (rv != NS_RDF_NO_VALUE))
					{
						nsCOMPtr<nsIRDFLiteral>	currentETagLit = do_QueryInterface(currentETagNode);
						if (currentETagLit)
						{
							const PRUnichar	*currentETagStr = nsnull;
							currentETagLit->GetValueConst(&currentETagStr);
							if ((currentETagStr) && (!eTagValue.EqualsIgnoreCase(currentETagStr)))
							{
								changedFlag = PR_TRUE;
							}
							nsCOMPtr<nsIRDFLiteral>	newETagLiteral;
							if (NS_SUCCEEDED(rv = gRDF->GetLiteral(eTagValue.GetUnicode(),
								getter_AddRefs(newETagLiteral))))
							{
								rv = mInner->Change(busyResource, kWEB_LastPingETag,
									currentETagNode, newETagLiteral);
							}
						}
					}
					else
					{
						nsCOMPtr<nsIRDFLiteral>	newETagLiteral;
						if (NS_SUCCEEDED(rv = gRDF->GetLiteral(eTagValue.GetUnicode(),
							getter_AddRefs(newETagLiteral))))
						{
							rv = mInner->Assert(busyResource, kWEB_LastPingETag,
								newETagLiteral, PR_TRUE);
						}
					}
				}
			}
		}

		if ((changedFlag == PR_FALSE) && (lastModValue.Length() > 0))
		{
#ifdef	DEBUG_BOOKMARK_PING_OUTPUT
			const char *lastModVal = nsCAutoString(lastModValue);
			printf("Last-Modified: '%s'\n", lastModVal);
#endif
			nsCOMPtr<nsIRDFNode>	currentLastModNode;
			if (NS_SUCCEEDED(rv = mInner->GetTarget(busyResource, kWEB_LastPingModDate,
				PR_TRUE, getter_AddRefs(currentLastModNode))) && (rv != NS_RDF_NO_VALUE))
			{
				nsCOMPtr<nsIRDFLiteral>	currentLastModLit = do_QueryInterface(currentLastModNode);
				if (currentLastModLit)
				{
					const PRUnichar	*currentLastModStr = nsnull;
					currentLastModLit->GetValueConst(&currentLastModStr);
					if ((currentLastModStr) && (!lastModValue.EqualsIgnoreCase(currentLastModStr)))
					{
						changedFlag = PR_TRUE;
					}
					nsCOMPtr<nsIRDFLiteral>	newLastModLiteral;
					if (NS_SUCCEEDED(rv = gRDF->GetLiteral(lastModValue.GetUnicode(),
						getter_AddRefs(newLastModLiteral))))
					{
						rv = mInner->Change(busyResource, kWEB_LastPingModDate,
							currentLastModNode, newLastModLiteral);
					}
				}
			}
			else
			{
				nsCOMPtr<nsIRDFLiteral>	newLastModLiteral;
				if (NS_SUCCEEDED(rv = gRDF->GetLiteral(lastModValue.GetUnicode(),
					getter_AddRefs(newLastModLiteral))))
				{
					rv = mInner->Assert(busyResource, kWEB_LastPingModDate,
						newLastModLiteral, PR_TRUE);
				}
			}
		}

		if ((changedFlag == PR_FALSE) && (contentLengthValue.Length() > 0))
		{
#ifdef	DEBUG_BOOKMARK_PING_OUTPUT
			const char *contentLengthVal = nsCAutoString(contentLengthValue);
			printf("Content-Length: '%s'\n", contentLengthVal);
#endif
			nsCOMPtr<nsIRDFNode>	currentContentLengthNode;
			if (NS_SUCCEEDED(rv = mInner->GetTarget(busyResource, kWEB_LastPingContentLen,
				PR_TRUE, getter_AddRefs(currentContentLengthNode))) && (rv != NS_RDF_NO_VALUE))
			{
				nsCOMPtr<nsIRDFLiteral>	currentContentLengthLit = do_QueryInterface(currentContentLengthNode);
				if (currentContentLengthLit)
				{
					const PRUnichar	*currentContentLengthStr = nsnull;
					currentContentLengthLit->GetValueConst(&currentContentLengthStr);
					if ((currentContentLengthStr) && (!contentLengthValue.EqualsIgnoreCase(currentContentLengthStr)))
					{
						changedFlag = PR_TRUE;
					}
					nsCOMPtr<nsIRDFLiteral>	newContentLengthLiteral;
					if (NS_SUCCEEDED(rv = gRDF->GetLiteral(contentLengthValue.GetUnicode(),
						getter_AddRefs(newContentLengthLiteral))))
					{
						rv = mInner->Change(busyResource, kWEB_LastPingContentLen,
							currentContentLengthNode, newContentLengthLiteral);
					}
				}
			}
			else
			{
				nsCOMPtr<nsIRDFLiteral>	newContentLengthLiteral;
				if (NS_SUCCEEDED(rv = gRDF->GetLiteral(contentLengthValue.GetUnicode(),
					getter_AddRefs(newContentLengthLiteral))))
				{
					rv = mInner->Assert(busyResource, kWEB_LastPingContentLen,
						newContentLengthLiteral, PR_TRUE);
				}
			}
		}

		// update last poll date
		nsCOMPtr<nsIRDFDate>	dateLiteral;
		if (NS_SUCCEEDED(rv = gRDF->GetDateLiteral(PR_Now(), getter_AddRefs(dateLiteral))))
		{
			nsCOMPtr<nsIRDFNode>	lastPingNode;
			if (NS_SUCCEEDED(rv = mInner->GetTarget(busyResource, kWEB_LastPingDate, PR_TRUE,
				getter_AddRefs(lastPingNode))) && (rv != NS_RDF_NO_VALUE))
			{
				rv = mInner->Change(busyResource, kWEB_LastPingDate, lastPingNode, dateLiteral);
			}
			else
			{
				rv = mInner->Assert(busyResource, kWEB_LastPingDate, dateLiteral, PR_TRUE);
			}
			NS_ASSERTION(rv == NS_RDF_ASSERTION_ACCEPTED, "unable to assert new time");

			// XXX For the moment, do an immediate flush.
			if (NS_SUCCEEDED(rv))
			{
				Flush();
			}

		}
		else
		{
			NS_ERROR("unable to get date literal for now");
		}

		// If its changed, set the appropriate info
		if (changedFlag == PR_TRUE)
		{
#ifdef	DEBUG_BOOKMARK_PING_OUTPUT
			printf("URL has changed!\n\n");
#endif

			nsAutoString		schedule;

			nsCOMPtr<nsIRDFNode>	scheduleNode;
			if (NS_SUCCEEDED(rv = mInner->GetTarget(busyResource, kWEB_Schedule, PR_TRUE,
				getter_AddRefs(scheduleNode))) && (rv != NS_RDF_NO_VALUE))
			{
				nsCOMPtr<nsIRDFLiteral>	scheduleLiteral = do_QueryInterface(scheduleNode);
				if (scheduleLiteral)
				{
					const PRUnichar		*scheduleUni = nsnull;
					if (NS_SUCCEEDED(rv = scheduleLiteral->GetValueConst(&scheduleUni))
						&& (scheduleUni))
					{
						schedule = scheduleUni;
					}
				}
			}

			// update icon?
			if (schedule.Find(nsAutoString("icon"), PR_TRUE, 0) >= 0)
			{
				nsCOMPtr<nsIRDFLiteral>	statusLiteral;
				if (NS_SUCCEEDED(rv = gRDF->GetLiteral(nsAutoString("new").GetUnicode(), getter_AddRefs(statusLiteral))))
				{
					nsCOMPtr<nsIRDFNode>	currentStatusNode;
					if (NS_SUCCEEDED(rv = mInner->GetTarget(busyResource, kWEB_Status, PR_TRUE,
						getter_AddRefs(currentStatusNode))) && (rv != NS_RDF_NO_VALUE))
					{
						rv = mInner->Change(busyResource, kWEB_Status, currentStatusNode, statusLiteral);
					}
					else
					{
						rv = mInner->Assert(busyResource, kWEB_Status, statusLiteral, PR_TRUE);
					}
					NS_ASSERTION(rv == NS_RDF_ASSERTION_ACCEPTED, "unable to assert changed status");
				}
			}
			
			// play a sound?
			if (schedule.Find(nsAutoString("sound"), PR_TRUE, 0) >= 0)
			{
/*
				nsCOMPtr<nsISound>	soundInterface;
				rv = nsComponentManager::CreateInstance(kSoundCID,
						nsnull, nsISound::GetIID(),
						getter_AddRefs(soundInterface));
				if (NS_SUCCEEDED(rv))
				{
					// XXX for the moment, just beep
					soundInterface->Beep();
				}
*/
			}
			
			PRBool		openURLFlag = PR_FALSE;

			// show an alert?
			if (schedule.Find(nsAutoString("alert"), PR_TRUE, 0) >= 0)
			{
				NS_WITH_SERVICE(nsIPrompt, dialog, kNetSupportDialogCID, &rv);
				if (NS_SUCCEEDED(rv))
				{
					// XXX localization (for the hard-coded strings below)

					nsAutoString	promptStr;
					promptStr += "The following web page has been updated:\n\n";
					nsCOMPtr<nsIRDFNode>	nameNode;
					if (NS_SUCCEEDED(mInner->GetTarget(busyResource, kNC_Name,
						PR_TRUE, getter_AddRefs(nameNode))))
					{
						nsCOMPtr<nsIRDFLiteral>	nameLiteral = do_QueryInterface(nameNode);
						if (nameLiteral)
						{
							PRUnichar	*nameUni = nsnull;
							if (NS_SUCCEEDED(rv = nameLiteral->GetValueConst(&nameUni))
								&& (nameUni))
							{
								promptStr += "Title: ";
								promptStr += nameUni;
								promptStr += "\nURL: ";
							}
						}
					}
					promptStr += uri;
					promptStr += "\n\nWould you like to display it?";
					
					PRBool		stopCheckingFlag = PR_FALSE;
					rv = dialog->ConfirmCheck(promptStr.GetUnicode(),
						nsAutoString("Stop checking for updates on this web page").GetUnicode(),
						&stopCheckingFlag, &openURLFlag);
					if (NS_FAILED(rv))
					{
						openURLFlag = PR_FALSE;
						stopCheckingFlag = PR_FALSE;
					}
					if (stopCheckingFlag == PR_TRUE)
					{
#ifdef	DEBUG_BOOKMARK_PING_OUTPUT
						printf("\nStop checking this URL.\n");
#endif
						rv = mInner->Unassert(busyResource, kWEB_Schedule, scheduleNode);
						NS_ASSERTION(NS_SUCCEEDED(rv), "unable to unassert kWEB_Schedule");
					}
				}
			}
			
			// open the URL in a new window?
			if ((openURLFlag == PR_TRUE) ||
				(schedule.Find(nsAutoString("open"), PR_TRUE, 0) >= 0))
			{
				NS_WITH_SERVICE(nsIAppShellService, appShell, kAppShellServiceCID, &rv);
				if (NS_SUCCEEDED(rv))
				{
					nsCOMPtr<nsIURI> urlObj;
					const char *chromeURLStr = "chrome://navigator/content";
					if (NS_SUCCEEDED(rv))
					{
						nsCOMPtr<nsIURI>	url;
						if (NS_SUCCEEDED(rv = NS_NewURI(getter_AddRefs(url), chromeURLStr)))
						{
							appShell->PushThreadEventQueue();

							nsCOMPtr<nsIWebShellWindow>	window;
							if (NS_SUCCEEDED(rv = appShell->CreateTopLevelWindow(nsnull, url, PR_TRUE, PR_FALSE,
								NS_CHROME_ALL_CHROME, nsnull, NS_SIZETOCONTENT, NS_SIZETOCONTENT, getter_AddRefs(window))))
							{
								nsCOMPtr<nsIBrowserWindow> browser(do_QueryInterface(window));
								if (browser)
									browser->SetChrome(NS_CHROME_ALL_CHROME);

								// Spin into the modal loop.
								nsIAppShell *subshell;
								rv = nsComponentManager::CreateInstance(kAppShellCID, nsnull, kIAppShellIID, (void**)&subshell);
								if (NS_SUCCEEDED(rv))
								{
									subshell->Create(0, nsnull);
									subshell->Spinup(); // Spin up 

									// Specify that we want the window to remain locked until the chrome has loaded.
									window->LockUntilChromeLoad();
									PRBool locked = PR_FALSE;
									nsresult looprv = NS_OK;
									window->GetLockedState(locked);
									while (NS_SUCCEEDED(looprv) && locked)
									{
										void      *data;
										PRBool    isRealEvent;

										looprv = subshell->GetNativeEvent(isRealEvent, data);
										subshell->DispatchNativeEvent(isRealEvent, data);

										window->GetLockedState(locked);
									}
									subshell->Spindown();
									NS_RELEASE(subshell);
								}
								appShell->PopThreadEventQueue();

								nsCOMPtr<nsIWebShell>	content;
								if (NS_SUCCEEDED(rv = window->GetContentShellById(nsAutoString("content"),
									getter_AddRefs(content))))
								{
									content->LoadURL(nsAutoString(uri).GetUnicode() /* , "view" */);
								}
							}
						}
					}
				}
			}
		}
#ifdef	DEBUG_BOOKMARK_PING_OUTPUT
		else
		{
			printf("URL has not changed status.\n\n");
		}
#endif
	}

	busyResource = null_nsCOMPtr();
	busySchedule = PR_FALSE;

	return(NS_OK);
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

	rv = parser.AddBookmark(container, aURI, aOptionalTitle, now32,
				0L, 0L, nsnull, kNC_Bookmark, nsnull);

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

				// also update bookmark's "status"!
				nsCOMPtr<nsIRDFNode>	currentStatusNode;
				if (NS_SUCCEEDED(rv = mInner->GetTarget(bookmark, kWEB_Status, PR_TRUE,
					getter_AddRefs(currentStatusNode))) && (rv != NS_RDF_NO_VALUE))
				{
					rv = mInner->Unassert(bookmark, kWEB_Status, currentStatusNode);
					NS_ASSERTION(rv == NS_RDF_ASSERTION_ACCEPTED, "unable to Unassert changed status");
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
	nsCRT::free(newURLCStr);
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

							// output SCHEDULE
							WriteBookmarkProperties(ds, strm, child, kWEB_Schedule, kScheduleEquals, PR_FALSE);

							// output LAST_PING
							WriteBookmarkProperties(ds, strm, child, kWEB_LastPingDate, kLastPingEquals, PR_FALSE);

							// output PING_ETAG
							WriteBookmarkProperties(ds, strm, child, kWEB_LastPingETag, kPingETagEquals, PR_FALSE);

							// output PING_LAST_MODIFIED
							WriteBookmarkProperties(ds, strm, child, kWEB_LastPingModDate, kPingLastModEquals, PR_FALSE);

							// output PING_CONTENT_LEN
							WriteBookmarkProperties(ds, strm, child, kWEB_LastPingContentLen, kPingContentLenEquals, PR_FALSE);

							// output PING_STATUS
							WriteBookmarkProperties(ds, strm, child, kWEB_Status, kPingStatusEquals, PR_FALSE);

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
					nsCRT::free(name);
					name = nsnull;
				}
					
				if (NS_FAILED(rv))	break;
			}

			strm << indentation;
			strm << "</DL><p>\n";
		}
	}
	nsCRT::free(indentation);
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
	if (NS_SUCCEEDED(rv = ds->GetTarget(child, property, PR_TRUE, getter_AddRefs(node)))
		&& (rv != NS_RDF_NO_VALUE))
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

							nsCRT::free(escapedAttrib);
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
				nsCRT::free(attribute);
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
		 (aProperty == kNC_BookmarkAddDate) ||
		 (aProperty == kWEB_Schedule)) {
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
