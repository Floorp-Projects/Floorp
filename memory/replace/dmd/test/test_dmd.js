/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-*/
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { FileUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/FileUtils.sys.mjs"
);

// The xpcshell test harness sets PYTHON so we can read it here.
var gPythonName = Services.env.get("PYTHON");

const gCwd = Services.dirsvc.get("CurWorkD", Ci.nsIFile);

function getRelativeFile(...components) {
  return new FileUtils.File(PathUtils.join(gCwd.path, ...components));
}

// If we're testing locally, the executable file is in "CurProcD". Otherwise,
// it is in another location that we have to find.
function getExecutable(aFilename) {
  let file = new FileUtils.File(
    PathUtils.join(Services.dirsvc.get("CurProcD", Ci.nsIFile).path, aFilename)
  );
  if (!file.exists()) {
    file = gCwd.clone();
    while (file.path.includes("xpcshell")) {
      file = file.parent;
    }
    file.append("bin");
    file.append(aFilename);
  }
  return file;
}

var gIsWindows = Services.appinfo.OS === "WINNT";
var gDmdTestFile = getExecutable("SmokeDMD" + (gIsWindows ? ".exe" : ""));

var gDmdScriptFile = getExecutable("dmd.py");

var gScanTestFile = getRelativeFile("scan-test.py");

function readFile(aFile) {
  let fstream = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(
    Ci.nsIFileInputStream
  );
  let cstream = Cc["@mozilla.org/intl/converter-input-stream;1"].createInstance(
    Ci.nsIConverterInputStream
  );
  fstream.init(aFile, -1, 0, 0);
  cstream.init(fstream, "UTF-8", 0, 0);

  let data = "";
  let str = {};
  let read = 0;
  do {
    // Read as much as we can and put it in str.value.
    read = cstream.readString(0xffffffff, str);
    data += str.value;
  } while (read != 0);

  cstream.close(); // this closes fstream
  return data.replace(/\r/g, ""); // normalize line endings
}

function runProcess(aExeFile, aArgs) {
  let process = Cc["@mozilla.org/process/util;1"].createInstance(Ci.nsIProcess);
  process.init(aExeFile);
  process.run(/* blocking = */ true, aArgs, aArgs.length);
  return process.exitValue;
}

function test(aPrefix, aArgs) {
  // DMD writes the JSON files to CurWorkD, so we do likewise here with
  // |actualFile| for consistency. It is removed once we've finished.
  let expectedFile = getRelativeFile(`${aPrefix}-expected.txt`);
  let actualFile = getRelativeFile(`${aPrefix}-actual.txt`);

  // Run dmd.py on the JSON file, producing |actualFile|.

  let args = [
    gDmdScriptFile.path,
    "--filter-stacks-for-testing",
    "-o",
    actualFile.path,
  ].concat(aArgs);

  runProcess(new FileUtils.File(gPythonName), args);

  // Compare |expectedFile| with |actualFile|. We produce nice diffs with
  // /usr/bin/diff on systems that have it (Mac and Linux). Otherwise (Windows)
  // we do a string compare of the file contents and then print them both if
  // they don't match.

  let success;
  try {
    let rv = runProcess(new FileUtils.File("/usr/bin/diff"), [
      "-u",
      expectedFile.path,
      actualFile.path,
    ]);
    success = rv == 0;
  } catch (e) {
    let expectedData = readFile(expectedFile);
    let actualData = readFile(actualFile);
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

// Run scan-test.py on the JSON file and see if it succeeds.
function scanTest(aJsonFilePath, aExtraArgs) {
  let args = [gScanTestFile.path, aJsonFilePath].concat(aExtraArgs);

  return runProcess(new FileUtils.File(gPythonName), args) == 0;
}

function run_test() {
  let jsonFile, jsonFile2;

  // These tests do complete end-to-end testing of DMD, i.e. both the C++ code
  // that generates the JSON output, and the script that post-processes that
  // output.
  //
  // Run these synchronously, because test() updates the complete*.json files
  // in-place (to fix stacks) when it runs dmd.py, and that's not safe to do
  // asynchronously.

  Services.env.set("DMD", "1");

  runProcess(gDmdTestFile, []);

  function test2(aTestName, aMode) {
    let name = "complete-" + aTestName + "-" + aMode;
    jsonFile = getRelativeFile(`${name}.json`);
    test(name, [jsonFile.path]);
    jsonFile.remove(true);
  }

  // Please keep this in sync with RunTests() in SmokeDMD.cpp.

  test2("empty", "live");
  test2("empty", "dark-matter");
  test2("empty", "cumulative");

  test2("full1", "live");
  test2("full1", "dark-matter");

  test2("full2", "dark-matter");
  test2("full2", "cumulative");

  test2("partial", "live");

  // Heap scan testing.
  jsonFile = getRelativeFile("basic-scan.json");
  ok(scanTest(jsonFile.path), "Basic scan test");

  let is64Bit = Services.appinfo.is64Bit;
  let basicScanFileName = "basic-scan-" + (is64Bit ? "64" : "32");
  test(basicScanFileName, ["--clamp-contents", jsonFile.path]);
  ok(
    scanTest(jsonFile.path, ["--clamp-contents"]),
    "Scan with address clamping"
  );

  // Run the generic test a second time to ensure that the first time produced
  // valid JSON output. "--clamp-contents" is passed in so we don't have to have
  // more variants of the files.
  test(basicScanFileName, ["--clamp-contents", jsonFile.path]);
  jsonFile.remove(true);

  // These tests only test the post-processing script. They use hand-written
  // JSON files as input. Ideally the JSON files would contain comments
  // explaining how they work, but JSON doesn't allow comments, so I've put
  // explanations here.

  // This just tests that stack traces of various lengths are truncated
  // appropriately. The number of records in the output is different for each
  // of the tested values.
  jsonFile = getRelativeFile("script-max-frames.json");
  test("script-max-frames-8", [jsonFile.path]); // --max-frames=8 is the default
  test("script-max-frames-3", [
    "--max-frames=3",
    "--no-fix-stacks",
    jsonFile.path,
  ]);
  test("script-max-frames-1", ["--max-frames=1", jsonFile.path]);

  // This file has three records that are shown in a different order for each
  // of the different sort values. It also tests the handling of gzipped JSON
  // files.
  jsonFile = getRelativeFile("script-sort-by.json.gz");
  test("script-sort-by-usable", ["--sort-by=usable", jsonFile.path]);
  test("script-sort-by-req", [
    "--sort-by=req",
    "--no-fix-stacks",
    jsonFile.path,
  ]);
  test("script-sort-by-slop", ["--sort-by=slop", jsonFile.path]);
  test("script-sort-by-num-blocks", ["--sort-by=num-blocks", jsonFile.path]);

  // This file has several real stack traces taken from Firefox execution, each
  // of which tests a different allocator function (or functions).
  jsonFile = getRelativeFile("script-ignore-alloc-fns.json");
  test("script-ignore-alloc-fns", ["--ignore-alloc-fns", jsonFile.path]);

  // This tests "live"-mode diffs.
  jsonFile = getRelativeFile("script-diff-live1.json");
  jsonFile2 = getRelativeFile("script-diff-live2.json");
  test("script-diff-live", [jsonFile.path, jsonFile2.path]);

  // This tests "dark-matter"-mode diffs.
  jsonFile = getRelativeFile("script-diff-dark-matter1.json");
  jsonFile2 = getRelativeFile("script-diff-dark-matter2.json");
  test("script-diff-dark-matter", [jsonFile.path, jsonFile2.path]);
}
