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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert John Churchill    <rjc@netscape.com>
 *   Chris Waterson           <waterson@netscape.com>
 *   Pierre Phaneuf           <pp@ludusdesign.com>
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

#define NS_IMPL_IDS

/*
  The global bookmarks service.
 */

#include "nsBookmarksService.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsFileStream.h"
#include "nsIComponentManager.h"
#include "nsIDOMWindow.h"
#include "nsIObserverService.h"
#include "nsObserverService.h"
#include "nsIRDFContainer.h"
#include "nsIRDFContainerUtils.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsRDFCID.h"
#include "nsSpecialSystemDirectory.h"
#include "nsVoidArray.h"
#include "nsXPIDLString.h"
#include "nsISupportsPrimitives.h"
#include "prio.h"
#include "prlog.h"
#include "rdf.h"
#include "xp_core.h"
#include "prlong.h"
#include "prtime.h"
#include "nsEnumeratorUtils.h"
#include "nsEscape.h"
#include "nsIAtom.h"
#include "nsIDirectoryService.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsReadableUtils.h"

#include "nsISound.h"
#include "nsIPrompt.h"
#include "nsIWindowWatcher.h"
#include "nsIWebShell.h"
#include "nsWidgetsCID.h"
#include "nsIAppShell.h"

#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIIOService.h"
#include "nsIChannel.h"
#include "nsIHttpChannel.h"


#include "nsIInputStream.h"
#include "nsIInputStream.h"

#include "nsICharsetConverterManager.h"
#include "nsICharsetAlias.h"
#include "nsIPlatformCharset.h"
#include "nsIPref.h"

// Interfaces Needed
#include "nsIXULWindow.h"

nsIRDFResource		*kNC_IEFavoritesRoot;
nsIRDFResource		*kNC_Bookmark;
nsIRDFResource		*kNC_BookmarkSeparator;
nsIRDFResource		*kNC_BookmarkAddDate;
nsIRDFResource		*kNC_BookmarksRoot;
nsIRDFResource		*kNC_Description;
nsIRDFResource		*kNC_Folder;
nsIRDFResource		*kNC_FolderType;
nsIRDFResource		*kNC_IEFavorite;
nsIRDFResource		*kNC_IEFavoriteFolder;
nsIRDFResource		*kNC_Name;
nsIRDFResource		*kNC_NewBookmarkFolder;
nsIRDFResource		*kNC_NewSearchFolder;
nsIRDFResource		*kNC_PersonalToolbarFolder;
nsIRDFResource		*kNC_ShortcutURL;
nsIRDFResource		*kNC_URL;
nsIRDFResource		*kRDF_type;
nsIRDFResource		*kRDF_nextVal;
nsIRDFResource		*kWEB_LastModifiedDate;
nsIRDFResource		*kWEB_LastVisitDate;
nsIRDFResource		*kWEB_Schedule;
nsIRDFResource		*kWEB_Status;
nsIRDFResource		*kWEB_LastPingDate;
nsIRDFResource		*kWEB_LastPingETag;
nsIRDFResource		*kWEB_LastPingModDate;
nsIRDFResource    *kWEB_LastCharset;
nsIRDFResource		*kWEB_LastPingContentLen;

nsIRDFResource		*kNC_Parent;

nsIRDFResource		*kNC_BookmarkCommand_NewBookmark;
nsIRDFResource		*kNC_BookmarkCommand_NewFolder;
nsIRDFResource		*kNC_BookmarkCommand_NewSeparator;
nsIRDFResource		*kNC_BookmarkCommand_DeleteBookmark;
nsIRDFResource		*kNC_BookmarkCommand_DeleteBookmarkFolder;
nsIRDFResource		*kNC_BookmarkCommand_DeleteBookmarkSeparator;
nsIRDFResource		*kNC_BookmarkCommand_SetNewBookmarkFolder = nsnull;
nsIRDFResource		*kNC_BookmarkCommand_SetPersonalToolbarFolder;
nsIRDFResource		*kNC_BookmarkCommand_SetNewSearchFolder;
nsIRDFResource		*kNC_BookmarkCommand_Import;
nsIRDFResource		*kNC_BookmarkCommand_Export;

#define	BOOKMARK_TIMEOUT		15000		// fire every 15 seconds
// #define	DEBUG_BOOKMARK_PING_OUTPUT	1

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_CID(kRDFInMemoryDataSourceCID,   NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_CID(kRDFServiceCID,              NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFContainerCID,            NS_RDFCONTAINER_CID);
static NS_DEFINE_CID(kRDFContainerUtilsCID,       NS_RDFCONTAINERUTILS_CID);
static NS_DEFINE_CID(kIOServiceCID,		  NS_IOSERVICE_CID);
static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);
static NS_DEFINE_CID(kPrefCID,                    NS_PREF_CID);
static NS_DEFINE_IID(kSoundCID,                   NS_SOUND_CID);
static NS_DEFINE_CID(kStringBundleServiceCID,     NS_STRINGBUNDLESERVICE_CID);
static NS_DEFINE_CID(kPlatformCharsetCID,         NS_PLATFORMCHARSET_CID);

static const char kURINC_BookmarksRoot[]          = "NC:BookmarksRoot"; // XXX?
static const char kURINC_IEFavoritesRoot[]        = "NC:IEFavoritesRoot"; // XXX?
static const char kURINC_NewBookmarkFolder[]      = "NC:NewBookmarkFolder"; // XXX?
static const char kURINC_PersonalToolbarFolder[]  = "NC:PersonalToolbarFolder"; // XXX?
static const char kURINC_NewSearchFolder[]        = "NC:NewSearchFolder"; // XXX?
static const char kDefaultPersonalToolbarFolder[] = "Personal Toolbar Folder";
static const char kBookmarkCommand[]              = "http://home.netscape.com/NC-rdf#command?";

#define bookmark_properties  "chrome://communicator/locale/bookmarks/bookmark.properties"
#define NAVIGATOR_CHROME_URL "chrome://navigator/content/"

////////////////////////////////////////////////////////////////////////

PRInt32			gRefCnt=0;
nsIRDFService		*gRDF;
nsIRDFContainerUtils	*gRDFC;
nsICharsetAlias		*gCharsetAlias;

static nsresult
bm_AddRefGlobals()
{
	if (gRefCnt++ == 0) {
		nsresult rv;
		rv = nsServiceManager::GetService(kRDFServiceCID,
						  NS_GET_IID(nsIRDFService),
						  (nsISupports**) &gRDF);

		NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF service");
		if (NS_FAILED(rv)) return rv;

		rv = nsServiceManager::GetService(kRDFContainerUtilsCID,
						  NS_GET_IID(nsIRDFContainerUtils),
						  (nsISupports**) &gRDFC);

		NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF container utils");
		if (NS_FAILED(rv)) return rv;

		rv = nsServiceManager::GetService(kCharsetAliasCID,
						  NS_GET_IID(nsICharsetAlias),
						  (nsISupports**) &gCharsetAlias);
		
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get charset alias service");
		if (NS_FAILED(rv)) return rv;

		gRDF->GetResource(kURINC_BookmarksRoot,                   &kNC_BookmarksRoot);
		gRDF->GetResource(kURINC_IEFavoritesRoot,                 &kNC_IEFavoritesRoot);
		gRDF->GetResource(kURINC_NewBookmarkFolder,               &kNC_NewBookmarkFolder);
		gRDF->GetResource(kURINC_PersonalToolbarFolder,           &kNC_PersonalToolbarFolder);
		gRDF->GetResource(kURINC_NewSearchFolder,                 &kNC_NewSearchFolder);

		gRDF->GetResource(NC_NAMESPACE_URI "Bookmark",            &kNC_Bookmark);
		gRDF->GetResource(NC_NAMESPACE_URI "BookmarkSeparator",   &kNC_BookmarkSeparator);
		gRDF->GetResource(NC_NAMESPACE_URI "BookmarkAddDate",     &kNC_BookmarkAddDate);
		gRDF->GetResource(NC_NAMESPACE_URI "Description",         &kNC_Description);
		gRDF->GetResource(NC_NAMESPACE_URI "Folder",              &kNC_Folder);
		gRDF->GetResource(NC_NAMESPACE_URI "FolderType",          &kNC_FolderType);
		gRDF->GetResource(NC_NAMESPACE_URI "IEFavorite",          &kNC_IEFavorite);
		gRDF->GetResource(NC_NAMESPACE_URI "IEFavoriteFolder",    &kNC_IEFavoriteFolder);
		gRDF->GetResource(NC_NAMESPACE_URI "Name",                &kNC_Name);
		gRDF->GetResource(NC_NAMESPACE_URI "ShortcutURL",         &kNC_ShortcutURL);
		gRDF->GetResource(NC_NAMESPACE_URI "URL",                 &kNC_URL);
		gRDF->GetResource(RDF_NAMESPACE_URI "type",               &kRDF_type);
		gRDF->GetResource(RDF_NAMESPACE_URI "nextVal",            &kRDF_nextVal);

		gRDF->GetResource(WEB_NAMESPACE_URI "LastModifiedDate",   &kWEB_LastModifiedDate);
		gRDF->GetResource(WEB_NAMESPACE_URI "LastVisitDate",      &kWEB_LastVisitDate);
    gRDF->GetResource(WEB_NAMESPACE_URI "LastCharset",        &kWEB_LastCharset);

		gRDF->GetResource(WEB_NAMESPACE_URI "Schedule",           &kWEB_Schedule);
		gRDF->GetResource(WEB_NAMESPACE_URI "status",             &kWEB_Status);
		gRDF->GetResource(WEB_NAMESPACE_URI "LastPingDate",       &kWEB_LastPingDate);
		gRDF->GetResource(WEB_NAMESPACE_URI "LastPingETag",       &kWEB_LastPingETag);
		gRDF->GetResource(WEB_NAMESPACE_URI "LastPingModDate",    &kWEB_LastPingModDate);
		gRDF->GetResource(WEB_NAMESPACE_URI "LastPingContentLen", &kWEB_LastPingContentLen);

		gRDF->GetResource(NC_NAMESPACE_URI "parent",              &kNC_Parent);

		gRDF->GetResource(NC_NAMESPACE_URI "command?cmd=newbookmark",             &kNC_BookmarkCommand_NewBookmark);
		gRDF->GetResource(NC_NAMESPACE_URI "command?cmd=newfolder",               &kNC_BookmarkCommand_NewFolder);
		gRDF->GetResource(NC_NAMESPACE_URI "command?cmd=newseparator",            &kNC_BookmarkCommand_NewSeparator);
		gRDF->GetResource(NC_NAMESPACE_URI "command?cmd=deletebookmark",          &kNC_BookmarkCommand_DeleteBookmark);
		gRDF->GetResource(NC_NAMESPACE_URI "command?cmd=deletebookmarkfolder",    &kNC_BookmarkCommand_DeleteBookmarkFolder);
		gRDF->GetResource(NC_NAMESPACE_URI "command?cmd=deletebookmarkseparator", &kNC_BookmarkCommand_DeleteBookmarkSeparator);
		gRDF->GetResource(NC_NAMESPACE_URI "command?cmd=setnewbookmarkfolder",    &kNC_BookmarkCommand_SetNewBookmarkFolder);
		gRDF->GetResource(NC_NAMESPACE_URI "command?cmd=setpersonaltoolbarfolder",&kNC_BookmarkCommand_SetPersonalToolbarFolder);
		gRDF->GetResource(NC_NAMESPACE_URI "command?cmd=setnewsearchfolder",      &kNC_BookmarkCommand_SetNewSearchFolder);
		gRDF->GetResource(NC_NAMESPACE_URI "command?cmd=import",                  &kNC_BookmarkCommand_Import);
		gRDF->GetResource(NC_NAMESPACE_URI "command?cmd=export",                  &kNC_BookmarkCommand_Export);
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

		if (gCharsetAlias)
		{
			nsServiceManager::ReleaseService(kCharsetAliasCID, gCharsetAlias);
			gCharsetAlias = nsnull;
		}

		NS_IF_RELEASE(kNC_Bookmark);
		NS_IF_RELEASE(kNC_BookmarkSeparator);
		NS_IF_RELEASE(kNC_BookmarkAddDate);
		NS_IF_RELEASE(kNC_BookmarksRoot);
		NS_IF_RELEASE(kNC_Description);
		NS_IF_RELEASE(kNC_Folder);
		NS_IF_RELEASE(kNC_FolderType);
		NS_IF_RELEASE(kNC_IEFavorite);
		NS_IF_RELEASE(kNC_IEFavoriteFolder);
		NS_IF_RELEASE(kNC_IEFavoritesRoot);
		NS_IF_RELEASE(kNC_Name);
		NS_IF_RELEASE(kNC_NewBookmarkFolder);
		NS_IF_RELEASE(kNC_NewSearchFolder);
		NS_IF_RELEASE(kNC_PersonalToolbarFolder);
		NS_IF_RELEASE(kNC_ShortcutURL);
		NS_IF_RELEASE(kNC_URL);
		NS_IF_RELEASE(kRDF_type);
		NS_IF_RELEASE(kRDF_nextVal);
		NS_IF_RELEASE(kWEB_LastModifiedDate);
		NS_IF_RELEASE(kWEB_LastVisitDate);
    NS_IF_RELEASE(kNC_Parent);

		NS_IF_RELEASE(kWEB_Schedule);
		NS_IF_RELEASE(kWEB_Status);
		NS_IF_RELEASE(kWEB_LastPingDate);
		NS_IF_RELEASE(kWEB_LastPingETag);
		NS_IF_RELEASE(kWEB_LastPingModDate);
		NS_IF_RELEASE(kWEB_LastPingContentLen);
    NS_IF_RELEASE(kWEB_LastCharset);
		NS_IF_RELEASE(kNC_BookmarkCommand_NewBookmark);
		NS_IF_RELEASE(kNC_BookmarkCommand_NewFolder);
		NS_IF_RELEASE(kNC_BookmarkCommand_NewSeparator);
		NS_IF_RELEASE(kNC_BookmarkCommand_DeleteBookmark);
		NS_IF_RELEASE(kNC_BookmarkCommand_DeleteBookmarkFolder);
		NS_IF_RELEASE(kNC_BookmarkCommand_DeleteBookmarkSeparator);
		NS_IF_RELEASE(kNC_BookmarkCommand_SetNewBookmarkFolder);
		NS_IF_RELEASE(kNC_BookmarkCommand_SetPersonalToolbarFolder);
		NS_IF_RELEASE(kNC_BookmarkCommand_SetNewSearchFolder);
		NS_IF_RELEASE(kNC_BookmarkCommand_Import);
		NS_IF_RELEASE(kNC_BookmarkCommand_Export);
	}
}



////////////////////////////////////////////////////////////////////////

/**
 * The bookmark parser knows how to read <tt>bookmarks.html</tt> and convert it
 * into an RDF graph.
 */
class BookmarkParser {
private:
  nsCOMPtr<nsIUnicodeDecoder>	mUnicodeDecoder;
  nsIRDFDataSource*           mDataSource;
  nsCString                   mIEFavoritesRoot;
  PRBool                      mFoundIEFavoritesRoot;
  PRBool                      mFoundPersonalToolbarFolder;
  PRBool                      mIsImportOperation;
  char*                       mContents;
  PRUint32                    mContentsLen;
  PRInt32                     mStartOffset;
  nsInputFileStream*          mInputStream;
  nsString                    mPersonalToolbarName;

friend	class nsBookmarksService;

protected:
    struct BookmarkField
    {
        const char      *mName;
        const char      *mPropertyName;
        nsIRDFResource  *mProperty;
        nsresult        (*mParse)(nsIRDFResource *arc, nsString& aValue, nsIRDFNode** aResult);
        nsIRDFNode      *mValue;
    };

    static BookmarkField gBookmarkFieldTable[];
    static BookmarkField gBookmarkHeaderFieldTable[];

	nsresult AssertTime(nsIRDFResource* aSource,
			    nsIRDFResource* aLabel,
			    PRInt32 aTime);

	nsresult setFolderHint(nsIRDFResource *newSource, nsIRDFResource *objType);

	static nsresult CreateAnonymousResource(nsIRDFResource** aResult);

	nsresult Unescape(nsString &text);

	nsresult ParseMetaTag(const nsString &aLine, nsIUnicodeDecoder **decoder);

	nsresult ParseBookmarkInfo(BookmarkField *fields, PRBool isBookmarkFlag, const nsString &aLine,
			       const nsCOMPtr<nsIRDFContainer> &aContainer,
			       nsIRDFResource *nodeType, nsCOMPtr<nsIRDFResource> &bookmarkNode);

	nsresult ParseBookmarkSeparator(const nsString &aLine,
					const nsCOMPtr<nsIRDFContainer> &aContainer);

	nsresult ParseHeaderBegin(const nsString &aLine,
				  const nsCOMPtr<nsIRDFContainer> &aContainer);

	nsresult ParseHeaderEnd(const nsString &aLine);

	PRInt32	getEOL(const char *whole, PRInt32 startOffset, PRInt32 totalLength);

    nsresult updateAtom(nsIRDFDataSource *db, nsIRDFResource *src,
			nsIRDFResource *prop, nsIRDFNode *newValue, PRBool *dirtyFlag);

static    nsresult ParseResource(nsIRDFResource *arc, nsString& aValue, nsIRDFNode** aResult);
static    nsresult ParseLiteral(nsIRDFResource *arc, nsString& aValue, nsIRDFNode** aResult);
static    nsresult ParseDate(nsIRDFResource *arc, nsString& aValue, nsIRDFNode** aResult);

public:
	BookmarkParser();
	~BookmarkParser();

	nsresult Init(nsFileSpec *fileSpec, nsIRDFDataSource *aDataSource, 
                const nsString &defaultPersonalToolbarName, PRBool aIsImportOperation = PR_FALSE);
	nsresult DecodeBuffer(nsString &line, char *buf, PRUint32 aLength);
	nsresult ProcessLine(nsIRDFContainer *aContainer, nsIRDFResource *nodeType,
			nsCOMPtr<nsIRDFResource> &bookmarkNode, const nsString &line,
			nsString &description, PRBool &inDescription, PRBool &isActiveFlag);
	nsresult Parse(nsIRDFResource* aContainer, nsIRDFResource *nodeType);

	nsresult AddBookmark(nsCOMPtr<nsIRDFContainer> aContainer,
			     const char*      aURL,
			     const PRUnichar* aOptionalTitle,
			     PRInt32          aAddDate,
			     PRInt32          aLastVisitDate,
			     PRInt32          aLastModifiedDate,
			     const char*      aShortcutURL,
			     nsIRDFResource*  aNodeType,
			     nsIRDFResource** bookmarkNode,
           const PRUnichar* aCharset,
           PRInt32          aIndex);

	nsresult SetIEFavoritesRoot(nsCString& IEFavoritesRootURL)
	{
		mIEFavoritesRoot = IEFavoritesRootURL;
		return(NS_OK);
	}
	nsresult ParserFoundIEFavoritesRoot(PRBool *foundIEFavoritesRoot)
	{
		*foundIEFavoritesRoot = mFoundIEFavoritesRoot;
		return(NS_OK);
	}
	nsresult ParserFoundPersonalToolbarFolder(PRBool *foundPersonalToolbarFolder)
	{
		*foundPersonalToolbarFolder = mFoundPersonalToolbarFolder;
		return(NS_OK);
	}
};

BookmarkParser::BookmarkParser()
	: mContents(nsnull), mContentsLen(0L), mStartOffset(0L), mInputStream(nsnull)
{
	bm_AddRefGlobals();
}

static const char kOpenAnchor[]  = "<A ";
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

static const char kOpenMeta[]   = "<META ";

static const char kNewBookmarkFolderEquals[]      = "NEW_BOOKMARK_FOLDER=\"";
static const char kNewSearchFolderEquals[]        = "NEW_SEARCH_FOLDER=\"";
static const char kPersonalToolbarFolderEquals[]  = "PERSONAL_TOOLBAR_FOLDER=\"";

static const char kHREFEquals[]            = "HREF=\"";
static const char kTargetEquals[]          = "TARGET=\"";
static const char kAddDateEquals[]         = "ADD_DATE=\"";
static const char kLastVisitEquals[]       = "LAST_VISIT=\"";
static const char kLastModifiedEquals[]    = "LAST_MODIFIED=\"";
static const char kLastCharsetEquals[]     = "LAST_CHARSET=\"";
static const char kShortcutURLEquals[]     = "SHORTCUTURL=\"";
static const char kScheduleEquals[]        = "SCHEDULE=\"";
static const char kLastPingEquals[]        = "LAST_PING=\"";
static const char kPingETagEquals[]        = "PING_ETAG=\"";
static const char kPingLastModEquals[]     = "PING_LAST_MODIFIED=\"";
static const char kPingContentLenEquals[]  = "PING_CONTENT_LEN=\"";
static const char kPingStatusEquals[]      = "PING_STATUS=\"";
static const char kIDEquals[]              = "ID=\"";
static const char kContentEquals[]         = "CONTENT=\"";
static const char kHTTPEquivEquals[]       = "HTTP-EQUIV=\"";
static const char kCharsetEquals[]         = "charset=";		// note: no quote

nsresult
BookmarkParser::Init(nsFileSpec *fileSpec, nsIRDFDataSource *aDataSource, 
                     const nsString &defaultPersonalToolbarName, PRBool aIsImportOperation)
{
	mDataSource = aDataSource;
	mFoundIEFavoritesRoot = PR_FALSE;
	mFoundPersonalToolbarFolder = PR_FALSE;
  mIsImportOperation = aIsImportOperation;
  mPersonalToolbarName = defaultPersonalToolbarName;

	nsresult		rv;

	// determine default platform charset...
	nsCOMPtr<nsIPlatformCharset> platformCharset = 
	         do_GetService(kPlatformCharsetCID, &rv);
	if (NS_SUCCEEDED(rv) && (platformCharset))
	{
		nsAutoString	defaultCharset;
		if (NS_SUCCEEDED(rv = platformCharset->GetCharset(kPlatformCharsetSel_4xBookmarkFile, defaultCharset)))
		{
			// found the default platform charset, now try and get a decoder from it to Unicode
			nsCOMPtr<nsICharsetConverterManager> charsetConv = 
			         do_GetService(kCharsetConverterManagerCID, &rv);
			if (NS_SUCCEEDED(rv) && (charsetConv))
			{
				rv = charsetConv->GetUnicodeDecoder(&defaultCharset, getter_AddRefs(mUnicodeDecoder));
			}
		}
	}

    nsCAutoString   str;
    BookmarkField   *field;
    for (field = gBookmarkFieldTable; field->mName; ++field)
    {
        str = field->mPropertyName;
        rv = gRDF->GetResource(str, &field->mProperty);
        if (NS_FAILED(rv))  return(rv);
    }
    for (field = gBookmarkHeaderFieldTable; field->mName; ++field)
    {
        str = field->mPropertyName;
        rv = gRDF->GetResource(str, &field->mProperty);
        if (NS_FAILED(rv))  return(rv);
    }

	if (fileSpec)
	{
		mContentsLen = fileSpec->GetFileSize();
		if (mContentsLen > 0)
		{
			mContents = new char [mContentsLen + 1];
			if (mContents)
			{
				nsInputFileStream inputStream(*fileSpec);	// defaults to read only
			       	PRInt32 howMany = inputStream.read(mContents, mContentsLen);
			        if (PRUint32(howMany) == mContentsLen)
			        {
					mContents[mContentsLen] = '\0';
			        }
			        else
			        {
			        	delete [] mContents;
			        	mContents = nsnull;
			        }
			}
		}

		if (!mContents)
		{
			// we were unable to read in the entire bookmark file at once,
			// so let's try reading it in a bit at a time instead
			mInputStream = new nsInputFileStream(*fileSpec);
			if (mInputStream)
			{
				if (! mInputStream->is_open())
				{
					delete mInputStream;
					mInputStream = nsnull;
				}
			}
		}
	}
	return(NS_OK);
}



BookmarkParser::~BookmarkParser()
{
	if (mContents)
	{
		delete [] mContents;
		mContents = nsnull;
	}
	if (mInputStream)
	{
		delete mInputStream;
		mInputStream = nsnull;
	}
	BookmarkField   *field;
    for (field = gBookmarkFieldTable; field->mName; ++field)
    {
        NS_IF_RELEASE(field->mProperty);
    }
    for (field = gBookmarkHeaderFieldTable; field->mName; ++field)
    {
        NS_IF_RELEASE(field->mProperty);
    }
	bm_ReleaseGlobals();
}

PRInt32
BookmarkParser::getEOL(const char *whole, PRInt32 startOffset, PRInt32 totalLength)
{
	PRInt32		eolOffset = -1;

	while (startOffset < totalLength)
	{
		char	c;
		c = whole[startOffset];
		if ((c == '\n') || (c == '\r') || (c == '\0'))
		{
			eolOffset = startOffset;
			break;
		}
		++startOffset;
	}
	return(eolOffset);
}

nsresult
BookmarkParser::DecodeBuffer(nsString &line, char *buf, PRUint32 aLength)
{
	if (mUnicodeDecoder)
	{
		nsresult	rv;
		char		*aBuffer = buf;
		PRInt32		unicharBufLen = 0;
		mUnicodeDecoder->GetMaxLength(aBuffer, aLength, &unicharBufLen);
		PRUnichar	*unichars = new PRUnichar [ unicharBufLen+1 ];
		do
		{
			PRInt32		srcLength = aLength;
			PRInt32		unicharLength = unicharBufLen;
			rv = mUnicodeDecoder->Convert(aBuffer, &srcLength, unichars, &unicharLength);
			unichars[unicharLength]=0;  //add this since the unicode converters can't be trusted to do so.

			// Move the nsParser.cpp 00 -> space hack to here so it won't break UCS2 file

			// Hack Start
			for(PRInt32 i=0;i<unicharLength-1; i++)
				if(0x0000 == unichars[i])	unichars[i] = 0x0020;
			// Hack End

			line.Append(unichars, unicharLength);
			// if we failed, we consume one byte by replace it with U+FFFD
			// and try conversion again.
			if(NS_FAILED(rv))
			{
				mUnicodeDecoder->Reset();
				line.Append( (PRUnichar)0xFFFD);
				if(((PRUint32) (srcLength + 1)) > (PRUint32)aLength)
					srcLength = aLength;
				else 
					srcLength++;
				aBuffer += srcLength;
				aLength -= srcLength;
			}
		} while (NS_FAILED(rv) && (aLength > 0));
		delete [] unichars;
		unichars = nsnull;
	}
	else
	{
		line.AppendWithConversion(buf, aLength);
	}
	return(NS_OK);
}



nsresult
BookmarkParser::ProcessLine(nsIRDFContainer *container, nsIRDFResource *nodeType,
			nsCOMPtr<nsIRDFResource> &bookmarkNode, const nsString &line,
			nsString &description, PRBool &inDescription, PRBool &isActiveFlag)
{
	nsresult	rv = NS_OK;
	PRInt32		offset;

	if (inDescription == PR_TRUE)
	{
		offset = line.FindChar('<');
		if (offset < 0)
		{
			if (description.Length() > 0)
			{
				description.AppendWithConversion("\n");
			}
			description += line;
			return(NS_OK);
		}

		Unescape(description);

		if (bookmarkNode)
		{
			nsCOMPtr<nsIRDFLiteral>	descLiteral;
			if (NS_SUCCEEDED(rv = gRDF->GetLiteral(description.get(), getter_AddRefs(descLiteral))))
			{
				rv = mDataSource->Assert(bookmarkNode, kNC_Description, descLiteral, PR_TRUE);
			}
		}

		inDescription = PR_FALSE;
		description.Truncate();
	}

	if ((offset = line.Find(kHREFEquals, PR_TRUE)) >= 0)
	{
		rv = ParseBookmarkInfo(gBookmarkFieldTable, PR_TRUE, line, container, nodeType, bookmarkNode);
	}
	else if ((offset = line.Find(kOpenMeta, PR_TRUE)) >= 0)
	{
		rv = ParseMetaTag(line, getter_AddRefs(mUnicodeDecoder));
	}
	else if ((offset = line.Find(kOpenHeading, PR_TRUE)) >= 0 &&
		 nsCRT::IsAsciiDigit(line.CharAt(offset + 2)))
	{
		// XXX Ignore <H1> so that bookmarks root _is_ <H1>
		if (line.CharAt(offset + 2) != PRUnichar('1'))
		{
		    nsCOMPtr<nsIRDFResource>    dummy;
			rv = ParseBookmarkInfo(gBookmarkHeaderFieldTable, PR_FALSE, line, container, nodeType, dummy);
		}
	}
	else if ((offset = line.Find(kSeparator, PR_TRUE)) >= 0)
	{
		rv = ParseBookmarkSeparator(line, container);
	}
	else if ((offset = line.Find(kCloseUL, PR_TRUE)) >= 0 ||
		 (offset = line.Find(kCloseMenu, PR_TRUE)) >= 0 ||
		 (offset = line.Find(kCloseDL, PR_TRUE)) >= 0)
	{
		isActiveFlag = PR_FALSE;
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
		description = line;
		description.Cut(0, offset+sizeof(kOpenDD)-1);
	}
	else
	{
		// XXX Discard the line?
	}
	return(rv);
}



nsresult
BookmarkParser::Parse(nsIRDFResource *aContainer, nsIRDFResource *nodeType)
{
	// tokenize the input stream.
	// XXX this needs to handle quotes, etc. it'd be nice to use the real parser for this...
	nsresult			rv;

	nsCOMPtr<nsIRDFContainer> container;
	rv = nsComponentManager::CreateInstance(kRDFContainerCID,
						nsnull,
						NS_GET_IID(nsIRDFContainer),
						getter_AddRefs(container));
	if (NS_FAILED(rv)) return rv;

	rv = container->Init(mDataSource, aContainer);
	if (NS_FAILED(rv)) return rv;

	nsCOMPtr<nsIRDFResource>	bookmarkNode = aContainer;
	nsAutoString			description, line;
	PRBool				isActiveFlag = PR_TRUE, inDescriptionFlag = PR_FALSE;

	if ((mContents) && (mContentsLen > 0))
	{
		// we were able to read the entire bookmark file into memory, so process it
		char				*linePtr;
		PRInt32				eol;

		while ((isActiveFlag == PR_TRUE) && (mStartOffset < (signed)mContentsLen))
		{
			linePtr = &mContents[mStartOffset];
			eol = getEOL(mContents, mStartOffset, mContentsLen);

			PRInt32	aLength;

			if ((eol >= mStartOffset) && (eol < (signed)mContentsLen))
			{
				// mContents[eol] = '\0';
				aLength = eol - mStartOffset;
				mStartOffset = eol + 1;
			}
			else
			{
				aLength = mContentsLen - mStartOffset;
				mStartOffset = mContentsLen + 1;
				isActiveFlag = PR_FALSE;
			}
			if (aLength < 1)	continue;

			line.Truncate();
			DecodeBuffer(line, linePtr, aLength);

			rv = ProcessLine(container, nodeType, bookmarkNode,
				line, description, inDescriptionFlag, isActiveFlag);
			if (NS_FAILED(rv))	break;
		}
	}
	else if (mInputStream)
	{
		// we were unable to read in the entire bookmark file at once,
		// so let's try reading it in a bit at a time instead, and process it
		while (NS_SUCCEEDED(rv) && (isActiveFlag == PR_TRUE) &&
			(!mInputStream->eof()) && (!mInputStream->failed()))
		{
			line.Truncate();

			while (PR_TRUE)
			{
				char	buf[256];
				PRBool	untruncated = mInputStream->readline(buf, sizeof(buf));

				// in.readline() return PR_FALSE if there was buffer overflow,
				// or there was a catastrophe. Check to see if we're here
				// because of the latter...
				NS_ASSERTION (! mInputStream->failed(), "error reading file");
				if (mInputStream->failed())
				{
					rv = NS_ERROR_FAILURE;
					break;
				}

				PRUint32		aLength;
				if (untruncated)	aLength = strlen(buf);
				else			aLength = sizeof(buf);

				DecodeBuffer(line, buf, aLength);
				
				if (untruncated)	break;
			}
			if (NS_SUCCEEDED(rv))
			{
				rv = ProcessLine(container, nodeType, bookmarkNode,
					line, description, inDescriptionFlag, isActiveFlag);
			}
		}
	}
	return(rv);
}



nsresult
BookmarkParser::Unescape(nsString &text)
{
	// convert some HTML-escaped (such as "&lt;") values back

	PRInt32		offset=0;

	while((offset = text.FindChar((PRUnichar('&')), PR_FALSE, offset)) >= 0)
	{
		// XXX get max of 6 chars; change the value below if
		// we ever start looking for longer HTML-escaped values
		nsAutoString	temp;
		text.Mid(temp, offset, 6);

		if (temp.CompareWithConversion("&lt;", PR_TRUE, 4) == 0)
		{
			text.Cut(offset, 4);
			text.Insert(PRUnichar('<'), offset);
		}
		else if (temp.CompareWithConversion("&gt;", PR_TRUE, 4) == 0)
		{
			text.Cut(offset, 4);
			text.Insert(PRUnichar('>'), offset);
		}
		else if (temp.CompareWithConversion("&amp;", PR_TRUE, 5) == 0)
		{
			text.Cut(offset, 5);
			text.Insert(PRUnichar('&'), offset);
		}
		else if (temp.CompareWithConversion("&quot;", PR_TRUE, 6) == 0)
		{
			text.Cut(offset, 6);
			text.Insert(PRUnichar('\"'), offset);
		}

		++offset;
	}
	return(NS_OK);
}



nsresult
BookmarkParser::CreateAnonymousResource(nsIRDFResource** aResult)
{
	static PRInt32 gNext = 0;
	if (! gNext) {
		LL_L2I(gNext, PR_Now());
	}
	nsAutoString    uri;
	uri.AssignWithConversion(kURINC_BookmarksRoot);
	uri.AppendWithConversion("#$");
	uri.AppendInt(++gNext, 16);

	return gRDF->GetUnicodeResource(uri.get(), aResult);
}



nsresult
BookmarkParser::ParseMetaTag(const nsString &aLine, nsIUnicodeDecoder **decoder)
{
	nsresult	rv = NS_OK;

	*decoder = nsnull;

	// get the HTTP-EQUIV attribute
	PRInt32 start = aLine.Find(kHTTPEquivEquals, PR_TRUE);
	NS_ASSERTION(start >= 0, "no 'HTTP-EQUIV=\"' string: how'd we get here?");
	if (start < 0)	return(NS_ERROR_UNEXPECTED);
	// Skip past the first double-quote
	start += (sizeof(kHTTPEquivEquals) - 1);
	// ...and find the next so we can chop the HTTP-EQUIV attribute
	PRInt32 end = aLine.FindChar(PRUnichar('"'), PR_FALSE, start);
	nsAutoString	httpEquiv;
	aLine.Mid(httpEquiv, start, end - start);

	// if HTTP-EQUIV isn't "Content-Type", just ignore the META tag
	if (!httpEquiv.EqualsIgnoreCase("Content-Type"))
		return(NS_OK);

	// get the CONTENT attribute
	start = aLine.Find(kContentEquals, PR_TRUE);
	NS_ASSERTION(start >= 0, "no 'CONTENT=\"' string: how'd we get here?");
	if (start < 0)	return(NS_ERROR_UNEXPECTED);
	// Skip past the first double-quote
	start += (sizeof(kContentEquals) - 1);
	// ...and find the next so we can chop the CONTENT attribute
	end = aLine.FindChar(PRUnichar('"'), PR_FALSE, start);
	nsAutoString	content;
	aLine.Mid(content, start, end - start);

	// look for the charset value
	start = content.Find(kCharsetEquals, PR_TRUE);
	NS_ASSERTION(start >= 0, "no 'charset=' string: how'd we get here?");
	if (start < 0)	return(NS_ERROR_UNEXPECTED);
	start += (sizeof(kCharsetEquals)-1);
	nsAutoString	charset;
	content.Mid(charset, start, content.Length() - start);
	if (charset.Length() < 1)	return(NS_ERROR_UNEXPECTED);

	if (gCharsetAlias)
	{
		nsAutoString	charsetName;
		if (NS_SUCCEEDED(rv = gCharsetAlias->GetPreferred(charset, charsetName)))
		{
			if (charsetName.Length() > 0)
			{
				charset = charsetName;
			}
		}
	}

	// found a charset, now try and get a decoder from it to Unicode
	nsICharsetConverterManager	*charsetConv = nsnull;
	rv = nsServiceManager::GetService(kCharsetConverterManagerCID, 
			NS_GET_IID(nsICharsetConverterManager), 
			(nsISupports**)&charsetConv);
	if (NS_SUCCEEDED(rv) && (charsetConv))
	{
		rv = charsetConv->GetUnicodeDecoder(&charset, decoder);
		NS_RELEASE(charsetConv);
	}
	return(rv);
}



BookmarkParser::BookmarkField
BookmarkParser::gBookmarkFieldTable[] =
{
  // Note: the first entry MUST be the URL/resource of the bookmark
  { kHREFEquals,            NC_NAMESPACE_URI  "URL",               nsnull,  BookmarkParser::ParseResource,  nsnull },
  { kAddDateEquals,         NC_NAMESPACE_URI  "BookmarkAddDate",   nsnull,  BookmarkParser::ParseDate,      nsnull },
  { kLastVisitEquals,       WEB_NAMESPACE_URI "LastVisitDate",     nsnull,  BookmarkParser::ParseDate,      nsnull },
  { kLastModifiedEquals,    WEB_NAMESPACE_URI "LastModifiedDate",  nsnull,  BookmarkParser::ParseDate,      nsnull },
  { kShortcutURLEquals,     NC_NAMESPACE_URI  "ShortcutURL",       nsnull,  BookmarkParser::ParseLiteral,   nsnull },
  { kLastCharsetEquals,     WEB_NAMESPACE_URI "LastCharset",       nsnull,  BookmarkParser::ParseLiteral,   nsnull },
  { kScheduleEquals,        WEB_NAMESPACE_URI "Schedule",          nsnull,  BookmarkParser::ParseLiteral,   nsnull },
  { kLastPingEquals,        WEB_NAMESPACE_URI "LastPingDate",      nsnull,  BookmarkParser::ParseDate,      nsnull },
  { kPingETagEquals,        WEB_NAMESPACE_URI "LastPingETag",      nsnull,  BookmarkParser::ParseLiteral,   nsnull },
  { kPingLastModEquals,     WEB_NAMESPACE_URI "LastPingModDate",   nsnull,  BookmarkParser::ParseLiteral,   nsnull },
  { kPingContentLenEquals,  WEB_NAMESPACE_URI "LastPingContentLen",nsnull,  BookmarkParser::ParseLiteral,   nsnull },
  { kPingStatusEquals,      WEB_NAMESPACE_URI "status",            nsnull,  BookmarkParser::ParseLiteral,   nsnull },
  // Note: end of table
  { nsnull,                 nsnull,                                nsnull,  nsnull,                         nsnull },
};



BookmarkParser::BookmarkField
BookmarkParser::gBookmarkHeaderFieldTable[] =
{
  // Note: the first entry MUST be the URL/resource of the bookmark
  { kIDEquals,                    NC_NAMESPACE_URI  "URL",               nsnull,  BookmarkParser::ParseResource,  nsnull },
  { kAddDateEquals,               NC_NAMESPACE_URI  "BookmarkAddDate",   nsnull,  BookmarkParser::ParseDate,      nsnull },
  { kLastModifiedEquals,          WEB_NAMESPACE_URI "LastModifiedDate",  nsnull,  BookmarkParser::ParseDate,      nsnull },
  // Note: these last three are specially handled
  { kNewBookmarkFolderEquals,     kURINC_NewBookmarkFolder,              nsnull,  BookmarkParser::ParseLiteral,   nsnull },
  { kNewSearchFolderEquals,       kURINC_NewSearchFolder,                nsnull,  BookmarkParser::ParseLiteral,   nsnull },
  { kPersonalToolbarFolderEquals, kURINC_PersonalToolbarFolder,          nsnull,  BookmarkParser::ParseLiteral,   nsnull },
  // Note: end of table
  { nsnull,                       nsnull,                                nsnull,  nsnull,                         nsnull },
};



nsresult
BookmarkParser::ParseBookmarkInfo(BookmarkField *fields, PRBool isBookmarkFlag,
                const nsString &aLine, const nsCOMPtr<nsIRDFContainer> &aContainer,
				nsIRDFResource *nodeType, nsCOMPtr<nsIRDFResource> &bookmarkNode)
{
	NS_PRECONDITION(aContainer != nsnull, "null ptr");
	if (! aContainer)
		return NS_ERROR_NULL_POINTER;

    bookmarkNode = nsnull;

    PRInt32     lineLen = aLine.Length();

    PRInt32     attrStart=0;
    if (isBookmarkFlag == PR_TRUE)
    {
        attrStart = aLine.Find(kOpenAnchor, PR_TRUE, attrStart);
        if (attrStart < 0)  return(NS_ERROR_UNEXPECTED);
        attrStart += sizeof(kOpenAnchor)-1;
    }
    else
    {
        attrStart = aLine.Find(kOpenHeading, PR_TRUE, attrStart);
        if (attrStart < 0)  return(NS_ERROR_UNEXPECTED);
        attrStart += sizeof(kOpenHeading)-1;
    }

    // free up any allocated data in field table BEFORE processing
    for (BookmarkField *preField = fields; preField->mName; ++preField)
    {
        NS_IF_RELEASE(preField->mValue);
    }

    // loop over attributes
    while((attrStart < lineLen) && (aLine[attrStart] != '>'))
    {
        while(nsCRT::IsAsciiSpace(aLine[attrStart]))   ++attrStart;

        PRBool  fieldFound = PR_FALSE;

        nsAutoString id;
        id.AssignWithConversion(kIDEquals);
        for (BookmarkField *field = fields; field->mName; ++field)
        {
            nsAutoString name;
            name.AssignWithConversion(field->mName);
            if (mIsImportOperation && name.Equals(id)) 
              // For import operations, we don't want to save the unique identifier for folders,
              // because this can cause bugs like 74969 (importing duplicate bookmark folder 
              // hierachy causes bookmarks file to grow infinitely).
              continue;
  
            if (aLine.Find(field->mName, PR_TRUE, attrStart, 1) == attrStart)
            {
                attrStart += nsCRT::strlen(field->mName);

                // skip to terminating quote of string
                PRInt32 termQuote = aLine.FindChar(PRUnichar('\"'), PR_FALSE, attrStart);
                if (termQuote > attrStart)
                {
                    // process data
                    nsAutoString    data;
                    aLine.Mid(data, attrStart, termQuote-attrStart);

                    attrStart = termQuote + 1;
                    fieldFound = PR_TRUE;

                    if (data.Length() > 0)
                    {
                        // XXX Bug 58421 We should not ever hit this assertion
                        NS_ASSERTION(!field->mValue, "Field already has a value");
                        // but prevent a leak if we do hit it
                        NS_IF_RELEASE(field->mValue);

                        nsresult rv = (*field->mParse)(field->mProperty, data, &field->mValue);
                        if (NS_FAILED(rv)) break;
                    }
                }
                break;
            }
        }

        if (fieldFound == PR_FALSE)
        {
            // skip to next attribute
            while((attrStart < lineLen) && (aLine[attrStart] != '>') &&
                (!nsCRT::IsAsciiSpace(aLine[attrStart])))
            {
                ++attrStart;
            }
        }
    }

    nsresult    rv;

    // Note: the first entry MUST be the URL/resource of the bookmark
    nsCOMPtr<nsIRDFResource>    bookmark = do_QueryInterface(fields[0].mValue);
    if ((!bookmark) && (isBookmarkFlag == PR_FALSE))
    {
		// We've never seen this folder before. Assign it an anonymous ID
		rv = CreateAnonymousResource(getter_AddRefs(bookmark));
		NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create anonymous resource for folder");
    }
    else if (bookmark.get() == kNC_PersonalToolbarFolder)
    {
		mFoundPersonalToolbarFolder = PR_TRUE;
    }

    if (bookmark)
    {
        const char* bookmarkURI;
        bookmark->GetValueConst(&bookmarkURI);

        bookmarkNode = bookmark;

        // assert appropriate node type
        PRBool		isIEFavoriteRoot = PR_FALSE;
        if (!mIEFavoritesRoot.IsEmpty())
        {
            if (!nsCRT::strcmp(mIEFavoritesRoot, bookmarkURI))
            {
            	mFoundIEFavoritesRoot = PR_TRUE;
            	isIEFavoriteRoot = PR_TRUE;
            }
        }
        if ((isIEFavoriteRoot == PR_TRUE) || ((nodeType == kNC_IEFavorite) && (isBookmarkFlag == PR_FALSE)))
        {
            mDataSource->Assert(bookmark, kRDF_type, kNC_IEFavoriteFolder, PR_TRUE);
        }
        else if (nodeType)
        {
            if (isBookmarkFlag == PR_TRUE)
            {
                rv = mDataSource->Assert(bookmark, kRDF_type, nodeType, PR_TRUE);
            }
            else
            {
                rv = mDataSource->Assert(bookmark, kRDF_type, kNC_Folder, PR_TRUE);
            }
        }

        // process data
        for (BookmarkField *field = fields; field->mName; ++field)
        {
            if (field->mValue)
            {
            	// check for and set various folder hints
            	if (field->mProperty == kNC_NewBookmarkFolder)
            	{
            		rv = setFolderHint(bookmark, kNC_NewBookmarkFolder);
            	}
            	else if (field->mProperty == kNC_NewSearchFolder)
            	{
            		rv = setFolderHint(bookmark, kNC_NewSearchFolder);
            	}
            	else if (field->mProperty == kNC_PersonalToolbarFolder)
            	{
            		rv = setFolderHint(bookmark, kNC_PersonalToolbarFolder);
            		mFoundPersonalToolbarFolder = PR_TRUE;
            	}
                else if (field->mProperty)
                {
        			updateAtom(mDataSource, bookmark, field->mProperty,
                               field->mValue, nsnull);
                }
            }
        }

        // look for bookmark name (and unescape)
        if (aLine[attrStart] == '>')
        {
            PRInt32 nameEnd;
            if (isBookmarkFlag == PR_TRUE)
                    nameEnd = aLine.Find(kCloseAnchor, PR_TRUE, ++attrStart);
            else    nameEnd = aLine.Find(kCloseHeading, PR_TRUE, ++attrStart);

            if (nameEnd > attrStart)
            {
                nsAutoString    name;
                aLine.Mid(name, attrStart, nameEnd-attrStart);
                if (name.Length() > 0)
                {
                    Unescape(name);

                    nsCOMPtr<nsIRDFNode>    nameNode;
                    rv = ParseLiteral(kNC_Name, name, getter_AddRefs(nameNode));
                    if (NS_SUCCEEDED(rv) && nameNode)
                    {
            			updateAtom(mDataSource, bookmark, kNC_Name, nameNode, nsnull);
                    }
                }
            }
        }

        if (isBookmarkFlag == PR_FALSE)
        {
            rv = gRDFC->MakeSeq(mDataSource, bookmark, nsnull);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to make new folder as sequence");
            if (NS_SUCCEEDED(rv))
            {
            	// And now recursively parse the rest of the file...
                rv = Parse(bookmark, nodeType);
            	NS_ASSERTION(NS_SUCCEEDED(rv), "unable to parse bookmarks");
            }
        }

	// The last thing we do is add the bookmark to the container.
	// This ensures the minimal amount of reflow.
	rv = aContainer->AppendElement(bookmark);
	NS_ASSERTION(NS_SUCCEEDED(rv), "unable to add bookmark to container");
    }

    // free up any allocated data in field table AFTER processing
    for (BookmarkField *postField = fields; postField->mName; ++postField)
    {
        NS_IF_RELEASE(postField->mValue);
    }

    return(NS_OK);
}



nsresult
BookmarkParser::ParseResource(nsIRDFResource *arc, nsString& url, nsIRDFNode** aResult)
{
    *aResult = nsnull;

    if (arc == kNC_URL)
    {
    	// Now do properly replace %22's; this is particularly important for javascript: URLs
    	static const char kEscape22[] = "%22";
    	PRInt32 offset;
    	while ((offset = url.Find(kEscape22)) >= 0)
    	{
    		url.SetCharAt('\"',offset);
    		url.Cut(offset + 1, sizeof(kEscape22) - 2);
    	}

    	// XXX At this point, the URL may be relative. 4.5 called into
    	// netlib to make an absolute URL, and there was some magic
    	// "relative_URL" parameter that got sent down as well. We punt on
    	// that stuff.

    	// hack fix for bug # 21175:
    	// if we don't have a protocol scheme, add "http://" as a default scheme
    	if (url.FindChar(PRUnichar(':')) < 0)
    	{
    		url.InsertWithConversion("http://", 0);
    	}
    }

    nsresult                    rv;
    nsCOMPtr<nsIRDFResource>    result;
    rv = gRDF->GetUnicodeResource(url.get(), getter_AddRefs(result));
    if (NS_FAILED(rv)) return rv;
    return result->QueryInterface(NS_GET_IID(nsIRDFNode), (void**) aResult);
}



nsresult
BookmarkParser::ParseLiteral(nsIRDFResource *arc, nsString& aValue, nsIRDFNode** aResult)
{
    *aResult = nsnull;

    if (arc == kNC_ShortcutURL)
    {
	    // lowercase the shortcut URL before storing internally
    	aValue.ToLowerCase();
    }
    else if (arc == kWEB_LastCharset)
    {
        if (gCharsetAlias)
        {
            gCharsetAlias->GetPreferred(aValue, aValue);
        }
    }
    else if (arc == kWEB_LastPingETag)
    {
		// don't allow quotes in etag
		PRInt32 offset;
		while ((offset = aValue.FindChar('\"')) >= 0)
		{
			aValue.Cut(offset, 1);
		}
    }

    nsresult                rv;
    nsCOMPtr<nsIRDFLiteral> result;
    rv = gRDF->GetLiteral(aValue.get(), getter_AddRefs(result));
    if (NS_FAILED(rv)) return rv;
    return result->QueryInterface(NS_GET_IID(nsIRDFNode), (void**) aResult);
}



nsresult
BookmarkParser::ParseDate(nsIRDFResource *arc, nsString& aValue, nsIRDFNode** aResult)
{
    *aResult = nsnull;

	PRInt32 theDate = 0;
	if (aValue.Length() > 0)
	{
		PRInt32 err;
		theDate = aValue.ToInteger(&err); // ignored.
	}
	if (theDate == 0)   return(NS_RDF_NO_VALUE);

	// convert from seconds to microseconds (PRTime)
	PRInt64		dateVal, temp, million;
	LL_I2L(temp, theDate);
	LL_I2L(million, PR_USEC_PER_SEC);
	LL_MUL(dateVal, temp, million);

    nsresult                rv;
	nsCOMPtr<nsIRDFDate>	result;
	if (NS_FAILED(rv = gRDF->GetDateLiteral(dateVal, getter_AddRefs(result))))
	{
		NS_ERROR("unable to get date literal for time");
		return(rv);
	}
    return result->QueryInterface(NS_GET_IID(nsIRDFNode), (void**) aResult);
}



nsresult
BookmarkParser::updateAtom(nsIRDFDataSource *db, nsIRDFResource *src,
			nsIRDFResource *prop, nsIRDFNode *newValue, PRBool *dirtyFlag)
{
	nsresult		rv;
	nsCOMPtr<nsIRDFNode>	oldValue;

	if (dirtyFlag != nsnull)
	{
		*dirtyFlag = PR_FALSE;
	}

	if (NS_SUCCEEDED(rv = db->GetTarget(src, prop, PR_TRUE, getter_AddRefs(oldValue))) &&
		(rv != NS_RDF_NO_VALUE))
	{
		rv = db->Change(src, prop, oldValue, newValue);

		if ((oldValue.get() != newValue) && (dirtyFlag != nsnull))
		{
			*dirtyFlag = PR_TRUE;
		}
	}
	else
	{
		rv = db->Assert(src, prop, newValue, PR_TRUE);
	}
	return(rv);
}



nsresult
BookmarkParser::AddBookmark(nsCOMPtr<nsIRDFContainer> aContainer,
                            const char*      aURL,
                            const PRUnichar* aOptionalTitle,
                            PRInt32          aAddDate,
                            PRInt32          aLastVisitDate,
                            PRInt32          aLastModifiedDate,
                            const char*      aShortcutURL,
                            nsIRDFResource*  aNodeType,
                            nsIRDFResource** bookmarkNode,
                            const PRUnichar* aCharset,
                            PRInt32          aIndex)
{
	nsresult	rv;
	nsAutoString	fullURL;
	fullURL.AssignWithConversion(aURL);

	// hack fix for bug # 21175:
	// if we don't have a protocol scheme, add "http://" as a default scheme
	if (fullURL.FindChar(PRUnichar(':')) < 0)
	{
		fullURL.InsertWithConversion("http://", 0);
	}

	nsCOMPtr<nsIRDFResource> bookmark;
	nsCAutoString fullurlC;
	fullurlC.AssignWithConversion(fullURL);
	if (NS_FAILED(rv = gRDF->GetResource(fullurlC, getter_AddRefs(bookmark) )))
	{
		NS_ERROR("unable to get bookmark resource");
		return(rv);
	}
	if (bookmarkNode)
	{
		*bookmarkNode = bookmark;
		NS_ADDREF(*bookmarkNode);
	}

	PRBool		isIEFavoriteRoot = PR_FALSE;

	if (!mIEFavoritesRoot.IsEmpty())
	{
		if (mIEFavoritesRoot.EqualsIgnoreCase(aURL))
		{
			mFoundIEFavoritesRoot = PR_TRUE;
			isIEFavoriteRoot = PR_TRUE;
		}
	}

	if (isIEFavoriteRoot == PR_TRUE)
	{
		rv = mDataSource->Assert(bookmark, kRDF_type, kNC_IEFavoriteFolder, PR_TRUE);
	}
	else
	{
		rv = mDataSource->Assert(bookmark, kRDF_type, aNodeType, PR_TRUE);
	}

	if (rv != NS_RDF_ASSERTION_ACCEPTED)
	{
		NS_ERROR("unable to add bookmark to data source");
		return(rv);
	}

	if ((nsnull != aOptionalTitle) && (*aOptionalTitle != PRUnichar('\0')))
	{
		nsCOMPtr<nsIRDFLiteral> titleLiteral;
		if (NS_SUCCEEDED(rv = gRDF->GetLiteral(aOptionalTitle, getter_AddRefs(titleLiteral))))
		{
		    updateAtom(mDataSource, bookmark, kNC_Name, titleLiteral, nsnull);
		}
		else
		{
			NS_ERROR("unable to create literal for bookmark name");
		}
	}

    AssertTime(bookmark, kNC_BookmarkAddDate, aAddDate);
    AssertTime(bookmark, kWEB_LastVisitDate, aLastVisitDate);
    AssertTime(bookmark, kWEB_LastModifiedDate, aLastModifiedDate);

    if ((nsnull != aCharset) && (*aCharset != PRUnichar('\0')))
    {
        nsCOMPtr<nsIRDFLiteral> charsetliteral;
        if (NS_SUCCEEDED(rv = gRDF->GetLiteral(aCharset, getter_AddRefs(charsetliteral))))
        {
            updateAtom(mDataSource, bookmark, kWEB_LastCharset, charsetliteral, nsnull);
        }
        else
        {
            NS_ERROR("unable to create literal for bookmark document charset");
        }
    }

	if ((nsnull != aShortcutURL) && (*aShortcutURL != '\0'))
	{
		nsCOMPtr<nsIRDFLiteral> shortcutLiteral;
		if (NS_SUCCEEDED(rv = gRDF->GetLiteral(NS_ConvertASCIItoUCS2(aShortcutURL).get(),
						    getter_AddRefs(shortcutLiteral))))
		{
		    updateAtom(mDataSource, bookmark, kNC_ShortcutURL, shortcutLiteral, nsnull);
		}
		else
		{
			NS_ERROR("unable to get literal for bookmark shortcut URL");
		}
	}

	// The last thing we do is add the bookmark to the container. This ensures the minimal amount of reflow.
  if (aIndex < 0)
  	rv = aContainer->AppendElement(bookmark);
  else
    rv = aContainer->InsertElementAt(bookmark, aIndex, PR_TRUE);
	NS_ASSERTION(NS_SUCCEEDED(rv), "unable to add bookmark to container");
	return(rv);
}



nsresult
BookmarkParser::ParseBookmarkSeparator(const nsString &aLine, const nsCOMPtr<nsIRDFContainer> &aContainer)
{
	nsresult			rv;
	nsCOMPtr<nsIRDFResource>	separator;

	if (NS_SUCCEEDED(rv = CreateAnonymousResource(getter_AddRefs(separator))))
	{
		nsAutoString		defaultSeparatorName;
		defaultSeparatorName.AssignWithConversion("-----");

		nsCOMPtr<nsIRDFLiteral> nameLiteral;
		if (NS_SUCCEEDED(rv = gRDF->GetLiteral(defaultSeparatorName.get(), getter_AddRefs(nameLiteral))))
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
BookmarkParser::ParseHeaderBegin(const nsString &aLine, const nsCOMPtr<nsIRDFContainer> &aContainer)
{
	return(NS_OK);
}



nsresult
BookmarkParser::ParseHeaderEnd(const nsString &aLine)
{
	return(NS_OK);
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
		updateAtom(mDataSource, aSource, aLabel, dateLiteral, nsnull);
	}
	return(rv);
}



nsresult
BookmarkParser::setFolderHint(nsIRDFResource *newSource, nsIRDFResource *objType)
{
	nsresult			rv;
	nsCOMPtr<nsISimpleEnumerator>	srcList;
	if (NS_FAILED(rv = mDataSource->GetSources(kNC_FolderType, objType, PR_TRUE, getter_AddRefs(srcList))))
		return(rv);

	PRBool	hasMoreSrcs = PR_TRUE;
	while(NS_SUCCEEDED(rv = srcList->HasMoreElements(&hasMoreSrcs))
		&& (hasMoreSrcs == PR_TRUE))
	{
		nsCOMPtr<nsISupports>	aSrc;
		if (NS_FAILED(rv = srcList->GetNext(getter_AddRefs(aSrc))))
			break;
		nsCOMPtr<nsIRDFResource>	aSource = do_QueryInterface(aSrc);
		if (!aSource)	continue;

		if (NS_FAILED(rv = mDataSource->Unassert(aSource, kNC_FolderType, objType)))
			continue;
	}

	rv = mDataSource->Assert(newSource, kNC_FolderType, objType, PR_TRUE);

	return(rv);
}



////////////////////////////////////////////////////////////////////////
// BookmarkDataSourceImpl

nsBookmarksService::nsBookmarksService()
	: mInner(nsnull), mBookmarksAvailable(PR_FALSE), mDirty(PR_FALSE), mUpdateBatchNest(0)

#ifdef	XP_MAC
	,mIEFavoritesAvailable(PR_FALSE)
#endif
{
	NS_INIT_REFCNT();
}



nsBookmarksService::~nsBookmarksService()
{
	if (mTimer)
	{
		// be sure to cancel the timer, as it holds a
		// weak reference back to nsBookmarksService
		mTimer->Cancel();
		mTimer = nsnull;
	}

	// Unregister ourselves from the RDF service
	if (gRDF)
	    gRDF->UnregisterDataSource(this);

	// Note: can't flush in the DTOR, as the RDF service
	// has probably already been destroyed
	// Flush();
	bm_ReleaseGlobals();
	NS_IF_RELEASE(mInner);
}



nsresult
nsBookmarksService::Init()
{
	nsresult rv;
	rv = bm_AddRefGlobals();
	if (NS_FAILED(rv))	return(rv);

	/* create a URL for the string resource file */
	nsCOMPtr<nsIIOService>	pNetService;
	if (NS_SUCCEEDED(rv = nsServiceManager::GetService(kIOServiceCID, NS_GET_IID(nsIIOService),
		getter_AddRefs(pNetService))))
	{
		nsCOMPtr<nsIURI>	uri;
		if (NS_SUCCEEDED(rv = pNetService->NewURI(bookmark_properties, nsnull,
			getter_AddRefs(uri))))
		{
			/* create a bundle for the localization */
			nsCOMPtr<nsIStringBundleService>	stringService;
			if (NS_SUCCEEDED(rv = nsServiceManager::GetService(kStringBundleServiceCID,
				NS_GET_IID(nsIStringBundleService), getter_AddRefs(stringService))))
			{
				char	*spec = nsnull;
				if (NS_SUCCEEDED(rv = uri->GetSpec(&spec)) && (spec))
				{
					if (NS_SUCCEEDED(rv = stringService->CreateBundle(spec,
						getter_AddRefs(mBundle))))
					{
					}
					nsCRT::free(spec);
					spec = nsnull;
				}
			}
		}
	}

	// determine what the name of the Personal Toolbar Folder is...
	// first from user preference, then string bundle, then hard-coded default
	nsCOMPtr<nsIPref> prefServ(do_GetService(kPrefCID, &rv));
	if (NS_SUCCEEDED(rv) && (prefServ))
	{
		char	*prefVal = nsnull;
		if (NS_SUCCEEDED(rv = prefServ->CopyCharPref("custtoolbar.personal_toolbar_folder",
			&prefVal)) && (prefVal))
		{
			if (*prefVal)
			{
				mPersonalToolbarName = NS_ConvertUTF8toUCS2(prefVal);
			}
			nsCRT::free(prefVal);
			prefVal = nsnull;
		}

		if (mPersonalToolbarName.Length() == 0)
		{
			// rjc note: always try to get the string bundle (see above) before trying this
			getLocaleString("DefaultPersonalToolbarFolder", mPersonalToolbarName);
		}

		if (mPersonalToolbarName.Length() == 0)
		{
			// no preference, so fallback to a well-known name
			mPersonalToolbarName.AssignWithConversion(kDefaultPersonalToolbarFolder);
		}
	}

    // Register as an observer of profile changes
    nsCOMPtr<nsIObserverService> observerService = 
             do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv);
    NS_ASSERTION(observerService, "Could not get observer service.");
    if (observerService) {
        observerService->AddObserver(this, "profile-before-change", PR_TRUE);
        observerService->AddObserver(this, "profile-do-change", PR_TRUE);
    }

	// read in bookmarks AFTER trying to get string bundle
	rv = ReadBookmarks();
	if (NS_FAILED(rv))	return(rv);

	/* timer initialization */
	busyResource = nsnull;

	if (!mTimer)
	{
		busySchedule = PR_FALSE;
		mTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
		NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create a timer");
		if (NS_FAILED(rv)) return rv;
		mTimer->Init(nsBookmarksService::FireTimer, this, BOOKMARK_TIMEOUT, NS_PRIORITY_LOWEST, NS_TYPE_REPEATING_SLACK);
		// Note: don't addref "this" as we'll cancel the timer in the nsBookmarkService destructor
	}

	// register this as a named data source with the RDF
	// service. Do this *last*, because if Init() fails, then the
	// object will be destroyed (leaving the RDF service with a
	// dangling pointer).
	rv = gRDF->RegisterDataSource(this, PR_FALSE);
	if (NS_FAILED(rv)) return rv;

	return NS_OK;
}



nsresult
nsBookmarksService::getLocaleString(const char *key, nsString &str)
{
	PRUnichar	*keyUni = nsnull;
	nsAutoString	keyStr;
	keyStr.AssignWithConversion(key);

	nsresult	rv = NS_RDF_NO_VALUE;
	if (mBundle && (NS_SUCCEEDED(rv = mBundle->GetStringFromName(keyStr.get(), &keyUni)))
		&& (keyUni))
	{
		str = keyUni;
		nsCRT::free(keyUni);
	}
	else
	{
		str.Truncate();
	}
	return(rv);
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
	dayNum.AppendInt(nowInfo.tm_wday, 10);

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
	char *methodStr = ToNewCString(notificationMethod);
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
	nsBookmarksService *bmks = NS_STATIC_CAST(nsBookmarksService *, aClosure);
	if (!bmks)	return;
	nsresult			rv;

	if ((bmks->mBookmarksAvailable == PR_TRUE) && (bmks->mDirty == PR_TRUE))
	{
		bmks->Flush();
	}

	if (bmks->busySchedule == PR_FALSE)
	{
		nsCOMPtr<nsIRDFResource>	bookmark;
		if (NS_SUCCEEDED(rv = bmks->GetBookmarkToPing(getter_AddRefs(bookmark))) && (bookmark))
		{
			bmks->busyResource = bookmark;
			const char		*url = nsnull;
			bookmark->GetValueConst(&url);

#ifdef	DEBUG_BOOKMARK_PING_OUTPUT
			printf("nsBookmarksService::FireTimer - Pinging '%s'\n", url);
#endif

			nsCOMPtr<nsIURI>	uri;
			if (NS_SUCCEEDED(rv = NS_NewURI(getter_AddRefs(uri), url)))
			{
				nsCOMPtr<nsIChannel>	channel;
				if (NS_SUCCEEDED(rv = NS_OpenURI(getter_AddRefs(channel), uri, nsnull)))
				{
					channel->SetLoadFlags(nsIRequest::VALIDATE_ALWAYS);
					nsCOMPtr<nsIHttpChannel>	httpChannel = do_QueryInterface(channel);
					if (httpChannel)
					{
						bmks->htmlSize = 0;
                        httpChannel->SetRequestMethod("HEAD");
						if (NS_SUCCEEDED(rv = channel->AsyncOpen(bmks, nsnull)))
						{
							bmks->busySchedule = PR_TRUE;
						}
					}
				}
			}
		}
	}
#ifdef	DEBUG_BOOKMARK_PING_OUTPUT
else
	{
	printf("nsBookmarksService::FireTimer - busy pinging.\n");
	}
#endif

#ifndef	REPEATING_TIMERS
	if (bmks->mTimer)
	{
		bmks->mTimer->Cancel();
		bmks->mTimer = nsnull;
	}
	bmks->mTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
	if (NS_FAILED(rv) || (!bmks->mTimer)) return;
	bmks->mTimer->Init(nsBookmarksService::FireTimer, bmks, BOOKMARK_TIMEOUT, NS_PRIORITY_LOWEST, NS_TYPE_REPEATING_SLACK);
	// Note: don't addref "this" as we'll cancel the timer in the nsBookmarkService destructor
#endif
}



// stream observer methods



NS_IMETHODIMP
nsBookmarksService::OnStartRequest(nsIRequest* request, nsISupports *ctxt)
{
	return(NS_OK);
}



NS_IMETHODIMP
nsBookmarksService::OnDataAvailable(nsIRequest* request, nsISupports *ctxt, nsIInputStream *aIStream,
					  PRUint32 sourceOffset, PRUint32 aLength)
{
	// calculate html page size if server doesn't tell us in headers
	htmlSize += aLength;

	return(NS_OK);
}



NS_IMETHODIMP
nsBookmarksService::OnStopRequest(nsIRequest* request, nsISupports *ctxt,
					nsresult status)
{
	nsresult		rv;

	const char		*uri = nsnull;
	if (NS_SUCCEEDED(rv = busyResource->GetValueConst(&uri)) && (uri))
	{
#ifdef	DEBUG_BOOKMARK_PING_OUTPUT
		printf("Finished polling '%s'\n", uri);
#endif
	}
    nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
	nsCOMPtr<nsIHttpChannel>	httpChannel = do_QueryInterface(channel);
	if (httpChannel)
	{
		nsCAutoString			eTagValue, lastModValue, contentLengthValue;

        nsXPIDLCString val;
        if (NS_SUCCEEDED(httpChannel->GetResponseHeader("ETag", getter_Copies(val))))
            eTagValue = val;
        if (NS_SUCCEEDED(httpChannel->GetResponseHeader("Last-Modified", getter_Copies(val))))
            lastModValue = val;
        if (NS_SUCCEEDED(httpChannel->GetResponseHeader("Content-Length", getter_Copies(val))))
            contentLengthValue = val;
        val.Adopt(0);

		PRBool		changedFlag = PR_FALSE;

		PRUint32	respStatus;
		if (NS_SUCCEEDED(rv = httpChannel->GetResponseStatus(&respStatus)))
		{
			if ((respStatus >= 200) && (respStatus <= 299))
			{
				if (eTagValue.Length() > 0)
				{
#ifdef	DEBUG_BOOKMARK_PING_OUTPUT
					printf("eTag: '%s'\n", eTagValue.get());
#endif
					nsAutoString		eTagStr;
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
							eTagStr.AssignWithConversion(eTagValue);
							nsCOMPtr<nsIRDFLiteral>	newETagLiteral;
							if (NS_SUCCEEDED(rv = gRDF->GetLiteral(eTagStr.get(),
								getter_AddRefs(newETagLiteral))))
							{
								rv = mInner->Change(busyResource, kWEB_LastPingETag,
									currentETagNode, newETagLiteral);
							}
						}
					}
					else
					{
						eTagStr.AssignWithConversion(eTagValue);
						nsCOMPtr<nsIRDFLiteral>	newETagLiteral;
						if (NS_SUCCEEDED(rv = gRDF->GetLiteral(eTagStr.get(),
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
			printf("Last-Modified: '%s'\n", lastModValue.get());
#endif
			nsAutoString		lastModStr;
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
					lastModStr.AssignWithConversion(lastModValue);
					nsCOMPtr<nsIRDFLiteral>	newLastModLiteral;
					if (NS_SUCCEEDED(rv = gRDF->GetLiteral(lastModStr.get(),
						getter_AddRefs(newLastModLiteral))))
					{
						rv = mInner->Change(busyResource, kWEB_LastPingModDate,
							currentLastModNode, newLastModLiteral);
					}
				}
			}
			else
			{
				lastModStr.AssignWithConversion(lastModValue);
				nsCOMPtr<nsIRDFLiteral>	newLastModLiteral;
				if (NS_SUCCEEDED(rv = gRDF->GetLiteral(lastModStr.get(),
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
			printf("Content-Length: '%s'\n", contentLengthValue.get());
#endif
			nsAutoString		contentLenStr;
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
					contentLenStr.AssignWithConversion(contentLengthValue);
					nsCOMPtr<nsIRDFLiteral>	newContentLengthLiteral;
					if (NS_SUCCEEDED(rv = gRDF->GetLiteral(contentLenStr.get(),
						getter_AddRefs(newContentLengthLiteral))))
					{
						rv = mInner->Change(busyResource, kWEB_LastPingContentLen,
							currentContentLengthNode, newContentLengthLiteral);
					}
				}
			}
			else
			{
				contentLenStr.AssignWithConversion(contentLengthValue);
				nsCOMPtr<nsIRDFLiteral>	newContentLengthLiteral;
				if (NS_SUCCEEDED(rv = gRDF->GetLiteral(contentLenStr.get(),
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

//			mDirty = PR_TRUE;
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
			if (schedule.Find(NS_LITERAL_STRING("icon").get(), PR_TRUE, 0) >= 0)
			{
				nsAutoString		statusStr;
				statusStr.AssignWithConversion("new");

				nsCOMPtr<nsIRDFLiteral>	statusLiteral;
				if (NS_SUCCEEDED(rv = gRDF->GetLiteral(statusStr.get(), getter_AddRefs(statusLiteral))))
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
			if (schedule.Find(NS_LITERAL_STRING("sound").get(), PR_TRUE, 0) >= 0)
			{
				nsCOMPtr<nsISound>	soundInterface;
				rv = nsComponentManager::CreateInstance(kSoundCID,
						nsnull, NS_GET_IID(nsISound),
						getter_AddRefs(soundInterface));
				if (NS_SUCCEEDED(rv))
				{
					// for the moment, just beep
					soundInterface->Beep();
				}
			}
			
			PRBool		openURLFlag = PR_FALSE;

			// show an alert?
			if (schedule.Find(NS_LITERAL_STRING("alert").get(), PR_TRUE, 0) >= 0)
			{
				nsCOMPtr<nsIInterfaceRequestor> interfaces;
				nsCOMPtr<nsIPrompt> prompter;

				channel->GetNotificationCallbacks(getter_AddRefs(interfaces));
				if (interfaces)
					interfaces->GetInterface(NS_GET_IID(nsIPrompt), getter_AddRefs(prompter));
				if (!prompter)
				{
					nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
					if (wwatch)
						wwatch->GetNewPrompter(0, getter_AddRefs(prompter));
				}

				if (prompter)
				{
					nsAutoString	promptStr;
					getLocaleString("WebPageUpdated", promptStr);
					if (promptStr.Length() > 0)	promptStr.AppendWithConversion("\n\n");

					nsCOMPtr<nsIRDFNode>	nameNode;
					if (NS_SUCCEEDED(mInner->GetTarget(busyResource, kNC_Name,
						PR_TRUE, getter_AddRefs(nameNode))))
					{
						nsCOMPtr<nsIRDFLiteral>	nameLiteral = do_QueryInterface(nameNode);
						if (nameLiteral)
						{
							const PRUnichar	*nameUni = nsnull;
							if (NS_SUCCEEDED(rv = nameLiteral->GetValueConst(&nameUni))
								&& (nameUni))
							{
								nsAutoString	info;
								getLocaleString("WebPageTitle", info);
								promptStr += info;
								promptStr.AppendWithConversion(" ");
								promptStr += nameUni;
								promptStr.AppendWithConversion("\n");
								getLocaleString("WebPageURL", info);
								promptStr += info;
								promptStr.AppendWithConversion(" ");
							}
						}
					}
					promptStr.AppendWithConversion(uri);
					
					nsAutoString	temp;
					getLocaleString("WebPageAskDisplay", temp);
					if (temp.Length() > 0)
					{
						promptStr.AppendWithConversion("\n\n");
						promptStr += temp;
					}

					nsAutoString	stopOption;
					getLocaleString("WebPageAskStopOption", stopOption);
					PRBool		stopCheckingFlag = PR_FALSE;
					rv = prompter->ConfirmCheck(nsnull, promptStr.get(), stopOption.get(),
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
				(schedule.Find(NS_LITERAL_STRING("open").get(), PR_TRUE, 0) >= 0))
			{
				if (NS_SUCCEEDED(rv))
				{
					nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
					if (wwatch)
					{
						nsCOMPtr<nsIDOMWindow> newWindow;
            nsCOMPtr<nsISupportsArray> suppArray;
            rv = NS_NewISupportsArray(getter_AddRefs(suppArray));
            if (NS_FAILED(rv)) return rv;
            nsCOMPtr<nsISupportsString> suppString(do_CreateInstance("@mozilla.org/supports-string;1", &rv));
            if (!suppString) return rv;
            rv = suppString->SetData(uri);
            if (NS_FAILED(rv)) return rv;
            suppArray->AppendElement(suppString);
            wwatch->OpenWindow(0, NAVIGATOR_CHROME_URL, "_blank", "chrome,dialog=no,all", 
                               suppArray, getter_AddRefs(newWindow));
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

	busyResource = nsnull;
	busySchedule = PR_FALSE;

	return(NS_OK);
}

// nsIObserver methods

NS_IMETHODIMP nsBookmarksService::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *someData)
{
  nsresult rv = NS_OK;

  if (!nsCRT::strcmp(aTopic, "profile-before-change"))
  {
    // The profile has not changed yet.
    rv = Flush();
    
    if (!nsCRT::strcmp(someData, NS_LITERAL_STRING("shutdown-cleanse").get())) {
      nsFileSpec	bookmarksFile;
	    rv = GetBookmarksFile(&bookmarksFile);
      if (NS_SUCCEEDED(rv) && bookmarksFile.IsFile())
        bookmarksFile.Delete(PR_FALSE);
    }
  }    
  else if (!nsCRT::strcmp(aTopic, "profile-do-change"))
  {
    // The profile has aleady changed.
    rv = ReadBookmarks();
  }

  return rv;
}


////////////////////////////////////////////////////////////////////////

NS_IMPL_ADDREF(nsBookmarksService);

NS_IMETHODIMP_(nsrefcnt)
nsBookmarksService::Release()
{
	// We need a special implementation of Release() because our mInner
	// holds a Circular References back to us.
	NS_PRECONDITION(PRInt32(mRefCnt) > 0, "duplicate release");
	--mRefCnt;
	NS_LOG_RELEASE(this, mRefCnt, "nsBookmarksService");

	if (mInner && mRefCnt == 1) {
		nsIRDFDataSource* tmp = mInner;
		mInner = nsnull;
		NS_IF_RELEASE(tmp);
		return 0;
	}
	else if (mRefCnt == 0) {
		delete this;
		return 0;
	}
	else {
		return mRefCnt;
	}
}



NS_IMPL_QUERY_INTERFACE8(nsBookmarksService, 
			 nsIBookmarksService,
			 nsIRDFDataSource,
			 nsIRDFRemoteDataSource,
			 nsIRDFObserver,
			 nsIStreamListener,
			 nsIRequestObserver,
			 nsIObserver,
			 nsISupportsWeakReference)

////////////////////////////////////////////////////////////////////////
// nsIBookmarksService

NS_IMETHODIMP
nsBookmarksService::AddBookmarkToFolder(const char *aURI,
                                        nsIRDFResource *aFolder,
                                        const PRUnichar* aTitle,
                                        const PRUnichar *aCharset)
{
  return InsertBookmarkInFolder(aURI, aTitle, aCharset, aFolder, -1);
}


NS_IMETHODIMP
nsBookmarksService::InsertBookmarkInFolder(const char *aURI,
                                           const PRUnichar* aTitle,
                                           const PRUnichar *aCharset,
                                           nsIRDFResource *aFolder,
                                           PRInt32 aIndex)
{
	// XXX Constructing a parser object to do this is bad.
	// We need to factor AddBookmark() into its own little
	// routine or something.

  nsresult rv;

	BookmarkParser parser;
	parser.Init(nsnull, mInner, mPersonalToolbarName);


	nsCOMPtr<nsIRDFContainer> container;
	rv = nsComponentManager::CreateInstance(kRDFContainerCID,
						nsnull,
						NS_GET_IID(nsIRDFContainer),
						getter_AddRefs(container));
	if (NS_FAILED(rv)) return rv;

	rv = container->Init(this, aFolder);
	if (NS_FAILED(rv)) return rv;

	// convert the current date/time from microseconds (PRTime) to seconds
	PRTime		now64 = PR_Now(), million;
	LL_I2L(million, PR_USEC_PER_SEC);
	LL_DIV(now64, now64, million);
	PRInt32		now32;
	LL_L2I(now32, now64);

	rv = parser.AddBookmark(container, aURI, aTitle, now32,
				0L, 0L, nsnull, kNC_Bookmark, nsnull, aCharset, aIndex);

	if (NS_FAILED(rv)) return rv;

	mDirty = PR_TRUE;
	Flush();

  return NS_OK;
}


NS_IMETHODIMP
nsBookmarksService::AddBookmark(const char *aURI,
                                const PRUnichar *aTitle, 
                                PRInt32 aBookmarkType,
                                const PRUnichar *aCharset)
{
  nsresult rv;

  // figure out where to add the new bookmark
	nsCOMPtr<nsIRDFResource>	bookmarkFolder = kNC_NewBookmarkFolder;

	switch(aBookmarkType)
	{
		case	BOOKMARK_SEARCH_TYPE:
		case	BOOKMARK_FIND_TYPE:
		bookmarkFolder = kNC_NewSearchFolder;
		break;
	}

  nsCOMPtr<nsIRDFResource> destinationFolder;
  rv = getFolderViaHint(bookmarkFolder, PR_TRUE, getter_AddRefs(destinationFolder));
  if (NS_FAILED(rv)) return rv;

  return AddBookmarkToFolder(aURI, destinationFolder, aTitle, aCharset);
}



NS_IMETHODIMP
nsBookmarksService::IsBookmarked(const char *aURI, PRBool *isBookmarkedFlag)
{
	if (!aURI)		return(NS_ERROR_UNEXPECTED);
	if (!isBookmarkedFlag)	return(NS_ERROR_UNEXPECTED);
	if (!mInner)		return(NS_ERROR_UNEXPECTED);

	*isBookmarkedFlag = PR_FALSE;

	nsresult			rv;
	nsCOMPtr<nsIRDFResource>	bookmark;

	// check if it has the proper type
	if (NS_FAILED(rv = gRDF->GetResource(aURI, getter_AddRefs(bookmark))))
		return(rv);

	// make sure it is referred to by an ordinal (i.e. is contained in a rdf seq)
	nsCOMPtr<nsISimpleEnumerator>	enumerator;
	if (NS_FAILED(rv = mInner->ArcLabelsIn(bookmark, getter_AddRefs(enumerator))))
		return(rv);
		
	PRBool	more = PR_TRUE;
	while(NS_SUCCEEDED(rv = enumerator->HasMoreElements(&more))
		&& (more == PR_TRUE))
	{
		nsCOMPtr<nsISupports>		isupports;
		if (NS_FAILED(rv = enumerator->GetNext(getter_AddRefs(isupports))))
			break;
		nsCOMPtr<nsIRDFResource>	property = do_QueryInterface(isupports);
		if (!property)	continue;

		PRBool	flag = PR_FALSE;
		if (NS_FAILED(rv = gRDFC->IsOrdinalProperty(property, &flag)))	continue;
		if (flag == PR_TRUE)
		{
			*isBookmarkedFlag = PR_TRUE;
			break;
		}
	}
	return(rv);
}


NS_IMETHODIMP
nsBookmarksService::GetLastCharset(const char *aURI,  PRUnichar **aLastCharset)
{
	if (!aURI)		  return(NS_ERROR_UNEXPECTED);
	if (!mInner)		return(NS_ERROR_UNEXPECTED);
	NS_PRECONDITION(aLastCharset != nsnull, "null ptr");
	if (!aLastCharset)	return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIRDFResource>	bookmark;
	nsresult			rv = nsnull;

	if (NS_SUCCEEDED(rv = gRDF->GetResource(aURI, getter_AddRefs(bookmark) )))
	{
		PRBool			isBookmark = PR_FALSE;

		// Note: always use mInner!! Otherwise, could get into an infinite loop
		// due to Assert/Change calling UpdateBookmarkLastModifiedDate()
		if (NS_SUCCEEDED(rv = mInner->HasAssertion(bookmark, kRDF_type, kNC_Bookmark,
			PR_TRUE, &isBookmark)) && (isBookmark == PR_TRUE))
		{
      nsCOMPtr<nsIRDFNode>	lastCharactersetNode;

			if (NS_SUCCEEDED(rv = mInner->GetTarget(bookmark, kWEB_LastCharset, PR_TRUE,
				getter_AddRefs(lastCharactersetNode))) && (rv != NS_RDF_NO_VALUE))
      {
	        nsCOMPtr<nsIRDFLiteral>	charsetLiteral = do_QueryInterface(lastCharactersetNode);

	        if (!charsetLiteral)	return(NS_ERROR_NO_INTERFACE);	        
	        if (NS_FAILED(rv = charsetLiteral->GetValue(aLastCharset))) return(rv);
	        if (!*aLastCharset)	return(NS_ERROR_NULL_POINTER);
          return(NS_OK);
			}

    }

  }

	*aLastCharset = nsnull;
	return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP
nsBookmarksService::UpdateBookmarkLastVisitedDate(const char *aURL, const PRUnichar *aCharset)
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

        //piggy-backing last charset...
        if ((nsnull != aCharset) && (*aCharset != PRUnichar('\0')))
        {
               nsCOMPtr<nsIRDFLiteral> charsetliteral;
               if (NS_FAILED(rv = gRDF->GetLiteral(aCharset, getter_AddRefs(charsetliteral))))
               {
                       NS_ERROR("unable to create literal for bookmark document charset");
               }

               if (NS_SUCCEEDED(rv))
               {
          				nsCOMPtr<nsIRDFNode>	lastCharacterset;

				          if (NS_SUCCEEDED(rv = mInner->GetTarget(bookmark, kWEB_LastCharset, PR_TRUE,
					          getter_AddRefs(lastCharacterset))) && (rv != NS_RDF_NO_VALUE))
                  {
					          rv = mInner->Change(bookmark, kWEB_LastCharset, lastCharacterset, charsetliteral);
				          }
				          else
				          {
					          rv = mInner->Assert(bookmark, kWEB_LastCharset, charsetliteral, PR_TRUE);
				          }
               }
        } 

				// also update bookmark's "status"!
				nsCOMPtr<nsIRDFNode>	currentStatusNode;
				if (NS_SUCCEEDED(rv = mInner->GetTarget(bookmark, kWEB_Status, PR_TRUE,
					getter_AddRefs(currentStatusNode))) && (rv != NS_RDF_NO_VALUE))
				{
					rv = mInner->Unassert(bookmark, kWEB_Status, currentStatusNode);
					NS_ASSERTION(rv == NS_RDF_ASSERTION_ACCEPTED, "unable to Unassert changed status");
				}

//				mDirty = PR_TRUE;
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

	// shortcuts are always lowercased internally
	nsAutoString		shortcut(aUserInput);
	shortcut.ToLowerCase();

	nsCOMPtr<nsIRDFLiteral> literalTarget;
	rv = gRDF->GetLiteral(shortcut.get(), getter_AddRefs(literalTarget));
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

NS_IMETHODIMP 
nsBookmarksService::GetAnonymousResource(nsIRDFResource** aResult)
{
  return BookmarkParser::CreateAnonymousResource(aResult);
}


////////////////////////////////////////////////////////////////////////
// nsIRDFDataSource



NS_IMETHODIMP
nsBookmarksService::GetURI(char* *aURI)
{
	*aURI = nsCRT::strdup("rdf:bookmarks");
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
		    && hasAssertion) ||
		    (NS_SUCCEEDED(mInner->HasAssertion(aSource, kRDF_type, kNC_Folder, PR_TRUE, &hasAssertion))
		    && hasAssertion))
		{
			const char	*uri;
			if (NS_FAILED(rv = aSource->GetValueConst( &uri )))
			{
				NS_ERROR("unable to get source's URI");
				return rv;
			}

			nsAutoString	ncURI; ncURI.AssignWithConversion(uri);
			if (ncURI.Find("NC:", PR_TRUE, 0) == 0)
			{
				return(NS_RDF_NO_VALUE);
			}

			nsIRDFLiteral* literal;
			if (NS_FAILED(rv = gRDF->GetLiteral(NS_ConvertASCIItoUCS2(uri).get(), &literal)))
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
			getLocaleString("NewBookmark", name);
		else if (aSource == kNC_BookmarkCommand_NewFolder)
			getLocaleString("NewFolder", name);
		else if (aSource == kNC_BookmarkCommand_NewSeparator)
			getLocaleString("NewSeparator", name);
		else if (aSource == kNC_BookmarkCommand_DeleteBookmark)
			getLocaleString("DeleteBookmark", name);
		else if (aSource == kNC_BookmarkCommand_DeleteBookmarkFolder)
			getLocaleString("DeleteFolder", name);
		else if (aSource == kNC_BookmarkCommand_DeleteBookmarkSeparator)
			getLocaleString("DeleteSeparator", name);
		else if (aSource == kNC_BookmarkCommand_SetNewBookmarkFolder)
			getLocaleString("SetNewBookmarkFolder", name);
		else if (aSource == kNC_BookmarkCommand_SetPersonalToolbarFolder)
			getLocaleString("SetPersonalToolbarFolder", name);
		else if (aSource == kNC_BookmarkCommand_SetNewSearchFolder)
			getLocaleString("SetNewSearchFolder", name);
		else if (aSource == kNC_BookmarkCommand_Import)
			getLocaleString("Import", name);
		else if (aSource == kNC_BookmarkCommand_Export)
			getLocaleString("Export", name);

		if (name.Length() > 0)
		{
			*aTarget = nsnull;
			nsCOMPtr<nsIRDFLiteral>	literal;
			if (NS_FAILED(rv = gRDF->GetLiteral(name.get(), getter_AddRefs(literal))))
				return(rv);
			*aTarget = literal;
			NS_IF_ADDREF(*aTarget);
			return(rv);
		}
	}

	rv = mInner->GetTarget(aSource, aProperty, aTruthValue, aTarget);
	return(rv);
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
		if (aProperty == kNC_URL)
		{
			nsCOMPtr<nsIRDFResource> newURL;
			rv = getResourceFromLiteralNode(aTarget, getter_AddRefs(newURL));
			if (NS_FAILED(rv)) return rv;
			
			rv = ChangeURL(aSource, newURL);
		}
		else if (NS_SUCCEEDED(rv = mInner->Assert(aSource, aProperty, aTarget, aTruthValue)))
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

	if (aProperty == kNC_URL) {
		// We can't accept somebody trying to remove a URL. Sorry!
	}
	else if (CanAccept(aSource, aProperty, aTarget))
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

	nsCOMPtr<nsIRDFResource>	newURLRes = do_QueryInterface(node);
	if (newURLRes)
	{
		*res = newURLRes;
		NS_IF_ADDREF(*res);
		return(NS_OK);
	}

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
	rv = gRDF->GetUnicodeResource(newURL, res);
	return(rv);
}



nsresult
nsBookmarksService::ChangeURL(nsIRDFResource* aOldURL,
			      nsIRDFResource* aNewURL)
{
	nsresult rv;

	// Make all arcs coming out of aOldURL also come out of
	// aNewURL.  Wallop any previous values.
	
	nsCOMPtr<nsISimpleEnumerator>	arcsOut;
	rv = mInner->ArcLabelsOut(aOldURL, getter_AddRefs(arcsOut));
	if (NS_FAILED(rv)) return(rv);

	while (1)
	{
		PRBool hasMoreArcsOut;
		rv = arcsOut->HasMoreElements(&hasMoreArcsOut);
		if (NS_FAILED(rv)) return rv;

		if (! hasMoreArcsOut)
			break;

		nsCOMPtr<nsISupports> arc;
		rv = arcsOut->GetNext(getter_AddRefs(arc));
		if (NS_FAILED(rv)) return rv;

		nsCOMPtr<nsIRDFResource> property = do_QueryInterface(arc);
		NS_ASSERTION(property != nsnull, "arc is not a property");
		if (!property)
			return NS_ERROR_UNEXPECTED;

		// don't copy URL property as it is special
		if (property.get() == kNC_URL)
			continue;

		// XXX What if more than one target?
		nsCOMPtr<nsIRDFNode> oldvalue;
		rv = mInner->GetTarget(aNewURL, property, PR_TRUE, getter_AddRefs(oldvalue));
		if (NS_FAILED(rv)) return rv;

		nsCOMPtr<nsIRDFNode> newvalue;
		rv = mInner->GetTarget(aOldURL, property, PR_TRUE, getter_AddRefs(newvalue));
		if (NS_FAILED(rv)) return rv;

		if (oldvalue) {
			if (newvalue) {
				rv = mInner->Change(aNewURL, property, oldvalue, newvalue);
			}
			else {
				rv = mInner->Unassert(aNewURL, property, oldvalue);
			}
		}
		else if (newvalue) {
			rv = mInner->Assert(aNewURL, property, newvalue, PR_TRUE);
		}
		else {
			// do nothing
			rv = NS_OK;
		}

		if (NS_FAILED(rv)) return rv;
	}
	
	// Make all arcs pointing to aOldURL now point to aNewURL
	nsCOMPtr<nsISimpleEnumerator> arcsIn;
	rv = mInner->ArcLabelsIn(aOldURL, getter_AddRefs(arcsIn));
	if (NS_FAILED(rv)) return rv;

	while (1)
	{
		PRBool hasMoreArcsIn;
		rv = arcsIn->HasMoreElements(&hasMoreArcsIn);
		if (NS_FAILED(rv)) return rv;

		if (! hasMoreArcsIn)
			break;
		
		nsCOMPtr<nsIRDFResource> property;

		{
			nsCOMPtr<nsISupports> isupports;
			rv = arcsIn->GetNext(getter_AddRefs(isupports));
			if (NS_FAILED(rv)) return rv;

			property = do_QueryInterface(isupports);
			NS_ASSERTION(property != nsnull, "arc is not a property");
			if (! property)
				return NS_ERROR_UNEXPECTED;
		}

		nsCOMPtr<nsISimpleEnumerator> sources;
		rv = GetSources(property, aOldURL, PR_TRUE, getter_AddRefs(sources));
		if (NS_FAILED(rv)) return rv;
		
		while (1)
		{
			PRBool hasMoreSrcs;
			rv = sources->HasMoreElements(&hasMoreSrcs);
			if (NS_FAILED(rv)) return rv;

			if (! hasMoreSrcs)
				break;

			nsCOMPtr<nsISupports> isupports;
			rv = sources->GetNext(getter_AddRefs(isupports));
			if (NS_FAILED(rv)) return rv;

			nsCOMPtr<nsIRDFResource> source = do_QueryInterface(isupports);
			NS_ASSERTION(source != nsnull, "source is not a resource");
			if (! source)
				return NS_ERROR_UNEXPECTED;

			rv = mInner->Change(source, property, aOldURL, aNewURL);
			if (NS_FAILED(rv)) return rv;
		}
	}

	// Set a notification that the URL property changed, so that
	// anyone observing it'll update correctly.
	{
		const char* uri;
		rv = aNewURL->GetValueConst(&uri);
		if (NS_FAILED(rv)) return rv;

		nsCOMPtr<nsIRDFLiteral> literal;
		rv = gRDF->GetLiteral(NS_ConvertASCIItoUCS2(uri).get(), getter_AddRefs(literal));
		if (NS_FAILED(rv)) return rv;

		// XXX rjc: was just aNewURL. Don't both aOldURL as well as aNewURL need to be pinged?
		rv = OnAssert(this, aOldURL, kNC_URL, literal);
		if (NS_FAILED(rv)) return rv;
		rv = OnAssert(this, aNewURL, kNC_URL, literal);
		if (NS_FAILED(rv)) return rv;
	}


	return NS_OK;
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
			// It should be the case that aOldTarget
			// points to a literal whose value is the same
			// as aSource's URI.
			nsCOMPtr<nsIRDFResource> newURL;
			rv = getResourceFromLiteralNode(aNewTarget, getter_AddRefs(newURL));
			if (NS_FAILED(rv)) return rv;
			
			rv = ChangeURL(aSource, newURL);
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
nsBookmarksService::HasAssertion(nsIRDFResource* source,
				nsIRDFResource* property,
				nsIRDFNode* target,
				PRBool tv,
				PRBool* hasAssertion)
{
#ifdef	XP_MAC
	    // on the Mac, IE favorites are stored in an HTML file.
	    // Defer importing this files contents until necessary.

	    if ((source == kNC_IEFavoritesRoot) && (mIEFavoritesAvailable == PR_FALSE))
	    {
		    ReadFavorites();
	    }
#endif
		return mInner->HasAssertion(source, property, target, tv, hasAssertion);
}


NS_IMETHODIMP
nsBookmarksService::AddObserver(nsIRDFObserver* aObserver)
{
	if (! aObserver)
		return NS_ERROR_NULL_POINTER;

	if (! mObservers) {
		nsresult rv;
		rv = NS_NewISupportsArray(getter_AddRefs(mObservers));
		if (NS_FAILED(rv)) return rv;
	}

	mObservers->AppendElement(aObserver);
	return NS_OK;
}


NS_IMETHODIMP
nsBookmarksService::RemoveObserver(nsIRDFObserver* aObserver)
{
	if (! aObserver)
		return NS_ERROR_NULL_POINTER;

	if (mObservers) {
		mObservers->RemoveElement(aObserver);
	}

	return NS_OK;
}

NS_IMETHODIMP
nsBookmarksService::HasArcIn(nsIRDFNode *aNode, nsIRDFResource *aArc, PRBool *_retval)
{
#ifdef	XP_MAC
	    // on the Mac, IE favorites are stored in an HTML file.
	    // Defer importing this files contents until necessary.

	    if ((aNode == kNC_IEFavoritesRoot) && (mIEFavoritesAvailable == PR_FALSE))
	    {
		    ReadFavorites();
	    }
#endif
	    return mInner->HasArcIn(aNode, aArc, _retval);
}

NS_IMETHODIMP
nsBookmarksService::HasArcOut(nsIRDFResource *aSource, nsIRDFResource *aArc, PRBool *_retval)
{
#ifdef	XP_MAC
	    // on the Mac, IE favorites are stored in an HTML file.
	    // Defer importing this files contents until necessary.

	    if ((aSource == kNC_IEFavoritesRoot) && (mIEFavoritesAvailable == PR_FALSE))
	    {
		    ReadFavorites();
	    }
#endif
	    return mInner->HasArcOut(aSource, aArc, _retval);
}

NS_IMETHODIMP
nsBookmarksService::ArcLabelsOut(nsIRDFResource* source,
				nsISimpleEnumerator** labels)
{
#ifdef	XP_MAC

		// on the Mac, IE favorites are stored in an HTML file.
		// Defer importing this files contents until necessary.

		if ((source == kNC_IEFavoritesRoot) && (mIEFavoritesAvailable == PR_FALSE))
		{
			ReadFavorites();
		}
#endif

		return mInner->ArcLabelsOut(source, labels);
}

NS_IMETHODIMP
nsBookmarksService::GetAllResources(nsISimpleEnumerator** aResult)
{
#ifdef	XP_MAC
		if (mIEFavoritesAvailable == PR_FALSE)
		{
			ReadFavorites();
		}
#endif
		return mInner->GetAllResources(aResult);
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
		cmdArray->AppendElement(kNC_BookmarkSeparator);
	}
	if (isBookmark)
	{
		cmdArray->AppendElement(kNC_BookmarkCommand_DeleteBookmark);
	}
	if (isBookmarkFolder && (source != kNC_BookmarksRoot) && (source != kNC_IEFavoritesRoot))
	{
		cmdArray->AppendElement(kNC_BookmarkCommand_DeleteBookmarkFolder);
	}
	if (isBookmarkSeparator)
	{
		cmdArray->AppendElement(kNC_BookmarkCommand_DeleteBookmarkSeparator);
	}
	if (isBookmarkFolder)
	{
		nsCOMPtr<nsIRDFResource>	newBookmarkFolder, personalToolbarFolder, newSearchFolder;
		getFolderViaHint(kNC_NewBookmarkFolder, PR_FALSE, getter_AddRefs(newBookmarkFolder));
		getFolderViaHint(kNC_PersonalToolbarFolder, PR_FALSE, getter_AddRefs(personalToolbarFolder));
		getFolderViaHint(kNC_NewSearchFolder, PR_FALSE, getter_AddRefs(newSearchFolder));

		cmdArray->AppendElement(kNC_BookmarkSeparator);
		if (source != newBookmarkFolder.get())		cmdArray->AppendElement(kNC_BookmarkCommand_SetNewBookmarkFolder);
		if (source != newSearchFolder.get())		cmdArray->AppendElement(kNC_BookmarkCommand_SetNewSearchFolder);
		if (source != personalToolbarFolder.get())	cmdArray->AppendElement(kNC_BookmarkCommand_SetPersonalToolbarFolder);
	}

	// always append a separator last (due to aggregation of commands from multiple datasources)
	cmdArray->AppendElement(kNC_BookmarkSeparator);

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
				PRInt32 offset, nsIRDFNode **argValue)
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
			nsCOMPtr<nsIRDFNode>	val = do_QueryInterface(aValue);
			if (!val)	return(NS_ERROR_NO_INTERFACE);

			*argValue = val;
			NS_ADDREF(*argValue);
			return(NS_OK);
		}
	}
	return(NS_ERROR_INVALID_ARG);
}


/** 
 * Inserts a bookmark item of the given type adjacent to the node specified 
 * with aRelativeNode. Creation parameters are given in aArguments. 
 */
nsresult
nsBookmarksService::insertBookmarkItem(nsIRDFResource *aRelativeNode, 
                                       nsISupportsArray *aArguments, 
                                       nsIRDFResource *aItemType)
{
  nsresult rv;
  const PRInt32 kParentArgumentIndex = 0;
  
  nsCOMPtr<nsIRDFResource> rParent;

  if (aRelativeNode == kNC_BookmarksRoot)
    rParent = aRelativeNode;
  else {
    nsCOMPtr<nsIRDFNode> parentNode;
    rv = getArgumentN(aArguments, kNC_Parent, kParentArgumentIndex, getter_AddRefs(parentNode));
    if (NS_FAILED(rv)) return rv;
    rParent = do_QueryInterface(parentNode, &rv);
    if (NS_FAILED(rv)) return rv;
  }

  nsCOMPtr<nsIRDFContainer> container(do_GetService("@mozilla.org/rdf/container;1", &rv));
  if (NS_FAILED(rv)) return rv;

  rv = container->Init(this, rParent);
  if (NS_FAILED(rv)) return rv;

  PRInt32 relNodeIdx = 0;
  if (aRelativeNode != kNC_BookmarksRoot) {
    // Find the position of the relative node, so we can create this item 
    // adjacent to it.
    rv = container->IndexOf(aRelativeNode, &relNodeIdx);
    if (NS_FAILED(rv)) return rv;

    // If the item isn't in the container, just append to the container.
    if (relNodeIdx == -1) {
      rv = container->GetCount(&relNodeIdx);
      if (NS_FAILED(rv)) return rv;
    }
  }

  nsAutoString itemName;
  
  // If a creation name was supplied, use it. 
  if (aItemType == kNC_Bookmark || aItemType == kNC_Folder) {
    nsCOMPtr<nsIRDFNode> nameNode;
    getArgumentN(aArguments, kNC_Name, kParentArgumentIndex, getter_AddRefs(nameNode));
    nsCOMPtr<nsIRDFLiteral> nameLiteral;
    nameLiteral = do_QueryInterface(nameNode);
    if (nameLiteral) {
      const PRUnichar* uName = nsnull;
      nameLiteral->GetValueConst(&uName);
      if (uName) 
        itemName = uName;
    }
  }

  if (itemName.IsEmpty()) {
    // Retrieve a default name from the bookmark properties file.
    if (aItemType == kNC_Bookmark) 
      getLocaleString("NewBookmark", itemName);
    else if (aItemType == kNC_Folder) 
      getLocaleString("NewFolder", itemName);
  }

  nsCOMPtr<nsIRDFResource> newResource;
  
  // Retrieve the URL from the arguments list. 
  if (aItemType == kNC_Bookmark || aItemType == kNC_Folder) {
    nsCOMPtr<nsIRDFNode> urlNode;
    getArgumentN(aArguments, kNC_URL, kParentArgumentIndex, getter_AddRefs(urlNode));
    nsCOMPtr<nsIRDFLiteral> bookmarkURILiteral(do_QueryInterface(urlNode));
    if (bookmarkURILiteral) {
      const PRUnichar* uURL = nsnull;
      bookmarkURILiteral->GetValueConst(&uURL);
      if (uURL) {
          rv = gRDF->GetUnicodeResource(uURL, getter_AddRefs(newResource));
          if (NS_FAILED(rv)) return rv;
      }
    }
  }

  if (!newResource) {
    // We're a folder, or some other type of anonymous resource.
    rv = BookmarkParser::CreateAnonymousResource(getter_AddRefs(newResource));
    if (NS_FAILED(rv)) return rv;
  }

  if (aItemType == kNC_Folder) {
    // Make Sequences for Folders.
    rv = gRDFC->MakeSeq(mInner, newResource, nsnull);
    if (NS_FAILED(rv)) return rv;
  }

  // Assert Name arc
  if (!itemName.IsEmpty()) {
    nsCOMPtr<nsIRDFLiteral> nameLiteral;
    rv = gRDF->GetLiteral(itemName.get(), getter_AddRefs(nameLiteral));
    if (NS_FAILED(rv)) return rv;
    rv = mInner->Assert(newResource, kNC_Name, nameLiteral, PR_TRUE);
    if (NS_FAILED(rv)) return rv;
  }

  // Assert type arc
  rv = mInner->Assert(newResource, kRDF_type, aItemType, PR_TRUE);
  if (NS_FAILED(rv)) return rv;

  // XXX - investigate asserting date as a resource with a raw date value for
  //       lookup, and a Name arc with a pretty display name, e.g. "Saturday"
  
  // Convert the current date/time from microseconds (PRTime) to seconds.
  nsCOMPtr<nsIRDFDate> dateLiteral;
  rv = gRDF->GetDateLiteral(PR_Now(), getter_AddRefs(dateLiteral));
  if (NS_FAILED(rv)) return rv;
  rv = mInner->Assert(newResource, kNC_BookmarkAddDate, dateLiteral, PR_TRUE);
  if (NS_FAILED(rv)) return rv;

  // Add to container. 
  rv = container->InsertElementAt(newResource, !relNodeIdx ? 1 : relNodeIdx + 1, PR_TRUE);
  
  return rv;
}



nsresult
nsBookmarksService::deleteBookmarkItem(nsIRDFResource *src, nsISupportsArray *aArguments, PRInt32 parentArgIndex, nsIRDFResource *objType)
{
	nsresult			rv;

	nsCOMPtr<nsIRDFNode>		aNode;
	if (NS_FAILED(rv = getArgumentN(aArguments, kNC_Parent,
			parentArgIndex, getter_AddRefs(aNode))))
		return(rv);
	nsCOMPtr<nsIRDFResource>	argParent = do_QueryInterface(aNode);
	if (!argParent)	return(NS_ERROR_NO_INTERFACE);

	// make sure its an object of the correct type (bookmark, folder, separator, ...)
	PRBool	isCorrectObjectType = PR_FALSE;
	if (NS_FAILED(rv = mInner->HasAssertion(src, kRDF_type,
			objType, PR_TRUE, &isCorrectObjectType)))
		return(rv);
	if (!isCorrectObjectType)	return(NS_ERROR_UNEXPECTED);

	nsCOMPtr<nsIRDFContainer>	container;
	if (NS_FAILED(rv = nsComponentManager::CreateInstance(kRDFContainerCID, nsnull,
			NS_GET_IID(nsIRDFContainer), getter_AddRefs(container))))
		return(rv);
	if (NS_FAILED(rv = container->Init(this, argParent)))
		return(rv);

	if (NS_FAILED(rv = container->RemoveElement(src, PR_TRUE)))
		return(rv);

	return(rv);
}



nsresult
nsBookmarksService::setFolderHint(nsIRDFResource *newSource, nsIRDFResource *objType)
{
	nsresult			rv;
	nsCOMPtr<nsISimpleEnumerator>	srcList;
	if (NS_FAILED(rv = GetSources(kNC_FolderType, objType, PR_TRUE, getter_AddRefs(srcList))))
		return(rv);

	PRBool	hasMoreSrcs = PR_TRUE;
	while(NS_SUCCEEDED(rv = srcList->HasMoreElements(&hasMoreSrcs))
		&& (hasMoreSrcs == PR_TRUE))
	{
		nsCOMPtr<nsISupports>	aSrc;
		if (NS_FAILED(rv = srcList->GetNext(getter_AddRefs(aSrc))))
			break;
		nsCOMPtr<nsIRDFResource>	aSource = do_QueryInterface(aSrc);
		if (!aSource)	continue;

        // if folder is already marked, nothing left to do
        if (aSource.get() == newSource)   return(NS_OK);

		if (NS_FAILED(rv = mInner->Unassert(aSource, kNC_FolderType, objType)))
			continue;
	}

	// if not setting a new Personal Toolbar Folder, just assert new type, and then done

	if (objType != kNC_PersonalToolbarFolder)
	{
		if (NS_SUCCEEDED(rv = mInner->Assert(newSource, kNC_FolderType, objType, PR_TRUE)))
		{
		}
		mDirty = PR_TRUE;
		return(rv);
	}

	// else if setting a new Personal Toolbar Folder, we need to work some magic!

	nsCOMPtr<nsIRDFResource>	newAnonURL;
	if (NS_FAILED(rv = BookmarkParser::CreateAnonymousResource(getter_AddRefs(newAnonURL))))
		return(rv);
	// Note: use our Change() method, not mInner->Change(), due to Bookmarks magical #URL handling
	rv = Change(kNC_PersonalToolbarFolder, kNC_URL, kNC_PersonalToolbarFolder, newAnonURL);


	// change newSource's URL to be kURINC_PersonalToolbarFolder
	const char	*newSourceURI = nsnull;
	if (NS_FAILED(rv = newSource->GetValueConst( &newSourceURI )))	return(rv);
	nsCOMPtr<nsIRDFLiteral>		newSourceLiteral;
	if (NS_FAILED(rv = gRDF->GetLiteral(NS_ConvertASCIItoUCS2(newSourceURI).get(),
			getter_AddRefs(newSourceLiteral))))	return(rv);

	// Note: use our Change() method, not mInner->Change(), due to Bookmarks magical #URL handling
	if (NS_FAILED(rv = Change(newSource, kNC_URL, newSourceLiteral, kNC_PersonalToolbarFolder)))
		return(rv);

	if (NS_FAILED(rv = mInner->Assert(kNC_PersonalToolbarFolder, kNC_FolderType, objType, PR_TRUE)))
		return(rv);

	mDirty = PR_TRUE;
	Flush();

	return(rv);
}



nsresult
nsBookmarksService::getFolderViaHint(nsIRDFResource *objType, PRBool fallbackFlag, nsIRDFResource **folder)
{
	if (!folder)	return(NS_ERROR_UNEXPECTED);
	*folder = nsnull;
	if (!objType)	return(NS_ERROR_UNEXPECTED);

	nsresult			rv;
	nsCOMPtr<nsIRDFResource>	oldSource;
	if (NS_FAILED(rv = mInner->GetSource(kNC_FolderType, objType, PR_TRUE, getter_AddRefs(oldSource))))
		return(rv);

	if ((rv != NS_RDF_NO_VALUE) && (oldSource))
	{
		const	char		*uri = nsnull;
		oldSource->GetValueConst(&uri);
		if (uri)
		{
			PRBool	isBookmarkedFlag = PR_FALSE;
			if (NS_SUCCEEDED(rv = IsBookmarked(uri, &isBookmarkedFlag)) &&
				(isBookmarkedFlag == PR_TRUE))
			{
				*folder = oldSource;
			}
		}
	}

	// if we couldn't find a real "New Internet Search Folder", fallback to looking for
	// a "New Bookmark Folder", and if can't find that, then default to the bookmarks root
	if ((!(*folder)) && (fallbackFlag == PR_TRUE) && (objType == kNC_NewSearchFolder))
	{
		rv = getFolderViaHint(kNC_NewBookmarkFolder, fallbackFlag, folder);
	}

	if (!(*folder))
	{
		// fallback to some well-known defaults
		if (objType == kNC_NewBookmarkFolder || objType == kNC_NewSearchFolder)
		{
			*folder = kNC_BookmarksRoot;
		}
		else if (objType == kNC_PersonalToolbarFolder)
		{
			*folder = kNC_PersonalToolbarFolder;
		}
	}

	NS_IF_ADDREF(*folder);

	return(NS_OK);
}



nsresult
nsBookmarksService::importBookmarks(nsISupportsArray *aArguments)
{
	// look for #URL which is the "file:///" URL to import
	nsresult		rv;
	nsCOMPtr<nsIRDFNode>	aNode;
	if (NS_FAILED(rv = getArgumentN(aArguments, kNC_URL, 0, getter_AddRefs(aNode))))
		return(rv);
	nsCOMPtr<nsIRDFLiteral>		pathLiteral = do_QueryInterface(aNode);
	if (!pathLiteral)	return(NS_ERROR_NO_INTERFACE);

	const PRUnichar		*pathUni = nsnull;
	pathLiteral->GetValueConst(&pathUni);
	if (!pathUni)	return(NS_ERROR_NULL_POINTER);

	nsAutoString		fileName(pathUni);
	nsFileURL		fileURL(fileName);
	nsFileSpec		fileSpec(fileURL);
	if (!fileSpec.IsFile())	return(NS_ERROR_UNEXPECTED);

	// figure out where to add the imported bookmarks
	nsCOMPtr<nsIRDFResource>	newBookmarkFolder;
	if (NS_FAILED(rv = getFolderViaHint(kNC_NewBookmarkFolder, PR_TRUE,
			getter_AddRefs(newBookmarkFolder))))
		return(rv);

	// read 'em in
	BookmarkParser		parser;
	parser.Init(&fileSpec, mInner, nsAutoString(), PR_TRUE);

	// Note: can't Begin|EndUpdateBatch() this as notifications are required
	parser.Parse(newBookmarkFolder, kNC_Bookmark);

	return(NS_OK);
}



nsresult
nsBookmarksService::exportBookmarks(nsISupportsArray *aArguments)
{
	// look for #URL which is the "file:///" URL to export
	nsresult		rv;
	nsCOMPtr<nsIRDFNode>	aNode;
	if (NS_FAILED(rv = getArgumentN(aArguments, kNC_URL, 0, getter_AddRefs(aNode))))
		return(rv);
	nsCOMPtr<nsIRDFLiteral>		pathLiteral = do_QueryInterface(aNode);
	if (!pathLiteral)	return(NS_ERROR_NO_INTERFACE);

	const PRUnichar		*pathUni = nsnull;
	pathLiteral->GetValueConst(&pathUni);
	if (!pathUni)	return(NS_ERROR_NULL_POINTER);

	nsAutoString		fileName(pathUni);
	nsFileURL		fileURL(fileName);
	nsFileSpec		fileSpec(fileURL);

	// write 'em out
	rv = WriteBookmarks(&fileSpec, mInner, kNC_BookmarksRoot);

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

	// Note: some commands only run once (instead of looping over selection);
	//       if that's the case, be sure to "break" (if success) so that "mDirty"
	//       is set (and "bookmarks.html" will be flushed out shortly afterwards)

	for (loop=((PRInt32)numSources)-1; loop>=0; loop--)
	{
		nsCOMPtr<nsISupports>	aSource = aSources->ElementAt(loop);
		if (!aSource)	return(NS_ERROR_NULL_POINTER);
		nsCOMPtr<nsIRDFResource>	src = do_QueryInterface(aSource);
		if (!src)	return(NS_ERROR_NO_INTERFACE);

		if (aCommand == kNC_BookmarkCommand_NewBookmark)
		{
			rv = insertBookmarkItem(src, aArguments, kNC_Bookmark);
			if (NS_FAILED(rv))	return(rv);
			break;
		}
		else if (aCommand == kNC_BookmarkCommand_NewFolder)
		{
			rv = insertBookmarkItem(src, aArguments, kNC_Folder);
			if (NS_FAILED(rv))	return(rv);
			break;
		}
		else if (aCommand == kNC_BookmarkCommand_NewSeparator)
		{
			rv = insertBookmarkItem(src, aArguments, kNC_BookmarkSeparator);
			if (NS_FAILED(rv))	return(rv);
			break;
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
		else if (aCommand == kNC_BookmarkCommand_SetNewBookmarkFolder)
		{
			rv = setFolderHint(src, kNC_NewBookmarkFolder);
			if (NS_FAILED(rv))	return(rv);
			break;
		}
		else if (aCommand == kNC_BookmarkCommand_SetPersonalToolbarFolder)
		{
			rv = setFolderHint(src, kNC_PersonalToolbarFolder);
			if (NS_FAILED(rv))	return(rv);
			break;
		}
		else if (aCommand == kNC_BookmarkCommand_SetNewSearchFolder)
		{
			rv = setFolderHint(src, kNC_NewSearchFolder);
			if (NS_FAILED(rv))	return(rv);
			break;
		}
		else if (aCommand == kNC_BookmarkCommand_Import)
		{
			rv = importBookmarks(aArguments);
			if (NS_FAILED(rv))	return(rv);
			break;
		}
		else if (aCommand == kNC_BookmarkCommand_Export)
		{
			rv = exportBookmarks(aArguments);
			if (NS_FAILED(rv))	return(rv);
			break;
		}
	}

	mDirty = PR_TRUE;

	return(NS_OK);
}

////////////////////////////////////////////////////////////////////////
// nsIRDFRemoteDataSource

NS_IMETHODIMP
nsBookmarksService::GetLoaded(PRBool* _result)
{
    *_result = PR_TRUE;
    return NS_OK;
}

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
	nsresult	rv = NS_OK;

	if (mBookmarksAvailable == PR_TRUE)
	{
		nsFileSpec	bookmarksFile;
		rv = GetBookmarksFile(&bookmarksFile);

		// Oh well, couldn't get the bookmarks file. Guess there
		// aren't any bookmarks for us to write out.
		if (NS_FAILED(rv))	return NS_OK;

		rv = WriteBookmarks(&bookmarksFile, mInner, kNC_BookmarksRoot);
	}
	return(rv);
}



////////////////////////////////////////////////////////////////////////
// Implementation methods

nsresult
nsBookmarksService::GetBookmarksFile(nsFileSpec* aResult)
{
    nsresult rv;

    // First we see if the user has set a pref for the location of the 
    // bookmarks file.
    nsCOMPtr<nsIPref> prefServ(do_GetService(kPrefCID, &rv));
    if (NS_SUCCEEDED(rv)) {
        nsXPIDLCString prefVal;
        rv = prefServ->CopyCharPref("browser.bookmarks.file",
                                    getter_Copies(prefVal));      
        if (NS_SUCCEEDED(rv)) {
            *aResult = prefVal;
        }
    }


    if (NS_FAILED(rv)) {
        // Otherwise, we look for bookmarks.html in the current profile
        // directory using the magic directory service.
        nsCOMPtr<nsIFile> bookmarksFile;
        rv = NS_GetSpecialDirectory(NS_APP_BOOKMARKS_50_FILE, 
                                    getter_AddRefs(bookmarksFile));
        if (NS_SUCCEEDED(rv)) {

            // XXX: When the code which calls this can us nsIFile
            // or nsILocalFile, this conversion from nsIFile to
            // nsFileSpec can go away. Bug 36974.

            nsXPIDLCString pathBuf;
            rv = bookmarksFile->GetPath(getter_Copies(pathBuf));
            if (NS_SUCCEEDED(rv))
                *aResult = pathBuf.get();
        }
    }

#ifdef DEBUG
	if (NS_FAILED(rv)) {
		*aResult = nsSpecialSystemDirectory(nsSpecialSystemDirectory::Moz_BinDirectory); // Use Moz_BinDirectory, NOT CurrentProcess
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



#ifdef	XP_MAC

nsresult
nsBookmarksService::ReadFavorites()
{
	mIEFavoritesAvailable = PR_TRUE;
			
#ifdef	DEBUG
	PRTime		now;
	Microseconds((UnsignedWide *)&now);
	printf("Start reading in IE Favorites.html\n");
#endif

	// look for and import any IE Favorites
	nsAutoString	ieTitle;
	getLocaleString("ImportedIEFavorites", ieTitle);

	nsSpecialSystemDirectory ieFavoritesFile(nsSpecialSystemDirectory::Mac_PreferencesDirectory);
	ieFavoritesFile += "Explorer";
	ieFavoritesFile += "Favorites.html";

	nsresult	rv;
	if (NS_SUCCEEDED(rv = gRDFC->MakeSeq(mInner, kNC_IEFavoritesRoot, nsnull)))
	{
		BookmarkParser parser;
		parser.Init(&ieFavoritesFile, mInner, nsAutoString());
        BeginUpdateBatch(this);
		parser.Parse(kNC_IEFavoritesRoot, kNC_IEFavorite);
		EndUpdateBatch(this);
			
		nsCOMPtr<nsIRDFLiteral>	ieTitleLiteral;
		rv = gRDF->GetLiteral(ieTitle.get(), getter_AddRefs(ieTitleLiteral));
		if (NS_SUCCEEDED(rv) && ieTitleLiteral)
		{
			rv = mInner->Assert(kNC_IEFavoritesRoot, kNC_Name, ieTitleLiteral, PR_TRUE);
		}
	}
#ifdef	DEBUG
	PRTime		now2;
	Microseconds((UnsignedWide *)&now2);
	PRUint64	loadTime64;
	LL_SUB(loadTime64, now2, now);
	PRUint32	loadTime32;
	LL_L2UI(loadTime32, loadTime64);
	printf("Finished reading in IE Favorites.html  (%u microseconds)\n", loadTime32);
#endif
	return(rv);
}

#endif



NS_IMETHODIMP
nsBookmarksService::ReadBookmarks()
{
	nsresult	rv;

	// the profile manager might call Readbookmarks() in certain circumstances
	// so we need to forget about any previous bookmarks
	mInner = nsnull;
	if (NS_FAILED(rv = nsComponentManager::CreateInstance(kRDFInMemoryDataSourceCID,
				nsnull, NS_GET_IID(nsIRDFDataSource), (void**) &mInner)))
		return(rv);

	rv = mInner->AddObserver(this);
	if (NS_FAILED(rv)) return rv;

	nsFileSpec	bookmarksFile;
	rv = GetBookmarksFile(&bookmarksFile);

	// Oh well, couldn't get the bookmarks file. Guess there
	// aren't any bookmarks to read in.
	if (NS_FAILED(rv))
	    return NS_OK;

	rv = gRDFC->MakeSeq(mInner, kNC_BookmarksRoot, nsnull);
	NS_ASSERTION(NS_SUCCEEDED(rv), "Unable to make NC:BookmarksRoot a sequence");
	if (NS_FAILED(rv)) return rv;

	// Make sure bookmark's root has the correct type
	rv = mInner->Assert(kNC_BookmarksRoot, kRDF_type, kNC_Folder, PR_TRUE);
	if (NS_FAILED(rv)) return rv;

	PRBool	foundIERoot = PR_FALSE;

#ifdef	DEBUG
	PRTime		now;
#ifdef	XP_MAC
	Microseconds((UnsignedWide *)&now);
#else
	now = PR_Now();
#endif
	printf("Start reading in bookmarks.html\n");
#endif

#ifdef	XP_WIN
	nsCOMPtr<nsIRDFResource>	ieFolder;
	char				*ieFavoritesURL = nsnull;
#endif
#ifdef	XP_BEOS
	nsCOMPtr<nsIRDFResource>	netPositiveFolder;
	char				*netPositiveURL = nsnull;
#endif

	{ // <-- scope the stream to get the open/close automatically.
		BookmarkParser parser;
		parser.Init(&bookmarksFile, mInner, mPersonalToolbarName);

#ifdef	XP_MAC
		parser.SetIEFavoritesRoot(nsCString(kURINC_IEFavoritesRoot));
#endif

#ifdef	XP_WIN
		nsSpecialSystemDirectory	ieFavoritesFile(nsSpecialSystemDirectory::Win_Favorites);
		nsFileURL ieFavoritesURLSpec(ieFavoritesFile);
		const char* favoritesURL = ieFavoritesURLSpec.GetAsString();
		if (favoritesURL)
			ieFavoritesURL = strdup(favoritesURL);
		parser.SetIEFavoritesRoot(nsCString(ieFavoritesURL));
#endif

#ifdef	XP_BEOS
		nsSpecialSystemDirectory	netPositiveFile(nsSpecialSystemDirectory::BeOS_SettingsDirectory);
		nsFileURL			netPositiveURLSpec(netPositiveFile);

		// XXX Currently hard-coded; does the BeOS have anything like a
		// system registry which we could use to get this instead?
		netPositiveURLSpec += "NetPositive/Bookmarks/";

		const char			*constNetPositiveURL = netPositiveURLSpec.GetAsString();
		if (constNetPositiveURL)
			netPositiveURL = strdup(constNetPositiveURL);
		nsCString cstringNetPositiveURL=nsCString(netPositiveURL);
		parser.SetIEFavoritesRoot(cstringNetPositiveURL);
#endif

        BeginUpdateBatch(this);
		parser.Parse(kNC_BookmarksRoot, kNC_Bookmark);
        EndUpdateBatch(this);
		mBookmarksAvailable = PR_TRUE;
		
		PRBool	foundPTFolder = PR_FALSE;
		parser.ParserFoundPersonalToolbarFolder(&foundPTFolder);
		// try to ensure that we end up with a personal toolbar folder
		if ((foundPTFolder == PR_FALSE) && (mPersonalToolbarName.Length() > 0))
		{
			nsCOMPtr<nsIRDFLiteral>	ptNameLiteral;
			if (NS_SUCCEEDED(rv = gRDF->GetLiteral(mPersonalToolbarName.get(),
				getter_AddRefs(ptNameLiteral))))
			{
				nsCOMPtr<nsIRDFResource>	ptSource;
				if (NS_FAILED(rv = mInner->GetSource(kNC_Name, ptNameLiteral,
						PR_TRUE, getter_AddRefs(ptSource))))
					return(rv);
				if ((rv != NS_RDF_NO_VALUE) && (ptSource))
				{
					setFolderHint(ptSource, kNC_PersonalToolbarFolder);
				}
			}
		}

		parser.ParserFoundIEFavoritesRoot(&foundIERoot);
	} // <-- scope the stream to get the open/close automatically.
	
	// Look for and import any IE favorites. Only do this if the pref 
  // |browser.bookmarks.import_system_favorites| is set. 
  // Se bug 22642 for details. 
  PRBool useSystemBookmarks = PR_TRUE;
  nsCOMPtr<nsIPref> prefSvc(do_GetService("@mozilla.org/preferences;1"));
  if (prefSvc)
      prefSvc->GetBoolPref("browser.bookmarks.import_system_favorites", &useSystemBookmarks);
  
  nsAutoString	ieTitle;
	getLocaleString("ImportedIEFavorites", ieTitle);

#ifdef	XP_BEOS
	nsAutoString	netPositiveTitle;
	getLocaleString("ImportedNetPositiveBookmarks", netPositiveTitle);
#endif

#ifdef	XP_MAC
	// if the IE Favorites root isn't somewhere in bookmarks.html, add it
	if (useSystemBookmarks && !foundIERoot)
	{
		nsCOMPtr<nsIRDFContainer> bookmarksRoot;
		rv = nsComponentManager::CreateInstance(kRDFContainerCID,
							nsnull,
							NS_GET_IID(nsIRDFContainer),
							getter_AddRefs(bookmarksRoot));
		if (NS_FAILED(rv)) return rv;

		rv = bookmarksRoot->Init(this, kNC_BookmarksRoot);
		if (NS_FAILED(rv)) return rv;

		rv = bookmarksRoot->AppendElement(kNC_IEFavoritesRoot);
		if (NS_FAILED(rv)) return rv;

		// make sure IE Favorites root folder has the proper type		
		rv = mInner->Assert(kNC_IEFavoritesRoot, kRDF_type, kNC_IEFavoriteFolder, PR_TRUE);
		if (NS_FAILED(rv)) return rv;
	}
#endif

#ifdef	XP_WIN
	rv = gRDF->GetResource(ieFavoritesURL, getter_AddRefs(ieFolder));
	if (NS_SUCCEEDED(rv))
	{
		nsCOMPtr<nsIRDFLiteral>	ieTitleLiteral;
		rv = gRDF->GetLiteral(ieTitle.get(), getter_AddRefs(ieTitleLiteral));
		if (NS_SUCCEEDED(rv) && ieTitleLiteral)
		{
			rv = mInner->Assert(ieFolder, kNC_Name, ieTitleLiteral, PR_TRUE);
		}

		// if the IE Favorites root isn't somewhere in bookmarks.html, add it
		if (useSystemBookmarks && !foundIERoot)
		{
			nsCOMPtr<nsIRDFContainer> container;
			rv = nsComponentManager::CreateInstance(kRDFContainerCID,
								nsnull,
								NS_GET_IID(nsIRDFContainer),
								getter_AddRefs(container));
			if (NS_FAILED(rv)) return rv;

			rv = container->Init(this, kNC_BookmarksRoot);
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
		rv = gRDF->GetLiteral(netPositiveTitle.get(), getter_AddRefs(netPositiveTitleLiteral));
		if (NS_SUCCEEDED(rv) && netPositiveTitleLiteral)
		{
			rv = mInner->Assert(netPositiveFolder, kNC_Name, netPositiveTitleLiteral, PR_TRUE);
		}

		// if the Favorites root isn't somewhere in bookmarks.html, add it
		if (useSystemBookmarks && !foundIERoot)
		{
			nsCOMPtr<nsIRDFContainer> container;
			rv = nsComponentManager::CreateInstance(kRDFContainerCID,
								nsnull,
								NS_GET_IID(nsIRDFContainer),
								getter_AddRefs(container));
			if (NS_FAILED(rv)) return rv;

			rv = container->Init(this, kNC_BookmarksRoot);
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

#ifdef	DEBUG
	PRTime		now2;
#ifdef	XP_MAC
	Microseconds((UnsignedWide *)&now2);
#else
	now2 = PR_Now();
#endif
	PRUint64	loadTime64;
	LL_SUB(loadTime64, now2, now);
	PRUint32	loadTime32;
	LL_L2UI(loadTime32, loadTime64);
	printf("Finished reading in bookmarks.html  (%u microseconds)\n", loadTime32);
#endif

	return(NS_OK);
}



nsresult
nsBookmarksService::WriteBookmarks(nsFileSpec *bookmarksFile, nsIRDFDataSource *ds,
				   nsIRDFResource *root)
{
	if (!bookmarksFile)	return(NS_ERROR_NULL_POINTER);
	if (!ds)		return(NS_ERROR_NULL_POINTER);
	if (!root)		return(NS_ERROR_NULL_POINTER);

	nsresult			rv;
	nsCOMPtr<nsISupportsArray>	parentArray;
	if (NS_FAILED(rv = NS_NewISupportsArray(getter_AddRefs(parentArray))))
		return(rv);

	rv = NS_ERROR_FAILURE;
	nsOutputFileStream	strm(*bookmarksFile);
	if (strm.is_open())
	{
		strm << "<!DOCTYPE NETSCAPE-Bookmark-file-1>\n";
		strm << "<!-- This is an automatically generated file.\n";
		strm << "It will be read and overwritten.\n";
		strm << "Do Not Edit! -->\n";

		// Note: we write out bookmarks in UTF-8
		strm << "<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=UTF-8\">\n";

		strm << "<TITLE>Bookmarks</TITLE>\n";
		strm << "<H1>Bookmarks</H1>\n\n";

		rv = WriteBookmarksContainer(ds, strm, root, 0, parentArray);
		mDirty = PR_FALSE;
	}
	return(rv);
}



nsresult
nsBookmarksService::WriteBookmarksContainer(nsIRDFDataSource *ds, nsOutputFileStream& strm,
			nsIRDFResource *parent, PRInt32 level, nsISupportsArray *parentArray)
{
	nsresult	rv = NS_OK;

	nsCOMPtr<nsIRDFContainer> container;
	rv = nsComponentManager::CreateInstance(kRDFContainerCID, nsnull,
		NS_GET_IID(nsIRDFContainer), getter_AddRefs(container));
	if (NS_FAILED(rv)) return rv;

	nsAutoString	indentationString;
	  // STRING USE WARNING: converting in a loop.  Probably not a good idea
	for (PRInt32 loop=0; loop<level; loop++)	indentationString.AppendWithConversion("    ");
	char		*indentation = ToNewCString(indentationString);
	if (nsnull == indentation)	return(NS_ERROR_OUT_OF_MEMORY);

	strm << indentation;
	strm << "<DL><p>\n";

	rv = container->Init(ds, parent);
	if (NS_SUCCEEDED(rv) && (parentArray->IndexOf(parent) < 0))
	{
		// Note: once we've added something into the parentArray, don't "return" out
		//       of this function without removing it from the parentArray!
		parentArray->InsertElementAt(parent, 0);

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
				nsAutoString		nameString;
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
							name = ToNewUTF8String(nameString);
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

					// output various special folder hints
					PRBool	hasType = PR_FALSE;
					if (NS_SUCCEEDED(rv = mInner->HasAssertion(child, kNC_FolderType, kNC_NewBookmarkFolder,
						PR_TRUE, &hasType)) && (hasType == PR_TRUE))
					{
						strm << " " << kNewBookmarkFolderEquals << "true\"";
					}
					if (NS_SUCCEEDED(rv = mInner->HasAssertion(child, kNC_FolderType, kNC_NewSearchFolder,
						PR_TRUE, &hasType)) && (hasType == PR_TRUE))
					{
						strm << " " << kNewSearchFolderEquals << "true\"";
					}
					if (NS_SUCCEEDED(rv = mInner->HasAssertion(child, kNC_FolderType, kNC_PersonalToolbarFolder,
						PR_TRUE, &hasType)) && (hasType == PR_TRUE))
					{
						strm << " " << kPersonalToolbarFolderEquals << "true\"";
					}

					// output ID
					const char	*id = nsnull;
					rv = child->GetValueConst(&id);
					if (NS_SUCCEEDED(rv) && (id))
					{
						strm << " " << kIDEquals << (const char *) id << "\"";
					}

					strm << ">";

					// output title
					if (name)	strm << name;
					strm << "</H3>\n";

					// output description (if one exists)
					WriteBookmarkProperties(ds, strm, child, kNC_Description, kOpenDD, PR_TRUE);

					rv = WriteBookmarksContainer(ds, strm, child, level+1, parentArray);
				}
				else
				{
					const char	*url = nsnull;
					if (NS_SUCCEEDED(rv = child->GetValueConst(&url)) && (url))
					{
						nsCAutoString	uri(url);

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
							// output URL
							strm << "<DT><A HREF=\"";

							// Now do properly replace %22's; this is particularly important for javascript: URLs
							static const char kEscape22[] = "%22";
							PRInt32 offset;
							while ((offset = uri.FindChar('\"')) >= 0)
							{
								uri.Cut(offset, 1);
								uri.Insert(kEscape22, offset);
							}

							strm << uri.get();
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

              // output LAST_CHARSET
              WriteBookmarkProperties(ds, strm, child, kWEB_LastCharset, kLastCharsetEquals, PR_FALSE);

							// output PING_CONTENT_LEN
							WriteBookmarkProperties(ds, strm, child, kWEB_LastPingContentLen, kPingContentLenEquals, PR_FALSE);

							// output PING_STATUS
							WriteBookmarkProperties(ds, strm, child, kWEB_Status, kPingStatusEquals, PR_FALSE);

							strm << ">";
							// output title
							if (name)
							{
								// Note: we escape the title due to security issues;
								//       see bug # 13197 for details
								char *escapedAttrib = nsEscapeHTML(name);
								if (escapedAttrib)
								{
									strm << escapedAttrib;
									nsCRT::free(escapedAttrib);
									escapedAttrib = nsnull;
								}
							}
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
		}

		// cleanup: remove current parent element from parentArray
		parentArray->RemoveElementAt(0);
	}

	strm << indentation;
	strm << "</DL><p>\n";

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
    else if (NS_SUCCEEDED(rv = aNode->QueryInterface(NS_GET_IID(nsIRDFResource), (void**) &resource))) {
    	const char	*p = nsnull;
        if (NS_SUCCEEDED(rv = resource->GetValueConst( &p )) && (p)) {
            aResult.AssignWithConversion(p);
        }
        NS_RELEASE(resource);
    }
    else if (NS_SUCCEEDED(rv = aNode->QueryInterface(NS_GET_IID(nsIRDFDate), (void**) &dateLiteral))) {
	PRInt64		theDate, million;
        if (NS_SUCCEEDED(rv = dateLiteral->GetValue( &theDate ))) {
		LL_I2L(million, PR_USEC_PER_SEC);
		LL_DIV(theDate, theDate, million);			// convert from microseconds (PRTime) to seconds
		PRInt32		now32;
		LL_L2I(now32, theDate);
		aResult.Truncate();
        	aResult.AppendInt(now32, 10);
        }
        NS_RELEASE(dateLiteral);
    }
    else if (NS_SUCCEEDED(rv = aNode->QueryInterface(NS_GET_IID(nsIRDFInt), (void**) &intLiteral))) {
	PRInt32		theInt;
	aResult.Truncate();
        if (NS_SUCCEEDED(rv = intLiteral->GetValue( &theInt ))) {
        	aResult.AppendInt(theInt, 10);
        }
        NS_RELEASE(intLiteral);
    }
    else if (NS_SUCCEEDED(rv = aNode->QueryInterface(NS_GET_IID(nsIRDFLiteral), (void**) &literal))) {
	const PRUnichar		*p = nsnull;
        if (NS_SUCCEEDED(rv = literal->GetValueConst( &p )) && (p)) {
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
nsBookmarksService::WriteBookmarkProperties(nsIRDFDataSource *ds, nsOutputFileStream& strm,
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
			char		*attribute = ToNewUTF8String(literalString);
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
	nsresult	rv;
	PRBool		canAcceptFlag = PR_FALSE, isOrdinal;

	if (NS_SUCCEEDED(rv = gRDFC->IsOrdinalProperty(aProperty, &isOrdinal)))
	{
		if (isOrdinal == PR_TRUE)
		{
			canAcceptFlag = PR_TRUE;
		}
		else if ((aProperty == kNC_Description) ||
			 (aProperty == kNC_Name) ||
			 (aProperty == kNC_ShortcutURL) ||
			 (aProperty == kNC_URL) ||
			 (aProperty == kWEB_LastModifiedDate) ||
			 (aProperty == kWEB_LastVisitDate) ||
			 (aProperty == kNC_BookmarkAddDate) ||
			 (aProperty == kRDF_nextVal) ||
			 (aProperty == kRDF_type) ||
			 (aProperty == kWEB_Schedule))
		{
			canAcceptFlag = PR_TRUE;
		}
	}
	return(canAcceptFlag);
}


//----------------------------------------------------------------------
//
// nsIRDFObserver interface
//

NS_IMETHODIMP
nsBookmarksService::OnAssert(nsIRDFDataSource* aDataSource,
			     nsIRDFResource* aSource,
			     nsIRDFResource* aProperty,
			     nsIRDFNode* aTarget)
{
    if (mUpdateBatchNest != 0)  return(NS_OK);

	if (mObservers) {
		nsresult rv;

		PRUint32 count;
		rv = mObservers->Count(&count);
		if (NS_FAILED(rv)) return rv;

		for (PRInt32 i = 0; i < PRInt32(count); ++i) {
			nsIRDFObserver* obs =
				NS_REINTERPRET_CAST(nsIRDFObserver*, mObservers->ElementAt(i));

			(void) obs->OnAssert(this, aSource, aProperty, aTarget);
			NS_RELEASE(obs);
		}
	}

	return NS_OK;
}


NS_IMETHODIMP
nsBookmarksService::OnUnassert(nsIRDFDataSource* aDataSource,
			       nsIRDFResource* aSource,
			       nsIRDFResource* aProperty,
			       nsIRDFNode* aTarget)
{
    if (mUpdateBatchNest != 0)  return(NS_OK);

	if (mObservers) {
		nsresult rv;

		PRUint32 count;
		rv = mObservers->Count(&count);
		if (NS_FAILED(rv)) return rv;

		for (PRInt32 i = 0; i < PRInt32(count); ++i) {
			nsIRDFObserver* obs =
				NS_REINTERPRET_CAST(nsIRDFObserver*, mObservers->ElementAt(i));

			(void) obs->OnUnassert(this, aSource, aProperty, aTarget);
			NS_RELEASE(obs);
		}
	}

	return NS_OK;
}

NS_IMETHODIMP
nsBookmarksService::OnChange(nsIRDFDataSource* aDataSource,
			     nsIRDFResource* aSource,
			     nsIRDFResource* aProperty,
			     nsIRDFNode* aOldTarget,
			     nsIRDFNode* aNewTarget)
{
    if (mUpdateBatchNest != 0)  return(NS_OK);

	if (mObservers) {
		nsresult rv;

		PRUint32 count;
		rv = mObservers->Count(&count);
		if (NS_FAILED(rv)) return rv;

		for (PRInt32 i = 0; i < PRInt32(count); ++i) {
			nsIRDFObserver* obs =
				NS_REINTERPRET_CAST(nsIRDFObserver*, mObservers->ElementAt(i));

			(void) obs->OnChange(this, aSource, aProperty, aOldTarget, aNewTarget);
			NS_RELEASE(obs);
		}
	}

	return NS_OK;
}

NS_IMETHODIMP
nsBookmarksService::OnMove(nsIRDFDataSource* aDataSource,
			   nsIRDFResource* aOldSource,
			   nsIRDFResource* aNewSource,
			   nsIRDFResource* aProperty,
			   nsIRDFNode* aTarget)
{
    if (mUpdateBatchNest != 0)  return(NS_OK);

	if (mObservers) {
		nsresult rv;

		PRUint32 count;
		rv = mObservers->Count(&count);
		if (NS_FAILED(rv)) return rv;

		for (PRInt32 i = 0; i < PRInt32(count); ++i) {
			nsIRDFObserver* obs =
				NS_REINTERPRET_CAST(nsIRDFObserver*, mObservers->ElementAt(i));

			(void) obs->OnMove(this, aOldSource, aNewSource, aProperty, aTarget);
			NS_RELEASE(obs);
		}
	}

	return NS_OK;
}

NS_IMETHODIMP
nsBookmarksService::BeginUpdateBatch(nsIRDFDataSource* aDataSource)
{
	if ((mUpdateBatchNest++ == 0) && mObservers)
	{
		nsresult rv;

		PRUint32 count;
		rv = mObservers->Count(&count);
		if (NS_FAILED(rv)) return rv;

		for (PRInt32 i = 0; i < PRInt32(count); ++i) {
			nsIRDFObserver* obs =
				NS_REINTERPRET_CAST(nsIRDFObserver*, mObservers->ElementAt(i));

			(void) obs->BeginUpdateBatch(aDataSource);
			NS_RELEASE(obs);
		}
	}

	return NS_OK;
}

NS_IMETHODIMP
nsBookmarksService::EndUpdateBatch(nsIRDFDataSource* aDataSource)
{
    if (mUpdateBatchNest > 0)
    {
        --mUpdateBatchNest;
    }

	if ((mUpdateBatchNest == 0) && mObservers)
	{
		nsresult rv;

		PRUint32 count;
		rv = mObservers->Count(&count);
		if (NS_FAILED(rv)) return rv;

		for (PRInt32 i = 0; i < PRInt32(count); ++i) {
			nsIRDFObserver* obs =
				NS_REINTERPRET_CAST(nsIRDFObserver*, mObservers->ElementAt(i));

			(void) obs->EndUpdateBatch(aDataSource);
			NS_RELEASE(obs);
		}
	}

	return NS_OK;
}
