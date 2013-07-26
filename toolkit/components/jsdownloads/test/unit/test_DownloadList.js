/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the DownloadList object.
 */

"use strict";

////////////////////////////////////////////////////////////////////////////////
//// Tests

/**
 * Checks the testing mechanism used to build different download lists.
 */
add_task(function test_construction()
{
  let downloadListOne = yield promiseNewDownloadList();
  let downloadListTwo = yield promiseNewDownloadList();
  let privateDownloadListOne = yield promiseNewPrivateDownloadList();
  let privateDownloadListTwo = yield promiseNewPrivateDownloadList();

  do_check_neq(downloadListOne, downloadListTwo);
  do_check_neq(privateDownloadListOne, privateDownloadListTwo);
  do_check_neq(downloadListOne, privateDownloadListOne);
});

/**
 * Checks the methods to add and retrieve items from the list.
 */
add_task(function test_add_getAll()
{
  let list = yield promiseNewDownloadList();

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
  let list = yield promiseNewDownloadList();

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
 * Checks that views receive the download add and remove notifications, and that
 * adding and removing views works as expected.
 */
add_task(function test_notifications_add_remove()
{
  let list = yield promiseNewDownloadList();

  let downloadOne = yield promiseNewDownload();
  let downloadTwo = yield promiseNewDownload();
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
});

/**
 * Checks that views receive the download change notifications.
 */
add_task(function test_notifications_change()
{
  let list = yield promiseNewDownloadList();

  let downloadOne = yield promiseNewDownload();
  let downloadTwo = yield promiseNewDownload();
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
});

/**
 * Checks that download is removed on history expiration.
 */
add_task(function test_history_expiration()
{
  function cleanup() {
    Services.prefs.clearUserPref("places.history.expiration.max_pages");
  }
  do_register_cleanup(cleanup);

  // Set max pages to 0 to make the download expire.
  Services.prefs.setIntPref("places.history.expiration.max_pages", 0);

  // Add expirable visit for downloads.
  yield promiseAddDownloadToHistory();
  yield promiseAddDownloadToHistory(httpUrl("interruptible.txt"));

  let list = yield promiseNewDownloadList();
  let downloadOne = yield promiseNewDownload();
  let downloadTwo = yield promiseNewDownload(httpUrl("interruptible.txt"));
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

  // Start download one.
  yield downloadOne.start();

  // Start download two and then cancel it.
  downloadTwo.start();
  let promiseCanceled = downloadTwo.cancel();

  // Force a history expiration.
  let expire = Cc["@mozilla.org/places/expiration;1"]
                 .getService(Ci.nsIObserver);
  expire.observe(null, "places-debug-start-expiration", -1);

  yield deferred.promise;
  yield promiseCanceled;

  cleanup();
});

/**
 * Checks all downloads are removed after clearing history.
 */
add_task(function test_history_clear()
{
  // Add expirable visit for downloads.
  yield promiseAddDownloadToHistory();
  yield promiseAddDownloadToHistory();

  let list = yield promiseNewDownloadList();
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

  PlacesUtils.history.removeAllPages();

  yield deferred.promise;
});
