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

/**
 * Determine whether or not a given focused DOMWindow is in the content
 * area.
 **/
  function isDocumentFrame(aFocusedWindow)
  {
    var contentFrames = _content.frames;
    if (contentFrames.length) {
      for (var i = 0; i < contentFrames.length; ++i) {
        if (aFocusedWindow == contentFrames[i])
          return true;
      }
    }
    return false;
  }

  function urlSecurityCheck(url, doc) {
    // URL Loading Security Check
    var focusedWindow = doc.commandDispatcher.focusedWindow;
    var sourceWin = isDocumentFrame(focusedWindow) ? focusedWindow.location.href : focusedWindow._content.location.href;
    const nsIScriptSecurityManager = Components.interfaces.nsIScriptSecurityManager;
    var secMan = Components.classes["@mozilla.org/scriptsecuritymanager;1"].getService().
                     QueryInterface(nsIScriptSecurityManager);
    try {
      secMan.checkLoadURIStr(sourceWin, url, nsIScriptSecurityManager.STANDARD);
    } catch (e) {
       throw "Load of " + url + " denied.";
    }
  }

  function openNewWindowWith(url) {

    urlSecurityCheck(url, document);
    var newWin;
    var wintype = document.firstChild.getAttribute('windowtype');

    // if and only if the current window is a browser window and it has a document with a character
    // set, then extract the current charset menu setting from the current document and use it to
    // initialize the new browser window...
    if (window && (wintype == "navigator:browser") &&
      window._content && window._content.document) {
      var DocCharset = window._content.document.characterSet;
      var charsetArg = "charset="+DocCharset;
 
      //we should "inherit" the charset menu setting in a new window
      newWin = window.openDialog( getBrowserURL(), "_blank", "chrome,all,dialog=no", url, charsetArg, true );
    }
    else { // forget about the charset information.
      newWin = window.openDialog( getBrowserURL(), "_blank", "chrome,all,dialog=no", url, null, true );
    }
 
    // Fix new window.    
    newWin.saveFileAndPos = true;
  }

  function openNewTabWith(url) {

	  urlSecurityCheck(url, document);
    var wintype = document.firstChild.getAttribute('windowtype');

    // if and only if the current window is a browser window and it has a document with a character
    // set, then extract the current charset menu setting from the current document and use it to
    // initialize the new browser window...
    if (window && (wintype == "navigator:browser")) {
		  var browser=getBrowser();
		  var t = browser.addTab(url); // open link in new tab
      if (pref && !pref.getBoolPref("browser.tabs.loadInBackground"))
        browser.selectedTab = t;
    }

    // Fix new window.    
    newWin.saveFileAndPos = true;
  }

  function savePage(url) 
  {
    var postData = null; // No post data, usually.
    var cacheKey = null;
    // Default is to save current page.
    if ( !url )
      url = window._content.location.href;

    try {
      var sessionHistory = getWebNavigation().sessionHistory;
      var entry = sessionHistory.getEntryAtIndex(sessionHistory.index, false).QueryInterface(Components.interfaces.nsISHEntry);
      postData = entry.postData;
      cacheKey = entry.cacheKey;
    } catch(e) {
    }

    // Use stream xfer component to prompt for destination and save.
    var xfer = Components.classes["@mozilla.org/appshell/component/xfer;1"].getService(Components.interfaces["nsIStreamTransfer"]);
    try {
      xfer.SelectFileAndTransferLocationSpec( url, window, "", "", true, postData, cacheKey );
    } catch( exception ) {
      return false;
    }

    return true;
  }

  //Note: "function editPage(url)" was moved to utilityOverlay.js

  function findParentNode(node, parentNode)
  {
    if (node && node.nodeType == Node.TEXT_NODE) {
      node = node.parentNode;
    }
    while (node) {
      var nodeName = node.localName;
      if (!nodeName)
        return null;
      nodeName = nodeName.toLowerCase();
      if (nodeName == "body" || nodeName == "html" ||
          nodeName == "#document") {
        return null;
      }
      if (nodeName == parentNode)
        return node;
      node = node.parentNode;
    }
    return null;
  }

