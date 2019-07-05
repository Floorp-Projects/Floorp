/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { DownloadHistory } = ChromeUtils.import(
  "resource://gre/modules/DownloadHistory.jsm"
);

/**
 * This test is designed to ensure the cache of download history is correctly
 * loaded and initialized via adding downloads. We do this by having the test as
 * the only test in this file.
 */
add_task(async function test_initialization_via_addDownload() {
  // Clean up at the beginning and at the end of the test.
  async function cleanup() {
    await PlacesUtils.history.clear();
  }
  registerCleanupFunction(cleanup);
  await cleanup();

  const download1FileLocation = getTempFile(`${TEST_TARGET_FILE_NAME}1`).path;
  const download2FileLocation = getTempFile(`${TEST_TARGET_FILE_NAME}2`).path;
  const download = {
    source: {
      url: httpUrl(`source1`),
      isPrivate: false,
    },
    target: { path: download1FileLocation },
  };

  await DownloadHistory.addDownloadToHistory(download);

  // Initialize DownloadHistoryList only after having added the history and
  // session downloads.
  let historyList = await DownloadHistory.getList();
  let downloads = await historyList.getAll();
  Assert.equal(downloads.length, 1, "Should have only one entry");

  Assert.equal(
    downloads[0].target.path,
    download1FileLocation,
    "Should have the correct target path"
  );

  // Now re-add the download but with a different target.
  download.target.path = download2FileLocation;

  await DownloadHistory.addDownloadToHistory(download);

  historyList = await DownloadHistory.getList();
  downloads = await historyList.getAll();
  Assert.equal(downloads.length, 1, "Should still have only one entry");

  Assert.equal(
    downloads[0].target.path,
    download2FileLocation,
    "Should have the correct revised target path"
  );
});
