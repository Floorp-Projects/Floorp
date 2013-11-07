/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests getIcons() and getIconURLBySize() on engine with multiple icons.
 */

"use strict";

const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://testing-common/httpd.js");

  function test_multiIcon() {
  let engine = Services.search.getEngineByName("IconsTest");
  do_check_neq(engine, null);

  do_print("Running tests on IconsTest engine");

  do_print("The default should be the 16x16 icon");
  do_check_true(engine.iconURI.spec.contains("ico16"));

  do_check_true(engine.getIconURLBySize(32,32).contains("ico32"));
  do_check_true(engine.getIconURLBySize(74,74).contains("ico74"));

  do_print("Invalid dimensions should return null.");
  do_check_null(engine.getIconURLBySize(50,50));

  let allIcons = engine.getIcons();

  do_print("Check that allIcons contains expected icon sizes");
  do_check_eq(allIcons.length, 3);
  let expectedWidths = [16, 32, 74];
  do_check_true(allIcons.every((item) => {
    let width = item.width;
    do_check_neq(expectedWidths.indexOf(width), -1);
    do_check_eq(width, item.height);

    let icon = item.url.split(",").pop();
    do_check_eq(icon, "ico" + width);

    return true;
  }));

  do_test_finished();
}

function run_test() {
  removeMetadata();
  updateAppInfo();

  let httpServer = new HttpServer();
  httpServer.start(4444);
  httpServer.registerDirectory("/", do_get_cwd());

  do_register_cleanup(function cleanup() {
    httpServer.stop(function() {});
  });

  do_test_pending();

  let observer = function(aSubject, aTopic, aData) {
    if (aData == "engine-loaded") {
      test_multiIcon();
    }
  };

  Services.obs.addObserver(observer, "browser-search-engine-modified", false);

  do_print("Adding engine with multiple images");
  Services.search.addEngine("http://localhost:4444/data/engineImages.xml",
    Ci.nsISearchEngine.DATA_XML, null, false);

  do_timeout(12000, function() {
    do_throw("Timeout");
  });
}
