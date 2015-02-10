/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */


function run_test() {
  run_next_test();
}

add_task(function* () {
  do_load_manifest("nsDummyObserver.manifest");

  let dummyCreated = false;
  let dummyReceivedOnVisit = false;

  Services.obs.addObserver(function created() {
    Services.obs.removeObserver(created, "dummy-observer-created");
    dummyCreated = true;
  }, "dummy-observer-created", false);
  Services.obs.addObserver(function visited() {
    Services.obs.removeObserver(visited, "dummy-observer-visited");
    dummyReceivedOnVisit = true;
  }, "dummy-observer-visited", false);

  let initialObservers = PlacesUtils.history.getObservers();

  // Add a common observer, it should be invoked after the category observer.
  let notificationsPromised = new Promise((resolve, reject) => {
    PlacesUtils.history.addObserver({
      __proto__: NavHistoryObserver.prototype,
      onVisit() {
        let observers = PlacesUtils.history.getObservers();
        Assert.equal(observers.length, initialObservers.length + 1);

        // Check the common observer is the last one.
        for (let i = 0; i < initialObservers.length; ++i) {
          Assert.equal(initialObservers[i], observers[i]);
        }

        PlacesUtils.history.removeObserver(this);
        observers = PlacesUtils.history.getObservers();
        Assert.equal(observers.length, initialObservers.length);

        // Check the category observer has been invoked before this one.
        Assert.ok(dummyCreated);
        Assert.ok(dummyReceivedOnVisit);
        resolve();
      }
    }, false);
  });

  // Add a visit.
  yield PlacesTestUtils.addVisits(uri("http://typed.mozilla.org"));

  yield notificationsPromised;
});
