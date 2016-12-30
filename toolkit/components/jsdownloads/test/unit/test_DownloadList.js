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
 * Returns a PRTime in the past usable to add expirable visits.
 *
 * @note Expiration ignores any visit added in the last 7 days, but it's
 *       better be safe against DST issues, by going back one day more.
 */
function getExpirablePRTime()
{
  let dateObj = new Date();
  // Normalize to midnight
  dateObj.setHours(0);
  dateObj.setMinutes(0);
  dateObj.setSeconds(0);
  dateObj.setMilliseconds(0);
  dateObj = new Date(dateObj.getTime() - 8 * 86400000);
  return dateObj.getTime() * 1000;
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
function promiseExpirableDownloadVisit(aSourceUrl)
{
  let deferred = Promise.defer();
  PlacesUtils.asyncHistory.updatePlaces(
    {
      uri: NetUtil.newURI(aSourceUrl || httpUrl("source.txt")),
      visits: [{
        transitionType: Ci.nsINavHistoryService.TRANSITION_DOWNLOAD,
        visitDate: getExpirablePRTime(),
      }]
    },
    {
      handleError: function handleError(aResultCode, aPlaceInfo) {
        let ex = new Components.Exception("Unexpected error in adding visits.",
                                          aResultCode);
        deferred.reject(ex);
      },
      handleResult() {},
      handleCompletion: function handleCompletion() {
        deferred.resolve();
      }
    });
  return deferred.promise;
}

// Tests

/**
 * Checks the testing mechanism used to build different download lists.
 */
add_task(function* test_construction()
{
  let downloadListOne = yield promiseNewList();
  let downloadListTwo = yield promiseNewList();
  let privateDownloadListOne = yield promiseNewList(true);
  let privateDownloadListTwo = yield promiseNewList(true);

  do_check_neq(downloadListOne, downloadListTwo);
  do_check_neq(privateDownloadListOne, privateDownloadListTwo);
  do_check_neq(downloadListOne, privateDownloadListOne);
});

/**
 * Checks the methods to add and retrieve items from the list.
 */
add_task(function* test_add_getAll()
{
  let list = yield promiseNewList();

  let downloadOne = yield promiseNewDownload();
  yield list.add(downloadOne);

  let itemsOne = yield list.getAll();
  do_check_eq(itemsOne.length, 1);
  do_check_eq(itemsOne[0], downloadOne);

  let downloadTwo = yield promiseNewDownload();
  yield list.add(downloadTwo);

  let itemsTwo = yield list.getAll();
  do_check_eq(itemsTwo.length, 2);
  do_check_eq(itemsTwo[0], downloadOne);
  do_check_eq(itemsTwo[1], downloadTwo);

  // The first snapshot should not have been modified.
  do_check_eq(itemsOne.length, 1);
});

/**
 * Checks the method to remove items from the list.
 */
add_task(function* test_remove()
{
  let list = yield promiseNewList();

  yield list.add(yield promiseNewDownload());
  yield list.add(yield promiseNewDownload());

  let items = yield list.getAll();
  yield list.remove(items[0]);

  // Removing an item that was never added should not raise an error.
  yield list.remove(yield promiseNewDownload());

  items = yield list.getAll();
  do_check_eq(items.length, 1);
});

/**
 * Tests that the "add", "remove", and "getAll" methods on the global
 * DownloadCombinedList object combine the contents of the global DownloadList
 * objects for public and private downloads.
 */
add_task(function* test_DownloadCombinedList_add_remove_getAll()
{
  let publicList = yield promiseNewList();
  let privateList = yield Downloads.getList(Downloads.PRIVATE);
  let combinedList = yield Downloads.getList(Downloads.ALL);

  let publicDownload = yield promiseNewDownload();
  let privateDownload = yield Downloads.createDownload({
    source: { url: httpUrl("source.txt"), isPrivate: true },
    target: getTempFile(TEST_TARGET_FILE_NAME).path,
  });

  yield publicList.add(publicDownload);
  yield privateList.add(privateDownload);

  do_check_eq((yield combinedList.getAll()).length, 2);

  yield combinedList.remove(publicDownload);
  yield combinedList.remove(privateDownload);

  do_check_eq((yield combinedList.getAll()).length, 0);

  yield combinedList.add(publicDownload);
  yield combinedList.add(privateDownload);

  do_check_eq((yield publicList.getAll()).length, 1);
  do_check_eq((yield privateList.getAll()).length, 1);
  do_check_eq((yield combinedList.getAll()).length, 2);

  yield publicList.remove(publicDownload);
  yield privateList.remove(privateDownload);

  do_check_eq((yield combinedList.getAll()).length, 0);
});

/**
 * Checks that views receive the download add and remove notifications, and that
 * adding and removing views works as expected, both for a normal and a combined
 * list.
 */
add_task(function* test_notifications_add_remove()
{
  for (let isCombined of [false, true]) {
    // Force creating a new list for both the public and combined cases.
    let list = yield promiseNewList();
    if (isCombined) {
      list = yield Downloads.getList(Downloads.ALL);
    }

    let downloadOne = yield promiseNewDownload();
    let downloadTwo = yield Downloads.createDownload({
      source: { url: httpUrl("source.txt"), isPrivate: true },
      target: getTempFile(TEST_TARGET_FILE_NAME).path,
    });
    yield list.add(downloadOne);
    yield list.add(downloadTwo);

    // Check that we receive add notifications for existing elements.
    let addNotifications = 0;
    let viewOne = {
      onDownloadAdded(aDownload) {
        // The first download to be notified should be the first that was added.
        if (addNotifications == 0) {
          do_check_eq(aDownload, downloadOne);
        } else if (addNotifications == 1) {
          do_check_eq(aDownload, downloadTwo);
        }
        addNotifications++;
      },
    };
    yield list.addView(viewOne);
    do_check_eq(addNotifications, 2);

    // Check that we receive add notifications for new elements.
    yield list.add(yield promiseNewDownload());
    do_check_eq(addNotifications, 3);

    // Check that we receive remove notifications.
    let removeNotifications = 0;
    let viewTwo = {
      onDownloadRemoved(aDownload) {
        do_check_eq(aDownload, downloadOne);
        removeNotifications++;
      },
    };
    yield list.addView(viewTwo);
    yield list.remove(downloadOne);
    do_check_eq(removeNotifications, 1);

    // We should not receive remove notifications after the view is removed.
    yield list.removeView(viewTwo);
    yield list.remove(downloadTwo);
    do_check_eq(removeNotifications, 1);

    // We should not receive add notifications after the view is removed.
    yield list.removeView(viewOne);
    yield list.add(yield promiseNewDownload());
    do_check_eq(addNotifications, 3);
  }
});

/**
 * Checks that views receive the download change notifications, both for a
 * normal and a combined list.
 */
add_task(function* test_notifications_change()
{
  for (let isCombined of [false, true]) {
    // Force creating a new list for both the public and combined cases.
    let list = yield promiseNewList();
    if (isCombined) {
      list = yield Downloads.getList(Downloads.ALL);
    }

    let downloadOne = yield promiseNewDownload();
    let downloadTwo = yield Downloads.createDownload({
      source: { url: httpUrl("source.txt"), isPrivate: true },
      target: getTempFile(TEST_TARGET_FILE_NAME).path,
    });
    yield list.add(downloadOne);
    yield list.add(downloadTwo);

    // Check that we receive change notifications.
    let receivedOnDownloadChanged = false;
    yield list.addView({
      onDownloadChanged(aDownload) {
        do_check_eq(aDownload, downloadOne);
        receivedOnDownloadChanged = true;
      },
    });
    yield downloadOne.start();
    do_check_true(receivedOnDownloadChanged);

    // We should not receive change notifications after a download is removed.
    receivedOnDownloadChanged = false;
    yield list.remove(downloadTwo);
    yield downloadTwo.start();
    do_check_false(receivedOnDownloadChanged);
  }
});

/**
 * Checks that the reference to "this" is correct in the view callbacks.
 */
add_task(function* test_notifications_this()
{
  let list = yield promiseNewList();

  // Check that we receive change notifications.
  let receivedOnDownloadAdded = false;
  let receivedOnDownloadChanged = false;
  let receivedOnDownloadRemoved = false;
  let view = {
    onDownloadAdded() {
      do_check_eq(this, view);
      receivedOnDownloadAdded = true;
    },
    onDownloadChanged() {
      // Only do this check once.
      if (!receivedOnDownloadChanged) {
        do_check_eq(this, view);
        receivedOnDownloadChanged = true;
      }
    },
    onDownloadRemoved() {
      do_check_eq(this, view);
      receivedOnDownloadRemoved = true;
    },
  };
  yield list.addView(view);

  let download = yield promiseNewDownload();
  yield list.add(download);
  yield download.start();
  yield list.remove(download);

  // Verify that we executed the checks.
  do_check_true(receivedOnDownloadAdded);
  do_check_true(receivedOnDownloadChanged);
  do_check_true(receivedOnDownloadRemoved);
});

/**
 * Checks that download is removed on history expiration.
 */
add_task(function* test_history_expiration()
{
  mustInterruptResponses();

  function cleanup() {
    Services.prefs.clearUserPref("places.history.expiration.max_pages");
  }
  do_register_cleanup(cleanup);

  // Set max pages to 0 to make the download expire.
  Services.prefs.setIntPref("places.history.expiration.max_pages", 0);

  let list = yield promiseNewList();
  let downloadOne = yield promiseNewDownload();
  let downloadTwo = yield promiseNewDownload(httpUrl("interruptible.txt"));

  let deferred = Promise.defer();
  let removeNotifications = 0;
  let downloadView = {
    onDownloadRemoved(aDownload) {
      if (++removeNotifications == 2) {
        deferred.resolve();
      }
    },
  };
  yield list.addView(downloadView);

  // Work with one finished download and one canceled download.
  yield downloadOne.start();
  downloadTwo.start().catch(() => {});
  yield downloadTwo.cancel();

  // We must replace the visits added while executing the downloads with visits
  // that are older than 7 days, otherwise they will not be expired.
  yield PlacesTestUtils.clearHistory();
  yield promiseExpirableDownloadVisit();
  yield promiseExpirableDownloadVisit(httpUrl("interruptible.txt"));

  // After clearing history, we can add the downloads to be removed to the list.
  yield list.add(downloadOne);
  yield list.add(downloadTwo);

  // Force a history expiration.
  Cc["@mozilla.org/places/expiration;1"]
    .getService(Ci.nsIObserver).observe(null, "places-debug-start-expiration", -1);

  // Wait for both downloads to be removed.
  yield deferred.promise;

  cleanup();
});

/**
 * Checks all downloads are removed after clearing history.
 */
add_task(function* test_history_clear()
{
  let list = yield promiseNewList();
  let downloadOne = yield promiseNewDownload();
  let downloadTwo = yield promiseNewDownload();
  yield list.add(downloadOne);
  yield list.add(downloadTwo);

  let deferred = Promise.defer();
  let removeNotifications = 0;
  let downloadView = {
    onDownloadRemoved(aDownload) {
      if (++removeNotifications == 2) {
        deferred.resolve();
      }
    },
  };
  yield list.addView(downloadView);

  yield downloadOne.start();
  yield downloadTwo.start();

  yield PlacesTestUtils.clearHistory();

  // Wait for the removal notifications that may still be pending.
  yield deferred.promise;
});

/**
 * Tests the removeFinished method to ensure that it only removes
 * finished downloads.
 */
add_task(function* test_removeFinished()
{
  let list = yield promiseNewList();
  let downloadOne = yield promiseNewDownload();
  let downloadTwo = yield promiseNewDownload();
  let downloadThree = yield promiseNewDownload();
  let downloadFour = yield promiseNewDownload();
  yield list.add(downloadOne);
  yield list.add(downloadTwo);
  yield list.add(downloadThree);
  yield list.add(downloadFour);

  let deferred = Promise.defer();
  let removeNotifications = 0;
  let downloadView = {
    onDownloadRemoved(aDownload) {
      do_check_true(aDownload == downloadOne ||
                    aDownload == downloadTwo ||
                    aDownload == downloadThree);
      do_check_true(removeNotifications < 3);
      if (++removeNotifications == 3) {
        deferred.resolve();
      }
    },
  };
  yield list.addView(downloadView);

  // Start three of the downloads, but don't start downloadTwo, then set
  // downloadFour to have partial data. All downloads except downloadFour
  // should be removed.
  yield downloadOne.start();
  yield downloadThree.start();
  yield downloadFour.start();
  downloadFour.hasPartialData = true;

  list.removeFinished();
  yield deferred.promise;

  let downloads = yield list.getAll()
  do_check_eq(downloads.length, 1);
});

/**
 * Tests the global DownloadSummary objects for the public, private, and
 * combined download lists.
 */
add_task(function* test_DownloadSummary()
{
  mustInterruptResponses();

  let publicList = yield promiseNewList();
  let privateList = yield Downloads.getList(Downloads.PRIVATE);

  let publicSummary = yield Downloads.getSummary(Downloads.PUBLIC);
  let privateSummary = yield Downloads.getSummary(Downloads.PRIVATE);
  let combinedSummary = yield Downloads.getSummary(Downloads.ALL);

  // Add a public download that has succeeded.
  let succeededPublicDownload = yield promiseNewDownload();
  yield succeededPublicDownload.start();
  yield publicList.add(succeededPublicDownload);

  // Add a public download that has been canceled midway.
  let canceledPublicDownload =
      yield promiseNewDownload(httpUrl("interruptible.txt"));
  canceledPublicDownload.start().catch(() => {});
  yield promiseDownloadMidway(canceledPublicDownload);
  yield canceledPublicDownload.cancel();
  yield publicList.add(canceledPublicDownload);

  // Add a public download that is in progress.
  let inProgressPublicDownload =
      yield promiseNewDownload(httpUrl("interruptible.txt"));
  inProgressPublicDownload.start().catch(() => {});
  yield promiseDownloadMidway(inProgressPublicDownload);
  yield publicList.add(inProgressPublicDownload);

  // Add a private download that is in progress.
  let inProgressPrivateDownload = yield Downloads.createDownload({
    source: { url: httpUrl("interruptible.txt"), isPrivate: true },
    target: getTempFile(TEST_TARGET_FILE_NAME).path,
  });
  inProgressPrivateDownload.start().catch(() => {});
  yield promiseDownloadMidway(inProgressPrivateDownload);
  yield privateList.add(inProgressPrivateDownload);

  // Verify that the summary includes the total number of bytes and the
  // currently transferred bytes only for the downloads that are not stopped.
  // For simplicity, we assume that after a download is added to the list, its
  // current state is immediately propagated to the summary object, which is
  // true in the current implementation, though it is not guaranteed as all the
  // download operations may happen asynchronously.
  do_check_false(publicSummary.allHaveStopped);
  do_check_eq(publicSummary.progressTotalBytes, TEST_DATA_SHORT.length * 2);
  do_check_eq(publicSummary.progressCurrentBytes, TEST_DATA_SHORT.length);

  do_check_false(privateSummary.allHaveStopped);
  do_check_eq(privateSummary.progressTotalBytes, TEST_DATA_SHORT.length * 2);
  do_check_eq(privateSummary.progressCurrentBytes, TEST_DATA_SHORT.length);

  do_check_false(combinedSummary.allHaveStopped);
  do_check_eq(combinedSummary.progressTotalBytes, TEST_DATA_SHORT.length * 4);
  do_check_eq(combinedSummary.progressCurrentBytes, TEST_DATA_SHORT.length * 2);

  yield inProgressPublicDownload.cancel();

  // Stopping the download should have excluded it from the summary.
  do_check_true(publicSummary.allHaveStopped);
  do_check_eq(publicSummary.progressTotalBytes, 0);
  do_check_eq(publicSummary.progressCurrentBytes, 0);

  do_check_false(privateSummary.allHaveStopped);
  do_check_eq(privateSummary.progressTotalBytes, TEST_DATA_SHORT.length * 2);
  do_check_eq(privateSummary.progressCurrentBytes, TEST_DATA_SHORT.length);

  do_check_false(combinedSummary.allHaveStopped);
  do_check_eq(combinedSummary.progressTotalBytes, TEST_DATA_SHORT.length * 2);
  do_check_eq(combinedSummary.progressCurrentBytes, TEST_DATA_SHORT.length);

  yield inProgressPrivateDownload.cancel();

  // All the downloads should be stopped now.
  do_check_true(publicSummary.allHaveStopped);
  do_check_eq(publicSummary.progressTotalBytes, 0);
  do_check_eq(publicSummary.progressCurrentBytes, 0);

  do_check_true(privateSummary.allHaveStopped);
  do_check_eq(privateSummary.progressTotalBytes, 0);
  do_check_eq(privateSummary.progressCurrentBytes, 0);

  do_check_true(combinedSummary.allHaveStopped);
  do_check_eq(combinedSummary.progressTotalBytes, 0);
  do_check_eq(combinedSummary.progressCurrentBytes, 0);
});

/**
 * Checks that views receive the summary change notification.  This is tested on
 * the combined summary when adding a public download, as we assume that if we
 * pass the test in this case we will also pass it in the others.
 */
add_task(function* test_DownloadSummary_notifications()
{
  let list = yield promiseNewList();
  let summary = yield Downloads.getSummary(Downloads.ALL);

  let download = yield promiseNewDownload();
  yield list.add(download);

  // Check that we receive change notifications.
  let receivedOnSummaryChanged = false;
  yield summary.addView({
    onSummaryChanged() {
      receivedOnSummaryChanged = true;
    },
  });
  yield download.start();
  do_check_true(receivedOnSummaryChanged);
});
