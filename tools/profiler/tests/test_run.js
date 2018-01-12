/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  if (!AppConstants.MOZ_GECKO_PROFILER) {
    return;
  }

  Assert.ok(!Services.profiler.IsActive());

  Services.profiler.StartProfiler(1000, 10, [], 0);

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
    Assert.ok(profileObj.threads.length >= 1);
    Assert.notEqual(profileObj.threads[0].samples, null);
    // NOTE: The number of samples will be empty since we
    //       don't have any labels in the xpcshell code

    Services.profiler.StopProfiler();
    Assert.ok(!Services.profiler.IsActive());
    do_test_finished();
  });


}
