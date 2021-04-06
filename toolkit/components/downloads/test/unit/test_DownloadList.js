/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the DownloadList object.
 */

"use strict";

// Globals

/**
 * Returns a Date in the past usable to add expirable visits.
 *
 * @note Expiration ignores any visit added in the last 7 days, but it's
 *       better be safe against DST issues, by going back one day more.
 */
function getExpirableDate() {
  let dateObj = new Date();
  // Normalize to midnight
  dateObj.setHours(0);
  dateObj.setMinutes(0);
  dateObj.setSeconds(0);
  dateObj.setMilliseconds(0);
  return new Date(dateObj.getTime() - 8 * 86400000);
}

/**
 * Adds an expirable history visit for a download.
 *
 * @param aSourceUrl
 *        String containing the URI for the download source, or null to use
 *        httpUrl("source.txt").
 *
 * @return {Promise}
 * @rejects JavaScript exception.
 */
function promiseExpirableDownloadVisit(aSourceUrl) {
  return PlacesUtils.history.insert({
    url: aSourceUrl || httpUrl("source.txt"),
    visits: [
      {
        transition: PlacesUtils.history.TRANSITIONS.DOWNLOAD,
        date: getExpirableDate(),
      },
    ],
  });
}

// Tests

/**
 * Checks the testing mechanism used to build different download lists.
 */
add_task(async function test_construction() {
  let downloadListOne = await promiseNewList();
  let downloadListTwo = await promiseNewList();
  let privateDownloadListOne = await promiseNewList(true);
  let privateDownloadListTwo = await promiseNewList(true);

  Assert.notEqual(downloadListOne, downloadListTwo);
  Assert.notEqual(privateDownloadListOne, privateDownloadListTwo);
  Assert.notEqual(downloadListOne, privateDownloadListOne);
});

/**
 * Checks the methods to add and retrieve items from the list.
 */
add_task(async function test_add_getAll() {
  let list = await promiseNewList();

  let downloadOne = await promiseNewDownload();
  await list.add(downloadOne);

  let itemsOne = await list.getAll();
  Assert.equal(itemsOne.length, 1);
  Assert.equal(itemsOne[0], downloadOne);

  let downloadTwo = await promiseNewDownload();
  await list.add(downloadTwo);

  let itemsTwo = await list.getAll();
  Assert.equal(itemsTwo.length, 2);
  Assert.equal(itemsTwo[0], downloadOne);
  Assert.equal(itemsTwo[1], downloadTwo);

  // The first snapshot should not have been modified.
  Assert.equal(itemsOne.length, 1);
});

/**
 * Checks the method to remove items from the list.
 */
add_task(async function test_remove() {
  let list = await promiseNewList();

  await list.add(await promiseNewDownload());
  await list.add(await promiseNewDownload());

  let items = await list.getAll();
  await list.remove(items[0]);

  // Removing an item that was never added should not raise an error.
  await list.remove(await promiseNewDownload());

  items = await list.getAll();
  Assert.equal(items.length, 1);
});

/**
 * Tests that the "add", "remove", and "getAll" methods on the global
 * DownloadCombinedList object combine the contents of the global DownloadList
 * objects for public and private downloads.
 */
add_task(async function test_DownloadCombinedList_add_remove_getAll() {
  let publicList = await promiseNewList();
  let privateList = await Downloads.getList(Downloads.PRIVATE);
  let combinedList = await Downloads.getList(Downloads.ALL);

  let publicDownload = await promiseNewDownload();
  let privateDownload = await Downloads.createDownload({
    source: { url: httpUrl("source.txt"), isPrivate: true },
    target: getTempFile(TEST_TARGET_FILE_NAME).path,
  });

  await publicList.add(publicDownload);
  await privateList.add(privateDownload);

  Assert.equal((await combinedList.getAll()).length, 2);

  await combinedList.remove(publicDownload);
  await combinedList.remove(privateDownload);

  Assert.equal((await combinedList.getAll()).length, 0);

  await combinedList.add(publicDownload);
  await combinedList.add(privateDownload);

  Assert.equal((await publicList.getAll()).length, 1);
  Assert.equal((await privateList.getAll()).length, 1);
  Assert.equal((await combinedList.getAll()).length, 2);

  await publicList.remove(publicDownload);
  await privateList.remove(privateDownload);

  Assert.equal((await combinedList.getAll()).length, 0);
});

/**
 * Checks that views receive the download add and remove notifications, and that
 * adding and removing views works as expected, both for a normal and a combined
 * list.
 */
add_task(async function test_notifications_add_remove() {
  for (let isCombined of [false, true]) {
    // Force creating a new list for both the public and combined cases.
    let list = await promiseNewList();
    if (isCombined) {
      list = await Downloads.getList(Downloads.ALL);
    }

    let downloadOne = await promiseNewDownload();
    let downloadTwo = await Downloads.createDownload({
      source: { url: httpUrl("source.txt"), isPrivate: true },
      target: getTempFile(TEST_TARGET_FILE_NAME).path,
    });
    await list.add(downloadOne);
    await list.add(downloadTwo);

    // Check that we receive add notifications for existing elements.
    let addNotifications = 0;
    let viewOne = {
      onDownloadAdded(aDownload) {
        // The first download to be notified should be the first that was added.
        if (addNotifications == 0) {
          Assert.equal(aDownload, downloadOne);
        } else if (addNotifications == 1) {
          Assert.equal(aDownload, downloadTwo);
        }
        addNotifications++;
      },
    };
    await list.addView(viewOne);
    Assert.equal(addNotifications, 2);

    // Check that we receive add notifications for new elements.
    await list.add(await promiseNewDownload());
    Assert.equal(addNotifications, 3);

    // Check that we receive remove notifications.
    let removeNotifications = 0;
    let viewTwo = {
      onDownloadRemoved(aDownload) {
        Assert.equal(aDownload, downloadOne);
        removeNotifications++;
      },
    };
    await list.addView(viewTwo);
    await list.remove(downloadOne);
    Assert.equal(removeNotifications, 1);

    // We should not receive remove notifications after the view is removed.
    await list.removeView(viewTwo);
    await list.remove(downloadTwo);
    Assert.equal(removeNotifications, 1);

    // We should not receive add notifications after the view is removed.
    await list.removeView(viewOne);
    await list.add(await promiseNewDownload());
    Assert.equal(addNotifications, 3);
  }
});

/**
 * Checks that views receive the download change notifications, both for a
 * normal and a combined list.
 */
add_task(async function test_notifications_change() {
  for (let isCombined of [false, true]) {
    // Force creating a new list for both the public and combined cases.
    let list = await promiseNewList();
    if (isCombined) {
      list = await Downloads.getList(Downloads.ALL);
    }

    let downloadOne = await promiseNewDownload();
    let downloadTwo = await Downloads.createDownload({
      source: { url: httpUrl("source.txt"), isPrivate: true },
      target: getTempFile(TEST_TARGET_FILE_NAME).path,
    });
    await list.add(downloadOne);
    await list.add(downloadTwo);

    // Check that we receive change notifications.
    let receivedOnDownloadChanged = false;
    await list.addView({
      onDownloadChanged(aDownload) {
        Assert.equal(aDownload, downloadOne);
        receivedOnDownloadChanged = true;
      },
    });
    await downloadOne.start();
    Assert.ok(receivedOnDownloadChanged);

    // We should not receive change notifications after a download is removed.
    receivedOnDownloadChanged = false;
    await list.remove(downloadTwo);
    await downloadTwo.start();
    Assert.ok(!receivedOnDownloadChanged);
  }
});

/**
 * Checks that the reference to "this" is correct in the view callbacks.
 */
add_task(async function test_notifications_this() {
  let list = await promiseNewList();

  // Check that we receive change notifications.
  let receivedOnDownloadAdded = false;
  let receivedOnDownloadChanged = false;
  let receivedOnDownloadRemoved = false;
  let view = {
    onDownloadAdded() {
      Assert.equal(this, view);
      receivedOnDownloadAdded = true;
    },
    onDownloadChanged() {
      // Only do this check once.
      if (!receivedOnDownloadChanged) {
        Assert.equal(this, view);
        receivedOnDownloadChanged = true;
      }
    },
    onDownloadRemoved() {
      Assert.equal(this, view);
      receivedOnDownloadRemoved = true;
    },
  };
  await list.addView(view);

  let download = await promiseNewDownload();
  await list.add(download);
  await download.start();
  await list.remove(download);

  // Verify that we executed the checks.
  Assert.ok(receivedOnDownloadAdded);
  Assert.ok(receivedOnDownloadChanged);
  Assert.ok(receivedOnDownloadRemoved);
});

/**
 * Checks that download is removed on history expiration.
 */
add_task(async function test_history_expiration() {
  mustInterruptResponses();

  function cleanup() {
    Services.prefs.clearUserPref("places.history.expiration.max_pages");
  }
  registerCleanupFunction(cleanup);

  // Set max pages to 0 to make the download expire.
  Services.prefs.setIntPref("places.history.expiration.max_pages", 0);

  let list = await promiseNewList();
  let downloadOne = await promiseNewDownload();
  let downloadTwo = await promiseNewDownload(httpUrl("interruptible.txt"));

  let deferred = PromiseUtils.defer();
  let removeNotifications = 0;
  let downloadView = {
    onDownloadRemoved(aDownload) {
      if (++removeNotifications == 2) {
        deferred.resolve();
      }
    },
  };
  await list.addView(downloadView);

  // Work with one finished download and one canceled download.
  await downloadOne.start();
  downloadTwo.start().catch(() => {});
  await downloadTwo.cancel();

  // We must replace the visits added while executing the downloads with visits
  // that are older than 7 days, otherwise they will not be expired.
  await PlacesUtils.history.clear();
  await promiseExpirableDownloadVisit();
  await promiseExpirableDownloadVisit(httpUrl("interruptible.txt"));

  // After clearing history, we can add the downloads to be removed to the list.
  await list.add(downloadOne);
  await list.add(downloadTwo);

  // Force a history expiration.
  Cc["@mozilla.org/places/expiration;1"]
    .getService(Ci.nsIObserver)
    .observe(null, "places-debug-start-expiration", -1);

  // Wait for both downloads to be removed.
  await deferred.promise;

  cleanup();
});

/**
 * Checks all downloads are removed after clearing history.
 */
add_task(async function test_history_clear() {
  let list = await promiseNewList();
  let downloadOne = await promiseNewDownload();
  let downloadTwo = await promiseNewDownload();
  await list.add(downloadOne);
  await list.add(downloadTwo);

  let deferred = PromiseUtils.defer();
  let removeNotifications = 0;
  let downloadView = {
    onDownloadRemoved(aDownload) {
      if (++removeNotifications == 2) {
        deferred.resolve();
      }
    },
  };
  await list.addView(downloadView);

  await downloadOne.start();
  await downloadTwo.start();

  await PlacesUtils.history.clear();

  // Wait for the removal notifications that may still be pending.
  await deferred.promise;
});

/**
 * Tests the removeFinished method to ensure that it only removes
 * finished downloads.
 */
add_task(async function test_removeFinished() {
  let list = await promiseNewList();
  let downloadOne = await promiseNewDownload();
  let downloadTwo = await promiseNewDownload();
  let downloadThree = await promiseNewDownload();
  let downloadFour = await promiseNewDownload();
  await list.add(downloadOne);
  await list.add(downloadTwo);
  await list.add(downloadThree);
  await list.add(downloadFour);

  let deferred = PromiseUtils.defer();
  let removeNotifications = 0;
  let downloadView = {
    onDownloadRemoved(aDownload) {
      Assert.ok(
        aDownload == downloadOne ||
          aDownload == downloadTwo ||
          aDownload == downloadThree
      );
      Assert.ok(removeNotifications < 3);
      if (++removeNotifications == 3) {
        deferred.resolve();
      }
    },
  };
  await list.addView(downloadView);

  // Start three of the downloads, but don't start downloadTwo, then set
  // downloadFour to have partial data. All downloads except downloadFour
  // should be removed.
  await downloadOne.start();
  await downloadThree.start();
  await downloadFour.start();
  downloadFour.hasPartialData = true;

  list.removeFinished();
  await deferred.promise;

  let downloads = await list.getAll();
  Assert.equal(downloads.length, 1);
});

/**
 * Tests the global DownloadSummary objects for the public, private, and
 * combined download lists.
 */
add_task(async function test_DownloadSummary() {
  mustInterruptResponses();

  let publicList = await promiseNewList();
  let privateList = await Downloads.getList(Downloads.PRIVATE);

  let publicSummary = await Downloads.getSummary(Downloads.PUBLIC);
  let privateSummary = await Downloads.getSummary(Downloads.PRIVATE);
  let combinedSummary = await Downloads.getSummary(Downloads.ALL);

  // Add a public download that has succeeded.
  let succeededPublicDownload = await promiseNewDownload();
  await succeededPublicDownload.start();
  await publicList.add(succeededPublicDownload);

  // Add a public download that has been canceled midway.
  let canceledPublicDownload = await promiseNewDownload(
    httpUrl("interruptible.txt")
  );
  canceledPublicDownload.start().catch(() => {});
  await promiseDownloadMidway(canceledPublicDownload);
  await canceledPublicDownload.cancel();
  await publicList.add(canceledPublicDownload);

  // Add a public download that is in progress.
  let inProgressPublicDownload = await promiseNewDownload(
    httpUrl("interruptible.txt")
  );
  inProgressPublicDownload.start().catch(() => {});
  await promiseDownloadMidway(inProgressPublicDownload);
  await publicList.add(inProgressPublicDownload);

  // Add a public download of unknown size that is in progress.
  let inProgressSizelessPublicDownload = await promiseNewDownload(
    httpUrl("interruptible_nosize.txt")
  );
  inProgressSizelessPublicDownload.start().catch(() => {});
  await promiseDownloadStarted(inProgressSizelessPublicDownload);
  await publicList.add(inProgressSizelessPublicDownload);

  // Add a private download that is in progress.
  let inProgressPrivateDownload = await Downloads.createDownload({
    source: { url: httpUrl("interruptible.txt"), isPrivate: true },
    target: getTempFile(TEST_TARGET_FILE_NAME).path,
  });
  inProgressPrivateDownload.start().catch(() => {});
  await promiseDownloadMidway(inProgressPrivateDownload);
  await privateList.add(inProgressPrivateDownload);

  // Verify that the summary includes the total number of bytes and the
  // currently transferred bytes only for the downloads that are not stopped.
  // For simplicity, we assume that after a download is added to the list, its
  // current state is immediately propagated to the summary object, which is
  // true in the current implementation, though it is not guaranteed as all the
  // download operations may happen asynchronously.
  Assert.ok(!publicSummary.allHaveStopped);
  Assert.ok(!publicSummary.allUnknownSize);
  Assert.equal(publicSummary.progressTotalBytes, TEST_DATA_SHORT.length * 3);
  Assert.equal(publicSummary.progressCurrentBytes, TEST_DATA_SHORT.length * 2);

  Assert.ok(!privateSummary.allHaveStopped);
  Assert.ok(!privateSummary.allUnknownSize);
  Assert.equal(privateSummary.progressTotalBytes, TEST_DATA_SHORT.length * 2);
  Assert.equal(privateSummary.progressCurrentBytes, TEST_DATA_SHORT.length);

  Assert.ok(!combinedSummary.allHaveStopped);
  Assert.ok(!combinedSummary.allUnknownSize);
  Assert.equal(combinedSummary.progressTotalBytes, TEST_DATA_SHORT.length * 5);
  Assert.equal(
    combinedSummary.progressCurrentBytes,
    TEST_DATA_SHORT.length * 3
  );

  await inProgressPublicDownload.cancel();

  // Stopping the download should have excluded it from the summary, but we
  // should still have one public download (with unknown size) and also one
  // private download remaining.
  Assert.ok(!publicSummary.allHaveStopped);
  Assert.ok(publicSummary.allUnknownSize);
  Assert.equal(publicSummary.progressTotalBytes, TEST_DATA_SHORT.length);
  Assert.equal(publicSummary.progressCurrentBytes, TEST_DATA_SHORT.length);

  Assert.ok(!privateSummary.allHaveStopped);
  Assert.ok(!privateSummary.allUnknownSize);
  Assert.equal(privateSummary.progressTotalBytes, TEST_DATA_SHORT.length * 2);
  Assert.equal(privateSummary.progressCurrentBytes, TEST_DATA_SHORT.length);

  Assert.ok(!combinedSummary.allHaveStopped);
  Assert.ok(!combinedSummary.allUnknownSize);
  Assert.equal(combinedSummary.progressTotalBytes, TEST_DATA_SHORT.length * 3);
  Assert.equal(
    combinedSummary.progressCurrentBytes,
    TEST_DATA_SHORT.length * 2
  );

  await inProgressPrivateDownload.cancel();

  // Stopping the private download should have excluded it from the summary, so
  // now only the unknown size public download should remain.
  Assert.ok(!publicSummary.allHaveStopped);
  Assert.ok(publicSummary.allUnknownSize);
  Assert.equal(publicSummary.progressTotalBytes, TEST_DATA_SHORT.length);
  Assert.equal(publicSummary.progressCurrentBytes, TEST_DATA_SHORT.length);

  Assert.ok(privateSummary.allHaveStopped);
  Assert.ok(privateSummary.allUnknownSize);
  Assert.equal(privateSummary.progressTotalBytes, 0);
  Assert.equal(privateSummary.progressCurrentBytes, 0);

  Assert.ok(!combinedSummary.allHaveStopped);
  Assert.ok(combinedSummary.allUnknownSize);
  Assert.equal(combinedSummary.progressTotalBytes, TEST_DATA_SHORT.length);
  Assert.equal(combinedSummary.progressCurrentBytes, TEST_DATA_SHORT.length);

  await inProgressSizelessPublicDownload.cancel();

  // All the downloads should be stopped now.
  Assert.ok(publicSummary.allHaveStopped);
  Assert.ok(publicSummary.allUnknownSize);
  Assert.equal(publicSummary.progressTotalBytes, 0);
  Assert.equal(publicSummary.progressCurrentBytes, 0);

  Assert.ok(privateSummary.allHaveStopped);
  Assert.ok(privateSummary.allUnknownSize);
  Assert.equal(privateSummary.progressTotalBytes, 0);
  Assert.equal(privateSummary.progressCurrentBytes, 0);

  Assert.ok(combinedSummary.allHaveStopped);
  Assert.ok(combinedSummary.allUnknownSize);
  Assert.equal(combinedSummary.progressTotalBytes, 0);
  Assert.equal(combinedSummary.progressCurrentBytes, 0);
});

/**
 * Checks that views receive the summary change notification.  This is tested on
 * the combined summary when adding a public download, as we assume that if we
 * pass the test in this case we will also pass it in the others.
 */
add_task(async function test_DownloadSummary_notifications() {
  let list = await promiseNewList();
  let summary = await Downloads.getSummary(Downloads.ALL);

  let download = await promiseNewDownload();
  await list.add(download);

  // Check that we receive change notifications.
  let receivedOnSummaryChanged = false;
  await summary.addView({
    onSummaryChanged() {
      receivedOnSummaryChanged = true;
    },
  });
  await download.start();
  Assert.ok(receivedOnSummaryChanged);
});
