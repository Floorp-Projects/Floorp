/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Initialization for tests related to invoking external handler applications.
 */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

Cu.import("resource://testing-common/HandlerServiceTestUtils.jsm", this);
Cu.import("resource://testing-common/TestUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "gHandlerServiceJSON",
                                   "@mozilla.org/uriloader/handler-service;1",
                                   "nsIHandlerService");
XPCOMUtils.defineLazyServiceGetter(this, "gHandlerServiceRDF",
                                   "@mozilla.org/uriloader/handler-service-rdf;1",
                                   "nsIHandlerService");

do_get_profile();

let jsonPath = OS.Path.join(OS.Constants.Path.profileDir, "handlers.json");
let rdfFile = FileUtils.getFile("ProfD", ["mimeTypes.rdf"]);

/**
 * Unloads the nsIHandlerService data store, so the back-end file can be
 * accessed or modified, and the new data will be loaded at the next access.
 */
let unloadHandlerStoreJSON = async function() {
  // If this function is called before the nsIHandlerService instance has been
  // initialized for the first time, the observer below will not be registered.
  // We have to force initialization to prevent the function from stalling.
  gHandlerServiceJSON;

  let promise = TestUtils.topicObserved("handlersvc-json-replace-complete");
  Services.obs.notifyObservers(null, "handlersvc-json-replace", null);
  await promise;
};
let unloadHandlerStoreRDF = async function() {
  // If this function is called before the nsIHandlerService instance has been
  // initialized for the first time, the observer below will not be registered.
  // We have to force initialization to prevent the function from stalling.
  gHandlerServiceRDF;

  let promise = TestUtils.topicObserved("handlersvc-rdf-replace-complete");
  Services.obs.notifyObservers(null, "handlersvc-rdf-replace", null);
  await promise;
};

/**
 * Unloads the data store and deletes it.
 */
let deleteHandlerStoreJSON = async function() {
  await unloadHandlerStoreJSON();

  await OS.File.remove(jsonPath, { ignoreAbsent: true });
};
let deleteHandlerStoreRDF = async function() {
  await unloadHandlerStoreRDF();

  await OS.File.remove(rdfFile.path, { ignoreAbsent: true });
};

/**
 * Unloads the data store and replaces it with the test data file.
 */
let copyTestDataToHandlerStoreJSON = async function() {
  await unloadHandlerStoreJSON();

  await OS.File.copy(do_get_file("handlers.json").path, jsonPath);
};
let copyTestDataToHandlerStoreRDF = async function() {
  await unloadHandlerStoreRDF();

  let fileName = AppConstants.platform == "android" ? "mimeTypes-android.rdf"
                                                    : "mimeTypes.rdf";
  await OS.File.copy(do_get_file(fileName).path, rdfFile.path);
};

/**
 * Ensures the JSON implementation doesn't migrate entries from the legacy RDF
 * data source during the other tests. This is important for both back-ends,
 * because the JSON implementation is the default one and is always invoked by
 * the MIME service when building new nsIHandlerInfo objects.
 */
add_task(async function test_initialize() {
  // We don't need to reset this preference when the tests end, because it's
  // irrelevant for any other test in the tree.
  Services.prefs.setBoolPref("gecko.handlerService.migrated", true);
});

/**
 * Ensures the files are removed and the services unloaded when the tests end.
 */
registerCleanupFunction(async function test_terminate() {
  await deleteHandlerStoreJSON();
  await deleteHandlerStoreRDF();
});
