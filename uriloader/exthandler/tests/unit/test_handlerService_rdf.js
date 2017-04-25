/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests the nsIHandlerService interface using the JSON backend.
 */

XPCOMUtils.defineLazyServiceGetter(this, "gHandlerService",
                                   "@mozilla.org/uriloader/handler-service;1",
                                   "nsIHandlerService");

/**
 * Unloads the nsIHandlerService data store, so the back-end file can be
 * accessed or modified, and the new data will be loaded at the next access.
 */
let unloadHandlerStore = Task.async(function* () {
  // If this function is called before the nsIHandlerService instance has been
  // initialized for the first time, the observer below will not be registered.
  // We have to force initialization to prevent the function from stalling.
  gHandlerService;

  let promise = TestUtils.topicObserved("handlersvc-rdf-replace-complete");
  Services.obs.notifyObservers(null, "handlersvc-rdf-replace");
  yield promise;
});

/**
 * Unloads the data store and deletes it.
 */
let deleteHandlerStore = Task.async(function* () {
  yield unloadHandlerStore();

  yield OS.File.remove(rdfFile.path, { ignoreAbsent: true });
});

/**
 * Unloads the data store and replaces it with the test data file.
 */
let copyTestDataToHandlerStore = Task.async(function* () {
  yield unloadHandlerStore();

  let fileName = AppConstants.platform == "android" ? "mimeTypes-android.rdf"
                                                    : "mimeTypes.rdf";
  yield OS.File.copy(do_get_file(fileName).path, rdfFile.path);
});

var scriptFile = do_get_file("common_test_handlerService.js");
Services.scriptloader.loadSubScript(NetUtil.newURI(scriptFile).spec);
