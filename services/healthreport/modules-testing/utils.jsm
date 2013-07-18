/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "getAppInfo",
  "updateAppInfo",
  "makeFakeAppDir",
  "createFakeCrash",
  "InspectedHealthReporter",
  "getHealthReporter",
];


const {classes: Cc, interfaces: Ci, results: Cr, utils: Cu} = Components;

Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/services-common/utils.js");
Cu.import("resource://gre/modules/services/datareporting/policy.jsm");
Cu.import("resource://gre/modules/services/healthreport/healthreporter.jsm");
Cu.import("resource://gre/modules/services/healthreport/providers.jsm");


let APP_INFO = {
  vendor: "Mozilla",
  name: "xpcshell",
  ID: "xpcshell@tests.mozilla.org",
  version: "1",
  appBuildID: "20121107",
  platformVersion: "p-ver",
  platformBuildID: "20121106",
  inSafeMode: false,
  logConsoleErrors: true,
  OS: "XPCShell",
  XPCOMABI: "noarch-spidermonkey",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIXULAppInfo, Ci.nsIXULRuntime]),
  invalidateCachesOnRestart: function() {},
};


/**
 * Obtain a reference to the current object used to define XULAppInfo.
 */
this.getAppInfo = function () { return APP_INFO; }

/**
 * Update the current application info.
 *
 * If the argument is defined, it will be the object used. Else, APP_INFO is
 * used.
 *
 * To change the current XULAppInfo, simply call this function. If there was
 * a previously registered app info object, it will be unloaded and replaced.
 */
this.updateAppInfo = function (obj) {
  obj = obj || APP_INFO;
  APP_INFO = obj;

  let id = Components.ID("{fbfae60b-64a4-44ef-a911-08ceb70b9f31}");
  let cid = "@mozilla.org/xre/app-info;1";
  let registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);

  // Unregister an existing factory if one exists.
  try {
    let existing = Components.manager.getClassObjectByContractID(cid, Ci.nsIFactory);
    registrar.unregisterFactory(id, existing);
  } catch (ex) {}

  let factory = {
    createInstance: function (outer, iid) {
      if (outer != null) {
        throw Cr.NS_ERROR_NO_AGGREGATION;
      }

      return obj.QueryInterface(iid);
    },
  };

  registrar.registerFactory(id, "XULAppInfo", cid, factory);
};

// Reference needed in order for fake app dir provider to be active.
let gFakeAppDirectoryProvider;

/**
 * Installs a fake UAppData directory.
 *
 * This is needed by tests because a UAppData directory typically isn't
 * present in the test environment.
 *
 * This function is suitable for use in different components. If we ever
 * establish a central location for convenient test helpers, this should
 * go there.
 *
 * We create the new UAppData directory under the profile's directory
 * because the profile directory is automatically cleaned as part of
 * test shutdown.
 *
 * This returns a promise that will be resolved once the new directory
 * is created and installed.
 */
this.makeFakeAppDir = function () {
  let dirMode = OS.Constants.libc.S_IRWXU;
  let dirService = Cc["@mozilla.org/file/directory_service;1"]
                     .getService(Ci.nsIProperties);
  let baseFile = dirService.get("ProfD", Ci.nsIFile);
  let appD = baseFile.clone();
  appD.append("UAppData");

  if (gFakeAppDirectoryProvider) {
    return Promise.resolve(appD.path);
  }

  function makeDir(f) {
    if (f.exists()) {
      return;
    }

    dump("Creating directory: " + f.path + "\n");
    f.create(Ci.nsIFile.DIRECTORY_TYPE, dirMode);
  }

  makeDir(appD);

  let reportsD = appD.clone();
  reportsD.append("Crash Reports");

  let pendingD = reportsD.clone();
  pendingD.append("pending");
  let submittedD = reportsD.clone();
  submittedD.append("submitted");

  makeDir(reportsD);
  makeDir(pendingD);
  makeDir(submittedD);

  let provider = {
    getFile: function (prop, persistent) {
      persistent.value = true;
      if (prop == "UAppData") {
        return appD.clone();
      }

      throw Cr.NS_ERROR_FAILURE;
    },

    QueryInterace: function (iid) {
      if (iid.equals(Ci.nsIDirectoryServiceProvider) ||
          iid.equals(Ci.nsISupports)) {
        return this;
      }

      throw Cr.NS_ERROR_NO_INTERFACE;
    },
  };

  // Register the new provider.
  dirService.QueryInterface(Ci.nsIDirectoryService)
            .registerProvider(provider);

  // And undefine the old one.
  try {
    dirService.undefine("UAppData");
  } catch (ex) {};

  gFakeAppDirectoryProvider = provider;

  dump("Successfully installed fake UAppDir\n");
  return Promise.resolve(appD.path);
};


/**
 * Creates a fake crash in the Crash Reports directory.
 *
 * Currently, we just create a dummy file. A more robust implementation would
 * create something that actually resembles a crash report file.
 *
 * This is very similar to code in crashreporter/tests/browser/head.js.
 *
 * FUTURE consolidate code in a shared JSM.
 */
this.createFakeCrash = function (submitted=false, date=new Date()) {
  let id = CommonUtils.generateUUID();
  let filename;

  let paths = ["Crash Reports"];
  let mode;

  if (submitted) {
    paths.push("submitted");
    filename = "bp-" + id + ".txt";
    mode = OS.Constants.libc.S_IRUSR | OS.Constants.libc.S_IWUSR |
           OS.Constants.libc.S_IRGRP | OS.Constants.libc.S_IROTH;
  } else {
    paths.push("pending");
    filename = id + ".dmp";
    mode = OS.Constants.libc.S_IRUSR | OS.Constants.libc.S_IWUSR;
  }

  paths.push(filename);

  let file = FileUtils.getFile("UAppData", paths, true);
  file.create(file.NORMAL_FILE_TYPE, mode);
  file.lastModifiedTime = date.getTime();
  dump("Created fake crash: " + id + "\n");

  return id;
};


/**
 * A HealthReporter that is probed with various callbacks and counters.
 *
 * The purpose of this type is to aid testing of startup and shutdown.
 */
this.InspectedHealthReporter = function (branch, policy, recorder, stateLeaf) {
  HealthReporter.call(this, branch, policy, recorder, stateLeaf);

  this.onStorageCreated = null;
  this.onProviderManagerInitialized = null;
  this.providerManagerShutdownCount = 0;
  this.storageCloseCount = 0;
}

InspectedHealthReporter.prototype = {
  __proto__: HealthReporter.prototype,

  _onStorageCreated: function (storage) {
    if (this.onStorageCreated) {
      this.onStorageCreated(storage);
    }

    return HealthReporter.prototype._onStorageCreated.call(this, storage);
  },

  _initializeProviderManager: function () {
    for (let result of HealthReporter.prototype._initializeProviderManager.call(this)) {
      yield result;
    }

    if (this.onInitializeProviderManagerFinished) {
      this.onInitializeProviderManagerFinished();
    }
  },

  _onProviderManagerInitialized: function () {
    if (this.onProviderManagerInitialized) {
      this.onProviderManagerInitialized();
    }

    return HealthReporter.prototype._onProviderManagerInitialized.call(this);
  },

  _onProviderManagerShutdown: function () {
    this.providerManagerShutdownCount++;

    return HealthReporter.prototype._onProviderManagerShutdown.call(this);
  },

  _onStorageClose: function () {
    this.storageCloseCount++;

    return HealthReporter.prototype._onStorageClose.call(this);
  },
};

const DUMMY_URI="http://localhost:62013/";

this.getHealthReporter = function (name, uri=DUMMY_URI, inspected=false) {
  let branch = "healthreport.testing." + name + ".";

  let prefs = new Preferences(branch + "healthreport.");
  prefs.set("documentServerURI", uri);
  prefs.set("dbName", name);

  let reporter;

  let policyPrefs = new Preferences(branch + "policy.");
  let policy = new DataReportingPolicy(policyPrefs, prefs, {
    onRequestDataUpload: function (request) {
      reporter.requestDataUpload(request);
    },

    onNotifyDataPolicy: function (request) { },

    onRequestRemoteDelete: function (request) {
      reporter.deleteRemoteData(request);
    },
  });

  let type = inspected ? InspectedHealthReporter : HealthReporter;
  reporter = new type(branch + "healthreport.", policy, null,
                      "state-" + name + ".json");

  return reporter;
};
