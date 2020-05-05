/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["MockPermissionPrompt"];

const Cm = Components.manager;

const CONTRACT_ID = "@mozilla.org/content-permission/prompt;1";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

var registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);
var oldClassID, oldFactory;
var newClassID = Cc["@mozilla.org/uuid-generator;1"]
  .getService(Ci.nsIUUIDGenerator)
  .generateUUID();
var newFactory = {
  createInstance(aOuter, aIID) {
    if (aOuter) {
      throw Components.Exception("", Cr.NS_ERROR_NO_AGGREGATION);
    }
    return new MockPermissionPromptInstance().QueryInterface(aIID);
  },
  lockFactory(aLock) {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  },
  QueryInterface: ChromeUtils.generateQI([Ci.nsIFactory]),
};

var MockPermissionPrompt = {
  init() {
    this.reset();
    if (!registrar.isCIDRegistered(newClassID)) {
      try {
        oldClassID = registrar.contractIDToCID(CONTRACT_ID);
        oldFactory = Cm.getClassObject(Cc[CONTRACT_ID], Ci.nsIFactory);
      } catch (ex) {
        oldClassID = "";
        oldFactory = null;
        dump(
          "TEST-INFO | can't get permission prompt registered component, " +
            "assuming there is none"
        );
      }
      if (oldFactory) {
        registrar.unregisterFactory(oldClassID, oldFactory);
      }
      registrar.registerFactory(newClassID, "", CONTRACT_ID, newFactory);
    }
  },

  reset() {},

  cleanup() {
    this.reset();
    if (oldFactory) {
      registrar.unregisterFactory(newClassID, newFactory);
      registrar.registerFactory(oldClassID, "", CONTRACT_ID, oldFactory);
    }
  },
};

function MockPermissionPromptInstance() {}
MockPermissionPromptInstance.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIContentPermissionPrompt]),

  promptResult: Ci.nsIPermissionManager.UNKNOWN_ACTION,

  prompt(request) {
    let perms = request.types.QueryInterface(Ci.nsIArray);
    for (let idx = 0; idx < perms.length; idx++) {
      let perm = perms.queryElementAt(idx, Ci.nsIContentPermissionType);
      if (
        Services.perms.testExactPermissionFromPrincipal(
          request.principal,
          perm.type
        ) != Ci.nsIPermissionManager.ALLOW_ACTION
      ) {
        request.cancel();
        return;
      }
    }

    request.allow();
  },
};
