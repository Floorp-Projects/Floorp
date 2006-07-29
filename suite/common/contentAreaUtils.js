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
 * Contributors:
 * 
 */

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

  function openNewWindowWith(url) {

 // URL Loading Security Check
    const nsIStandardURL = Components.interfaces.nsIStandardURL;
    const nsIURI = Components.interfaces.nsIURI;
    const stdURL = Components.classes["@mozilla.org/network/standard-url;1"];

    var sourceURL = stdURL.createInstance(nsIStandardURL);
    var focusedWindow = document.commandDispatcher.focusedWindow;
    var sourceWin = isDocumentFrame(focusedWindow) ? focusedWindow.location.href : window._content.location.href;
    sourceURL.init(nsIStandardURL.URLTYPE_STANDARD, 80, sourceWin, null);

    var targetURL = stdURL.createInstance(nsIStandardURL);
    targetURL.init(nsIStandardURL.URLTYPE_STANDARD, 80, url, null);

    const nsIScriptSecurityManager = Components.interfaces.nsIScriptSecurityManager;
    var secMan = Components.classes["@mozilla.org/scriptsecuritymanager;1"].getService().
                 QueryInterface(nsIScriptSecurityManager);
    secMan.CheckLoadURI(sourceURL, targetURL, nsIScriptSecurityManager.STANDARD);

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
      newWin = window.openDialog( getBrowserURL(), "_blank", "chrome,all,dialog=no", url, charsetArg );
    }
    else { // forget about the charset information.
      newWin = window.openDialog( getBrowserURL(), "_blank", "chrome,all,dialog=no", url );
    }
 
    // Fix new window.    
    newWin.saveFileAndPos = true;
  }

  function savePage(url) 
  {
    var postData = null; // No post data, usually.
    // Default is to save current page.
    if ( !url )
      url = window._content.location.href;
    // Post data comes from appcore.
    try {
      postData = window.appCore.postData;
    } catch(e) {
    }
    // Use stream xfer component to prompt for destination and save.
    var xfer = Components.classes["@mozilla.org/appshell/component/xfer;1"].getService(Components.interfaces["nsIStreamTransfer"]);
    try {
      xfer.SelectFileAndTransferLocationSpec( url, window, "", "", true, postData );
    } catch( exception ) {
      return false;
    }

    return true;
  }

  //Note: "function editPage(url)" was moved to utilityOverlay.js

  function findParentNode(node, parentNode)
  {
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

  function addBookmark(url,title)
  {
    if (!title)
      title = url;

    var focusedWindow = document.commandDispatcher.focusedWindow;
    if (focusedWindow == window)
      focusedWindow = _content;

    var docCharset = focusedWindow.document.characterSet;

    var bmks = Components.classes["@mozilla.org/browser/bookmarks-service;1"]
                         .getService(Components.interfaces.nsIBookmarksService);

    bmks.AddBookmark(url, title, bmks.BOOKMARK_DEFAULT_TYPE, docCharset);
  }
