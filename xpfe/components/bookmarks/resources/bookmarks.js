/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *   Ben Goodger <ben@netscape.com> (Original Author, v2.0)
 *   Peter Annema <disttsc@bart.nl>
 *   Blake Ross   <blakeross@telocity.com>
 */

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
    bookmarksTree.setAttribute("ref", window.arguments[0]);
    const krNameArc = gBookmarksShell.RDF.GetResource(NC_NS + "Name");
    const krRoot = gBookmarksShell.RDF.GetResource(window.arguments[0]);
    var rName = gBookmarksShell.db.GetTarget(krRoot, krNameArc, true);
    rName = rName.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
    const titleString = gBookmarksShell.getLocaleString("window_title");
    windowNode.setAttribute("title", titleString.replace(/%folder_name%/gi, rName));
  }
  else {
    // There's a better way of doing this, but for the initial revision just
    // using the root folder in the bookmarks window will do. 
    // For the next milestone I'd like to move this either into the bookmarks 
    // service or some other higher level datasource so that it can be accessed
    // from everywhere. 
    const kRDF = gBookmarksShell.RDF;
    const krNavCenter = kRDF.GetResource("NC:NavCenter");
    const kRDFCUtilsContractID = "@mozilla.org/rdf/container-utils;1";
    const kRDFCUtilsIID = Components.interfaces.nsIRDFContainerUtils;
    const kRDFCUtils = Components.classes[kRDFCUtilsContractID].getService(kRDFCUtilsIID);
    const kNC_NavCenter = kRDFCUtils.MakeSeq(gBookmarksShell.db, krNavCenter);
    const krBookmarksRoot = kRDF.GetResource("NC:BookmarksRoot");
    const krName = kRDF.GetResource(NC_NS + "Name");
    var rootfoldername = gBookmarksShell.getLocaleString("bookmarks_root");
    const kProfileContractID = "@mozilla.org/profile/manager;1";
    const kProfileIID = Components.interfaces.nsIProfile;
    const kProfile = Components.classes[kProfileContractID].getService(kProfileIID);
    rootfoldername = rootfoldername.replace(/%user_name%/, kProfile.currentProfile);
    const klBookmarksRootName = kRDF.GetLiteral(rootfoldername);
    const kBMDS = kRDF.GetDataSource("rdf:bookmarks");
    kBMDS.Assert(krBookmarksRoot, krName, klBookmarksRootName, true);
    kNC_NavCenter.AppendElement(krBookmarksRoot);
    windowNode.setAttribute("title", rootfoldername);
    
    /* Blue Sky
    const krHistoryRoot = kRDF.GetResource("NC:HistoryRoot");
    const klHistoryRootName = kRDF.GetLiteral("Browsing History");
    const kHistory = kRDF.GetDataSource("rdf:history");
    kHistory.Assert(krHistoryRoot, krName, klHistoryRootName, true);
    kNC_NavCenter.AppendElement(krHistoryRoot);
    */    
    
    const krType = kRDF.GetResource(RDF_NS + "type");
    const krFolderType = kRDF.GetResource(NC_NS + "Folder");
    gBookmarksShell.db.Assert(krNavCenter, krType, krFolderType, true);
  }
  
  // Update to the last sort.
  RefreshSort();
  
  // Initialize the tree widget. 
  var children = document.getElementById("treechildren-bookmarks");
  if (children.firstChild) {
    bookmarksTree.selectItem(children.firstChild);
    children.firstChild.setAttribute("open", "true");
  }
  
  // XXX templates suck ASS
  var node = document.getElementById("NC:BookmarksRoot");
  if (node.localName == "menubutton")
    node.removeAttribute("open");
  
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
      else 
        displayValue = LITERAL(this.db, currItem, NC_NS + "URL");
      if (displayValue.substring(0, 3) == "NC:") 
        displayValue = "";
    }
    kStatusBar.setAttribute("value", displayValue);
  }
};

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
    if (uri == "NC:BookmarksRoot") {
      var str = gBookmarksShell.getLocaleString("bookmarks_root")
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
    var str = gBookmarksShell.getLocaleString("file_in");
    str = str.replace(/%folder_name%/, nameVal);
    menuitem.setAttribute("folder-uri", uri);
    menuitem.setAttribute("value", str);
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

