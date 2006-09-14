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
 *   Blake Ross <blakeross@telocity.com>
 *   Peter Annema <disttsc@bart.nl>
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

const nsIWebNavigation = Components.interfaces.nsIWebNavigation;
var gPrintSettings = null;
var gUseGlobalPrintSettings = false;

function getWebNavigation()
{
  try {
    return getBrowser().webNavigation;
  } catch (e) {
    return null;
  }
}

function BrowserReloadWithFlags(reloadFlags)
{
  /* First, we'll try to use the session history object to reload so 
   * that framesets are handled properly. If we're in a special 
   * window (such as view-source) that has no session history, fall 
   * back on using the web navigation's reload method.
   */

  var webNav = getWebNavigation();
  try {
    var sh = webNav.sessionHistory;
    if (sh)
      webNav = sh.QueryInterface(Components.interfaces.nsIWebNavigation);
  } catch (e) {
  }

  try {
    webNav.reload(reloadFlags);
  } catch (e) {
  }
}

function BrowserPrintPreview()
{
  var printOptionsService = Components.classes["@mozilla.org/gfx/printoptions;1"]
                                         .getService(Components.interfaces.nsIPrintOptions);
  if (gPrintSettings == null) {
    gPrintSettings = printOptionsService.CreatePrintSettings();
  }
  // using _content.printPreview() until printing becomes scriptable on docShell
  try {
    _content.printPreview(gPrintSettings, false);

  } catch (e) {
    // Pressing cancel is expressed as an NS_ERROR_FAILURE return value,
    // causing an exception to be thrown which we catch here.
    // Unfortunately this will also consume helpful failures, so add a
    // dump(e); // if you need to debug
  }
}


function BrowserPrintSetup()
{

  try {
    var pref = Components.classes["@mozilla.org/preferences-service;1"]
                         .getService(Components.interfaces.nsIPrefBranch);
    if (pref) {
      gUseGlobalPrintSettings = pref.getBoolPref("print.use_global_printsettings", false);
    }

    var printOptionsService = Components.classes["@mozilla.org/gfx/printoptions;1"]
                                           .getService(Components.interfaces.nsIPrintOptions);

    // create our own local copy of the print settings
    if (gPrintSettings == null) {
      gPrintSettings = printOptionsService.CreatePrintSettings();
    }

    // if we are using the global settings then get them 
    // before calling page setup
    if (gUseGlobalPrintSettings) {
      gPrintSettings = printOptionsService.printSettingsValues;
    }

    goPageSetup(gPrintSettings);  // from utilityOverlay.js

    // now set our setting into the global settings
    // after the changes were made
    if (gUseGlobalPrintSettings) {
      printOptionsService.printSettingsValues = gPrintSettings;
    }
  } catch (e) {
    alert("BrowserPrintSetup "+e);
  }
}

function BrowserPrint()
{
  try {
    var pref = Components.classes["@mozilla.org/preferences-service;1"]
                         .getService(Components.interfaces.nsIPrefBranch);
    if (pref) {
      gUseGlobalPrintSettings = pref.getBoolPref("print.use_global_printsettings");
    }
    var printOptionsService = Components.classes["@mozilla.org/gfx/printoptions;1"]
                                             .getService(Components.interfaces.nsIPrintOptions);
    if (gPrintSettings == null) {
      gPrintSettings = printOptionsService.CreatePrintSettings();
    }

    // if we are using the global settings then get them 
    // before calling print
    if (gUseGlobalPrintSettings) {
      gPrintSettings = printOptionsService.printSettingsValues;
    }

    // using _content.print() until printing becomes scriptable on docShell
    _content.print(gPrintSettings, false);

    // now set our setting into the global settings
    // after the changes were made
    if (gUseGlobalPrintSettings) {
      printOptionsService.printSettingsValues = gPrintSettings;
    }

  } catch (e) {
    // Pressing cancel is expressed as an NS_ERROR_ABORT return value,
    // causing an exception to be thrown which we catch here.
    // Unfortunately this will also consume helpful failures, so add a
    // dump(e); // if you need to debug
  }
}

function BrowserSetDefaultCharacterSet(aCharset)
{
  // no longer needed; set when setting Force; see bug 79608
}

function BrowserSetForcedCharacterSet(aCharset)
{
  var docCharset = getBrowser().docShell.QueryInterface(
                            Components.interfaces.nsIDocCharset);
  docCharset.charset = aCharset;
  BrowserReloadWithFlags(nsIWebNavigation.LOAD_FLAGS_CHARSET_CHANGE);
}

function BrowserSetForcedDetector()
{
  getBrowser().documentCharsetInfo.forcedDetector = true;
}

function BrowserFind()
{
  var focusedWindow = document.commandDispatcher.focusedWindow;
  if (!focusedWindow || focusedWindow == window)
    focusedWindow = window._content;

  findInPage(getBrowser(), window._content, focusedWindow)
}

function BrowserFindAgain()
{
    var focusedWindow = document.commandDispatcher.focusedWindow;
    if (!focusedWindow || focusedWindow == window)
      focusedWindow = window._content;

  findAgainInPage(getBrowser(), window._content, focusedWindow)
}

function BrowserCanFindAgain()
{
  return canFindAgainInPage();
}

function getMarkupDocumentViewer()
{
  return getBrowser().markupDocumentViewer;
}
