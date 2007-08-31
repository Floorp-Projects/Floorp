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
 * Alexander J. Vincent <ajvincent@gmail.com>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
 * the terms of any one of the MPL, the GPL or the LGPL
 *
 * ***** END LICENSE BLOCK *****
 */

/* XXX ajvincent XPCOM_DEBUG_BREAK automatically causes a xpcshell test to crash
   if a NS_ASSERTION fires.  However, the assertions this testcase triggers are
   unrelated to the actual test, and the component this test runs against is
   JavaScript-based - so assertions here do not apply against the tested
   component.  I am (reluctantly) turning the assertions into stack warnings in
   order to prevent test failures at this point which are not the fault of the
   code being tested or the test script.

   At present, the assertions fired are for calls which aren't thread-safe.
*/
var env = Components.classes["@mozilla.org/process/environment;1"]
                    .getService(Components.interfaces.nsIEnvironment);
env.set("XPCOM_DEBUG_BREAK", "stack");

// This allows the EM to attempt to display errors to the user without failing.
var promptService = {
  // nsIPromptService
  alert: function alert(aParent,
                        aDialogTitle,
                        aText) {
    const title = "Bug 299716 test ";
    var keyChar = aText.charAt(title.length).toLowerCase();
    var id = "bug299716-" + keyChar + "@tests.mozilla.org";
    for (var i = 0; i < ADDONS.length; i++) {
      if (ADDONS[i].id != id) {
        continue;
      }

      do_check_false(ADDONS[i].installed);
      break;
    }
  },

  // nsIPromptService
  alertCheck: function alertCheck(aParent,
                                  aDialogTitle,
                                  aText,
                                  aCheckMsg,
                                  aCheckState) {
    do_throw("Unexpected call to alertCheck!");
  },

  // nsIPromptService
  confirm: function confirm(aParent,
                            aDialogTitle,
                            aText) {
    do_throw("Unexpected call to confirm!");
  },

  // nsIPromptService
  confirmCheck: function confirmCheck(aParent,
                                      aDialogTitle,
                                      aText,
                                      aCheckMsg,
                                      aCheckState) {
    do_throw("Unexpected call to confirmCheck!");
  },

  // nsIPromptService
  confirmEx: function confirmEx(aParent,
                                aDialogTitle,
                                aText,
                                aButtonFlags,
                                aButton0Title,
                                aButton1Title,
                                aButton2Title,
                                aCheckMsg,
                                aCheckState) {
    do_throw("Unexpected call to confirmEx!");
  },

  // nsIPromptService
  prompt: function prompt(aParent,
                          aDialogTitle,
                          aText,
                          aValue,
                          aCheckMsg,
                          aCheckState) {
    do_throw("Unexpected call to prompt!");
  },

  // nsIPromptService
  promptUsernameAndPassword:
  function promptUsernameAndPassword(aParent,
                                     aDialogTitle,
                                     aText,
                                     aUsername,
                                     aPassword,
                                     aCheckMsg,
                                     aCheckState) {
    do_throw("Unexpected call to promptUsernameAndPassword!");
  },

  // nsIPromptService
  promptPassword: function promptPassword(aParent,
                                          aDialogTitle,
                                          aText,
                                          aPassword,
                                          aCheckMsg,
                                          aCheckState) {
    do_throw("Unexpected call to promptPassword!");
  },

  // nsIPromptService
  select: function select(aParent,
                          aDialogTitle,
                          aText,
                          aCount,
                          aSelectList,
                          aOutSelection) {
    do_throw("Unexpected call to select!");
  },

  // nsISupports
  QueryInterface: function QueryInterface(iid) {
    if (iid.equals(Components.interfaces.nsIPromptService)
     || iid.equals(Components.interfaces.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
};

var PromptServiceFactory = {
  createInstance: function createInstance(outer, iid) {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return promptService.QueryInterface(iid);
  }
};
const nsIComponentRegistrar = Components.interfaces.nsIComponentRegistrar;
var registrar = Components.manager.QueryInterface(nsIComponentRegistrar);
const psID = Components.ID("{6cc9c9fe-bc0b-432b-a410-253ef8bcc699}");
registrar.registerFactory(psID,
                          "PromptService",
                          "@mozilla.org/embedcomp/prompt-service;1",
                          PromptServiceFactory);

const updateListener = {
  _state: -1,

  // nsIAddonUpdateListener
  onStateChange: function onStateChange(aAddon, aState, aValue) {
    if ((this._state == -1) &&
        (aState == Components.interfaces.nsIXPIProgressDialog.DIALOG_CLOSE)) {
      this._state = aState;
      next_test();
    }
  },

  onProgress: function onProgress(aAddon, aValue, aMaxValue) {
    // do nothing.
  }
};

// Update check listener.
const checkListener = {
  // nsIAddonUpdateCheckListener
  onUpdateStarted: function onUpdateStarted() {
    // do nothing
  },

  // nsIAddonUpdateCheckListener
  onUpdateEnded: function onUpdateEnded() {
    next_test();
  },

  // nsIAddonUpdateCheckListener
  onAddonUpdateStarted: function onAddonUpdateStarted(aAddon) {
    // do nothing
  },

  // nsIAddonUpdateCheckListener
  onAddonUpdateEnded: function onAddonUpdateEnded(aAddon, aStatus) {
    for (var i = 0; i < ADDONS.length; i++) {
      if (ADDONS[i].id == aAddon.id) {
        ADDONS[i].newItem = aAddon;
        return;
      }
    }
  }
}

// Get the HTTP server.
do_import_script("netwerk/test/httpserver/httpd.js");
var testserver;
var updateItems = [];

// Configure test.
const DELAY = 2000;

var ADDONS = [
  // XPCShell
  {
    id: "bug299716-a@tests.mozilla.org",
    addon: "test_bug299716_a_1",
    installed: true,
    item: null,
    newItem: null
  },

  // Toolkit
  {
    id: "bug299716-b@tests.mozilla.org",
    addon: "test_bug299716_b_1",
    installed: true,
    item: null,
    newItem: null
  },

  // XPCShell + Toolkit
  {
    id: "bug299716-c@tests.mozilla.org",
    addon: "test_bug299716_c_1",
    installed: true,
    item: null,
    newItem: null
  },

  // XPCShell (Toolkit invalid)
  {
    id: "bug299716-d@tests.mozilla.org",
    addon: "test_bug299716_d_1",
    installed: true,
    item: null,
    newItem: null
  },

  // Toolkit (XPCShell invalid)
  {
    id: "bug299716-e@tests.mozilla.org",
    addon: "test_bug299716_e_1",
    installed: false,
    item: null,
    newItem: null,
    failedAppName: "XPCShell"
  },

  // None (XPCShell, Toolkit invalid)
  {
    id: "bug299716-f@tests.mozilla.org",
    addon: "test_bug299716_f_1",
    installed: false,
    item: null,
    newItem: null,
    failedAppName: "XPCShell"
  },

  // None (Toolkit invalid)
  {
    id: "bug299716-g@tests.mozilla.org",
    addon: "test_bug299716_g_1",
    installed: false,
    item: null,
    newItem: null,
    failedAppName: "Toolkit"
  },
];

var currentAddonObj = null;
var next_test = function() {};

function do_check_item(aItem, aVersion, aAddonsEntry) {
  if (aAddonsEntry.installed) {
    do_check_eq(aItem.version, aVersion);
  } else {
    do_check_eq(aItem, null);
  }
}

/**
 * Start the test by installing extensions.
 */
function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "5", "1.9");

  const dataDir = do_get_file("toolkit/mozapps/extensions/test/unit/data");
  const addonsDir = do_get_addon(ADDONS[0].addon).parent;

  // Make sure we can actually get our data files.
  const xpiFile = addonsDir.clone();
  xpiFile.append("test_bug299716_a_2.xpi");
  do_check_true(xpiFile.exists());

  // Create and configure the HTTP server.
  testserver = new nsHttpServer();
  testserver.registerDirectory("/addons/", addonsDir);
  testserver.registerDirectory("/data/", dataDir);
  testserver.start(4444);

  // Make sure we can fetch the files over HTTP.
  const Ci = Components.interfaces;
  const xhr = Components.classes["@mozilla.org/xmlextras/xmlhttprequest;1"]
                        .createInstance(Ci.nsIXMLHttpRequest)
  xhr.open("GET", "http://localhost:4444/addons/test_bug299716_a_2.xpi", false);
  xhr.send(null);
  do_check_true(xhr.status == 200);

  xhr.open("GET", "http://localhost:4444/data/test_bug299716.rdf", false);
  xhr.send(null);
  do_check_true(xhr.status == 200);

  // Start the real test.
  startupEM();
  dump("\n\n*** INSTALLING NEW ITEMS\n\n");

  gEM.addUpdateListener(updateListener);

  for (var i = 0; i < ADDONS.length; i++) {
    gEM.installItemFromFile(do_get_addon(ADDONS[i].addon),
                            NS_INSTALL_LOCATION_APPPROFILE);
  }
  do_test_pending();

  // Give time for phone home to complete.
  do_timeout(DELAY, "run_test_pt2()");
}

/**
 * Check the versions of all items, and ask the extension manager to find updates.
 */
function run_test_pt2() {
  dump("\n\n*** RESTARTING EXTENSION MANAGER\n\n");
  restartEM();

  // Try to update the items.
  for (var i = 0; i < ADDONS.length; i++) {
    var item = gEM.getItemForID(ADDONS[i].id);
    do_check_item(item, "0.1", ADDONS[i]);
    ADDONS[i].item = item;
    updateItems[updateItems.length] = item;
  }

  dump("\n\n*** REQUESTING UPDATE\n\n");
  // updateListener will call run_test_pt3().
  next_test = run_test_pt3;
  try {
    gEM.update(updateItems,
               updateItems.length,
               Components.interfaces.nsIExtensionManager.UPDATE_CHECK_NEWVERSION,
               checkListener);
    do_throw("Shouldn't reach here!");
  } catch (e if (e instanceof Components.interfaces.nsIException &&
                 e.result == Components.results.NS_ERROR_ILLEGAL_VALUE)) {
    // do nothing, this is good
  }

  var addonsArray = [];
  for (var i = 0; i < ADDONS.length; i++) {
    if (ADDONS[i].item) {
      addonsArray.push(ADDONS[i].item);
    }
  }
  gEM.update(addonsArray,
             addonsArray.length,
             Components.interfaces.nsIExtensionManager.UPDATE_CHECK_NEWVERSION,
             checkListener);
}

/**
 * Install new items for each enabled extension.
 */
function run_test_pt3() {
  // Install the new items.
  var addonsArray = [];
  for (var i = 0; i < ADDONS.length; i++) {
    addonsArray.push(ADDONS[i].newItem);
  }
  dump("\n\n*** UPDATING " + addonsArray.length + " ITEMS\n\n");

  // updateListener will call run_test_pt4().
  next_test = run_test_pt4;

  // Here, we have some bad items that try to update.  Pepto-Bismol time.
  try {
    gEM.addDownloads(addonsArray, addonsArray.length, true);
    do_throw("Shouldn't reach here!");
  } catch (e if (e instanceof Components.interfaces.nsIException &&
                 e.result == Components.results.NS_ERROR_ILLEGAL_VALUE)) {
    // do nothing, this is good
  }

  for (i = addonsArray.length - 1; i >= 0; i--) {
    if (!addonsArray[i]) {
      addonsArray.splice(i, 1);
    }
  }

  do_check_true(addonsArray.length > 0);
  gEM.addDownloads(addonsArray, addonsArray.length, true);
}

/**
 * Check the final version of each extension.
 */
function run_test_pt4() {
  dump("\n\n*** RESTARTING EXTENSION MANAGER\n\n");
  restartEM();

  dump("\n\n*** FINAL CHECKS\n\n");
  for (var i = 0; i < ADDONS.length; i++) {
    var item = gEM.getItemForID(ADDONS[i].id);
    do_check_item(item, "0.2", ADDONS[i]);
  }
  do_test_finished();

  testserver.stop();

  // If we've gotten this far, then the test has passed.
  env.set("XPCOM_DEBUG_BREAK", "abort");
}
