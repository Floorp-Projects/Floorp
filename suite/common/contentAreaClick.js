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
 *   Alec Flett      <alecf@netscape.com>
 *   Ben Goodger     <ben@netscape.com>
 *   Mike Pinkerton  <pinkerton@netscape.com>
 *   Blake Ross      <blakeross@telocity.com>
 */

  var pref = null;
  pref = Components.classes["@mozilla.org/preferences;1"];
  pref = pref.getService();
  pref = pref.QueryInterface(Components.interfaces.nsIPref);
  
  // Called whenever the user clicks in the content area,
  // except when left-clicking on links (special case)
  // should always return true for click to go through
  function contentAreaClick(event) 
  {
    var target = event.originalTarget;
    var linkNode;
    switch (target.localName.toLowerCase()) {
      case "a":
        linkNode = event.target;
        break;
      case "area":
        if (event.target.href) 
          linkNode = event.target;
        break;
      default:
        linkNode = findParentNode(target, "a");
        break;
    }
    if (linkNode) {
      handleLinkClick(event, linkNode.href);
      return true;
    }
    if (pref && event.button == 2 &&
        !findParentNode(target, "scrollbar") &&
        pref.GetBoolPref("middlemouse.paste")) {
      middleMousePaste(event);
    }
    return true;
  }

  function handleLinkClick(event, href)
  {
    switch (event.button) {                                   
      case 1:                                                         // if left button clicked
        if (event.metaKey || event.ctrlKey) {                         // and meta or ctrl are down
          openNewWindowWith(href);                                    // open link in new window
          event.preventBubble();
          return true;
        } 
        if (event.shiftKey) {                                         // if shift is down
          savePage(href);                                             // save the link
          return true;
        }
        if (event.altKey)                                             // if alt is down
          return true;                                                // do nothing
        return false;
      case 2:                                                         // if middle button clicked
        if (pref && pref.GetBoolPref("middlemouse.openNewWindow")) {  // and the pref is on
          openNewWindowWith(href);                                    // open link in new window
          event.preventBubble();
          return true;
        }
        break;
    }
    return false;
  }

  function middleMousePaste(event)
  {
      var url = readFromClipboard();
      //dump ("Loading URL on clipboard: '" + url + "'; length = " + url.length + "\n");
      if (url) {
        var urlBar = document.getElementById("urlbar");
        urlBar.value = url;
        BrowserLoadURL();
        event.preventBubble();
        return true;
    }
    return false;
  }
