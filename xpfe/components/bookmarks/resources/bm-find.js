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
 *  Original Author(s):
 *    Robert John Churchill    <rjc@netscape.com>
 *
 * Contributor(s): 
 *    Blake Ross <blakeross@telocity.com>
 *    Peter Annema <disttsc@bart.nl>
 */

var gFindText;

function doLoad()
{
  gFindText = document.getElementById("findtext");

  // disable "Save Search" button (initially)
  var searchButton = document.getElementById("SaveSearch");
  searchButton.disabled = true;

  if ('arguments' in window && window.arguments[0]) {
    var datasourcePopup = document.getElementById("datasource");
    var winArg = window.arguments[0].toLowerCase();
    if (winArg == "bookmarks") {
      var searchBookmarksItem = document.getElementById("searchBookmarksItem");
      datasourcePopup.selectedItem = searchBookmarksItem;
    }
    else if (winArg == "history") {
      var searchHistoryItem = document.getElementById("searchHistoryItem");
      datasourcePopup.selectedItem = searchHistoryItem;
    }
    toggleItems(winArg);
  }
  else {
    toggleItems("bookmarks");  // default is bookmarks
  }
      
  doEnabling();

  //set initial focus
  gFindText.focus();
 
}

var gDatasourceName;
var gMatchName;
var gMethodName;
var gTextName;

function doFind()
{
  // get RDF datasource to query
  var datasourceNode = document.getElementById("datasource");
  if (!datasourceNode)
    return false;

  var datasource = datasourceNode.selectedItem.getAttribute("data");
  gDatasourceName = datasourceNode.selectedItem.getAttribute("value");

  // get match
  var matchNode = document.getElementById("match");
  if (!matchNode)
    return false;

  var match = matchNode.selectedItem.getAttribute("data");
  gMatchName = matchNode.selectedItem.getAttribute("value");

  // get method
  var methodNode = document.getElementById("method");
  if (!methodNode)
    return false;
  var method = methodNode.selectedItem.getAttribute("data");
  gMethodName = methodNode.selectedItem.getAttribute("value");

  // get user text to find
  if (!gFindText || !gFindText.value)
    return false;
 
  gTextName = gFindText.value;

  // construct find URL
  var url = "find:datasource=" + datasource;
  url += "&match=" + match;
  url += "&method=" + method;
  url += "&text=" + escape(gTextName);

  // load find URL into results pane
  var resultsTree = document.getElementById("findresultstree");
  resultsTree.setAttribute("ref", "");
  resultsTree.setAttribute("ref", url);

  // enable "Save Search" button
  var searchButton = document.getElementById("SaveSearch");
  searchButton.removeAttribute("disabled");

  resultsTree.focus();
  return true;
}

function saveFind()
{
  var resultsTree = document.getElementById("findresultstree");
  var searchURL = resultsTree.getAttribute("ref");
  if (!searchURL)
    return false;

  var searchTitle = "Find: " + gMatchName + " " + gMethodName + " '" + gTextName + "' in "  gDatasourceName;
  var bmks = Components.classes["@mozilla.org/browser/bookmarks-service;1"]
                       .getService()
                       .QueryInterface(Components.interfaces.nsIBookmarksService)
                       .AddBookmark(searchURL, searchTitle, bmks.BOOKMARK_FIND_TYPE, null);
 
  return true;
}

function getAbsoluteID(root, node)
{
  var url = node.getAttribute("ref");
  if (!url) 
    url = node.getAttribute("id");

  try {
    var rootNode = document.getElementById(root);
    var ds = null;
    if (rootNode)
      ds = rootNode.database;
	
    // add support for anonymous resources such as Internet Search results,
    // IE favorites under Win32, and NetPositive URLs under BeOS
    var rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"]
                        .getService()
                        .QueryInterface(Components.interfaces.nsIRDFService);
    if (rdf && ds) {
      var src = rdf.GetResource(url, true);
      var prop = rdf.GetResource("http://home.netscape.com/NC-rdf#URL", true);
      var target = ds.GetTarget(src, prop, true);
      target = target.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
      if (target)
        url = target;
    }
  }
  catch(ex) {
  }
  return url;
}

function doEnabling()
{
  var findButton = document.getElementById("search");
  findButton.disabled = !gFindText.value;
}

function OpenURL(event, node, root)
{
  if (event.button != 1 ||
      event.detail != 2 ||
      node.nodeName != "treeitem" ||
      node.getAttribute("container") == "true") {
    return false;
  }

  var url = getAbsoluteID(root, node);

  // Ignore "NC:" urls.
  if (url.substring(0, 3) == "NC:")
    return false;

  // get right sized window
  window.openDialog( getBrowserURL(), "_blank", "chrome,all,dialog=no", url );
  return true;
}

function itemSelect()
{
  var tree = document.getElementById("findresultstree");
  if (tree.selectedItems.length == 1) {
    var status = document.getElementById("statusbar-display");
    var val = getAbsoluteID("findresultstree", tree.selectedItems[0]);

    // Ignore "NC:" urls.
    if (val.substring(0, 3) == "NC:") 
      status.value = "";
    else
      status.value = val;
  }
}

function toggleItems(searchType)
{
  var bookmarksArray = ["matchShortcutItem", "shortcutCell", "shortcutCell2", "ShortcutURLColumn"];
  if (searchType == "bookmarks")
    showItems(bookmarksArray);
  else if (searchType == "history")
    hideItems(bookmarksArray);
}

function showItems(elementArray)
{
  for (var i = 0; i < elementArray.length; i++) {
    var element = document.getElementById(elementArray[i]);
    if (element)
      element.removeAttribute("collapsed");
  }
}

function hideItems(elementArray)
{
  for (var i = 0; i < elementArray.length; i++) {
    var element = document.getElementById(elementArray[i]);
    if (element)
      element.setAttribute("collapsed", "true");
  }
}
