/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Ben Goodger <ben@netscape.com> (Original Author)
 */


////////////////////////////////////////////////////////////////////////////////
// Get the two bookmarks utility libraries running, attach controllers, focus
// tree widget, etc. 
function Startup() 
{
  // Create the Bookmarks Shell
  var bookmarksTree = document.getElementById("bookmarksTree");
  gBookmarksShell = new BookmarksPanelTree (bookmarksTree.database);

  // Set up the tree controller
  bookmarksTree.controllers.appendController(gBookmarksShell.controller);

  // Update to the last sort.
  RefreshSort();
  
  bookmarksTree.focus();
}

////////////////////////////////////////////////////////////////////////////////
// Class representing the bookmarks panel's tree. This subclasses BookmarksTree, 
// which contains methods generic to any tree-like bookmarks UI representation. 
// This class contains data specific to the tree in this panel, e.g. number of 
// clicks required to load a bookmark, which differs from other bookmarks tree 
// implementations (such as window). 
function BookmarksPanelTree (aCompositeDataSource) 
{ 
  // 'db' is used by the base class. 
  this.db = aCompositeDataSource;
  this.id = "bookmarksTree";
}

BookmarksPanelTree.prototype = {
  __proto__: BookmarksTree.prototype,
  
  /////////////////////////////////////////////////////////////////////////////
  // Number of clicks to activate a bookmark. 
  openClickCount: 1,
  
  /////////////////////////////////////////////////////////////////////////////
  // Open Bookmark in most recent window
  openNewWindow: false,
  
  addBookmark: function ()
  {
    // This looks evil, you'd think we'd want to find the most recent NAVIGATOR
    // window and add a bookmark to the page loaded in that, but that's not the
    // case. In mail/news, we want to bookmark the current mail message and in
    // editor we want to bookmark the current document. 
    BookmarksUtils.addBookmarkToWindow(top._content);
  },
  
  manageBookmarks: function ()
  {
    openDialog("chrome://communicator/content/bookmarks/bookmarks.xul", "", "chrome,dialog=no,resizable=yes");
  }
  
};

