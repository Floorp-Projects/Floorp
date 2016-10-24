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

// //////////////////////////////////////////////////////////////////////////////
// // Execution of common tests

var gUseLegacySaver = false;

var scriptFile = do_get_file("common_test_Download.js");
Services.scriptloader.loadSubScript(NetUtil.newURI(scriptFile).spec);

// //////////////////////////////////////////////////////////////////////////////
// // Tests

/**
 * Tests the DownloadError object.
 */
add_task(function test_DownloadError()
{
  let error = new DownloadError({ result: Cr.NS_ERROR_NOT_RESUMABLE,
                                  message: "Not resumable."});
  do_check_eq(error.result, Cr.NS_ERROR_NOT_RESUMABLE);
  do_check_eq(error.message, "Not resumable.");
  do_check_false(error.becauseSourceFailed);
  do_check_false(error.becauseTargetFailed);
  do_check_false(error.becauseBlocked);
  do_check_false(error.becauseBlockedByParentalControls);

  error = new DownloadError({ message: "Unknown error."});
  do_check_eq(error.result, Cr.NS_ERROR_FAILURE);
  do_check_eq(error.message, "Unknown error.");

  error = new DownloadError({ result: Cr.NS_ERROR_NOT_RESUMABLE });
  do_check_eq(error.result, Cr.NS_ERROR_NOT_RESUMABLE);
  do_check_true(error.message.indexOf("Exception") > 0);

  // becauseSourceFailed will be set, but not the unknown property.
  error = new DownloadError({ message: "Unknown error.",
                              becauseSourceFailed: true,
                              becauseUnknown: true });
  do_check_true(error.becauseSourceFailed);
  do_check_false("becauseUnknown" in error);

  error = new DownloadError({ result: Cr.NS_ERROR_MALFORMED_URI,
                              inferCause: true });
  do_check_eq(error.result, Cr.NS_ERROR_MALFORMED_URI);
  do_check_true(error.becauseSourceFailed);
  do_check_false(error.becauseTargetFailed);
  do_check_false(error.becauseBlocked);
  do_check_false(error.becauseBlockedByParentalControls);

  // This test does not set inferCause, so becauseSourceFailed will not be set.
  error = new DownloadError({ result: Cr.NS_ERROR_MALFORMED_URI });
  do_check_eq(error.result, Cr.NS_ERROR_MALFORMED_URI);
  do_check_false(error.becauseSourceFailed);

  error = new DownloadError({ result: Cr.NS_ERROR_FILE_INVALID_PATH,
                              inferCause: true });
  do_check_eq(error.result, Cr.NS_ERROR_FILE_INVALID_PATH);
  do_check_false(error.becauseSourceFailed);
  do_check_true(error.becauseTargetFailed);
  do_check_false(error.becauseBlocked);
  do_check_false(error.becauseBlockedByParentalControls);

  error = new DownloadError({ becauseBlocked: true });
  do_check_eq(error.message, "Download blocked.");
  do_check_false(error.becauseSourceFailed);
  do_check_false(error.becauseTargetFailed);
  do_check_true(error.becauseBlocked);
  do_check_false(error.becauseBlockedByParentalControls);

  error = new DownloadError({ becauseBlockedByParentalControls: true });
  do_check_eq(error.message, "Download blocked.");
  do_check_false(error.becauseSourceFailed);
  do_check_false(error.becauseTargetFailed);
  do_check_true(error.becauseBlocked);
  do_check_true(error.becauseBlockedByParentalControls);
});
