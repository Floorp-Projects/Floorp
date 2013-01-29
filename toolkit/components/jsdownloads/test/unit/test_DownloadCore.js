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

function promiseSimpleDownload() {
  return Downloads.createDownload({
    source: { uri: TEST_SOURCE_URI },
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

  do_check_false(download.done);
  do_check_eq(download.progress, 0);

  // Starts the download and waits for completion.
  yield download.start();

  do_check_true(download.done);
  do_check_eq(download.progress, 100);
});

/**
 * Checks that the view is notified of the final download state.
 */
add_task(function test_download_view_final_notified()
{
  let download = yield promiseSimpleDownload();

  let onchangeNotified = false;
  let lastNotifiedDone;
  let lastNotifiedProgress;
  download.onchange = function () {
    onchangeNotified = true;
    lastNotifiedDone = download.done;
    lastNotifiedProgress = download.progress;
  };

  // Starts the download and waits for completion.
  yield download.start();

  // The view should have been notified before the download completes.
  do_check_true(onchangeNotified);
  do_check_true(lastNotifiedDone);
  do_check_eq(lastNotifiedProgress, 100);
});

/**
 * Checks intermediate progress for a successful download.
 */
add_task(function test_download_intermediate_progress()
{
  let targetFile = getTempFile(TEST_TARGET_FILE_NAME);

  let deferUntilHalfProgress = Promise.defer();
  gHttpServer.registerPathHandler("/test_download_intermediate_progress",
    function (aRequest, aResponse)
    {
      aResponse.processAsync();
      aResponse.setHeader("Content-Type", "text/plain", false);
      aResponse.setHeader("Content-Length", "" + (TEST_DATA_SHORT.length * 2),
                          false);
      aResponse.write(TEST_DATA_SHORT);

      deferUntilHalfProgress.promise.then(function () {
        aResponse.write(TEST_DATA_SHORT);
        aResponse.finish();
      });
    });

  let download = yield Downloads.createDownload({
    source: { uri: NetUtil.newURI(HTTP_BASE +
                                  "/test_download_intermediate_progress") },
    target: { file: targetFile },
    saver: { type: "copy" },
  });

  download.onchange = function () {
    if (download.progress == 50) {
      do_check_true(download.hasProgress);
      do_check_eq(download.currentBytes, TEST_DATA_SHORT.length);
      do_check_eq(download.totalBytes, TEST_DATA_SHORT.length * 2);

      // Continue after the first chunk of data is fully received.
      deferUntilHalfProgress.resolve();
    }
  };

  // Starts the download and waits for completion.
  yield download.start();

  do_check_true(download.done);
  do_check_eq(download.progress, 100);

  yield promiseVerifyContents(targetFile, TEST_DATA_SHORT + TEST_DATA_SHORT);
});
