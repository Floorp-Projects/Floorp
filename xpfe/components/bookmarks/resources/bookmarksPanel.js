/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Ben Goodger <ben@netscape.com> (Original Author)
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
    // This is somewhat of a hack, and we'd like to parameterize this so that
    // eventually we can bookmark mail messages and editor documents.
    var contentArea = top.document.getElementById('content');
    if (contentArea)
      BookmarksUtils.addBookmarkForBrowser(contentArea.webNavigation, true);
  },
  
  manageBookmarks: function ()
  {
    openDialog("chrome://communicator/content/bookmarks/bookmarks.xul", "", "chrome,dialog=no,resizable=yes");
  },
  
  /////////////////////////////////////////////////////////////////////////////
  // This function only exists because we call onCommandUpdate manually in it,
  // because for some reason, the commandupdate handler on the commandupdater
  // is never being called. This is true of the bookmarks sidebar panel and the
  // personal toolbar.
  hlClick: function (aEvent)
  {
    // Command updater isn't working for us. Force a command update, beyotch. 
    this.controller.onCommandUpdate();
    this.treeClicked(aEvent);
  }

};

