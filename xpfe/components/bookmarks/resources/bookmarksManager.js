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
  const windowNode = document.getElementById("bookmark-window");
  const bookmarksView = document.getElementById("bookmarks-view");

  var titleString;

  // If we've been opened with a parameter, root the tree on it.
  if ("arguments" in window && window.arguments[0]) {
    var title;
    var uri = window.arguments[0];
    bookmarksView.tree.setAttribute("ref", uri);
    document.getElementById("bookmarks-search").setAttribute("hidden","true")
    if (uri.substring(0,5) == "find:") {
      title = BookmarksUtils.getLocaleString("search_results_title");
      // Update the windowtype so that future searches are directed 
      // there and the window is not re-used for bookmarks. 
      windowNode.setAttribute("windowtype", "bookmarks:searchresults");
    }
    else
      title = BookmarksUtils.getProperty(window.arguments[0], NC_NS+"Name");
    
    titleString = BookmarksUtils.getLocaleString("window_title", title);
  }
  else {
    titleString = BookmarksUtils.getLocaleString("bookmarks_title", title);
    // always open the bookmark top root folder
    if (!bookmarksView.treeBoxObject.view.isContainerOpen(0))
      bookmarksView.treeBoxObject.view.toggleOpenState(0);
  }

  bookmarksView.treeBoxObject.view.selection.select(0);

  windowNode.setAttribute("title", titleString);

  document.getElementById("CommandUpdate_Bookmarks").setAttribute("commandupdater","true");
  bookmarksView.tree.focus();
}

function Shutdown()
{
  // Store current window position and size in window attributes (for persistence).
  var win = document.getElementById("bookmark-window");
  win.setAttribute("x", screenX);
  win.setAttribute("y", screenY);
  win.setAttribute("height", outerHeight);
  win.setAttribute("width", outerWidth);
}

var gConstructedColumnsMenuItems = false;
function fillColumnsMenu(aEvent) 
{
  var bookmarksView = document.getElementById("bookmarks-view");
  var columns = bookmarksView.columns;
  var i;

  if (!gConstructedColumnsMenuItems) {
    for (i = 0; i < columns.length; ++i) {
      var menuitem = document.createElement("menuitem");
      menuitem.setAttribute("label", columns[i].label);
      menuitem.setAttribute("colid", columns[i].id);
      menuitem.setAttribute("id", "columnMenuItem:" + columns[i].id);
      menuitem.setAttribute("type", "checkbox");
      menuitem.setAttribute("checked", columns[i].hidden != "true");
      //Disable Name column because you cannot hide it
      if (columns[i].id == "Name")
        menuitem.setAttribute("disabled", "true");
      aEvent.target.appendChild(menuitem);
    }

    gConstructedColumnsMenuItems = true;
  }
  else {
    for (i = 0; i < columns.length; ++i) {
      var element = document.getElementById("columnMenuItem:" + columns[i].id);
      if (element)
        if (columns[i].hidden == "true")
          element.setAttribute("checked", "false");
        else
          element.setAttribute("checked", "true");
    }
  }
  
  aEvent.preventBubble();
}

function onViewMenuColumnItemSelected(aEvent)
{
  var colid = aEvent.target.getAttribute("colid");
  if (colid != "") {
    var bookmarksView = document.getElementById("bookmarks-view");
    bookmarksView.toggleColumnVisibility(colid);
  }  

  aEvent.preventBubble();
}

function OpenBookmarksFile()
{
  const nsIFilePicker = Components.interfaces.nsIFilePicker;
  var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
  fp.init(window, BookmarksUtils.getLocaleString("SelectOpen"), nsIFilePicker.modeOpen);
  fp.appendFilters(nsIFilePicker.filterHTML);
  if (fp.show() == nsIFilePicker.returnOK)
    PREF.setCharPref("browser.bookmarks.file", fp.file.path);
}
