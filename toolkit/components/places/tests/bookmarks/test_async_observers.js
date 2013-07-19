/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* This test checks that bookmarks service is correctly forwarding async
 * events like visit or favicon additions. */

const NOW = Date.now() * 1000;

let observer = {
  bookmarks: [],
  observedBookmarks: 0,
  observedVisitId: 0,
  deferred: null,

  /**
   * Returns a promise that is resolved when the observer determines that the
   * test can continue.  This is required rather than calling run_next_test
   * directly in the observer because there are cases where we must wait for
   * other asynchronous events to be completed in addition to this.
   */
  setupCompletionPromise: function ()
  {
    this.observedBookmarks = 0;
    this.deferred = Promise.defer();
    return this.deferred.promise;
  },

  onBeginUpdateBatch: function () {},
  onEndUpdateBatch: function () {},
  onItemAdded: function () {},
  onItemRemoved: function () {},
  onItemMoved: function () {},
  onItemChanged: function(aItemId, aProperty, aIsAnnotation, aNewValue,
                          aLastModified, aItemType)
  {
    do_log_info("Check that we got the correct change information.");
    do_check_neq(this.bookmarks.indexOf(aItemId), -1);
    if (aProperty == "favicon") {
      do_check_false(aIsAnnotation);
      do_check_eq(aNewValue, SMALLPNG_DATA_URI.spec);
      do_check_eq(aLastModified, 0);
      do_check_eq(aItemType, PlacesUtils.bookmarks.TYPE_BOOKMARK);
    }
    else if (aProperty == "cleartime") {
      do_check_false(aIsAnnotation);
      do_check_eq(aNewValue, "");
      do_check_eq(aLastModified, 0);
      do_check_eq(aItemType, PlacesUtils.bookmarks.TYPE_BOOKMARK);
    }
    else {
      do_throw("Unexpected property change " + aProperty);
    }

    if (++this.observedBookmarks == this.bookmarks.length) {
      this.deferred.resolve();
    }
  },
  onItemVisited: function(aItemId, aVisitId, aTime)
  {
    do_log_info("Check that we got the correct visit information.");
    do_check_neq(this.bookmarks.indexOf(aItemId), -1);
    this.observedVisitId = aVisitId;
    do_check_eq(aTime, NOW);
    if (++this.observedBookmarks == this.bookmarks.length) {
      this.deferred.resolve();
    }
  },

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsINavBookmarkObserver,
  ])
};
PlacesUtils.bookmarks.addObserver(observer, false);

add_task(function test_add_visit()
{
  let observerPromise = observer.setupCompletionPromise();

  // Add a visit to the bookmark and wait for the observer.
  let visitId;
  let deferUpdatePlaces = Promise.defer();
  PlacesUtils.asyncHistory.updatePlaces({
    uri: NetUtil.newURI("http://book.ma.rk/"),
    visits: [{ transitionType: TRANSITION_TYPED, visitDate: NOW }]
  }, {
    handleError: function TAV_handleError() {
      deferUpdatePlaces.reject(new Error("Unexpected error in adding visit."));
    },
    handleResult: function (aPlaceInfo) {
      visitId = aPlaceInfo.visits[0].visitId;
    },
    handleCompletion: function TAV_handleCompletion() {
      deferUpdatePlaces.resolve();
    }
  });

  // Wait for both the observer and the asynchronous update, in any order.
  yield deferUpdatePlaces.promise;
  yield observerPromise;

  // Check that both asynchronous results are consistent.
  do_check_eq(observer.observedVisitId, visitId);
});

add_task(function test_add_icon()
{
  let observerPromise = observer.setupCompletionPromise();
  PlacesUtils.favicons.setAndFetchFaviconForPage(NetUtil.newURI("http://book.ma.rk/"),
                                                   SMALLPNG_DATA_URI, true,
                                                   PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE);
  yield observerPromise;
});

add_task(function test_remove_page()
{
  let observerPromise = observer.setupCompletionPromise();
  PlacesUtils.history.removePage(NetUtil.newURI("http://book.ma.rk/"));
  yield observerPromise;
});

add_task(function cleanup()
{
  PlacesUtils.bookmarks.removeObserver(observer, false);
});

add_task(function shutdown()
{
  // Check that async observers don't try to create async statements after
  // shutdown.  That would cause assertions, since the async thread is gone
  // already.  Note that in such a case the notifications are not fired, so we
  // cannot test for them.
  // Put an history notification that triggers AsyncGetBookmarksForURI between
  // asyncClose() and the actual connection closing.  Enqueuing a main-thread
  // event just after places-will-close-connection should ensure it runs before
  // places-connection-closed.
  // Notice this code is not using helpers cause it depends on a very specific
  // order, a change in the helpers code could make this test useless.
  let deferred = Promise.defer();

  Services.obs.addObserver(function onNotification() {
    Services.obs.removeObserver(onNotification, "places-will-close-connection");
    do_check_true(true, "Observed fake places shutdown");

    Services.tm.mainThread.dispatch(() => {
      // WARNING: this is very bad, never use out of testing code.
      PlacesUtils.bookmarks.QueryInterface(Ci.nsINavHistoryObserver)
                           .onPageChanged(NetUtil.newURI("http://book.ma.rk/"),
                                          Ci.nsINavHistoryObserver.ATTRIBUTE_FAVICON,
                                          "test", "test");
      deferred.resolve(promiseTopicObserved("places-connection-closed"));
    }, Ci.nsIThread.DISPATCH_NORMAL);
  }, "places-will-close-connection", false);
  shutdownPlaces();

  yield deferred.promise;
});

function run_test()
{
  // Add multiple bookmarks to the same uri.
  observer.bookmarks.push(
    PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                         NetUtil.newURI("http://book.ma.rk/"),
                                         PlacesUtils.bookmarks.DEFAULT_INDEX,
                                         "Bookmark")
  );
  observer.bookmarks.push(
    PlacesUtils.bookmarks.insertBookmark(PlacesUtils.toolbarFolderId,
                                         NetUtil.newURI("http://book.ma.rk/"),
                                         PlacesUtils.bookmarks.DEFAULT_INDEX,
                                         "Bookmark")
  );

  run_next_test();
}
