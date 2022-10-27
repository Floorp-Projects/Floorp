/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(function test_queue_longer_than_1k() {
  // FOG needs a profile directory to put its data in.
  do_get_profile();

  // Before init, try and fill the preinit queue with > 1000 tasks.
  const kIterations = 2000;
  for (let _i = 0; _i < kIterations; _i++) {
    Glean.testOnly.badCode.add(1);
  }

  Services.fog.initializeFOG();

  Assert.equal(kIterations, Glean.testOnly.badCode.testGetValue());
});
