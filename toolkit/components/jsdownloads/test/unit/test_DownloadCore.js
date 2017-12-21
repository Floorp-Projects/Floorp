/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the main download interfaces using DownloadCopySaver.
 */

"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "DownloadError",
                                  "resource://gre/modules/DownloadCore.jsm");

// Execution of common tests

var gUseLegacySaver = false;

var scriptFile = do_get_file("common_test_Download.js");
Services.scriptloader.loadSubScript(NetUtil.newURI(scriptFile).spec);

// Tests

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
