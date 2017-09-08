/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the DownloadHistory module.
 */

"use strict";

Cu.import("resource://gre/modules/DownloadHistory.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "gDownloadHistory",
           "@mozilla.org/browser/download-history;1",
           Ci.nsIDownloadHistory);

let baseDate = new Date("2000-01-01");

/**
 * Waits for the download annotations to be set for the given page, required
 * because the addDownload method will add these to the database asynchronously.
 */
function waitForAnnotations(sourceUriSpec) {
  let sourceUri = Services.io.newURI(sourceUriSpec);
  let destinationFileUriSet = false;
  let metaDataSet = false;
  return new Promise(resolve => {
    PlacesUtils.annotations.addObserver({
      onPageAnnotationSet(page, name) {
        if (!page.equals(sourceUri)) {
          return;
        }
        switch (name) {
          case "downloads/destinationFileURI":
            destinationFileUriSet = true;
            break;
          case "downloads/metaData":
            metaDataSet = true;
            break;
        }
        if (destinationFileUriSet && metaDataSet) {
          PlacesUtils.annotations.removeObserver(this);
          resolve();
        }
      },
      onItemAnnotationSet() {},
      onPageAnnotationRemoved() {},
      onItemAnnotationRemoved() {},
    });
  });
}

/**
 * Non-fatal assertion used to test whether the downloads in the list already
 * match the expected state.
 */
function areEqual(a, b) {
  if (a === b) {
    Assert.equal(a, b);
    return true;
  }
  do_print(a + " !== " + b);
  return false;
}

/**
 * This allows waiting for an expected list at various points during the test.
 */
class TestView {
  constructor(expected) {
    this.expected = [...expected];
    this.downloads = [];
    this.resolveWhenExpected = () => {};
  }
  onDownloadAdded(download, options = {}) {
    if (options.insertBefore) {
      let index = this.downloads.indexOf(options.insertBefore);
      this.downloads.splice(index, 0, download);
    } else {
      this.downloads.push(download);
    }
    this.checkForExpectedDownloads();
  }
  onDownloadChanged(download) {
    this.checkForExpectedDownloads();
  }
  onDownloadRemoved(download) {
    let index = this.downloads.indexOf(download);
    this.downloads.splice(index, 1);
    this.checkForExpectedDownloads();
  }
  checkForExpectedDownloads() {
    // Wait for all the expected downloads to be added or removed before doing
    // the detailed tests. This is done to avoid creating irrelevant output.
    if (this.downloads.length != this.expected.length) {
      return;
    }
    for (let i = 0; i < this.downloads.length; i++) {
      if (this.downloads[i].source.url != this.expected[i].source.url ||
          this.downloads[i].target.path != this.expected[i].target.path) {
        return;
      }
    }
    // Check and report the actual state of the downloads. Even if the items
    // are in the expected order, the metadata for history downloads might not
    // have been updated to the final state yet.
    for (let i = 0; i < this.downloads.length; i++) {
      let download = this.downloads[i];
      let testDownload = this.expected[i];
      do_print("Checking download source " + download.source.url +
               " with target " + download.target.path);
      if (!areEqual(download.succeeded, !!testDownload.succeeded) ||
          !areEqual(download.canceled, !!testDownload.canceled) ||
          !areEqual(download.hasPartialData, !!testDownload.hasPartialData) ||
          !areEqual(!!download.error, !!testDownload.error)) {
        return;
      }
      // If the above properties match, the error details should be correct.
      if (download.error) {
        if (testDownload.error.becauseSourceFailed) {
          Assert.equal(download.error.message, "History download failed.");
        }
        Assert.equal(download.error.becauseBlockedByParentalControls,
                     testDownload.error.becauseBlockedByParentalControls);
        Assert.equal(download.error.becauseBlockedByReputationCheck,
                     testDownload.error.becauseBlockedByReputationCheck);
      }
    }
    this.resolveWhenExpected();
  }
  async waitForExpected() {
    let promise = new Promise(resolve => this.resolveWhenExpected = resolve);
    this.checkForExpectedDownloads();
    await promise;
  }
}

/**
 * Tests that various operations on session and history downloads are reflected
 * by the DownloadHistoryList object, and that the order of results is correct.
 */
add_task(async function test_DownloadHistory() {
  // Clean up at the beginning and at the end of the test.
  async function cleanup() {
    await PlacesUtils.history.clear();
  }
  do_register_cleanup(cleanup);
  await cleanup();

  let testDownloads = [
    // History downloads should appear in order at the beginning of the list.
    { offset: 10, canceled: true },
    { offset: 20, succeeded: true },
    { offset: 30, error: { becauseSourceFailed: true } },
    { offset: 40, error: { becauseBlockedByParentalControls: true } },
    { offset: 50, error: { becauseBlockedByReputationCheck: true } },
    // Session downloads should show up after all the history download, in the
    // same order as they were added.
    { offset: 45, canceled: true, inSession: true },
    { offset: 35, canceled: true, hasPartialData: true, inSession: true },
    { offset: 55, succeeded: true, inSession: true },
  ];
  const NEXT_OFFSET = 60;

  let publicList = await promiseNewList();
  let allList = await Downloads.getList(Downloads.ALL);

  async function addTestDownload(properties) {
    properties.source = {
      url: httpUrl("source" + properties.offset),
      isPrivate: properties.isPrivate,
    };
    let targetFile = getTempFile(TEST_TARGET_FILE_NAME + properties.offset);
    properties.target = { path: targetFile.path };
    properties.startTime = new Date(baseDate.getTime() + properties.offset);

    let download = await Downloads.createDownload(properties);
    if (properties.inSession) {
      await allList.add(download);
    }

    if (properties.isPrivate) {
      return;
    }

    // Add the download to history using the XPCOM service, then use the
    // DownloadHistory module to save the associated metadata.
    let promiseAnnotations = waitForAnnotations(properties.source.url);
    let promiseVisit = promiseWaitForVisit(properties.source.url);
    gDownloadHistory.addDownload(Services.io.newURI(properties.source.url),
                                 null,
                                 properties.startTime.getTime() * 1000,
                                 NetUtil.newURI(targetFile));
    await promiseVisit;
    DownloadHistory.updateMetaData(download);
    await promiseAnnotations;
  }

  // Add all the test downloads to history.
  for (let properties of testDownloads) {
    await addTestDownload(properties);
  }

  // Initialize DownloadHistoryList only after having added the history and
  // session downloads, and check that they are loaded in the correct order.
  let historyList = await DownloadHistory.getList();
  let view = new TestView(testDownloads);
  await historyList.addView(view);
  await view.waitForExpected();

  // Remove a download from history and verify that the change is reflected.
  let downloadToRemove = view.expected[1];
  view.expected.splice(1, 1);
  await PlacesUtils.history.remove(downloadToRemove.source.url);
  await view.waitForExpected();

  // Add a download to history and verify it's placed before session downloads,
  // even if the start date is more recent.
  let downloadToAdd = { offset: NEXT_OFFSET, canceled: true };
  view.expected.splice(view.expected.findIndex(d => d.inSession), 0,
                       downloadToAdd);
  await addTestDownload(downloadToAdd);
  await view.waitForExpected();

  // Add a session download and verify it's placed after all session downloads,
  // even if the start date is less recent.
  let sessionDownloadToAdd = { offset: 0, inSession: true, succeeded: true };
  view.expected.push(sessionDownloadToAdd);
  await addTestDownload(sessionDownloadToAdd);
  await view.waitForExpected();

  // Add a session download for the same URI without a history entry, and verify
  // it's visible and placed after all session downloads.
  view.expected.push(sessionDownloadToAdd);
  await publicList.add(await Downloads.createDownload(sessionDownloadToAdd));
  await view.waitForExpected();

  // Create a new DownloadHistoryList that also shows private downloads. Since
  // we only have public downloads, the two lists should contain the same items.
  let allHistoryList = await DownloadHistory.getList({ type: Downloads.ALL });
  let allView = new TestView(view.expected);
  await allHistoryList.addView(allView);
  await allView.waitForExpected();

  // Add a new private download and verify it appears only on the complete list.
  let privateDownloadToAdd = { offset: NEXT_OFFSET + 10, inSession: true,
                               succeeded: true, isPrivate: true };
  allView.expected.push(privateDownloadToAdd);
  await addTestDownload(privateDownloadToAdd);
  await view.waitForExpected();
  await allView.waitForExpected();

  // Now test the maxHistoryResults parameter.
  let allHistoryList2 = await DownloadHistory.getList({ type: Downloads.ALL,
    maxHistoryResults: 3 });
  // Prepare the set of downloads to contain fewer history downloads by removing
  // the oldest ones.
  let allView2 = new TestView(allView.expected.slice(3));
  await allHistoryList2.addView(allView2);
  await allView2.waitForExpected();

  // Clear history and check that session downloads with partial data remain.
  // Private downloads are also not cleared when clearing history.
  view.expected = view.expected.filter(d => d.hasPartialData);
  allView.expected = allView.expected.filter(d => d.hasPartialData ||
                                                  d.isPrivate);
  await PlacesUtils.history.clear();
  await view.waitForExpected();
  await allView.waitForExpected();
});
