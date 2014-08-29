/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/CloudSync.jsm");

function run_test() {
  run_next_test();
}

add_task(function test_lazyload() {
  ok(!CloudSync.ready, "CloudSync.ready is false before CloudSync() invoked");
  let cs1 = CloudSync();
  ok(CloudSync.ready, "CloudSync.ready is true after CloudSync() invoked");
  let cs2 = CloudSync();
  ok(cs1 === cs2, "CloudSync() returns the same instance on multiple invocations");
});
