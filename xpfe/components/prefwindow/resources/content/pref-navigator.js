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
 * The Original Code is Mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Goodger <ben@netscape.com>
 *   Alec Flett <alecf@netscape.com>
 *   Stephen Walker <walk84@yahoo.com>
 *   Christopher A. Aillon <christopher@aillon.com>
 *   Ian Neal <ian@arlen.demon.co.uk>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

const nsIFilePicker     = Components.interfaces.nsIFilePicker;
const nsIWindowMediator = Components.interfaces.nsIWindowMediator;
const nsIPrefService    = Components.interfaces.nsIPrefService;
const nsIPrefLocalizedString = Components.interfaces.nsIPrefLocalizedString;

const FILEPICKER_CONTRACTID     = "@mozilla.org/filepicker;1";
const WINDOWMEDIATOR_CONTRACTID = "@mozilla.org/appshell/window-mediator;1";
const PREFSERVICE_CONTRACTID    = "@mozilla.org/preferences-service;1";

var gDefaultPage;
var gData;

function setHomePageValue(value)
{
  var homePageField = document.getElementById("browserStartupHomepage");
  homePageField.value = value;
}

function getGroupIsSetMessage()
{
  var prefUtilitiesBundle = document.getElementById("bundle_prefutilities");
  return prefUtilitiesBundle.getString("groupIsSet");
}

function getMostRecentBrowser()
{
  var windowManager = Components.classes[WINDOWMEDIATOR_CONTRACTID]
                                .getService(nsIWindowMediator);

  var browserWindow = windowManager.getMostRecentWindow("navigator:browser");
  if (browserWindow)
    return browserWindow.document.getElementById("content");

  return null;
}

function getCurrentPage()
{
  var url;
  var tabbrowser = getMostRecentBrowser();
  if (tabbrowser)
    url = tabbrowser.currentURI.spec;

  return url;
}

function getCurrentPageGroup()
{
  var URIs = [];
  var tabbrowser = getMostRecentBrowser();
  if (tabbrowser) {
    var browsers = tabbrowser.browsers;
    for (var i = 0; i < browsers.length; ++i)
      URIs[i] = browsers[i].currentURI.spec;
  }

  return URIs;
}

function getDefaultPage()
{
  var prefService = Components.classes[PREFSERVICE_CONTRACTID]
                              .getService(nsIPrefService);
  var pref = prefService.getDefaultBranch(null);
  return pref.getComplexValue("browser.startup.homepage",
                              nsIPrefLocalizedString).data;
}

function updateHomePageButtons()
{
  var homepage = document.getElementById("browserStartupHomepage").value;

  // disable the "current page" button if the current page
  // is already the homepage
  var currentPageButton = document.getElementById("browserUseCurrent");
  currentPageButton.disabled = homepage == getCurrentPage();

  // disable the "default page" button if the default page
  // is already the homepage
  var defaultPageButton = document.getElementById("browserUseDefault");
  defaultPageButton.disabled = (homepage == gDefaultPage);

  // homePages.length == 1 if:
  //  - we're called from startup and there's one homepage
  //  - we're called from "current page" or "choose file"
  //  - the user typed something in the location field
  //   in those cases we only want to enable the button if:
  //    - there's more than one tab in the most recent browser
  // otherwise we have a group of homepages:
  //  - we're called from startup and there's a group of homepages
  //  - we're called from "current group"
  //   in those cases we only want to enable the button if:
  //    - there's more than one tab in the most recent browser and
  //      the current group doesn't match the group of homepages

  var enabled = false;

  var homePages = gData.navigatorData.homePages;
  if (homePages.length == 1) {
    var browser = getMostRecentBrowser();
    enabled = browser && browser.browsers.length > 1;
  } else {
    var currentURIs = getCurrentPageGroup();
    if (currentURIs.length == homePages.length) {
      for (var i = 0; !enabled && i < homePages.length; ++i) {
        if (homePages[i] != currentURIs[i])
          enabled = true;
      }
    } else if (currentURIs.length > 1) {
      enabled = true;
    }
  }

  var currentPageGroupButton = document.getElementById("browserUseCurrentGroup");
  currentPageGroupButton.disabled = !enabled;
}

function locationInputHandler()
{
  var navigatorData = gData.navigatorData;
  var homePage = document.getElementById("browserStartupHomepage");
  navigatorData.homePages = [ homePage.value ];
  updateHomePageButtons();
}

function selectFile()
{
  var fp = Components.classes[FILEPICKER_CONTRACTID]
                     .createInstance(nsIFilePicker);

  var prefutilitiesBundle = document.getElementById("bundle_prefutilities");
  var title = prefutilitiesBundle.getString("choosehomepage");
  fp.init(window, title, nsIFilePicker.modeOpen);
  fp.appendFilters(nsIFilePicker.filterAll | nsIFilePicker.filterText |
                   nsIFilePicker.filterXML | nsIFilePicker.filterHTML |
                   nsIFilePicker.filterImages);

  var ret = fp.show();
  if (ret == nsIFilePicker.returnOK) {
    var folderField = document.getElementById("browserStartupHomepage");
    folderField.value = fp.fileURL.spec;
    var url = fp.fileURL.spec;
    setHomePageValue(url);
    gData.navigatorData.homePages = [ url ];
    updateHomePageButtons();
  }
}

function setHomePageToCurrentPage()
{
  var url = getCurrentPage();
  if (url) {
    setHomePageValue(url);
    gData.navigatorData.homePages = [ url ];
    updateHomePageButtons();
  }
}

function setHomePageToCurrentGroup()
{
  var URIs = getCurrentPageGroup();
  if (URIs.length > 0) {
    setHomePageValue(gData.navigatorData.groupIsSet);
    gData.navigatorData.homePages = URIs;
    updateHomePageButtons();
  }
}

function setHomePageToDefaultPage()
{
  setHomePageValue(gDefaultPage);
  gData.navigatorData.homePages = [ gDefaultPage ];
  updateHomePageButtons();
}

function init()
{
  var prefWindow = parent.hPrefWindow;

  gData = prefWindow.wsm.dataManager
                        .pageData["chrome://communicator/content/pref/pref-navigator.xul"]

  gDefaultPage = getDefaultPage();

  if ("navigatorData" in gData)
    return;

  var navigatorData = {};

  navigatorData.groupIsSet = getGroupIsSetMessage();

  var URIs = [];
  try {
    URIs[0] = prefWindow.getPref("localizedstring",
                                 "browser.startup.homepage");

    var count = prefWindow.getPref("int",
                                   "browser.startup.homepage.count");

    for (var i = 1; i < count; ++i) {
      URIs[i] = prefWindow.getPref("localizedstring",
                                   "browser.startup.homepage."+i);
    }
    navigatorData.homePages = URIs;
  } catch(e) {
  }

  gData.navigatorData = navigatorData;

  prefWindow.registerOKCallbackFunc(doOnOk);
}

function Startup()
{
  init();

  var navigatorData = gData.navigatorData;

  var homePages = navigatorData.homePages;
  if (homePages.length == 1)
    setHomePageValue(homePages[0]);
  else
    setHomePageValue(navigatorData.groupIsSet);

  updateHomePageButtons();

  setPageAccessKeys(document.getElementById("behaviourDeck").firstChild);
}

function doOnOk()
{
  var prefWindow = parent.hPrefWindow;
  // OK could have been hit from another panel, so we need to
  // get at navigatorData the long but safer way
  var navigatorData = prefWindow.wsm.dataManager
                                .pageData["chrome://communicator/content/pref/pref-navigator.xul"]
                                .navigatorData;

  var URIs = navigatorData.homePages;

  prefWindow.setPref("string", "browser.startup.homepage", URIs[0]);

  var i = 1;
  for (; i < URIs.length; ++i)
    prefWindow.setPref("string", "browser.startup.homepage."+i, URIs[i]);
                 
  const countPref = "browser.startup.homepage.count";

  // remove the old user prefs values that we didn't overwrite
  var oldCount = prefWindow.getPref("int", countPref);
  for (; i < oldCount; ++i)
    prefWindow.pref.clearUserPref("browser.startup.homepage."+i);

  prefWindow.setPref("int", countPref, URIs.length);
}

function setPageAccessKeys(group)
{
  var nodes = group.childNodes;
  for (var i = 0; i < nodes.length; ++i)
    nodes[i].accessKey = nodes[i].getAttribute("ak");
}

function removePageAccessKeys(group)
{
  var nodes = group.childNodes;
  for (var i = 0; i < nodes.length; ++i)
    nodes[i].accessKey = '';
}

function switchPage(index)
{
  var deck = document.getElementById("behaviourDeck");
  removePageAccessKeys(deck.selectedPanel);
  deck.selectedIndex = index;
  setPageAccessKeys(deck.selectedPanel);
}
