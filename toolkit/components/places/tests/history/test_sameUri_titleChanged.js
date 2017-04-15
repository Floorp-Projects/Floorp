// Test that repeated additions of the same URI through updatePlaces, properly
// update from_visit and notify titleChanged.

add_task(function* test() {
  let uri = "http://test.com/";

  let promiseTitleChangedNotifications = new Promise(resolve => {
    let historyObserver = {
      _count: 0,
      __proto__: NavHistoryObserver.prototype,
      onTitleChanged(aURI, aTitle, aGUID) {
        Assert.equal(aURI.spec, uri, "Should notify the proper url");
        if (++this._count == 1) {
          PlacesUtils.history.removeObserver(historyObserver);
          resolve();
        }
      }
    };
    PlacesUtils.history.addObserver(historyObserver);
  });

  // This repeats the url on purpose, don't merge it into a single place entry.
  yield PlacesTestUtils.addVisits([
    { uri, title: "test" },
    { uri, referrer: uri, title: "test2" },
  ]);

  let options = PlacesUtils.history.getNewQueryOptions();
  let query = PlacesUtils.history.getNewQuery();
  query.uri = NetUtil.newURI(uri);
  options.resultType = options.RESULTS_AS_VISIT;
  let root = PlacesUtils.history.executeQuery(query, options).root;
  root.containerOpen = true;

  Assert.equal(root.childCount, 2);

  let child = root.getChild(0);
  Assert.equal(child.visitType, TRANSITION_LINK, "Visit type should be TRANSITION_LINK");
  Assert.equal(child.visitId, 1, "Visit ID should be 1");
  Assert.equal(child.fromVisitId, -1, "Should have no referrer visit ID");
  Assert.equal(child.title, "test2", "Should have the correct title");

  child = root.getChild(1);
  Assert.equal(child.visitType, TRANSITION_LINK, "Visit type should be TRANSITION_LINK");
  Assert.equal(child.visitId, 2, "Visit ID should be 2");
  Assert.equal(child.fromVisitId, 1, "First visit should be the referring visit");
  Assert.equal(child.title, "test2", "Should have the correct title");

  root.containerOpen = false;

  yield promiseTitleChangedNotifications;
});
