/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Initialization for tests related to invoking external handler applications.
 */

"use strict";

var { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
var { FileUtils } = ChromeUtils.import("resource://gre/modules/FileUtils.jsm");
var { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
var { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const { HandlerServiceTestUtils } = ChromeUtils.import(
  "resource://testing-common/HandlerServiceTestUtils.jsm"
);
var { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gHandlerService",
  "@mozilla.org/uriloader/handler-service;1",
  "nsIHandlerService"
);

do_get_profile();

let jsonPath = PathUtils.join(PathUtils.profileDir, "handlers.json");

/**
 * Unloads the nsIHandlerService data store, so the back-end file can be
 * accessed or modified, and the new data will be loaded at the next access.
 */
let unloadHandlerStore = async function() {
  // If this function is called before the nsIHandlerService instance has been
  // initialized for the first time, the observer below will not be registered.
  // We have to force initialization to prevent the function from stalling.
  gHandlerService;

  let promise = TestUtils.topicObserved("handlersvc-json-replace-complete");
  Services.obs.notifyObservers(null, "handlersvc-json-replace");
  await promise;
};

/**
 * Unloads the data store and deletes it.
 */
let deleteHandlerStore = async function() {
  await unloadHandlerStore();

  await IOUtils.remove(jsonPath, { ignoreAbsent: true });

  Services.prefs.clearUserPref("gecko.handlerService.defaultHandlersVersion");
};

/**
 * Unloads the data store and replaces it with the test data file.
 */
let copyTestDataToHandlerStore = async function() {
  await unloadHandlerStore();

  await IOUtils.copy(do_get_file("handlers.json").path, jsonPath);

  Services.prefs.setIntPref("gecko.handlerService.defaultHandlersVersion", 100);
};

/**
 * Ensures the files are removed and the services unloaded when the tests end.
 */
registerCleanupFunction(async function test_terminate() {
  await deleteHandlerStore();
});
