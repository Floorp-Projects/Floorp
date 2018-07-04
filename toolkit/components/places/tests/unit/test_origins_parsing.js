/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This test is a companion to test_origins.js.  It adds many URLs to the
// database and makes sure that their prefixes and hosts are correctly parsed.
// This test can take a while to run, which is why it's split out from
// test_origins.js.

"use strict";

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
        let expectedOrigins = prefixes.map(prefix =>
          [prefix, "example.com" + port]
        );
        let uris = expectedOrigins.map(([prefix, hostPort]) =>
          prefix + userinfo + hostPort + path
        );

        await PlacesTestUtils.addVisits(uris.map(uri => ({ uri })));
        await checkDB(expectedOrigins);

        // Remove each URI, one at a time, and make sure the remaining origins
        // in the database are correct.
        for (let i = 0; i < uris.length; i++) {
          await PlacesUtils.history.remove(uris[i]);
          await checkDB(expectedOrigins.slice(i + 1, expectedOrigins.length));
        }
        await cleanUp();
      }
    }
  }
  await checkDB([]);
});


/**
 * Asserts that the moz_origins table is correct.
 *
 * @param expectedOrigins
 *        An array of expected origins.  Each origin in the array is itself an
 *        array that looks like this: [prefix, host]
 */
async function checkDB(expectedOrigins) {
  let db = await PlacesUtils.promiseDBConnection();
  let rows = await db.execute(`
    SELECT prefix, host
    FROM moz_origins
    ORDER BY id ASC
  `);
  let actualOrigins = rows.map(row => {
    let o = [];
    for (let c = 0; c < 2; c++) {
      o.push(row.getResultByIndex(c));
    }
    return o;
  });
  Assert.deepEqual(actualOrigins, expectedOrigins);
}

async function cleanUp() {
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
}
