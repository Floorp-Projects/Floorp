/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  run_next_test()
}

add_task(function* test_observers() {
  do_load_manifest("nsDummyObserver.manifest");

  let dummyCreated = false;
  let dummyReceivedOnItemAdded = false;

  Services.obs.addObserver(function created() {
    Services.obs.removeObserver(created, "dummy-observer-created");
    dummyCreated = true;
  }, "dummy-observer-created", false);
  Services.obs.addObserver(function added() {
    Services.obs.removeObserver(added, "dummy-observer-item-added");
    dummyReceivedOnItemAdded = true;
  }, "dummy-observer-item-added", false);

  let initialObservers = PlacesUtils.bookmarks.getObservers();

  // Add a common observer, it should be invoked after the category observer.
  let notificationsPromised = new Promise((resolve, reject) => {
    PlacesUtils.bookmarks.addObserver( {
      __proto__: NavBookmarkObserver.prototype,
      onItemAdded() {
        let observers = PlacesUtils.bookmarks.getObservers();
        Assert.equal(observers.length, initialObservers.length + 1);

        // Check the common observer is the last one.
        for (let i = 0; i < initialObservers.length; ++i) {
          Assert.equal(initialObservers[i], observers[i]);
        }

        PlacesUtils.bookmarks.removeObserver(this);
        observers = PlacesUtils.bookmarks.getObservers();
        Assert.equal(observers.length, initialObservers.length);

        // Check the category observer has been invoked before this one.
        Assert.ok(dummyCreated);
        Assert.ok(dummyReceivedOnItemAdded);
        resolve();
      }
    }, false);
  });

  // Add a bookmark
  PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                       uri("http://typed.mozilla.org"),
                                       PlacesUtils.bookmarks.DEFAULT_INDEX,
                                       "bookmark");

  yield notificationsPromised;
});
