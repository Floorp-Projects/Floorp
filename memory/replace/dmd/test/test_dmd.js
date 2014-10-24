/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-*/
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components

Cu.import("resource://gre/modules/FileUtils.jsm");

// The xpcshell test harness sets PYTHON so we can read it here.
let gEnv = Cc["@mozilla.org/process/environment;1"]
             .getService(Ci.nsIEnvironment);
let gPythonName = gEnv.get("PYTHON");

// If we're testing locally, the script is in "CurProcD". Otherwise, it is in
// another location that we have to find.
let gDmdScriptFile = FileUtils.getFile("CurProcD", ["dmd.py"]);
if (!gDmdScriptFile.exists()) {
  gDmdScriptFile = FileUtils.getFile("CurWorkD", []);
  while (gDmdScriptFile.path.contains("xpcshell")) {
    gDmdScriptFile = gDmdScriptFile.parent;
  }
  gDmdScriptFile.append("bin");
  gDmdScriptFile.append("dmd.py");
}

function test(aJsonFile, aPrefix, aOptions) {
  // DMD writes the JSON files to CurWorkD, so we do likewise here with
  // |actualFile| for consistency. It is removed once we've finished.
  let expectedFile = FileUtils.getFile("CurWorkD", [aPrefix + "-expected.txt"]);
  let actualFile   = FileUtils.getFile("CurWorkD", [aPrefix + "-actual.txt"]);

  // Run dmd.py on the JSON file, producing |actualFile|.

  let pythonFile = new FileUtils.File(gPythonName);
  let pythonProcess = Cc["@mozilla.org/process/util;1"]
                        .createInstance(Components.interfaces.nsIProcess);
  pythonProcess.init(pythonFile);

  let args = [
    gDmdScriptFile.path,
    "--filter-stacks-for-testing",
    "-o", actualFile.path
  ];
  args = args.concat(aOptions);
  args.push(aJsonFile.path);

  pythonProcess.run(/* blocking = */true, args, args.length);

  // Compare |expectedFile| with |actualFile|. Difference are printed to
  // stdout.

  let diffFile = new FileUtils.File("/usr/bin/diff");
  let diffProcess = Cc["@mozilla.org/process/util;1"]
                      .createInstance(Components.interfaces.nsIProcess);
  // XXX: this doesn't work on Windows (bug 1076446).
  diffProcess.init(diffFile);

  args = ["-u", expectedFile.path, actualFile.path];
  diffProcess.run(/* blocking = */true, args, args.length);
  let success = diffProcess.exitValue == 0;
  ok(success, aPrefix);

  actualFile.remove(true);
}

function run_test() {
  let jsonFile;

  // These tests do full end-to-end testing of DMD, i.e. both the C++ code that
  // generates the JSON output, and the script that post-processes that output.
  // The test relies on DMD's test mode executing beforehand, in order to
  // produce the relevant JSON files.
  //
  // Run these synchronously, because test() updates the full*.json files
  // in-place (to fix stacks) when it runs dmd.py, and that's not safe to do
  // asynchronously.
  let fullTestNames = ["empty", "unsampled1", "unsampled2", "sampled"];
  for (let i = 0; i < fullTestNames.length; i++) {
      let name = fullTestNames[i];
      jsonFile = FileUtils.getFile("CurWorkD", ["full-" + name + ".json"]);
      test(jsonFile, "full-heap-" + name, ["--ignore-reports"])
      test(jsonFile, "full-reports-" + name, [])
      jsonFile.remove(true);
  }

  // These tests only test the post-processing script. They use hand-written
  // JSON files as input. Ideally the JSON files would contain comments
  // explaining how they work, but JSON doesn't allow comments, so I've put
  // explanations here.

  // This just tests that stack traces of various lengths are truncated
  // appropriately. The number of records in the output is different for each
  // of the tested values.
  jsonFile = FileUtils.getFile("CurWorkD", ["script-max-frames.json"]);
  test(jsonFile, "script-max-frames-8", ["-r", "--max-frames=8"]);
  test(jsonFile, "script-max-frames-3", ["-r", "--max-frames=3",
                                         "--no-fix-stacks"]);
  test(jsonFile, "script-max-frames-1", ["-r", "--max-frames=1"]);

  // This test has three records that are shown in a different order for each
  // of the different sort values. It also tests the handling of gzipped JSON
  // files.
  jsonFile = FileUtils.getFile("CurWorkD", ["script-sort-by.json.gz"]);
  test(jsonFile, "script-sort-by-usable", ["-r", "--sort-by=usable"]);
  test(jsonFile, "script-sort-by-req",    ["-r", "--sort-by=req",
                                           "--no-fix-stacks"]);
  test(jsonFile, "script-sort-by-slop",   ["-r", "--sort-by=slop"]);

  // This test has several real stack traces taken from Firefox execution, each
  // of which tests a different allocator function (or functions).
  jsonFile = FileUtils.getFile("CurWorkD", ["script-ignore-alloc-fns.json"]);
  test(jsonFile, "script-ignore-alloc-fns", ["-r", "--ignore-alloc-fns"]);

  // This test has numerous allocations of different sizes, some repeated, some
  // sampled, that all end up in the same record.
  jsonFile = FileUtils.getFile("CurWorkD", ["script-show-all-block-sizes.json"]);
  test(jsonFile, "script-show-all-block-sizes", ["-r", "--show-all-block-sizes"]);
}
