/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Simon Fraser <sfraser@netscape.com>
 *   Dean Tessman <dean_tessman@hotmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

var gPromptService;
var gFindBundle;

function nsFindInstData() {}
nsFindInstData.prototype =
{
  // set the next three attributes on your object to override the defaults
  browser : null,

  get rootSearchWindow() { return this._root || this.window.content; },
  set rootSearchWindow(val) { this._root = val; },

  get currentSearchWindow() {
    if (this._current)
      return this._current;

    var focusedWindow = this.window.document.commandDispatcher.focusedWindow;
    if (!focusedWindow || focusedWindow == this.window)
      focusedWindow = this.window.content;

    return focusedWindow;
  },
  set currentSearchWindow(val) { this._current = val; },

  get webBrowserFind() { return this.browser.webBrowserFind; },

  init : function() {
    var findInst = this.webBrowserFind;
    // set up the find to search the focussedWindow, bounded by the content window.
    var findInFrames = findInst.QueryInterface(Components.interfaces.nsIWebBrowserFindInFrames);
    findInFrames.rootSearchFrame = this.rootSearchWindow;
    findInFrames.currentSearchFrame = this.currentSearchWindow;
  
    // always search in frames for now. We could add a checkbox to the dialog for this.
    findInst.searchFrames = true;
  },

  window : window,
  _root : null,
  _current : null
}

// browser is the <browser> element
// rootSearchWindow is the window to constrain the search to (normally window.content)
// currentSearchWindow is the frame to start searching (can be, and normally, rootSearchWindow)
function findInPage(findInstData)
{
  // is the dialog up already?
  if ("findDialog" in window && window.findDialog)
    window.findDialog.focus();
  else
  {
    findInstData.init();
    window.findDialog = window.openDialog("chrome://global/content/finddialog.xul", "_blank", "chrome,resizable=no,dependent=yes", findInstData);
  }
}

function findAgainInPage(findInstData, reverse)
{
  if ("findDialog" in window && window.findDialog)
    window.findDialog.focus();
  else
  {
    // get the find service, which stores global find state, and init the
    // nsIWebBrowser find with it. We don't assume that there was a previous
    // Find that set this up.
    var findService = Components.classes["@mozilla.org/find/find_service;1"]
                           .getService(Components.interfaces.nsIFindService);

    var searchString = findService.searchString;
    if (searchString.length == 0) {
      // no previous find text
      findInPage(findInstData);
      return;
    }

    findInstData.init();
    var findInst = findInstData.webBrowserFind;
    findInst.searchString  = searchString;
    findInst.matchCase     = findService.matchCase;
    findInst.wrapFind      = findService.wrapFind;
    findInst.entireWord    = findService.entireWord;
    findInst.findBackwards = findService.findBackwards ^ reverse;

    var found = findInst.findNext();
    if (!found) {
      if (!gPromptService)
        gPromptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService()
                                   .QueryInterface(Components.interfaces.nsIPromptService);                                     
      if (!gFindBundle)
        gFindBundle = document.getElementById("findBundle");
          
      gPromptService.alert(window, gFindBundle.getString("notFoundTitle"), gFindBundle.getString("notFoundWarning"));
    }      

    // Reset to normal value, otherwise setting can get changed in find dialog
    findInst.findBackwards = findService.findBackwards; 
  }
}

function canFindAgainInPage()
{
    var findService = Components.classes["@mozilla.org/find/find_service;1"]
                           .getService(Components.interfaces.nsIFindService);
    return (findService.searchString.length > 0);
}

