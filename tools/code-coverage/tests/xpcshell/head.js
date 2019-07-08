/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
var { OS, require } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

function getFiles() {
  const env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );
  // This is the directory where gcov is emitting the gcda files.
  const jsCoveragePath = env.get("JS_CODE_COVERAGE_OUTPUT_DIR");

  const jsCoverageDir = Cc["@mozilla.org/file/local;1"].createInstance(
    Ci.nsIFile
  );
  jsCoverageDir.initWithPath(jsCoveragePath);

  let files = [];

  let entries = jsCoverageDir.directoryEntries;
  while (entries.hasMoreElements()) {
    files.push(entries.nextFile);
  }

  return files;
}

function diffFiles(files_after, files_before) {
  let files_before_set = new Set(files_before.map(file => file.leafName));
  return files_after.filter(file => !files_before_set.has(file.leafName));
}

function parseRecords(files) {
  let records = new Map();

  for (let file of files) {
    const lines = Cu.readUTF8File(file).split("\n");
    let currentSF = null;

    for (let line of lines) {
      let [recordType, ...recordContent] = line.split(":");
      recordContent = recordContent.join(":");

      switch (recordType) {
        case "FNDA": {
          if (currentSF == null) {
            throw new Error("SF missing");
          }

          let [hits, name] = recordContent.split(",");
          currentSF.push({
            type: "FNDA",
            hits,
            name,
          });
          break;
        }

        case "FN": {
          if (currentSF == null) {
            throw new Error("SF missing");
          }

          let name = recordContent.split(",")[1];
          currentSF.push({
            type: "FN",
            name,
          });
          break;
        }

        case "SF": {
          if (AppConstants.platform == "win") {
            recordContent = recordContent.replace(/\//g, "\\");
          }

          currentSF = [];
          records.set(OS.Path.basename(recordContent), currentSF);
          break;
        }
      }
    }
  }

  return records;
}
