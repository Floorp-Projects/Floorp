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
 *   Ben Goodger <ben@netscape.com> (Original Author)
 */

var gOKButton;
var gSearchField;
function Startup()
{
  doSetOKCancel(find);
  var bundle = document.getElementById("bookmarksBundle");
  gOKButton = document.getElementById("ok");
  gOKButton.label = bundle.getString("search_button_label");
  gOKButton.disabled = true;
  gSearchField = document.getElementById("searchField");
  gSearchField.focus();
}

function find()
{
  // Build up a find URI from the search fields and open a new window
  // rooted on the URI. 
  var match = document.getElementById("matchList");
  var method = document.getElementById("methodList");
  var searchURI = "find:datasource=rdf:bookmarks"
  searchURI += "&match=" + match.selectedItem.value;
  searchURI += "&method=" + method.selectedItem.value;
  searchURI += "&text=" + escape(gSearchField.value);
  var bmWindow = findMostRecentWindow("bookmarks:searchresults", "chrome://communicator/content/bookmarks/bookmarks.xul", searchURI);
  
  // Update the root of the tree if we're using an existing search window. 
  if (!gCreatingNewWindow) 
    bmWindow.gBookmarksShell.setRoot(searchURI);

  bmWindow.focus();
  close();
}

var gCreatingNewWindow = false;
function findMostRecentWindow(aType, aURI, aParam)
{
  var WM = Components.classes['@mozilla.org/rdf/datasource;1?name=window-mediator'].getService();
  WM = WM.QueryInterface(Components.interfaces.nsIWindowMediator);
  var topWindow = WM.getMostRecentWindow(aType);
  if (!topWindow) gCreatingNewWindow = true;
  return topWindow || openDialog("chrome://communicator/content/bookmarks/bookmarks.xul", 
                                 "", "chrome,all,dialog=no", aParam);
}

function doEnabling()
{
  gOKButton.disabled = !gSearchField.value;
}