/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Ben Goodger <ben@netscape.com> (Original Author, v2.0)
 *   Peter Annema <disttsc@bart.nl>
 *   Blake Ross   <blakeross@telocity.com>
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
  gBookmarksShell = new BookmarksWindowTree (bookmarksTree.database);

  // Set up the tree controller
  bookmarksTree.controllers.appendController(gBookmarksShell.controller);

  const windowNode = document.getElementById("bookmark-window");
  // If we've been opened with a parameter, root the tree on it.
  if ("arguments" in window && window.arguments[0]) {
    var uri = window.arguments[0];
    bookmarksTree.setAttribute("ref", uri);
    var title = "";
    if (uri.substring(0,5) == "find:") {
      title = gBookmarksShell.getLocaleString("search_results_title");
      // Update the windowtype so that future searches are directed 
      // there and the window is not re-used for bookmarks. 
      windowNode.setAttribute("windowtype", "bookmarks:searchresults");
    }
    else {
      const krNameArc = gBookmarksShell.RDF.GetResource(NC_NS + "Name");
      const krRoot = gBookmarksShell.RDF.GetResource(window.arguments[0]);
      var rName = gBookmarksShell.db.GetTarget(krRoot, krNameArc, true);
      title = rName.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
    }
    const titleString = gBookmarksShell.getLocaleString("window_title");
    windowNode.setAttribute("title", titleString.replace(/%folder_name%/gi, title));
  }
  else {
    var rootfoldername = gBookmarksShell.getLocaleString("bookmarks_root");
    const kProfileContractID = "@mozilla.org/profile/manager;1";
    const kProfileIID = Components.interfaces.nsIProfile;
    const kProfile = Components.classes[kProfileContractID].getService(kProfileIID);
    rootfoldername = rootfoldername.replace(/%user_name%/, kProfile.currentProfile);
    windowNode.setAttribute("title", rootfoldername);
  }

  // Update to the last sort.
  RefreshSort();

  var kids = document.getElementById("treechildren-bookmarks");
  if (kids.firstChild)
    bookmarksTree.selectItem(kids.firstChild);

  bookmarksTree.focus();
}

function Shutdown ()
{
  // Store current window position and size in window attributes (for persistence).
  var win = document.getElementById("bookmark-window");
  win.setAttribute("x", screenX);
  win.setAttribute("y", screenY);
  win.setAttribute("height", outerHeight);
  win.setAttribute("width", outerWidth);

  gBookmarksShell.flushDataSource();
}

////////////////////////////////////////////////////////////////////////////////
// Class representing the bookmarks window's tree. This subclasses BookmarksTree,
// which contains methods generic to any tree-like bookmarks UI representation.
// This class contains data specific to the tree in this window, e.g. number of
// clicks required to load a bookmark, which differs from other bookmarks tree
// implementations (such as sidebar).
function BookmarksWindowTree (aCompositeDataSource)
{
  // 'db' is used by the base class.
  this.db = aCompositeDataSource;
  this.id = "bookmarksTree";
}

BookmarksWindowTree.prototype = {
  __proto__: BookmarksTree.prototype,

  /////////////////////////////////////////////////////////////////////////////
  // Number of clicks to activate a bookmark.
  openClickCount: 2,

  /////////////////////////////////////////////////////////////////////////////
  // Open Bookmark in new window
  openNewWindow: true,

  /////////////////////////////////////////////////////////////////////////////
  // Selection change (clicks, key navigation) in the tree update the statusbar
  // with the current item's URL.
  treeSelect: function (aEvent)
  {
    document.commandDispatcher.updateCommands("tree-select");
    const kBookmarksTree = document.getElementById("bookmarksTree");
    const kStatusBar = document.getElementById("statusbar-text");
    var displayValue = "";
    if (kBookmarksTree.selectedItems.length == 1) {
      var currItem = kBookmarksTree.selectedItems[0]
      if (currItem.getAttribute("container") == "true") {
        const kRDFCContractID = "@mozilla.org/rdf/container;1";
        const kRDFCIID = Components.interfaces.nsIRDFContainer;
        const kRDFC = Components.classes[kRDFCContractID].getService(kRDFCIID);
        const krSrc = this.RDF.GetResource(NODE_ID(currItem));
        try {
          kRDFC.Init(this.db, krSrc);
          var count = kRDFC.GetCount();
          displayValue = gBookmarksShell.getLocaleString("status_foldercount");
          displayValue = displayValue.replace(/%num_items%/, count);
        }
        catch (e) {
        }
      }
      else {
        try {
          displayValue = LITERAL(this.db, currItem, NC_NS + "URL");
        }
        catch (e) {
          displayValue = "";
        }
      }
      if (displayValue.substring(0, 3) == "NC:")
        displayValue = "";
    }
    kStatusBar.label = displayValue;
  }

};

/*
Not used anymore but keeping code around in case it comes in handy for bookmarks button D&D
or personal toolbar D&D.

///////////////////////////////////////////////////////////////////////////////
// This will not work properly until drag & drop is asynchronous and allows
// events to be processed during a drag.
var fileButton = {
  menuIsOpen: { },

  onDragOver: function (aEvent, aFlavour, aDragSession)
  {
    const kMBO = Components.interfaces.nsIMenuBoxObject;
    var target = aEvent.target;
    if (!(target.id in fileButton.menuIsOpen))
      fileButton.menuIsOpen[target.id] = false;

    if (!fileButton.menuIsOpen[target.id] &&
        (target.localName == "menu" || target.localName == "menubutton")) {
      var mBO = target.boxObject.QueryInterface(kMBO);
      mBO.openMenu(true);
      fileButton.menuIsOpen[target.id] = true;
    }
    if (target.localName == "menu" || target.localName == "menuitem") {
      var parentMenu = target.parentNode.parentNode;
      var parentBO = parentMenu.boxObject.QueryInterface(kMBO);
      parentBO.activeChild = target;
    }
  },

  onPopupDestroy: function (aEvent)
  {
    fileButton.menuIsOpen[aEvent.target.parentNode.id] = false;
  },

  createPopupHeader: function (aEvent)
  {
    const kXULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
    const target = aEvent.target;
    var parentNode = target.localName == "menupopup" ? target : target.firstChild;
    // Don't create a separator for an empty folder,
    if (target.localName == "menu") {
      parentNode = document.createElementNS(kXULNS, "menupopup");
      target.appendChild(parentNode);
      var bo = target.boxObject.QueryInterface(Components.interfaces.nsIMenuBoxObject);
      bo.openMenu(true);
    }
    if (parentNode.firstChild &&
        parentNode.firstChild.getAttribute("x-generated") == "true") {
      // clear out cruft.
      parentNode.removeChild(target.firstChild);
      parentNode.removeChild(target.firstChild);
    }
    var menuseparator = document.createElementNS(kXULNS, "menuseparator");
    menuseparator.setAttribute("x-generated", "true");
    parentNode.insertBefore(menuseparator, target.firstChild);
    var menuitem = document.createElementNS(kXULNS, "menuitem");
    const kRDF = gBookmarksShell.RDF;
    const kNameArc = kRDF.GetResource(NC_NS + "Name");
    var uri = NODE_ID(target.localName == "menupopup" ? target.parentNode : target);
    var nameVal;
    var str;
    if (uri == "NC:BookmarksRoot") {
      str = gBookmarksShell.getLocaleString("bookmarks_root")
      const kProfileContractID = "@mozilla.org/profile/manager;1";
      const kProfileIID = Components.interfaces.nsIProfile;
      const kProfile = Components.classes[kProfileContractID].getService(kProfileIID);
      nameVal = str.replace(/%user_name%/, kProfile.currentProfile);
    }
    else {
      const kSrc = kRDF.GetResource(uri);
      const kBMDS = kRDF.GetDataSource("rdf:bookmarks");
      nameVal = kBMDS.GetTarget(kSrc, kNameArc, true);
      nameVal = nameVal.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
    }
    str = gBookmarksShell.getLocaleString("file_in");
    str = str.replace(/%folder_name%/, nameVal);
    menuitem.setAttribute("folder-uri", uri);
    menuitem.setAttribute("label", str);
    menuitem.setAttribute("x-generated", "true");
    parentNode.insertBefore(menuitem, target.firstChild);
    menuitem.addEventListener("command", fileButton.fileBookmarks, false);
  },

  fileBookmarks: function (aEvent)
  {
    var bookmarksTree = document.getElementById("bookmarksTree");
    if (!bookmarksTree.selectedItems.length) return;
    var toFolderURI = aEvent.target.getAttribute("folder-uri");
    var additiveFlag = false;

    if ("beginBatch" in this)
      this.beginBatch();
    for (var i = 0; i < bookmarksTree.selectedItems.length; ++i) {
      var currItem = bookmarksTree.selectedItems[i];
      var currURI = NODE_ID(currItem);
      var parent = gBookmarksShell.findRDFNode(currItem, false);
      gBookmarksShell.moveBookmark(currURI, NODE_ID(parent), toFolderURI);
      gBookmarksShell.selectFolderItem(toFolderURI, currURI, additiveFlag);
      if (!additiveFlag) additiveFlag = true;
    }
    if ("endBatch" in this)
      this.endBatch();
    gBookmarksShell.flushDataSource();
  },

  _flavourSet: null,
  getSupportedFlavours: function ()
  {
    if (!this._flavourSet) {
      this._flavourSet = new FlavourSet();
      this._flavourSet.appendFlavour("moz/rdfitem");
    }
    return this._flavourSet;
  },

  canHandleMultipleItems: true,
  onDrop: function (aEvent, aXferData, aDragSession)
  {
    var dataList = aXferData.dataList;
    const kToFolderURI = aEvent.target.getAttribute("folder-uri");
    var additiveFlag = false;
    for (var i = 0; i < dataList.length; ++i) {
      var flavourData = dataList[i].first;
      var data = flavourData.data;
      var ix = data.indexOf("\n");
      var sourceID, parentID;
      sourceID = ix >= 0 ? (parentID = data.substr(ix+1), data.substr(0, ix)) : data;
      gBookmarksShell.moveBookmark(sourceID, parentID, kToFolderURI);
      gBookmarksShell.selectFolderItem(kToFolderURI, sourceID, additiveFlag);
      if (!additiveFlag) additiveFlag = true;
    }
    gBookmarksShell.flushDataSource();
    var menu = document.getElementById("file-bookmarks");
    var bo = menu.boxObject.QueryInterface(Components.interfaces.nsIMenuBoxObject);
    bo.openMenu(false);
    this.menuIsOpen = { };
  }
};

*/

// requires mailNavigatorOverlay.xul
function sendBookmarksLink()
{
  var selectedItem = document.getElementById('bookmarksTree').selectedItems[0];
  sendLink(LITERAL(gBookmarksShell.db, selectedItem, NC_NS + 'URL'),
           LITERAL(gBookmarksShell.db, selectedItem, NC_NS + 'Name'));
}

function updateSendLinkItem()
{
  var menuitem = document.getElementById("menu_sendLink");
  if (menuitem) {
    var selectedItems = document.getElementById("bookmarksTree").selectedItems;
    var command = document.getElementById("Browser:SendLink");
    if (selectedItems.length == 1 && selectedItems[0].getAttribute("type") != NC_NS + "Folder")
      command.removeAttribute("disabled");
    else
      command.setAttribute("disabled", "true");
  }
}
           