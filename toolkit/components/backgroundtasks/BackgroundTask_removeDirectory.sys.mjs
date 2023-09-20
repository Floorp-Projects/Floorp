/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
});

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
import { EXIT_CODE } from "resource://gre/modules/BackgroundTasksManager.sys.mjs";

class Metrics {
  /**
   * @param {string} metricsId
   */
  constructor(metricsId) {
    this.metricsId = metricsId;
    this.startedTime = new Date();

    this.wasFirst = true;
    this.retryCount = 0;
    this.removalCountObj = { value: 0 };
    this.succeeded = true;

    this.suffixRemovalCountObj = { value: 0 };
    this.suffixEverFailed = false;
  }

  async report() {
    if (!this.metricsId) {
      console.warn(`Skipping Glean as no metrics id is passed`);
      return;
    }
    if (AppConstants.MOZ_APP_NAME !== "firefox") {
      console.warn(
        `Skipping Glean as the app is not Firefox: ${AppConstants.MOZ_APP_NAME}`
      );
      return;
    }

    const elapsedMs = new Date().valueOf() - this.startedTime.valueOf();

    // Note(krosylight): This FOG initialization happens within a unique
    // temporary directory created for each background task, which will
    // be removed after each run.
    // That means any failed submission will be lost, but we are fine with
    // that as we only have a single submission.
    Services.fog.initializeFOG(undefined, "firefox.desktop.background.tasks");

    const gleanMetrics = Glean[`backgroundTasksRmdir${this.metricsId}`];
    if (!gleanMetrics) {
      throw new Error(
        `The metrics id "${this.metricsId}" is not available in toolkit/components/backgroundtasks/metrics.yaml. ` +
          `Make sure that the id has no typo and is in PascalCase. ` +
          `Note that you can omit the id for testing.`
      );
    }

    gleanMetrics.elapsedMs.set(elapsedMs);
    gleanMetrics.wasFirst.set(this.wasFirst);
    gleanMetrics.retryCount.set(this.retryCount);
    gleanMetrics.removalCount.set(this.removalCountObj.value);
    gleanMetrics.succeeded.set(this.succeeded);
    gleanMetrics.suffixRemovalCount.set(this.suffixRemovalCountObj.value);
    gleanMetrics.suffixEverFailed.set(this.suffixEverFailed);

    GleanPings.backgroundTasks.submit();

    // XXX: We wait for arbitrary time for Glean to submit telemetry.
    // Bug 1790702 should add a better way.
    console.error("Pinged glean, waiting for submission.");
    await new Promise(resolve => lazy.setTimeout(resolve, 5000));
  }
}

// Recursively removes a directory.
// Returns true if it succeeds, false otherwise.
function tryRemoveDir(aFile, countObj) {
  try {
    aFile.remove(true, countObj);
  } catch (e) {
    return false;
  }

  return true;
}

const FILE_CHECK_ITERATION_TIMEOUT_MS = 1000;

function cleanupDirLockFile(aLock, aProfileName) {
  let lockFile = aLock.getLockFile(aProfileName);
  try {
    // Try to clean up the lock file
    lockFile.remove(false);
  } catch (ex) {}
}

async function deleteChildDirectory(
  parentDirPath,
  childDirName,
  secondsToWait,
  metrics
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

  let locked = false;
  try {
    dirLock.lock(childDirName);
    locked = true;
    metrics.wasFirst = !dirLock.isOtherInstanceRunning();
  } catch (e) {
    console.error("Failed to check dirLock");
  }

  if (!metrics.wasFirst) {
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
  while (!targetFile.exists()) {
    if (
      metrics.retryCount * FILE_CHECK_ITERATION_TIMEOUT_MS >
      secondsToWait * 1000
    ) {
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
    metrics.retryCount++;
    console.error(`Cache folder attempt no ${metrics.retryCount}`);
  }

  if (!targetFile.isDirectory()) {
    if (locked) {
      dirLock.unlock();
      locked = false;
    }
    throw new Error("Path was not a directory");
  }

  console.error(`started removing ${targetFile.path}`);
  try {
    targetFile.remove(true, metrics.removalCountObj);
  } catch (err) {
    console.error(
      `failed removing ${targetFile.path}. removed ${metrics.removalCountObj.value} entries.`
    );
    throw err;
  } finally {
    console.error(
      `done removing ${targetFile.path}. removed ${metrics.removalCountObj.value} entries.`
    );
    if (locked) {
      dirLock.unlock();
      locked = false;
      cleanupDirLockFile(dirLock, childDirName);
    }
  }
}

async function cleanupOtherDirectories(
  parentDirPath,
  otherFoldersSuffix,
  metrics
) {
  if (!otherFoldersSuffix || !otherFoldersSuffix.length) {
    return;
  }

  let targetFile = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  targetFile.initWithPath(parentDirPath);

  let entries = targetFile.directoryEntries;
  while (entries.hasMoreElements()) {
    let entry = entries.nextFile;

    if (
      otherFoldersSuffix !== "*" &&
      !entry.leafName.endsWith(otherFoldersSuffix)
    ) {
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
    let removedDir = tryRemoveDir(entry, metrics.suffixRemovalCountObj);
    if (!removedDir && entry.exists()) {
      // If first deletion of the directory failed, then we try again once more
      // just in case.
      metrics.suffixEverFailed = true;
      removedDir = tryRemoveDir(entry, metrics.suffixRemovalCountObj);
    }
    console.error(
      `Deletion of folder ${entry.leafName} - success=${removedDir}`
    );
    dirLock.unlock();
    cleanupDirLockFile(dirLock, entry.leafName);
  }
}

// Usage:
// removeDirectory parentDirPath childDirName secondsToWait [otherFoldersSuffix]
//                  arg0           arg1     arg2            arg3
//                 [--test-sleep testSleep]
//                 [--metrics-id metricsId]
// parentDirPath - The path to the parent directory that includes the target directory
// childDirName - The "leaf name" of the moved cache directory
//                If empty, the background task will only purge folders that have the "otherFoldersSuffix".
// secondsToWait - String representing the number of seconds to wait for the cacheDir to be moved
// otherFoldersSuffix - [optional] The suffix of directories that should be removed
//                      When not empty, this task will also attempt to remove all directories in
//                      the parent dir that end with this suffix
//                      As a special command, "*" will remove all subdirectories.
// testSleep - [optional] A test-only argument to sleep for a given milliseconds before removal.
//             This exists to test whether a long-running task can survive.
// metricsId - [optional] The identifier for Glean metrics, in PascalCase.
//             It'll be submitted only when the matching identifier exists in
//             toolkit/components/backgroundtasks/metrics.yaml.
export async function runBackgroundTask(commandLine) {
  const testSleep = Number.parseInt(
    commandLine.handleFlagWithParam("test-sleep", false)
  );
  const metricsId = commandLine.handleFlagWithParam("metrics-id", false) || "";

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

  console.error(
    parentDirPath,
    childDirName,
    secondsToWait,
    otherFoldersSuffix,
    metricsId
  );

  if (!Number.isNaN(testSleep)) {
    await new Promise(resolve => lazy.setTimeout(resolve, testSleep));
  }

  const metrics = new Metrics(metricsId);

  try {
    await deleteChildDirectory(
      parentDirPath,
      childDirName,
      secondsToWait,
      metrics
    );
    await cleanupOtherDirectories(parentDirPath, otherFoldersSuffix, metrics);
  } catch (err) {
    metrics.succeeded = false;
    throw err;
  } finally {
    await metrics.report();
  }

  return EXIT_CODE.SUCCESS;
}
