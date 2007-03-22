/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla History System
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brett Wilson <brettw@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
 *   For backwards compatability, if we don't find this, we assume that the
 *   hierarchy is rooted at the bookmarks menu.
 * Heading := any heading other than h1
 *   Old version used this to set attributes on the current container. We only
 *   care about the content of the heading container, which contains the title
 *   of the bookmark container.
 * Bookmark := a
 *   HREF is the destination of the bookmark
 *   FEEDURL is the URI of the RSS feed if this is a livemark.
 *   LAST_CHARSET should be stored as an annotation (FIXME bug 334408) so that the
 *     next time we go to that page we remember the user's preference.
 *   WEB_PANEL is set to "true" if the bookmark should be loaded in the sidebar.
 *   ICON will be stored in the favicon service
 *   ICON_URI is new for places bookmarks.html, it refers to the original
 *     URI of the favicon so we don't have to make up favicon URLs.
 *   Text of the <a> container is the name of the bookmark
 *   Ignored: ADD_DATE, LAST_VISIT, LAST_MODIFIED, ID
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

#include "nsToolkitCompsCID.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsEscape.h"
#include "nsFaviconService.h"
#include "nsIAnnotationService.h"
#include "nsIFile.h"
#include "nsIHTMLContentSink.h"
#include "nsILivemarkService.h"
#include "nsIParser.h"
#include "nsIServiceManager.h"
#include "nsNavBookmarks.h"
#include "nsNavHistory.h"
#include "nsAnnotationService.h"
#include "nsNavHistoryResult.h"
#include "nsNetUtil.h"
#include "nsParserCIID.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsUnicharUtils.h"
#include "mozStorageHelper.h"
#include "plbase64.h"
#include "prtime.h"

static NS_DEFINE_CID(kParserCID, NS_PARSER_CID);

#define KEY_TOOLBARFOLDER_LOWER "personal_toolbar_folder"
#define KEY_BOOKMARKSMENU_LOWER "bookmarks_menu"
#define KEY_PLACESROOT_LOWER "places_root"
#define KEY_HREF_LOWER "href"
#define KEY_FEEDURL_LOWER "feedurl"
#define KEY_WEB_PANEL_LOWER "web_panel"
#define KEY_LASTCHARSET_LOWER "last_charset"
#define KEY_ICON_LOWER "icon"
#define KEY_ICON_URI_LOWER "icon_uri"
#define KEY_SHORTCUTURL_LOWER "shortcuturl"

#define LOAD_IN_SIDEBAR_ANNO NS_LITERAL_CSTRING("bookmarkProperties/loadInSidebar")

#define BOOKMARKS_MENU_ICON_URI "chrome://browser/skin/places/bookmarksMenu.png"
#define BOOKMARKS_TOOLBAR_ICON_URI "chrome://browser/skin/places/bookmarksToolbar.png"

// define to get debugging messages on console about import
//#define DEBUG_IMPORT

static const char kWhitespace[] = " \r\n\t\b";

static nsresult WriteEscapedUrl(const nsCString& aString,
                                nsIOutputStream* aOutput);

class BookmarkImportFrame
{
public:
  BookmarkImportFrame(PRInt64 aID) :
      mContainerID(aID),
      mContainerNesting(0),
      mLastContainerType(Container_Normal),
      mInDescription(PR_FALSE)
  {
  }

  enum ContainerType { Container_Normal,
                       Container_Places,
                       Container_Menu,
                       Container_Toolbar };

  PRInt64 mContainerID;

  // How many <dl>s have been nested. Each frame/container should start
  // with a heading, and is then followed by a <dl>, <ul>, or <menu>. When
  // that list is complete, then it is the end of this container and we need
  // to pop back up one level for new items. If we never get an open tag for
  // one of these things, we should assume that the container is empty and
  // that things we find should be siblings of it. Normally, these <dl>s won't
  // be nested so this will be 0 or 1.
  PRInt32 mContainerNesting;

  // when we find a heading tag, it actually affects the title of the NEXT
  // container in the list. This stores that heading tag and whether it was
  // special. 'ConsumeHeading' resets this.
  ContainerType mLastContainerType;

  // this contains the text from the last begin tag until now. It is reset
  // at every begin tag. We can check it when we see a </a>, or </h3>
  // to see what the text content of that node should be.
  nsString mPreviousText;

  // true when we hit a <dd>, which contains the description for the preceeding
  // <a> tag. We can't just check for </dd> like we can for </a> or </h3>
  // because if there is a sub-folder, it is actually a child of the <dd>
  // because the tag is never explicitly closed. If this is true and we see a
  // new open tag, that means to commit the description to the previous
  // bookmark.
  //
  // Additional weirdness happens when the previous <dt> tag contains a <h3>:
  // this means there is a new folder with the given description, and whose
  // children are contained in the following <dl> list.
  //
  // This is handled in OpenContainer(), which commits previous text if
  // necessary.
  PRBool mInDescription;

  // contains the URL of the previous bookmark created. This is used so that
  // when we encounter a <dd>, we know what bookmark to associate the text with.
  // This is cleared whenever we hit a <h3>, so that we know NOT to save this
  // with a bookmark, but to keep it until 
  nsCOMPtr<nsIURI> mPreviousLink;

  // contains the URL of the previous livemark, so that when the link ends,
  // and the livemark title is known, we can create it.
  nsCOMPtr<nsIURI> mPreviousFeed;

  void ConsumeHeading(nsAString* aHeading, ContainerType* aContainerType)
  {
    *aHeading = mPreviousText;
    *aContainerType = mLastContainerType;
    mPreviousText.Truncate(0);
  }

  // Contains the id of a newly created bookmark.
  // XXXDietrich - may also be a pre-existing bookmark once we support importing Places-exported bookmarks.html files.
  PRInt64 mPreviousId;
};


/**
 * The content sink stuff is based loosely on 
 */
class BookmarkContentSink : public nsIHTMLContentSink
{
public:
  nsresult Init(PRBool aAllowRootChanges,
                nsINavBookmarksService* bookmarkService,
                PRInt64 aFolder,
                PRBool aIsImportDefaults);

  NS_DECL_ISUPPORTS

  // nsIContentSink (superclass of nsIHTMLContentSink)
  NS_IMETHOD WillTokenize() { return NS_OK; }
  NS_IMETHOD WillBuildModel() { return NS_OK; }
  NS_IMETHOD DidBuildModel() { return NS_OK; }
  NS_IMETHOD WillInterrupt() { return NS_OK; }
  NS_IMETHOD WillResume() { return NS_OK; }
  NS_IMETHOD SetParser(nsIParser* aParser) { return NS_OK; }
  virtual void FlushPendingNotifications(mozFlushType aType) { }
  NS_IMETHOD SetDocumentCharset(nsACString& aCharset) { return NS_OK; }
  virtual nsISupports *GetTarget() { return nsnull; }

  // nsIHTMLContentSink
#ifdef MOZILLA_1_8_BRANCH
  NS_IMETHOD SetTitle(const nsString& aValue) { return NS_OK; }
  NS_IMETHOD OpenHTML(const nsIParserNode& aNode) { return NS_OK; }
  NS_IMETHOD CloseHTML() { return NS_OK; }
  NS_IMETHOD OpenHead(const nsIParserNode& aNode) { return NS_OK; }
  NS_IMETHOD CloseHead() { return NS_OK; }
  NS_IMETHOD OpenBody(const nsIParserNode& aNode) { return NS_OK; }
  NS_IMETHOD CloseBody() { return NS_OK; }
  NS_IMETHOD OpenForm(const nsIParserNode& aNode) { return NS_OK; }
  NS_IMETHOD CloseForm() { return NS_OK; }
  NS_IMETHOD OpenMap(const nsIParserNode& aNode) { return NS_OK; }
  NS_IMETHOD CloseMap() { return NS_OK; }
  NS_IMETHOD OpenFrameset(const nsIParserNode& aNode) { return NS_OK; }
  NS_IMETHOD CloseFrameset() { return NS_OK; }
  NS_IMETHOD AddHeadContent(const nsIParserNode& aNode) { return NS_OK; }
#else
  NS_IMETHOD OpenHead() { return NS_OK; }
#endif
  NS_IMETHOD BeginContext(PRInt32 aPosition) { return NS_OK; }
  NS_IMETHOD EndContext(PRInt32 aPosition) { return NS_OK; }
  NS_IMETHOD IsEnabled(PRInt32 aTag, PRBool* aReturn)
    { *aReturn = PR_TRUE; return NS_OK; }
  NS_IMETHOD WillProcessTokens() { return NS_OK; }
  NS_IMETHOD DidProcessTokens() { return NS_OK; }
  NS_IMETHOD WillProcessAToken() { return NS_OK; }
  NS_IMETHOD DidProcessAToken() { return NS_OK; }
  NS_IMETHOD OpenContainer(const nsIParserNode& aNode);
  NS_IMETHOD CloseContainer(const nsHTMLTag aTag);
  NS_IMETHOD AddLeaf(const nsIParserNode& aNode);
  NS_IMETHOD AddComment(const nsIParserNode& aNode) { return NS_OK; }
  NS_IMETHOD AddProcessingInstruction(const nsIParserNode& aNode) { return NS_OK; }
  NS_IMETHOD AddDocTypeDecl(const nsIParserNode& aNode) { return NS_OK; }
  NS_IMETHOD NotifyTagObservers(nsIParserNode* aNode) { return NS_OK; }
  NS_IMETHOD_(PRBool) IsFormOnStack() { return PR_FALSE; }

protected:
  nsCOMPtr<nsINavBookmarksService> mBookmarksService;
  nsCOMPtr<nsINavHistoryService> mHistoryService;
  nsCOMPtr<nsIAnnotationService> mAnnotationService;
  nsCOMPtr<nsILivemarkService> mLivemarkService;

  // If set, we will move root items to from their existing position
  // in the hierarchy, to where we find them in the bookmarks file
  // being imported. This should be set when we are loading 
  // the default places html file, and should be unset when doing
  // normal imports so that root folders will not get moved  when
  // importing bookmarks.html files.
  PRBool mAllowRootChanges;

  // if set, this is an import of initial bookmarks.html content,
  // so we don't want to kick off HTTP traffic
  PRBool mIsImportDefaults;

  // If a folder was specified to import into, then ignore flags to put
  // bookmarks in the bookmarks menu or toolbar and keep them inside
  // the folder.
  PRBool mFolderSpecified;

  void HandleContainerBegin(const nsIParserNode& node);
  void HandleContainerEnd();
  void HandleHead1Begin(const nsIParserNode& node);
  void HandleHeadBegin(const nsIParserNode& node);
  void HandleHeadEnd();
  void HandleLinkBegin(const nsIParserNode& node);
  void HandleLinkEnd();
  void HandleSeparator();

  // This is a list of frames. We really want a recursive parser, but the HTML
  // parser gives us tags as a stream. This implements all the state on a stack
  // so we can get the recursive information we need. Use "CurFrame" to get the
  // top "stack frame" with the current state in it.
  nsTArray<BookmarkImportFrame> mFrames;
  BookmarkImportFrame& CurFrame()
  {
    NS_ASSERTION(mFrames.Length() > 0, "Asking for frame when there are none!");
    return mFrames[mFrames.Length() - 1];
  }
  nsresult NewFrame();
  nsresult PopFrame();

  nsresult SetFaviconForURI(nsIURI* aPageURI, nsIURI* aFaviconURI,
                            const nsCString& aData);
  nsresult SetFaviconForFolder(PRInt64 aFolder, const nsACString& aFavicon);

#ifdef DEBUG_IMPORT
  // prints spaces for indenting to the current frame depth
  void PrintNesting()
  {
    for (PRUint32 i = 0; i < mFrames.Length(); i ++)
      printf("  ");
  }
#endif
};


// BookmarkContentSink::Init
//
//    Note that the bookmark service pointer is passed in. We can not create
//    the bookmark service from here because this can be called from bookmark
//    service creation, making a weird reentrant loop.

nsresult
BookmarkContentSink::Init(PRBool aAllowRootChanges,
                          nsINavBookmarksService* bookmarkService,
                          PRInt64 aFolder,
                          PRBool aIsImportDefaults)
{
  nsresult rv;
  mBookmarksService = bookmarkService;
  mHistoryService = do_GetService(NS_NAVHISTORYSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  mAnnotationService = do_GetService(NS_ANNOTATIONSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  mLivemarkService = do_GetService(NS_LIVEMARKSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  mAllowRootChanges = aAllowRootChanges;
  mIsImportDefaults = aIsImportDefaults;

  // initialize the root frame with the menu root
  PRInt64 menuRoot;
  if (aFolder == 0) {
    rv = mBookmarksService->GetBookmarksRoot(&menuRoot);
    NS_ENSURE_SUCCESS(rv, rv);
    mFolderSpecified = false;
  }
  else {
    menuRoot = aFolder;
    mFolderSpecified = true;
  }
  if (! mFrames.AppendElement(BookmarkImportFrame(menuRoot)))
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}


NS_IMPL_ISUPPORTS2(BookmarkContentSink,
                   nsIContentSink,
                   nsIHTMLContentSink)

// nsIContentSink **************************************************************

NS_IMETHODIMP
BookmarkContentSink::OpenContainer(const nsIParserNode& aNode)
{
  // see the comment for the definition of mInDescription. Basically, we commit
  // any text in mPreviousText to the description of the node/folder if there
  // is any.
  BookmarkImportFrame& frame = CurFrame();
  if (frame.mInDescription) {
    frame.mPreviousText.Trim(kWhitespace); // important!
    if (! frame.mPreviousText.IsEmpty()) {
      // FIXME bug 334758: This description should be stored as an annotation
      // on the URL or folder. We should probably not overwrite existing
      // annotations.
      frame.mPreviousText.Truncate(0);
    }
    frame.mInDescription = PR_FALSE;
  }

  switch(aNode.GetNodeType()) {
    case eHTMLTag_h1:
      HandleHead1Begin(aNode);
      break;
    case eHTMLTag_h2:
    case eHTMLTag_h3:
    case eHTMLTag_h4:
    case eHTMLTag_h5:
    case eHTMLTag_h6:
      HandleHeadBegin(aNode);
      break;
    case eHTMLTag_a:
      HandleLinkBegin(aNode);
      break;
    case eHTMLTag_dl:
    case eHTMLTag_ul:
    case eHTMLTag_menu:
      HandleContainerBegin(aNode);
      break;
  }
  return NS_OK;
}

NS_IMETHODIMP
BookmarkContentSink::CloseContainer(const nsHTMLTag aTag)
{
  switch (aTag) {
    case eHTMLTag_dl:
    case eHTMLTag_ul:
    case eHTMLTag_menu:
      HandleContainerEnd();
      break;
    case eHTMLTag_dt:
      break;
    case eHTMLTag_h1:
      // ignore
      break;
    case eHTMLTag_h2:
    case eHTMLTag_h3:
    case eHTMLTag_h4:
    case eHTMLTag_h5:
    case eHTMLTag_h6:
      HandleHeadEnd();
      break;
    case eHTMLTag_a:
      HandleLinkEnd();
      break;
    default:
      break;
  }
  return NS_OK;
}


// BookmarkContentSink::AddLeaf
//
//    XXX on the branch, we should be calling CollectSkippedContent as in
//    nsHTMLFragmentContentSink.cpp:AddLeaf when we encounter title, script,
//    style, or server tags. Apparently if we don't, we'll leak the next DOM
//    node. However, this requires that we keep a reference to the parser we'll
//    introduce a circular reference because it has a reference to us.
//
//    This is annoying to fix and these elements are not allowed in bookmarks
//    files anyway. So if somebody tries to import a crazy bookmarks file, it
//    will leak a little bit.

NS_IMETHODIMP
BookmarkContentSink::AddLeaf(const nsIParserNode& aNode)
{
  switch (aNode.GetNodeType()) {
  case eHTMLTag_text:
    // save any text we find
    CurFrame().mPreviousText += aNode.GetText();
    break;
  case eHTMLTag_entity: {
    nsAutoString tmp;
    PRInt32 unicode = aNode.TranslateToUnicodeStr(tmp);
    if (unicode < 0) {
      // invalid entity - just use the text of it
      CurFrame().mPreviousText += aNode.GetText();
    } else {
      CurFrame().mPreviousText.Append(unicode);
    }
    break;
  }
  case eHTMLTag_whitespace:
    CurFrame().mPreviousText.Append(PRUnichar(' '));
    break;
  case eHTMLTag_hr:
    HandleSeparator();
    break;
  }

  return NS_OK;
}

// BookmarkContentSink::HandleContainerBegin

void
BookmarkContentSink::HandleContainerBegin(const nsIParserNode& node)
{
  CurFrame().mContainerNesting ++;
}


// BookmarkContentSink::HandleContainerEnd
//
//    Our "indent" count has decreased, and when we hit 0 that means that this
//    container is complete and we need to pop back to the outer frame. Never
//    pop the toplevel frame

void
BookmarkContentSink::HandleContainerEnd()
{
  BookmarkImportFrame& frame = CurFrame();
  if (frame.mContainerNesting > 0)
    frame.mContainerNesting --;
  if (mFrames.Length() > 1 && frame.mContainerNesting == 0)
    PopFrame();
}


// BookmarkContentSink::HandleHead1Begin
//
//    Handles <H1>. We check for the attribute PLACES_ROOT and reset the
//    container id if it's found. Otherwise, the default bookmark menu
//    root is assumed and imported things will go into the bookmarks menu.

void
BookmarkContentSink::HandleHead1Begin(const nsIParserNode& node)
{
  PRInt32 attrCount = node.GetAttributeCount();
  for (PRInt32 i = 0; i < attrCount; i ++) {
    if (node.GetKeyAt(i).LowerCaseEqualsLiteral(KEY_PLACESROOT_LOWER)) {
      if (mFrames.Length() > 1) {
        NS_WARNING("Trying to set the places root from the middle of the hierarchy. "
                   "This can only be set at the beginning.");
        return;
      }
      PRInt64 mPlacesRoot;
      mBookmarksService->GetPlacesRoot(&mPlacesRoot);
      CurFrame().mContainerID = mPlacesRoot;
      break;
    }
  }
}


// BookmarkContentSink::HandleHeadBegin
//
//    Called for h2,h3,h4,h5,h6. This just stores the correct information in
//    the current frame; the actual new frame corresponding to the container
//    associated with the heading will be created when the tag has been closed
//    and we know the title (we don't know to create a new folder or to merge
//    with an existing one until we have the title).

void
BookmarkContentSink::HandleHeadBegin(const nsIParserNode& node)
{
  BookmarkImportFrame& frame = CurFrame();

  // after a heading, a previous bookmark is not applicable (for example, for
  // the descriptions contained in a <dd>). Neither is any previous head type
  frame.mPreviousLink = nsnull;
  frame.mLastContainerType = BookmarkImportFrame::Container_Normal;

  // It is syntactically possible for a heading to appear after another heading
  // but before the <dl> that encloses that folder's contents.  This should not
  // happen in practice, as the file will contain "<dl></dl>" sequence for
  // empty containers.
  //
  // Just to be on the safe side, if we encounter
  //   <h3>FOO</h3>
  //   <h3>BAR</h3>
  //   <dl>...content 1...</dl>
  //   <dl>...content 2...</dl>
  // we'll pop the stack when we find the h3 for BAR, treating that as an
  // implicit ending of the FOO container. The output will be FOO and BAR as
  // siblings. If there's another <dl> following (as in "content 2"), those
  // items will be treated as further siblings of FOO and BAR
  if (frame.mContainerNesting == 0)
    PopFrame();

  // We have to check for some attributes to see if this is a "special"
  // folder, which will have different creation rules when the end tag is
  // processed.
  PRInt32 attrCount = node.GetAttributeCount();
  frame.mLastContainerType = BookmarkImportFrame::Container_Normal;
  if (!mFolderSpecified) {
    for (PRInt32 i = 0; i < attrCount; i ++) {
      if (node.GetKeyAt(i).LowerCaseEqualsLiteral(KEY_TOOLBARFOLDER_LOWER)) {
        frame.mLastContainerType = BookmarkImportFrame::Container_Toolbar;
        break;
      } else if (node.GetKeyAt(i).LowerCaseEqualsLiteral(KEY_BOOKMARKSMENU_LOWER)) {
        frame.mLastContainerType = BookmarkImportFrame::Container_Menu;
        break;
      } else if (node.GetKeyAt(i).LowerCaseEqualsLiteral(KEY_PLACESROOT_LOWER)) {
        frame.mLastContainerType = BookmarkImportFrame::Container_Places;
        break;
      }
    }
  }
  CurFrame().mPreviousText.Truncate(0);
}


// BookmarkContentSink::HandleHeadEnd
//
//    Creates the new frame for this heading now that we know the name of the
//    container (tokens since the heading open tag will have been placed in
//    mPreviousText).

void
BookmarkContentSink::HandleHeadEnd()
{
  NewFrame();
}


// BookmarkContentSink::HandleLinkBegin
//
//    Handles "<a" tags by creating a new bookmark. The title of the bookmark
//    will be the text content, which will be stuffed in mPreviousText for us
//    and which will be saved by HandleLinkEnd

void
BookmarkContentSink::HandleLinkBegin(const nsIParserNode& node)
{
  BookmarkImportFrame& frame = CurFrame();

  // We need to make sure that the feed URIs from previous frames are emptied. 
  frame.mPreviousFeed = nsnull;

  // mPreviousText will hold our link text, clear it so that can be appended to
  frame.mPreviousText.Truncate();

  // get the attributes we care about
  nsAutoString href;
  nsAutoString feedUrl;
  nsAutoString icon;
  nsAutoString iconUri;
  nsAutoString lastCharset;
  nsAutoString keyword;
  nsAutoString webPanel;
  PRInt32 attrCount = node.GetAttributeCount();
  for (PRInt32 i = 0; i < attrCount; i ++) {
    const nsAString& key = node.GetKeyAt(i);
    if (key.LowerCaseEqualsLiteral(KEY_HREF_LOWER)) {
      href = node.GetValueAt(i);
    } else if (key.LowerCaseEqualsLiteral(KEY_FEEDURL_LOWER)) {
      feedUrl = node.GetValueAt(i);
    } else if (key.LowerCaseEqualsLiteral(KEY_ICON_LOWER)) {
      icon = node.GetValueAt(i);
    } else if (key.LowerCaseEqualsLiteral(KEY_ICON_URI_LOWER)) {
      iconUri = node.GetValueAt(i);
    } else if (key.LowerCaseEqualsLiteral(KEY_LASTCHARSET_LOWER)) {
      lastCharset = node.GetValueAt(i);
    } else if (key.LowerCaseEqualsLiteral(KEY_SHORTCUTURL_LOWER)) {
      keyword = node.GetValueAt(i);
    } else if (key.LowerCaseEqualsLiteral(KEY_WEB_PANEL_LOWER)) {
      webPanel = node.GetValueAt(i);
    }
  }
  href.Trim(kWhitespace);
  feedUrl.Trim(kWhitespace);
  icon.Trim(kWhitespace);
  iconUri.Trim(kWhitespace);
  lastCharset.Trim(kWhitespace);
  keyword.Trim(kWhitespace);
  webPanel.Trim(kWhitespace);

  // For feeds, get the feed URL. If it is invalid, it will leave mPreviousFeed
  // NULL and we'll continue trying to create it as a normal bookmark.
  if (! feedUrl.IsEmpty()) {
    NS_NewURI(getter_AddRefs(frame.mPreviousFeed),
              NS_ConvertUTF16toUTF8(feedUrl), nsnull);
  }

  // Ignore <a> tags that have no href: we don't know what to do with them.
  if (href.IsEmpty()) {
    frame.mPreviousLink = nsnull;

    // The exception is for feeds, where the href is an optional component
    // indicating the source web site.
    if (! frame.mPreviousFeed)
      return;
  } else {
    // Save this so the link text and descriptions can be associated with it.
    // Note that we ignore errors if this is a feed: URLs aren't strictly
    // necessary in these cases.
    nsresult rv = NS_NewURI(getter_AddRefs(frame.mPreviousLink),
                   href, nsnull);
    if (NS_FAILED(rv) && ! frame.mPreviousFeed) {
      frame.mPreviousLink = nsnull;
      return; // invalid link
    }
  }

  // if there is a feedURL, this is a livemark, which is a special case
  // that we handle in HandleLinkBegin(): don't create normal bookmarks
  if (frame.mPreviousFeed)
    return;

  // create the bookmark
  nsresult rv = mBookmarksService->InsertItem(frame.mContainerID, frame.mPreviousLink,
                                     mBookmarksService->DEFAULT_INDEX, &frame.mPreviousId);
  NS_ASSERTION(NS_SUCCEEDED(rv), "InsertItem failed");

  // save the favicon, ignore errors
  if (! icon.IsEmpty() || ! iconUri.IsEmpty()) {
    nsCOMPtr<nsIURI> iconUriObject;
    NS_NewURI(getter_AddRefs(iconUriObject), iconUri);
    if (! icon.IsEmpty() || iconUriObject)
      SetFaviconForURI(frame.mPreviousLink, iconUriObject,
                       NS_ConvertUTF16toUTF8(icon));
  }

  // save the keyword, ignore errors
  if (! keyword.IsEmpty())
    mBookmarksService->SetKeywordForBookmark(frame.mPreviousId, keyword);

  if (webPanel.LowerCaseEqualsLiteral("true")) {
    // set load-in-sidebar annotation for the bookmark

    nsCOMPtr<nsIURI> placeURI;
    rv = mBookmarksService->GetItemURI(frame.mPreviousId,
                                       getter_AddRefs(placeURI));
    NS_ASSERTION(NS_SUCCEEDED(rv),
                 "failed to get a place: uri for a new bookmark");
    if (NS_SUCCEEDED(rv)) {
      mAnnotationService->SetAnnotationInt32(placeURI, LOAD_IN_SIDEBAR_ANNO,
                                             1, 0,
                                             nsIAnnotationService::EXPIRE_NEVER);
    }
  }
  // FIXME bug 334408: save the last charset
}


// BookmarkContentSink::HandleLinkEnd
//
//    Saves the title for the given bookmark. This only writes the user title.
//    Any previous title will be untouched. If this is a new entry, it will have
//    an empty "official" title until you visit it.

void
BookmarkContentSink::HandleLinkEnd()
{
  BookmarkImportFrame& frame = CurFrame();
  frame.mPreviousText.Trim(kWhitespace);
  if (frame.mPreviousFeed) {
    // The bookmark is actually a livemark.  Create it here.
    // (It gets created here instead of in HandleLinkBegin()
    // because we need to know the title before creating it.)
    PRInt64 folderId;

    if (mIsImportDefaults) {
      mLivemarkService->CreateLivemarkFolderOnly(mBookmarksService,
                                                 frame.mContainerID,
                                                 frame.mPreviousText,
                                                 frame.mPreviousLink,
                                                 frame.mPreviousFeed,
                                                 -1,
                                                 &folderId);
    } else {
      mLivemarkService->CreateLivemark(frame.mContainerID,
                                       frame.mPreviousText,
                                       frame.mPreviousLink,
                                       frame.mPreviousFeed,
                                       -1,
                                       &folderId);
    }
#ifdef DEBUG_IMPORT
    PrintNesting();
    printf("Creating livemark '%s'\n",
           NS_ConvertUTF16toUTF8(frame.mPreviousText).get());
#endif
  }
  else if (frame.mPreviousLink) {
#ifdef DEBUG_IMPORT
    PrintNesting();
    printf("Creating bookmark '%s'\n",
           NS_ConvertUTF16toUTF8(frame.mPreviousText).get());
#endif
    mBookmarksService->SetItemTitle(frame.mPreviousId, frame.mPreviousText);
  }
  frame.mPreviousText.Truncate(0);
}


// BookmarkContentSink::HandleSeparator
//
//    Inserts a separator into the current container
void
BookmarkContentSink::HandleSeparator()
{
  BookmarkImportFrame& frame = CurFrame();

  // bookmarks.html contains a separator between the toolbar menu and the
  // rest of the items.  Since we pull the toolbar menu out into the top level,
  // we want to skip over this separator since it looks out of place.
  if (frame.mLastContainerType != BookmarkImportFrame::Container_Toolbar) {
    // create the separator
#ifdef DEBUG_IMPORT
    PrintNesting();
    printf("--------\n");
#endif
    mBookmarksService->InsertSeparator(frame.mContainerID,
                                       mBookmarksService->DEFAULT_INDEX);
  }
}


// BookmarkContentSink::NewFrame
//
//    This is called when there is a new folder found. The folder takes the
//    name from the previous frame's heading.

nsresult
BookmarkContentSink::NewFrame()
{
  nsresult rv;

  nsString containerName;
  BookmarkImportFrame::ContainerType containerType;
  CurFrame().ConsumeHeading(&containerName, &containerType);

  PRBool updateFolder = PR_FALSE;
  PRInt64 ourID = 0;
  switch (containerType) {
    case BookmarkImportFrame::Container_Normal:
      // regular folder: use an existing folder if that name already exists
      rv = mBookmarksService->GetChildFolder(CurFrame().mContainerID,
                                             containerName, &ourID);
      NS_ENSURE_SUCCESS(rv, rv);
      if (! ourID) {
        // need to append a new folder
        rv = mBookmarksService->CreateFolder(CurFrame().mContainerID,
                                            containerName,
                                            mBookmarksService->DEFAULT_INDEX, &ourID);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      break;
    case BookmarkImportFrame::Container_Places:
      // places root, never reparent here, when we're building the initial
      // hierarchy, it will only be defined at the top level
      rv = mBookmarksService->GetPlacesRoot(&ourID);
      NS_ENSURE_SUCCESS(rv, rv);
      break;
    case BookmarkImportFrame::Container_Menu:
      // menu root
      rv = mBookmarksService->GetBookmarksRoot(&ourID);
      NS_ENSURE_SUCCESS(rv, rv);
      if (mAllowRootChanges) {
        updateFolder = PR_TRUE;
        SetFaviconForFolder(ourID, NS_LITERAL_CSTRING(BOOKMARKS_MENU_ICON_URI));
      }
      break;
    case BookmarkImportFrame::Container_Toolbar:
      // get toolbar folder
      PRInt64 btf;
      rv = mBookmarksService->GetToolbarFolder(&btf);
      NS_ENSURE_SUCCESS(rv, rv);
      if (!btf) {
        // create new folder
        rv = mBookmarksService->CreateFolder(CurFrame().mContainerID,
                                            containerName,
                                            mBookmarksService->DEFAULT_INDEX, &ourID);
        NS_ENSURE_SUCCESS(rv, rv);
        // there's no toolbar folder, so make us the toolbar folder
        rv = mBookmarksService->SetToolbarFolder(ourID);
        NS_ENSURE_SUCCESS(rv, rv);
        // set favicon
        SetFaviconForFolder(ourID, NS_LITERAL_CSTRING(BOOKMARKS_TOOLBAR_ICON_URI));
      }
      else {
        ourID = btf;
      }
      break;
    default:
      NS_NOTREACHED("Unknown container type");
  }
#ifdef DEBUG_IMPORT
  PrintNesting();
  printf("Folder %lld \'%s\'", ourID, NS_ConvertUTF16toUTF8(containerName).get());
#endif

  if (updateFolder) {
    // move the menu folder to the current position
    mBookmarksService->MoveFolder(ourID, CurFrame().mContainerID, -1);
    mBookmarksService->SetFolderTitle(ourID, containerName);
#ifdef DEBUG_IMPORT
    printf(" [reparenting]");
#endif
  }

#ifdef DEBUG_IMPORT
  printf("\n");
#endif

  if (! mFrames.AppendElement(BookmarkImportFrame(ourID)))
    return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}


// BookmarkContentSink::PopFrame
//

nsresult
BookmarkContentSink::PopFrame()
{
  // we must always have one frame
  if (mFrames.Length() <= 1) {
    NS_NOTREACHED("Trying to complete more bookmark folders than you started");
    return NS_ERROR_FAILURE;
  }
  mFrames.RemoveElementAt(mFrames.Length() - 1);
  return NS_OK;
}


// BookmarkContentSink::SetFaviconForURI
//
//    aData is a string that is a data URI for the favicon. Our job is to
//    decode it and store it in the favicon service.
//
//    When aIconURI is non-null, we will use that as the URI of the favicon
//    when storing in the favicon service.
//
//    When aIconURI is null, we have to make up a URI for this favicon so that
//    it can be stored in the service. The real one will be set the next time
//    the user visits the page. Our made up one should get expired when the
//    page no longer references it.

nsresult
BookmarkContentSink::SetFaviconForURI(nsIURI* aPageURI, nsIURI* aIconURI,
                                      const nsCString& aData)
{
  nsresult rv;
  static PRUint32 serialNumber = 0; // for made-up favicon URIs

  nsFaviconService* faviconService = nsFaviconService::GetFaviconService();
  if (! faviconService)
    return NS_ERROR_NO_INTERFACE;

  // if the input favicon URI is a chrome: URI, then we just save it and don't
  // worry about data
  if (aIconURI) {
    nsCString faviconScheme;
    aIconURI->GetScheme(faviconScheme);
    if (faviconScheme.EqualsLiteral("chrome")) {
      return faviconService->SetFaviconUrlForPage(aPageURI, aIconURI);
    }
  }

  // some bookmarks have placeholder URIs that contain just "data:"
  // ignore these
  if (aData.Length() <= 5)
    return NS_OK;

  nsCOMPtr<nsIURI> faviconURI;
  if (aIconURI) {
    faviconURI = aIconURI;
  } else {
    // make up favicon URL
    nsCAutoString faviconSpec;
    faviconSpec.AssignLiteral("http://www.mozilla.org/2005/made-up-favicon/");
    faviconSpec.AppendInt(serialNumber);
    faviconSpec.AppendLiteral("-");
    faviconSpec.AppendInt(PR_Now());
    rv = NS_NewURI(getter_AddRefs(faviconURI), faviconSpec);
    NS_ENSURE_SUCCESS(rv, rv);
    serialNumber ++;
  }

  nsCOMPtr<nsIURI> dataURI;
  rv = NS_NewURI(getter_AddRefs(dataURI), aData);
  NS_ENSURE_SUCCESS(rv, rv);

  // use the data: protocol handler to convert the data
  nsCOMPtr<nsIIOService> ioService = do_GetIOService(&rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIProtocolHandler> protocolHandler;
  rv = ioService->GetProtocolHandler("data", getter_AddRefs(protocolHandler));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIChannel> channel;
  rv = protocolHandler->NewChannel(dataURI, getter_AddRefs(channel));
  NS_ENSURE_SUCCESS(rv, rv);

  // blocking stream is OK for data URIs
  nsCOMPtr<nsIInputStream> stream;
  rv = channel->Open(getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 available;
  rv = stream->Available(&available);
  NS_ENSURE_SUCCESS(rv, rv);
  if (available == 0)
    return NS_ERROR_FAILURE;

  // read all the decoded data
  PRUint8* buffer = NS_STATIC_CAST(PRUint8*,
                                   nsMemory::Alloc(sizeof(PRUint8) * available));
  if (! buffer)
    return NS_ERROR_OUT_OF_MEMORY;
  PRUint32 numRead;
  rv = stream->Read(NS_REINTERPRET_CAST(char*, buffer), available, &numRead);
  if (NS_FAILED(rv) || numRead != available) {
    nsMemory::Free(buffer);
    return rv;
  }

  nsCAutoString mimeType;
  rv = channel->GetContentType(mimeType);
  NS_ENSURE_SUCCESS(rv, rv);

  // save in service
  rv = faviconService->SetFaviconData(faviconURI, buffer, available, mimeType, 0);
  nsMemory::Free(buffer);
  NS_ENSURE_SUCCESS(rv, rv);
  return faviconService->SetFaviconUrlForPage(aPageURI, faviconURI);
}


// BookmarkContentSink::SetFaviconForFolder
//
//    This sets the given favicon URI for the given folder. It is used to
//    initialize the favicons for the bookmarks menu and toolbar. We don't
//    actually set any data here because we assume the URI is a chrome: URI.
//    These do not have to contain any data for them to work.

nsresult
BookmarkContentSink::SetFaviconForFolder(PRInt64 aFolder,
                                         const nsACString& aFavicon)
{
  nsFaviconService* faviconService = nsFaviconService::GetFaviconService();
  NS_ENSURE_TRUE(faviconService, NS_ERROR_OUT_OF_MEMORY);

  nsCOMPtr<nsIURI> folderURI;
  nsresult rv = mBookmarksService->GetFolderURI(aFolder,
                                                getter_AddRefs(folderURI));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> faviconURI;
  rv = NS_NewURI(getter_AddRefs(faviconURI), aFavicon);
  NS_ENSURE_SUCCESS(rv, rv);

  return faviconService->SetFaviconUrlForPage(folderURI, faviconURI);
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


// nsNavBookmarks::ImportBookmarksHTML

NS_IMETHODIMP
nsNavBookmarks::ImportBookmarksHTML(nsIURI* aURL)
{
  // this version is exposed on the interface and disallows changing of roots
  return ImportBookmarksHTMLInternal(aURL, PR_FALSE, 0, PR_FALSE);
}

NS_IMETHODIMP
nsNavBookmarks::ImportBookmarksHTMLToFolder(nsIURI* aURL, PRInt64 aFolder)
{
  // this version is exposed on the interface and disallows changing of roots
  return ImportBookmarksHTMLInternal(aURL, PR_FALSE, aFolder, PR_FALSE);
}

nsresult
nsNavBookmarks::ImportBookmarksHTMLInternal(nsIURI* aURL,
                                            PRBool aAllowRootChanges,
                                            PRInt64 aFolder,
                                            PRBool aIsImportDefaults)
{
  // wrap the import in a transaction to make it faster
  mozStorageTransaction transaction(DBConn(), PR_FALSE);

#ifdef DEBUG_IMPORT
  nsCAutoString spec;
  aURL->GetSpec(spec);
  printf("\nImporting %s\n", spec.get());
#endif

  nsresult rv;
  nsCOMPtr<nsIParser> parser = do_CreateInstance(kParserCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<BookmarkContentSink> sink = new BookmarkContentSink;
  NS_ENSURE_TRUE(sink, NS_ERROR_OUT_OF_MEMORY);
  rv = sink->Init(aAllowRootChanges, this, aFolder, aIsImportDefaults);
  NS_ENSURE_SUCCESS(rv, rv);
  parser->SetContentSink(sink);

  // channel: note we have to set the content type or the default "unknown" type
  // will confuse the parser
  nsCOMPtr<nsIIOService> ioservice = do_GetIOService(&rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIChannel> channel;
  rv = ioservice->NewChannelFromURI(aURL, getter_AddRefs(channel));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = channel->SetContentType(NS_LITERAL_CSTRING("text/html"));
  NS_ENSURE_SUCCESS(rv, rv);

  // streams
  nsCOMPtr<nsIInputStream> stream;
  rv = channel->Open(getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIInputStream> bufferedstream;
  rv = NS_NewBufferedInputStream(getter_AddRefs(bufferedstream), stream, 4096);
  NS_ENSURE_SUCCESS(rv, rv);

  // init parser
#ifdef MOZILLA_1_8_BRANCH
  rv = parser->Parse(aURL, nsnull, PR_FALSE);
#else
  rv = parser->Parse(aURL, nsnull);
#endif
  NS_ENSURE_SUCCESS(rv, rv);

  // feed the parser the data
  // Note: on error, we always need to set the channel's status to be the
  // same, and to always call OnStopRequest with the channel error.
  nsCOMPtr<nsIStreamListener> listener = do_QueryInterface(parser, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = listener->OnStartRequest(channel, nsnull);
  rv = SyncChannelStatus(channel, rv);
  while(NS_SUCCEEDED(rv))
  {
    PRUint32 available;
    rv = bufferedstream->Available(&available);
    if (rv == NS_BASE_STREAM_CLOSED) {
      rv = NS_OK;
      available = 0;
    }
    if (NS_FAILED(rv)) {
      channel->Cancel(rv);
      break;
    }
    if (! available)
      break; // blocking input stream has none available when done

    rv = listener->OnDataAvailable(channel, nsnull, bufferedstream, 0, available);
    rv = SyncChannelStatus(channel, rv);
    if (NS_FAILED(rv))
      break;
  }
  listener->OnStopRequest(channel, nsnull, rv);
  transaction.Commit();
  return NS_OK;
}

static char kFileIntro[] =
    "<!DOCTYPE NETSCAPE-Bookmark-file-1>" NS_LINEBREAK
    // Note: we write bookmarks in UTF-8
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
static const char kSeparator[] = "<HR>" NS_LINEBREAK;
static const char kQuoteStr[] = "\"";
static const char kCloseAngle[] = ">";
static const char kIndent[] = "    ";

static const char kPlacesRootAttribute[] = " PLACES_ROOT=\"true\"";
static const char kBookmarksRootAttribute[] = " BOOKMARKS_MENU=\"true\"";
static const char kToolbarFolderAttribute[] = " PERSONAL_TOOLBAR_FOLDER=\"true\"";
static const char kIconAttribute[] = " ICON=\"";
static const char kIconURIAttribute[] = " ICON_URI=\"";
static const char kHrefAttribute[] = " HREF=\"";
static const char kFeedURIAttribute[] = " FEEDURL=\"";
static const char kWebPanelAttribute[] = " WEB_PANEL=\"true\"";

// WriteContainerPrologue
//
//    <DL><p>
//
//    Goes after the container header (<H3...) but before the contents

static nsresult
WriteContainerPrologue(const nsCString& aIndent, nsIOutputStream* aOutput)
{
  PRUint32 dummy;
  nsresult rv = aOutput->Write(aIndent.get(), aIndent.Length(), &dummy);
  if (NS_FAILED(rv)) return rv;
  rv = aOutput->Write(kBookmarkIntro, sizeof(kBookmarkIntro)-1, &dummy);
  if (NS_FAILED(rv)) return rv;
  return NS_OK;
}


// WriteContainerEpilogue
//
//    </DL><p>
//
//    Goes after the container contents to close the container

static nsresult
WriteContainerEpilogue(const nsCString& aIndent, nsIOutputStream* aOutput)
{
  PRUint32 dummy;
  nsresult rv = aOutput->Write(aIndent.get(), aIndent.Length(), &dummy);
  if (NS_FAILED(rv)) return rv;
  rv = aOutput->Write(kBookmarkClose, sizeof(kBookmarkClose)-1, &dummy);
  if (NS_FAILED(rv)) return rv;
  return NS_OK;
}


// DataToDataURI

static nsresult
DataToDataURI(PRUint8* aData, PRUint32 aDataLen, const nsACString& aMimeType,
              nsACString& aDataURI)
{
  char* encoded = PL_Base64Encode(NS_REINTERPRET_CAST(const char*, aData),
                                  aDataLen, nsnull);
  if (! encoded)
    return NS_ERROR_OUT_OF_MEMORY;

  aDataURI.AssignLiteral("data:");
  aDataURI.Append(aMimeType);
  aDataURI.AppendLiteral(";base64,");
  aDataURI.Append(encoded);

  nsMemory::Free(encoded);
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
  nsresult rv;
  PRUint32 dummy;

  nsFaviconService* faviconService = nsFaviconService::GetFaviconService();
  NS_ENSURE_TRUE(faviconService, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  // get favicon
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
  if (NS_FAILED(rv)) return rv;
  rv = WriteEscapedUrl(faviconSpec, aOutput);
  if (NS_FAILED(rv)) return rv;
  rv = aOutput->Write(kQuoteStr, sizeof(kQuoteStr)-1, &dummy);
  if (NS_FAILED(rv)) return rv;

  if (! faviconScheme.EqualsLiteral("chrome")) {
    // only store data for non-chrome URIs

    // get the data - BE SURE TO FREE
    nsCAutoString mimeType;
    PRUint32 dataLen;
    PRUint8* data;
    rv = faviconService->GetFaviconData(faviconURI, mimeType, &dataLen, &data);
    NS_ENSURE_SUCCESS(rv, rv);
    if (dataLen > 0) {
      // convert to URI
      nsCString faviconContents;
      rv = DataToDataURI(data, dataLen, mimeType, faviconContents);
      nsMemory::Free(data);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = aOutput->Write(kIconAttribute, sizeof(kIconAttribute)-1, &dummy);
      if (NS_FAILED(rv)) return rv;
      rv = aOutput->Write(faviconContents.get(), faviconContents.Length(), &dummy);
      if (NS_FAILED(rv)) return rv;
      rv = aOutput->Write(kQuoteStr, sizeof(kQuoteStr)-1, &dummy);
      if (NS_FAILED(rv)) return rv;
    }
  }
  return NS_OK;
}


// nsNavBookmarks::WriteContainer
//
//    Writes out all the necessary parts of a bookmarks folder.

nsresult
nsNavBookmarks::WriteContainer(PRInt64 aFolder, const nsCString& aIndent,
                               nsIOutputStream* aOutput)
{
  nsresult rv = WriteContainerHeader(aFolder, aIndent, aOutput);
  if (NS_FAILED(rv)) return rv;

  // FIXME bug 334758: write container description here as a <DD>

  rv = WriteContainerPrologue(aIndent, aOutput);
  if (NS_FAILED(rv)) return rv;
  rv = WriteContainerContents(aFolder, aIndent, aOutput);
  if (NS_FAILED(rv)) return rv;
  rv = WriteContainerEpilogue(aIndent, aOutput);
  if (NS_FAILED(rv)) return rv;
  return NS_OK;
}


// nsNavBookmarks::WriteContainerHeader
//
//    This writes '<DL><H3>Title</H3>'
//    Remember folders can also have favicons, which we put in the H3 tag

nsresult
nsNavBookmarks::WriteContainerHeader(PRInt64 aFolder, const nsCString& aIndent,
                                     nsIOutputStream* aOutput)
{
  PRUint32 dummy;
  nsresult rv;

  // indent
  if (! aIndent.IsEmpty()) {
    rv = aOutput->Write(aIndent.get(), aIndent.Length(), &dummy);
    if (NS_FAILED(rv)) return rv;
  }

  // "<DL H3"
  rv = aOutput->Write(kContainerIntro, sizeof(kContainerIntro)-1, &dummy);
  if (NS_FAILED(rv)) return rv;

  // " PERSONAL_TOOLBAR_FOLDER="true"", etc.
  if (aFolder == mRoot) {
    rv = aOutput->Write(kPlacesRootAttribute, sizeof(kPlacesRootAttribute)-1, &dummy);
    if (NS_FAILED(rv)) return rv;
  } else if (aFolder == mBookmarksRoot) {
    rv = aOutput->Write(kBookmarksRootAttribute, sizeof(kBookmarksRootAttribute)-1, &dummy);
    if (NS_FAILED(rv)) return rv;
  } else if (aFolder == mToolbarFolder) {
    rv = aOutput->Write(kToolbarFolderAttribute, sizeof(kToolbarFolderAttribute)-1, &dummy);
    if (NS_FAILED(rv)) return rv;
  }

  // favicon (most folders won't have one)
  nsCOMPtr<nsIURI> folderURI;
  rv = GetFolderURI(aFolder, getter_AddRefs(folderURI));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCAutoString folderSpec;
  rv = folderURI->GetSpec(folderSpec);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = WriteFaviconAttribute(folderSpec, aOutput);
  if (NS_FAILED(rv)) return rv;

  // ">"
  rv = aOutput->Write(kCloseAngle, sizeof(kCloseAngle)-1, &dummy);
  if (NS_FAILED(rv)) return rv;

  // title
  rv = WriteContainerTitle(aFolder, aOutput);
  if (NS_FAILED(rv)) return rv;

  // "</H3>\n"
  rv = aOutput->Write(kContainerClose, sizeof(kContainerClose)-1, &dummy);
  return rv;
}


// nsNavBookmarks::WriteContainerTitle
//
//    Retrieves, escaped and writes the container title to the stream.

nsresult
nsNavBookmarks::WriteContainerTitle(PRInt64 aFolder, nsIOutputStream* aOutput)
{
  nsAutoString title;
  nsresult rv = GetFolderTitle(aFolder, title);
  NS_ENSURE_SUCCESS(rv, rv);

  char* escapedTitle = nsEscapeHTML(NS_ConvertUTF16toUTF8(title).get());
  if (escapedTitle) {
    PRUint32 dummy;
    rv = aOutput->Write(escapedTitle, strlen(escapedTitle), &dummy);
    nsMemory::Free(escapedTitle);
    if (NS_FAILED(rv)) return rv;
  }
  return NS_OK;
}


// nsNavBookmarks::WriteItem
//
//    "<DT><A HREF="..." ICON="...">Name</A>"

nsresult
nsNavBookmarks::WriteItem(nsNavHistoryResultNode* aItem,
                          const nsCString& aIndent,
                          nsIOutputStream* aOutput)
{
  PRUint32 dummy;
  nsresult rv;

  // indent
  if (! aIndent.IsEmpty()) {
    rv = aOutput->Write(aIndent.get(), aIndent.Length(), &dummy);
    if (NS_FAILED(rv)) return rv;
  }

  // '<DT><A'
  rv = aOutput->Write(kItemOpen, sizeof(kItemOpen)-1, &dummy);
  if (NS_FAILED(rv)) return rv;

  // ' HREF="http://..."' - note that we need to call GetURI on the result
  // node because some nodes (eg queries) generate this lazily.
  rv = aOutput->Write(kHrefAttribute, sizeof(kHrefAttribute)-1, &dummy);
  if (NS_FAILED(rv)) return rv;
  nsCAutoString uri;
  rv = aItem->GetUri(uri);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = WriteEscapedUrl(uri, aOutput);
  if (NS_FAILED(rv)) return rv;
  rv = aOutput->Write(kQuoteStr, sizeof(kQuoteStr)-1, &dummy);
  if (NS_FAILED(rv)) return rv;

  // ' ICON="..."'
  rv = WriteFaviconAttribute(uri, aOutput);
  if (NS_FAILED(rv)) return rv;

  // annotations
  PRInt64 bookmarkId;
  rv = aItem->GetBookmarkId(&bookmarkId);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> placeURI;
  rv = GetItemURI(bookmarkId, getter_AddRefs(placeURI));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAnnotationService* annosvc = nsAnnotationService::GetAnnotationService();
  NS_ENSURE_TRUE(annosvc, NS_ERROR_OUT_OF_MEMORY);

  // Write WEB_PANEL="true" if the load-in-sidebar annotation is set for the
  // item
  PRBool loadInSidebar = PR_FALSE;
  rv = annosvc->HasAnnotation(placeURI, LOAD_IN_SIDEBAR_ANNO, &loadInSidebar);
  NS_ENSURE_SUCCESS(rv, rv);
  if (loadInSidebar)
    aOutput->Write(kWebPanelAttribute, sizeof(kWebPanelAttribute)-1, &dummy);

  // FIXME bug 334408: write last character set here

  // '>'
  rv = aOutput->Write(kCloseAngle, sizeof(kCloseAngle)-1, &dummy);
  if (NS_FAILED(rv)) return rv;

  // title
  char* escapedTitle = nsEscapeHTML(aItem->mTitle.get());
  if (escapedTitle) {
    rv = aOutput->Write(escapedTitle, strlen(escapedTitle), &dummy);
    nsMemory::Free(escapedTitle);
    if (NS_FAILED(rv)) return rv;
  }

  // '</A>\n'
  rv = aOutput->Write(kItemClose, sizeof(kItemClose)-1, &dummy);
  if (NS_FAILED(rv)) return rv;

  // FIXME bug 334758: write item description here as a <DD>

  return NS_OK;
}


// WriteLivemark
//
//    Similar to WriteItem, this has an additional FEEDURL attribute and
//    the HREF is optional and points to the source page.

nsresult
nsNavBookmarks::WriteLivemark(PRInt64 aFolderId, const nsCString& aIndent,
                              nsIOutputStream* aOutput)
{
  PRUint32 dummy;
  nsresult rv;

  nsCOMPtr<nsILivemarkService> livemarkService(do_GetService(NS_LIVEMARKSERVICE_CONTRACTID));
  NS_ENSURE_TRUE(livemarkService, NS_ERROR_UNEXPECTED);

  // indent
  if (! aIndent.IsEmpty()) {
    rv = aOutput->Write(aIndent.get(), aIndent.Length(), &dummy);
    if (NS_FAILED(rv)) return rv;
  }

  // '<DT><A'
  rv = aOutput->Write(kItemOpen, sizeof(kItemOpen)-1, &dummy);
  if (NS_FAILED(rv)) return rv;

  // get feed URI
  nsCOMPtr<nsIURI> feedURI;
  rv = livemarkService->GetFeedURI(aFolderId, getter_AddRefs(feedURI));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCString feedSpec;
  rv = feedURI->GetSpec(feedSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  // write feed URI
  rv = aOutput->Write(kFeedURIAttribute, sizeof(kFeedURIAttribute)-1, &dummy);
  if (NS_FAILED(rv)) return rv;
  rv = WriteEscapedUrl(feedSpec, aOutput);
  if (NS_FAILED(rv)) return rv;
  rv = aOutput->Write(kQuoteStr, sizeof(kQuoteStr)-1, &dummy);
  if (NS_FAILED(rv)) return rv;

  // get the optional site URI
  nsCOMPtr<nsIURI> siteURI;
  rv = livemarkService->GetSiteURI(aFolderId, getter_AddRefs(siteURI));
  NS_ENSURE_SUCCESS(rv, rv);
  if (siteURI) {
    nsCString siteSpec;
    rv = siteURI->GetSpec(siteSpec);
    NS_ENSURE_SUCCESS(rv, rv);

    // write site URI
    rv = aOutput->Write(kHrefAttribute, sizeof(kHrefAttribute)-1, &dummy);
    if (NS_FAILED(rv)) return rv;
    rv = WriteEscapedUrl(siteSpec, aOutput);
    if (NS_FAILED(rv)) return rv;
    rv = aOutput->Write(kQuoteStr, sizeof(kQuoteStr)-1, &dummy);
    if (NS_FAILED(rv)) return rv;
  }

  // '>'
  rv = aOutput->Write(kCloseAngle, sizeof(kCloseAngle)-1, &dummy);
  if (NS_FAILED(rv)) return rv;

  // title
  rv = WriteContainerTitle(aFolderId, aOutput);
  if (NS_FAILED(rv)) return rv;

  // '</A>\n'
  rv = aOutput->Write(kItemClose, sizeof(kItemClose)-1, &dummy);
  if (NS_FAILED(rv)) return rv;

  return NS_OK;
}


// WriteSeparator
//
//    "<HR>"

nsresult
WriteSeparator(const nsCString& aIndent, nsIOutputStream* aOutput)
{
  PRUint32 dummy;
  nsresult rv;

  // indent
  if (! aIndent.IsEmpty()) {
    rv = aOutput->Write(aIndent.get(), aIndent.Length(), &dummy);
    if (NS_FAILED(rv)) return rv;
  }

  rv = aOutput->Write(kSeparator, sizeof(kSeparator)-1, &dummy);
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


// nsNavBookmarks::WriteContainerContents
//
//    The indent here is the indent of the parent. We will add an additional
//    indent before writing data.

nsresult
nsNavBookmarks::WriteContainerContents(PRInt64 aFolder, const nsCString& aIndent,
                                       nsIOutputStream* aOutput)
{
  nsCString myIndent = aIndent;
  myIndent.Append(kIndent);

  nsNavHistory* historyService = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(historyService, NS_ERROR_UNEXPECTED);

  // get empty options
  nsCOMPtr<nsINavHistoryQueryOptions> optionsInterface;
  nsresult rv = historyService->GetNewQueryOptions(getter_AddRefs(optionsInterface));
  NS_ENSURE_SUCCESS(rv, rv);

  // QueryFolderChildren requires a concrete options
  nsCOMPtr<nsNavHistoryQueryOptions> options = do_QueryInterface(optionsInterface);
  NS_ENSURE_TRUE(options, NS_ERROR_UNEXPECTED);

  // get contents of folder
  nsCOMArray<nsNavHistoryResultNode> items;
  rv = QueryFolderChildren(aFolder, options, &items);
  NS_ENSURE_SUCCESS(rv, rv);

  // write each item
  for (PRInt32 i = 0; i < items.Count(); i ++) {
    if (items[i]->IsFolder()) {
      // bookmarks folder
      PRInt64 folderId = items[i]->GetAsFolder()->mFolderId;
      if (aFolder == mRoot && (folderId == mToolbarFolder ||
                               folderId == mBookmarksRoot)) {
        // don't write out the bookmarks menu folder from the
        // places root. When writing to bookmarks.html, it is reparented
        // to the menu, which is the root of the namespace. This provides
        // better backwards compatability.
        continue;
      }

      // it could be a regular folder or it could be a livemark
      nsCOMPtr<nsILivemarkService> livemarkService(do_GetService(NS_LIVEMARKSERVICE_CONTRACTID));
      NS_ENSURE_TRUE(livemarkService, NS_ERROR_UNEXPECTED);
      PRBool isLivemark;
      rv = livemarkService->IsLivemark(folderId, &isLivemark);
      NS_ENSURE_SUCCESS(rv, rv);

      if (isLivemark)
        rv = WriteLivemark(folderId, myIndent, aOutput);
      else
        rv = WriteContainer(folderId, myIndent, aOutput);
    } else if (items[i]->IsSeparator()) {
      rv = WriteSeparator(myIndent, aOutput);
    } else {
      rv = WriteItem(items[i], myIndent, aOutput);
    }
    if (NS_FAILED(rv)) return rv;
  }
  return NS_OK;
}


// nsNavBookmarks::ExportBookmarksHTML
//

NS_IMETHODIMP
nsNavBookmarks::ExportBookmarksHTML(nsIFile* aBookmarksFile)
{
  if (! aBookmarksFile)
    return NS_ERROR_NULL_POINTER;

  // get a safe output stream, so we don't clobber the bookmarks file unless
  // all the writes succeeded.
  nsCOMPtr<nsIOutputStream> out;
  nsresult rv = NS_NewSafeLocalFileOutputStream(getter_AddRefs(out),
                                                aBookmarksFile,
                                                PR_WRONLY | PR_CREATE_FILE,
                                                /*octal*/ 0600,
                                                0);
  if (NS_FAILED(rv)) return rv;

  // We need a buffered output stream for performance.
  // See bug 202477.
  nsCOMPtr<nsIOutputStream> strm;
  rv = NS_NewBufferedOutputStream(getter_AddRefs(strm), out, 4096);
  if (NS_FAILED(rv)) return rv;

  // file header
  PRUint32 dummy;
  rv = strm->Write(kFileIntro, sizeof(kFileIntro)-1, &dummy);
  if (NS_FAILED(rv)) return rv;

  // '<H1'
  rv = strm->Write(kRootIntro, sizeof(kRootIntro)-1, &dummy); // <H1
  if (NS_FAILED(rv)) return rv;

  // bookmarks menu favicon
  nsCOMPtr<nsIURI> folderURI;
  rv = GetFolderURI(mBookmarksRoot, getter_AddRefs(folderURI));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCAutoString folderSpec;
  rv = folderURI->GetSpec(folderSpec);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = WriteFaviconAttribute(folderSpec, strm);
  if (NS_FAILED(rv)) return rv;

  // '>Bookmarks</H1>
  rv = strm->Write(kCloseAngle, sizeof(kCloseAngle)-1, &dummy); // >
  if (NS_FAILED(rv)) return rv;
  rv = WriteContainerTitle(mBookmarksRoot, strm);
  if (NS_FAILED(rv)) return rv;
  rv = strm->Write(kCloseRootH1, sizeof(kCloseRootH1)-1, &dummy); // </H1>
  if (NS_FAILED(rv)) return rv;

  // prologue
  rv = WriteContainerPrologue(EmptyCString(), strm);
  if (NS_FAILED(rv)) return rv;

  // indents
  nsCAutoString indent;
  indent.Assign(kIndent);

  // places root
  rv = WriteContainer(mRoot, indent, strm);
  if (NS_FAILED(rv)) return rv;

  // bookmarks menu contents
  rv = WriteContainerContents(mBookmarksRoot, EmptyCString(), strm);
  if (NS_FAILED(rv)) return rv;

  // epilogue
  rv = WriteContainerEpilogue(EmptyCString(), strm);
  if (NS_FAILED(rv)) return rv;

  // commit the write
  nsCOMPtr<nsISafeOutputStream> safeStream = do_QueryInterface(strm, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  return safeStream->Finish();
}
