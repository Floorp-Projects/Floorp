/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the integration with legacy interfaces for downloads.
 */

"use strict";

// Execution of common tests

var gUseLegacySaver = true;

var scriptFile = do_get_file("common_test_Download.js");
Services.scriptloader.loadSubScript(NetUtil.newURI(scriptFile).spec);

/**
 * Checks the referrer for restart downloads.
 * If the legacy download is stopped and restarted, the saving method
 * is changed from DownloadLegacySaver to the DownloadCopySaver.
 * The referrer header should be passed correctly.
 */
add_task(async function test_referrer_restart() {
  let sourcePath = "/test_referrer_restart.txt";
  let sourceUrl = httpUrl("test_referrer_restart.txt");

  function cleanup() {
    gHttpServer.registerPathHandler(sourcePath, null);
  }
  registerCleanupFunction(cleanup);

  registerInterruptibleHandler(sourcePath,
    function firstPart(aRequest, aResponse) {
      aResponse.setHeader("Content-Type", "text/plain", false);
      Assert.ok(aRequest.hasHeader("Referer"));
      Assert.equal(aRequest.getHeader("Referer"), TEST_REFERRER_URL);

      aResponse.setHeader("Content-Length", "" + (TEST_DATA_SHORT.length * 2),
                          false);
      aResponse.write(TEST_DATA_SHORT);
    }, function secondPart(aRequest, aResponse) {
      Assert.ok(aRequest.hasHeader("Referer"));
      Assert.equal(aRequest.getHeader("Referer"), TEST_REFERRER_URL);

      aResponse.write(TEST_DATA_SHORT);
    });

  async function restart_and_check_referrer(download) {
    let promiseSucceeded = download.whenSucceeded();

    // Cancel the first download attempt.
    await promiseDownloadMidway(download);
    await download.cancel();

    // The second request is allowed to complete.
    continueResponses();
    download.start().catch(() => {});

    // Wait for the download to finish by waiting on the whenSucceeded promise.
    await promiseSucceeded;

    Assert.ok(download.stopped);
    Assert.ok(download.succeeded);
    Assert.ok(!download.canceled);

    Assert.equal(download.source.referrer, TEST_REFERRER_URL);
  }

  mustInterruptResponses();
  let download = await promiseStartLegacyDownload(
    sourceUrl, { referrer: TEST_REFERRER_URL });
  await restart_and_check_referrer(download);

  mustInterruptResponses();
  download = await promiseStartLegacyDownload(
    sourceUrl, { referrer: TEST_REFERRER_URL,
                 isPrivate: true});
  await restart_and_check_referrer(download);

  cleanup();
});

