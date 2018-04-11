/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

Cu.importGlobalProperties(["URL"]);

ChromeUtils.import("chrome://marionette/content/navigate.js");

add_test(function test_isLoadEventExpected() {
  Assert.throws(() => navigate.isLoadEventExpected(undefined),
      /Expected at least one URL/);

  equal(true, navigate.isLoadEventExpected("http://a/"));
  equal(true, navigate.isLoadEventExpected("http://a/", "http://a/"));

  equal(true, navigate.isLoadEventExpected("http://a/", "http://a/#"));
  equal(true, navigate.isLoadEventExpected("http://a/#", "http://a/"));
  equal(true, navigate.isLoadEventExpected("http://a/#a", "http://a/#A"));
  equal(false, navigate.isLoadEventExpected("http://a/#a", "http://a/#a"));

  equal(false, navigate.isLoadEventExpected("http://a/", "javascript:whatever"));

  run_next_test();
});
