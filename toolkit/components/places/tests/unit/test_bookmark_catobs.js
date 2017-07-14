/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_observers() {
  do_load_manifest("nsDummyObserver.manifest");

  let dummyCreated = false;
  let dummyReceivedOnItemAdded = false;

  Services.obs.addObserver(function created() {
    Services.obs.removeObserver(created, "dummy-observer-created");
    dummyCreated = true;
  }, "dummy-observer-created");
  Services.obs.addObserver(function added() {
    Services.obs.removeObserver(added, "dummy-observer-item-added");
    dummyReceivedOnItemAdded = true;
  }, "dummy-observer-item-added");

  // This causes various Async API helpers to be initialised before the main
  // part of the test runs - the helpers add their own listeners, which we don't
  // want to count within the test (e.g. gKeywordsCachePromise & GuidHelper).
  await PlacesUtils.promiseItemId(PlacesUtils.bookmarks.unfiledGuid);

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
    });
  });

  // Add a bookmark
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "bookmark",
    url: "http://typed.mozilla.org",
  });

  await notificationsPromised;
});
