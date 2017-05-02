/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests the validity of various triggers that add remove hosts from moz_hosts
 */

XPCOMUtils.defineLazyServiceGetter(this, "gHistory",
                                   "@mozilla.org/browser/history;1",
                                   "mozIAsyncHistory");

// add some visits and remove them, add a bookmark,
// change its uri, then remove it, and
// for each change check that moz_hosts has correctly been updated.

function isHostInMozPlaces(aURI) {
  let stmt = DBConn().createStatement(
    `SELECT url
       FROM moz_places
       WHERE url_hash = hash(:host) AND url = :host`
  );
  let result = false;
  stmt.params.host = aURI.spec;
  while (stmt.executeStep()) {
    if (stmt.row.url == aURI.spec) {
      result = true;
      break;
    }
  }
  stmt.finalize();
  return result;
}

function checkHostInMozHosts(aURI, aTyped, aPrefix, aShouldBePresent = true) {
  if (typeof aURI == "string") {
    aURI = new URL(aURI);
  }
  let stmt = DBConn().createStatement(
    `SELECT host, typed, prefix
       FROM moz_hosts
       WHERE host = fixup_url(:host)
       AND frecency NOTNULL`
  );
  let result;
  stmt.params.host = aURI.host;
  if (stmt.executeStep()) {
    result = {typed: stmt.row.typed, prefix: stmt.row.prefix};
  }
  stmt.finalize();

  if (aShouldBePresent) {
    Assert.ok(result, "Result should be defined.");
    Assert.equal(result.typed, aTyped, "The typed field should match.");
    Assert.equal(result.prefix, aPrefix, "The prefix field should match.");
  } else {
    Assert.strictEqual(result, undefined);
  }
}

function checkHostNotInMozHosts(aURI, aTyped, aPrefix) {
  checkHostInMozHosts(aURI, aTyped, aPrefix, false);
}

var urls = [{uri: NetUtil.newURI("http://visit1.mozilla.org"),
             expected: "visit1.mozilla.org",
             typed: 0,
             prefix: null
            },
            {uri: NetUtil.newURI("http://visit2.mozilla.org"),
             expected: "visit2.mozilla.org",
             typed: 0,
             prefix: null
            },
            {uri: NetUtil.newURI("http://www.foo.mozilla.org"),
             expected: "foo.mozilla.org",
             typed: 1,
             prefix: "www."
            },
           ];

const NEW_URL = "http://different.mozilla.org/";

add_task(function* test_moz_hosts_update() {
  let places = [];
  urls.forEach(function(url) {
    let place = { uri: url.uri,
                  title: "test for " + url.url,
                  transition: url.typed ? TRANSITION_TYPED : undefined };
    places.push(place);
  });

  yield PlacesTestUtils.addVisits(places);

  checkHostInMozHosts(urls[0].uri, urls[0].typed, urls[0].prefix);
  checkHostInMozHosts(urls[1].uri, urls[1].typed, urls[1].prefix);
  checkHostInMozHosts(urls[2].uri, urls[2].typed, urls[2].prefix);
});

add_task(function* test_remove_places() {
  yield PlacesUtils.history.remove(urls.map(x => x.uri));

  for (let idx in urls) {
    checkHostNotInMozHosts(urls[idx].uri, urls[idx].typed, urls[idx].prefix);
  }
});

add_task(function* test_bookmark_changes() {
  let testUri = NetUtil.newURI("http://test.mozilla.org");

  let itemId = PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                                     testUri,
                                                     PlacesUtils.bookmarks.DEFAULT_INDEX,
                                                     "bookmark title");

  do_check_true(isHostInMozPlaces(testUri));

  // Change the hostname
  PlacesUtils.bookmarks.changeBookmarkURI(itemId, NetUtil.newURI(NEW_URL));

  yield PlacesTestUtils.clearHistory();

  let newUri = NetUtil.newURI(NEW_URL);
  do_check_true(isHostInMozPlaces(newUri));
  checkHostInMozHosts(newUri, false, null);
  checkHostNotInMozHosts(NetUtil.newURI("http://test.mozilla.org"), false, null);
});

add_task(function* test_bookmark_removal() {
  let itemId = PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.unfiledBookmarksFolderId,
                                                    PlacesUtils.bookmarks.DEFAULT_INDEX);
  let newUri = NetUtil.newURI(NEW_URL);
  PlacesUtils.bookmarks.removeItem(itemId);
  yield PlacesTestUtils.clearHistory();

  checkHostNotInMozHosts(newUri, false, null);
});

add_task(function* test_moz_hosts_typed_update() {
  const TEST_URI = NetUtil.newURI("http://typed.mozilla.com");
  let places = [{ uri: TEST_URI
                , title: "test for " + TEST_URI.spec
                },
                { uri: TEST_URI
                , title: "test for " + TEST_URI.spec
                , transition: TRANSITION_TYPED
                }];

  yield PlacesTestUtils.addVisits(places);

  checkHostInMozHosts(TEST_URI, true, null);
  yield PlacesTestUtils.clearHistory();
});

add_task(function* test_moz_hosts_www_remove() {
  function* test_removal(aURIToRemove, aURIToKeep, aCallback) {
    let places = [{ uri: aURIToRemove
                  , title: "test for " + aURIToRemove.spec
                  , transition: TRANSITION_TYPED
                  },
                  { uri: aURIToKeep
                  , title: "test for " + aURIToKeep.spec
                  , transition: TRANSITION_TYPED
                  }];

    yield PlacesTestUtils.addVisits(places);
    print("removing " + aURIToRemove.spec + " keeping " + aURIToKeep);
    dump_table("moz_hosts");
    dump_table("moz_places");
    yield PlacesUtils.history.remove(aURIToRemove);
    let prefix = /www/.test(aURIToKeep.spec) ? "www." : null;
    dump_table("moz_hosts");
    dump_table("moz_places");
    checkHostInMozHosts(aURIToKeep, true, prefix);
  }

  const TEST_URI = NetUtil.newURI("http://rem.mozilla.com");
  const TEST_WWW_URI = NetUtil.newURI("http://www.rem.mozilla.com");
  yield test_removal(TEST_URI, TEST_WWW_URI);
  yield test_removal(TEST_WWW_URI, TEST_URI);
  yield PlacesTestUtils.clearHistory();
});

add_task(function* test_moz_hosts_ftp_matchall() {
  const TEST_URI_1 = NetUtil.newURI("ftp://www.mozilla.com/");
  const TEST_URI_2 = NetUtil.newURI("ftp://mozilla.com/");

  yield PlacesTestUtils.addVisits([
    { uri: TEST_URI_1, transition: TRANSITION_TYPED },
    { uri: TEST_URI_2, transition: TRANSITION_TYPED }
  ]);

  checkHostInMozHosts(TEST_URI_1, true, "ftp://");
});

add_task(function* test_moz_hosts_ftp_not_matchall() {
  const TEST_URI_1 = NetUtil.newURI("http://mozilla.com/");
  const TEST_URI_2 = NetUtil.newURI("ftp://mozilla.com/");

  yield PlacesTestUtils.addVisits([
    { uri: TEST_URI_1, transition: TRANSITION_TYPED },
    { uri: TEST_URI_2, transition: TRANSITION_TYPED }
  ]);

  checkHostInMozHosts(TEST_URI_1, true, null);
});

add_task(function* test_moz_hosts_update_2() {
  // Check that updating trigger takes into account prefixes for different
  // rev_hosts.
  const TEST_URI_1 = NetUtil.newURI("https://www.google.it/");
  const TEST_URI_2 = NetUtil.newURI("https://google.it/");
  let places = [{ uri: TEST_URI_1
                , transition: TRANSITION_TYPED
                },
                { uri: TEST_URI_2
                }];
  yield PlacesTestUtils.addVisits(places);

  checkHostInMozHosts(TEST_URI_1, true, "https://www.");
});

function getTestSection(baseURL1, baseURL2, extra) {
  let extraStr = "";
  let expectedSimplePrefix = null;
  let expectedSecurePrefix = "https://";
  if (extra) {
    extraStr = ` (${extra})`;
    expectedSimplePrefix = `${extra}.`;
    expectedSecurePrefix = `https://${extra}.`;
  }
  return [{
    title: `Test simple insecure${extraStr}`,
    visits: [{ uri: baseURL1, transition: TRANSITION_TYPED }],
    expect: [baseURL1, true, expectedSimplePrefix ]
  }, {
    title: `Test upgrade secure${extraStr}`,
    visits: [{ uri: baseURL2, transition: TRANSITION_TYPED }],
    expect: [baseURL2, true, null]
  }, {
    title: `Test remove insecure completely${extraStr}`,
    remove: baseURL1,
    expect: [baseURL2, true, expectedSecurePrefix]
  }, {
    title: `Test add more visits${extraStr}`,
    visits: [
      { uri: baseURL2, transition: TRANSITION_TYPED },
      { uri: baseURL1, transition: TRANSITION_TYPED },
    ],
    expect: [baseURL2, true, null]
  }, {
    title: `Test switch to insecure${extraStr}`,
    visits: [{ uri: baseURL1, transition: TRANSITION_TYPED }],
    expect: [baseURL2, true, null]
  }];
}

const updateTestURL1 = "http://example.com/";
const updateTestURL2 = "https://example.com/";

const hostsUpdateTests = [{
  title: "Upgrade Secure/Downgrade Insecure",
  tests: getTestSection("http://example.com", "https://example.com")
}, {
  title: "Upgrade Secure/Downgrade Insecure (www)",
  tests: getTestSection("http://www.example1.com", "https://www.example1.com", "www")
}];

add_task(function* test_moz_hosts_update() {
  for (const section of hostsUpdateTests) {
    do_print(section.title);

    for (const test of section.tests) {
      do_print(test.title);

      if ("visits" in test) {
        yield PlacesTestUtils.addVisits(test.visits);
      }
      if ("remove" in test) {
        yield PlacesUtils.history.remove(test.remove);
      }
      checkHostInMozHosts(test.expect[0], test.expect[1], test.expect[2]);
    }
  }
});
