/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(function* setup() {
  yield setupPlacesDatabase("places_v25.sqlite");
});

add_task(function* database_is_valid() {
  Assert.equal(PlacesUtils.history.databaseStatus,
               PlacesUtils.history.DATABASE_STATUS_UPGRADED);

  let db = yield PlacesUtils.promiseDBConnection();
  Assert.equal((yield db.getSchemaVersion()), CURRENT_SCHEMA_VERSION);
});

add_task(function* test_dates_rounded() {
  let root = yield PlacesUtils.promiseBookmarksTree();
  function ensureDates(node) {
    // When/if promiseBookmarksTree returns these as Date objects, switch this
    // test to use getItemDateAdded and getItemLastModified.  And when these
    // methods are removed, this test can be eliminated altogether.
    Assert.strictEqual(typeof(node.dateAdded), "number");
    Assert.strictEqual(typeof(node.lastModified), "number");
    Assert.strictEqual(node.dateAdded % 1000, 0);
    Assert.strictEqual(node.lastModified % 1000, 0);
    if ("children" in node)
      node.children.forEach(ensureDates);
  }
  ensureDates(root);
});
