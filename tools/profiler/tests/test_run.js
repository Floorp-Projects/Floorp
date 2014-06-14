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

  do_check_true(!profiler.IsActive());

  profiler.StartProfiler(1000, 10, [], 0);

  do_check_true(profiler.IsActive());

  do_test_pending();

  do_timeout(1000, function wait() {
    // Check text profile format
    var profileStr = profiler.GetProfile();
    do_check_true(profileStr.length > 10);

    // check json profile format
    var profileObj = profiler.getProfileData();
    do_check_neq(profileObj, null);
    do_check_neq(profileObj.threads, null);
    do_check_true(profileObj.threads.length >= 1);
    do_check_neq(profileObj.threads[0].samples, null);
    // NOTE: The number of samples will be empty since we
    //       don't have any labels in the xpcshell code

    profiler.StopProfiler();
    do_check_true(!profiler.IsActive());
    do_test_finished();
  });


}
