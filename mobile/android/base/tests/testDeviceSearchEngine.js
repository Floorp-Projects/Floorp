// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/Services.jsm");

var Cc = Components.classes;
var Ci = Components.interfaces;

function search_observer(aSubject, aTopic, aData) {
  let engine = aSubject.QueryInterface(Ci.nsISearchEngine);
  do_print("Observer: " + aData + " for " + engine.name);

  if (aData != "engine-added")
    return;

  if (engine.name != "Test search engine")
    return;

  function check_submission(aExpected, aSearchTerm, aType) {
    do_check_eq(engine.getSubmission(aSearchTerm, aType).uri.spec, "http://example.com/search" + aExpected);
  }

  // Force the type and check for the expected URL
  check_submission("?q=foo", "foo", "text/html");
  check_submission("/tablet?q=foo", "foo", "application/x-moz-tabletsearch");
  check_submission("/phone?q=foo", "foo", "application/x-moz-phonesearch");

  // Let the service pick the appropriate type based on the device
  // and check for expected URL
  let sysInfo = Cc["@mozilla.org/system-info;1"].getService(Ci.nsIPropertyBag2);
  if (sysInfo.get("tablet")) {
    do_print("Device: tablet");
    check_submission("/tablet?q=foo", "foo", null);
  } else {
    do_print("Device: phone");
    check_submission("/phone?q=foo", "foo", null);
  }

  run_next_test();
};

add_test(function test_default() {
  do_register_cleanup(function cleanup() {
    Services.obs.removeObserver(search_observer, "browser-search-engine-modified");
  });

  Services.obs.addObserver(search_observer, "browser-search-engine-modified", false);

  do_print("Loading search engine");
  Services.search.addEngine("http://mochi.test:8888/tests/robocop/devicesearch.xml", Ci.nsISearchEngine.DATA_XML, null, false);
});

run_next_test();
