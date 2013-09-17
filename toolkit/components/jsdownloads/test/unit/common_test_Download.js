/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This script is loaded by "test_DownloadCore.js" and "test_DownloadLegacy.js"
 * with different values of the gUseLegacySaver variable, to apply tests to both
 * the "copy" and "legacy" saver implementations.
 */

"use strict";

////////////////////////////////////////////////////////////////////////////////
//// Globals

/**
 * Creates and starts a new download, using either DownloadCopySaver or
 * DownloadLegacySaver based on the current test run.
 *
 * @return {Promise}
 * @resolves The newly created Download object.  The download may be in progress
 *           or already finished.  The promiseDownloadStopped function can be
 *           used to wait for completion.
 * @rejects JavaScript exception.
 */
function promiseStartDownload(aSourceUrl) {
  if (gUseLegacySaver) {
    return promiseStartLegacyDownload(aSourceUrl);
  }

  return promiseNewDownload(aSourceUrl).then(download => {
    download.start();
    return download;
  });
}

/**
 * Creates and starts a new download, configured to keep partial data, and
 * returns only when the first part of "interruptible_resumable.txt" has been
 * saved to disk.  You must call "continueResponses" to allow the interruptible
 * request to continue.
 *
 * This function uses either DownloadCopySaver or DownloadLegacySaver based on
 * the current test run.
 *
 * @return {Promise}
 * @resolves The newly created Download object, still in progress.
 * @rejects JavaScript exception.
 */
function promiseStartDownload_tryToKeepPartialData() {
  return Task.spawn(function () {
    mustInterruptResponses();

    // Start a new download and configure it to keep partially downloaded data.
    let download;
    if (!gUseLegacySaver) {
      let targetFilePath = getTempFile(TEST_TARGET_FILE_NAME).path;
      download = yield Downloads.createDownload({
        source: httpUrl("interruptible_resumable.txt"),
        target: { path: targetFilePath,
                  partFilePath: targetFilePath + ".part" },
      });
      download.tryToKeepPartialData = true;
      download.start();
    } else {
      // Start a download using nsIExternalHelperAppService, that is configured
      // to keep partially downloaded data by default.
      download = yield promiseStartExternalHelperAppServiceDownload();
    }

    yield promiseDownloadMidway(download);
    yield promisePartFileReady(download);

    throw new Task.Result(download);
  });
}

/**
 * This function should be called after the progress notification for a download
 * is received, and waits for the worker thread of BackgroundFileSaver to
 * receive the data to be written to the ".part" file on disk.
 *
 * @return {Promise}
 * @resolves When the ".part" file has been written to disk.
 * @rejects JavaScript exception.
 */
function promisePartFileReady(aDownload) {
  return Task.spawn(function () {
    // We don't have control over the file output code in BackgroundFileSaver.
    // After we receive the download progress notification, we may only check
    // that the ".part" file has been created, while its size cannot be
    // determined because the file is currently open.
    try {
      do {
        yield promiseTimeout(50);
      } while (!(yield OS.File.exists(aDownload.target.partFilePath)));
    } catch (ex if ex instanceof OS.File.Error) {
      // This indicates that the file has been created and cannot be accessed.
      // The specific error might vary with the platform.
      do_print("Expected exception while checking existence: " + ex.toString());
      // Wait some more time to allow the write to complete.
      yield promiseTimeout(100);
    }
  });
}

////////////////////////////////////////////////////////////////////////////////
//// Tests

/**
 * Executes a download and checks its basic properties after construction.
 * The download is started by constructing the simplest Download object with
 * the "copy" saver, or using the legacy nsITransfer interface.
 */
add_task(function test_basic()
{
  let targetFile = getTempFile(TEST_TARGET_FILE_NAME);

  let download;
  if (!gUseLegacySaver) {
    // When testing DownloadCopySaver, we have control over the download, thus
    // we can check its basic properties before it starts.
    download = yield Downloads.createDownload({
      source: { url: httpUrl("source.txt") },
      target: { path: targetFile.path },
      saver: { type: "copy" },
    });

    do_check_eq(download.source.url, httpUrl("source.txt"));
    do_check_eq(download.target.path, targetFile.path);

    yield download.start();
  } else {
    // When testing DownloadLegacySaver, the download is already started when it
    // is created, thus we must check its basic properties while in progress.
    download = yield promiseStartLegacyDownload(null,
                                                { targetFile: targetFile });

    do_check_eq(download.source.url, httpUrl("source.txt"));
    do_check_eq(download.target.path, targetFile.path);

    yield promiseDownloadStopped(download);
  }

  // Check additional properties on the finished download.
  do_check_true(download.source.referrer === null);

  yield promiseVerifyContents(download.target.path, TEST_DATA_SHORT);
});

/**
 * Executes a download with the tryToKeepPartialData property set, and ensures
 * that the file is saved correctly.  When testing DownloadLegacySaver, the
 * download is executed using the nsIExternalHelperAppService component.
 */
add_task(function test_basic_tryToKeepPartialData()
{
  let download = yield promiseStartDownload_tryToKeepPartialData();
  continueResponses();
  yield promiseDownloadStopped(download);

  // The target file should now have been created, and the ".part" file deleted.
  yield promiseVerifyContents(download.target.path,
                              TEST_DATA_SHORT + TEST_DATA_SHORT);
  do_check_false(yield OS.File.exists(download.target.partFilePath));
});

/**
 * Checks the referrer for downloads.
 */
add_task(function test_referrer()
{
  let sourcePath = "/test_referrer.txt";
  let sourceUrl = httpUrl("test_referrer.txt");
  let targetPath = getTempFile(TEST_TARGET_FILE_NAME).path;

  function cleanup() {
    gHttpServer.registerPathHandler(sourcePath, null);
  }
  do_register_cleanup(cleanup);

  gHttpServer.registerPathHandler(sourcePath, function (aRequest, aResponse) {
    aResponse.setHeader("Content-Type", "text/plain", false);

    do_check_true(aRequest.hasHeader("Referer"));
    do_check_eq(aRequest.getHeader("Referer"), TEST_REFERRER_URL);
  });
  let download = yield Downloads.createDownload({
    source: { url: sourceUrl, referrer: TEST_REFERRER_URL },
    target: targetPath,
  });
  do_check_eq(download.source.referrer, TEST_REFERRER_URL);
  yield download.start();

  download = yield Downloads.createDownload({
    source: { url: sourceUrl, referrer: TEST_REFERRER_URL,
              isPrivate: true },
    target: targetPath,
  });
  do_check_eq(download.source.referrer, TEST_REFERRER_URL);
  yield download.start();

  // Test the download still works for non-HTTP channel with referrer.
  sourceUrl = "data:text/html,<html><body></body></html>";
  download = yield Downloads.createDownload({
    source: { url: sourceUrl, referrer: TEST_REFERRER_URL },
    target: targetPath,
  });
  do_check_eq(download.source.referrer, TEST_REFERRER_URL);
  yield download.start();

  cleanup();
});

/**
 * Checks initial and final state and progress for a successful download.
 */
add_task(function test_initial_final_state()
{
  let download;
  if (!gUseLegacySaver) {
    // When testing DownloadCopySaver, we have control over the download, thus
    // we can check its state before it starts.
    download = yield promiseNewDownload();

    do_check_true(download.stopped);
    do_check_false(download.succeeded);
    do_check_false(download.canceled);
    do_check_true(download.error === null);
    do_check_eq(download.progress, 0);
    do_check_true(download.startTime === null);

    yield download.start();
  } else {
    // When testing DownloadLegacySaver, the download is already started when it
    // is created, thus we cannot check its initial state.
    download = yield promiseStartLegacyDownload();
    yield promiseDownloadStopped(download);
  }

  do_check_true(download.stopped);
  do_check_true(download.succeeded);
  do_check_false(download.canceled);
  do_check_true(download.error === null);
  do_check_eq(download.progress, 100);
  do_check_true(isValidDate(download.startTime));
});

/**
 * Checks the notification of the final download state.
 */
add_task(function test_final_state_notified()
{
  mustInterruptResponses();

  let download = yield promiseStartDownload(httpUrl("interruptible.txt"));

  let onchangeNotified = false;
  let lastNotifiedStopped;
  let lastNotifiedProgress;
  download.onchange = function () {
    onchangeNotified = true;
    lastNotifiedStopped = download.stopped;
    lastNotifiedProgress = download.progress;
  };

  // Allow the download to complete.
  let promiseAttempt = download.start();
  continueResponses();
  yield promiseAttempt;

  // The view should have been notified before the download completes.
  do_check_true(onchangeNotified);
  do_check_true(lastNotifiedStopped);
  do_check_eq(lastNotifiedProgress, 100);
});

/**
 * Checks intermediate progress for a successful download.
 */
add_task(function test_intermediate_progress()
{
  mustInterruptResponses();

  let download = yield promiseStartDownload(httpUrl("interruptible.txt"));

  yield promiseDownloadMidway(download);

  do_check_true(download.hasProgress);
  do_check_eq(download.currentBytes, TEST_DATA_SHORT.length);
  do_check_eq(download.totalBytes, TEST_DATA_SHORT.length * 2);

  // Continue after the first chunk of data is fully received.
  continueResponses();
  yield promiseDownloadStopped(download);

  do_check_true(download.stopped);
  do_check_eq(download.progress, 100);

  yield promiseVerifyContents(download.target.path,
                              TEST_DATA_SHORT + TEST_DATA_SHORT);
});

/**
 * Downloads a file with a "Content-Length" of 0 and checks the progress.
 */
add_task(function test_empty_progress()
{
  let download = yield promiseStartDownload(httpUrl("empty.txt"));
  yield promiseDownloadStopped(download);

  do_check_true(download.stopped);
  do_check_true(download.hasProgress);
  do_check_eq(download.progress, 100);
  do_check_eq(download.currentBytes, 0);
  do_check_eq(download.totalBytes, 0);

  // We should have received the content type even for an empty file.
  do_check_eq(download.contentType, "text/plain");

  do_check_eq((yield OS.File.stat(download.target.path)).size, 0);
});

/**
 * Downloads a file with a "Content-Length" of 0 with the tryToKeepPartialData
 * property set, and ensures that the file is saved correctly.
 */
add_task(function test_empty_progress_tryToKeepPartialData()
{
  // Start a new download and configure it to keep partially downloaded data.
  let download;
  if (!gUseLegacySaver) {
    let targetFilePath = getTempFile(TEST_TARGET_FILE_NAME).path;
    download = yield Downloads.createDownload({
      source: httpUrl("empty.txt"),
      target: { path: targetFilePath,
                partFilePath: targetFilePath + ".part" },
    });
    download.tryToKeepPartialData = true;
    download.start();
  } else {
    // Start a download using nsIExternalHelperAppService, that is configured
    // to keep partially downloaded data by default.
    download = yield promiseStartExternalHelperAppServiceDownload(
                                                         httpUrl("empty.txt"));
  }
  yield promiseDownloadStopped(download);

  // The target file should now have been created, and the ".part" file deleted.
  do_check_eq((yield OS.File.stat(download.target.path)).size, 0);
  do_check_false(yield OS.File.exists(download.target.partFilePath));
});

/**
 * Downloads an empty file with no "Content-Length" and checks the progress.
 */
add_task(function test_empty_noprogress()
{
  let sourcePath = "/test_empty_noprogress.txt";
  let sourceUrl = httpUrl("test_empty_noprogress.txt");
  let deferRequestReceived = Promise.defer();

  // Register an interruptible handler that notifies us when the request occurs.
  function cleanup() {
    gHttpServer.registerPathHandler(sourcePath, null);
  }
  do_register_cleanup(cleanup);

  registerInterruptibleHandler(sourcePath,
    function firstPart(aRequest, aResponse) {
      aResponse.setHeader("Content-Type", "text/plain", false);
      deferRequestReceived.resolve();
    }, function secondPart(aRequest, aResponse) { });

  // Start the download, without allowing the request to finish.
  mustInterruptResponses();
  let download;
  if (!gUseLegacySaver) {
    // When testing DownloadCopySaver, we have control over the download, thus
    // we can hook its onchange callback that will be notified when the
    // download starts.
    download = yield promiseNewDownload(sourceUrl);

    download.onchange = function () {
      if (!download.stopped) {
        do_check_false(download.hasProgress);
        do_check_eq(download.currentBytes, 0);
        do_check_eq(download.totalBytes, 0);
      }
    };

    download.start();
  } else {
    // When testing DownloadLegacySaver, the download is already started when it
    // is created, and it may have already made all needed property change
    // notifications, thus there is no point in checking the onchange callback.
    download = yield promiseStartLegacyDownload(sourceUrl);
  }

  // Wait for the request to be received by the HTTP server, but don't allow the
  // request to finish yet.  Before checking the download state, wait for the
  // events to be processed by the client.
  yield deferRequestReceived.promise;
  yield promiseExecuteSoon();

  // Check that this download has no progress report.
  do_check_false(download.stopped);
  do_check_false(download.hasProgress);
  do_check_eq(download.currentBytes, 0);
  do_check_eq(download.totalBytes, 0);

  // Now allow the response to finish.
  continueResponses();
  yield promiseDownloadStopped(download);

  // We should have received the content type even if no progress is reported.
  do_check_eq(download.contentType, "text/plain");

  // Verify the state of the completed download.
  do_check_true(download.stopped);
  do_check_false(download.hasProgress);
  do_check_eq(download.progress, 100);
  do_check_eq(download.currentBytes, 0);
  do_check_eq(download.totalBytes, 0);

  do_check_eq((yield OS.File.stat(download.target.path)).size, 0);
});

/**
 * Calls the "start" method two times before the download is finished.
 */
add_task(function test_start_twice()
{
  mustInterruptResponses();

  let download;
  if (!gUseLegacySaver) {
    // When testing DownloadCopySaver, we have control over the download, thus
    // we can start the download later during the test.
    download = yield promiseNewDownload(httpUrl("interruptible.txt"));
  } else {
    // When testing DownloadLegacySaver, the download is already started when it
    // is created.  Effectively, we are starting the download three times.
    download = yield promiseStartLegacyDownload(httpUrl("interruptible.txt"));
  }

  // Call the start method two times.
  let promiseAttempt1 = download.start();
  let promiseAttempt2 = download.start();

  // Allow the download to finish.
  continueResponses();

  // Both promises should now be resolved.
  yield promiseAttempt1;
  yield promiseAttempt2;

  do_check_true(download.stopped);
  do_check_true(download.succeeded);
  do_check_false(download.canceled);
  do_check_true(download.error === null);

  yield promiseVerifyContents(download.target.path,
                              TEST_DATA_SHORT + TEST_DATA_SHORT);
});

/**
 * Cancels a download and verifies that its state is reported correctly.
 */
add_task(function test_cancel_midway()
{
  mustInterruptResponses();

  // In this test case, we execute different checks that are only possible with
  // DownloadCopySaver or DownloadLegacySaver respectively.
  let download;
  let options = {};
  if (!gUseLegacySaver) {
    download = yield promiseNewDownload(httpUrl("interruptible.txt"));
  } else {
    download = yield promiseStartLegacyDownload(httpUrl("interruptible.txt"),
                                                options);
  }

  // Cancel the download after receiving the first part of the response.
  let deferCancel = Promise.defer();
  let onchange = function () {
    if (!download.stopped && !download.canceled && download.progress == 50) {
      // Cancel the download immediately during the notification.
      deferCancel.resolve(download.cancel());

      // The state change happens immediately after calling "cancel", but
      // temporary files or part files may still exist at this point.
      do_check_true(download.canceled);
    }
  };

  // Register for the notification, but also call the function directly in
  // case the download already reached the expected progress.  This may happen
  // when using DownloadLegacySaver.
  download.onchange = onchange;
  onchange();

  let promiseAttempt;
  if (!gUseLegacySaver) {
    promiseAttempt = download.start();
  }

  // Wait on the promise returned by the "cancel" method to ensure that the
  // cancellation process finished and temporary files were removed.
  yield deferCancel.promise;

  if (gUseLegacySaver) {
    // The nsIWebBrowserPersist instance should have been canceled now.
    do_check_eq(options.outPersist.result, Cr.NS_ERROR_ABORT);
  }

  do_check_true(download.stopped);
  do_check_true(download.canceled);
  do_check_true(download.error === null);

  do_check_false(yield OS.File.exists(download.target.path));

  // Progress properties are not reset by canceling.
  do_check_eq(download.progress, 50);
  do_check_eq(download.totalBytes, TEST_DATA_SHORT.length * 2);
  do_check_eq(download.currentBytes, TEST_DATA_SHORT.length);

  if (!gUseLegacySaver) {
    // The promise returned by "start" should have been rejected meanwhile.
    try {
      yield promiseAttempt;
      do_throw("The download should have been canceled.");
    } catch (ex if ex instanceof Downloads.Error) {
      do_check_false(ex.becauseSourceFailed);
      do_check_false(ex.becauseTargetFailed);
    }
  }
});

/**
 * Cancels a download right after starting it.
 */
add_task(function test_cancel_immediately()
{
  mustInterruptResponses();

  let download = yield promiseStartDownload(httpUrl("interruptible.txt"));

  let promiseAttempt = download.start();
  do_check_false(download.stopped);

  let promiseCancel = download.cancel();
  do_check_true(download.canceled);

  // At this point, we don't know whether the download has already stopped or
  // is still waiting for cancellation.  We can wait on the promise returned
  // by the "start" method to know for sure.
  try {
    yield promiseAttempt;
    do_throw("The download should have been canceled.");
  } catch (ex if ex instanceof Downloads.Error) {
    do_check_false(ex.becauseSourceFailed);
    do_check_false(ex.becauseTargetFailed);
  }

  do_check_true(download.stopped);
  do_check_true(download.canceled);
  do_check_true(download.error === null);

  do_check_false(yield OS.File.exists(download.target.path));

  // Check that the promise returned by the "cancel" method has been resolved.
  yield promiseCancel;
});

/**
 * Cancels and restarts a download sequentially.
 */
add_task(function test_cancel_midway_restart()
{
  mustInterruptResponses();

  let download = yield promiseStartDownload(httpUrl("interruptible.txt"));

  // The first time, cancel the download midway.
  yield promiseDownloadMidway(download);
  yield download.cancel();

  do_check_true(download.stopped);

  // The second time, we'll provide the entire interruptible response.
  continueResponses();
  download.onchange = null;
  let promiseAttempt = download.start();

  // Download state should have already been reset.
  do_check_false(download.stopped);
  do_check_false(download.canceled);
  do_check_true(download.error === null);

  // For the following test, we rely on the network layer reporting its progress
  // asynchronously.  Otherwise, there is nothing stopping the restarted
  // download from reaching the same progress as the first request already.
  do_check_eq(download.progress, 0);
  do_check_eq(download.totalBytes, 0);
  do_check_eq(download.currentBytes, 0);

  yield promiseAttempt;

  do_check_true(download.stopped);
  do_check_true(download.succeeded);
  do_check_false(download.canceled);
  do_check_true(download.error === null);

  yield promiseVerifyContents(download.target.path,
                              TEST_DATA_SHORT + TEST_DATA_SHORT);
});

/**
 * Cancels a download and restarts it from where it stopped.
 */
add_task(function test_cancel_midway_restart_tryToKeepPartialData()
{
  let download = yield promiseStartDownload_tryToKeepPartialData();
  yield download.cancel();

  // This time-based solution is a workaround to avoid intermittent failures,
  // and will be removed when bug 899102 is resolved.
  if (gUseLegacySaver) {
    yield promiseTimeout(250);
  }

  do_check_true(download.stopped);
  do_check_true(download.hasPartialData);

  // The target file should not exist, but we should have kept the partial data.
  do_check_false(yield OS.File.exists(download.target.path));
  yield promiseVerifyContents(download.target.partFilePath, TEST_DATA_SHORT);

  // Verify that the server sent the response from the start.
  do_check_eq(gMostRecentFirstBytePos, 0);

  // The second time, we'll request and obtain the second part of the response,
  // but we still stop when half of the remaining progress is reached.
  let deferMidway = Promise.defer();
  download.onchange = function () {
    if (!download.stopped && !download.canceled &&
        download.currentBytes == Math.floor(TEST_DATA_SHORT.length * 3 / 2)) {
      download.onchange = null;
      deferMidway.resolve();
    }
  };

  mustInterruptResponses();
  let promiseAttempt = download.start();

  // Continue when the number of bytes we received is correct, then check that
  // progress is at about 75 percent.  The exact figure may vary because of
  // rounding issues, since the total number of bytes in the response might not
  // be a multiple of four.
  yield deferMidway.promise;
  do_check_true(download.progress > 72 && download.progress < 78);

  // Now we allow the download to finish.
  continueResponses();
  yield promiseAttempt;

  // Check that the server now sent the second part only.
  do_check_eq(gMostRecentFirstBytePos, TEST_DATA_SHORT.length);

  // The target file should now have been created, and the ".part" file deleted.
  yield promiseVerifyContents(download.target.path,
                              TEST_DATA_SHORT + TEST_DATA_SHORT);
  do_check_false(yield OS.File.exists(download.target.partFilePath));
});

/**
 * Cancels a download while keeping partially downloaded data, then removes the
 * data and restarts the download from the beginning.
 */
add_task(function test_cancel_midway_restart_removePartialData()
{
  let download = yield promiseStartDownload_tryToKeepPartialData();
  yield download.cancel();

  // This time-based solution is a workaround to avoid intermittent failures,
  // and will be removed when bug 899102 is resolved.
  if (gUseLegacySaver) {
    yield promiseTimeout(250);
  }

  do_check_true(download.hasPartialData);
  yield promiseVerifyContents(download.target.partFilePath, TEST_DATA_SHORT);

  yield download.removePartialData();

  do_check_false(download.hasPartialData);
  do_check_false(yield OS.File.exists(download.target.partFilePath));

  // The second time, we'll request and obtain the entire response again.
  continueResponses();
  yield download.start();

  // Verify that the server sent the response from the start.
  do_check_eq(gMostRecentFirstBytePos, 0);

  // The target file should now have been created, and the ".part" file deleted.
  yield promiseVerifyContents(download.target.path,
                              TEST_DATA_SHORT + TEST_DATA_SHORT);
  do_check_false(yield OS.File.exists(download.target.partFilePath));
});

/**
 * Cancels a download while keeping partially downloaded data, then removes the
 * data and restarts the download from the beginning without keeping the partial
 * data anymore.
 */
add_task(function test_cancel_midway_restart_tryToKeepPartialData_false()
{
  let download = yield promiseStartDownload_tryToKeepPartialData();
  yield download.cancel();

  // This time-based solution is a workaround to avoid intermittent failures,
  // and will be removed when bug 899102 is resolved.
  if (gUseLegacySaver) {
    yield promiseTimeout(250);
  }

  download.tryToKeepPartialData = false;

  // The above property change does not affect existing partial data.
  do_check_true(download.hasPartialData);
  yield promiseVerifyContents(download.target.partFilePath, TEST_DATA_SHORT);

  yield download.removePartialData();
  do_check_false(yield OS.File.exists(download.target.partFilePath));

  // Restart the download from the beginning.
  mustInterruptResponses();
  download.start();

  yield promiseDownloadMidway(download);
  yield promisePartFileReady(download);

  // While the download is in progress, we should still have a ".part" file.
  do_check_false(download.hasPartialData);
  do_check_true(yield OS.File.exists(download.target.partFilePath));

  yield download.cancel();

  // This time-based solution is a workaround to avoid intermittent failures,
  // and will be removed when bug 899102 is resolved.
  if (gUseLegacySaver) {
    yield promiseTimeout(250);
  }

  // The ".part" file should be deleted now that the download is canceled.
  do_check_false(download.hasPartialData);
  do_check_false(yield OS.File.exists(download.target.partFilePath));

  // The third time, we'll request and obtain the entire response again.
  continueResponses();
  yield download.start();

  // Verify that the server sent the response from the start.
  do_check_eq(gMostRecentFirstBytePos, 0);

  // The target file should now have been created, and the ".part" file deleted.
  yield promiseVerifyContents(download.target.path,
                              TEST_DATA_SHORT + TEST_DATA_SHORT);
  do_check_false(yield OS.File.exists(download.target.partFilePath));
});

/**
 * Cancels a download right after starting it, then restarts it immediately.
 */
add_task(function test_cancel_immediately_restart_immediately()
{
  mustInterruptResponses();

  let download = yield promiseStartDownload(httpUrl("interruptible.txt"));
  let promiseAttempt = download.start();

  do_check_false(download.stopped);

  download.cancel();
  do_check_true(download.canceled);

  let promiseRestarted = download.start();
  do_check_false(download.stopped);
  do_check_false(download.canceled);
  do_check_true(download.error === null);

  // For the following test, we rely on the network layer reporting its progress
  // asynchronously.  Otherwise, there is nothing stopping the restarted
  // download from reaching the same progress as the first request already.
  do_check_eq(download.hasProgress, false);
  do_check_eq(download.progress, 0);
  do_check_eq(download.totalBytes, 0);
  do_check_eq(download.currentBytes, 0);

  // Ensure the next request is now allowed to complete, regardless of whether
  // the canceled request was received by the server or not.
  continueResponses();
  try {
    yield promiseAttempt;
    do_throw("The download should have been canceled.");
  } catch (ex if ex instanceof Downloads.Error) {
    do_check_false(ex.becauseSourceFailed);
    do_check_false(ex.becauseTargetFailed);
  }

  yield promiseRestarted;

  do_check_true(download.stopped);
  do_check_true(download.succeeded);
  do_check_false(download.canceled);
  do_check_true(download.error === null);

  yield promiseVerifyContents(download.target.path,
                              TEST_DATA_SHORT + TEST_DATA_SHORT);
});

/**
 * Cancels a download midway, then restarts it immediately.
 */
add_task(function test_cancel_midway_restart_immediately()
{
  mustInterruptResponses();

  let download = yield promiseStartDownload(httpUrl("interruptible.txt"));
  let promiseAttempt = download.start();

  // The first time, cancel the download midway.
  yield promiseDownloadMidway(download);
  download.cancel();
  do_check_true(download.canceled);

  let promiseRestarted = download.start();
  do_check_false(download.stopped);
  do_check_false(download.canceled);
  do_check_true(download.error === null);

  // For the following test, we rely on the network layer reporting its progress
  // asynchronously.  Otherwise, there is nothing stopping the restarted
  // download from reaching the same progress as the first request already.
  do_check_eq(download.hasProgress, false);
  do_check_eq(download.progress, 0);
  do_check_eq(download.totalBytes, 0);
  do_check_eq(download.currentBytes, 0);

  // The second request is allowed to complete.
  continueResponses();
  try {
    yield promiseAttempt;
    do_throw("The download should have been canceled.");
  } catch (ex if ex instanceof Downloads.Error) {
    do_check_false(ex.becauseSourceFailed);
    do_check_false(ex.becauseTargetFailed);
  }

  yield promiseRestarted;

  do_check_true(download.stopped);
  do_check_true(download.succeeded);
  do_check_false(download.canceled);
  do_check_true(download.error === null);

  yield promiseVerifyContents(download.target.path,
                              TEST_DATA_SHORT + TEST_DATA_SHORT);
});

/**
 * Calls the "cancel" method on a successful download.
 */
add_task(function test_cancel_successful()
{
  let download = yield promiseStartDownload();
  yield promiseDownloadStopped(download);

  // The cancel method should succeed with no effect.
  yield download.cancel();

  do_check_true(download.stopped);
  do_check_true(download.succeeded);
  do_check_false(download.canceled);
  do_check_true(download.error === null);

  yield promiseVerifyContents(download.target.path, TEST_DATA_SHORT);
});

/**
 * Calls the "cancel" method two times in a row.
 */
add_task(function test_cancel_twice()
{
  mustInterruptResponses();

  let download = yield promiseStartDownload(httpUrl("interruptible.txt"));

  let promiseAttempt = download.start();
  do_check_false(download.stopped);

  let promiseCancel1 = download.cancel();
  do_check_true(download.canceled);
  let promiseCancel2 = download.cancel();

  try {
    yield promiseAttempt;
    do_throw("The download should have been canceled.");
  } catch (ex if ex instanceof Downloads.Error) {
    do_check_false(ex.becauseSourceFailed);
    do_check_false(ex.becauseTargetFailed);
  }

  // Both promises should now be resolved.
  yield promiseCancel1;
  yield promiseCancel2;

  do_check_true(download.stopped);
  do_check_false(download.succeeded);
  do_check_true(download.canceled);
  do_check_true(download.error === null);

  do_check_false(yield OS.File.exists(download.target.path));
});

/**
 * Checks that a download cannot be restarted after the "finalize" method.
 */
add_task(function test_finalize()
{
  mustInterruptResponses();

  let download = yield promiseStartDownload(httpUrl("interruptible.txt"));

  let promiseFinalized = download.finalize();

  try {
    yield download.start();
    do_throw("It should not be possible to restart after finalization.");
  } catch (ex) { }

  yield promiseFinalized;

  do_check_true(download.stopped);
  do_check_false(download.succeeded);
  do_check_true(download.canceled);
  do_check_true(download.error === null);

  do_check_false(yield OS.File.exists(download.target.path));
});

/**
 * Checks that the "finalize" method can remove partially downloaded data.
 */
add_task(function test_finalize_tryToKeepPartialData()
{
  // Check finalization without removing partial data.
  let download = yield promiseStartDownload_tryToKeepPartialData();
  yield download.finalize();

  // This time-based solution is a workaround to avoid intermittent failures,
  // and will be removed when bug 899102 is resolved.
  if (gUseLegacySaver) {
    yield promiseTimeout(250);
  }

  do_check_true(download.hasPartialData);
  do_check_true(yield OS.File.exists(download.target.partFilePath));

  // Clean up.
  yield download.removePartialData();

  // Check finalization while removing partial data.
  download = yield promiseStartDownload_tryToKeepPartialData();
  yield download.finalize(true);

  // This time-based solution is a workaround to avoid intermittent failures,
  // and will be removed when bug 899102 is resolved.
  if (gUseLegacySaver) {
    yield promiseTimeout(250);
  }

  do_check_false(download.hasPartialData);
  do_check_false(yield OS.File.exists(download.target.partFilePath));
});

/**
 * Checks that whenSucceeded returns a promise that is resolved after a restart.
 */
add_task(function test_whenSucceeded_after_restart()
{
  mustInterruptResponses();

  let promiseSucceeded;

  let download;
  if (!gUseLegacySaver) {
    // When testing DownloadCopySaver, we have control over the download, thus
    // we can verify getting a reference before the first download attempt.
    download = yield promiseNewDownload(httpUrl("interruptible.txt"));
    promiseSucceeded = download.whenSucceeded();
    download.start();
  } else {
    // When testing DownloadLegacySaver, the download is already started when it
    // is created, thus we cannot get the reference before the first attempt.
    download = yield promiseStartLegacyDownload(httpUrl("interruptible.txt"));
    promiseSucceeded = download.whenSucceeded();
  }

  // Cancel the first download attempt.
  yield download.cancel();

  // The second request is allowed to complete.
  continueResponses();
  download.start();

  // Wait for the download to finish by waiting on the whenSucceeded promise.
  yield promiseSucceeded;

  do_check_true(download.stopped);
  do_check_true(download.succeeded);
  do_check_false(download.canceled);
  do_check_true(download.error === null);

  yield promiseVerifyContents(download.target.path,
                              TEST_DATA_SHORT + TEST_DATA_SHORT);
});

/**
 * Ensures download error details are reported on network failures.
 */
add_task(function test_error_source()
{
  let serverSocket = startFakeServer();
  try {
    let sourceUrl = "http://localhost:" + serverSocket.port + "/source.txt";

    let download;
    try {
      if (!gUseLegacySaver) {
        // When testing DownloadCopySaver, we want to check that the promise
        // returned by the "start" method is rejected.
        download = yield promiseNewDownload(sourceUrl);

        do_check_true(download.error === null);

        yield download.start();
      } else {
        // When testing DownloadLegacySaver, we cannot be sure whether we are
        // testing the promise returned by the "start" method or we are testing
        // the "error" property checked by promiseDownloadStopped.  This happens
        // because we don't have control over when the download is started.
        download = yield promiseStartLegacyDownload(sourceUrl);
        yield promiseDownloadStopped(download);
      }
      do_throw("The download should have failed.");
    } catch (ex if ex instanceof Downloads.Error && ex.becauseSourceFailed) {
      // A specific error object is thrown when reading from the source fails.
    }

    // Check the properties now that the download stopped.
    do_check_true(download.stopped);
    do_check_false(download.canceled);
    do_check_true(download.error !== null);
    do_check_true(download.error.becauseSourceFailed);
    do_check_false(download.error.becauseTargetFailed);
  } finally {
    serverSocket.close();
  }
});

/**
 * Ensures download error details are reported on local writing failures.
 */
add_task(function test_error_target()
{
  // Create a file without write access permissions before downloading.
  let targetFile = getTempFile(TEST_TARGET_FILE_NAME);
  targetFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0);
  try {
    let download;
    try {
      if (!gUseLegacySaver) {
        // When testing DownloadCopySaver, we want to check that the promise
        // returned by the "start" method is rejected.
        download = yield Downloads.createDownload({
          source: httpUrl("source.txt"),
          target: targetFile,
        });
        yield download.start();
      } else {
        // When testing DownloadLegacySaver, we cannot be sure whether we are
        // testing the promise returned by the "start" method or we are testing
        // the "error" property checked by promiseDownloadStopped.  This happens
        // because we don't have control over when the download is started.
        download = yield promiseStartLegacyDownload(null,
                                                    { targetFile: targetFile });
        yield promiseDownloadStopped(download);
      }
      do_throw("The download should have failed.");
    } catch (ex if ex instanceof Downloads.Error && ex.becauseTargetFailed) {
      // A specific error object is thrown when writing to the target fails.
    }

    // Check the properties now that the download stopped.
    do_check_true(download.stopped);
    do_check_false(download.canceled);
    do_check_true(download.error !== null);
    do_check_true(download.error.becauseTargetFailed);
    do_check_false(download.error.becauseSourceFailed);
  } finally {
    // Restore the default permissions to allow deleting the file on Windows.
    if (targetFile.exists()) {
      targetFile.permissions = FileUtils.PERMS_FILE;
      targetFile.remove(false);
    }
  }
});

/**
 * Restarts a failed download.
 */
add_task(function test_error_restart()
{
  let download;

  // Create a file without write access permissions before downloading.
  let targetFile = getTempFile(TEST_TARGET_FILE_NAME);
  targetFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0);
  try {
    // Use DownloadCopySaver or DownloadLegacySaver based on the test run,
    // specifying the target file we created.
    if (!gUseLegacySaver) {
      download = yield Downloads.createDownload({
        source: httpUrl("source.txt"),
        target: targetFile,
      });
      download.start();
    } else {
      download = yield promiseStartLegacyDownload(null,
                                                  { targetFile: targetFile });
    }
    yield promiseDownloadStopped(download);
    do_throw("The download should have failed.");
  } catch (ex if ex instanceof Downloads.Error && ex.becauseTargetFailed) {
    // A specific error object is thrown when writing to the target fails.
  } finally {
    // Restore the default permissions to allow deleting the file on Windows.
    if (targetFile.exists()) {
      targetFile.permissions = FileUtils.PERMS_FILE;

      // Also for Windows, rename the file before deleting.  This makes the
      // current file name available immediately for a new file, while deleting
      // in place prevents creation of a file with the same name for some time.
      targetFile.moveTo(null, targetFile.leafName + ".delete.tmp");
      targetFile.remove(false);
    }
  }

  // Restart the download and wait for completion.
  yield download.start();

  do_check_true(download.stopped);
  do_check_true(download.succeeded);
  do_check_false(download.canceled);
  do_check_true(download.error === null);
  do_check_eq(download.progress, 100);

  yield promiseVerifyContents(download.target.path, TEST_DATA_SHORT);
});

/**
 * Executes download in both public and private modes.
 */
add_task(function test_public_and_private()
{
  let sourcePath = "/test_public_and_private.txt";
  let sourceUrl = httpUrl("test_public_and_private.txt");
  let testCount = 0;

  // Apply pref to allow all cookies.
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);

  function cleanup() {
    Services.prefs.clearUserPref("network.cookie.cookieBehavior");
    Services.cookies.removeAll();
    gHttpServer.registerPathHandler(sourcePath, null);
  }
  do_register_cleanup(cleanup);

  gHttpServer.registerPathHandler(sourcePath, function (aRequest, aResponse) {
    aResponse.setHeader("Content-Type", "text/plain", false);

    if (testCount == 0) {
      // No cookies should exist for first public download.
      do_check_false(aRequest.hasHeader("Cookie"));
      aResponse.setHeader("Set-Cookie", "foobar=1", false);
      testCount++;
    } else if (testCount == 1) {
      // The cookie should exists for second public download.
      do_check_true(aRequest.hasHeader("Cookie"));
      do_check_eq(aRequest.getHeader("Cookie"), "foobar=1");
      testCount++;
    } else if (testCount == 2)  {
      // No cookies should exist for first private download.
      do_check_false(aRequest.hasHeader("Cookie"));
    }
  });

  let targetFile = getTempFile(TEST_TARGET_FILE_NAME);
  yield Downloads.simpleDownload(sourceUrl, targetFile);
  yield Downloads.simpleDownload(sourceUrl, targetFile);

  if (!gUseLegacySaver) {
    let download = yield Downloads.createDownload({
      source: { url: sourceUrl, isPrivate: true },
      target: targetFile,
    });
    yield download.start();
  } else {
    let download = yield promiseStartLegacyDownload(sourceUrl,
                                                    { isPrivate: true });
    yield promiseDownloadStopped(download);
  }

  cleanup();
});

/**
 * Checks the startTime gets updated even after a restart.
 */
add_task(function test_cancel_immediately_restart_and_check_startTime()
{
  let download = yield promiseStartDownload();

  let startTime = download.startTime;
  do_check_true(isValidDate(download.startTime));

  yield download.cancel();
  do_check_eq(download.startTime.getTime(), startTime.getTime());

  // Wait for a timeout.
  yield promiseTimeout(10);

  yield download.start();
  do_check_true(download.startTime.getTime() > startTime.getTime());
});

/**
 * Executes download with content-encoding.
 */
add_task(function test_with_content_encoding()
{
  let sourcePath = "/test_with_content_encoding.txt";
  let sourceUrl = httpUrl("test_with_content_encoding.txt");

  function cleanup() {
    gHttpServer.registerPathHandler(sourcePath, null);
  }
  do_register_cleanup(cleanup);

  gHttpServer.registerPathHandler(sourcePath, function (aRequest, aResponse) {
    aResponse.setHeader("Content-Type", "text/plain", false);
    aResponse.setHeader("Content-Encoding", "gzip", false);
    aResponse.setHeader("Content-Length",
                        "" + TEST_DATA_SHORT_GZIP_ENCODED.length, false);

    let bos = new BinaryOutputStream(aResponse.bodyOutputStream);
    bos.writeByteArray(TEST_DATA_SHORT_GZIP_ENCODED,
                       TEST_DATA_SHORT_GZIP_ENCODED.length);
  });

  let download = yield promiseStartDownload(sourceUrl);
  yield promiseDownloadStopped(download);

  do_check_eq(download.progress, 100);
  do_check_eq(download.totalBytes, TEST_DATA_SHORT_GZIP_ENCODED.length);

  // Ensure the content matches the decoded test data.
  yield promiseVerifyContents(download.target.path, TEST_DATA_SHORT);

  cleanup();
});

/**
 * Checks that the file is not decoded if the extension matches the encoding.
 */
add_task(function test_with_content_encoding_ignore_extension()
{
  let sourcePath = "/test_with_content_encoding_ignore_extension.gz";
  let sourceUrl = httpUrl("test_with_content_encoding_ignore_extension.gz");

  function cleanup() {
    gHttpServer.registerPathHandler(sourcePath, null);
  }
  do_register_cleanup(cleanup);

  gHttpServer.registerPathHandler(sourcePath, function (aRequest, aResponse) {
    aResponse.setHeader("Content-Type", "text/plain", false);
    aResponse.setHeader("Content-Encoding", "gzip", false);
    aResponse.setHeader("Content-Length",
                        "" + TEST_DATA_SHORT_GZIP_ENCODED.length, false);

    let bos = new BinaryOutputStream(aResponse.bodyOutputStream);
    bos.writeByteArray(TEST_DATA_SHORT_GZIP_ENCODED,
                       TEST_DATA_SHORT_GZIP_ENCODED.length);
  });

  let download = yield promiseStartDownload(sourceUrl);
  yield promiseDownloadStopped(download);

  do_check_eq(download.progress, 100);
  do_check_eq(download.totalBytes, TEST_DATA_SHORT_GZIP_ENCODED.length);

  // Ensure the content matches the encoded test data.  We convert the data to a
  // string before executing the content check.
  yield promiseVerifyContents(download.target.path,
        String.fromCharCode.apply(String, TEST_DATA_SHORT_GZIP_ENCODED));

  cleanup();
});

/**
 * Cancels and restarts a download sequentially with content-encoding.
 */
add_task(function test_cancel_midway_restart_with_content_encoding()
{
  mustInterruptResponses();

  let download = yield promiseStartDownload(httpUrl("interruptible_gzip.txt"));

  // The first time, cancel the download midway.
  let deferCancel = Promise.defer();
  let onchange = function () {
    if (!download.stopped && !download.canceled &&
        download.currentBytes == TEST_DATA_SHORT_GZIP_ENCODED_FIRST.length) {
      deferCancel.resolve(download.cancel());
    }
  };

  // Register for the notification, but also call the function directly in
  // case the download already reached the expected progress.
  download.onchange = onchange;
  onchange();

  yield deferCancel.promise;

  do_check_true(download.stopped);

  // The second time, we'll provide the entire interruptible response.
  continueResponses();
  download.onchange = null;
  yield download.start();

  do_check_eq(download.progress, 100);
  do_check_eq(download.totalBytes, TEST_DATA_SHORT_GZIP_ENCODED.length);

  yield promiseVerifyContents(download.target.path, TEST_DATA_SHORT);
});

/**
 * Download with parental controls enabled.
 */
add_task(function test_blocked_parental_controls()
{
  function cleanup() {
    DownloadIntegration.shouldBlockInTest = false;
  }
  do_register_cleanup(cleanup);
  DownloadIntegration.shouldBlockInTest = true;

  let download;
  try {
    if (!gUseLegacySaver) {
      // When testing DownloadCopySaver, we want to check that the promise
      // returned by the "start" method is rejected.
      download = yield promiseNewDownload();
      yield download.start();
    } else {
      // When testing DownloadLegacySaver, we cannot be sure whether we are
      // testing the promise returned by the "start" method or we are testing
      // the "error" property checked by promiseDownloadStopped.  This happens
      // because we don't have control over when the download is started.
      download = yield promiseStartLegacyDownload();
      yield promiseDownloadStopped(download);
    }
    do_throw("The download should have blocked.");
  } catch (ex if ex instanceof Downloads.Error && ex.becauseBlocked) {
    do_check_true(ex.becauseBlockedByParentalControls);
  }

  // Now that the download stopped, the target file should not exist.
  do_check_false(yield OS.File.exists(download.target.path));

  cleanup();
});

/**
 * download.showContainingDirectory() action
 */
add_task(function test_showContainingDirectory() {
  DownloadIntegration._deferTestShowDir = Promise.defer();

  let targetPath = getTempFile(TEST_TARGET_FILE_NAME).path;

  let download = yield Downloads.createDownload({
    source: { url: httpUrl("source.txt") },
    target: ""
  });

  try {
    yield download.showContainingDirectory();
    do_throw("Should have failed because of an invalid path.");
  } catch (ex if ex instanceof Components.Exception) {
    // Invalid paths on Windows are reported with NS_ERROR_FAILURE,
    // but with NS_ERROR_FILE_UNRECOGNIZED_PATH on Mac/Linux
    let validResult = ex.result == Cr.NS_ERROR_FILE_UNRECOGNIZED_PATH ||
                      ex.result == Cr.NS_ERROR_FAILURE;
    do_check_true(validResult);
  }

  download = yield Downloads.createDownload({
    source: { url: httpUrl("source.txt") },
    target: targetPath
  });


  DownloadIntegration._deferTestShowDir = Promise.defer();
  download.showContainingDirectory();
  let result = yield DownloadIntegration._deferTestShowDir.promise;
  do_check_eq(result, "success");
});

/**
 * download.launch() action
 */
add_task(function test_launch() {
  let customLauncher = getTempFile("app-launcher");

  // Test both with and without setting a custom application.
  for (let launcherPath of [null, customLauncher.path]) {
    let download;
    if (!gUseLegacySaver) {
      // When testing DownloadCopySaver, we have control over the download, thus
      // we can test that file is not launched if download.succeeded is not set.
      download = yield Downloads.createDownload({
        source: httpUrl("source.txt"),
        target: getTempFile(TEST_TARGET_FILE_NAME).path,
        launcherPath: launcherPath,
      });

      try {
        yield download.launch();
        do_throw("Can't launch download file as it has not completed yet");
      } catch (ex) {
        do_check_eq(ex.message,
                    "launch can only be called if the download succeeded");
      }

      yield download.start();
    } else {
      // When testing DownloadLegacySaver, the download is already started when
      // it is created, thus we don't test calling "launch" before starting.
      download = yield promiseStartLegacyDownload(
                                         httpUrl("source.txt"),
                                         { launcherPath: launcherPath });
      yield promiseDownloadStopped(download);
    }

    do_check_false(download.launchWhenSucceeded);

    DownloadIntegration._deferTestOpenFile = Promise.defer();
    download.launch();
    let result = yield DownloadIntegration._deferTestOpenFile.promise;

    // Verify that the results match the test case.
    if (!launcherPath) {
      // This indicates that the default handler has been chosen.
      do_check_true(result === null);
    } else {
      // Check the nsIMIMEInfo instance that would have been used for launching.
      do_check_eq(result.preferredAction, Ci.nsIMIMEInfo.useHelperApp);
      do_check_true(result.preferredApplicationHandler
                          .QueryInterface(Ci.nsILocalHandlerApp)
                          .executable.equals(customLauncher));
    }
  }
});

/**
 * Test passing an invalid path as the launcherPath property.
 */
add_task(function test_launcherPath_invalid() {
  let download = yield Downloads.createDownload({
    source: { url: httpUrl("source.txt") },
    target: { path: getTempFile(TEST_TARGET_FILE_NAME).path },
    launcherPath: " "
  });

  DownloadIntegration._deferTestOpenFile = Promise.defer();
  yield download.start();
  try {
    download.launch();
    result = yield DownloadIntegration._deferTestOpenFile.promise;
    do_throw("Can't launch file with invalid custom launcher")
  } catch (ex if ex instanceof Components.Exception) {
    // Invalid paths on Windows are reported with NS_ERROR_FAILURE,
    // but with NS_ERROR_FILE_UNRECOGNIZED_PATH on Mac/Linux
    let validResult = ex.result == Cr.NS_ERROR_FILE_UNRECOGNIZED_PATH ||
                      ex.result == Cr.NS_ERROR_FAILURE;
    do_check_true(validResult);
  }
});

/**
 * Tests that download.launch() is automatically called after
 * the download finishes if download.launchWhenSucceeded = true
 */
add_task(function test_launchWhenSucceeded() {
  let customLauncher = getTempFile("app-launcher");

  // Test both with and without setting a custom application.
  for (let launcherPath of [null, customLauncher.path]) {
    DownloadIntegration._deferTestOpenFile = Promise.defer();

    if (!gUseLegacySaver) {
      let download = yield Downloads.createDownload({
        source: httpUrl("source.txt"),
        target: getTempFile(TEST_TARGET_FILE_NAME).path,
        launchWhenSucceeded: true,
        launcherPath: launcherPath,
      });
      yield download.start();
    } else {
      let download = yield promiseStartLegacyDownload(
                                             httpUrl("source.txt"),
                                             { launcherPath: launcherPath,
                                               launchWhenSucceeded: true });
      yield promiseDownloadStopped(download);
    }

    let result = yield DownloadIntegration._deferTestOpenFile.promise;

    // Verify that the results match the test case.
    if (!launcherPath) {
      // This indicates that the default handler has been chosen.
      do_check_true(result === null);
    } else {
      // Check the nsIMIMEInfo instance that would have been used for launching.
      do_check_eq(result.preferredAction, Ci.nsIMIMEInfo.useHelperApp);
      do_check_true(result.preferredApplicationHandler
                          .QueryInterface(Ci.nsILocalHandlerApp)
                          .executable.equals(customLauncher));
    }
  }
});

/**
 * Tests that the proper content type is set for a normal download.
 */
add_task(function test_contentType() {
  let download = yield promiseStartDownload(httpUrl("source.txt"));
  yield promiseDownloadStopped(download);

  do_check_eq("text/plain", download.contentType);
});

/**
 * Tests that the serialization/deserialization of the startTime Date
 * object works correctly.
 */
add_task(function test_toSerializable_startTime()
{
  let download1 = yield promiseStartDownload(httpUrl("source.txt"));
  yield promiseDownloadStopped(download1);

  let serializable = download1.toSerializable();
  let reserialized = JSON.parse(JSON.stringify(serializable));

  let download2 = yield Downloads.createDownload(reserialized);

  do_check_eq(download1.startTime.constructor.name, "Date");
  do_check_eq(download2.startTime.constructor.name, "Date");
  do_check_eq(download1.startTime.toJSON(), download2.startTime.toJSON());
});

/**
 * This test will call the platform specific operations within
 * DownloadPlatform::DownloadDone. While there is no test to verify the
 * specific behaviours, this at least ensures that there is no error or crash.
 */
add_task(function test_platform_integration()
{
  let downloadFiles = [];
  function cleanup() {
    for (let file of downloadFiles) {
      file.remove(true);
    }
  }
  do_register_cleanup(cleanup);

  for (let isPrivate of [false, true]) {
    DownloadIntegration.downloadDoneCalled = false;

    // Some platform specific operations only operate on files outside the
    // temporary directory or in the Downloads directory (such as setting
    // the Windows searchable attribute, and the Mac Downloads icon bouncing),
    // so use the system Downloads directory for the target file.
    let targetFile = yield DownloadIntegration.getSystemDownloadsDirectory();
    targetFile = targetFile.clone();
    targetFile.append("test" + (Math.floor(Math.random() * 1000000)));
    downloadFiles.push(targetFile);

    let download;
    if (gUseLegacySaver) {
      download = yield promiseStartLegacyDownload(httpUrl("source.txt"),
                                                  { targetFile: targetFile });
    }
    else {
      download = yield Downloads.createDownload({
        source: httpUrl("source.txt"),
        target: targetFile,
      });
      download.start();
    }

    // Wait for the whenSucceeded promise to be resolved first.
    // downloadDone should be called before the whenSucceeded promise is resolved.
    yield download.whenSucceeded().then(function () {
      do_check_true(DownloadIntegration.downloadDoneCalled);
    });

    // Then, wait for the promise returned by "start" to be resolved.
    yield promiseDownloadStopped(download);

    yield promiseVerifyContents(download.target.path, TEST_DATA_SHORT);
  }
});

/**
 * Checks that downloads are added to browsing history when they start.
 */
add_task(function test_history()
{
  mustInterruptResponses();

  // We will wait for the visit to be notified during the download.
  yield promiseClearHistory();
  let promiseVisit = promiseWaitForVisit(httpUrl("interruptible.txt"));

  // Start a download that is not allowed to finish yet.
  let download = yield promiseStartDownload(httpUrl("interruptible.txt"));

  // The history notifications should be received before the download completes.
  let [time, transitionType] = yield promiseVisit;
  do_check_eq(time, download.startTime.getTime() * 1000);
  do_check_eq(transitionType, Ci.nsINavHistoryService.TRANSITION_DOWNLOAD);

  // Restart and complete the download after clearing history.
  yield promiseClearHistory();
  download.cancel();
  continueResponses();
  yield download.start();

  // The restart should not have added a new history visit.
  do_check_false(yield promiseIsURIVisited(httpUrl("interruptible.txt")));
});

/**
 * Checks that downloads started by nsIHelperAppService are added to the
 * browsing history when they start.
 */
add_task(function test_history_tryToKeepPartialData()
{
  // We will wait for the visit to be notified during the download.
  yield promiseClearHistory();
  let promiseVisit =
      promiseWaitForVisit(httpUrl("interruptible_resumable.txt"));

  // Start a download that is not allowed to finish yet.
  let beforeStartTimeMs = Date.now();
  let download = yield promiseStartDownload_tryToKeepPartialData();

  // The history notifications should be received before the download completes.
  let [time, transitionType] = yield promiseVisit;
  do_check_eq(transitionType, Ci.nsINavHistoryService.TRANSITION_DOWNLOAD);

  // The time set by nsIHelperAppService may be different than the start time in
  // the download object, thus we only check that it is a meaningful time.  Note
  // that we subtract one second from the earliest time to account for rounding.
  do_check_true(time >= beforeStartTimeMs * 1000 - 1000000);

  // Complete the download before finishing the test.
  continueResponses();
  yield promiseDownloadStopped(download);
});

/**
 * Tests that the temp download files are removed on exit and exiting private
 * mode after they have been launched.
 */
add_task(function test_launchWhenSucceeded_deleteTempFileOnExit() {
  const kDeleteTempFileOnExit = "browser.helperApps.deleteTempFileOnExit";

  let customLauncherPath = getTempFile("app-launcher").path;
  let autoDeleteTargetPathOne = getTempFile(TEST_TARGET_FILE_NAME).path;
  let autoDeleteTargetPathTwo = getTempFile(TEST_TARGET_FILE_NAME).path;
  let noAutoDeleteTargetPath = getTempFile(TEST_TARGET_FILE_NAME).path;

  let autoDeleteDownloadOne = yield Downloads.createDownload({
    source: { url: httpUrl("source.txt"), isPrivate: true },
    target: autoDeleteTargetPathOne,
    launchWhenSucceeded: true,
    launcherPath: customLauncherPath,
  });
  yield autoDeleteDownloadOne.start();

  Services.prefs.setBoolPref(kDeleteTempFileOnExit, true);
  let autoDeleteDownloadTwo = yield Downloads.createDownload({
    source: httpUrl("source.txt"),
    target: autoDeleteTargetPathTwo,
    launchWhenSucceeded: true,
    launcherPath: customLauncherPath,
  });
  yield autoDeleteDownloadTwo.start();

  Services.prefs.setBoolPref(kDeleteTempFileOnExit, false);
  let noAutoDeleteDownload = yield Downloads.createDownload({
    source: httpUrl("source.txt"),
    target: noAutoDeleteTargetPath,
    launchWhenSucceeded: true,
    launcherPath: customLauncherPath,
  });
  yield noAutoDeleteDownload.start();

  Services.prefs.clearUserPref(kDeleteTempFileOnExit);

  do_check_true(yield OS.File.exists(autoDeleteTargetPathOne));
  do_check_true(yield OS.File.exists(autoDeleteTargetPathTwo));
  do_check_true(yield OS.File.exists(noAutoDeleteTargetPath));

  // Simulate leaving private browsing mode
  Services.obs.notifyObservers(null, "last-pb-context-exited", null);
  do_check_false(yield OS.File.exists(autoDeleteTargetPathOne));

  // Simulate browser shutdown
  let expire = Cc["@mozilla.org/uriloader/external-helper-app-service;1"]
                 .getService(Ci.nsIObserver);
  expire.observe(null, "profile-before-change", null);
  do_check_false(yield OS.File.exists(autoDeleteTargetPathTwo));
  do_check_true(yield OS.File.exists(noAutoDeleteTargetPath));
});

