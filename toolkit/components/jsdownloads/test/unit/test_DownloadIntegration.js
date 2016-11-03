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
  let cancelQuit = Cc["@mozilla.org/supports-PRBool;1"].
                   createInstance(Ci.nsISupportsPRBool);

  // Notify quit application requested observer.
  DownloadIntegration._testPromptDownloads = -1;
  Services.obs.notifyObservers(cancelQuit, "quit-application-requested", null);
  do_check_eq(DownloadIntegration._testPromptDownloads, aExpectedCount);

  // Notify offline requested observer.
  DownloadIntegration._testPromptDownloads = -1;
  Services.obs.notifyObservers(cancelQuit, "offline-requested", null);
  do_check_eq(DownloadIntegration._testPromptDownloads, aExpectedCount);

  if (aIsPrivate) {
    // Notify last private browsing requested observer.
    DownloadIntegration._testPromptDownloads = -1;
    Services.obs.notifyObservers(cancelQuit, "last-pb-context-exiting", null);
    do_check_eq(DownloadIntegration._testPromptDownloads, aExpectedPBCount);
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
  do_register_cleanup(cleanup);
  return cleanup;
}

XPCOMUtils.defineLazyGetter(this, "gStringBundle", function() {
  return Services.strings.
    createBundle("chrome://mozapps/locale/downloads/downloads.properties");
});

/**
 * Tests that getSystemDownloadsDirectory returns an existing directory or
 * creates a new directory depending on the platform. Instead of the real
 * directory, this test is executed in the temporary directory so we can safely
 * delete the created folder to check whether it is created again.
 */
add_task(function* test_getSystemDownloadsDirectory_exists_or_creates()
{
  let tempDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  let downloadDir;

  // OSX / Linux / Windows but not XP/2k
  if (Services.appinfo.OS == "Darwin" ||
      Services.appinfo.OS == "Linux" ||
      (Services.appinfo.OS == "WINNT" &&
       parseFloat(Services.sysinfo.getProperty("version")) >= 6)) {
    downloadDir = yield DownloadIntegration.getSystemDownloadsDirectory();
    do_check_eq(downloadDir, tempDir.path);
    do_check_true(yield OS.File.exists(downloadDir));

    let info = yield OS.File.stat(downloadDir);
    do_check_true(info.isDir);
  } else {
    let targetPath = OS.Path.join(tempDir.path,
                       gStringBundle.GetStringFromName("downloadsFolder"));
    try {
      yield OS.File.removeEmptyDir(targetPath);
    } catch (e) {}
    downloadDir = yield DownloadIntegration.getSystemDownloadsDirectory();
    do_check_eq(downloadDir, targetPath);
    do_check_true(yield OS.File.exists(downloadDir));

    let info = yield OS.File.stat(downloadDir);
    do_check_true(info.isDir);
    yield OS.File.removeEmptyDir(targetPath);
  }
});

/**
 * Tests that the real directory returned by getSystemDownloadsDirectory is not
 * the one that is used during unit tests. Since this is the actual downloads
 * directory of the operating system, we don't try to delete it afterwards.
 */
add_task(function* test_getSystemDownloadsDirectory_real()
{
  let fakeDownloadDir = yield DownloadIntegration.getSystemDownloadsDirectory();

  let cleanup = allowDirectoriesInTest();
  let realDownloadDir = yield DownloadIntegration.getSystemDownloadsDirectory();
  cleanup();

  do_check_neq(fakeDownloadDir, realDownloadDir);
});

/**
 * Tests that the getPreferredDownloadsDirectory returns a valid download
 * directory string path.
 */
add_task(function* test_getPreferredDownloadsDirectory()
{
  let cleanupDirectories = allowDirectoriesInTest();

  let folderListPrefName = "browser.download.folderList";
  let dirPrefName = "browser.download.dir";
  function cleanupPrefs() {
    Services.prefs.clearUserPref(folderListPrefName);
    Services.prefs.clearUserPref(dirPrefName);
  }
  do_register_cleanup(cleanupPrefs);

  // Should return the system downloads directory.
  Services.prefs.setIntPref(folderListPrefName, 1);
  let systemDir = yield DownloadIntegration.getSystemDownloadsDirectory();
  let downloadDir = yield DownloadIntegration.getPreferredDownloadsDirectory();
  do_check_neq(downloadDir, "");
  do_check_eq(downloadDir, systemDir);

  // Should return the desktop directory.
  Services.prefs.setIntPref(folderListPrefName, 0);
  downloadDir = yield DownloadIntegration.getPreferredDownloadsDirectory();
  do_check_neq(downloadDir, "");
  do_check_eq(downloadDir, Services.dirsvc.get("Desk", Ci.nsIFile).path);

  // Should return the system downloads directory because the dir preference
  // is not set.
  Services.prefs.setIntPref(folderListPrefName, 2);
  downloadDir = yield DownloadIntegration.getPreferredDownloadsDirectory();
  do_check_neq(downloadDir, "");
  do_check_eq(downloadDir, systemDir);

  // Should return the directory which is listed in the dir preference.
  let time = (new Date()).getTime();
  let tempDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  tempDir.append(time);
  Services.prefs.setComplexValue("browser.download.dir", Ci.nsIFile, tempDir);
  downloadDir = yield DownloadIntegration.getPreferredDownloadsDirectory();
  do_check_neq(downloadDir, "");
  do_check_eq(downloadDir,  tempDir.path);
  do_check_true(yield OS.File.exists(downloadDir));
  yield OS.File.removeEmptyDir(tempDir.path);

  // Should return the system downloads directory beacause the path is invalid
  // in the dir preference.
  tempDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  tempDir.append("dir_not_exist");
  tempDir.append(time);
  Services.prefs.setComplexValue("browser.download.dir", Ci.nsIFile, tempDir);
  downloadDir = yield DownloadIntegration.getPreferredDownloadsDirectory();
  do_check_eq(downloadDir, systemDir);

  // Should return the system downloads directory because the folderList
  // preference is invalid
  Services.prefs.setIntPref(folderListPrefName, 999);
  downloadDir = yield DownloadIntegration.getPreferredDownloadsDirectory();
  do_check_eq(downloadDir, systemDir);

  cleanupPrefs();
  cleanupDirectories();
});

/**
 * Tests that the getTemporaryDownloadsDirectory returns a valid download
 * directory string path.
 */
add_task(function* test_getTemporaryDownloadsDirectory()
{
  let cleanup = allowDirectoriesInTest();

  let downloadDir = yield DownloadIntegration.getTemporaryDownloadsDirectory();
  do_check_neq(downloadDir, "");

  if ("nsILocalFileMac" in Ci) {
    let preferredDownloadDir = yield DownloadIntegration.getPreferredDownloadsDirectory();
    do_check_eq(downloadDir, preferredDownloadDir);
  } else {
    let tempDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
    do_check_eq(downloadDir, tempDir.path);
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
add_task(function* test_observers_setup()
{
  DownloadIntegration.allowObservers = true;
  do_register_cleanup(function () {
    DownloadIntegration.allowObservers = false;
  });
});

/**
 * Tests notifications prompts when observers are notified if there are public
 * and private active downloads.
 */
add_task(function* test_notifications()
{
  for (let isPrivate of [false, true]) {
    mustInterruptResponses();

    let list = yield promiseNewList(isPrivate);
    let download1 = yield promiseNewDownload(httpUrl("interruptible.txt"));
    let download2 = yield promiseNewDownload(httpUrl("interruptible.txt"));
    let download3 = yield promiseNewDownload(httpUrl("interruptible.txt"));
    let promiseAttempt1 = download1.start();
    let promiseAttempt2 = download2.start();
    download3.start().catch(() => {});

    // Add downloads to list.
    yield list.add(download1);
    yield list.add(download2);
    yield list.add(download3);
    // Cancel third download
    yield download3.cancel();

    notifyPromptObservers(isPrivate, 2, 2);

    // Allow the downloads to complete.
    continueResponses();
    yield promiseAttempt1;
    yield promiseAttempt2;

    // Clean up.
    yield list.remove(download1);
    yield list.remove(download2);
    yield list.remove(download3);
  }
});

/**
 * Tests that notifications prompts observers are not notified if there are no
 * public or private active downloads.
 */
add_task(function* test_no_notifications()
{
  for (let isPrivate of [false, true]) {
    let list = yield promiseNewList(isPrivate);
    let download1 = yield promiseNewDownload(httpUrl("interruptible.txt"));
    let download2 = yield promiseNewDownload(httpUrl("interruptible.txt"));
    download1.start().catch(() => {});
    download2.start().catch(() => {});

    // Add downloads to list.
    yield list.add(download1);
    yield list.add(download2);

    yield download1.cancel();
    yield download2.cancel();

    notifyPromptObservers(isPrivate, 0, 0);

    // Clean up.
    yield list.remove(download1);
    yield list.remove(download2);
  }
});

/**
 * Tests notifications prompts when observers are notified if there are public
 * and private active downloads at the same time.
 */
add_task(function* test_mix_notifications()
{
  mustInterruptResponses();

  let publicList = yield promiseNewList();
  let privateList = yield Downloads.getList(Downloads.PRIVATE);
  let download1 = yield promiseNewDownload(httpUrl("interruptible.txt"));
  let download2 = yield promiseNewDownload(httpUrl("interruptible.txt"));
  let promiseAttempt1 = download1.start();
  let promiseAttempt2 = download2.start();

  // Add downloads to lists.
  yield publicList.add(download1);
  yield privateList.add(download2);

  notifyPromptObservers(true, 2, 1);

  // Allow the downloads to complete.
  continueResponses();
  yield promiseAttempt1;
  yield promiseAttempt2;

  // Clean up.
  yield publicList.remove(download1);
  yield privateList.remove(download2);
});

/**
 * Tests suspending and resuming as well as going offline and then online again.
 * The downloads should stop when suspending and start again when resuming.
 */
add_task(function* test_suspend_resume()
{
  // The default wake delay is 10 seconds, so set the wake delay to be much
  // faster for these tests.
  Services.prefs.setIntPref("browser.download.manager.resumeOnWakeDelay", 5);

  let addDownload = function(list)
  {
    return Task.spawn(function* () {
      let download = yield promiseNewDownload(httpUrl("interruptible.txt"));
      download.start().catch(() => {});
      list.add(download);
      return download;
    });
  }

  let publicList = yield promiseNewList();
  let privateList = yield promiseNewList(true);

  let download1 = yield addDownload(publicList);
  let download2 = yield addDownload(publicList);
  let download3 = yield addDownload(privateList);
  let download4 = yield addDownload(privateList);
  let download5 = yield addDownload(publicList);

  // First, check that the downloads are all canceled when going to sleep.
  Services.obs.notifyObservers(null, "sleep_notification", null);
  do_check_true(download1.canceled);
  do_check_true(download2.canceled);
  do_check_true(download3.canceled);
  do_check_true(download4.canceled);
  do_check_true(download5.canceled);

  // Remove a download. It should not be started again.
  publicList.remove(download5);
  do_check_true(download5.canceled);

  // When waking up again, the downloads start again after the wake delay. To be
  // more robust, don't check after a delay but instead just wait for the
  // downloads to finish.
  Services.obs.notifyObservers(null, "wake_notification", null);
  yield download1.whenSucceeded();
  yield download2.whenSucceeded();
  yield download3.whenSucceeded();
  yield download4.whenSucceeded();

  // Downloads should no longer be canceled. However, as download5 was removed
  // from the public list, it will not be restarted.
  do_check_false(download1.canceled);
  do_check_true(download5.canceled);

  // Create four new downloads and check for going offline and then online again.

  download1 = yield addDownload(publicList);
  download2 = yield addDownload(publicList);
  download3 = yield addDownload(privateList);
  download4 = yield addDownload(privateList);

  // Going offline should cancel the downloads.
  Services.obs.notifyObservers(null, "network:offline-about-to-go-offline", null);
  do_check_true(download1.canceled);
  do_check_true(download2.canceled);
  do_check_true(download3.canceled);
  do_check_true(download4.canceled);

  // Going back online should start the downloads again.
  Services.obs.notifyObservers(null, "network:offline-status-changed", "online");
  yield download1.whenSucceeded();
  yield download2.whenSucceeded();
  yield download3.whenSucceeded();
  yield download4.whenSucceeded();

  Services.prefs.clearUserPref("browser.download.manager.resumeOnWakeDelay");
});

/**
 * Tests both the downloads list and the in-progress downloads are clear when
 * private browsing observer is notified.
 */
add_task(function* test_exit_private_browsing()
{
  mustInterruptResponses();

  let privateList = yield promiseNewList(true);
  let download1 = yield promiseNewDownload(httpUrl("source.txt"));
  let download2 = yield promiseNewDownload(httpUrl("interruptible.txt"));
  let promiseAttempt1 = download1.start();
  let promiseAttempt2 = download2.start();

  // Add downloads to list.
  yield privateList.add(download1);
  yield privateList.add(download2);

  // Complete the download.
  yield promiseAttempt1;

  do_check_eq((yield privateList.getAll()).length, 2);

  // Simulate exiting the private browsing.
  yield new Promise(resolve => {
    DownloadIntegration._testResolveClearPrivateList = resolve;
    Services.obs.notifyObservers(null, "last-pb-context-exited", null);
  });
  delete DownloadIntegration._testResolveClearPrivateList;

  do_check_eq((yield privateList.getAll()).length, 0);

  continueResponses();
});
