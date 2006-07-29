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
 *   Simon Fraser <sfraser@netscape.com>
 */

// browser is the <browser> element
// rootSearchWindow is the window to constrain the search to (normally window._content)
// startSearchWindow is the frame to start searching (can be, and normally, rootSearchWindow)
function findInPage(browser, rootSearchWindow, startSearchWindow)
{
  var findInst = browser.webBrowserFind;
  // set up the find to search the focussedWindow, bounded by the content window.
  var findInFrames = findInst.QueryInterface(Components.interfaces.nsIWebBrowserFindInFrames);
  findInFrames.rootSearchFrame = rootSearchWindow;
  findInFrames.currentSearchFrame = startSearchWindow;

  // always search in frames for now. We could add a checkbox to the dialog for this.
  findInst.searchFrames = true;
  
  // is the dialog up already?
  if (window.findDialog)
    window.findDialog.focus();
  else
    window.findDialog = window.openDialog("chrome://global/content/finddialog.xul", "Find on Page", "chrome,resizable=no,dependent=yes", findInst);
}

function findAgainInPage(browser, rootSearchWindow, startSearchWindow)
{
  if (window.findDialog)
    window.findDialog.focus();
  else
  {
    var findInst = browser.webBrowserFind;
    // set up the find to search the focussedWindow, bounded by the content window.
    var findInFrames = findInst.QueryInterface(Components.interfaces.nsIWebBrowserFindInFrames);
    findInFrames.rootSearchFrame = rootSearchWindow;
    findInFrames.currentSearchFrame = startSearchWindow;

    // always search in frames for now. We could add a checkbox to the dialog for this.
    findInst.searchFrames = true;

    // get the find service, which stores global find state, and init the
    // nsIWebBrowser find with it. We don't assume that there was a previous
    // Find that set this up.
    var findService = Components.classes["@mozilla.org/find/find_service;1"]
                           .getService(Components.interfaces.nsIFindService);
    findInst.searchString  = findService.searchString;
    findInst.matchCase     = findService.matchCase;
    findInst.wrapFind      = findService.wrapFind;
    findInst.entireWord    = findService.entireWord;
    findInst.findBackwards = findService.findBackwards;

    var found = false;
    if (findInst.searchString.length > 0)   // should never happen if command updating works
        found = findInst.findNext();
  }
}

function canFindAgainInPage()
{
    var findService = Components.classes["@mozilla.org/find/find_service;1"]
                           .getService(Components.interfaces.nsIFindService);
    return (findService.searchString.length > 0);
}

