/* -*- Mode: javascript; js-indent-level: 2; -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that we can nest event loops when needed in
// ThreadActor.prototype.synchronize.

const { defer } = devtools.require("sdk/core/promise");

function run_test() {
  initTestDebuggerServer();
  do_test_pending();
  test_nesting();
}

function test_nesting() {
  const thread = new DebuggerServer.ThreadActor(DebuggerServer);
  const { resolve, reject, promise } = defer();

  let currentStep = 0;

  executeSoon(function () {
    // Should be on the first step
    do_check_eq(++currentStep, 1);
    // We should have one nested event loop from synchronize
    do_check_eq(thread._nestedEventLoops.size, 1);
    resolve(true);
  });

  do_check_eq(thread.synchronize(promise), true);

  // Should be on the second step
  do_check_eq(++currentStep, 2);
  // There shouldn't be any nested event loops anymore
  do_check_eq(thread._nestedEventLoops.size, 0);

  do_test_finished();
}
