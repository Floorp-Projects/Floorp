/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { AsyncShutdown } = ChromeUtils.import(
  "resource://gre/modules/AsyncShutdown.jsm"
);

// Set to true if the application is quitting
var gQuitting = false;

// Tracks all the running instances of the minidump-analyzer
var gRunningProcesses = new Set();

/**
 * Run the minidump-analyzer with the given options unless we're already
 * shutting down or the main process has been instructed to shut down in the
 * case a content process crashes. Minidump analysis can take a while so we
 * don't want to block shutdown waiting for it.
 */
async function maybeRunMinidumpAnalyzer(minidumpPath, allThreads) {
  let env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );
  let shutdown = env.exists("MOZ_CRASHREPORTER_SHUTDOWN");

  if (gQuitting || shutdown) {
    return;
  }

  await runMinidumpAnalyzer(minidumpPath, allThreads).catch(e =>
    Cu.reportError(e)
  );
}

function getMinidumpAnalyzerPath() {
  const binSuffix = AppConstants.platform === "win" ? ".exe" : "";
  const exeName = "minidump-analyzer" + binSuffix;

  let exe = Services.dirsvc.get("GreBinD", Ci.nsIFile);
  exe.append(exeName);

  return exe;
}

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
      let exe = getMinidumpAnalyzerPath();
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
      reject(e);
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
      let minidumpData = await IOUtils.read(minidumpPath);
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
      let extraData = await IOUtils.read(extraPath);

      return JSON.parse(decoder.decode(extraData));
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
function CrashService() {
  Services.obs.addObserver(this, "quit-application");
}

CrashService.prototype = Object.freeze({
  classID: Components.ID("{92668367-1b17-4190-86b2-1061b2179744}"),
  QueryInterface: ChromeUtils.generateQI(["nsICrashService", "nsIObserver"]),

  async addCrash(processType, crashType, id) {
    if (processType === Ci.nsIXULRuntime.PROCESS_TYPE_IPDLUNITTEST) {
      return;
    }

    processType = Services.crashmanager.processTypes[processType];

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

    await maybeRunMinidumpAnalyzer(minidumpPath, allThreads);
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
