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

  var libs = profiler.sharedLibraries;

  do_check_eq(typeof libs, "object");
  do_check_true(Array.isArray(libs));
  do_check_eq(typeof libs, "object");
  do_check_true(libs.length >= 1);
  do_check_eq(typeof libs[0], "object");
  do_check_eq(typeof libs[0].name, "string");
  do_check_eq(typeof libs[0].path, "string");
  do_check_eq(typeof libs[0].debugName, "string");
  do_check_eq(typeof libs[0].debugPath, "string");
  do_check_eq(typeof libs[0].arch, "string");
  do_check_eq(typeof libs[0].start, "number");
  do_check_eq(typeof libs[0].end, "number");
  do_check_true(libs[0].start <= libs[0].end);
}
