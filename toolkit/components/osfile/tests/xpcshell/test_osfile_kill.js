"use strict";

Components.utils.import("resource://gre/modules/osfile.jsm");

// We want the actual global to get at the internals since Scheduler is not
// exported.
var AsyncFrontGlobal = Components.utils.import(
                         "resource://gre/modules/osfile/osfile_async_front.jsm",
                          null);
var Scheduler = AsyncFrontGlobal.Scheduler;

/**
 * Verify that Scheduler.kill() interacts with other OS.File requests correctly,
 * and that no requests are lost.  This is relevant because on B2G we
 * auto-kill the worker periodically, making it very possible for valid requests
 * to be interleaved with the automatic kill().
 *
 * This test is being created with the fix for Bug 1125989 where `kill` queue
 * management was found to be buggy.  It is a glass-box test that explicitly
 * re-creates the observed failure situation; it is not guaranteed to prevent
 * all future regressions.  The following is a detailed explanation of the test
 * for your benefit if this test ever breaks or you are wondering what was the
 * point of all this.  You might want to skim the code below first.
 *
 * OS.File maintains a `queue` of operations to be performed.  This queue is
 * nominally implemented as a chain of promises.  Every time a new job is
 * OS.File.push()ed, it effectively becomes the new `queue` promise.  (An
 * extra promise is interposed with a rejection handler to avoid the rejection
 * cascading, but that does not matter for our purposes.)
 *
 * The flaw in `kill` was that it would wait for the `queue` to complete before
 * replacing `queue`.  As a result, another OS.File operation could use `push`
 * (by way of OS.File.post()) to also use .then() on the same `queue` promise.
 * Accordingly, assuming that promise was not yet resolved (due to a pending
 * OS.File request), when it was resolved, both the task scheduled in `kill`
 * and in `post` would be triggered.  Both of those tasks would run until
 * encountering a call to worker.post().
 *
 * Re-creating this race is not entirely trivial because of the large number of
 * promises used by the code causing control flow to repeatedly be deferred.  In
 * a slightly simpler world we could run the follwing in the same turn of the
 * event loop and trigger the problem.
 * - any OS.File request
 * - Scheduler.kill()
 * - any OS.File request
 *
 * However, we need the Scheduler.kill task to reach the point where it is
 * waiting on the same `queue` that another task has been scheduled against.
 * Since the `kill` task yields on the `killQueue` promise prior to yielding
 * on `queue`, however, some turns of the event loop are required.  Happily,
 * for us, as discussed above, the problem triggers when we have two promises
 * scheduled on the `queue`, so we can just wait to schedule the second OS.File
 * request on the queue.  (Note that because of the additional then() added to
 * eat rejections, there is an important difference between the value of
 * `queue` and the value returned by the first OS.File request.)
 */
add_task(async function test_kill_race() {
  // Ensure the worker has been created and that SET_DEBUG has taken effect.
  // We have chosen OS.File.exists for our tests because it does not trigger
  // a rejection and we absolutely do not care what the operation is other
  // than it does not invoke a native fast-path.
  await OS.File.exists("foo.foo");

  info("issuing first request");
  let firstRequest = OS.File.exists("foo.bar"); // eslint-disable-line no-unused-vars
  let secondRequest;
  let secondResolved = false;

  // As noted in our big block comment, we want to wait to schedule the
  // second request so that it races `kill`'s call to `worker.post`.  Having
  // ourselves wait on the same promise, `queue`, and registering ourselves
  // before we issue the kill request means we will get run before the `kill`
  // task resumes and allow us to precisely create the desired race.
  Scheduler.queue.then(function() {
    info("issuing second request");
    secondRequest = OS.File.exists("foo.baz");
    secondRequest.then(function() {
      secondResolved = true;
    });
  });

  info("issuing kill request");
  let killRequest = Scheduler.kill({ reset: true, shutdown: false });

  // Wait on the killRequest so that we can schedule a new OS.File request
  // after it completes...
  await killRequest;
  // ...because our ordering guarantee ensures that there is at most one
  // worker (and this usage here should not be vulnerable even with the
  // bug present), so when this completes the secondRequest has either been
  // resolved or lost.
  await OS.File.exists("foo.goz");

  ok(secondResolved,
     "The second request was resolved so we avoided the bug. Victory!");
});
