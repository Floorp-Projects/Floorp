/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/Services.jsm");

function getMainThreadHangStats() {
  let threads = Services.telemetry.threadHangStats;
  return threads.find((thread) => (thread.name === "Gecko"));
}

function run_test() {
  let startHangs = getMainThreadHangStats();

  // We disable hang reporting in several situations (e.g. debug builds,
  // official releases). In those cases, we don't have hang stats available
  // and should exit the test early.
  if (!startHangs) {
    ok("Hang reporting not enabled.");
    return;
  }

  if (Services.appinfo.OS === "Linux" || Services.appinfo.OS === "Android") {
    // We use the rt_tgsigqueueinfo syscall on Linux which requires a
    // certain kernel version. It's not an error if the system running
    // the test is older than that.
    let kernel = Services.sysinfo.get("kernel_version") ||
                 Services.sysinfo.get("version");
    if (Services.vc.compare(kernel, "2.6.31") < 0) {
      ok("Hang reporting not supported for old kernel.");
      return;
    }
  }

  // Run three events in the event loop:
  // the first event causes a transient hang;
  // the second event causes a permanent hang;
  // the third event checks results from previous events.

  do_execute_soon(() => {
    // Cause a hang lasting 1 second (transient hang).
    let startTime = Date.now();
    while ((Date.now() - startTime) < 1000);
  });

  do_execute_soon(() => {
    // Cause a hang lasting 10 seconds (permanent hang).
    let startTime = Date.now();
    while ((Date.now() - startTime) < 10000);
  });

  do_execute_soon(() => {
    do_test_pending();

    let check_results = () => {
      let endHangs = getMainThreadHangStats();

      // Because hangs are recorded asynchronously, if we don't see new hangs,
      // we should wait for pending hangs to be recorded. On the other hand,
      // if hang monitoring is broken, this test will time out.
      if (endHangs.hangs.length === startHangs.hangs.length) {
        do_timeout(100, check_results);
        return;
      }

      let check_histogram = (histogram) => {
        equal(typeof histogram, "object");
        equal(histogram.histogram_type, 0);
        equal(typeof histogram.min, "number");
        equal(typeof histogram.max, "number");
        equal(typeof histogram.sum, "number");
        ok(Array.isArray(histogram.ranges));
        ok(Array.isArray(histogram.counts));
        equal(histogram.counts.length, histogram.ranges.length);
      };

      // Make sure the hang stats structure is what we expect.
      equal(typeof endHangs, "object");
      check_histogram(endHangs.activity);

      ok(Array.isArray(endHangs.hangs));
      notEqual(endHangs.hangs.length, 0);

      ok(Array.isArray(endHangs.hangs[0].stack));
      notEqual(endHangs.hangs[0].stack.length, 0);
      equal(typeof endHangs.hangs[0].stack[0], "string");

      // Make sure there is a nativeStacks field with native stacks in it.
      ok(Array.isArray(endHangs.nativeStacks.memoryMap));
      ok(Array.isArray(endHangs.nativeStacks.stacks));
      ok(endHangs.nativeStacks.stacks.length > 0);

      // Make sure that at least one of the hangs contains a native stack.
      ok(endHangs.hangs.some((hang) => {
        return typeof hang.nativeStack == "number" &&
          hang.nativeStack < endHangs.nativeStacks.stacks.length &&
          Array.isArray(endHangs.nativeStacks.stacks[hang.nativeStack]);
      }));

      check_histogram(endHangs.hangs[0].histogram);

      do_test_finished();
    };

    check_results();
  });
}
