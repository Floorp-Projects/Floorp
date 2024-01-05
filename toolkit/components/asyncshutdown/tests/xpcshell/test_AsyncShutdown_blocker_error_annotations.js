/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that when addBlocker fails, we store that failure internally
 * and include its information in crash report annotation information.
 */
add_task(async function test_addBlockerFailureState() {
  info("Testing addBlocker information reported to crash reporter");

  let BLOCKER_NAME = "test_addBlocker_state blocker " + Math.random();

  // Set up the barrier. Note that we cannot test `barrier.state`
  // immediately, as it initially contains "Not started"
  let barrier = new AsyncShutdown.Barrier("test_addBlocker_failure");
  let deferred = Promise.withResolvers();
  barrier.client.addBlocker(BLOCKER_NAME, function () {
    return deferred.promise;
  });

  // Add a blocker and confirm that throws.
  const THROWING_BLOCKER_NAME = "test_addBlocker_throws blocker";
  Assert.throws(() => {
    barrier.client.addBlocker(THROWING_BLOCKER_NAME, Promise.resolve(), 5);
  }, /object as third argument/);

  let promiseDone = barrier.wait();

  // Now that we have called `wait()`, the state should match crash
  // reporting info
  let crashInfo = barrier._gatherCrashReportTimeoutData(
    barrier._name,
    barrier.state
  );
  Assert.deepEqual(
    crashInfo.conditions,
    barrier.state,
    "Barrier state should match crash info."
  );
  Assert.equal(
    crashInfo.brokenAddBlockers.length,
    1,
    "Should have registered the broken addblocker call."
  );
  Assert.stringMatches(
    crashInfo.brokenAddBlockers?.[0] || "undefined",
    THROWING_BLOCKER_NAME,
    "Throwing call's blocker name should be listed in message."
  );

  deferred.resolve();
  await promiseDone;
});
