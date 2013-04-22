/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the integration with legacy interfaces for downloads.
 */

"use strict";

////////////////////////////////////////////////////////////////////////////////
//// Globals

/**
 * Starts a new download using the nsIWebBrowserPersist interface, and controls
 * it using the legacy nsITransfer interface.
 *
 * @param aSourceURI
 *        The nsIURI for the download source, or null to use TEST_SOURCE_URI.
 * @param aOutPersist
 *        Optional object that receives a reference to the created
 *        nsIWebBrowserPersist instance in the "value" property.
 *
 * @return {Promise}
 * @resolves The Download object created as a consequence of controlling the
 *           download through the legacy nsITransfer interface.
 * @rejects Never.  The current test fails in case of exceptions.
 */
function promiseStartLegacyDownload(aSourceURI, aOutPersist) {
  let sourceURI = aSourceURI || TEST_SOURCE_URI;
  let targetFile = getTempFile(TEST_TARGET_FILE_NAME);

  let persist = Cc["@mozilla.org/embedding/browser/nsWebBrowserPersist;1"]
                  .createInstance(Ci.nsIWebBrowserPersist);

  // We must create the nsITransfer implementation using its class ID because
  // the "@mozilla.org/transfer;1" contract is currently implemented in
  // "toolkit/components/downloads".  When the other folder is not included in
  // builds anymore (bug 851471), we'll be able to use the contract ID.
  let transfer =
      Components.classesByID["{1b4c85df-cbdd-4bb6-b04e-613caece083c}"]
                .createInstance(Ci.nsITransfer);

  if (aOutPersist) {
    aOutPersist.value = persist;
  }

  let deferred = Promise.defer();

  Downloads.getPublicDownloadList().then(function (aList) {
    // Temporarily register a view that will get notified when the download we
    // are controlling becomes visible in the list of public downloads.
    aList.addView({
      onDownloadAdded: function (aDownload) {
        aList.removeView(this);

        // Remove the download to keep the list empty for the next test.  This
        // also allows the caller to register the "onchange" event directly.
        aList.remove(aDownload);

        // When the download object is ready, make it available to the caller.
        deferred.resolve(aDownload);
      },
    });

    // Initialize the components so they reference each other.  This will cause
    // the Download object to be created and added to the public downloads.
    transfer.init(sourceURI, NetUtil.newURI(targetFile), null, null, null, null,
                  persist, false);
    persist.progressListener = transfer;

    // Start the actual download process.
    persist.saveURI(sourceURI, null, null, null, null, targetFile, null);
  }.bind(this)).then(null, do_report_unexpected_exception);

  return deferred.promise;
}

////////////////////////////////////////////////////////////////////////////////
//// Tests

/**
 * Executes a download controlled by the legacy nsITransfer interface.
 */
add_task(function test_basic()
{
  let tempDirectory = FileUtils.getDir("TmpD", []);

  let download = yield promiseStartLegacyDownload();

  // Checks the generated DownloadSource and DownloadTarget properties.
  do_check_true(download.source.uri.equals(TEST_SOURCE_URI));
  do_check_true(download.target.file.parent.equals(tempDirectory));

  // The download is already started, wait for completion and report any errors.
  if (!download.stopped) {
    yield download.start();
  }

  yield promiseVerifyContents(download.target.file, TEST_DATA_SHORT);
});

/**
 * Checks final state and progress for a successful download.
 */
add_task(function test_final_state()
{
  let download = yield promiseStartLegacyDownload();

  // The download is already started, wait for completion and report any errors.
  if (!download.stopped) {
    yield download.start();
  }

  do_check_true(download.stopped);
  do_check_true(download.succeeded);
  do_check_false(download.canceled);
  do_check_true(download.error === null);
  do_check_eq(download.progress, 100);
});

/**
 * Checks intermediate progress for a successful download.
 */
add_task(function test_intermediate_progress()
{
  let deferResponse = deferNextResponse();

  let download = yield promiseStartLegacyDownload(TEST_INTERRUPTIBLE_URI);

  let onchange = function () {
    if (download.progress == 50) {
      do_check_true(download.hasProgress);
      do_check_eq(download.currentBytes, TEST_DATA_SHORT.length);
      do_check_eq(download.totalBytes, TEST_DATA_SHORT.length * 2);

      // Continue after the first chunk of data is fully received.
      deferResponse.resolve();
    }
  };

  // Register for the notification, but also call the function directly in case
  // the download already reached the expected progress.
  download.onchange = onchange;
  onchange();

  // The download is already started, wait for completion and report any errors.
  if (!download.stopped) {
    yield download.start();
  }

  do_check_true(download.stopped);
  do_check_eq(download.progress, 100);

  yield promiseVerifyContents(download.target.file,
                              TEST_DATA_SHORT + TEST_DATA_SHORT);
});

/**
 * Downloads a file with a "Content-Length" of 0 and checks the progress.
 */
add_task(function test_empty_progress()
{
  let download = yield promiseStartLegacyDownload(TEST_EMPTY_URI);

  // The download is already started, wait for completion and report any errors.
  if (!download.stopped) {
    yield download.start();
  }

  do_check_true(download.stopped);
  do_check_true(download.hasProgress);
  do_check_eq(download.progress, 100);
  do_check_eq(download.currentBytes, 0);
  do_check_eq(download.totalBytes, 0);

  do_check_eq(download.target.file.fileSize, 0);
});

/**
 * Downloads an empty file with no "Content-Length" and checks the progress.
 */
add_task(function test_empty_noprogress()
{
  let deferResponse = deferNextResponse();
  let promiseEmptyRequestReceived = promiseNextRequestReceived();

  let download = yield promiseStartLegacyDownload(TEST_EMPTY_NOPROGRESS_URI);

  // Wait for the request to be received by the HTTP server, but don't allow the
  // request to finish yet.  Before checking the download state, wait for the
  // events to be processed by the client.
  yield promiseEmptyRequestReceived;
  yield promiseExecuteSoon();

  // Check that this download has no progress report.
  do_check_false(download.stopped);
  do_check_false(download.hasProgress);
  do_check_eq(download.currentBytes, 0);
  do_check_eq(download.totalBytes, 0);

  // Now allow the response to finish, and wait for the download to complete,
  // while reporting any errors that may occur.
  deferResponse.resolve();
  if (!download.stopped) {
    yield download.start();
  }

  // Verify the state of the completed download.
  do_check_true(download.stopped);
  do_check_false(download.hasProgress);
  do_check_eq(download.progress, 100);
  do_check_eq(download.currentBytes, 0);
  do_check_eq(download.totalBytes, 0);

  do_check_eq(download.target.file.fileSize, 0);
});

/**
 * Cancels a download and verifies that its state is reported correctly.
 */
add_task(function test_cancel_midway()
{
  let deferResponse = deferNextResponse();
  let outPersist = {};
  let download = yield promiseStartLegacyDownload(TEST_INTERRUPTIBLE_URI,
                                                  outPersist);

  try {
    // Cancel the download after receiving the first part of the response.
    let deferCancel = Promise.defer();
    let onchange = function () {
      if (!download.stopped && !download.canceled && download.progress == 50) {
        deferCancel.resolve(download.cancel());

        // The state change happens immediately after calling "cancel", but
        // temporary files or part files may still exist at this point.
        do_check_true(download.canceled);
      }
    };

    // Register for the notification, but also call the function directly in
    // case the download already reached the expected progress.
    download.onchange = onchange;
    onchange();

    // Wait on the promise returned by the "cancel" method to ensure that the
    // cancellation process finished and temporary files were removed.
    yield deferCancel.promise;

    // The nsIWebBrowserPersist instance should have been canceled now.
    do_check_eq(outPersist.value.result, Cr.NS_ERROR_ABORT);

    do_check_true(download.stopped);
    do_check_true(download.canceled);
    do_check_true(download.error === null);

    do_check_false(download.target.file.exists());

    // Progress properties are not reset by canceling.
    do_check_eq(download.progress, 50);
    do_check_eq(download.totalBytes, TEST_DATA_SHORT.length * 2);
    do_check_eq(download.currentBytes, TEST_DATA_SHORT.length);
  } finally {
    deferResponse.resolve();
  }
});

/**
 * Ensures download error details are reported for legacy downloads.
 */
add_task(function test_error()
{
  let serverSocket = startFakeServer();
  try {
    let download = yield promiseStartLegacyDownload(TEST_FAKE_SOURCE_URI);

    // We must check the download properties instead of calling the "start"
    // method because the download has been started and may already be stopped.
    let deferStopped = Promise.defer();
    let onchange = function () {
      if (download.stopped) {
        deferStopped.resolve();
      }
    };
    download.onchange = onchange;
    onchange();
    yield deferStopped.promise;

    // Check the properties now that the download stopped.
    do_check_false(download.canceled);
    do_check_true(download.error !== null);
    do_check_true(download.error.becauseSourceFailed);
    do_check_false(download.error.becauseTargetFailed);
  } finally {
    serverSocket.close();
  }
});
