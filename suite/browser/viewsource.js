/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
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
 * Copyright (C) Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *    Doron Rosenberg (doronr@naboonline.com) 
 *    Neil Rashbrook (neil@parkwaycc.co.uk)
 */

const pageLoaderIface = Components.interfaces.nsIWebPageDescriptor;
var gBrowser = null;
var appCore = null;
var gPrefs = null;

try {
  var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                              .getService(Components.interfaces.nsIPrefService);
  gPrefs = prefService.getBranch(null);
} catch (ex) {
}

function onLoadViewSource() 
{
  viewSource(window.arguments[0]);
  window._content.focus();
}

function getBrowser()
{
  if (!gBrowser)
    gBrowser = document.getElementById("content");
  return gBrowser;
}

function viewSource(url)
{
  if (!url)
    return false; // throw Components.results.NS_ERROR_FAILURE;

  try {
    appCore = Components.classes["@mozilla.org/appshell/component/browser/instance;1"]
                        .createInstance(Components.interfaces.nsIBrowserInstance);

    // Initialize browser instance..
    appCore.setWebShellWindow(window);
  } catch(ex) {
    // Give up.
    window.close();
    return false;
  }

  var loadFromURL = true;
  //
  // Parse the 'arguments' supplied with the dialog.
  //    arg[0] - URL string.
  //    arg[1] - Charset value in the form 'charset=xxx'.
  //    arg[2] - Page descriptor used to load content from the cache.
  //
  if ("arguments" in window) {
    var arg;
    //
    // Set the charset of the viewsource window...
    //
    if (window.arguments.length >= 2) {
      arg = window.arguments[1];

      try {
        if (typeof(arg) == "string" && arg.indexOf('charset=') != -1) {
          var arrayArgComponents = arg.split('=');
          if (arrayArgComponents) {
            //we should "inherit" the charset menu setting in a new window
            getMarkupDocumentViewer().defaultCharacterSet = arrayArgComponents[1];
          } 
        }
      } catch (ex) {
        // Ignore the failure and keep processing arguments...
      }
    }
    //
    // Use the page descriptor to load the content from the cache (if
    // available).
    //
    if (window.arguments.length >= 3) {
      arg = window.arguments[2];

      try {
        if (typeof(arg) == "object" && arg != null) {
          var PageLoader = getBrowser().webNavigation.QueryInterface(pageLoaderIface);

          //
          // Load the page using the page descriptor rather than the URL.
          // This allows the content to be fetched from the cache (if
          // possible) rather than the network...
          //
          PageLoader.LoadPage(arg, pageLoaderIface.DISPLAY_AS_SOURCE);
          // The content was successfully loaded from the page cookie.
          loadFromURL = false;
        }
      } catch(ex) {
        // Ignore the failure.  The content will be loaded via the URL
        // that was supplied in arg[0].
      }
    }
  }

  if (loadFromURL) {
    //
    // Currently, an exception is thrown if the URL load fails...
    //
    var loadFlags = Components.interfaces.nsIWebNavigation.LOAD_FLAGS_NONE;
    var viewSrcUrl = "view-source:" + url;
    getBrowser().webNavigation.loadURI(viewSrcUrl, loadFlags, null, null, null);
  }

  //check the view_source.wrap_long_lines pref and set the menuitem's checked attribute accordingly
  if (gPrefs) {
    try {
      var wraplonglinesPrefValue = gPrefs.getBoolPref("view_source.wrap_long_lines");

      if (wraplonglinesPrefValue)
        document.getElementById('menu_wrapLongLines').setAttribute("checked", "true");
    } catch (ex) {
    }
    try {
      document.getElementById("cmd_highlightSyntax").setAttribute("checked", gPrefs.getBoolPref("view_source.syntax_highlight"));
    } catch (ex) {
    }
  } else {
    document.getElementById("cmd_highlightSyntax").setAttribute("hidden", "true");
  }

  window._content.focus();
  return true;
}

function ViewSourceClose()
{
  window.close();
}

// Strips the |view-source:| for editPage()
function ViewSourceEditPage()
{
  var url = window._content.location.href;
  url = url.substring(12,url.length);
  editPage(url,window, false);
}

// Strips the |view-source:| for saveURL()
function ViewSourceSavePage()
{
  var url = window._content.document.location.href;
  url = url.substring(12,url.length);

  saveURL(url, null, "SaveLinkTitle");
}

//function to toggle long-line wrapping and set the view_source.wrap_long_lines 
//pref to persist the last state
function wrapLongLines()
{
  //get the first pre tag which surrounds the entire viewsource content
  var myWrap = window._content.document.getElementById('viewsource');

  if (myWrap.className == '')
    myWrap.className = 'wrap';
  else myWrap.className = '';

  //since multiple viewsource windows are possible, another window could have 
  //affected the pref, so instead of determining the new pref value via the current
  //pref value, we use myWrap.className  
  if (gPrefs){
    try {
      if (myWrap.className == '') {
        gPrefs.setBoolPref("view_source.wrap_long_lines", false);
      }
      else {
        gPrefs.setBoolPref("view_source.wrap_long_lines", true);
      }
    } catch (ex) {
    }
  }
}

//function to toggle syntax highlighting and set the view_source.syntax_highlight
//pref to persist the last state
function highlightSyntax()
{
  var highlightSyntaxCmd = document.getElementById("cmd_highlightSyntax");
  var highlightSyntax = highlightSyntaxCmd.getAttribute("checked") != "true";
  highlightSyntaxCmd.setAttribute("checked", highlightSyntax);
  gPrefs.setBoolPref("view_source.syntax_highlight", highlightSyntax);

  var PageLoader = getBrowser().webNavigation.QueryInterface(pageLoaderIface);
  PageLoader.LoadPage(PageLoader.currentDescriptor, pageLoaderIface.DISPLAY_NORMAL);
}

// Fix for bug 136322: this function overrides the function in
// browser.js to call PageLoader.LoadPage() instead of BrowserReloadWithFlags()
function BrowserSetForcedCharacterSet(aCharset)
{
  var docCharset = getBrowser().docShell.QueryInterface(
                            Components.interfaces.nsIDocCharset);
  docCharset.charset = aCharset;
  var PageLoader = getBrowser().webNavigation.QueryInterface(pageLoaderIface);
  PageLoader.LoadPage(PageLoader.currentDescriptor, pageLoaderIface.DISPLAY_NORMAL);
}
