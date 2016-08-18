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
    { type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      url: "http://example1.com/",
      parentGuid: PlacesUtils.bookmarks.unfiledGuid
    },
    { type: PlacesUtils.bookmarks.TYPE_FOLDER,
      parentGuid: PlacesUtils.bookmarks.unfiledGuid
    },
    { type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
      parentGuid: PlacesUtils.bookmarks.unfiledGuid
    },
    { type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      url: "http://example2.com/",
      parentGuid: PlacesUtils.bookmarks.unfiledGuid
    },
    { type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      url: "http://example3.com/",
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

function run_test() {
  run_next_test();
}
