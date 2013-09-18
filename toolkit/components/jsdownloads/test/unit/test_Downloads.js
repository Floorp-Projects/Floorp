/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the functions located directly in the "Downloads" object.
 */

"use strict";

////////////////////////////////////////////////////////////////////////////////
//// Tests

/**
 * Tests that the createDownload function exists and can be called.  More
 * detailed tests are implemented separately for the DownloadCore module.
 */
add_task(function test_createDownload()
{
  // Creates a simple Download object without starting the download.
  yield Downloads.createDownload({
    source: { url: "about:blank" },
    target: { path: getTempFile(TEST_TARGET_FILE_NAME).path },
    saver: { type: "copy" },
  });
});

/**
 * Tests createDownload for private download.
 */
add_task(function test_createDownload_private()
{
  let download = yield Downloads.createDownload({
    source: { url: "about:blank", isPrivate: true },
    target: { path: getTempFile(TEST_TARGET_FILE_NAME).path },
    saver: { type: "copy" }
  });
  do_check_true(download.source.isPrivate);
});

/**
 * Tests createDownload for normal (public) download.
 */
add_task(function test_createDownload_public()
{
  let tempPath = getTempFile(TEST_TARGET_FILE_NAME).path;
  let download = yield Downloads.createDownload({
    source: { url: "about:blank", isPrivate: false },
    target: { path: tempPath },
    saver: { type: "copy" }
  });
  do_check_false(download.source.isPrivate);

  download = yield Downloads.createDownload({
    source: { url: "about:blank" },
    target: { path: tempPath },
    saver: { type: "copy" }
  });
  do_check_false(download.source.isPrivate);
});

/**
 * Tests "fetch" with nsIURI and nsIFile as arguments.
 */
add_task(function test_fetch_uri_file_arguments()
{
  let targetFile = getTempFile(TEST_TARGET_FILE_NAME);
  yield Downloads.fetch(NetUtil.newURI(httpUrl("source.txt")), targetFile);
  yield promiseVerifyContents(targetFile.path, TEST_DATA_SHORT);
});

/**
 * Tests "fetch" with DownloadSource and DownloadTarget as arguments.
 */
add_task(function test_fetch_object_arguments()
{
  let targetPath = getTempFile(TEST_TARGET_FILE_NAME).path;
  yield Downloads.fetch({ url: httpUrl("source.txt") }, { path: targetPath });
  yield promiseVerifyContents(targetPath, TEST_DATA_SHORT);
});

/**
 * Tests "fetch" with string arguments.
 */
add_task(function test_fetch_string_arguments()
{
  let targetPath = getTempFile(TEST_TARGET_FILE_NAME).path;
  yield Downloads.fetch(httpUrl("source.txt"), targetPath);
  yield promiseVerifyContents(targetPath, TEST_DATA_SHORT);

  targetPath = getTempFile(TEST_TARGET_FILE_NAME).path;
  yield Downloads.fetch(new String(httpUrl("source.txt")),
                        new String(targetPath));
  yield promiseVerifyContents(targetPath, TEST_DATA_SHORT);
});

/**
 * Tests that the getList function returns the same list when called multiple
 * times with the same argument, but returns different lists when called with
 * different arguments.  More detailed tests are implemented separately for the
 * DownloadList module.
 */
add_task(function test_getList()
{
  let publicListOne = yield Downloads.getList(Downloads.PUBLIC);
  let privateListOne = yield Downloads.getList(Downloads.PRIVATE);

  let publicListTwo = yield Downloads.getList(Downloads.PUBLIC);
  let privateListTwo = yield Downloads.getList(Downloads.PRIVATE);

  do_check_eq(publicListOne, publicListTwo);
  do_check_eq(privateListOne, privateListTwo);

  do_check_neq(publicListOne, privateListOne);
});

/**
 * Tests that the getSummary function returns the same summary when called
 * multiple times with the same argument, but returns different summaries when
 * called with different arguments.  More detailed tests are implemented
 * separately for the DownloadSummary object in the DownloadList module.
 */
add_task(function test_getSummary()
{
  let publicSummaryOne = yield Downloads.getSummary(Downloads.PUBLIC);
  let privateSummaryOne = yield Downloads.getSummary(Downloads.PRIVATE);

  let publicSummaryTwo = yield Downloads.getSummary(Downloads.PUBLIC);
  let privateSummaryTwo = yield Downloads.getSummary(Downloads.PRIVATE);

  do_check_eq(publicSummaryOne, publicSummaryTwo);
  do_check_eq(privateSummaryOne, privateSummaryTwo);

  do_check_neq(publicSummaryOne, privateSummaryOne);
});

/**
 * Tests that the getSystemDownloadsDirectory returns a valid nsFile
 * download directory object.
 */
add_task(function test_getSystemDownloadsDirectory()
{
  let downloadDir = yield Downloads.getSystemDownloadsDirectory();
  do_check_true(downloadDir instanceof Ci.nsIFile);
});

/**
 * Tests that the getUserDownloadsDirectory returns a valid nsFile
 * download directory object.
 */
add_task(function test_getUserDownloadsDirectory()
{
  let downloadDir = yield Downloads.getUserDownloadsDirectory();
  do_check_true(downloadDir instanceof Ci.nsIFile);
});

/**
 * Tests that the getTemporaryDownloadsDirectory returns a valid nsFile
 * download directory object.
 */
add_task(function test_getTemporaryDownloadsDirectory()
{
  let downloadDir = yield Downloads.getTemporaryDownloadsDirectory();
  do_check_true(downloadDir instanceof Ci.nsIFile);
});

////////////////////////////////////////////////////////////////////////////////
//// Termination

let tailFile = do_get_file("tail.js");
Services.scriptloader.loadSubScript(NetUtil.newURI(tailFile).spec);
