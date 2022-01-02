/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the DownloadIntegration object.
 */

"use strict";

// Globals

/**
 * Notifies the prompt observers and verify the expected downloads count.
 *
 * @param aIsPrivate
 *        Flag to know is test private observers.
 * @param aExpectedCount
 *        the expected downloads count for quit and offline observers.
 * @param aExpectedPBCount
 *        the expected downloads count for private browsing observer.
 */
function notifyPromptObservers(aIsPrivate, aExpectedCount, aExpectedPBCount) {
  let cancelQuit = Cc["@mozilla.org/supports-PRBool;1"].createInstance(
    Ci.nsISupportsPRBool
  );

  // Notify quit application requested observer.
  DownloadIntegration._testPromptDownloads = -1;
  Services.obs.notifyObservers(cancelQuit, "quit-application-requested");
  Assert.equal(DownloadIntegration._testPromptDownloads, aExpectedCount);

  // Notify offline requested observer.
  DownloadIntegration._testPromptDownloads = -1;
  Services.obs.notifyObservers(cancelQuit, "offline-requested");
  Assert.equal(DownloadIntegration._testPromptDownloads, aExpectedCount);

  if (aIsPrivate) {
    // Notify last private browsing requested observer.
    DownloadIntegration._testPromptDownloads = -1;
    Services.obs.notifyObservers(cancelQuit, "last-pb-context-exiting");
    Assert.equal(DownloadIntegration._testPromptDownloads, aExpectedPBCount);
  }

  delete DownloadIntegration._testPromptDownloads;
}

// Tests

/**
 * Allows re-enabling the real download directory logic during one test.
 */
function allowDirectoriesInTest() {
  DownloadIntegration.allowDirectories = true;
  function cleanup() {
    DownloadIntegration.allowDirectories = false;
  }
  registerCleanupFunction(cleanup);
  return cleanup;
}

XPCOMUtils.defineLazyGetter(this, "gStringBundle", function() {
  return Services.strings.createBundle(
    "chrome://mozapps/locale/downloads/downloads.properties"
  );
});

/**
 * Tests that getSystemDownloadsDirectory returns an existing directory or
 * creates a new directory depending on the platform. Instead of the real
 * directory, this test is executed in the temporary directory so we can safely
 * delete the created folder to check whether it is created again.
 */
add_task(async function test_getSystemDownloadsDirectory_exists_or_creates() {
  let tempDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  let downloadDir;

  // OSX / Linux / Windows but not XP/2k
  if (
    Services.appinfo.OS == "Darwin" ||
    Services.appinfo.OS == "Linux" ||
    (Services.appinfo.OS == "WINNT" &&
      parseFloat(Services.sysinfo.getProperty("version")) >= 6)
  ) {
    downloadDir = await DownloadIntegration.getSystemDownloadsDirectory();
    Assert.equal(downloadDir, tempDir.path);
    Assert.ok(await IOUtils.exists(downloadDir));

    let info = await IOUtils.stat(downloadDir);
    Assert.equal(info.type, "directory");
  } else {
    let targetPath = PathUtils.join(
      tempDir.path,
      gStringBundle.GetStringFromName("downloadsFolder")
    );
    try {
      await IOUtils.remove(targetPath);
    } catch (e) {}
    downloadDir = await DownloadIntegration.getSystemDownloadsDirectory();
    Assert.equal(downloadDir, targetPath);
    Assert.ok(await IOUtils.exists(downloadDir));

    let info = await IOUtils.stat(downloadDir);
    Assert.equal(info.type, "directory");
    await IOUtils.remove(targetPath);
  }
});

/**
 * Tests that the real directory returned by getSystemDownloadsDirectory is not
 * the one that is used during unit tests. Since this is the actual downloads
 * directory of the operating system, we don't try to delete it afterwards.
 */
add_task(async function test_getSystemDownloadsDirectory_real() {
  let fakeDownloadDir = await DownloadIntegration.getSystemDownloadsDirectory();

  let cleanup = allowDirectoriesInTest();
  let realDownloadDir = await DownloadIntegration.getSystemDownloadsDirectory();
  cleanup();

  Assert.notEqual(fakeDownloadDir, realDownloadDir);
});

/**
 * Tests that the getPreferredDownloadsDirectory returns a valid download
 * directory string path.
 */
add_task(async function test_getPreferredDownloadsDirectory() {
  let cleanupDirectories = allowDirectoriesInTest();

  let folderListPrefName = "browser.download.folderList";
  let dirPrefName = "browser.download.dir";
  function cleanupPrefs() {
    Services.prefs.clearUserPref(folderListPrefName);
    Services.prefs.clearUserPref(dirPrefName);
  }
  registerCleanupFunction(cleanupPrefs);

  // Should return the system downloads directory.
  Services.prefs.setIntPref(folderListPrefName, 1);
  let systemDir = await DownloadIntegration.getSystemDownloadsDirectory();
  let downloadDir = await DownloadIntegration.getPreferredDownloadsDirectory();
  Assert.notEqual(downloadDir, "");
  Assert.equal(downloadDir, systemDir);

  // Should return the desktop directory.
  Services.prefs.setIntPref(folderListPrefName, 0);
  downloadDir = await DownloadIntegration.getPreferredDownloadsDirectory();
  Assert.notEqual(downloadDir, "");
  Assert.equal(downloadDir, Services.dirsvc.get("Desk", Ci.nsIFile).path);

  // Should return the system downloads directory because the dir preference
  // is not set.
  Services.prefs.setIntPref(folderListPrefName, 2);
  downloadDir = await DownloadIntegration.getPreferredDownloadsDirectory();
  Assert.notEqual(downloadDir, "");
  Assert.equal(downloadDir, systemDir);

  // Should return the directory which is listed in the dir preference.
  let time = new Date().getTime();
  let tempDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  tempDir.append(time);
  Services.prefs.setComplexValue("browser.download.dir", Ci.nsIFile, tempDir);
  downloadDir = await DownloadIntegration.getPreferredDownloadsDirectory();
  Assert.notEqual(downloadDir, "");
  Assert.equal(downloadDir, tempDir.path);
  Assert.ok(await IOUtils.exists(downloadDir));
  await IOUtils.remove(tempDir.path);

  // Should return the system downloads directory beacause the path is invalid
  // in the dir preference.
  tempDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  tempDir.append("dir_not_exist");
  tempDir.append(time);
  Services.prefs.setComplexValue("browser.download.dir", Ci.nsIFile, tempDir);
  downloadDir = await DownloadIntegration.getPreferredDownloadsDirectory();
  Assert.equal(downloadDir, systemDir);

  // Should return the system downloads directory because the folderList
  // preference is invalid
  Services.prefs.setIntPref(folderListPrefName, 999);
  downloadDir = await DownloadIntegration.getPreferredDownloadsDirectory();
  Assert.equal(downloadDir, systemDir);

  cleanupPrefs();
  cleanupDirectories();
});

/**
 * Tests that the getTemporaryDownloadsDirectory returns a valid download
 * directory string path.
 */
add_task(async function test_getTemporaryDownloadsDirectory() {
  let cleanup = allowDirectoriesInTest();

  let downloadDir = await DownloadIntegration.getTemporaryDownloadsDirectory();
  Assert.notEqual(downloadDir, "");

  if ("nsILocalFileMac" in Ci) {
    let preferredDownloadDir = await DownloadIntegration.getPreferredDownloadsDirectory();
    Assert.equal(downloadDir, preferredDownloadDir);
  } else {
    let tempDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
    Assert.equal(downloadDir, tempDir.path);
  }

  cleanup();
});

// Tests DownloadObserver

/**
 * Re-enables the default observers for the following tests.
 *
 * This takes effect the first time a DownloadList object is created, and lasts
 * until this test file has completed.
 */
add_task(async function test_observers_setup() {
  DownloadIntegration.allowObservers = true;
  registerCleanupFunction(function() {
    DownloadIntegration.allowObservers = false;
  });
});

/**
 * Tests notifications prompts when observers are notified if there are public
 * and private active downloads.
 */
add_task(async function test_notifications() {
  for (let isPrivate of [false, true]) {
    mustInterruptResponses();

    let list = await promiseNewList(isPrivate);
    let download1 = await promiseNewDownload(httpUrl("interruptible.txt"));
    let download2 = await promiseNewDownload(httpUrl("interruptible.txt"));
    let download3 = await promiseNewDownload(httpUrl("interruptible.txt"));
    let promiseAttempt1 = download1.start();
    let promiseAttempt2 = download2.start();
    download3.start().catch(() => {});

    // Add downloads to list.
    await list.add(download1);
    await list.add(download2);
    await list.add(download3);
    // Cancel third download
    await download3.cancel();

    notifyPromptObservers(isPrivate, 2, 2);

    // Allow the downloads to complete.
    continueResponses();
    await promiseAttempt1;
    await promiseAttempt2;

    // Clean up.
    await list.remove(download1);
    await list.remove(download2);
    await list.remove(download3);
  }
});

/**
 * Tests that notifications prompts observers are not notified if there are no
 * public or private active downloads.
 */
add_task(async function test_no_notifications() {
  for (let isPrivate of [false, true]) {
    let list = await promiseNewList(isPrivate);
    let download1 = await promiseNewDownload(httpUrl("interruptible.txt"));
    let download2 = await promiseNewDownload(httpUrl("interruptible.txt"));
    download1.start().catch(() => {});
    download2.start().catch(() => {});

    // Add downloads to list.
    await list.add(download1);
    await list.add(download2);

    await download1.cancel();
    await download2.cancel();

    notifyPromptObservers(isPrivate, 0, 0);

    // Clean up.
    await list.remove(download1);
    await list.remove(download2);
  }
});

/**
 * Tests notifications prompts when observers are notified if there are public
 * and private active downloads at the same time.
 */
add_task(async function test_mix_notifications() {
  mustInterruptResponses();

  let publicList = await promiseNewList();
  let privateList = await Downloads.getList(Downloads.PRIVATE);
  let download1 = await promiseNewDownload(httpUrl("interruptible.txt"));
  let download2 = await promiseNewDownload(httpUrl("interruptible.txt"));
  let promiseAttempt1 = download1.start();
  let promiseAttempt2 = download2.start();

  // Add downloads to lists.
  await publicList.add(download1);
  await privateList.add(download2);

  notifyPromptObservers(true, 2, 1);

  // Allow the downloads to complete.
  continueResponses();
  await promiseAttempt1;
  await promiseAttempt2;

  // Clean up.
  await publicList.remove(download1);
  await privateList.remove(download2);
});

/**
 * Tests suspending and resuming as well as going offline and then online again.
 * The downloads should stop when suspending and start again when resuming.
 */
add_task(async function test_suspend_resume() {
  // The default wake delay is 10 seconds, so set the wake delay to be much
  // faster for these tests.
  Services.prefs.setIntPref("browser.download.manager.resumeOnWakeDelay", 5);

  let addDownload = function(list) {
    return (async function() {
      let download = await promiseNewDownload(httpUrl("interruptible.txt"));
      download.start().catch(() => {});
      list.add(download);
      return download;
    })();
  };

  let publicList = await promiseNewList();
  let privateList = await promiseNewList(true);

  let download1 = await addDownload(publicList);
  let download2 = await addDownload(publicList);
  let download3 = await addDownload(privateList);
  let download4 = await addDownload(privateList);
  let download5 = await addDownload(publicList);

  // First, check that the downloads are all canceled when going to sleep.
  Services.obs.notifyObservers(null, "sleep_notification");
  Assert.ok(download1.canceled);
  Assert.ok(download2.canceled);
  Assert.ok(download3.canceled);
  Assert.ok(download4.canceled);
  Assert.ok(download5.canceled);

  // Remove a download. It should not be started again.
  publicList.remove(download5);
  Assert.ok(download5.canceled);

  // When waking up again, the downloads start again after the wake delay. To be
  // more robust, don't check after a delay but instead just wait for the
  // downloads to finish.
  Services.obs.notifyObservers(null, "wake_notification");
  await download1.whenSucceeded();
  await download2.whenSucceeded();
  await download3.whenSucceeded();
  await download4.whenSucceeded();

  // Downloads should no longer be canceled. However, as download5 was removed
  // from the public list, it will not be restarted.
  Assert.ok(!download1.canceled);
  Assert.ok(download5.canceled);

  // Create four new downloads and check for going offline and then online again.

  download1 = await addDownload(publicList);
  download2 = await addDownload(publicList);
  download3 = await addDownload(privateList);
  download4 = await addDownload(privateList);

  // Going offline should cancel the downloads.
  Services.obs.notifyObservers(null, "network:offline-about-to-go-offline");
  Assert.ok(download1.canceled);
  Assert.ok(download2.canceled);
  Assert.ok(download3.canceled);
  Assert.ok(download4.canceled);

  // Going back online should start the downloads again.
  Services.obs.notifyObservers(
    null,
    "network:offline-status-changed",
    "online"
  );
  await download1.whenSucceeded();
  await download2.whenSucceeded();
  await download3.whenSucceeded();
  await download4.whenSucceeded();

  Services.prefs.clearUserPref("browser.download.manager.resumeOnWakeDelay");
});

/**
 * Tests both the downloads list and the in-progress downloads are clear when
 * private browsing observer is notified.
 */
add_task(async function test_exit_private_browsing() {
  mustInterruptResponses();

  let privateList = await promiseNewList(true);
  let download1 = await promiseNewDownload(httpUrl("source.txt"));
  let download2 = await promiseNewDownload(httpUrl("interruptible.txt"));
  let promiseAttempt1 = download1.start();
  download2.start();

  // Add downloads to list.
  await privateList.add(download1);
  await privateList.add(download2);

  // Complete the download.
  await promiseAttempt1;

  Assert.equal((await privateList.getAll()).length, 2);

  // Simulate exiting the private browsing.
  await new Promise(resolve => {
    DownloadIntegration._testResolveClearPrivateList = resolve;
    Services.obs.notifyObservers(null, "last-pb-context-exited");
  });
  delete DownloadIntegration._testResolveClearPrivateList;

  Assert.equal((await privateList.getAll()).length, 0);

  continueResponses();
});
