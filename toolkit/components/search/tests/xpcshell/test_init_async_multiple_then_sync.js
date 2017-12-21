/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test nsSearchService with with the following initialization scenario:
 * - launch asynchronous initialization several times;
 * - force fallback to synchronous initialization.
 * - all asynchronous initializations must complete;
 * - no asynchronous initialization must complete more than once.
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
    Services.search.init(function search_initialized(aStatus) {
      Assert.ok(Components.isSuccessCode(aStatus));
      init_complete(me);
    });
  }

  // Ensure that all asynchronous initializers eventually complete
  let init_complete = function init_complete(i) {
    info("init complete " + i);
    Assert.ok(pending[i]);
    pending[i] = false;
    numberPending--;
    Assert.ok(numberPending >= 0);
    Assert.ok(Services.search.isInitialized);
    if (numberPending != 0) {
      info("Still waiting for the following initializations: " + JSON.stringify(pending));
      return;
    }
    info("All initializations have completed");
    // Just check that we can access a list of engines.
    let engines = Services.search.getEngines();
    Assert.notEqual(engines, null);

    info("Waiting a second before quitting");
    // Wait a little before quitting: if some initializer is
    // triggered twice, we want to catch that error.
    do_timeout(1000, function() {
      info("Test is complete");
      do_test_finished();
    });
  };

  // ... but don't wait for asynchronous initializations to complete
  let engines = Services.search.getEngines();
  Assert.notEqual(engines, null);
  info("Synchronous part of the test complete");
}

