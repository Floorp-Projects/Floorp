/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// FOG needs a profile directory to put its data in.
do_get_profile();

const FOG = Cc["@mozilla.org/toolkit/glean;1"].createInstance(Ci.nsIFOG);
// We need to initialize it once, otherwise operations will be stuck in the pre-init queue.
FOG.initializeFOG();

add_task(function test_fog_experiment_annotations() {
  const id = "my-experiment-id";
  const branch = "my-branch";
  const extra = { extra_key: "extra_value" };
  FOG.setExperimentActive(id, branch, extra);

  let data = FOG.testGetExperimentData(id);
  Assert.equal(data.branch, branch);
  Assert.deepEqual(data.extra, extra);

  // Unknown id gets nothing.
  Assert.equal(undefined, FOG.testGetExperimentData(id + id));

  // Inactive id gets nothing.
  FOG.setExperimentInactive(id);
  Assert.equal(undefined, FOG.testGetExperimentData(id));
});
