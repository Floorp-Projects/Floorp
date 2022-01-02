/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  Assert.ok(!Services.profiler.IsActive());

  Services.profiler.StartProfiler(1000, 10, []);

  Assert.ok(Services.profiler.IsActive());

  do_test_pending();

  do_timeout(1000, function wait() {
    // Check text profile format
    var profileStr = Services.profiler.GetProfile();
    Assert.ok(profileStr.length > 10);

    // check json profile format
    var profileObj = Services.profiler.getProfileData();
    Assert.notEqual(profileObj, null);
    Assert.notEqual(profileObj.threads, null);
    // We capture memory counters by default only when jemalloc is turned
    // on (and it isn't for ASAN), so unless we can conditionalize for ASAN
    // here we can't check that we're capturing memory counter data.
    Assert.notEqual(profileObj.counters, null);
    Assert.notEqual(profileObj.memory, null);
    Assert.ok(profileObj.threads.length >= 1);
    Assert.notEqual(profileObj.threads[0].samples, null);
    // NOTE: The number of samples will be empty since we
    //       don't have any labels in the xpcshell code

    Services.profiler.StopProfiler();
    Assert.ok(!Services.profiler.IsActive());
    do_test_finished();
  });
}
