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
 *   Blake Ross <blakeross@telocity.com>
 *   Peter Annema <disttsc@bart.nl>
 *   Samir Gehani <sgehani@netscape.com>
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

const nsIWebNavigation = Components.interfaces.nsIWebNavigation;
var gPrintSettingsAreGlobal = true;
var gSavePrintSettings = true;
var gChromeState = null; // chrome state before we went into print preview
var gInPrintPreviewMode = false;

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

function toggleAffectedChrome(aHide)
{
  // chrome to toggle includes:
  //   (*) menubar
  //   (*) navigation bar
  //   (*) personal toolbar
  //   (*) tab browser ``strip''
  //   (*) sidebar
  //   (*) statusbar

  if (!gChromeState)
    gChromeState = new Object;

  var statusbar = document.getElementById("status-bar");
  var navToolbox = document.getElementById("navigator-toolbox");
  navToolbox.hidden = aHide;
  var theTabbrowser = document.getElementById("content"); 

  // sidebar states map as follows:
  //   was-hidden    => hide/show nothing
  //   was-collapsed => hide/show only the splitter
  //   was-shown     => hide/show the splitter and the box
  if (aHide)
  {
    // going into print preview mode
    if (sidebar_is_collapsed())
    {
      gChromeState.sidebar = "was-collapsed";
    }
    else if (sidebar_is_hidden())
      gChromeState.sidebar = "was-hidden";
    else 
    {
      gChromeState.sidebar = "was-visible";
    }
    document.getElementById("sidebar-box").hidden = true;
    document.getElementById("sidebar-splitter").hidden = true;

    // deal with tab browser
    gChromeState.hadTabStrip = theTabbrowser.getStripVisibility();
    theTabbrowser.setStripVisibilityTo(false);

    // deal with the Status Bar
    gChromeState.statusbarWasHidden = statusbar.hidden;
    statusbar.hidden = true;
  }
  else
  {
    // restoring normal mode (i.e., leaving print preview mode)
    if (gChromeState.sidebar == "was-collapsed" ||
        gChromeState.sidebar == "was-visible")
      document.getElementById("sidebar-splitter").hidden = false;
    if (gChromeState.sidebar == "was-visible")
      document.getElementById("sidebar-box").hidden = false;

    // restore tab browser
    theTabbrowser.setStripVisibilityTo(gChromeState.hadTabStrip);

    // restore the Status Bar
    statusbar.hidden = gChromeState.statusbarWasHidden;
  }

  // if we are unhiding and sidebar used to be there rebuild it
  if (!aHide && gChromeState.sidebar == "was-visible")
    SidebarRebuild();
}

function onEnterPrintPreview()
{
  toggleAffectedChrome(true);
  gInPrintPreviewMode = true;
}

function onExitPrintPreview()
{
  gInPrintPreviewMode = false;
  // restore chrome to original state
  toggleAffectedChrome(false);
}

function getEngineWebBrowserPrint()
{
  return content.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                .getInterface(Components.interfaces.nsIWebBrowserPrint);
}

function getNavToolbox()
{
  return document.getElementById("navigator-toolbox");
}

function getPPBrowser()
{
  return document.getElementById("browser");
}

function BrowserPrintPreview()
{
  PrintUtils.printPreview(onEnterPrintPreview, onExitPrintPreview);
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

function BrowserSetForcedDetector(doReload)
{
  getBrowser().documentCharsetInfo.forcedDetector = true;
  if (doReload)
    BrowserReloadWithFlags(nsIWebNavigation.LOAD_FLAGS_CHARSET_CHANGE);
}

var gFindInstData;
function getFindInstData()
{
  if (!gFindInstData) {
    gFindInstData = new nsFindInstData();
    gFindInstData.browser = getBrowser();
    // defaults for rootSearchWindow and currentSearchWindow are fine here
  }
  return gFindInstData;
}

function BrowserFind()
{
  findInPage(getFindInstData());
}

function BrowserFindAgain(reverse)
{
  findAgainInPage(getFindInstData(), reverse);
}

function BrowserCanFindAgain()
{
  return canFindAgainInPage();
}

function getMarkupDocumentViewer()
{
  return getBrowser().markupDocumentViewer;
}

/**
 * Content area tooltip.
 * XXX - this must move into XBL binding/equiv! Do not want to pollute
 *       navigator.js with functionality that can be encapsulated into
 *       browser widget. TEMPORARY!
 *
 * NOTE: Any changes to this routine need to be mirrored in ChromeListener::FindTitleText()
 *       (located in mozilla/embedding/browser/webBrowser/nsDocShellTreeOwner.cpp)
 *       which performs the same function, but for embedded clients that
 *       don't use a XUL/JS layer. It is important that the logic of
 *       these two routines be kept more or less in sync.
 *       (pinkerton)
 **/
function FillInHTMLTooltip(tipElement)
{
  var retVal = false;
  if (tipElement.namespaceURI == "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul")
    return retVal;

  const XLinkNS = "http://www.w3.org/1999/xlink";


  var titleText = null;
  var XLinkTitleText = null;
  
  while (!titleText && !XLinkTitleText && tipElement) {
    if (tipElement.nodeType == Node.ELEMENT_NODE) {
      titleText = tipElement.getAttribute("title");
      XLinkTitleText = tipElement.getAttributeNS(XLinkNS, "title");
    }
    tipElement = tipElement.parentNode;
  }

  var texts = [titleText, XLinkTitleText];
  var tipNode = document.getElementById("aHTMLTooltip");

  for (var i = 0; i < texts.length; ++i) {
    var t = texts[i];
    if (t && t.search(/\S/) >= 0) {
      tipNode.setAttribute("label", t);
      retVal = true;
    }
  }

  return retVal;
}
