/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { DownloadHistory } = ChromeUtils.import(
  "resource://gre/modules/DownloadHistory.jsm"
);
const { PlacesTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PlacesTestUtils.sys.mjs"
);

let baseDate = new Date("2000-01-01");

/**
 * This test is designed to ensure the cache of download history is correctly
 * loaded and initialized. We do this by having the test as the only test in
 * this file, and injecting data into the places database before we start.
 */
add_task(async function test_DownloadHistory_initialization() {
  // Clean up at the beginning and at the end of the test.
  async function cleanup() {
    await PlacesUtils.history.clear();
  }
  registerCleanupFunction(cleanup);
  await cleanup();

  let testDownloads = [];
  for (let i = 10; i <= 30; i += 10) {
    let targetFile = getTempFile(`${TEST_TARGET_FILE_NAME}${i}`);
    let download = {
      source: {
        url: httpUrl(`source${i}`),
        isPrivate: false,
      },
      target: { path: targetFile.path },
      endTime: baseDate.getTime() + i,
      fileSize: 100 + i,
      state: i / 10,
    };

    await PlacesTestUtils.addVisits([
      {
        uri: download.source.url,
        transition: PlacesUtils.history.TRANSITIONS.DOWNLOAD,
      },
    ]);

    let targetUri = Services.io.newFileURI(
      new FileUtils.File(download.target.path)
    );

    await PlacesUtils.history.update({
      annotations: new Map([
        ["downloads/destinationFileURI", targetUri.spec],
        [
          "downloads/metaData",
          JSON.stringify({
            state: download.state,
            endTime: download.endTime,
            fileSize: download.fileSize,
          }),
        ],
      ]),
      url: download.source.url,
    });

    testDownloads.push(download);
  }

  // Initialize DownloadHistoryList only after having added the history and
  // session downloads.
  let historyList = await DownloadHistory.getList();
  let downloads = await historyList.getAll();
  Assert.equal(downloads.length, testDownloads.length);

  for (let expected of testDownloads) {
    let download = downloads.find(d => d.source.url == expected.source.url);

    info(`Checking download ${expected.source.url}`);
    Assert.ok(download, "Should have found the expected download");
    Assert.equal(
      download.endTime,
      expected.endTime,
      "Should have the correct end time"
    );
    Assert.equal(
      download.target.size,
      expected.fileSize,
      "Should have the correct file size"
    );
    Assert.equal(
      download.succeeded,
      expected.state == 1,
      "Should have the correct succeeded value"
    );
    Assert.equal(
      download.canceled,
      expected.state == 3,
      "Should have the correct canceled value"
    );
    Assert.equal(
      download.target.path,
      expected.target.path,
      "Should have the correct target path"
    );
  }
});
