/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the main download interfaces using DownloadCopySaver.
 */

"use strict";

ChromeUtils.defineModuleGetter(this, "DownloadError",
                               "resource://gre/modules/DownloadCore.jsm");

// Execution of common tests

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
  await Assert.rejects(download.start(), ex => ex instanceof Downloads.Error &&
                                               ex.becauseTargetFailed);

  Assert.ok(await OS.File.exists(download.target.path),
            "The file should not have been deleted.");
});

/**
 * Tests the DownloadError object.
 */
add_task(function test_DownloadError() {
  let error = new DownloadError({ result: Cr.NS_ERROR_NOT_RESUMABLE,
                                  message: "Not resumable."});
  Assert.equal(error.result, Cr.NS_ERROR_NOT_RESUMABLE);
  Assert.equal(error.message, "Not resumable.");
  Assert.ok(!error.becauseSourceFailed);
  Assert.ok(!error.becauseTargetFailed);
  Assert.ok(!error.becauseBlocked);
  Assert.ok(!error.becauseBlockedByParentalControls);

  error = new DownloadError({ message: "Unknown error."});
  Assert.equal(error.result, Cr.NS_ERROR_FAILURE);
  Assert.equal(error.message, "Unknown error.");

  error = new DownloadError({ result: Cr.NS_ERROR_NOT_RESUMABLE });
  Assert.equal(error.result, Cr.NS_ERROR_NOT_RESUMABLE);
  Assert.ok(error.message.indexOf("Exception") > 0);

  // becauseSourceFailed will be set, but not the unknown property.
  error = new DownloadError({ message: "Unknown error.",
                              becauseSourceFailed: true,
                              becauseUnknown: true });
  Assert.ok(error.becauseSourceFailed);
  Assert.equal(false, "becauseUnknown" in error);

  error = new DownloadError({ result: Cr.NS_ERROR_MALFORMED_URI,
                              inferCause: true });
  Assert.equal(error.result, Cr.NS_ERROR_MALFORMED_URI);
  Assert.ok(error.becauseSourceFailed);
  Assert.ok(!error.becauseTargetFailed);
  Assert.ok(!error.becauseBlocked);
  Assert.ok(!error.becauseBlockedByParentalControls);

  // This test does not set inferCause, so becauseSourceFailed will not be set.
  error = new DownloadError({ result: Cr.NS_ERROR_MALFORMED_URI });
  Assert.equal(error.result, Cr.NS_ERROR_MALFORMED_URI);
  Assert.ok(!error.becauseSourceFailed);

  error = new DownloadError({ result: Cr.NS_ERROR_FILE_INVALID_PATH,
                              inferCause: true });
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
