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

add_task(function* test_new_fields() {
  let path = OS.Path.join(OS.Constants.Path.profileDir, DB_FILENAME);
  let db = yield Sqlite.openConnection({ path });

  // Manually update these two fields for a places record.
  yield db.execute(`
    UPDATE moz_places
    SET description = :description, preview_image_url = :previewImageURL
    WHERE id = 1`, { description: "Page description",
                     previewImageURL: "https://example.com/img.png" });
  let rows = yield db.execute(`SELECT description FROM moz_places
                               WHERE description IS NOT NULL
                               AND preview_image_url IS NOT NULL`);
  Assert.equal(rows.length, 1,
    "should fetch one record with not null description and preview_image_url");

  // Reset them to the default value
  yield db.execute(`
    UPDATE moz_places
    SET description = NULL,
        preview_image_url = NULL
    WHERE id = 1`);
  rows = yield db.execute(`SELECT description FROM moz_places
                           WHERE description IS NOT NULL
                           AND preview_image_url IS NOT NULL`);
  Assert.equal(rows.length, 0,
    "should fetch 0 record with not null description and preview_image_url");

  yield db.close();
});
