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

  Assert.equal(typeof libs, "object");
  Assert.ok(Array.isArray(libs));
  Assert.equal(typeof libs, "object");
  Assert.ok(libs.length >= 1);
  Assert.equal(typeof libs[0], "object");
  Assert.equal(typeof libs[0].name, "string");
  Assert.equal(typeof libs[0].path, "string");
  Assert.equal(typeof libs[0].debugName, "string");
  Assert.equal(typeof libs[0].debugPath, "string");
  Assert.equal(typeof libs[0].arch, "string");
  Assert.equal(typeof libs[0].start, "number");
  Assert.equal(typeof libs[0].end, "number");
  Assert.ok(libs[0].start <= libs[0].end);
}
