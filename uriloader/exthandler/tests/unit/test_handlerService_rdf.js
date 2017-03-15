/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the handlerService interfaces using RDF backend.
 */

"use strict"

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "gHandlerService",
                                   "@mozilla.org/uriloader/handler-service;1",
                                   "nsIHandlerService");

var scriptFile = do_get_file("common_test_handlerService.js");
Services.scriptloader.loadSubScript(NetUtil.newURI(scriptFile).spec);

var prepareImportDB = Task.async(function* () {
  yield reloadData();

  let dst = HandlerServiceTest._dirSvc.get("UMimTyp", Ci.nsIFile);
  yield OS.File.copy(do_get_file("mimeTypes.rdf").path, dst.path);
  Assert.ok((yield OS.File.exists(dst.path)), "should have a DB now");
});

var removeImportDB = Task.async(function* () {
  yield reloadData();
  HandlerServiceTest._deleteDatasourceFile();
  let dst = HandlerServiceTest._dirSvc.get("UMimTyp", Ci.nsIFile);
  Assert.ok(!(yield OS.File.exists(dst.path)), "should not have a DB now");
});

var reloadData = Task.async(function* () {
  // Force the initialization of handlerService to prevent observer is not initialized yet.
  let svc = gHandlerService;
  let promise = TestUtils.topicObserved("handlersvc-rdf-replace-complete");
  Services.obs.notifyObservers(null, "handlersvc-rdf-replace", null);
  yield promise;
});
