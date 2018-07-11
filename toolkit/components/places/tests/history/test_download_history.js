/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests the nsIDownloadHistory Places implementation.
 */

XPCOMUtils.defineLazyServiceGetter(this, "gDownloadHistory",
                                   "@mozilla.org/browser/download-history;1",
                                   "nsIDownloadHistory");

const DOWNLOAD_URI = NetUtil.newURI("http://www.example.com/");
const REFERRER_URI = NetUtil.newURI("http://www.example.org/");
const PRIVATE_URI = NetUtil.newURI("http://www.example.net/");

/**
 * Waits for the first visit notification to be received.
 *
 * @param aCallback
 *        Callback function to be called with the same arguments of onVisit.
 */
async function waitForOnVisit(aCallback) {
  await PlacesTestUtils.waitForNotification("page-visited", aEvents => {
    Assert.equal(aEvents.length, 1, "Right number of visits notified");
    Assert.equal(aEvents[0].type, "page-visited");
    let {
      url,
      visitId,
      visitTime,
      referringVisitId,
      transitionType,
      pageGuid,
      hidden,
      visitCount,
      typedCount,
      lastKnownTitle,
    } = aEvents[0];
    let uriArg = NetUtil.newURI(url);
    aCallback(uriArg, visitId, visitTime, 0, referringVisitId,
              transitionType, pageGuid, hidden, visitCount,
              typedCount, lastKnownTitle);
    return true;
  }, "places");
}

/**
 * Waits for the first onDeleteURI notification to be received.
 *
 * @param aCallback
 *        Callback function to be called with the same arguments of onDeleteURI.
 */
function waitForOnDeleteURI(aCallback) {
  let historyObserver = {
    __proto__: NavHistoryObserver.prototype,
    onDeleteURI: function HO_onDeleteURI() {
      PlacesUtils.history.removeObserver(this);
      aCallback.apply(null, arguments);
    }
  };
  PlacesUtils.history.addObserver(historyObserver);
}

/**
 * Waits for the first onDeleteVisits notification to be received.
 *
 * @param aCallback
 *        Callback function to be called with the same arguments of onDeleteVisits.
 */
function waitForOnDeleteVisits(aCallback) {
  let historyObserver = {
    __proto__: NavHistoryObserver.prototype,
    onDeleteVisits: function HO_onDeleteVisits() {
      PlacesUtils.history.removeObserver(this);
      aCallback.apply(null, arguments);
    }
  };
  PlacesUtils.history.addObserver(historyObserver);
}

add_task(async function test_dh_is_from_places() {
  // Test that this nsIDownloadHistory is the one places implements.
  Assert.ok(gDownloadHistory instanceof Ci.mozIAsyncHistory);
});


async function checkAddAndRemove(expected) {
  let visitedPromise = PlacesTestUtils.waitForNotification("page-visited",
    visits => visits[0].url == DOWNLOAD_URI.spec,
    "places");

  let pageInfo = await PlacesUtils.history.insert({
    url: DOWNLOAD_URI.spec,
    visits: [{
      date: new Date(),
      transition: PlacesUtils.history.TRANSITIONS.DOWNLOAD,
    }]
  });

  await visitedPromise;

  Assert.ok(!!page_in_database(DOWNLOAD_URI),
    "Should have added the page to the database");

  let notificationArgs;
  let removedPromise = PlacesTestUtils.waitForNotification(
    expected.deleteVisitOnly ? "onDeleteVisits" : "onDeleteURI",
    (...args) => {
      notificationArgs = args;
      return true;
    },
    "history");

  await PlacesUtils.history.removeVisitsByFilter({
    transition: PlacesUtils.history.TRANSITIONS.DOWNLOAD
  });

  await removedPromise;

  Assert.equal(notificationArgs[0].spec, DOWNLOAD_URI.spec,
    "Should have received the correct URI in the notification");

  if (expected.deleteVisitOnly) {
    Assert.equal(notificationArgs[1], expected.partialRemoval,
      "Should have received the expected partialRemoval in the notification");
    Assert.equal(notificationArgs[2], pageInfo.guid,
      "Should have received the correct GUID in the notification");
    Assert.equal(notificationArgs[3], Ci.nsINavHistoryObserver.REASON_DELETED,
      "Should have received the correct reason in the notification");
    Assert.equal(notificationArgs[4], PlacesUtils.history.TRANSITIONS.DOWNLOAD,
      "Should have received the correct transition type in the notification");
    Assert.ok(!!page_in_database(DOWNLOAD_URI),
      "Should have kept the page in the database.");
  } else {
    Assert.equal(notificationArgs[1], pageInfo.guid,
      "Should have received the correct GUID in the notification");
    Assert.equal(notificationArgs[2], Ci.nsINavHistoryObserver.REASON_DELETED,
      "Should have received the correct reason in the notification");
    Assert.ok(!page_in_database(DOWNLOAD_URI),
      "Should have removed the page from the database.");
  }
}

add_task(async function test_dh_addRemoveDownload() {
  await checkAddAndRemove({
    deleteVisitOnly: false,
    partialRemoval: false,
  });
});

add_task(async function test_dh_addMultiRemoveDownload() {
  await PlacesTestUtils.addVisits({
    uri: DOWNLOAD_URI,
    transition: TRANSITION_TYPED
  });

  await checkAddAndRemove({
    deleteVisitOnly: true,
    partialRemoval: true,
  });

  await PlacesUtils.history.clear();
});

add_task(async function test_dh_addBookmarkRemoveDownload() {
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: DOWNLOAD_URI,
    title: "A bookmark"
  });

  await checkAddAndRemove({
    deleteVisitOnly: true,
    partialRemoval: false,
  });
});

add_task(async function test_dh_addDownload_referrer() {
  // Wait for visits notification and get the visit id.
  let visitId;
  let referrerPromise = PlacesTestUtils.waitForNotification("page-visited", visits => {
    visitId = visits[0].visitId;
    let {url} = visits[0];
    return url == REFERRER_URI.spec;
  }, "places");

  await PlacesTestUtils.addVisits([{
    uri: REFERRER_URI,
    transition: Ci.nsINavHistoryService.TRANSITION_TYPED
  }]);
  await referrerPromise;

  // Verify results for referrer uri.
  Assert.ok(!!PlacesTestUtils.isPageInDB(REFERRER_URI));
  Assert.equal(visitId, 1);

  // Wait for visits notification and get the referrer Id.
  let referrerId;
  let downloadPromise = PlacesTestUtils.waitForNotification("page-visited", visits => {
    referrerId = visits[0].referringVisitId;
    let {url} = visits[0];
    return url == DOWNLOAD_URI.spec;
  }, "places");

  gDownloadHistory.addDownload(DOWNLOAD_URI, REFERRER_URI, Date.now() * 1000);
  await downloadPromise;

  // Verify results for download uri.
  // ensure that we receive the 'page-visited' notification before we call addDownload.
  Assert.ok(!!PlacesTestUtils.isPageInDB(DOWNLOAD_URI));
  Assert.equal(visitId, referrerId);

  await PlacesUtils.history.clear();
});

add_test(function test_dh_addDownload_disabledHistory() {
  waitForOnVisit(function DHAD_onVisit(aURI) {
    // We should only receive the notification for the non-private URI.  This
    // test is based on the assumption that visit notifications are received in
    // the same order of the addDownload calls, which is currently true because
    // database access is serialized on the same worker thread.
    Assert.ok(aURI.equals(DOWNLOAD_URI));

    Assert.ok(!!page_in_database(DOWNLOAD_URI));
    Assert.ok(!page_in_database(PRIVATE_URI));

    PlacesUtils.history.clear().then(run_next_test);
  });

  Services.prefs.setBoolPref("places.history.enabled", false);
  gDownloadHistory.addDownload(PRIVATE_URI, REFERRER_URI, Date.now() * 1000);

  // The addDownload functions calls CanAddURI synchronously, thus we can set
  // the preference back to true immediately (not all apps enable places by
  // default).
  Services.prefs.setBoolPref("places.history.enabled", true);
  gDownloadHistory.addDownload(DOWNLOAD_URI, REFERRER_URI, Date.now() * 1000);
});

/**
 * Tests that nsIDownloadHistory::AddDownload saves the additional download
 * details if the optional destination URL is specified.
 */
add_test(function test_dh_details() {
  const REMOTE_URI = NetUtil.newURI("http://localhost/");
  const SOURCE_URI = NetUtil.newURI("http://example.com/test_dh_details");
  const DEST_FILE_NAME = "dest.txt";

  // We must build a real, valid file URI for the destination.
  let destFileUri = NetUtil.newURI(FileUtils.getFile("TmpD", [DEST_FILE_NAME]));

  let titleSet = false;
  let destinationFileUriSet = false;

  function checkFinished() {
    if (titleSet && destinationFileUriSet) {
      PlacesUtils.annotations.removeObserver(annoObserver);
      PlacesUtils.history.removeObserver(historyObserver);

      PlacesUtils.history.clear().then(run_next_test);
    }
  }

  let annoObserver = {
    onPageAnnotationSet: function AO_onPageAnnotationSet(aPage, aName) {
      if (aPage.equals(SOURCE_URI)) {
        let value = PlacesUtils.annotations.getPageAnnotation(aPage, aName);
        switch (aName) {
          case "downloads/destinationFileURI":
            destinationFileUriSet = true;
            Assert.equal(value, destFileUri.spec);
            break;
        }
        checkFinished();
      }
    },
    onItemAnnotationSet() {},
    onPageAnnotationRemoved() {},
    onItemAnnotationRemoved() {}
  };

  let historyObserver = {
    onBeginUpdateBatch() {},
    onEndUpdateBatch() {},
    onTitleChanged: function HO_onTitleChanged(aURI, aPageTitle) {
      if (aURI.equals(SOURCE_URI)) {
        titleSet = true;
        Assert.equal(aPageTitle, DEST_FILE_NAME);
        checkFinished();
      }
    },
    onDeleteURI() {},
    onClearHistory() {},
    onPageChanged() {},
    onDeleteVisits() {}
  };

  PlacesUtils.annotations.addObserver(annoObserver);
  PlacesUtils.history.addObserver(historyObserver);

  // Both null values and remote URIs should not cause errors.
  gDownloadHistory.addDownload(SOURCE_URI, null, Date.now() * 1000);
  gDownloadHistory.addDownload(SOURCE_URI, null, Date.now() * 1000, null);
  gDownloadHistory.addDownload(SOURCE_URI, null, Date.now() * 1000, REMOTE_URI);

  // Valid local file URIs should cause the download details to be saved.
  gDownloadHistory.addDownload(SOURCE_URI, null, Date.now() * 1000,
                               destFileUri);
});
