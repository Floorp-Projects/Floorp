/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(function* setup() {
  yield setupPlacesDatabase("places_v26.sqlite");
  // Setup database contents to be migrated.
  let path = OS.Path.join(OS.Constants.Path.profileDir, DB_FILENAME);
  let db = yield Sqlite.openConnection({ path });
  // Add pages.
  yield db.execute(`INSERT INTO moz_places (url, guid)
                    VALUES ("http://test1.com/", "test1_______")
                         , ("http://test2.com/", "test2_______")
                         , ("http://test3.com/", "test3_______")
                   `);
  // Add keywords.
  yield db.execute(`INSERT INTO moz_keywords (keyword)
                    VALUES ("kw1")
                         , ("kw2")
                         , ("kw3")
                         , ("kw4")
                         , ("kw5")
                   `);
  // Add bookmarks.
  let now = Date.now() * 1000;
  let index = 0;
  yield db.execute(`INSERT INTO moz_bookmarks (type, fk, parent, position, dateAdded, lastModified, keyword_id, guid)
                    VALUES (1, (SELECT id FROM moz_places WHERE guid = 'test1_______'), 3, ${index++}, ${now}, ${now},
                             (SELECT id FROM moz_keywords WHERE keyword = 'kw1'), "bookmark1___")
                            /* same uri, different keyword */
                         , (1, (SELECT id FROM moz_places WHERE guid = 'test1_______'), 3, ${index++}, ${now}, ${now},
                             (SELECT id FROM moz_keywords WHERE keyword = 'kw2'), "bookmark2___")
                           /* different uri, same keyword as 1 */
                         , (1, (SELECT id FROM moz_places WHERE guid = 'test2_______'), 3, ${index++}, ${now}, ${now},
                             (SELECT id FROM moz_keywords WHERE keyword = 'kw1'), "bookmark3___")
                           /* same uri, same keyword as 1 */
                         , (1, (SELECT id FROM moz_places WHERE guid = 'test1_______'), 3, ${index++}, ${now}, ${now},
                             (SELECT id FROM moz_keywords WHERE keyword = 'kw1'), "bookmark4___")
                           /* same uri, same keyword as 2 */
                         , (1, (SELECT id FROM moz_places WHERE guid = 'test2_______'), 3, ${index++}, ${now}, ${now},
                             (SELECT id FROM moz_keywords WHERE keyword = 'kw2'), "bookmark5___")
                           /* different uri, same keyword as 1 */
                         , (1, (SELECT id FROM moz_places WHERE guid = 'test1_______'), 3, ${index++}, ${now}, ${now},
                             (SELECT id FROM moz_keywords WHERE keyword = 'kw3'), "bookmark6___")
                         , (1, (SELECT id FROM moz_places WHERE guid = 'test3_______'), 3, ${index++}, ${now}, ${now},
                             (SELECT id FROM moz_keywords WHERE keyword = 'kw4'), "bookmark7___")
                         /* same uri and post_data as bookmark7, different keyword */
                         , (1, (SELECT id FROM moz_places WHERE guid = 'test3_______'), 3, ${index++}, ${now}, ${now},
                             (SELECT id FROM moz_keywords WHERE keyword = 'kw5'), "bookmark8___")
                   `);
  // Add postData.
  yield db.execute(`INSERT INTO moz_anno_attributes (name)
                    VALUES ("bookmarkProperties/POSTData")
                         , ("someOtherAnno")`);
  yield db.execute(`INSERT INTO moz_items_annos(anno_attribute_id, item_id, content)
                    VALUES ((SELECT id FROM moz_anno_attributes where name = "bookmarkProperties/POSTData"),
                            (SELECT id FROM moz_bookmarks WHERE guid = "bookmark3___"), "postData1")
                         , ((SELECT id FROM moz_anno_attributes where name = "bookmarkProperties/POSTData"),
                            (SELECT id FROM moz_bookmarks WHERE guid = "bookmark5___"), "postData2")
                         , ((SELECT id FROM moz_anno_attributes where name = "someOtherAnno"),
                            (SELECT id FROM moz_bookmarks WHERE guid = "bookmark5___"), "zzzzzzzzzz")
                         , ((SELECT id FROM moz_anno_attributes where name = "bookmarkProperties/POSTData"),
                            (SELECT id FROM moz_bookmarks WHERE guid = "bookmark7___"), "postData3")
                         , ((SELECT id FROM moz_anno_attributes where name = "bookmarkProperties/POSTData"),
                            (SELECT id FROM moz_bookmarks WHERE guid = "bookmark8___"), "postData3")
                    `);
  yield db.close();
});

add_task(function* database_is_valid() {
  Assert.equal(PlacesUtils.history.databaseStatus,
               PlacesUtils.history.DATABASE_STATUS_UPGRADED);

  let db = yield PlacesUtils.promiseDBConnection();
  Assert.equal((yield db.getSchemaVersion()), CURRENT_SCHEMA_VERSION);
});

add_task(function* test_keywords() {
  // When 2 urls have the same keyword, if one has postData it will be
  // preferred.
  let entry1 = yield PlacesUtils.keywords.fetch("kw1");
  Assert.equal(entry1.url.href, "http://test2.com/");
  Assert.equal(entry1.postData, "postData1");
  let entry2 = yield PlacesUtils.keywords.fetch("kw2");
  Assert.equal(entry2.url.href, "http://test2.com/");
  Assert.equal(entry2.postData, "postData2");
  let entry3 = yield PlacesUtils.keywords.fetch("kw3");
  Assert.equal(entry3.url.href, "http://test1.com/");
  Assert.equal(entry3.postData, null);
  let entry4 = yield PlacesUtils.keywords.fetch("kw4");
  Assert.equal(entry4, null);
  let entry5 = yield PlacesUtils.keywords.fetch("kw5");
  Assert.equal(entry5.url.href, "http://test3.com/");
  Assert.equal(entry5.postData, "postData3");

  Assert.equal((yield foreign_count("http://test1.com/")), 5); // 4 bookmark2 + 1 keywords
  Assert.equal((yield foreign_count("http://test2.com/")), 4); // 2 bookmark2 + 2 keywords
  Assert.equal((yield foreign_count("http://test3.com/")), 3); // 2 bookmark2 + 1 keywords
});
