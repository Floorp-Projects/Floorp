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
 */

var gBrowser = null;
var appCore = null;
var gPrefs = null;

try {
  var prefsService = Components.classes["@mozilla.org/preferences;1"];
  if (prefsService)
    prefsService = prefsService.getService();
  if (prefsService)
    gPrefs = prefsService.QueryInterface(Components.interfaces.nsIPref);
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

  try {
    if ("arguments" in window && window.arguments.length >= 2) {
      if (window.arguments[1].indexOf('charset=') != -1) {
        var arrayArgComponents = window.arguments[1].split('=');
        if (arrayArgComponents) {
          appCore.setDefaultCharacterSet(arrayArgComponents[1]); //XXXjag see bug 67442
        } 
      }
    }
  } catch(ex) {
  }

  var loadFlags = Components.interfaces.nsIWebNavigation.LOAD_FLAGS_NONE;
  var viewSrcUrl = "view-source:" + url;
  getBrowser().webNavigation.loadURI(viewSrcUrl, loadFlags);

  //check the view_source.wrap_long_lines pref and set the menuitem's checked attribute accordingly
  if (gPrefs){
    try {
      var wraplonglinesPrefValue = gPrefs.GetBoolPref("view_source.wrap_long_lines");

      if (wraplonglinesPrefValue)
        document.getElementById('menu_wrapLongLines').setAttribute("checked", "true");
    } catch (ex) {
    }
  }

  window._content.focus();
  return true;
}

function getMarkupDocumentViewer()
{
  return getBrowser().markupDocumentViewer;
}

// Strips the |view-source:| for editPage()
function ViewSourceEditPage()
{
  var url = window._content.location.href;
  url = url.substring(12,url.length);
  editPage(url,window, false);
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
      if (myWrap.className == ''){
        gPrefs.SetBoolPref("view_source.wrap_long_lines", false);
      }
      else {
        gPrefs.SetBoolPref("view_source.wrap_long_lines", true);
      }
    } catch (ex) {
    }
  }
}
