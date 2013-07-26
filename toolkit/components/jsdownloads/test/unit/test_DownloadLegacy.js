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
 * @param aSourceUrl
 *        String containing the URI for the download source, or null to use
 *        httpUrl("source.txt").
 * @param isPrivate
 *        Optional boolean indicates whether the download originated from a
 *        private window.
 * @param aOutPersist
 *        Optional object that receives a reference to the created
 *        nsIWebBrowserPersist instance in the "value" property.
 *
 * @return {Promise}
 * @resolves The Download object created as a consequence of controlling the
 *           download through the legacy nsITransfer interface.
 * @rejects Never.  The current test fails in case of exceptions.
 */
function promiseStartLegacyDownload(aSourceUrl, aIsPrivate, aOutPersist) {
  let sourceURI = NetUtil.newURI(aSourceUrl || httpUrl("source.txt"));
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
  let promise = aIsPrivate ? Downloads.getPrivateDownloadList() :
                Downloads.getPublicDownloadList();
  promise.then(function (aList) {
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
                  persist, aIsPrivate);
    persist.progressListener = transfer;

    // Start the actual download process.
    persist.savePrivacyAwareURI(sourceURI, null, null, null, null, targetFile, aIsPrivate);
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
  do_check_eq(download.source.url, httpUrl("source.txt"));
  do_check_true(new FileUtils.File(download.target.path).parent
                                                        .equals(tempDirectory));

  // The download is already started, wait for completion and report any errors.
  if (!download.stopped) {
    yield download.start();
  }

  yield promiseVerifyContents(download.target.path, TEST_DATA_SHORT);
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

  let download = yield promiseStartLegacyDownload(httpUrl("interruptible.txt"));

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

  yield promiseVerifyContents(download.target.path,
                              TEST_DATA_SHORT + TEST_DATA_SHORT);
});

/**
 * Downloads a file with a "Content-Length" of 0 and checks the progress.
 */
add_task(function test_empty_progress()
{
  let download = yield promiseStartLegacyDownload(httpUrl("empty.txt"));

  // The download is already started, wait for completion and report any errors.
  if (!download.stopped) {
    yield download.start();
  }

  do_check_true(download.stopped);
  do_check_true(download.hasProgress);
  do_check_eq(download.progress, 100);
  do_check_eq(download.currentBytes, 0);
  do_check_eq(download.totalBytes, 0);

  do_check_eq((yield OS.File.stat(download.target.path)).size, 0);
});

/**
 * Downloads an empty file with no "Content-Length" and checks the progress.
 */
add_task(function test_empty_noprogress()
{
  let deferResponse = deferNextResponse();
  let promiseEmptyRequestReceived = promiseNextRequestReceived();

  let download = yield promiseStartLegacyDownload(
                                         httpUrl("empty-noprogress.txt"));

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

  do_check_eq((yield OS.File.stat(download.target.path)).size, 0);
});

/**
 * Cancels a download and verifies that its state is reported correctly.
 */
add_task(function test_cancel_midway()
{
  let deferResponse = deferNextResponse();
  let outPersist = {};
  let download = yield promiseStartLegacyDownload(httpUrl("interruptible.txt"),
                                                  false, outPersist);

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

    do_check_false(yield OS.File.exists(download.target.path));

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
    let sourceUrl = "http://localhost:" + serverSocket.port + "/source.txt";

    let download = yield promiseStartLegacyDownload(sourceUrl);

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

/**
 * Executes download in both public and private modes.
 */
add_task(function test_download_public_and_private()
{
  let sourcePath = "/test_download_public_and_private.txt";
  let sourceUrl = httpUrl("test_download_public_and_private.txt");
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
  let download = yield promiseStartLegacyDownload(sourceUrl, true);
  // The download is already started, wait for completion and report any errors.
  if (!download.stopped) {
    yield download.start();
  }

  cleanup();
});

/**
 * Download with parental controls enabled.
 */
add_task(function test_download_blocked_parental_controls()
{
  function cleanup() {
    DownloadIntegration.shouldBlockInTest = false;
  }
  do_register_cleanup(cleanup);
  DownloadIntegration.shouldBlockInTest = true;

  let download = yield promiseStartLegacyDownload();

  try {
    yield download.start();
    do_throw("The download should have blocked.");
  } catch (ex if ex instanceof Downloads.Error && ex.becauseBlocked) {
    do_check_true(ex.becauseBlockedByParentalControls);
  }

  do_check_false(yield OS.File.exists(download.target.path));

  cleanup();
});
