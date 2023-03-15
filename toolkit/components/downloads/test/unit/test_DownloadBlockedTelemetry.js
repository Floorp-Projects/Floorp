/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.prefs.setBoolPref(
  "toolkit.telemetry.testing.overrideProductsCheck",
  true
);

const errors = [
  Downloads.Error.BLOCK_VERDICT_MALWARE,
  Downloads.Error.BLOCK_VERDICT_POTENTIALLY_UNWANTED,
  Downloads.Error.BLOCK_VERDICT_INSECURE,
  Downloads.Error.BLOCK_VERDICT_UNCOMMON,
];

add_task(async function test_confirm_block_download() {
  for (let error of errors) {
    info(`Testing block ${error} download`);
    let histogram = TelemetryTestUtils.getAndClearKeyedHistogram(
      "DOWNLOADS_USER_ACTION_ON_BLOCKED_DOWNLOAD"
    );

    let download;
    try {
      info(`Create ${error} download`);
      if (error == Downloads.Error.BLOCK_VERDICT_INSECURE) {
        download = await promiseStartLegacyDownload(null, {
          downloadClassification: Ci.nsITransfer.DOWNLOAD_POTENTIALLY_UNSAFE,
        });
      } else {
        download = await promiseBlockedDownload({
          keepPartialData: true,
          keepBlockedData: true,
          useLegacySaver: false,
          verdict: error,
        });
      }
      await download.start();
      do_throw("The download should have failed.");
    } catch (ex) {
      if (!(ex instanceof Downloads.Error)) {
        throw ex;
      }
    }

    // Test blocked download is recorded
    TelemetryTestUtils.assertKeyedHistogramValue(histogram, error, 0, 1);

    // Test confirm block
    histogram = TelemetryTestUtils.getAndClearKeyedHistogram(
      "DOWNLOADS_USER_ACTION_ON_BLOCKED_DOWNLOAD"
    );
    info(`Block ${error} download`);
    await download.confirmBlock();
    TelemetryTestUtils.assertKeyedHistogramValue(histogram, error, 1, 1);
  }
});

add_task(async function test_confirm_unblock_download() {
  for (let error of errors) {
    info(`Testing unblock ${error} download`);
    let histogram = TelemetryTestUtils.getAndClearKeyedHistogram(
      "DOWNLOADS_USER_ACTION_ON_BLOCKED_DOWNLOAD"
    );

    let download;
    try {
      info(`Create ${error} download`);
      if (error == Downloads.Error.BLOCK_VERDICT_INSECURE) {
        download = await promiseStartLegacyDownload(null, {
          downloadClassification: Ci.nsITransfer.DOWNLOAD_POTENTIALLY_UNSAFE,
        });
      } else {
        download = await promiseBlockedDownload({
          keepPartialData: true,
          keepBlockedData: true,
          useLegacySaver: false,
          verdict: error,
        });
      }
      await download.start();
      do_throw("The download should have failed.");
    } catch (ex) {
      if (!(ex instanceof Downloads.Error)) {
        throw ex;
      }
    }

    // Test blocked download is recorded
    TelemetryTestUtils.assertKeyedHistogramValue(histogram, error, 0, 1);

    // Test unblock
    histogram = TelemetryTestUtils.getAndClearKeyedHistogram(
      "DOWNLOADS_USER_ACTION_ON_BLOCKED_DOWNLOAD"
    );
    info(`Unblock ${error} download`);
    let promise = new Promise(r => (download.onchange = r));
    await download.unblock();
    // The environment is not set up properly for performing a real download, cancel
    // the unblocked download so it doesn't affect the next testcase.
    await download.cancel();
    await promise;
    if (error == Downloads.Error.BLOCK_VERDICT_INSECURE) {
      Assert.ok(!download.error, "Ensure we didn't set download.error");
    }

    TelemetryTestUtils.assertKeyedHistogramValue(histogram, error, 2, 1);
  }
});
