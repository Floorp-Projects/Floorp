/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-*/
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

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

function test(aJsonFile, aKind, aOptions, aN) {
  // DMD writes the JSON files to CurWorkD, so we do likewise here with
  // |actualFile| for consistency. It is removed once we've finished.
  let expectedFile =
    FileUtils.getFile("CurWorkD",
                      ["full-" + aKind + "-expected" + aN + ".txt"]);
  let actualFile =
    FileUtils.getFile("CurWorkD",
                      ["full-" + aKind + "-actual"   + aN + ".txt"]);

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
  ok(success, aKind + " " + aN);

  actualFile.remove(true);
}

function run_test() {
  // These tests do full end-to-end testing of DMD, i.e. both the C++ code that
  // generates the JSON output, and the script that post-processes that output.
  // The test relies on DMD's test mode executing beforehand, in order to
  // produce the relevant JSON files.
  //
  // Run these synchronously, because test() updates the full*.json files
  // in-place (to fix stacks) when it runs dmd.py, and that's not safe to do
  // asynchronously.
  for (let i = 1; i <= 4; i++) {
      let jsonFile = FileUtils.getFile("CurWorkD", ["full" + i + ".json"]);
      test(jsonFile, "heap", ["--ignore-reports"], i);
      test(jsonFile, "reports", [], i);
      jsonFile.remove(true);
  }
}
