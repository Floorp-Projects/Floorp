/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
});

import { EXIT_CODE } from "resource://gre/modules/BackgroundTasksManager.sys.mjs";

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

const FILE_CHECK_ITERATION_TIMEOUT_MS = 1000;

async function deleteChildDirectory(
  parentDirPath,
  childDirName,
  secondsToWait
) {
  if (!childDirName || !childDirName.length) {
    return;
  }

  let targetFile = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  targetFile.initWithPath(parentDirPath);
  targetFile.append(childDirName);

  // We create the lock before the file is actually there so this task
  // is the first one to acquire the lock. Otherwise a different task
  // could be cleaning suffixes and start deleting the folder while this
  // task is waiting for it to show up.
  let dirLock = Cc["@mozilla.org/net/CachePurgeLock;1"].createInstance(
    Ci.nsICachePurgeLock
  );

  let wasFirst = false;
  let locked = false;
  try {
    dirLock.lock(childDirName);
    locked = true;
    wasFirst = !dirLock.isOtherInstanceRunning();
  } catch (e) {
    console.error("Failed to check dirLock");
  }

  if (!wasFirst) {
    if (locked) {
      dirLock.unlock();
      locked = false;
    }
    console.error("Another instance is already purging this directory");
    return;
  }

  // This backgroundtask process is spawned by the call to
  // PR_CreateProcessDetached in CacheFileIOManager::SyncRemoveAllCacheFiles
  // Only if spawning the process is successful is the cache folder renamed,
  // so we need to wait until that is done.
  let retryCount = 0;
  while (!targetFile.exists()) {
    if (retryCount * FILE_CHECK_ITERATION_TIMEOUT_MS > secondsToWait * 1000) {
      // We don't know for sure if the folder was renamed or if a different
      // task removed it already. The second variant is more likely but to
      // be sure we'd have to consult a log file, which introduces
      // more complexity.
      console.error(`couldn't find cache folder ${targetFile.path}`);
      if (locked) {
        dirLock.unlock();
        locked = false;
      }
      return;
    }
    await new Promise(resolve =>
      lazy.setTimeout(resolve, FILE_CHECK_ITERATION_TIMEOUT_MS)
    );
    retryCount++;
    console.error(`Cache folder attempt no ${retryCount}`);
  }

  if (!targetFile.isDirectory()) {
    if (locked) {
      dirLock.unlock();
      locked = false;
    }
    throw new Error("Path was not a directory");
  }

  console.error(`started removing ${targetFile.path}`);
  targetFile.remove(true);
  console.error(`done removing ${targetFile.path}`);

  if (locked) {
    dirLock.unlock();
    locked = false;
  }
}

async function cleanupOtherDirectories(parentDirPath, otherFoldersSuffix) {
  if (!otherFoldersSuffix || !otherFoldersSuffix.length) {
    return;
  }

  let targetFile = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  targetFile.initWithPath(parentDirPath);

  let entries = targetFile.directoryEntries;
  while (entries.hasMoreElements()) {
    let entry = entries.nextFile;

    if (!entry.leafName.endsWith(otherFoldersSuffix)) {
      continue;
    }

    let shouldProcessEntry = false;
    // The folder could already be gone, so isDirectory could throw
    try {
      shouldProcessEntry = entry.isDirectory();
    } catch (e) {}

    if (!shouldProcessEntry) {
      continue;
    }

    let dirLock = Cc["@mozilla.org/net/CachePurgeLock;1"].createInstance(
      Ci.nsICachePurgeLock
    );
    let wasFirst = false;

    try {
      dirLock.lock(entry.leafName);
      wasFirst = !dirLock.isOtherInstanceRunning();
    } catch (e) {
      console.error("Failed to check dirlock. Skipping folder");
      dirLock.unlock();
      continue;
    }

    if (!wasFirst) {
      dirLock.unlock();
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
    dirLock.unlock();
  }
}

// Usage:
// removeDirectory parentDirPath childDirName secondsToWait [otherFoldersSuffix] [--test-sleep testSleep]
//                  arg0           arg1     arg2            arg3
// parentDirPath - The path to the parent directory that includes the target directory
// childDirName - The "leaf name" of the moved cache directory
//                If empty, the background task will only purge folders that have the "otherFoldersSuffix".
// secondsToWait - String representing the number of seconds to wait for the cacheDir to be moved
// otherFoldersSuffix - [optional] The suffix of directories that should be removed
//                      When not empty, this task will also attempt to remove all directories in
//                      the parent dir that end with this suffix
// testSleep - [optional] A test-only argument to sleep for a given milliseconds before removal.
//             This exists to test whether a long-running task can survive.
export async function runBackgroundTask(commandLine) {
  const testSleep = Number.parseInt(
    commandLine.handleFlagWithParam("test-sleep", false)
  );

  if (commandLine.length < 3) {
    throw new Error("Insufficient arguments");
  }

  const parentDirPath = commandLine.getArgument(0);
  const childDirName = commandLine.getArgument(1);
  let secondsToWait = parseInt(commandLine.getArgument(2));
  if (isNaN(secondsToWait)) {
    secondsToWait = 10;
  }
  commandLine.removeArguments(0, 2);

  let otherFoldersSuffix = "";
  if (commandLine.length) {
    otherFoldersSuffix = commandLine.getArgument(0);
    commandLine.removeArguments(0, 0);
  }

  if (commandLine.length) {
    throw new Error(
      `${commandLine.length} unknown command args exist, closing.`
    );
  }

  console.error(parentDirPath, childDirName, secondsToWait, otherFoldersSuffix);

  if (!Number.isNaN(testSleep)) {
    await new Promise(resolve => lazy.setTimeout(resolve, testSleep));
  }

  await deleteChildDirectory(parentDirPath, childDirName, secondsToWait);
  await cleanupOtherDirectories(parentDirPath, otherFoldersSuffix);

  // TODO: event telemetry with timings, and how often we have left over cache folders from previous runs.

  return EXIT_CODE.SUCCESS;
}
