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
 *   Ben Goodger <ben@netscape.com> (Original Author, v3.0)
 */

////////////////////////////////////////////////////////////////////////////////
// Initialize the command controllers, set focus, tree root, 
// window title state, etc. 
function Startup()
{
  var bookmarksView = document.getElementById("bookmarks-view");

  const windowNode = document.getElementById("bookmark-window");
  // If we've been opened with a parameter, root the tree on it.
  if ("arguments" in window && window.arguments[0]) {
    var uri = window.arguments[0];

    bookmarksView.tree.setAttribute("ref", uri);

    var title = "";
    if (uri.substring(0,5) == "find:") {
      title = bookmarksView._bundle.GetStringFromName("search_results_title");
      // Update the windowtype so that future searches are directed 
      // there and the window is not re-used for bookmarks. 
      windowNode.setAttribute("windowtype", "bookmarks:searchresults");
    }
    else {
      const krNameArc = bookmarksView.rdf.GetResource(NC_NS + "Name");
      const krRoot = bookmarksView.rdf.GetResource(window.arguments[0]);
      var rName = bookmarksView.db.GetTarget(krRoot, krNameArc, true);
      title = rName.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
    }
    const titleString = bookmarksView._bundle.GetStringFromName("window_title");
    windowNode.setAttribute("title", titleString.replace(/%folder_name%/gi, title));
  }
  else {
    var rootfoldername = bookmarksView._bundle.GetStringFromName("bookmarks_root");
    const kProfileContractID = "@mozilla.org/profile/manager;1";
    const kProfileIID = Components.interfaces.nsIProfile;
    const kProfile = Components.classes[kProfileContractID].getService(kProfileIID);
    rootfoldername = rootfoldername.replace(/%user_name%/, kProfile.currentProfile);
    windowNode.setAttribute("title", rootfoldername);
  }
 
  bookmarksView.treeBoxObject.selection.select(0);
  bookmarksView.tree.focus();
}

function Shutdown ()
{
  // Store current window position and size in window attributes (for persistence).
  var win = document.getElementById("bookmark-window");
  win.setAttribute("x", screenX);
  win.setAttribute("y", screenY);
  win.setAttribute("height", outerHeight);
  win.setAttribute("width", outerWidth);

  var bookmarksView = document.getElementById("bookmarks-view");
  bookmarksView.flushBMDatasource();
}

var gConstructedViewMenuSortItems = false;
function fillViewMenu(aEvent)
{
  var adjacentElement = document.getElementById("fill-before-this-node");
  var popupElement = aEvent.target;
  
  var bookmarksView = document.getElementById("bookmarks-view");
  var columns = bookmarksView.columns;

  if (!gConstructedViewMenuSortItems) {
    for (var i = 0; i < columns.length; ++i) {
      var name = columns[i].name;
      var accesskey = columns[i].accesskey;
      
      var menuitem = document.createElement("menuitem");
      var nameTemplate = bookmarksView._bundle.GetStringFromName("SortMenuItem");
      var name = nameTemplate.replace(/%NAME%/g, columns[i].label);
      menuitem.setAttribute("label", name);
      menuitem.setAttribute("accesskey", columns[i].accesskey);
      menuitem.setAttribute("resource", columns[i].resource);
      menuitem.setAttribute("id", "sortMenuItem:" + columns[i].resource);
      menuitem.setAttribute("checked", columns[i].sortActive);
      menuitem.setAttribute("name", "sortSet");
      menuitem.setAttribute("type", "radio");
      
      popupElement.insertBefore(menuitem, adjacentElement);
    }
    
    gConstructedViewMenuSortItems = true;
  }  

  const kPrefSvcContractID = "@mozilla.org/preferences;1";
  const kPrefSvcIID = Components.interfaces.nsIPrefService;
  var prefSvc = Components.classes[kPrefSvcContractID].getService(kPrefSvcIID);
  var bookmarksSortPrefs = prefSvc.getBranch("browser.bookmarks.sort.");

  if (gConstructedViewMenuSortItems) {
    var resource = bookmarksSortPrefs.getCharPref("resource");
    var element = document.getElementById("sortMenuItem:" + resource);
    if (element)
      element.setAttribute("checked", "true");
  }  

  var sortAscendingMenu = document.getElementById("ascending");
  var sortDescendingMenu = document.getElementById("descending");
  var noSortMenu = document.getElementById("natural");
  
  sortAscendingMenu.setAttribute("checked", "false");
  sortDescendingMenu.setAttribute("checked", "false");
  noSortMenu.setAttribute("checked", "false");
  var direction = bookmarksSortPrefs.getCharPref("direction");
  if (direction == "natural")
    sortAscendingMenu.setAttribute("checked", "true");
  else if (direction == "ascending") 
    sortDescendingMenu.setAttribute("checked", "true");
  else
    noSortMenu.setAttribute("checked", "true");
}

function onViewMenuSortItemSelected(aEvent)
{
  var resource = aEvent.target.getAttribute("resource");
  
  const kPrefSvcContractID = "@mozilla.org/preferences;1";
  const kPrefSvcIID = Components.interfaces.nsIPrefService;
  var prefSvc = Components.classes[kPrefSvcContractID].getService(kPrefSvcIID);
  var bookmarksSortPrefs = prefSvc.getBranch("browser.bookmarks.sort.");

  switch (resource) {
  case "":
    break;
  case "direction":
    var dirn = bookmarksSortPrefs.getCharPref("direction");
    if (aEvent.target.id == "ascending")
      bookmarksSortPrefs.setCharPref("direction", "natural");
    else if (aEvent.target.id == "descending")
      bookmarksSortPrefs.setCharPref("direction", "ascending");
    else
      bookmarksSortPrefs.setCharPref("direction", "descending");
    break;
  default:
    bookmarksSortPrefs.setCharPref("resource", resource);
    var direction = bookmarksSortPrefs.getCharPref("direction");
    if (direction == "descending")
      bookmarksSortPrefs.setCharPref("direction", "natural");
    break;
  }

  aEvent.preventCapture();
}  

var gConstructedColumnsMenuItems = false;
function fillColumnsMenu(aEvent) 
{
  var bookmarksView = document.getElementById("bookmarks-view");
  var columns = bookmarksView.columns;

  if (!gConstructedColumnsMenuItems) {
    for (var i = 0; i < columns.length; ++i) {
      var menuitem = document.createElement("menuitem");
      menuitem.setAttribute("label", columns[i].label);
      menuitem.setAttribute("resource", columns[i].resource);
      menuitem.setAttribute("id", "columnMenuItem:" + columns[i].resource);
      menuitem.setAttribute("type", "checkbox");
      menuitem.setAttribute("checked", columns[i].hidden != "true");
      aEvent.target.appendChild(menuitem);
    }

    gConstructedColumnsMenuItems = true;
  }
  else {
    for (var i = 0; i < columns.length; ++i) {
      var element = document.getElementById("columnMenuItem:" + columns[i].resource);
      if (element && columns[i].hidden != "true")
        element.setAttribute("checked", "true");
    }
  }
  
  aEvent.preventBubble();
}

function onViewMenuColumnItemSelected(aEvent)
{
  var resource = aEvent.target.getAttribute("resource");
  if (resource != "") {
    var bookmarksView = document.getElementById("bookmarks-view");
    bookmarksView.toggleColumnVisibility(resource);
  }  

  aEvent.preventBubble();
}

