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

  var sharedStr = profiler.getSharedLibraryInformation();
  sharedStr = sharedStr.toLowerCase();

  // Let's not hardcode anything too specific
  // just some sanity checks.
  do_check_neq(sharedStr, null);
  do_check_neq(sharedStr, "");
}
