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
 *   Alec Flett      <alecf@netscape.com>
 *   Ben Goodger     <ben@netscape.com>
 *   Mike Pinkerton  <pinkerton@netscape.com>
 *   Blake Ross      <blakeross@telocity.com>
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

  var pref = null;
  pref = Components.classes["@mozilla.org/preferences;1"];
  pref = pref.getService();
  pref = pref.QueryInterface(Components.interfaces.nsIPref);

  // Prefill a single text field
  function prefillTextBox(target) {

    // obtain values to be used for prefilling
    var walletService = Components.classes["@mozilla.org/wallet/wallet-service;1"].getService(Components.interfaces.nsIWalletService);
    var value = walletService.WALLET_PrefillOneElement(window._content, target);
    if (value) {

      // result is a linear sequence of values, each preceded by a separator character
      // convert linear sequence of values into an array of values
      var separator = value[0];
      var valueList = value.substring(1, value.length).split(separator);

      target.value = valueList[0];
/*
 * Following code is a replacement for above line.  In the case of multiple values, it
 * presents the user with a dialog containing a list from which he can select the value
 * he wants.  However it is being commented out for now because of several problems, namely
 *
 *   1. There is no easy way to put localizable strings for the title and message of
 *      the dialog without introducing a .properties file which currently doesn't exist
 *   2. Using blank title and messages as shown below have a problem because a zero-length
 *      title is being displayed as some garbage characters (which is why the code below
 *      has a title of " " instead of "").  This is a separate bug which will have to be
 *      investigated further.
 *   3. The current wallet tables present alternate values for items such as shipping
 *      address -- namely billing address and home address.  Up until now, these alternate
 *      values have never been a problem because the preferred value is always first and is
 *      all the user sees when doing a prefill.  However now he will be presented with a
 *      list showing all these values and asking him to pick one, even though the wallet
 *      code was clearly able to determine that he meant shipping address and not billing
 *      address.
 *   4. There is a relatively long delay before the dialog come up whereas values are
 *      filled in quickly when no dialog is involved.
 *
 * Once this feature is checked in, a separate bug will be opened asking that the above
 * problems be examined and this dialog turned on
 
      if (valueList.length == 1) {
        // only one value, use it for prefilling
        target.value = valueList[0];
      } else {

        // more than one value, have user select the one he wants
        var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService();
        promptService = promptService.QueryInterface(Components.interfaces.nsIPromptService);
        var position = {};
        var title = " ";
        var message = "";
        var ok =
          promptService.select
            (window, title, message, valueList.length, valueList, position)
        if (ok) {
          target.value = valueList[position.value];
        }
      }

 * End of commented out code
 */
    }
  }
  
  // Called whenever the user clicks in the content area,
  // except when left-clicking on links (special case)
  // should always return true for click to go through
  function contentAreaClick(event) 
  {
    var target = event.target;
    var linkNode;
    switch (target.localName.toLowerCase()) {
      case "a":
        linkNode = target;
        break;
      case "area":
        if (target.href) 
          linkNode = target;
        break;
      case "input":
        if ((event.target.type.toLowerCase() == "text" || event.target.type == "") // text field
            && event.detail == 2 // double click
            && event.button == 0 // left mouse button
            && event.target.value.length == 0) { // no text has been entered
          prefillTextBox(target); // prefill the empty text field if possible
        }
        break;
      default:
        linkNode = findParentNode(event.originalTarget, "a");
        break;
    }
    if (linkNode) {
      handleLinkClick(event, linkNode.href);
      return true;
    }
    if (pref && event.button == 1 &&
        !findParentNode(event.originalTarget, "scrollbar") &&
        pref.GetBoolPref("middlemouse.contentLoadURL")) {
      if (middleMousePaste()) {
        event.preventBubble();
      }
    }
    return true;
  }

  function handleLinkClick(event, href)
  {
    switch (event.button) {                                   
      case 0:                                                         // if left button clicked
        if (event.metaKey || event.ctrlKey) {                         // and meta or ctrl are down
          if (pref && pref.GetBoolPref("browser.tabs.opentabfor.middleclick") && getBrowser && 
            getBrowser() && getBrowser().localName == "tabbrowser") {
            var t = getBrowser().addTab(href); // open link in new tab
            if (!event.shiftKey && !pref.GetBoolPref("browser.tabs.loadInBackground"))
              getBrowser().selectedTab = t;
            event.preventBubble();
            return true;
          }
          
          if (pref && pref.GetBoolPref("middlemouse.openNewWindow")) {  
            openNewWindowWith(href);                                    // open link in new window
            event.preventBubble();
            return true;
          }
        } 
        var saveModifier = true;
        if (pref) {
          try {
            saveModifier = pref.GetBoolPref("ui.key.saveLink.shift");
          }
          catch(ex) {            
          }
        }
        saveModifier = saveModifier ? event.shiftKey : event.metaKey;
          
        if (saveModifier) {                                           // if saveModifier is down
          savePage(href);                                             // save the link
          return true;
        }
        if (event.altKey)                                             // if alt is down
          return true;                                                // do nothing
        return false;
      case 1:                                                         // if middle button clicked
        if (pref && pref.GetBoolPref("browser.tabs.opentabfor.middleclick") && getBrowser && 
            getBrowser() && getBrowser().localName == "tabbrowser") {
          var t = getBrowser().addTab(href); // open link in new tab
          if (!event.shiftKey && !pref.GetBoolPref("browser.tabs.loadInBackground"))
            getBrowser().selectedTab = t;
          event.preventBubble();
          return true;
        }
        
        if (pref && pref.GetBoolPref("middlemouse.openNewWindow")) {  
          openNewWindowWith(href);                                    // open link in new window
          event.preventBubble();
          return true;
        }
        break;
    }
    return false;
  }

  function middleMousePaste()
  {
    var url = readFromClipboard();
    if (url) {
      loadURI(getShortcutOrURI(url));
      return true;
    }
    return false;
  }
