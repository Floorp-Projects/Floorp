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
 * Dave Townsend <dtownsend@oxymoronical.com>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2008
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

// Use the internal webserver for regular update pings, will just return an error
gPrefs.setCharPref("extensions.update.url", "http://localhost:4444/");

do_import_script("netwerk/test/httpserver/httpd.js");
var testserver;

// This allows the EM to attempt to display errors to the user without failing
var promptService = {
  alert: function(aParent, aDialogTitle, aText) {
  },
  
  alertCheck: function(aParent, aDialogTitle, aText, aCheckMsg, aCheckState) {
  },
  
  confirm: function(aParent, aDialogTitle, aText) {
  },
  
  confirmCheck: function(aParent, aDialogTitle, aText, aCheckMsg, aCheckState) {
  },
  
  confirmEx: function(aParent, aDialogTitle, aText, aButtonFlags, aButton0Title, aButton1Title, aButton2Title, aCheckMsg, aCheckState) {
  },
  
  prompt: function(aParent, aDialogTitle, aText, aValue, aCheckMsg, aCheckState) {
  },
  
  promptUsernameAndPassword: function(aParent, aDialogTitle, aText, aUsername, aPassword, aCheckMsg, aCheckState) {
  },

  promptPassword: function(aParent, aDialogTitle, aText, aPassword, aCheckMsg, aCheckState) {
  },
  
  select: function(aParent, aDialogTitle, aText, aCount, aSelectList, aOutSelection) {
  },
  
  QueryInterface: function(iid) {
    if (iid.equals(Components.interfaces.nsIPromptService)
     || iid.equals(Components.interfaces.nsISupports))
      return this;
  
    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
};

var PromptServiceFactory = {
  createInstance: function (outer, iid) {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return promptService.QueryInterface(iid);
  }
};
var registrar = Components.manager.QueryInterface(Components.interfaces.nsIComponentRegistrar);
registrar.registerFactory(Components.ID("{6cc9c9fe-bc0b-432b-a410-253ef8bcc699}"), "PromptService",
                          "@mozilla.org/embedcomp/prompt-service;1", PromptServiceFactory);

var gNextState = null;
var gIndex = null;
var ADDONS = [
{
  id: null,
  addon: "test_bug428341_1",
  error: -8,
  checksCompatibility: false
},
{
  id: null,
  addon: "test_bug428341_2",
  error: -8,
  checksCompatibility: false
},
{
  id: null,
  addon: "test_bug428341_3",
  error: -8,
  checksCompatibility: false
},
{
  id: "{invalid-guid}",
  addon: "test_bug428341_4",
  error: -2,
  checksCompatibility: false
},
{
  id: "bug428341_5@tests.mozilla.org",
  addon: "test_bug428341_5",
  error: -5,
  checksCompatibility: false
},
{
  id: "bug428341_6@tests.mozilla.org",
  addon: "test_bug428341_6",
  error: -7,
  checksCompatibility: false
},
{
  id: "bug428341_7@tests.mozilla.org",
  addon: "test_bug428341_7",
  error: -3,
  checksCompatibility: false
},
{
  id: "bug428341_8@tests.mozilla.org",
  addon: "test_bug428341_8",
  error: -3,
  checksCompatibility: true
},
{
  id: "bug428341_9@tests.mozilla.org",
  addon: "test_bug428341_9",
  error: -3,
  checksCompatibility: true
}
];

// nsIAddonInstallListener
var installListener = {
  onDownloadStarted: function(aAddon) {
    do_throw("onDownloadStarted should not be called for a direct install");
  },

  onDownloadEnded: function(aAddon) {
    do_throw("onDownloadEnded should not be called for a direct install");
  },

  onInstallStarted: function(aAddon) {
    var state = "onInstallStarted";
    if (gNextState != state) {
      do_throw("Encountered invalid state installing add-on " + ADDONS[gIndex].addon +
               ". Saw " + state + " but expected " + gNextState);
    }

    dump("Seen " + state + " for add-on " + ADDONS[gIndex].addon + "\n");

    if (ADDONS[gIndex].checksCompatibility)
      gNextState = "onCompatibilityCheckStarted";
    else
      gNextState = "onInstallEnded";
  },

  onCompatibilityCheckStarted: function(aAddon) {
    var state = "onCompatibilityCheckStarted";
    if (gNextState != state) {
      do_throw("Encountered invalid state installing add-on " + ADDONS[gIndex].addon +
               ". Saw " + state + " but expected " + gNextState);
    }

    dump("Seen " + state + " for add-on " + ADDONS[gIndex].addon + "\n");

    gNextState = "onCompatibilityCheckEnded";
  },

  onCompatibilityCheckEnded: function(aAddon, aStatus) {
    var state = "onCompatibilityCheckEnded";
    if (gNextState != state) {
      do_throw("Encountered invalid state installing add-on " + ADDONS[gIndex].addon +
               ". Saw " + state + " but expected " + gNextState);
    }

    dump("Seen " + state + " for add-on " + ADDONS[gIndex].addon + "\n");

    gNextState = "onInstallEnded";
  },

  onInstallEnded: function(aAddon, aStatus) {
    var state = "onInstallEnded";
    if (gNextState != state) {
      do_throw("Encountered invalid state installing add-on " + ADDONS[gIndex].addon +
               ". Saw " + state + " but expected " + gNextState);
    }

    if (ADDONS[gIndex].id && ADDONS[gIndex].id != aAddon.id)
      do_throw("Add-on " + ADDONS[gIndex].addon + " had an incorrect id " + aAddon.id);

    if (aStatus != ADDONS[gIndex].error)
      do_throw("Add-on " + ADDONS[gIndex].addon + " returned incorrect status " + aStatus + ".");

    dump("Seen " + state + " for add-on " + ADDONS[gIndex].addon + "\n");

    gNextState = "onInstallsCompleted";
  },

  onInstallsCompleted: function() {
    var state = "onInstallsCompleted";
    if (gNextState != state) {
      do_throw("Encountered invalid state installing add-on " + ADDONS[gIndex].addon +
               ". Saw " + state + " but expected " + gNextState);
    }

    dump("Seen " + state + " for add-on " + ADDONS[gIndex].addon + "\n");

    gNextState = null;
    gIndex++;
    installNextAddon();
  },

  onDownloadProgress: function onProgress(aAddon, aValue, aMaxValue) {
    do_throw("onDownloadProgress should not be called for a direct install");
  }
}

function installNextAddon() {
  if (gIndex >= ADDONS.length) {
    testserver.stop();
    do_test_finished();
    return;
  }

  gNextState = "onInstallStarted";
  try {
    dump("Installing add-on " + ADDONS[gIndex].addon + "\n");
    gEM.installItemFromFile(do_get_addon(ADDONS[gIndex].addon),
                            NS_INSTALL_LOCATION_APPPROFILE);
  }
  catch (e) {
    do_throw("Exception installing add-on " + ADDONS[gIndex].addon + " " + e);
  }
}

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");
  startupEM();
  gEM.addInstallListener(installListener);
  gIndex = 0;
  do_test_pending();

  // Create and configure the HTTP server.
  testserver = new nsHttpServer();
  testserver.start(4444);

  installNextAddon();
}
