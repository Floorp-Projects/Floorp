/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/AppConstants.jsm", this);
ChromeUtils.import("resource://gre/modules/AsyncShutdown.jsm", this);
const { parseKeyValuePairs } = ChromeUtils.import(
  "resource://gre/modules/KeyValueParser.jsm"
);
ChromeUtils.import("resource://gre/modules/osfile.jsm", this);
ChromeUtils.import("resource://gre/modules/Services.jsm", this);

// Set to true if the application is quitting
var gQuitting = false;

// Tracks all the running instances of the minidump-analyzer
var gRunningProcesses = new Set();

/**
 * Run the minidump analyzer tool to gather stack traces from the minidump. The
 * stack traces will be stored in the .extra file under the StackTraces= entry.
 *
 * @param minidumpPath {string} The path to the minidump file
 * @param allThreads {bool} Gather stack traces for all threads, not just the
 *                   crashing thread.
 *
 * @returns {Promise} A promise that gets resolved once minidump analysis has
 *          finished.
 */
function runMinidumpAnalyzer(minidumpPath, allThreads) {
  return new Promise((resolve, reject) => {
    try {
      const binSuffix = AppConstants.platform === "win" ? ".exe" : "";
      const exeName = "minidump-analyzer" + binSuffix;

      let exe = Services.dirsvc.get("GreBinD", Ci.nsIFile);

      if (AppConstants.platform === "macosx") {
        exe.append("crashreporter.app");
        exe.append("Contents");
        exe.append("MacOS");
      }

      exe.append(exeName);

      let args = [minidumpPath];
      let process = Cc["@mozilla.org/process/util;1"].createInstance(
        Ci.nsIProcess
      );
      process.init(exe);
      process.startHidden = true;
      process.noShell = true;

      if (allThreads) {
        args.unshift("--full");
      }

      process.runAsync(args, args.length, (subject, topic, data) => {
        switch (topic) {
          case "process-finished":
            gRunningProcesses.delete(process);
            resolve();
            break;
          case "process-failed":
            gRunningProcesses.delete(process);
            resolve();
            break;
          default:
            reject(new Error("Unexpected topic received " + topic));
            break;
        }
      });

      gRunningProcesses.add(process);
    } catch (e) {
      Cu.reportError(e);
    }
  });
}

/**
 * Computes the SHA256 hash of a minidump file
 *
 * @param minidumpPath {string} The path to the minidump file
 *
 * @returns {Promise} A promise that resolves to the hash value of the
 *          minidump.
 */
function computeMinidumpHash(minidumpPath) {
  return (async function() {
    try {
      let minidumpData = await OS.File.read(minidumpPath);
      let hasher = Cc["@mozilla.org/security/hash;1"].createInstance(
        Ci.nsICryptoHash
      );
      hasher.init(hasher.SHA256);
      hasher.update(minidumpData, minidumpData.length);

      let hashBin = hasher.finish(false);
      let hash = "";

      for (let i = 0; i < hashBin.length; i++) {
        // Every character in the hash string contains a byte of the hash data
        hash += ("0" + hashBin.charCodeAt(i).toString(16)).slice(-2);
      }

      return hash;
    } catch (e) {
      Cu.reportError(e);
      return null;
    }
  })();
}

/**
 * Process the given .extra file and return the annotations it contains in an
 * object.
 *
 * @param extraPath {string} The path to the .extra file
 *
 * @return {Promise} A promise that resolves to an object holding the crash
 *         annotations.
 */
function processExtraFile(extraPath) {
  return (async function() {
    try {
      let decoder = new TextDecoder();
      let extraData = await OS.File.read(extraPath);
      let keyValuePairs = parseKeyValuePairs(decoder.decode(extraData));

      // When reading from an .extra file literal '\\n' sequences are
      // automatically unescaped to two backslashes plus a newline, so we need
      // to re-escape them into '\\n' again so that the fields holding JSON
      // strings are valid.
      ["TelemetryEnvironment", "StackTraces"].forEach(field => {
        if (field in keyValuePairs) {
          keyValuePairs[field] = keyValuePairs[field].replace(/\n/g, "n");
        }
      });

      return keyValuePairs;
    } catch (e) {
      Cu.reportError(e);
      return {};
    }
  })();
}

/**
 * This component makes crash data available throughout the application.
 *
 * It is a service because some background activity will eventually occur.
 */
this.CrashService = function() {
  Services.obs.addObserver(this, "quit-application");
};

CrashService.prototype = Object.freeze({
  classID: Components.ID("{92668367-1b17-4190-86b2-1061b2179744}"),
  QueryInterface: ChromeUtils.generateQI([Ci.nsICrashService, Ci.nsIObserver]),

  async addCrash(processType, crashType, id) {
    switch (processType) {
      case Ci.nsICrashService.PROCESS_TYPE_MAIN:
        processType = Services.crashmanager.PROCESS_TYPE_MAIN;
        break;
      case Ci.nsICrashService.PROCESS_TYPE_CONTENT:
        processType = Services.crashmanager.PROCESS_TYPE_CONTENT;
        break;
      case Ci.nsICrashService.PROCESS_TYPE_PLUGIN:
        processType = Services.crashmanager.PROCESS_TYPE_PLUGIN;
        break;
      case Ci.nsICrashService.PROCESS_TYPE_GMPLUGIN:
        processType = Services.crashmanager.PROCESS_TYPE_GMPLUGIN;
        break;
      case Ci.nsICrashService.PROCESS_TYPE_GPU:
        processType = Services.crashmanager.PROCESS_TYPE_GPU;
        break;
      case Ci.nsICrashService.PROCESS_TYPE_VR:
        processType = Services.crashmanager.PROCESS_TYPE_VR;
        break;
      case Ci.nsICrashService.PROCESS_TYPE_RDD:
        processType = Services.crashmanager.PROCESS_TYPE_RDD;
        break;
      case Ci.nsICrashService.PROCESS_TYPE_SOCKET:
        processType = Services.crashmanager.PROCESS_TYPE_SOCKET;
        break;
      case Ci.nsICrashService.PROCESS_TYPE_IPDLUNITTEST:
        // We'll never send crash reports for this type of process.
        return;
      default:
        throw new Error("Unrecognized PROCESS_TYPE: " + processType);
    }

    let allThreads = false;

    switch (crashType) {
      case Ci.nsICrashService.CRASH_TYPE_CRASH:
        crashType = Services.crashmanager.CRASH_TYPE_CRASH;
        break;
      case Ci.nsICrashService.CRASH_TYPE_HANG:
        crashType = Services.crashmanager.CRASH_TYPE_HANG;
        allThreads = true;
        break;
      default:
        throw new Error("Unrecognized CRASH_TYPE: " + crashType);
    }

    let cr = Cc["@mozilla.org/toolkit/crash-reporter;1"].getService(
      Ci.nsICrashReporter
    );
    let minidumpPath = cr.getMinidumpForID(id).path;
    let extraPath = cr.getExtraFileForID(id).path;
    let metadata = {};
    let hash = null;

    if (!gQuitting) {
      // Minidump analysis can take a long time, don't start it if the browser
      // is already quitting.
      await runMinidumpAnalyzer(minidumpPath, allThreads);
    }

    metadata = await processExtraFile(extraPath);
    hash = await computeMinidumpHash(minidumpPath);

    if (hash) {
      metadata.MinidumpSha256Hash = hash;
    }

    let blocker = Services.crashmanager.addCrash(
      processType,
      crashType,
      id,
      new Date(),
      metadata
    );

    AsyncShutdown.profileBeforeChange.addBlocker(
      "CrashService waiting for content crash ping to be sent",
      blocker
    );

    blocker.then(AsyncShutdown.profileBeforeChange.removeBlocker(blocker));

    await blocker;
  },

  observe(subject, topic, data) {
    switch (topic) {
      case "profile-after-change":
        // Side-effect is the singleton is instantiated.
        Services.crashmanager;
        break;
      case "quit-application":
        gQuitting = true;
        gRunningProcesses.forEach(process => {
          try {
            process.kill();
          } catch (e) {
            // If the process has already quit then kill() fails, but since
            // this failure is benign it is safe to silently ignore it.
          }
          Services.obs.notifyObservers(null, "test-minidump-analyzer-killed");
        });
        break;
    }
  },
});

var EXPORTED_SYMBOLS = ["CrashService"];
