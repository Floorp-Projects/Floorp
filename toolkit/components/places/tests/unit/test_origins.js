/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Makes sure the moz_origins table is updated correctly.

// Comprehensive prefix and origin parsing test.
add_task(async function parsing() {
  let prefixes = [
    "http://",
    "https://",
    "ftp://",
    "foo://",
    "bar:",
  ];

  let userinfos = [
    "",
    "user:pass@",
    "user:pass:word@",
    "user:@",
  ];

  let ports = [
    "",
    ":8888",
  ];

  let paths = [
    "",

    "/",
    "/1",
    "/1/2",

    "?",
    "?1",
    "#",
    "#1",

    "/?",
    "/1?",
    "/?1",
    "/1?2",

    "/#",
    "/1#",
    "/#1",
    "/1#2",

    "/?#",
    "/1?#",
    "/?1#",
    "/?#1",
    "/1?2#",
    "/1?#2",
    "/?1#2",
  ];

  for (let userinfo of userinfos) {
    for (let port of ports) {
      for (let path of paths) {
        info(`Testing userinfo='${userinfo}' port='${port}' path='${path}'`);
        let prefixAndHostPorts = prefixes.map(prefix =>
          [prefix, "example.com" + port]
        );
        let uris = prefixAndHostPorts.map(([prefix, hostPort]) =>
          prefix + userinfo + hostPort + path
        );

        await PlacesTestUtils.addVisits(uris.map(uri => ({ uri })));
        await checkDB(prefixAndHostPorts);

        // Remove each URI, one at a time, and make sure the remaining origins
        // in the database are correct.
        for (let i = 0; i < uris.length; i++) {
          await PlacesUtils.history.remove(uris[i]);
          await checkDB(prefixAndHostPorts.slice(i + 1,
                                                 prefixAndHostPorts.length));
        }
        await cleanUp();
      }
    }
  }
});


// Makes sure URIs with the same TLD but different www subdomains are recognized
// as different origins.  Makes sure removing one doesn't remove the others.
add_task(async function www1() {
  await PlacesTestUtils.addVisits([
    { uri: "http://example.com/" },
    { uri: "http://www.example.com/" },
    { uri: "http://www.www.example.com/" },
  ]);
  await checkDB([
    ["http://", "example.com"],
    ["http://", "www.example.com"],
    ["http://", "www.www.example.com"],
  ]);
  await PlacesUtils.history.remove("http://example.com/");
  await checkDB([
    ["http://", "www.example.com"],
    ["http://", "www.www.example.com"],
  ]);
  await PlacesUtils.history.remove("http://www.example.com/");
  await checkDB([
    ["http://", "www.www.example.com"],
  ]);
  await PlacesUtils.history.remove("http://www.www.example.com/");
  await checkDB([
  ]);
  await cleanUp();
});


// Same as www1, but removes URIs in a different order.
add_task(async function www2() {
  await PlacesTestUtils.addVisits([
    { uri: "http://example.com/" },
    { uri: "http://www.example.com/" },
    { uri: "http://www.www.example.com/" },
  ]);
  await checkDB([
    ["http://", "example.com"],
    ["http://", "www.example.com"],
    ["http://", "www.www.example.com"],
  ]);
  await PlacesUtils.history.remove("http://www.www.example.com/");
  await checkDB([
    ["http://", "example.com"],
    ["http://", "www.example.com"],
  ]);
  await PlacesUtils.history.remove("http://www.example.com/");
  await checkDB([
    ["http://", "example.com"],
  ]);
  await PlacesUtils.history.remove("http://example.com/");
  await checkDB([
  ]);
  await cleanUp();
});


// Makes sure removing an origin without a port doesn't remove the same host
// with a port.
add_task(async function ports1() {
  await PlacesTestUtils.addVisits([
    { uri: "http://example.com/" },
    { uri: "http://example.com:8888/" },
  ]);
  await checkDB([
    ["http://", "example.com"],
    ["http://", "example.com:8888"],
  ]);
  await PlacesUtils.history.remove("http://example.com/");
  await checkDB([
    ["http://", "example.com:8888"],
  ]);
  await PlacesUtils.history.remove("http://example.com:8888/");
  await checkDB([
  ]);
  await cleanUp();
});


// Makes sure removing an origin with a port doesn't remove the same host
// without a port.
add_task(async function ports2() {
  await PlacesTestUtils.addVisits([
    { uri: "http://example.com/" },
    { uri: "http://example.com:8888/" },
  ]);
  await checkDB([
    ["http://", "example.com"],
    ["http://", "example.com:8888"],
  ]);
  await PlacesUtils.history.remove("http://example.com:8888/");
  await checkDB([
    ["http://", "example.com"],
  ]);
  await PlacesUtils.history.remove("http://example.com/");
  await checkDB([
  ]);
  await cleanUp();
});


// Makes sure multiple URIs with the same origin don't create duplicate origins.
add_task(async function duplicates() {
  await PlacesTestUtils.addVisits([
    { uri: "http://example.com/" },
    { uri: "http://www.example.com/" },
    { uri: "http://www.www.example.com/" },
    { uri: "https://example.com/" },
    { uri: "ftp://example.com/" },
    { uri: "foo://example.com/" },
    { uri: "bar:example.com/" },
    { uri: "http://example.com:8888/" },

    { uri: "http://example.com/dupe" },
    { uri: "http://www.example.com/dupe" },
    { uri: "http://www.www.example.com/dupe" },
    { uri: "https://example.com/dupe" },
    { uri: "ftp://example.com/dupe" },
    { uri: "foo://example.com/dupe" },
    { uri: "bar:example.com/dupe" },
    { uri: "http://example.com:8888/dupe" },
  ]);
  await checkDB([
    ["http://", "example.com"],
    ["http://", "www.example.com"],
    ["http://", "www.www.example.com"],
    ["https://", "example.com"],
    ["ftp://", "example.com"],
    ["foo://", "example.com"],
    ["bar:", "example.com"],
    ["http://", "example.com:8888"],
  ]);

  await PlacesUtils.history.remove("http://example.com/");
  await checkDB([
    ["http://", "example.com"],
    ["http://", "www.example.com"],
    ["http://", "www.www.example.com"],
    ["https://", "example.com"],
    ["ftp://", "example.com"],
    ["foo://", "example.com"],
    ["bar:", "example.com"],
    ["http://", "example.com:8888"],
  ]);
  await PlacesUtils.history.remove("http://example.com/dupe");
  await checkDB([
    ["http://", "www.example.com"],
    ["http://", "www.www.example.com"],
    ["https://", "example.com"],
    ["ftp://", "example.com"],
    ["foo://", "example.com"],
    ["bar:", "example.com"],
    ["http://", "example.com:8888"],
  ]);

  await PlacesUtils.history.remove("http://www.example.com/");
  await checkDB([
    ["http://", "www.example.com"],
    ["http://", "www.www.example.com"],
    ["https://", "example.com"],
    ["ftp://", "example.com"],
    ["foo://", "example.com"],
    ["bar:", "example.com"],
    ["http://", "example.com:8888"],
  ]);
  await PlacesUtils.history.remove("http://www.example.com/dupe");
  await checkDB([
    ["http://", "www.www.example.com"],
    ["https://", "example.com"],
    ["ftp://", "example.com"],
    ["foo://", "example.com"],
    ["bar:", "example.com"],
    ["http://", "example.com:8888"],
  ]);

  await PlacesUtils.history.remove("http://www.www.example.com/");
  await checkDB([
    ["http://", "www.www.example.com"],
    ["https://", "example.com"],
    ["ftp://", "example.com"],
    ["foo://", "example.com"],
    ["bar:", "example.com"],
    ["http://", "example.com:8888"],
  ]);
  await PlacesUtils.history.remove("http://www.www.example.com/dupe");
  await checkDB([
    ["https://", "example.com"],
    ["ftp://", "example.com"],
    ["foo://", "example.com"],
    ["bar:", "example.com"],
    ["http://", "example.com:8888"],
  ]);

  await PlacesUtils.history.remove("https://example.com/");
  await checkDB([
    ["https://", "example.com"],
    ["ftp://", "example.com"],
    ["foo://", "example.com"],
    ["bar:", "example.com"],
    ["http://", "example.com:8888"],
  ]);
  await PlacesUtils.history.remove("https://example.com/dupe");
  await checkDB([
    ["ftp://", "example.com"],
    ["foo://", "example.com"],
    ["bar:", "example.com"],
    ["http://", "example.com:8888"],
  ]);

  await PlacesUtils.history.remove("ftp://example.com/");
  await checkDB([
    ["ftp://", "example.com"],
    ["foo://", "example.com"],
    ["bar:", "example.com"],
    ["http://", "example.com:8888"],
  ]);
  await PlacesUtils.history.remove("ftp://example.com/dupe");
  await checkDB([
    ["foo://", "example.com"],
    ["bar:", "example.com"],
    ["http://", "example.com:8888"],
  ]);

  await PlacesUtils.history.remove("foo://example.com/");
  await checkDB([
    ["foo://", "example.com"],
    ["bar:", "example.com"],
    ["http://", "example.com:8888"],
  ]);
  await PlacesUtils.history.remove("foo://example.com/dupe");
  await checkDB([
    ["bar:", "example.com"],
    ["http://", "example.com:8888"],
  ]);

  await PlacesUtils.history.remove("bar:example.com/");
  await checkDB([
    ["bar:", "example.com"],
    ["http://", "example.com:8888"],
  ]);
  await PlacesUtils.history.remove("bar:example.com/dupe");
  await checkDB([
    ["http://", "example.com:8888"],
  ]);

  await PlacesUtils.history.remove("http://example.com:8888/");
  await checkDB([
    ["http://", "example.com:8888"],
  ]);
  await PlacesUtils.history.remove("http://example.com:8888/dupe");
  await checkDB([
  ]);

  await cleanUp();
});


// Makes sure adding and removing bookmarks creates origins.
add_task(async function addRemoveBookmarks() {
  let bookmarks = [];
  let urls = [
    "http://example.com/",
    "http://www.example.com/",
  ];
  for (let url of urls) {
    bookmarks.push(await PlacesUtils.bookmarks.insert({
      url,
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    }));
  }
  await checkDB([
    ["http://", "example.com"],
    ["http://", "www.example.com"],
  ]);
  await PlacesUtils.bookmarks.remove(bookmarks[0]);
  await PlacesUtils.history.clear();
  await checkDB([
    ["http://", "www.example.com"],
  ]);
  await PlacesUtils.bookmarks.remove(bookmarks[1]);
  await PlacesUtils.history.clear();
  await checkDB([
  ]);
  await cleanUp();
});


// Makes sure changing bookmarks also changes the corresponding origins.
add_task(async function changeBookmarks() {
  let bookmarks = [];
  let urls = [
    "http://example.com/",
    "http://www.example.com/",
  ];
  for (let url of urls) {
    bookmarks.push(await PlacesUtils.bookmarks.insert({
      url,
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    }));
  }
  await checkDB([
    ["http://", "example.com"],
    ["http://", "www.example.com"],
  ]);
  await PlacesUtils.bookmarks.update({
    url: "http://www.example.com/",
    guid: bookmarks[0].guid,
  });
  await PlacesUtils.history.clear();
  await checkDB([
    ["http://", "www.example.com"],
  ]);
  await cleanUp();
});


async function checkDB(expectedOrigins) {
  let db = await PlacesUtils.promiseDBConnection();
  let rows = await db.execute(`
    SELECT prefix, host
    FROM moz_origins
    ORDER BY id ASC
  `);
  let actual = rows.map(r => [r.getString(0), r.getString(1)]);
  Assert.deepEqual(actual, expectedOrigins);
}

async function cleanUp() {
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
}
