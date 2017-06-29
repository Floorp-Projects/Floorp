add_task(function* setup() {
  yield setupPlacesDatabase("places_v38.sqlite");
});

add_task(function* database_is_valid() {
  // Accessing the database for the first time triggers migration.
  Assert.equal(PlacesUtils.history.databaseStatus,
               PlacesUtils.history.DATABASE_STATUS_UPGRADED);

  let db = yield PlacesUtils.promiseDBConnection();
  Assert.equal((yield db.getSchemaVersion()), CURRENT_SCHEMA_VERSION);
});

add_task(function* test_select_new_fields() {
  let db = yield PlacesUtils.promiseDBConnection();
  yield db.execute(`SELECT description, preview_image_url FROM moz_places`);
  Assert.ok(true, "should be able to select description and preview_image_url");
});
