/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  // If we can't get the profiler component then assume gecko was
  // built without it and pass all the tests
  var profilerCc = Cc["@mozilla.org/tools/profiler;1"];
  if (!profilerCc)
    return;

  var profiler = profilerCc.getService(Ci.nsIProfiler);
  if (!profiler)
    return;

  do_check_true(!profiler.IsActive());
  do_check_true(!profiler.IsPaused());

  profiler.StartProfiler(1000, 10, [], 0);

  do_check_true(profiler.IsActive());

  profiler.PauseSampling();

  do_check_true(profiler.IsPaused());

  profiler.ResumeSampling();

  do_check_true(!profiler.IsPaused());

  profiler.StopProfiler();
  do_check_true(!profiler.IsActive());
  do_check_true(!profiler.IsPaused());
  do_test_finished();
}
