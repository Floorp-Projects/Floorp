/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["makeFakeAppDir"];

const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

// Reference needed in order for fake app dir provider to be active.
var gFakeAppDirectoryProvider;

/**
 * Installs a fake UAppData directory.
 *
 * This is needed by tests because a UAppData directory typically isn't
 * present in the test environment.
 *
 * We create the new UAppData directory under the profile's directory
 * because the profile directory is automatically cleaned as part of
 * test shutdown.
 *
 * This returns a promise that will be resolved once the new directory
 * is created and installed.
 */
var makeFakeAppDir = function() {
  let dirMode = OS.Constants.libc.S_IRWXU;
  let baseFile = Services.dirsvc.get("ProfD", Ci.nsIFile);
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
    getFile(prop, persistent) {
      persistent.value = true;
      if (prop == "UAppData") {
        return appD.clone();
      }

      throw Components.Exception("", Cr.NS_ERROR_FAILURE);
    },

    QueryInterace(iid) {
      if (
        iid.equals(Ci.nsIDirectoryServiceProvider) ||
        iid.equals(Ci.nsISupports)
      ) {
        return this;
      }

      throw Components.Exception("", Cr.NS_ERROR_NO_INTERFACE);
    },
  };

  // Register the new provider.
  Services.dirsvc.registerProvider(provider);

  // And undefine the old one.
  try {
    Services.dirsvc.undefine("UAppData");
  } catch (ex) {}

  gFakeAppDirectoryProvider = provider;

  dump("Successfully installed fake UAppDir\n");
  return Promise.resolve(appD.path);
};
