/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(function* invalid_input_throws() {
  Assert.throws(() => PlacesUtils.bookmarks.reorder(),
                /Invalid value for property 'guid'/);
  Assert.throws(() => PlacesUtils.bookmarks.reorder(null),
                /Invalid value for property 'guid'/);

  Assert.throws(() => PlacesUtils.bookmarks.reorder("test"),
                /Invalid value for property 'guid'/);
  Assert.throws(() => PlacesUtils.bookmarks.reorder(123),
                /Invalid value for property 'guid'/);

  Assert.throws(() => PlacesUtils.bookmarks.reorder({ guid: "test" }),
                /Invalid value for property 'guid'/);

  Assert.throws(() => PlacesUtils.bookmarks.reorder("123456789012"),
                /Must provide a sorted array of children GUIDs./);
  Assert.throws(() => PlacesUtils.bookmarks.reorder("123456789012", {}),
                /Must provide a sorted array of children GUIDs./);
  Assert.throws(() => PlacesUtils.bookmarks.reorder("123456789012", null),
                /Must provide a sorted array of children GUIDs./);
  Assert.throws(() => PlacesUtils.bookmarks.reorder("123456789012", []),
                /Must provide a sorted array of children GUIDs./);

  Assert.throws(() => PlacesUtils.bookmarks.reorder("123456789012", [ null ]),
                /Invalid GUID found in the sorted children array/);
  Assert.throws(() => PlacesUtils.bookmarks.reorder("123456789012", [ "" ]),
                /Invalid GUID found in the sorted children array/);
  Assert.throws(() => PlacesUtils.bookmarks.reorder("123456789012", [ {} ]),
                /Invalid GUID found in the sorted children array/);
  Assert.throws(() => PlacesUtils.bookmarks.reorder("123456789012", [ "012345678901", null ]),
                /Invalid GUID found in the sorted children array/);
});

add_task(function* reorder_nonexistent_guid() {
  yield Assert.rejects(PlacesUtils.bookmarks.reorder("123456789012", [ "012345678901" ]),
                       /No folder found for the provided GUID/,
                       "Should throw for nonexisting guid");
});

add_task(function* reorder() {
  let bookmarks = [
    { url: "http://example1.com/",
      parentGuid: PlacesUtils.bookmarks.unfiledGuid
    },
    { type: PlacesUtils.bookmarks.TYPE_FOLDER,
      parentGuid: PlacesUtils.bookmarks.unfiledGuid
    },
    { type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
      parentGuid: PlacesUtils.bookmarks.unfiledGuid
    },
    { url: "http://example2.com/",
      parentGuid: PlacesUtils.bookmarks.unfiledGuid
    },
    { url: "http://example3.com/",
      parentGuid: PlacesUtils.bookmarks.unfiledGuid
    }
  ];

  let sorted = [];
  for (let bm of bookmarks) {
    sorted.push(yield PlacesUtils.bookmarks.insert(bm));
  }

  // Check the initial append sorting.
  Assert.ok(sorted.every((bm, i) => bm.index == i),
            "Initial bookmarks sorting is correct");

  // Apply random sorting and run multiple tests.
  for (let t = 0; t < 4; t++) {
    sorted.sort(() => 0.5 - Math.random());
    let sortedGuids = sorted.map(child => child.guid);
    dump("Expected order: " + sortedGuids.join() + "\n");
    // Add a nonexisting guid to the array, to ensure nothing will break.
    sortedGuids.push("123456789012");
    yield PlacesUtils.bookmarks.reorder(PlacesUtils.bookmarks.unfiledGuid,
                                        sortedGuids);
    for (let i = 0; i < sorted.length; ++i) {
      let item = yield PlacesUtils.bookmarks.fetch(sorted[i].guid);
      Assert.equal(item.index, i);
    }
  }

  do_print("Test partial sorting");
  // Try a partial sorting by passing only 2 entries.
  // The unspecified entries should retain the original order.
  sorted = [ sorted[1], sorted[0] ].concat(sorted.slice(2));
  let sortedGuids = [ sorted[0].guid, sorted[1].guid ];
  dump("Expected order: " + sorted.map(b => b.guid).join() + "\n");
  yield PlacesUtils.bookmarks.reorder(PlacesUtils.bookmarks.unfiledGuid,
                                      sortedGuids);
  for (let i = 0; i < sorted.length; ++i) {
    let item = yield PlacesUtils.bookmarks.fetch(sorted[i].guid);
    Assert.equal(item.index, i);
  }

  // Use triangular numbers to detect skipped position.
  let db = yield PlacesUtils.promiseDBConnection();
  let rows = yield db.execute(
    `SELECT parent
     FROM moz_bookmarks
     GROUP BY parent
     HAVING (SUM(DISTINCT position + 1) - (count(*) * (count(*) + 1) / 2)) <> 0`);
  Assert.equal(rows.length, 0, "All the bookmarks should have consistent positions");
});

add_task(function* move_and_reorder() {
  // Start clean.
  yield PlacesUtils.bookmarks.eraseEverything();

  let bm1 = yield PlacesUtils.bookmarks.insert({
    url: "http://example1.com/",
    parentGuid: PlacesUtils.bookmarks.unfiledGuid
  });
  let f1 = yield PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid
  });
  let bm2 = yield PlacesUtils.bookmarks.insert({
    url: "http://example2.com/",
    parentGuid: f1.guid
  });
  let f2 = yield PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid
  });
  let bm3 = yield PlacesUtils.bookmarks.insert({
    url: "http://example3.com/",
    parentGuid: f2.guid
  });
  let bm4 = yield PlacesUtils.bookmarks.insert({
    url: "http://example4.com/",
    parentGuid: f2.guid
  });

  // Invert f2 children.
  // This is critical to reproduce the bug, cause it inverts the position
  // compared to the natural insertion order.
  yield PlacesUtils.bookmarks.reorder(f2.guid, [bm4.guid, bm3.guid]);

  bm1.parentGuid = f1.guid;
  bm1.index = 0;
  yield PlacesUtils.bookmarks.update(bm1);

  bm1 = yield PlacesUtils.bookmarks.fetch(bm1.guid);
  Assert.equal(bm1.index, 0);
  bm2 = yield PlacesUtils.bookmarks.fetch(bm2.guid);
  Assert.equal(bm2.index, 1);
  bm3 = yield PlacesUtils.bookmarks.fetch(bm3.guid);
  Assert.equal(bm3.index, 2);
  bm4 = yield PlacesUtils.bookmarks.fetch(bm4.guid);
  Assert.equal(bm4.index, 1);

  // No-op reorder on f1 children.
  // Nothing should change. Though, due to bug 1293365 this was causing children
  // of other folders to get messed up.
  yield PlacesUtils.bookmarks.reorder(f1.guid, [bm1.guid, bm2.guid]);

  bm1 = yield PlacesUtils.bookmarks.fetch(bm1.guid);
  Assert.equal(bm1.index, 0);
  bm2 = yield PlacesUtils.bookmarks.fetch(bm2.guid);
  Assert.equal(bm2.index, 1);
  bm3 = yield PlacesUtils.bookmarks.fetch(bm3.guid);
  Assert.equal(bm3.index, 2);
  bm4 = yield PlacesUtils.bookmarks.fetch(bm4.guid);
  Assert.equal(bm4.index, 1);
});
