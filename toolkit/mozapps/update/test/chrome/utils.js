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
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert Strong <robert.bugzilla@gmail.com> (Original Author)
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
 * ***** END LICENSE BLOCK *****
 */

// The tests have to use the pageid due to the wizard's access method being
// random.
const PAGEID_CHECKING         = "checking";              // Done
const PAGEID_PLUGIN_UPDATES   = "pluginupdatesfound";
const PAGEID_NO_UPDATES_FOUND = "noupdatesfound";        // Done
const PAGEID_MANUAL_UPDATE    = "manualUpdate"; // Tested on license load failure
const PAGEID_INCOMPAT_CHECK   = "incompatibleCheck"; // Bug 546595
const PAGEID_FOUND_BASIC      = "updatesfoundbasic";     // Done
const PAGEID_FOUND_BILLBOARD  = "updatesfoundbillboard"; // Done
const PAGEID_LICENSE          = "license";               // Done
const PAGEID_INCOMPAT_LIST    = "incompatibleList"; // Bug 546595
const PAGEID_DOWNLOADING      = "downloading";           // Done
const PAGEID_ERRORS           = "errors";                // Done
const PAGEID_ERROR_PATCHING   = "errorpatching";         // Done
const PAGEID_FINISHED         = "finished";              // Done
const PAGEID_FINISHED_BKGRD   = "finishedBackground";    // Done
const PAGEID_INSTALLED        = "installed";             // Done

const URL_HOST = "http://example.com/";
const URL_PATH = "chrome/toolkit/mozapps/update/test/chrome";
const URL_UPDATE = URL_HOST + URL_PATH + "/update.sjs";

const URI_UPDATE_PROMPT_DIALOG  = "chrome://mozapps/content/update/updates.xul";

const CRC_ERROR = 4;

var qUpdateChannel;
var gWin;
var gDocElem;
var gPageId;
var gNextFunc;

#include ../shared.js

/**
 * Default run function that can be used by most tests
 */
function runTestDefault() {
  ok(true, "Entering runTestDefault");

  closeUpdateWindow();

  gWW.registerNotification(gWindowObserver);
  setUpdateChannel();
  test01();
}

/**
 * Default finish function that can be used by most tests
 */
function finishTestDefault() {
  ok(true, "Entering finishTestDefault");

  gWW.unregisterNotification(gWindowObserver);

  resetPrefs();
  removeUpdateDirsAndFiles();
  reloadUpdateManagerData();
  SimpleTest.finish();
}

__defineGetter__("gWW", function() {
  delete this.gWW;
  return this.gWW = AUS_Cc["@mozilla.org/embedcomp/window-watcher;1"].
                      getService(AUS_Ci.nsIWindowWatcher);
});


/**
 * Closes the update window in case a previous test failed to do so.
 */
function closeUpdateWindow() {
  var updateWindow = getUpdateWindow();
  if (!updateWindow)
    return;

  ok(true, "Found Update Window!!! Attempting to close it.");
  updateWindow.close();
}

/**
 * Returns the update window if it is open.
 */
function getUpdateWindow() {
  var wm = AUS_Cc["@mozilla.org/appshell/window-mediator;1"].
           getService(AUS_Ci.nsIWindowMediator);
  return wm.getMostRecentWindow("Update:Wizard");
}

var gWindowObserver = {
  observe: function(aSubject, aTopic, aData) {
    var win = aSubject.QueryInterface(AUS_Ci.nsIDOMEventTarget);

    if (aTopic == "domwindowclosed") {
      if (win.location == URI_UPDATE_PROMPT_DIALOG)
        gNextFunc();
      return;
    }

    win.addEventListener("load", function onLoad() {
      if (win.location != URI_UPDATE_PROMPT_DIALOG)
        return;

      gWin = win;
      gWin.removeEventListener("load", onLoad, false);
      gDocElem = gWin.document.documentElement;

      // If the page loaded is the same as the page to be tested call the next
      // function (e.g. the page shown is the wizard page the test is interested
      // in) otherwise add an event listener to run the next function (e.g. the
      // page will automatically advance to the wizard page the test is
      // interested in).
      if (gDocElem.currentPage && gDocElem.currentPage.pageid == gPageId)
        gNextFunc();
      else
        addPageShowListener();
    }, false);
  }
};

function nextFuncListener(aEvent) {
  gNextFunc(aEvent);
}

function addPageShowListener() {
  var page = gDocElem.getPageById(gPageId);
  page.addEventListener("pageshow", function onPageShow(aEvent) {
    page.removeEventListener("pageshow", onPageShow, false);
    // Let the page initialize
    SimpleTest.executeSoon(gNextFunc);
  }, false);
}

function resetPrefs() {
  if (qUpdateChannel)
    setUpdateChannel(qUpdateChannel);
  if (gPref.prefHasUserValue(PREF_APP_UPDATE_IDLETIME))
    gPref.clearUserPref(PREF_APP_UPDATE_IDLETIME);
  if (gPref.prefHasUserValue(PREF_APP_UPDATE_ENABLED))
    gPref.clearUserPref(PREF_APP_UPDATE_ENABLED);
  if (gPref.prefHasUserValue(PREF_APP_UPDATE_LOG))
    gPref.clearUserPref(PREF_APP_UPDATE_LOG);
  if (gPref.prefHasUserValue(PREF_APP_UPDATE_SHOW_INSTALLED_UI))
    gPref.clearUserPref(PREF_APP_UPDATE_SHOW_INSTALLED_UI);
  if (gPref.prefHasUserValue(PREF_APP_UPDATE_URL_DETAILS))
    gPref.clearUserPref(PREF_APP_UPDATE_URL_DETAILS);
  if (gPref.prefHasUserValue(PREF_APP_UPDATE_URL_OVERRIDE))
    gPref.clearUserPref(PREF_APP_UPDATE_URL_OVERRIDE);
  try {
    gPref.deleteBranch(PREF_APP_UPDATE_NEVER_BRANCH);    
  }
  catch(e) {
  }
}
