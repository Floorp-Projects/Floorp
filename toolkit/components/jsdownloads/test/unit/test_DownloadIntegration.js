/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the DownloadIntegration object.
 */

"use strict";

////////////////////////////////////////////////////////////////////////////////
//// Globals

/**
 * Enable test mode for the _confirmCancelDownloads method to return
 * the number of downloads instead of showing the prompt to cancel or not.
 */
function enableObserversTestMode() {
  DownloadIntegration.testMode = true;
  DownloadIntegration.dontLoadObservers = false;
  function cleanup() {
    DownloadIntegration.testMode = false;
    DownloadIntegration.dontLoadObservers = true;
  }
  do_register_cleanup(cleanup);
}

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
  DownloadIntegration.testPromptDownloads = -1;
  Services.obs.notifyObservers(cancelQuit, "quit-application-requested", null);
  do_check_eq(DownloadIntegration.testPromptDownloads, aExpectedCount);

  // Notify offline requested observer.
  DownloadIntegration.testPromptDownloads = -1;
  Services.obs.notifyObservers(cancelQuit, "offline-requested", null);
  do_check_eq(DownloadIntegration.testPromptDownloads, aExpectedCount);

  if (aIsPrivate) {
    // Notify last private browsing requested observer.
    DownloadIntegration.testPromptDownloads = -1;
    Services.obs.notifyObservers(cancelQuit, "last-pb-context-exiting", null);
    do_check_eq(DownloadIntegration.testPromptDownloads, aExpectedPBCount);
  }
}

////////////////////////////////////////////////////////////////////////////////
//// Tests

XPCOMUtils.defineLazyGetter(this, "gStringBundle", function() {
  return Services.strings.
    createBundle("chrome://mozapps/locale/downloads/downloads.properties");
});

/**
 * Tests that the getSystemDownloadsDirectory returns a valid nsFile
 * download directory object.
 */
add_task(function test_getSystemDownloadsDirectory()
{
  // Enable test mode for the getSystemDownloadsDirectory method to return
  // temp directory instead so we can check whether the desired directory
  // is created or not.
  DownloadIntegration.testMode = true;
  function cleanup() {
    DownloadIntegration.testMode = false;
  }
  do_register_cleanup(cleanup);

  let tempDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  let downloadDir;

  // OSX / Linux / Windows but not XP/2k
  if (Services.appinfo.OS == "Darwin" ||
      Services.appinfo.OS == "Linux" ||
      (Services.appinfo.OS == "WINNT" &&
       parseFloat(Services.sysinfo.getProperty("version")) >= 6)) {
    downloadDir = yield DownloadIntegration.getSystemDownloadsDirectory();
    do_check_true(downloadDir instanceof Ci.nsIFile);
    do_check_eq(downloadDir.path, tempDir.path);
    do_check_true(yield OS.File.exists(downloadDir.path));

    let info = yield OS.File.stat(downloadDir.path);
    do_check_true(info.isDir);
  } else {
    let targetPath = OS.Path.join(tempDir.path,
                       gStringBundle.GetStringFromName("downloadsFolder"));
    try {
      yield OS.File.removeEmptyDir(targetPath);
    } catch(e) {}
    downloadDir = yield DownloadIntegration.getSystemDownloadsDirectory();
    do_check_eq(downloadDir.path, targetPath);
    do_check_true(yield OS.File.exists(downloadDir.path));

    let info = yield OS.File.stat(downloadDir.path);
    do_check_true(info.isDir);
    yield OS.File.removeEmptyDir(targetPath);
  }

  let downloadDirBefore = yield DownloadIntegration.getSystemDownloadsDirectory();
  cleanup();
  let downloadDirAfter = yield DownloadIntegration.getSystemDownloadsDirectory();
  do_check_false(downloadDirBefore.equals(downloadDirAfter));
});

/**
 * Tests that the getUserDownloadsDirectory returns a valid nsFile
 * download directory object.
 */
add_task(function test_getUserDownloadsDirectory()
{
  let folderListPrefName = "browser.download.folderList";
  let dirPrefName = "browser.download.dir";
  function cleanup() {
    Services.prefs.clearUserPref(folderListPrefName);
    Services.prefs.clearUserPref(dirPrefName);
  }
  do_register_cleanup(cleanup);

  // Should return the system downloads directory.
  Services.prefs.setIntPref(folderListPrefName, 1);
  let systemDir = yield DownloadIntegration.getSystemDownloadsDirectory();
  let downloadDir = yield DownloadIntegration.getUserDownloadsDirectory();
  do_check_true(downloadDir instanceof Ci.nsIFile);
  do_check_eq(downloadDir.path, systemDir.path);

  // Should return the desktop directory.
  Services.prefs.setIntPref(folderListPrefName, 0);
  downloadDir = yield DownloadIntegration.getUserDownloadsDirectory();
  do_check_true(downloadDir instanceof Ci.nsIFile);
  do_check_eq(downloadDir.path, Services.dirsvc.get("Desk", Ci.nsIFile).path);

  // Should return the system downloads directory because the dir preference
  // is not set.
  Services.prefs.setIntPref(folderListPrefName, 2);
  let downloadDir = yield DownloadIntegration.getUserDownloadsDirectory();
  do_check_true(downloadDir instanceof Ci.nsIFile);
  do_check_eq(downloadDir.path, systemDir.path);

  // Should return the directory which is listed in the dir preference.
  let time = (new Date()).getTime();
  let tempDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  tempDir.append(time);
  Services.prefs.setComplexValue("browser.download.dir", Ci.nsIFile, tempDir);
  downloadDir = yield DownloadIntegration.getUserDownloadsDirectory();
  do_check_true(downloadDir instanceof Ci.nsIFile);
  do_check_eq(downloadDir.path,  tempDir.path);
  do_check_true(yield OS.File.exists(downloadDir.path));
  yield OS.File.removeEmptyDir(tempDir.path);

  // Should return the system downloads directory beacause the path is invalid
  // in the dir preference.
  tempDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  tempDir.append("dir_not_exist");
  tempDir.append(time);
  Services.prefs.setComplexValue("browser.download.dir", Ci.nsIFile, tempDir);
  downloadDir = yield DownloadIntegration.getUserDownloadsDirectory();
  do_check_eq(downloadDir.path, systemDir.path);

  // Should return the system downloads directory because the folderList
  // preference is invalid
  Services.prefs.setIntPref(folderListPrefName, 999);
  let downloadDir = yield DownloadIntegration.getUserDownloadsDirectory();
  do_check_eq(downloadDir.path, systemDir.path);

  cleanup();
});

/**
 * Tests that the getTemporaryDownloadsDirectory returns a valid nsFile
 * download directory object.
 */
add_task(function test_getTemporaryDownloadsDirectory()
{
  let downloadDir = yield DownloadIntegration.getTemporaryDownloadsDirectory();
  do_check_true(downloadDir instanceof Ci.nsIFile);

  if ("nsILocalFileMac" in Ci) {
    let userDownloadDir = yield DownloadIntegration.getUserDownloadsDirectory();
    do_check_eq(downloadDir.path, userDownloadDir.path);
  } else {
    let tempDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
    do_check_eq(downloadDir.path, tempDir.path);
  }
});

////////////////////////////////////////////////////////////////////////////////
//// Tests DownloadObserver

/**
 * Tests notifications prompts when observers are notified if there are public
 * and private active downloads.
 */
add_task(function test_notifications()
{
  enableObserversTestMode();

  for (let isPrivate of [false, true]) {
    mustInterruptResponses();

    let list = yield promiseNewList(isPrivate);
    let download1 = yield promiseNewDownload(httpUrl("interruptible.txt"));
    let download2 = yield promiseNewDownload(httpUrl("interruptible.txt"));
    let download3 = yield promiseNewDownload(httpUrl("interruptible.txt"));
    let promiseAttempt1 = download1.start();
    let promiseAttempt2 = download2.start();
    download3.start();

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
add_task(function test_no_notifications()
{
  enableObserversTestMode();

  for (let isPrivate of [false, true]) {
    let list = yield promiseNewList(isPrivate);
    let download1 = yield promiseNewDownload(httpUrl("interruptible.txt"));
    let download2 = yield promiseNewDownload(httpUrl("interruptible.txt"));
    download1.start();
    download2.start();

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
add_task(function test_mix_notifications()
{
  enableObserversTestMode();
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
add_task(function test_suspend_resume()
{
  enableObserversTestMode();

  // The default wake delay is 10 seconds, so set the wake delay to be much
  // faster for these tests.
  Services.prefs.setIntPref("browser.download.manager.resumeOnWakeDelay", 5);

  let addDownload = function(list)
  {
    return Task.spawn(function () {
      let download = yield promiseNewDownload(httpUrl("interruptible.txt"));
      download.start();
      list.add(download);
      throw new Task.Result(download);
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
add_task(function test_exit_private_browsing()
{
  enableObserversTestMode();
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
  DownloadIntegration._deferTestClearPrivateList = Promise.defer();
  Services.obs.notifyObservers(null, "last-pb-context-exited", null);
  let result = yield DownloadIntegration._deferTestClearPrivateList.promise;

  do_check_eq(result, "success");
  do_check_eq((yield privateList.getAll()).length, 0);

  continueResponses();
});

////////////////////////////////////////////////////////////////////////////////
//// Termination

let tailFile = do_get_file("tail.js");
Services.scriptloader.loadSubScript(NetUtil.newURI(tailFile).spec);
