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

var gOKButton;
var gSearchField;
function Startup()
{
  doSetOKCancel(find);
  var bundle = document.getElementById("historyBundle");
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
  var searchURI = "find:datasource=history"
  searchURI += "&match=" + match.selectedItem.value;
  searchURI += "&method=" + method.selectedItem.value;
  searchURI += "&text=" + escape(gSearchField.value);
  var hstWindow = findMostRecentWindow("history:searchresults", "chrome://communicator/content/history/history.xul", searchURI);
  
  // Update the root of the tree if we're using an existing search window. 
  if (!gCreatingNewWindow) 
    hstWindow.setRoot(searchURI);

  hstWindow.focus();
  close();
}

var gCreatingNewWindow = false;
function findMostRecentWindow(aType, aURI, aParam)
{
  var WM = Components.classes['@mozilla.org/rdf/datasource;1?name=window-mediator'].getService();
  WM = WM.QueryInterface(Components.interfaces.nsIWindowMediator);
  var topWindow = WM.getMostRecentWindow(aType);
  if (!topWindow) gCreatingNewWindow = true;
  return topWindow || openDialog("chrome://communicator/content/history/history.xul", 
                                 "", "chrome,all,dialog=no", aParam);
}

function doEnabling()
{
  gOKButton.disabled = !gSearchField.value;
}
