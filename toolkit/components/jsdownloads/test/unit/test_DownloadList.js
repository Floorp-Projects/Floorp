/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the DownloadList object.
 */

"use strict";

////////////////////////////////////////////////////////////////////////////////
//// Globals

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
      handleResult: function () {},
      handleCompletion: function handleCompletion() {
        deferred.resolve();
      }
    });
  return deferred.promise;
}

////////////////////////////////////////////////////////////////////////////////
//// Tests

/**
 * Checks the testing mechanism used to build different download lists.
 */
add_task(function test_construction()
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
add_task(function test_add_getAll()
{
  let list = yield promiseNewList();

  let downloadOne = yield promiseNewDownload();
  list.add(downloadOne);

  let itemsOne = yield list.getAll();
  do_check_eq(itemsOne.length, 1);
  do_check_eq(itemsOne[0], downloadOne);

  let downloadTwo = yield promiseNewDownload();
  list.add(downloadTwo);

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
add_task(function test_remove()
{
  let list = yield promiseNewList();

  list.add(yield promiseNewDownload());
  list.add(yield promiseNewDownload());

  let items = yield list.getAll();
  list.remove(items[0]);

  // Removing an item that was never added should not raise an error.
  list.remove(yield promiseNewDownload());

  items = yield list.getAll();
  do_check_eq(items.length, 1);
});

/**
 * Tests that the "add", "remove", and "getAll" methods on the global
 * DownloadCombinedList object combine the contents of the global DownloadList
 * objects for public and private downloads.
 */
add_task(function test_DownloadCombinedList_add_remove_getAll()
{
  let publicList = yield promiseNewList();
  let privateList = yield promiseNewList(true);
  let combinedList = yield Downloads.getList(Downloads.ALL);

  let publicDownload = yield promiseNewDownload();
  let privateDownload = yield Downloads.createDownload({
    source: { url: httpUrl("source.txt"), isPrivate: true },
    target: getTempFile(TEST_TARGET_FILE_NAME).path,
  });

  publicList.add(publicDownload);
  privateList.add(privateDownload);

  do_check_eq((yield combinedList.getAll()).length, 2);

  combinedList.remove(publicDownload);
  combinedList.remove(privateDownload);

  do_check_eq((yield combinedList.getAll()).length, 0);

  combinedList.add(publicDownload);
  combinedList.add(privateDownload);

  do_check_eq((yield publicList.getAll()).length, 1);
  do_check_eq((yield privateList.getAll()).length, 1);
  do_check_eq((yield combinedList.getAll()).length, 2);

  publicList.remove(publicDownload);
  privateList.remove(privateDownload);

  do_check_eq((yield combinedList.getAll()).length, 0);
});

/**
 * Checks that views receive the download add and remove notifications, and that
 * adding and removing views works as expected, both for a normal and a combined
 * list.
 */
add_task(function test_notifications_add_remove()
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
    list.add(downloadOne);
    list.add(downloadTwo);

    // Check that we receive add notifications for existing elements.
    let addNotifications = 0;
    let viewOne = {
      onDownloadAdded: function (aDownload) {
        // The first download to be notified should be the first that was added.
        if (addNotifications == 0) {
          do_check_eq(aDownload, downloadOne);
        } else if (addNotifications == 1) {
          do_check_eq(aDownload, downloadTwo);
        }
        addNotifications++;
      },
    };
    list.addView(viewOne);
    do_check_eq(addNotifications, 2);

    // Check that we receive add notifications for new elements.
    list.add(yield promiseNewDownload());
    do_check_eq(addNotifications, 3);

    // Check that we receive remove notifications.
    let removeNotifications = 0;
    let viewTwo = {
      onDownloadRemoved: function (aDownload) {
        do_check_eq(aDownload, downloadOne);
        removeNotifications++;
      },
    };
    list.addView(viewTwo);
    list.remove(downloadOne);
    do_check_eq(removeNotifications, 1);

    // We should not receive remove notifications after the view is removed.
    list.removeView(viewTwo);
    list.remove(downloadTwo);
    do_check_eq(removeNotifications, 1);

    // We should not receive add notifications after the view is removed.
    list.removeView(viewOne);
    list.add(yield promiseNewDownload());
    do_check_eq(addNotifications, 3);
  }
});

/**
 * Checks that views receive the download change notifications, both for a
 * normal and a combined list.
 */
add_task(function test_notifications_change()
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
    list.add(downloadOne);
    list.add(downloadTwo);

    // Check that we receive change notifications.
    let receivedOnDownloadChanged = false;
    list.addView({
      onDownloadChanged: function (aDownload) {
        do_check_eq(aDownload, downloadOne);
        receivedOnDownloadChanged = true;
      },
    });
    yield downloadOne.start();
    do_check_true(receivedOnDownloadChanged);

    // We should not receive change notifications after a download is removed.
    receivedOnDownloadChanged = false;
    list.remove(downloadTwo);
    yield downloadTwo.start();
    do_check_false(receivedOnDownloadChanged);
  }
});

/**
 * Checks that the reference to "this" is correct in the view callbacks.
 */
add_task(function test_notifications_this()
{
  let list = yield promiseNewList();

  // Check that we receive change notifications.
  let receivedOnDownloadAdded = false;
  let receivedOnDownloadChanged = false;
  let receivedOnDownloadRemoved = false;
  let view = {
    onDownloadAdded: function () {
      do_check_eq(this, view);
      receivedOnDownloadAdded = true;
    },
    onDownloadChanged: function () {
      // Only do this check once.
      if (!receivedOnDownloadChanged) {
        do_check_eq(this, view);
        receivedOnDownloadChanged = true;
      }
    },
    onDownloadRemoved: function () {
      do_check_eq(this, view);
      receivedOnDownloadRemoved = true;
    },
  };
  list.addView(view);

  let download = yield promiseNewDownload();
  list.add(download);
  yield download.start();
  list.remove(download);

  // Verify that we executed the checks.
  do_check_true(receivedOnDownloadAdded);
  do_check_true(receivedOnDownloadChanged);
  do_check_true(receivedOnDownloadRemoved);
});

/**
 * Checks that download is removed on history expiration.
 */
add_task(function test_history_expiration()
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
    onDownloadRemoved: function (aDownload) {
      if (++removeNotifications == 2) {
        deferred.resolve();
      }
    },
  };
  list.addView(downloadView);

  // Work with one finished download and one canceled download.
  yield downloadOne.start();
  downloadTwo.start();
  yield downloadTwo.cancel();

  // We must replace the visits added while executing the downloads with visits
  // that are older than 7 days, otherwise they will not be expired.
  yield promiseClearHistory();
  yield promiseExpirableDownloadVisit();
  yield promiseExpirableDownloadVisit(httpUrl("interruptible.txt"));

  // After clearing history, we can add the downloads to be removed to the list.
  list.add(downloadOne);
  list.add(downloadTwo);

  // Force a history expiration.
  let expire = Cc["@mozilla.org/places/expiration;1"]
                 .getService(Ci.nsIObserver);
  expire.observe(null, "places-debug-start-expiration", -1);

  // Wait for both downloads to be removed.
  yield deferred.promise;

  cleanup();
});

/**
 * Checks all downloads are removed after clearing history.
 */
add_task(function test_history_clear()
{
  let list = yield promiseNewList();
  let downloadOne = yield promiseNewDownload();
  let downloadTwo = yield promiseNewDownload();
  list.add(downloadOne);
  list.add(downloadTwo);

  let deferred = Promise.defer();
  let removeNotifications = 0;
  let downloadView = {
    onDownloadRemoved: function (aDownload) {
      if (++removeNotifications == 2) {
        deferred.resolve();
      }
    },
  };
  list.addView(downloadView);

  yield downloadOne.start();
  yield downloadTwo.start();

  yield promiseClearHistory();

  // Wait for the removal notifications that may still be pending.
  yield deferred.promise;
});

/**
 * Tests the removeFinished method to ensure that it only removes
 * finished downloads.
 */
add_task(function test_removeFinished()
{
  let list = yield promiseNewList();
  let downloadOne = yield promiseNewDownload();
  let downloadTwo = yield promiseNewDownload();
  let downloadThree = yield promiseNewDownload();
  let downloadFour = yield promiseNewDownload();
  list.add(downloadOne);
  list.add(downloadTwo);
  list.add(downloadThree);
  list.add(downloadFour);

  let deferred = Promise.defer();
  let removeNotifications = 0;
  let downloadView = {
    onDownloadRemoved: function (aDownload) {
      do_check_true(aDownload == downloadOne ||
                    aDownload == downloadTwo ||
                    aDownload == downloadThree);
      do_check_true(removeNotifications < 3);
      if (++removeNotifications == 3) {
        deferred.resolve();
      }
    },
  };
  list.addView(downloadView);

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
