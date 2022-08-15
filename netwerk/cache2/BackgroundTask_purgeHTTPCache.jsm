/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["runBackgroundTask"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const lazy = {};
XPCOMUtils.defineLazyModuleGetters(lazy, {
  setTimeout: "resource://gre/modules/Timer.jsm",
});

const { EXIT_CODE } = ChromeUtils.import(
  "resource://gre/modules/BackgroundTasksManager.jsm"
).BackgroundTasksManager;

// Recursively removes a directory.
// Returns true if it succeeds, false otherwise.
function tryRemoveDir(aFile) {
  try {
    aFile.remove(true);
  } catch (e) {
    return false;
  }

  return true;
}

// Usage:
// purgeHTTPCache profileDirPath cacheDirName secondsToWait [otherFoldersSuffix]
//                  arg0           arg1     arg2            arg3
// profileDirPath - The path to the "profile directory" containing the moved cache dir
// cacheDirName - The "leaf name" of the moved cache directory
//                If empty, the background task will only purge folders that have the "otherFoldersSuffix".
// secondsToWait - String representing the number of seconds to wait for the cacheDir to be moved
// otherFoldersSuffix - [optional] The suffix of moved purged cache directories
//                      When not empty, this task will also attempt to remove all directories in
//                      the profile dir that end with this suffix
async function runBackgroundTask(commandLine) {
  if (commandLine.length < 3) {
    throw new Error("Insufficient arguments");
  }

  let profileDirPath = commandLine.getArgument(0);
  let cacheDirName = commandLine.getArgument(1);
  let secondsToWait = parseInt(commandLine.getArgument(2));
  if (isNaN(secondsToWait)) {
    secondsToWait = 10;
  }
  let otherFoldersSuffix = "";
  if (commandLine.length >= 4) {
    otherFoldersSuffix = commandLine.getArgument(3);
  }

  console.error(
    profileDirPath,
    cacheDirName,
    secondsToWait,
    otherFoldersSuffix
  );

  if (cacheDirName && cacheDirName.length > 0) {
    let targetFile = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    targetFile.initWithPath(profileDirPath);
    // assert is directory?
    targetFile.append(cacheDirName);

    // This backgroundtask process is spawned by the call to
    // PR_CreateProcessDetached in CacheFileIOManager::SyncRemoveAllCacheFiles
    // Only if spawning the process is successful is the cache folder renamed,
    // so we need to wait until that is done.
    let retryCount = 0;
    while (!targetFile.exists()) {
      if (retryCount > secondsToWait) {
        throw new Error(`couldn't find cache folder ${targetFile.path}`);
      }
      await new Promise(resolve => lazy.setTimeout(resolve, 1000));
      retryCount++;
      console.error(`Cache folder attempt no ${retryCount}`);
    }

    if (!targetFile.isDirectory()) {
      throw new Error("Path was not a directory");
    }

    console.error("have path");
    // Actually delete the folder
    targetFile.remove(true);
    console.error(`done removing ${targetFile.path}`);
  }

  if (otherFoldersSuffix.length) {
    let targetFile = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    targetFile.initWithPath(profileDirPath);

    let entries = targetFile.directoryEntries;
    while (entries.hasMoreElements()) {
      let entry = entries.nextFile;
      if (
        !entry.leafName.endsWith(otherFoldersSuffix) ||
        !entry.isDirectory()
      ) {
        continue;
      }

      // Remove directory recursively.
      let removedDir = tryRemoveDir(entry);
      if (!removedDir && entry.exists()) {
        // If first deletion of the directory failed, then we try again once more
        // just in case.
        removedDir = tryRemoveDir(entry);
      }
      console.error(
        `Deletion of folder ${entry.leafName} - success=${removedDir}`
      );
    }
  }

  // TODO: event telemetry with timings, and how often we have left over cache folders from previous runs.

  return EXIT_CODE.SUCCESS;
}
