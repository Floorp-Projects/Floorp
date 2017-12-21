const MAX_UNSIGNED_SHORT = 65535;

var gTestcases = [];
add_task(async function setup() {
  // The database has been pre-populated, since we cannot generate the necessary
  // hashes from a raw connection.
  // To add further tests, the data must be copied over to this db from an
  // existing one.
  await setupPlacesDatabase("places_v36.sqlite");
  // Fetch database contents to be migrated.
  let path = OS.Path.join(OS.Constants.Path.profileDir, DB_FILENAME);
  let db = await Sqlite.openConnection({ path });

  let rows = await db.execute(`
    SELECT h.url AS page_url, f.url AS favicon_url, mime_type, expiration, LENGTH(data) AS datalen
    FROM moz_favicons f
    LEFT JOIN moz_places h ON h.favicon_id = f.id`);
  for (let row of rows) {
    let info = {
      icon_url: row.getResultByName("favicon_url"),
      page_url: row.getResultByName("page_url"),
      mime_type: row.getResultByName("mime_type"),
      expiration: row.getResultByName("expiration"),
      has_data: row.getResultByName("mime_type") != "broken" &&
                row.getResultByName("datalen") > 0,
    };
    gTestcases.push(info);
  }
  await db.close();
});

add_task(async function database_is_valid() {
  Assert.equal(PlacesUtils.history.databaseStatus,
               PlacesUtils.history.DATABASE_STATUS_UPGRADED);

  let db = await PlacesUtils.promiseDBConnection();
  Assert.equal((await db.getSchemaVersion()), CURRENT_SCHEMA_VERSION);
});

add_task(async function test_icons() {
  let db = await PlacesUtils.promiseDBConnection();
  await Assert.rejects(db.execute(`SELECT url FROM moz_favicons`),
                       "The moz_favicons table should not exist");
  for (let entry of gTestcases) {
    info("");
    info("Checking " + entry.icon_url + " - " + entry.page_url);
    let rows = await db.execute(`SELECT id, expire_ms, width FROM moz_icons
                                 WHERE fixed_icon_url_hash = hash(fixup_url(:icon_url))
                                   AND icon_url = :icon_url
                                 `, { icon_url: entry.icon_url });
    Assert.equal(!!rows.length, entry.has_data, "icon exists");
    if (!entry.has_data) {
      // Icon not migrated.
      continue;
    }
    Assert.equal(rows[0].getResultByName("expire_ms"), 0,
                 "expiration is correct");

    let width = rows[0].getResultByName("width");
    if (entry.mime_type == "image/svg+xml") {
      Assert.equal(width, MAX_UNSIGNED_SHORT, "width is correct");
    } else {
      Assert.ok(width > 0 && (width < 16 || width % 16 == 0), "width is correct");
    }

    let icon_id = rows[0].getResultByName("id");

    rows = await db.execute(`SELECT page_id FROM moz_icons_to_pages
                             WHERE icon_id = :icon_id`, { icon_id });
    let has_relation = !!entry.page_url;
    Assert.equal(!!rows.length, has_relation, "page relations found");
    if (has_relation) {
      let page_ids = rows.map(r => r.getResultByName("page_id"));

      rows = await db.execute(`SELECT page_url FROM moz_pages_w_icons
                               WHERE id IN(${page_ids.join(",")})`);
      let urls = rows.map(r => r.getResultByName("page_url"));
      Assert.ok(urls.some(url => url == entry.page_url),
                "page_url found");
    }
  }
});
