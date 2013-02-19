/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the objects defined in the "DownloadCore" module.
 */

"use strict";

////////////////////////////////////////////////////////////////////////////////
//// Globals

/**
 * Creates a new Download object, using TEST_TARGET_FILE_NAME as the target.
 * The target is deleted by getTempFile when this function is called.
 *
 * @param aSourceURI
 *        The nsIURI for the download source, or null to use TEST_SOURCE_URI.
 *
 * @return {Promise}
 * @resolves The newly created Download object.
 * @rejects JavaScript exception.
 */
function promiseSimpleDownload(aSourceURI) {
  return Downloads.createDownload({
    source: { uri: aSourceURI || TEST_SOURCE_URI },
    target: { file: getTempFile(TEST_TARGET_FILE_NAME) },
    saver: { type: "copy" },
  });
}

////////////////////////////////////////////////////////////////////////////////
//// Tests

/**
 * Executes a download, started by constructing the simplest Download object.
 */
add_task(function test_download_construction()
{
  let targetFile = getTempFile(TEST_TARGET_FILE_NAME);

  let download = yield Downloads.createDownload({
    source: { uri: TEST_SOURCE_URI },
    target: { file: targetFile },
    saver: { type: "copy" },
  });

  // Checks the generated DownloadSource and DownloadTarget properties.
  do_check_true(download.source.uri.equals(TEST_SOURCE_URI));
  do_check_eq(download.target.file, targetFile);

  // Starts the download and waits for completion.
  yield download.start();

  yield promiseVerifyContents(targetFile, TEST_DATA_SHORT);
});

/**
 * Checks initial and final state and progress for a successful download.
 */
add_task(function test_download_initial_final_state()
{
  let download = yield promiseSimpleDownload();

  do_check_false(download.stopped);
  do_check_eq(download.progress, 0);

  // Starts the download and waits for completion.
  yield download.start();

  do_check_true(download.stopped);
  do_check_eq(download.progress, 100);
});

/**
 * Checks that the view is notified of the final download state.
 */
add_task(function test_download_view_final_notified()
{
  let download = yield promiseSimpleDownload();

  let onchangeNotified = false;
  let lastNotifiedStopped;
  let lastNotifiedProgress;
  download.onchange = function () {
    onchangeNotified = true;
    lastNotifiedStopped = download.stopped;
    lastNotifiedProgress = download.progress;
  };

  // Starts the download and waits for completion.
  yield download.start();

  // The view should have been notified before the download completes.
  do_check_true(onchangeNotified);
  do_check_true(lastNotifiedStopped);
  do_check_eq(lastNotifiedProgress, 100);
});

/**
 * Checks intermediate progress for a successful download.
 */
add_task(function test_download_intermediate_progress()
{
  let interruptible = startInterruptibleResponseHandler();

  let download = yield promiseSimpleDownload(TEST_INTERRUPTIBLE_URI);

  download.onchange = function () {
    if (download.progress == 50) {
      do_check_true(download.hasProgress);
      do_check_eq(download.currentBytes, TEST_DATA_SHORT.length);
      do_check_eq(download.totalBytes, TEST_DATA_SHORT.length * 2);

      // Continue after the first chunk of data is fully received.
      interruptible.resolve();
    }
  };

  // Starts the download and waits for completion.
  yield download.start();

  do_check_true(download.stopped);
  do_check_eq(download.progress, 100);

  yield promiseVerifyContents(download.target.file,
                              TEST_DATA_SHORT + TEST_DATA_SHORT);
});

/**
 * Cancels a download and verifies that its state is reported correctly.
 */
add_task(function test_download_cancel()
{
  let interruptible = startInterruptibleResponseHandler();
  try {
    let download = yield promiseSimpleDownload(TEST_INTERRUPTIBLE_URI);

    // Cancel the download after receiving the first part of the response.
    download.onchange = function () {
      if (!download.stopped && download.progress == 50) {
        download.cancel();
      }
    };

    try {
      yield download.start();
      do_throw("The download should have been canceled.");
    } catch (ex if ex instanceof Downloads.Error) {
      do_check_false(ex.becauseSourceFailed);
      do_check_false(ex.becauseTargetFailed);
    }

    do_check_true(download.stopped);
    do_check_true(download.canceled);
    do_check_true(download.error === null);

    do_check_false(download.target.file.exists());
  } finally {
    interruptible.reject();
  }
});

/**
 * Cancels a download right after starting it.
 */
add_task(function test_download_cancel_immediately()
{
  let interruptible = startInterruptibleResponseHandler();
  try {
    let download = yield promiseSimpleDownload(TEST_INTERRUPTIBLE_URI);

    let promiseStopped = download.start();
    download.cancel();

    try {
      yield promiseStopped;
      do_throw("The download should have been canceled.");
    } catch (ex if ex instanceof Downloads.Error) {
      do_check_false(ex.becauseSourceFailed);
      do_check_false(ex.becauseTargetFailed);
    }

    do_check_true(download.stopped);
    do_check_true(download.canceled);
    do_check_true(download.error === null);

    do_check_false(download.target.file.exists());
  } finally {
    interruptible.reject();
  }
});

/**
 * Ensures download error details are reported on network failures.
 */
add_task(function test_download_error_source()
{
  let serverSocket = startFakeServer();
  try {
    let download = yield promiseSimpleDownload(TEST_FAKE_SOURCE_URI);

    do_check_true(download.error === null);

    try {
      yield download.start();
      do_throw("The download should have failed.");
    } catch (ex if ex instanceof Downloads.Error && ex.becauseSourceFailed) {
      // A specific error object is thrown when reading from the source fails.
    }

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
add_task(function test_download_error_target()
{
  let download = yield promiseSimpleDownload();

  do_check_true(download.error === null);

  // Create a file without write access permissions before downloading.
  download.target.file.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0);

  try {
    yield download.start();
    do_throw("The download should have failed.");
  } catch (ex if ex instanceof Downloads.Error && ex.becauseTargetFailed) {
    // A specific error object is thrown when writing to the target fails.
  }

  do_check_true(download.stopped);
  do_check_false(download.canceled);
  do_check_true(download.error !== null);
  do_check_true(download.error.becauseTargetFailed);
  do_check_false(download.error.becauseSourceFailed);
});
