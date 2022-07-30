/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the main download interfaces using DownloadCopySaver.
 */

"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "DownloadError",
  "resource://gre/modules/DownloadCore.jsm"
);

// Execution of common tests

// This is used in common_test_Download.js
// eslint-disable-next-line no-unused-vars
var gUseLegacySaver = false;

var scriptFile = do_get_file("common_test_Download.js");
Services.scriptloader.loadSubScript(NetUtil.newURI(scriptFile).spec);

// Tests

/**
 * The download should fail early if the source and the target are the same.
 */
add_task(async function test_error_target_downloadingToSameFile() {
  let targetFile = getTempFile(TEST_TARGET_FILE_NAME);
  targetFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);

  let download = await Downloads.createDownload({
    source: NetUtil.newURI(targetFile),
    target: targetFile,
  });
  await Assert.rejects(
    download.start(),
    ex => ex instanceof Downloads.Error && ex.becauseTargetFailed
  );

  Assert.ok(
    await OS.File.exists(download.target.path),
    "The file should not have been deleted."
  );
});

/**
 * Tests allowHttpStatus allowing requests
 */
add_task(async function test_error_notfound() {
  const targetFile = getTempFile(TEST_TARGET_FILE_NAME);
  let called = false;
  const download = await Downloads.createDownload({
    source: {
      url: httpUrl("notfound.gone"),
      allowHttpStatus(aDownload, aStatusCode) {
        Assert.strictEqual(download, aDownload, "Check Download objects");
        Assert.strictEqual(aStatusCode, 404, "The status should be correct");
        called = true;
        return true;
      },
    },
    target: targetFile,
  });
  await download.start();
  Assert.ok(called, "allowHttpStatus should have been called");
});

/**
 * Tests allowHttpStatus rejecting requests
 */
add_task(async function test_error_notfound_reject() {
  const targetFile = getTempFile(TEST_TARGET_FILE_NAME);
  let called = false;
  const download = await Downloads.createDownload({
    source: {
      url: httpUrl("notfound.gone"),
      allowHttpStatus(aDownload, aStatusCode) {
        Assert.strictEqual(download, aDownload, "Check Download objects");
        Assert.strictEqual(aStatusCode, 404, "The status should be correct");
        called = true;
        return false;
      },
    },
    target: targetFile,
  });
  await Assert.rejects(
    download.start(),
    ex => ex instanceof Downloads.Error && ex.becauseSourceFailed,
    "Download should have been rejected"
  );
  Assert.ok(called, "allowHttpStatus should have been called");
});

/**
 * Tests allowHttpStatus rejecting requests other than 404
 */
add_task(async function test_error_busy_reject() {
  const targetFile = getTempFile(TEST_TARGET_FILE_NAME);
  let called = false;
  const download = await Downloads.createDownload({
    source: {
      url: httpUrl("busy.txt"),
      allowHttpStatus(aDownload, aStatusCode) {
        Assert.strictEqual(download, aDownload, "Check Download objects");
        Assert.strictEqual(aStatusCode, 504, "The status should be correct");
        called = true;
        return false;
      },
    },
    target: targetFile,
  });
  await Assert.rejects(
    download.start(),
    ex => ex instanceof Downloads.Error && ex.becauseSourceFailed,
    "Download should have been rejected"
  );
  Assert.ok(called, "allowHttpStatus should have been called");
});

/**
 * Tests redirects are followed correctly, and the meta data corresponds
 * to the correct, final response
 */
add_task(async function test_redirects() {
  const targetFile = getTempFile(TEST_TARGET_FILE_NAME);
  let called = false;
  const download = await Downloads.createDownload({
    source: {
      url: httpUrl("redirect"),
      allowHttpStatus(aDownload, aStatusCode) {
        Assert.strictEqual(download, aDownload, "Check Download objects");
        Assert.strictEqual(
          aStatusCode,
          504,
          "The status should be correct after a redirect"
        );
        called = true;
        return true;
      },
    },
    target: targetFile,
  });
  await download.start();
  Assert.equal(
    download.contentType,
    "text/plain",
    "Content-Type is correct after redirect"
  );
  Assert.equal(
    download.totalBytes,
    TEST_DATA_SHORT.length,
    "Content-Length is correct after redirect"
  );
  Assert.equal(download.target.size, TEST_DATA_SHORT.length);
  Assert.ok(called, "allowHttpStatus should have been called");
});

/**
 * Tests the DownloadError object.
 */
add_task(function test_DownloadError() {
  let error = new DownloadError({
    result: Cr.NS_ERROR_NOT_RESUMABLE,
    message: "Not resumable.",
  });
  Assert.equal(error.result, Cr.NS_ERROR_NOT_RESUMABLE);
  Assert.equal(error.message, "Not resumable.");
  Assert.ok(!error.becauseSourceFailed);
  Assert.ok(!error.becauseTargetFailed);
  Assert.ok(!error.becauseBlocked);
  Assert.ok(!error.becauseBlockedByParentalControls);

  error = new DownloadError({ message: "Unknown error." });
  Assert.equal(error.result, Cr.NS_ERROR_FAILURE);
  Assert.equal(error.message, "Unknown error.");

  error = new DownloadError({ result: Cr.NS_ERROR_NOT_RESUMABLE });
  Assert.equal(error.result, Cr.NS_ERROR_NOT_RESUMABLE);
  Assert.ok(error.message.indexOf("Exception") > 0);

  // becauseSourceFailed will be set, but not the unknown property.
  error = new DownloadError({
    message: "Unknown error.",
    becauseSourceFailed: true,
    becauseUnknown: true,
  });
  Assert.ok(error.becauseSourceFailed);
  Assert.equal(false, "becauseUnknown" in error);

  error = new DownloadError({
    result: Cr.NS_ERROR_MALFORMED_URI,
    inferCause: true,
  });
  Assert.equal(error.result, Cr.NS_ERROR_MALFORMED_URI);
  Assert.ok(error.becauseSourceFailed);
  Assert.ok(!error.becauseTargetFailed);
  Assert.ok(!error.becauseBlocked);
  Assert.ok(!error.becauseBlockedByParentalControls);

  // This test does not set inferCause, so becauseSourceFailed will not be set.
  error = new DownloadError({ result: Cr.NS_ERROR_MALFORMED_URI });
  Assert.equal(error.result, Cr.NS_ERROR_MALFORMED_URI);
  Assert.ok(!error.becauseSourceFailed);

  error = new DownloadError({
    result: Cr.NS_ERROR_FILE_INVALID_PATH,
    inferCause: true,
  });
  Assert.equal(error.result, Cr.NS_ERROR_FILE_INVALID_PATH);
  Assert.ok(!error.becauseSourceFailed);
  Assert.ok(error.becauseTargetFailed);
  Assert.ok(!error.becauseBlocked);
  Assert.ok(!error.becauseBlockedByParentalControls);

  error = new DownloadError({ becauseBlocked: true });
  Assert.equal(error.message, "Download blocked.");
  Assert.ok(!error.becauseSourceFailed);
  Assert.ok(!error.becauseTargetFailed);
  Assert.ok(error.becauseBlocked);
  Assert.ok(!error.becauseBlockedByParentalControls);

  error = new DownloadError({ becauseBlockedByParentalControls: true });
  Assert.equal(error.message, "Download blocked.");
  Assert.ok(!error.becauseSourceFailed);
  Assert.ok(!error.becauseTargetFailed);
  Assert.ok(error.becauseBlocked);
  Assert.ok(error.becauseBlockedByParentalControls);
});

add_task(async function test_cancel_interrupted_download() {
  let targetFile = getTempFile(TEST_TARGET_FILE_NAME);

  let download = await Downloads.createDownload({
    source: httpUrl("interruptible_resumable.txt"),
    target: targetFile,
  });

  async function createAndCancelDownload() {
    info("Create an interruptible download and cancel it midway");
    mustInterruptResponses();
    const promiseDownloaded = download.start();
    await promiseDownloadMidway(download);
    await download.cancel();

    info("Unblock the interruptible download and wait for its annotation");
    continueResponses();
    await waitForAnnotation(
      httpUrl("interruptible_resumable.txt"),
      "downloads/destinationFileURI"
    );

    await Assert.rejects(
      promiseDownloaded,
      /DownloadError: Download canceled/,
      "Got a download error as expected"
    );
  }

  await new Promise(resolve => {
    const DONE = "=== download xpcshell test console listener done ===";
    const logDone = () => Services.console.logStringMessage(DONE);
    const consoleListener = msg => {
      if (msg == DONE) {
        Services.console.unregisterListener(consoleListener);
        resolve();
      }
    };
    Services.console.reset();
    Services.console.registerListener(consoleListener);

    createAndCancelDownload().then(logDone);
  });

  info(
    "Assert that nsIStreamListener.onDataAvailable has not been called after download.cancel"
  );
  let found = Services.console
    .getMessageArray()
    .map(m => m.message)
    .filter(message => {
      return message.includes("nsIStreamListener.onDataAvailable");
    });
  Assert.deepEqual(
    found,
    [],
    "Expect no nsIStreamListener.onDataAvaialable error"
  );
});
