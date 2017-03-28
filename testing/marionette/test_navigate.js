/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const {utils: Cu} = Components;

Cu.importGlobalProperties(["URL"]);

Cu.import("chrome://marionette/content/navigate.js");

add_test(function test_isLoadEventExpected() {
  Assert.throws(() => navigate.isLoadEventExpected(undefined),
      /Expected destination URL/);

  equal(true, navigate.isLoadEventExpected("http://a/"));
  equal(false, navigate.isLoadEventExpected("javascript:whatever"));

  run_next_test();
});
