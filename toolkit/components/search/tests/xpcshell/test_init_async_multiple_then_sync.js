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
  do_print("Setting up test");
  do_test_pending();

  do_print("Test starting");

  let numberOfInitializers = 4;
  let pending = [];
  let numberPending = numberOfInitializers;

  // Start asynchronous initializations
  for (let i = 0; i < numberOfInitializers; ++i) {
    let me = i;
    pending[me] = true;
    Services.search.init(function search_initialized(aStatus) {
      do_check_true(Components.isSuccessCode(aStatus));
      init_complete(me);
    });
  }

  // Ensure that all asynchronous initializers eventually complete
  let init_complete = function init_complete(i) {
    do_print("init complete " + i);
    do_check_true(pending[i]);
    pending[i] = false;
    numberPending--;
    do_check_true(numberPending >= 0);
    do_check_true(Services.search.isInitialized);
    if (numberPending != 0) {
      do_print("Still waiting for the following initializations: " + JSON.stringify(pending));
      return;
    }
    do_print("All initializations have completed");
    // Just check that we can access a list of engines.
    let engines = Services.search.getEngines();
    do_check_neq(engines, null);

    do_print("Waiting a second before quitting");
    // Wait a little before quitting: if some initializer is
    // triggered twice, we want to catch that error.
    do_timeout(1000, function() {
      do_print("Test is complete");
      do_test_finished();
    });
  };

  // ... but don't wait for asynchronous initializations to complete
  let engines = Services.search.getEngines();
  do_check_neq(engines, null);
  do_print("Synchronous part of the test complete");
}

