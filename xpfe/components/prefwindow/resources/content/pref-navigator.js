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
  }
}

function setHomePageToCurrentPage()
{
  var url = getCurrentPage();
  if (url) {
    var homePageField = document.getElementById("browserStartupHomepage");
    homePageField.value = url;
  }
  updateHomePageButtons();
}

function setHomePageToDefaultPage()
{
  var homePageField = document.getElementById("browserStartupHomepage");
  homePageField.value = gDefaultPage;
  updateHomePageButtons();
}

function getCurrentPage()
{
  var windowManager = Components.classes[WINDOWMEDIATOR_CONTRACTID]
                                .getService(nsIWindowMediator);
  var browser, url;
  var browserWindow = windowManager.getMostRecentWindow("navigator:browser");
  if (browserWindow) {
    browser = browserWindow.document.getElementById("content");
    if (browser) {
      url = browser.webNavigation.currentURI.spec;
    }
  }

  return url;
}

function getDefaultPage()
{
  var prefService = Components.classes[PREFSERVICE_CONTRACTID]
                              .getService(nsIPrefService);
  var pref = prefService.getDefaultBranch(null);
  return pref.getComplexValue("browser.startup.homepage",
                              nsIPrefLocalizedString).data;
}

function locationInputHandler()
{
  updateHomePageButtons();
}

function Startup()
{
  updateHomePageButtons();

  gDefaultPage = getDefaultPage();
}

function updateHomePageButtons()
{
  var homepage = document.getElementById("browserStartupHomepage").value;
  var currentPageButton = document.getElementById("browserUseCurrent");
  var defaultPageButton = document.getElementById("browserUseDefault");
  currentPageButton.disabled = (homepage == getCurrentPage());
  defaultPageButton.disabled = (homepage == gDefaultPage);
}

