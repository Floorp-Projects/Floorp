/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the functions located directly in the "Downloads" object.
 */

"use strict";

// Tests

/**
 * Tests that the createDownload function exists and can be called.  More
 * detailed tests are implemented separately for the DownloadCore module.
 */
add_task(async function test_createDownload() {
  // Creates a simple Download object without starting the download.
  await Downloads.createDownload({
    source: { url: "about:blank" },
    target: { path: getTempFile(TEST_TARGET_FILE_NAME).path },
    saver: { type: "copy" },
  });
});

/**
 * Tests createDownload for private download.
 */
add_task(async function test_createDownload_private() {
  let download = await Downloads.createDownload({
    source: { url: "about:blank", isPrivate: true },
    target: { path: getTempFile(TEST_TARGET_FILE_NAME).path },
    saver: { type: "copy" }
  });
  Assert.ok(download.source.isPrivate);
});

/**
 * Tests createDownload for normal (public) download.
 */
add_task(async function test_createDownload_public() {
  let tempPath = getTempFile(TEST_TARGET_FILE_NAME).path;
  let download = await Downloads.createDownload({
    source: { url: "about:blank", isPrivate: false },
    target: { path: tempPath },
    saver: { type: "copy" }
  });
  Assert.ok(!download.source.isPrivate);

  download = await Downloads.createDownload({
    source: { url: "about:blank" },
    target: { path: tempPath },
    saver: { type: "copy" }
  });
  Assert.ok(!download.source.isPrivate);
});

/**
 * Tests createDownload for a pdf saver throws if only given a url.
 */
add_task(async function test_createDownload_pdf() {
  let download = await Downloads.createDownload({
    source: { url: "about:blank" },
    target: { path: getTempFile(TEST_TARGET_FILE_NAME).path },
    saver: { type: "pdf" },
  });

  try {
    await download.start();
    do_throw("The download should have failed.");
  } catch (ex) {
    if (!(ex instanceof Downloads.Error) || !ex.becauseSourceFailed) {
      throw ex;
    }
  }

  Assert.ok(!download.succeeded);
  Assert.ok(download.stopped);
  Assert.ok(!download.canceled);
  Assert.ok(download.error !== null);
  Assert.ok(download.error.becauseSourceFailed);
  Assert.ok(!download.error.becauseTargetFailed);
  Assert.equal(false, await OS.File.exists(download.target.path));
});

/**
 * Tests "fetch" with nsIURI and nsIFile as arguments.
 */
add_task(async function test_fetch_uri_file_arguments() {
  let targetFile = getTempFile(TEST_TARGET_FILE_NAME);
  await Downloads.fetch(NetUtil.newURI(httpUrl("source.txt")), targetFile);
  await promiseVerifyContents(targetFile.path, TEST_DATA_SHORT);
});

/**
 * Tests "fetch" with DownloadSource and DownloadTarget as arguments.
 */
add_task(async function test_fetch_object_arguments() {
  let targetPath = getTempFile(TEST_TARGET_FILE_NAME).path;
  await Downloads.fetch({ url: httpUrl("source.txt") }, { path: targetPath });
  await promiseVerifyContents(targetPath, TEST_DATA_SHORT);
});

/**
 * Tests "fetch" with string arguments.
 */
add_task(async function test_fetch_string_arguments() {
  let targetPath = getTempFile(TEST_TARGET_FILE_NAME).path;
  await Downloads.fetch(httpUrl("source.txt"), targetPath);
  await promiseVerifyContents(targetPath, TEST_DATA_SHORT);

  targetPath = getTempFile(TEST_TARGET_FILE_NAME).path;
  await Downloads.fetch(httpUrl("source.txt"),
                        targetPath);
  await promiseVerifyContents(targetPath, TEST_DATA_SHORT);
});

/**
 * Tests that the getList function returns the same list when called multiple
 * times with the same argument, but returns different lists when called with
 * different arguments.  More detailed tests are implemented separately for the
 * DownloadList module.
 */
add_task(async function test_getList() {
  let publicListOne = await Downloads.getList(Downloads.PUBLIC);
  let privateListOne = await Downloads.getList(Downloads.PRIVATE);

  let publicListTwo = await Downloads.getList(Downloads.PUBLIC);
  let privateListTwo = await Downloads.getList(Downloads.PRIVATE);

  Assert.equal(publicListOne, publicListTwo);
  Assert.equal(privateListOne, privateListTwo);

  Assert.notEqual(publicListOne, privateListOne);
});

/**
 * Tests that the getSummary function returns the same summary when called
 * multiple times with the same argument, but returns different summaries when
 * called with different arguments.  More detailed tests are implemented
 * separately for the DownloadSummary object in the DownloadList module.
 */
add_task(async function test_getSummary() {
  let publicSummaryOne = await Downloads.getSummary(Downloads.PUBLIC);
  let privateSummaryOne = await Downloads.getSummary(Downloads.PRIVATE);

  let publicSummaryTwo = await Downloads.getSummary(Downloads.PUBLIC);
  let privateSummaryTwo = await Downloads.getSummary(Downloads.PRIVATE);

  Assert.equal(publicSummaryOne, publicSummaryTwo);
  Assert.equal(privateSummaryOne, privateSummaryTwo);

  Assert.notEqual(publicSummaryOne, privateSummaryOne);
});

/**
 * Tests that the getSystemDownloadsDirectory returns a non-empty download
 * directory string.
 */
add_task(async function test_getSystemDownloadsDirectory() {
  let downloadDir = await Downloads.getSystemDownloadsDirectory();
  Assert.notEqual(downloadDir, "");
});

/**
 * Tests that the getPreferredDownloadsDirectory returns a non-empty download
 * directory string.
 */
add_task(async function test_getPreferredDownloadsDirectory() {
  let downloadDir = await Downloads.getPreferredDownloadsDirectory();
  Assert.notEqual(downloadDir, "");
});

/**
 * Tests that the getTemporaryDownloadsDirectory returns a non-empty download
 * directory string.
 */
add_task(async function test_getTemporaryDownloadsDirectory() {
  let downloadDir = await Downloads.getTemporaryDownloadsDirectory();
  Assert.notEqual(downloadDir, "");
});
