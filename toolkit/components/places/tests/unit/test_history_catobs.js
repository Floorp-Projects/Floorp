/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function() {
  do_load_manifest("nsDummyObserver.manifest");

  let promises = [];
  let resolved = 0;
  promises.push(TestUtils.topicObserved("dummy-observer-created", () => ++resolved));
  promises.push(TestUtils.topicObserved("dummy-observer-visited", () => ++resolved));

  let initialObservers = PlacesUtils.history.getObservers();

  // Add a common observer, it should be invoked after the category observer.
  promises.push(new Promise(resolve => {
    let observer = new NavHistoryObserver();
    observer.onVisits = visits => {
      Assert.equal(visits.length, 1, "Got the right number of visits");
      let uri = visits[0].uri;
      info("Got visit for " + uri.spec);
      let observers = PlacesUtils.history.getObservers();
      let observersCount = observers.length;
      Assert.ok(observersCount > initialObservers.length);

      // Check the common observer is the last one.
      for (let i = 0; i < initialObservers.length; ++i) {
        Assert.equal(initialObservers[i], observers[i]);
      }

      PlacesUtils.history.removeObserver(observer);
      observers = PlacesUtils.history.getObservers();
      Assert.ok(observers.length < observersCount);

      // Check the category observer has been invoked before this one.
      Assert.equal(resolved, 2);
      resolve();
    };
    PlacesUtils.history.addObserver(observer);
  }));

  info("Add a visit");
  await PlacesTestUtils.addVisits(uri("http://typed.mozilla.org"));
  await Promise.all(promises);
});
