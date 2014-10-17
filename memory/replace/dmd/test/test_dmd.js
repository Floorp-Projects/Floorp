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

// If we're testing locally, the executable file is in "CurProcD". Otherwise,
// it is in another location that we have to find.
function getExecutable(aFilename) {
  let file = FileUtils.getFile("CurProcD", [aFilename]);
  if (!file.exists()) {
    file = FileUtils.getFile("CurWorkD", []);
    while (file.path.contains("xpcshell")) {
      file = file.parent;
    }
    file.append("bin");
    file.append(aFilename);
  }
  return file;
}

let gIsWindows = Cc["@mozilla.org/xre/app-info;1"]
                 .getService(Ci.nsIXULRuntime).OS === "WINNT";
let gDmdTestFile = getExecutable("SmokeDMD" + (gIsWindows ? ".exe" : ""));

let gDmdScriptFile = getExecutable("dmd.py");

function readFile(aFile) {
  var fstream = Cc["@mozilla.org/network/file-input-stream;1"]
                  .createInstance(Ci.nsIFileInputStream);
  var cstream = Cc["@mozilla.org/intl/converter-input-stream;1"]
                  .createInstance(Ci.nsIConverterInputStream);
  fstream.init(aFile, -1, 0, 0);
  cstream.init(fstream, "UTF-8", 0, 0);

  var data = "";
  let (str = {}) {
    let read = 0;
    do {
      // Read as much as we can and put it in str.value.
      read = cstream.readString(0xffffffff, str);
      data += str.value;
    } while (read != 0);
  }
  cstream.close();                // this closes fstream
  return data.replace(/\r/g, ""); // normalize line endings
}

function runProcess(aExeFile, aArgs) {
  let process = Cc["@mozilla.org/process/util;1"]
                  .createInstance(Components.interfaces.nsIProcess);
  process.init(aExeFile);
  process.run(/* blocking = */true, aArgs, aArgs.length);
  return process.exitValue;
}

function test(aPrefix, aArgs) {
  // DMD writes the JSON files to CurWorkD, so we do likewise here with
  // |actualFile| for consistency. It is removed once we've finished.
  let expectedFile = FileUtils.getFile("CurWorkD", [aPrefix + "-expected.txt"]);
  let actualFile   = FileUtils.getFile("CurWorkD", [aPrefix + "-actual.txt"]);

  // Run dmd.py on the JSON file, producing |actualFile|.

  let args = [
    gDmdScriptFile.path,
    "--filter-stacks-for-testing",
    "-o", actualFile.path
  ].concat(aArgs);

  runProcess(new FileUtils.File(gPythonName), args);

  // Compare |expectedFile| with |actualFile|. We produce nice diffs with
  // /usr/bin/diff on systems that have it (Mac and Linux). Otherwise (Windows)
  // we do a string compare of the file contents and then print them both if
  // they don't match.

  let success;
  try {
    let rv = runProcess(new FileUtils.File("/usr/bin/diff"),
                        ["-u", expectedFile.path, actualFile.path]);
    success = rv == 0;

  } catch (e) {
    let expectedData = readFile(expectedFile);
    let actualData   = readFile(actualFile);
    success = expectedData === actualData;
    if (!success) {
      expectedData = expectedData.split("\n");
      actualData = actualData.split("\n");
      for (let i = 0; i < expectedData.length; i++) {
        print("EXPECTED:" + expectedData[i]);
      }
      for (let i = 0; i < actualData.length; i++) {
        print("  ACTUAL:" + actualData[i]);
      }
    }
  }

  ok(success, aPrefix);

  actualFile.remove(true);
}

function run_test() {
  let jsonFile, jsonFile2;

  // These tests do full end-to-end testing of DMD, i.e. both the C++ code that
  // generates the JSON output, and the script that post-processes that output.
  //
  // Run these synchronously, because test() updates the full*.json files
  // in-place (to fix stacks) when it runs dmd.py, and that's not safe to do
  // asynchronously.

  gEnv.set("DMD", "1");
  gEnv.set(gEnv.get("DMD_PRELOAD_VAR"), gEnv.get("DMD_PRELOAD_VALUE"));

  runProcess(gDmdTestFile, []);

  let fullTestNames = ["empty", "unsampled1", "unsampled2", "sampled"];
  for (let i = 0; i < fullTestNames.length; i++) {
      let name = fullTestNames[i];
      jsonFile = FileUtils.getFile("CurWorkD", ["full-" + name + ".json"]);
      test("full-heap-" + name, ["--ignore-reports", jsonFile.path])
      test("full-reports-" + name, [jsonFile.path])
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
  test("script-max-frames-8",
       ["--ignore-reports", "--max-frames=8", jsonFile.path]);
  test("script-max-frames-3",
       ["--ignore-reports", "--max-frames=3", "--no-fix-stacks",
        jsonFile.path]);
  test("script-max-frames-1",
       ["--ignore-reports", "--max-frames=1", jsonFile.path]);

  // This file has three records that are shown in a different order for each
  // of the different sort values. It also tests the handling of gzipped JSON
  // files.
  jsonFile = FileUtils.getFile("CurWorkD", ["script-sort-by.json.gz"]);
  test("script-sort-by-usable",
       ["--ignore-reports", "--sort-by=usable", jsonFile.path]);
  test("script-sort-by-req",
       ["--ignore-reports", "--sort-by=req", "--no-fix-stacks", jsonFile.path]);
  test("script-sort-by-slop",
       ["--ignore-reports", "--sort-by=slop", jsonFile.path]);

  // This file has several real stack traces taken from Firefox execution, each
  // of which tests a different allocator function (or functions).
  jsonFile = FileUtils.getFile("CurWorkD", ["script-ignore-alloc-fns.json"]);
  test("script-ignore-alloc-fns",
       ["--ignore-reports", "--ignore-alloc-fns", jsonFile.path]);

  // This file has numerous allocations of different sizes, some repeated, some
  // sampled, that all end up in the same record.
  jsonFile = FileUtils.getFile("CurWorkD", ["script-show-all-block-sizes.json"]);
  test("script-show-all-block-sizes",
       ["--ignore-reports", "--show-all-block-sizes", jsonFile.path]);

  // This tests diffs. The first invocation has no options, the second has
  // several.
  jsonFile  = FileUtils.getFile("CurWorkD", ["script-diff1.json"]);
  jsonFile2 = FileUtils.getFile("CurWorkD", ["script-diff2.json"]);
  test("script-diff-basic",
       [jsonFile.path, jsonFile2.path]);
  test("script-diff-options",
       ["--ignore-reports", "--show-all-block-sizes",
        jsonFile.path, jsonFile2.path]);
}

