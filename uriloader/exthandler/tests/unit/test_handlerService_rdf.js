/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the handlerService interfaces using RDF backend.
 */

XPCOMUtils.defineLazyServiceGetter(this, "gHandlerService",
                                   "@mozilla.org/uriloader/handler-service;1",
                                   "nsIHandlerService");

var scriptFile = do_get_file("common_test_handlerService.js");
Services.scriptloader.loadSubScript(NetUtil.newURI(scriptFile).spec);

var prepareImportDB = Task.async(function* () {
  yield reloadData();

  let fileName = AppConstants.platform == "android" ? "mimeTypes-android.rdf"
                                                    : "mimeTypes.rdf";
  yield OS.File.copy(do_get_file(fileName).path, rdfFile.path);
});

var removeImportDB = Task.async(function* () {
  yield reloadData();

  yield OS.File.remove(rdfFile.path, { ignoreAbsent: true });
});

var reloadData = Task.async(function* () {
  // Force the initialization of handlerService to prevent observer is not initialized yet.
  let svc = gHandlerService;
  let promise = TestUtils.topicObserved("handlersvc-rdf-replace-complete");
  Services.obs.notifyObservers(null, "handlersvc-rdf-replace");
  yield promise;
});
