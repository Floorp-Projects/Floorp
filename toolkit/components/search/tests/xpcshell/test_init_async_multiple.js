/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test nsSearchService with with the following initialization scenario:
 * - launch asynchronous initialization several times;
 * - all asynchronous initializations must complete.
 *
 * Test case comes from test_645970.js
 */
function run_test() {
  info("Setting up test");

  do_test_pending();

  info("Test starting");
  let numberOfInitializers = 4;
  let pending = [];
  let numberPending = numberOfInitializers;

  // Start asynchronous initializations
  for (let i = 0; i < numberOfInitializers; ++i) {
    let me = i;
    pending[me] = true;
    Services.search.init(function search_initialized_0(aStatus) {
      Assert.ok(Components.isSuccessCode(aStatus));
      init_complete(me);
    });
  }

  // Wait until all initializers have completed
  let init_complete = function init_complete(i) {
    Assert.ok(pending[i]);
    pending[i] = false;
    numberPending--;
    Assert.ok(numberPending >= 0);
    Assert.ok(Services.search.isInitialized);
    if (numberPending == 0) {
      // Just check that we can access a list of engines.
      let engines = Services.search.getEngines();
      Assert.notEqual(engines, null);

      // Wait a little before quitting: if some initializer is
      // triggered twice, we want to catch that error.
      do_timeout(1000, function() {
        do_test_finished();
      });
    }
  };
}

