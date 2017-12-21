/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  // If we can't get the profiler component then assume gecko was
  // built without it and pass all the tests
  var profilerCc = Cc["@mozilla.org/tools/profiler;1"];
  if (!profilerCc)
    return;

  var profiler = Cc["@mozilla.org/tools/profiler;1"].getService(Ci.nsIProfiler);
  if (!profiler)
    return;

  Assert.ok(!profiler.IsActive());

  profiler.StartProfiler(1000, 10, [], 0);

  Assert.ok(profiler.IsActive());

  do_test_pending();

  do_timeout(1000, function wait() {
    // Check text profile format
    var profileStr = profiler.GetProfile();
    Assert.ok(profileStr.length > 10);

    // check json profile format
    var profileObj = profiler.getProfileData();
    Assert.notEqual(profileObj, null);
    Assert.notEqual(profileObj.threads, null);
    Assert.ok(profileObj.threads.length >= 1);
    Assert.notEqual(profileObj.threads[0].samples, null);
    // NOTE: The number of samples will be empty since we
    //       don't have any labels in the xpcshell code

    profiler.StopProfiler();
    Assert.ok(!profiler.IsActive());
    do_test_finished();
  });


}
