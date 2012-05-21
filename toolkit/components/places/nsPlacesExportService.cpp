/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Importer/exporter between the mozStorage-based bookmarks and the old-style
 * "bookmarks.html"
 *
 * Format:
 *
 * Primary heading := h1
 *   Old version used this to set attributes on the bookmarks RDF root, such
 *   as the last modified date. We only use H1 to check for the attribute
 *   PLACES_ROOT, which tells us that this hierarchy root is the places root.
 *   For backwards compatibility, if we don't find this, we assume that the
 *   hierarchy is rooted at the bookmarks menu.
 * Heading := any heading other than h1
 *   Old version used this to set attributes on the current container. We only
 *   care about the content of the heading container, which contains the title
 *   of the bookmark container.
 * Bookmark := a
 *   HREF is the destination of the bookmark
 *   FEEDURL is the URI of the RSS feed if this is a livemark.
 *   LAST_CHARSET is stored as an annotation so that the next time we go to
 *     that page we remember the user's preference.
 *   WEB_PANEL is set to "true" if the bookmark should be loaded in the sidebar.
 *   ICON will be stored in the favicon service
 *   ICON_URI is new for places bookmarks.html, it refers to the original
 *     URI of the favicon so we don't have to make up favicon URLs.
 *   Text of the <a> container is the name of the bookmark
 *   Ignored: LAST_VISIT, ID (writing out non-RDF IDs can confuse Firefox 2)
 * Bookmark comment := dd
 *   This affects the previosly added bookmark
 * Separator := hr
 *   Insert a separator into the current container
 * The folder hierarchy is defined by <dl>/<ul>/<menu> (the old importing code
 *     handles all these cases, when we write, use <dl>).
 *
 * Overall design
 * --------------
 *
 * We need to emulate a recursive parser. A "Bookmark import frame" is created
 * corresponding to each folder we encounter. These are arranged in a stack,
 * and contain all the state we need to keep track of.
 *
 * A frame is created when we find a heading, which defines a new container.
 * The frame also keeps track of the nesting of <DL>s, (in well-formed
 * bookmarks files, these will have a 1-1 correspondence with frames, but we
 * try to be a little more flexible here). When the nesting count decreases
 * to 0, then we know a frame is complete and to pop back to the previous
 * frame.
 *
 * Note that a lot of things happen when tags are CLOSED because we need to
 * get the text from the content of the tag. For example, link and heading tags
 * both require the content (= title) before actually creating it.
 */

#include "nsPlacesExportService.h"
#include "nsNetUtil.h"
#include "nsParserCIID.h"
#include "nsUnicharUtils.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsToolkitCompsCID.h"
#include "nsIParser.h"
#include "prprf.h"
#include "nsIObserverService.h"
#include "nsISupportsPrimitives.h"
#include "nsPlacesMacros.h"
#include "mozilla/Util.h"

using namespace mozilla;

#define LOAD_IN_SIDEBAR_ANNO NS_LITERAL_CSTRING("bookmarkProperties/loadInSidebar")
#define DESCRIPTION_ANNO NS_LITERAL_CSTRING("bookmarkProperties/description")
#define POST_DATA_ANNO NS_LITERAL_CSTRING("bookmarkProperties/POSTData")

#define LMANNO_FEEDURI "livemark/feedURI"
#define LMANNO_SITEURI "livemark/siteURI"

// define to get debugging messages on console about import/export
//#define DEBUG_EXPORT

#if defined(XP_WIN) || defined(XP_OS2)
#define NS_LINEBREAK "\015\012"
#else
#define NS_LINEBREAK "\012"
#endif

class nsIOutputStream;
static const char kWhitespace[] = " \r\n\t\b";
static nsresult WriteEscapedUrl(const nsCString &aString, nsIOutputStream* aOutput);

/**
 * Copied from nsEscape.cpp, which requires internal string API.
 */
char*
nsEscapeHTML(const char* string)
{
  /* XXX Hardcoded max entity len. The +1 is for the trailing null. */
  char* escaped = nsnull;
  PRUint32 len = strlen(string);
  if (len >= (PR_UINT32_MAX / 6))
    return nsnull;

  escaped = (char*)NS_Alloc((len * 6) + 1);
  if (escaped) {
    char* ptr = escaped;
    for (; *string != '\0'; string++) {
      switch(*string) {
        case '<':
          *ptr++ = '&';
          *ptr++ = 'l';
          *ptr++ = 't';
          *ptr++ = ';';
          break;
        case '>':
          *ptr++ = '&';
          *ptr++ = 'g';
          *ptr++ = 't';
          *ptr++ = ';';
          break;
        case '&':
          *ptr++ = '&';
          *ptr++ = 'a';
          *ptr++ = 'm';
          *ptr++ = 'p';
          *ptr++ = ';';
          break;
        case '"':
          *ptr++ = '&';
          *ptr++ = 'q';
          *ptr++ = 'u';
          *ptr++ = 'o';
          *ptr++ = 't';
          *ptr++ = ';';
          break;
        case '\'':
          *ptr++ = '&';
          *ptr++ = '#';
          *ptr++ = '3';
          *ptr++ = '9';
          *ptr++ = ';';
          break;
        default:
          *ptr++ = *string;
      }
    }
    *ptr = '\0';
  }
  return escaped;
}

PLACES_FACTORY_SINGLETON_IMPLEMENTATION(nsPlacesExportService, gExportService)

NS_IMPL_ISUPPORTS1(nsPlacesExportService, nsIPlacesImportExportService)

nsPlacesExportService::nsPlacesExportService()
{
  NS_ASSERTION(!gExportService,
               "Attempting to create two instances of the service!");
  gExportService = this;
}

nsPlacesExportService::~nsPlacesExportService()
{
  NS_ASSERTION(gExportService == this,
               "Deleting a non-singleton instance of the service");
  if (gExportService == this)
    gExportService = nsnull;
}

nsresult
nsPlacesExportService::Init()
{
  // Be sure to call EnsureServiceState() before using services.
  mHistoryService = do_GetService(NS_NAVHISTORYSERVICE_CONTRACTID);
  NS_ENSURE_TRUE(mHistoryService, NS_ERROR_OUT_OF_MEMORY);
  mFaviconService = do_GetService(NS_FAVICONSERVICE_CONTRACTID);
  NS_ENSURE_TRUE(mFaviconService, NS_ERROR_OUT_OF_MEMORY);
  mAnnotationService = do_GetService(NS_ANNOTATIONSERVICE_CONTRACTID);
  NS_ENSURE_TRUE(mAnnotationService, NS_ERROR_OUT_OF_MEMORY);
  mBookmarksService = do_GetService(NS_NAVBOOKMARKSSERVICE_CONTRACTID);
  NS_ENSURE_TRUE(mBookmarksService, NS_ERROR_OUT_OF_MEMORY);
  mLivemarkService = do_GetService(NS_LIVEMARKSERVICE_CONTRACTID);
  NS_ENSURE_TRUE(mLivemarkService, NS_ERROR_OUT_OF_MEMORY);
  return NS_OK;
}

// SyncChannelStatus
//
//    If a function returns an error, we need to set the channel status to be
//    the same, but only if the channel doesn't have its own error. This returns
//    the error code that should be sent to OnStopRequest.
static nsresult
SyncChannelStatus(nsIChannel* channel, nsresult status)
{
  nsresult channelStatus;
  channel->GetStatus(&channelStatus);
  if (NS_FAILED(channelStatus))
    return channelStatus;

  if (NS_SUCCEEDED(status))
    return NS_OK; // caller and the channel are happy

  // channel was OK, but caller wasn't: set the channel state
  channel->Cancel(status);
  return status;
}


static char kFileIntro[] =
    "<!DOCTYPE NETSCAPE-Bookmark-file-1>" NS_LINEBREAK
    // Note: we write bookmarks in UTF-8
    "<!-- This is an automatically generated file." NS_LINEBREAK
    "     It will be read and overwritten." NS_LINEBREAK
    "     DO NOT EDIT! -->" NS_LINEBREAK
    "<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=UTF-8\">" NS_LINEBREAK
    "<TITLE>Bookmarks</TITLE>" NS_LINEBREAK;
static const char kRootIntro[] = "<H1";
static const char kCloseRootH1[] = "</H1>" NS_LINEBREAK NS_LINEBREAK;

static const char kBookmarkIntro[] = "<DL><p>" NS_LINEBREAK;
static const char kBookmarkClose[] = "</DL><p>" NS_LINEBREAK;
static const char kContainerIntro[] = "<DT><H3";
static const char kContainerClose[] = "</H3>" NS_LINEBREAK;
static const char kItemOpen[] = "<DT><A";
static const char kItemClose[] = "</A>" NS_LINEBREAK;
static const char kSeparator[] = "<HR";
static const char kQuoteStr[] = "\"";
static const char kCloseAngle[] = ">";
static const char kIndent[] = "    ";
static const char kDescriptionIntro[] = "<DD>";
static const char kDescriptionClose[] = NS_LINEBREAK;

static const char kPlacesRootAttribute[] = " PLACES_ROOT=\"true\"";
static const char kBookmarksRootAttribute[] = " BOOKMARKS_MENU=\"true\"";
static const char kToolbarFolderAttribute[] = " PERSONAL_TOOLBAR_FOLDER=\"true\"";
static const char kUnfiledBookmarksFolderAttribute[] = " UNFILED_BOOKMARKS_FOLDER=\"true\"";
static const char kIconAttribute[] = " ICON=\"";
static const char kIconURIAttribute[] = " ICON_URI=\"";
static const char kHrefAttribute[] = " HREF=\"";
static const char kFeedURIAttribute[] = " FEEDURL=\"";
static const char kWebPanelAttribute[] = " WEB_PANEL=\"true\"";
static const char kKeywordAttribute[] = " SHORTCUTURL=\"";
static const char kPostDataAttribute[] = " POST_DATA=\"";
static const char kNameAttribute[] = " NAME=\"";
static const char kMicsumGenURIAttribute[]    = " MICSUM_GEN_URI=\"";
static const char kDateAddedAttribute[] = " ADD_DATE=\"";
static const char kLastModifiedAttribute[] = " LAST_MODIFIED=\"";
static const char kLastCharsetAttribute[] = " LAST_CHARSET=\"";


// WriteContainerPrologue
//
//    <DL><p>
//
//    Goes after the container header (<H3...) but before the contents
static nsresult
WriteContainerPrologue(const nsACString& aIndent, nsIOutputStream* aOutput)
{
  PRUint32 dummy;
  nsresult rv = aOutput->Write(PromiseFlatCString(aIndent).get(), aIndent.Length(), &dummy);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aOutput->Write(kBookmarkIntro, sizeof(kBookmarkIntro)-1, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}


// WriteContainerEpilogue
//
//    </DL><p>
//
//    Goes after the container contents to close the container
static nsresult
WriteContainerEpilogue(const nsACString& aIndent, nsIOutputStream* aOutput)
{
  PRUint32 dummy;
  nsresult rv = aOutput->Write(PromiseFlatCString(aIndent).get(), aIndent.Length(), &dummy);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aOutput->Write(kBookmarkClose, sizeof(kBookmarkClose)-1, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}


// WriteFaviconAttribute
//
//    This writes the 'ICON="data:asdlfkjas;ldkfja;skdljfasdf"' attribute for
//    an item. We special-case chrome favicon URIs by just writing the chrome:
//    URI.
static nsresult
WriteFaviconAttribute(const nsACString& aURI, nsIOutputStream* aOutput)
{
  PRUint32 dummy;

  // if favicon uri is invalid we skip the attribute silently, to avoid
  // creating a corrupt file.
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aURI);
  if (NS_FAILED(rv)) {
    nsCAutoString warnMsg;
    warnMsg.Append("Bookmarks Export: Found invalid favicon '");
    warnMsg.Append(aURI);
    warnMsg.Append("'");
    NS_WARNING(warnMsg.get());
    return NS_OK;
  }

  // get favicon
  nsCOMPtr<nsIFaviconService> faviconService = do_GetService(NS_FAVICONSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIURI> faviconURI;
  rv = faviconService->GetFaviconForPage(uri, getter_AddRefs(faviconURI));
  if (rv == NS_ERROR_NOT_AVAILABLE)
    return NS_OK; // no favicon
  NS_ENSURE_SUCCESS(rv, rv); // anything else is error

  nsCAutoString faviconScheme;
  nsCAutoString faviconSpec;
  rv = faviconURI->GetSpec(faviconSpec);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = faviconURI->GetScheme(faviconScheme);
  NS_ENSURE_SUCCESS(rv, rv);

  // write favicon URI: 'ICON_URI="..."'
  rv = aOutput->Write(kIconURIAttribute, sizeof(kIconURIAttribute)-1, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = WriteEscapedUrl(faviconSpec, aOutput);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aOutput->Write(kQuoteStr, sizeof(kQuoteStr)-1, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!faviconScheme.EqualsLiteral("chrome")) {
    // only store data for non-chrome URIs

    nsAutoString faviconContents;
    rv = faviconService->GetFaviconDataAsDataURL(faviconURI, faviconContents);
    NS_ENSURE_SUCCESS(rv, rv);
    if (faviconContents.Length() > 0) {
      rv = aOutput->Write(kIconAttribute, sizeof(kIconAttribute)-1, &dummy);
      NS_ENSURE_SUCCESS(rv, rv);
      NS_ConvertUTF16toUTF8 utf8Favicon(faviconContents);
      rv = aOutput->Write(utf8Favicon.get(), utf8Favicon.Length(), &dummy);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = aOutput->Write(kQuoteStr, sizeof(kQuoteStr)-1, &dummy);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  return NS_OK;
}


// WriteDateAttribute
//
//    This writes the '{attr value=}"{time in seconds}"' attribute for
//    an item.
static nsresult
WriteDateAttribute(const char aAttributeStart[], PRInt32 aLength, PRTime aAttributeValue, nsIOutputStream* aOutput)
{
  // write attribute start
  PRUint32 dummy;
  nsresult rv = aOutput->Write(aAttributeStart, aLength, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  // in bookmarks.html this value is in seconds, not microseconds
  aAttributeValue /= 1000000; 

  // write attribute value
  char dateInSeconds[32];
  PR_snprintf(dateInSeconds, sizeof(dateInSeconds), "%lld", aAttributeValue);
  rv = aOutput->Write(dateInSeconds, strlen(dateInSeconds), &dummy);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aOutput->Write(kQuoteStr, sizeof(kQuoteStr)-1, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


// nsPlacesExportService::WriteContainer
//
//    Writes out all the necessary parts of a bookmarks folder.
nsresult
nsPlacesExportService::WriteContainer(nsINavHistoryResultNode* aFolder,
                                            const nsACString& aIndent,
                                            nsIOutputStream* aOutput)
{
  nsresult rv = WriteContainerHeader(aFolder, aIndent, aOutput);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = WriteContainerPrologue(aIndent, aOutput);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = WriteContainerContents(aFolder, aIndent, aOutput);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = WriteContainerEpilogue(aIndent, aOutput);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}


// nsPlacesExportService::WriteContainerHeader
//
//    This writes '<DL><H3>Title</H3>'
//    Remember folders can also have favicons, which we put in the H3 tag
nsresult
nsPlacesExportService::WriteContainerHeader(nsINavHistoryResultNode* aFolder,
                                                  const nsACString& aIndent,
                                                  nsIOutputStream* aOutput)
{
  PRUint32 dummy;
  nsresult rv;

  // indent
  if (!aIndent.IsEmpty()) {
    rv = aOutput->Write(PromiseFlatCString(aIndent).get(), aIndent.Length(), &dummy);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // "<DL H3"
  rv = aOutput->Write(kContainerIntro, sizeof(kContainerIntro)-1, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  // get folder id
  PRInt64 folderId;
  rv = aFolder->GetItemId(&folderId);
  NS_ENSURE_SUCCESS(rv, rv);

  // write ADD_DATE
  PRTime dateAdded = 0;
  rv = aFolder->GetDateAdded(&dateAdded);
  NS_ENSURE_SUCCESS(rv, rv);

  if (dateAdded) {
    rv = WriteDateAttribute(kDateAddedAttribute, sizeof(kDateAddedAttribute)-1, dateAdded, aOutput);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // write LAST_MODIFIED
  PRTime lastModified = 0;
  rv = aFolder->GetLastModified(&lastModified);
  NS_ENSURE_SUCCESS(rv, rv);

  if (lastModified) {
    rv = WriteDateAttribute(kLastModifiedAttribute, sizeof(kLastModifiedAttribute)-1, lastModified, aOutput);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  PRInt64 placesRoot;
  rv = mBookmarksService->GetPlacesRoot(&placesRoot);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt64 bookmarksMenuFolder;
  rv = mBookmarksService->GetBookmarksMenuFolder(&bookmarksMenuFolder);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt64 toolbarFolder;
  rv = mBookmarksService->GetToolbarFolder(&toolbarFolder);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt64 unfiledBookmarksFolder;
  rv = mBookmarksService->GetUnfiledBookmarksFolder(&unfiledBookmarksFolder);
  NS_ENSURE_SUCCESS(rv, rv);

  // " PERSONAL_TOOLBAR_FOLDER="true"", etc.
  if (folderId == placesRoot) {
    rv = aOutput->Write(kPlacesRootAttribute, sizeof(kPlacesRootAttribute)-1, &dummy);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else if (folderId == bookmarksMenuFolder) {
    rv = aOutput->Write(kBookmarksRootAttribute, sizeof(kBookmarksRootAttribute)-1, &dummy);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else if (folderId == unfiledBookmarksFolder) {
    rv = aOutput->Write(kUnfiledBookmarksFolderAttribute, sizeof(kUnfiledBookmarksFolderAttribute)-1, &dummy);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else if (folderId == toolbarFolder) {
    rv = aOutput->Write(kToolbarFolderAttribute, sizeof(kToolbarFolderAttribute)-1, &dummy);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // ">"
  rv = aOutput->Write(kCloseAngle, sizeof(kCloseAngle)-1, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  // title
  rv = WriteTitle(aFolder, aOutput);
  NS_ENSURE_SUCCESS(rv, rv);

  // "</H3>\n"
  rv = aOutput->Write(kContainerClose, sizeof(kContainerClose)-1, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  // description
  rv = WriteDescription(folderId, nsINavBookmarksService::TYPE_FOLDER, aOutput);
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}


// nsPlacesExportService::WriteTitle
//
//    Retrieves, escapes and writes the title to the stream.
nsresult
nsPlacesExportService::WriteTitle(nsINavHistoryResultNode* aItem,
                                        nsIOutputStream* aOutput)
{
  // XXX Bug 381767 - support titles for separators
  PRUint32 type = 0;
  nsresult rv = aItem->GetType(&type);
  NS_ENSURE_SUCCESS(rv, rv);
  if (type == nsINavHistoryResultNode::RESULT_TYPE_SEPARATOR)
    return NS_ERROR_INVALID_ARG;

  nsCAutoString title;
  rv = aItem->GetTitle(title);
  NS_ENSURE_SUCCESS(rv, rv);

  char* escapedTitle = nsEscapeHTML(title.get());
  if (escapedTitle) {
    PRUint32 dummy;
    rv = aOutput->Write(escapedTitle, strlen(escapedTitle), &dummy);
    nsMemory::Free(escapedTitle);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}


// nsPlacesExportService::WriteDescription
//
//  Write description out for all item types.
nsresult
nsPlacesExportService::WriteDescription(PRInt64 aItemId, PRInt32 aType,
                                              nsIOutputStream* aOutput)
{
  bool hasDescription = false;
  nsresult rv = mAnnotationService->ItemHasAnnotation(aItemId,
                                                      DESCRIPTION_ANNO,
                                                      &hasDescription);
  if (NS_FAILED(rv) || !hasDescription)
    return rv;

  nsAutoString description;
  rv = mAnnotationService->GetItemAnnotationString(aItemId, DESCRIPTION_ANNO,
                                                   description);
  NS_ENSURE_SUCCESS(rv, rv);

  char* escapedDesc = nsEscapeHTML(NS_ConvertUTF16toUTF8(description).get());
  if (escapedDesc) {
    PRUint32 dummy;
    rv = aOutput->Write(kDescriptionIntro, sizeof(kDescriptionIntro)-1, &dummy);
    if (NS_FAILED(rv)) {
      nsMemory::Free(escapedDesc);
      return rv;
    }
    rv = aOutput->Write(escapedDesc, strlen(escapedDesc), &dummy);
    nsMemory::Free(escapedDesc);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aOutput->Write(kDescriptionClose, sizeof(kDescriptionClose)-1, &dummy);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

// nsBookmarks::WriteItem
//
//    "<DT><A HREF="..." ICON="...">Name</A>"
nsresult
nsPlacesExportService::WriteItem(nsINavHistoryResultNode* aItem,
                                       const nsACString& aIndent,
                                       nsIOutputStream* aOutput)
{
  // before doing any attempt to write the item check that uri is valid, if the
  // item has a bad uri we skip it silently, otherwise we could stop while
  // exporting, generating a corrupt file.
  nsCAutoString uri;
  nsresult rv = aItem->GetUri(uri);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIURI> pageURI;
  rv = NS_NewURI(getter_AddRefs(pageURI), uri, nsnull);
  if (NS_FAILED(rv)) {
    nsCAutoString warnMsg;
    warnMsg.Append("Bookmarks Export: Found invalid item uri '");
    warnMsg.Append(uri);
    warnMsg.Append("'");
    NS_WARNING(warnMsg.get());
    return NS_OK;
  }

  // indent
  PRUint32 dummy;
  if (!aIndent.IsEmpty()) {
    rv = aOutput->Write(PromiseFlatCString(aIndent).get(), aIndent.Length(), &dummy);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // '<DT><A'
  rv = aOutput->Write(kItemOpen, sizeof(kItemOpen)-1, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  // ' HREF="http://..."' - note that we need to call GetURI on the result
  // node because some nodes (eg queries) generate this lazily.
  rv = aOutput->Write(kHrefAttribute, sizeof(kHrefAttribute)-1, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = WriteEscapedUrl(uri, aOutput);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aOutput->Write(kQuoteStr, sizeof(kQuoteStr)-1, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  // write ADD_DATE
  PRTime dateAdded = 0;
  rv = aItem->GetDateAdded(&dateAdded);
  NS_ENSURE_SUCCESS(rv, rv);

  if (dateAdded) {
    rv = WriteDateAttribute(kDateAddedAttribute, sizeof(kDateAddedAttribute)-1, dateAdded, aOutput);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // write LAST_MODIFIED
  PRTime lastModified = 0;
  rv = aItem->GetLastModified(&lastModified);
  NS_ENSURE_SUCCESS(rv, rv);

  if (lastModified) {
    rv = WriteDateAttribute(kLastModifiedAttribute, sizeof(kLastModifiedAttribute)-1, lastModified, aOutput);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // ' ICON="..."'
  rv = WriteFaviconAttribute(uri, aOutput);
  NS_ENSURE_SUCCESS(rv, rv);

  // get item id 
  PRInt64 itemId;
  rv = aItem->GetItemId(&itemId);
  NS_ENSURE_SUCCESS(rv, rv);

  // keyword (shortcuturl)
  nsAutoString keyword;
  rv = mBookmarksService->GetKeywordForBookmark(itemId, keyword);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!keyword.IsEmpty()) {
    rv = aOutput->Write(kKeywordAttribute, sizeof(kKeywordAttribute)-1, &dummy);
    NS_ENSURE_SUCCESS(rv, rv);
    char* escapedKeyword = nsEscapeHTML(NS_ConvertUTF16toUTF8(keyword).get());
    rv = aOutput->Write(escapedKeyword, strlen(escapedKeyword), &dummy);
    nsMemory::Free(escapedKeyword);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aOutput->Write(kQuoteStr, sizeof(kQuoteStr)-1, &dummy);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // post data
  bool hasPostData;
  rv = mAnnotationService->ItemHasAnnotation(itemId, POST_DATA_ANNO,
                                             &hasPostData);
  NS_ENSURE_SUCCESS(rv, rv);
  if (hasPostData) {
    nsAutoString postData;
    rv = mAnnotationService->GetItemAnnotationString(itemId, POST_DATA_ANNO,
                                                     postData);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aOutput->Write(kPostDataAttribute, sizeof(kPostDataAttribute)-1, &dummy);
    NS_ENSURE_SUCCESS(rv, rv);
    char* escapedPostData = nsEscapeHTML(NS_ConvertUTF16toUTF8(postData).get());
    rv = aOutput->Write(escapedPostData, strlen(escapedPostData), &dummy);
    nsMemory::Free(escapedPostData);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aOutput->Write(kQuoteStr, sizeof(kQuoteStr)-1, &dummy);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Write WEB_PANEL="true" if the load-in-sidebar annotation is set for the
  // item
  bool loadInSidebar = false;
  rv = mAnnotationService->ItemHasAnnotation(itemId, LOAD_IN_SIDEBAR_ANNO,
                                             &loadInSidebar);
  NS_ENSURE_SUCCESS(rv, rv);
  if (loadInSidebar)
    aOutput->Write(kWebPanelAttribute, sizeof(kWebPanelAttribute)-1, &dummy);

  // last charset
  nsAutoString lastCharset;
  if (NS_SUCCEEDED(mHistoryService->GetCharsetForURI(pageURI, lastCharset)) &&
      !lastCharset.IsEmpty()) {
    rv = aOutput->Write(kLastCharsetAttribute, sizeof(kLastCharsetAttribute)-1, &dummy);
    NS_ENSURE_SUCCESS(rv, rv);
    char* escapedLastCharset = nsEscapeHTML(NS_ConvertUTF16toUTF8(lastCharset).get());
    rv = aOutput->Write(escapedLastCharset, strlen(escapedLastCharset), &dummy);
    nsMemory::Free(escapedLastCharset);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aOutput->Write(kQuoteStr, sizeof(kQuoteStr)-1, &dummy);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // '>'
  rv = aOutput->Write(kCloseAngle, sizeof(kCloseAngle)-1, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  // title
  nsCAutoString title;
  rv = aItem->GetTitle(title);
  NS_ENSURE_SUCCESS(rv, rv);
  char* escapedTitle = nsEscapeHTML(title.get());
  if (escapedTitle) {
    rv = aOutput->Write(escapedTitle, strlen(escapedTitle), &dummy);
    nsMemory::Free(escapedTitle);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // '</A>\n'
  rv = aOutput->Write(kItemClose, sizeof(kItemClose)-1, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  // description
  rv = WriteDescription(itemId, nsINavBookmarksService::TYPE_BOOKMARK, aOutput);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


// WriteLivemark
//
//    Similar to WriteItem, this has an additional FEEDURL attribute and
//    the HREF is optional and points to the source page.
nsresult
nsPlacesExportService::WriteLivemark(nsINavHistoryResultNode* aFolder, const nsACString& aIndent,
                              nsIOutputStream* aOutput)
{
  PRUint32 dummy;
  nsresult rv;

  // indent
  if (!aIndent.IsEmpty()) {
    rv = aOutput->Write(PromiseFlatCString(aIndent).get(), aIndent.Length(), &dummy);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // '<DT><A'
  rv = aOutput->Write(kItemOpen, sizeof(kItemOpen)-1, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  // get folder id
  PRInt64 folderId;
  rv = aFolder->GetItemId(&folderId);
  NS_ENSURE_SUCCESS(rv, rv);

  // get feed URI
  nsString feedSpec;
  rv = mAnnotationService->GetItemAnnotationString(folderId,
                                                   NS_LITERAL_CSTRING(LMANNO_FEEDURI),
                                                   feedSpec);
  
  NS_ENSURE_SUCCESS(rv, rv);

  // write feed URI
  rv = aOutput->Write(kFeedURIAttribute, sizeof(kFeedURIAttribute)-1, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = WriteEscapedUrl(NS_ConvertUTF16toUTF8(feedSpec), aOutput);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aOutput->Write(kQuoteStr, sizeof(kQuoteStr)-1, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  // get the optional site URI
  nsString siteSpec;
  rv = mAnnotationService->GetItemAnnotationString(folderId,
                                                   NS_LITERAL_CSTRING(LMANNO_SITEURI),
                                                   siteSpec);
  if (NS_SUCCEEDED(rv) && !siteSpec.IsEmpty()) {
    // write site URI
    rv = aOutput->Write(kHrefAttribute, sizeof(kHrefAttribute)-1, &dummy);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = WriteEscapedUrl(NS_ConvertUTF16toUTF8(siteSpec), aOutput);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aOutput->Write(kQuoteStr, sizeof(kQuoteStr)-1, &dummy);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // '>'
  rv = aOutput->Write(kCloseAngle, sizeof(kCloseAngle)-1, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  // title
  rv = WriteTitle(aFolder, aOutput);
  NS_ENSURE_SUCCESS(rv, rv);

  // '</A>\n'
  rv = aOutput->Write(kItemClose, sizeof(kItemClose)-1, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  // description
  rv = WriteDescription(folderId, nsINavBookmarksService::TYPE_BOOKMARK, aOutput);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


// nsPlacesExportService::WriteSeparator
//
//    "<HR NAME="...">"
nsresult
nsPlacesExportService::WriteSeparator(nsINavHistoryResultNode* aItem,
                                            const nsACString& aIndent,
                                            nsIOutputStream* aOutput)
{
  PRUint32 dummy;
  nsresult rv;

  // indent
  if (!aIndent.IsEmpty()) {
    rv = aOutput->Write(PromiseFlatCString(aIndent).get(), aIndent.Length(),
                        &dummy);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = aOutput->Write(kSeparator, sizeof(kSeparator)-1, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  // XXX: separator result nodes don't support the title getter yet
  PRInt64 itemId;
  rv = aItem->GetItemId(&itemId);
  NS_ENSURE_SUCCESS(rv, rv);

  // Note: we can't write the separator ID or anything else other than NAME
  // because it makes Firefox 2.x crash/hang - see bug #381129

  nsCAutoString title;
  rv = mBookmarksService->GetItemTitle(itemId, title);
  if (NS_SUCCEEDED(rv) && !title.IsEmpty()) {
    rv = aOutput->Write(kNameAttribute, strlen(kNameAttribute), &dummy);
    NS_ENSURE_SUCCESS(rv, rv);

    char* escapedTitle = nsEscapeHTML(title.get());
    if (escapedTitle) {
      PRUint32 dummy;
      rv = aOutput->Write(escapedTitle, strlen(escapedTitle), &dummy);
      nsMemory::Free(escapedTitle);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = aOutput->Write(kQuoteStr, sizeof(kQuoteStr)-1, &dummy);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // '>'
  rv = aOutput->Write(kCloseAngle, sizeof(kCloseAngle)-1, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  // line break
  rv = aOutput->Write(NS_LINEBREAK, sizeof(NS_LINEBREAK)-1, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}


// WriteEscapedUrl
//
//    Writes the given string to the stream escaped as necessary for URLs.
//
//    Unfortunately, the old bookmarks system uses a custom hardcoded and
//    braindead escaping scheme that we need to emulate. It just replaces
//    quotes with %22 and that's it.
nsresult
WriteEscapedUrl(const nsCString& aString, nsIOutputStream* aOutput)
{
  nsCAutoString escaped(aString);
  PRInt32 offset;
  while ((offset = escaped.FindChar('\"')) >= 0) {
    escaped.Cut(offset, 1);
    escaped.Insert(NS_LITERAL_CSTRING("%22"), offset);
  }
  PRUint32 dummy;
  return aOutput->Write(escaped.get(), escaped.Length(), &dummy);
}


// nsPlacesExportService::WriteContainerContents
//
//    The indent here is the indent of the parent. We will add an additional
//    indent before writing data.
nsresult
nsPlacesExportService::WriteContainerContents(nsINavHistoryResultNode* aFolder,
                                                    const nsACString& aIndent,
                                                    nsIOutputStream* aOutput)
{
  nsCAutoString myIndent(aIndent);
  myIndent.Append(kIndent);

  PRInt64 folderId;
  nsresult rv = aFolder->GetItemId(&folderId);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsINavHistoryContainerResultNode> folderNode = do_QueryInterface(aFolder, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = folderNode->SetContainerOpen(true);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 childCount = 0;
  folderNode->GetChildCount(&childCount);
  for (PRUint32 i = 0; i < childCount; ++i) {
    nsCOMPtr<nsINavHistoryResultNode> child;
    rv = folderNode->GetChild(i, getter_AddRefs(child));
    NS_ENSURE_SUCCESS(rv, rv);
    PRUint32 type = 0;
    rv = child->GetType(&type);
    NS_ENSURE_SUCCESS(rv, rv);
    if (type == nsINavHistoryResultNode::RESULT_TYPE_FOLDER) {
      // bookmarks folder
      PRInt64 childFolderId;
      rv = child->GetItemId(&childFolderId);
      NS_ENSURE_SUCCESS(rv, rv);

      // it could be a regular folder or it could be a livemark.
      // Livemarks service is async, for now just workaround using annotations
      // service.
      bool isLivemark;
      nsresult rv = mAnnotationService->ItemHasAnnotation(childFolderId,
                                                          NS_LITERAL_CSTRING(LMANNO_FEEDURI),
                                                          &isLivemark);
      NS_ENSURE_SUCCESS(rv, rv);

      if (isLivemark)
        rv = WriteLivemark(child, myIndent, aOutput);
      else
        rv = WriteContainer(child, myIndent, aOutput);
    }
    else if (type == nsINavHistoryResultNode::RESULT_TYPE_SEPARATOR) {
      rv = WriteSeparator(child, myIndent, aOutput);
    }
    else {
      rv = WriteItem(child, myIndent, aOutput);
    }
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}



NS_IMETHODIMP
nsPlacesExportService::ExportHTMLToFile(nsILocalFile* aBookmarksFile)
{
  NS_ENSURE_ARG(aBookmarksFile);

#ifdef DEBUG_EXPORT
  nsAutoString path;
  aBookmarksFile->GetPath(path);
  printf("\nExporting %s\n", NS_ConvertUTF16toUTF8(path).get());

  PRTime startTime = PR_Now();
  printf("\nStart time: %lld\n", startTime);
#endif

  nsresult rv = EnsureServiceState();
  NS_ENSURE_SUCCESS(rv, rv);

  // get a safe output stream, so we don't clobber the bookmarks file unless
  // all the writes succeeded.
  nsCOMPtr<nsIOutputStream> out;
  rv = NS_NewSafeLocalFileOutputStream(getter_AddRefs(out),
                                       aBookmarksFile,
                                       PR_WRONLY | PR_CREATE_FILE,
                                       0600, 0);
  NS_ENSURE_SUCCESS(rv, rv);

  // We need a buffered output stream for performance.
  // See bug 202477.
  nsCOMPtr<nsIOutputStream> strm;
  rv = NS_NewBufferedOutputStream(getter_AddRefs(strm), out, 4096);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get a new query object.
  nsCOMPtr<nsINavHistoryQuery> query;
  rv = mHistoryService->GetNewQuery(getter_AddRefs(query));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsINavHistoryQueryOptions> options;
  rv = mHistoryService->GetNewQueryOptions(getter_AddRefs(options));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsINavHistoryResult> result;

  // We need the bookmarks menu root node to write out the title.
  PRInt64 bookmarksMenuFolder;
  rv = mBookmarksService->GetBookmarksMenuFolder(&bookmarksMenuFolder);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = query->SetFolders(&bookmarksMenuFolder, 1);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mHistoryService->ExecuteQuery(query, options, getter_AddRefs(result));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsINavHistoryContainerResultNode> rootNode;
  rv = result->GetRoot(getter_AddRefs(rootNode));
  NS_ENSURE_SUCCESS(rv, rv);

  // file header
  PRUint32 dummy;
  rv = strm->Write(kFileIntro, sizeof(kFileIntro)-1, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  // '<H1'
  rv = strm->Write(kRootIntro, sizeof(kRootIntro)-1, &dummy); // <H1
  NS_ENSURE_SUCCESS(rv, rv);

  // '>Bookmarks</H1>
  rv = strm->Write(kCloseAngle, sizeof(kCloseAngle)-1, &dummy); // >
  NS_ENSURE_SUCCESS(rv, rv);
  rv = WriteTitle(rootNode, strm);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = strm->Write(kCloseRootH1, sizeof(kCloseRootH1)-1, &dummy); // </H1>
  NS_ENSURE_SUCCESS(rv, rv);

  // Container's prologue.
  rv = WriteContainerPrologue(EmptyCString(), strm);
  NS_ENSURE_SUCCESS(rv, rv);

  // indents
  nsCAutoString indent;
  indent.Assign(kIndent);

  // Bookmarks Menu.
  rv = WriteContainerContents(rootNode, EmptyCString(), strm);
  NS_ENSURE_SUCCESS(rv, rv);

  // Bookmarks Toolbar.
  // We write this folder under the bookmarks-menu for backwards compatibility.
  PRInt64 toolbarFolder;
  rv = mBookmarksService->GetToolbarFolder(&toolbarFolder);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = query->SetFolders(&toolbarFolder, 1);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mHistoryService->ExecuteQuery(query, options, getter_AddRefs(result));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = result->GetRoot(getter_AddRefs(rootNode));
  NS_ENSURE_SUCCESS(rv, rv);
  // Write it out only if it's not empty.
  rv = rootNode->SetContainerOpen(true);
  NS_ENSURE_SUCCESS(rv, rv);
  PRUint32 childCount = 0;
  rv = rootNode->GetChildCount(&childCount);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = rootNode->SetContainerOpen(false);
  NS_ENSURE_SUCCESS(rv, rv);
  if (childCount) {
    rv = WriteContainer(rootNode, nsDependentCString(kIndent), strm);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Unfiled Bookmarks.
  // We write this folder under the bookmarks-menu for backwards compatibility.
  PRInt64 unfiledBookmarksFolder;
  rv = mBookmarksService->GetUnfiledBookmarksFolder(&unfiledBookmarksFolder);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = query->SetFolders(&unfiledBookmarksFolder, 1);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mHistoryService->ExecuteQuery(query, options, getter_AddRefs(result));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = result->GetRoot(getter_AddRefs(rootNode));
  NS_ENSURE_SUCCESS(rv, rv);
  // Write it out only if it's not empty.
  rv = rootNode->SetContainerOpen(true);
  NS_ENSURE_SUCCESS(rv, rv);
  childCount = 0;
  rootNode->GetChildCount(&childCount);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = rootNode->SetContainerOpen(false);
  NS_ENSURE_SUCCESS(rv, rv);
  if (childCount) {
    rv = WriteContainer(rootNode, nsDependentCString(kIndent), strm);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Container's epilogue.
  rv = WriteContainerEpilogue(EmptyCString(), strm);
  NS_ENSURE_SUCCESS(rv, rv);

  // commit the write
  nsCOMPtr<nsISafeOutputStream> safeStream = do_QueryInterface(strm, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = safeStream->Finish();
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG_EXPORT
  printf("\nTotal time in seconds: %lld\n", (PR_Now() - startTime)/1000000);
#endif

  return NS_OK;
}


NS_IMETHODIMP
nsPlacesExportService::BackupBookmarksFile()
{
  nsresult rv = EnsureServiceState();
  NS_ENSURE_SUCCESS(rv, rv);

  // get bookmarks file
  nsCOMPtr<nsIFile> bookmarksFileDir;
  rv = NS_GetSpecialDirectory(NS_APP_BOOKMARKS_50_FILE,
                              getter_AddRefs(bookmarksFileDir));

  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsILocalFile> bookmarksFile = do_QueryInterface(bookmarksFileDir);
  NS_ENSURE_STATE(bookmarksFile);

  // Create the file if it doesn't exist.
  bool exists;
  rv = bookmarksFile->Exists(&exists);
  if (NS_FAILED(rv) || !exists) {
    rv = bookmarksFile->Create(nsIFile::NORMAL_FILE_TYPE, 0600);
    if (NS_FAILED(rv)) {
      NS_WARNING("Unable to create bookmarks.html!");
      return rv;
    }
  }

  // export bookmarks.html
  rv = ExportHTMLToFile(bookmarksFile);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}
