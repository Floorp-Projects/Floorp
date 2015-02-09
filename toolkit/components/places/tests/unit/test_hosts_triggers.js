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

function isHostInMozPlaces(aURI)
{
  let stmt = DBConn().createStatement(
    `SELECT url
       FROM moz_places
       WHERE url = :host`
  );
  let result = false;
  stmt.params.host = aURI.spec;
  while(stmt.executeStep()) {
    if (stmt.row.url == aURI.spec) {
      result = true;
      break;
    }
  }
  stmt.finalize();
  return result;
}

function isHostInMozHosts(aURI, aTyped, aPrefix)
{
  let stmt = DBConn().createStatement(
    `SELECT host, typed, prefix
       FROM moz_hosts
       WHERE host = fixup_url(:host)
       AND frecency NOTNULL`
  );
  let result = false;
  stmt.params.host = aURI.host;
  if (stmt.executeStep()) {
    result = aTyped == stmt.row.typed && aPrefix == stmt.row.prefix;
  }
  stmt.finalize();
  return result;
}

let urls = [{uri: NetUtil.newURI("http://visit1.mozilla.org"),
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

add_task(function test_moz_hosts_update()
{
  let places = [];
  urls.forEach(function(url) {
    let place = { uri: url.uri,
                  title: "test for " + url.url,
                  transition: url.typed ? TRANSITION_TYPED : undefined };
    places.push(place);
  });

  yield PlacesTestUtils.addVisits(places);

  do_check_true(isHostInMozHosts(urls[0].uri, urls[0].typed, urls[0].prefix));
  do_check_true(isHostInMozHosts(urls[1].uri, urls[1].typed, urls[1].prefix));
  do_check_true(isHostInMozHosts(urls[2].uri, urls[2].typed, urls[2].prefix));
});

add_task(function test_remove_places()
{
  for (let idx in urls) {
    PlacesUtils.history.removePage(urls[idx].uri);
  }

  yield PlacesTestUtils.clearHistory();

  for (let idx in urls) {
    do_check_false(isHostInMozHosts(urls[idx].uri, urls[idx].typed, urls[idx].prefix));
  }
});

add_task(function test_bookmark_changes()
{
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
  do_check_true(isHostInMozHosts(newUri, false, null));
  do_check_false(isHostInMozHosts(NetUtil.newURI("http://test.mozilla.org"), false, null));
});

add_task(function test_bookmark_removal()
{
  let itemId = PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.unfiledBookmarksFolderId,
                                                    PlacesUtils.bookmarks.DEFAULT_INDEX);
  let newUri = NetUtil.newURI(NEW_URL);
  PlacesUtils.bookmarks.removeItem(itemId);
  yield PlacesTestUtils.clearHistory();

  do_check_false(isHostInMozHosts(newUri, false, null));
});

add_task(function test_moz_hosts_typed_update()
{
  const TEST_URI = NetUtil.newURI("http://typed.mozilla.com");
  let places = [{ uri: TEST_URI
                , title: "test for " + TEST_URI.spec
                },
                { uri: TEST_URI
                , title: "test for " + TEST_URI.spec
                , transition: TRANSITION_TYPED
                }];

  yield PlacesTestUtils.addVisits(places);

  do_check_true(isHostInMozHosts(TEST_URI, true, null));
  yield PlacesTestUtils.clearHistory();
});

add_task(function test_moz_hosts_www_remove()
{
  function test_removal(aURIToRemove, aURIToKeep, aCallback) {
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
    PlacesUtils.history.removePage(aURIToRemove);
    let prefix = /www/.test(aURIToKeep.spec) ? "www." : null;
    dump_table("moz_hosts");
    dump_table("moz_places");
    do_check_true(isHostInMozHosts(aURIToKeep, true, prefix));
  }

  const TEST_URI = NetUtil.newURI("http://rem.mozilla.com");
  const TEST_WWW_URI = NetUtil.newURI("http://www.rem.mozilla.com");
  yield test_removal(TEST_URI, TEST_WWW_URI);
  yield test_removal(TEST_WWW_URI, TEST_URI);
  yield PlacesTestUtils.clearHistory();
});

add_task(function test_moz_hosts_ftp_matchall()
{
  const TEST_URI_1 = NetUtil.newURI("ftp://www.mozilla.com/");
  const TEST_URI_2 = NetUtil.newURI("ftp://mozilla.com/");

  yield PlacesTestUtils.addVisits([
    { uri: TEST_URI_1, transition: TRANSITION_TYPED },
    { uri: TEST_URI_2, transition: TRANSITION_TYPED }
  ]);

  do_check_true(isHostInMozHosts(TEST_URI_1, true, "ftp://"));
});

add_task(function test_moz_hosts_ftp_not_matchall()
{
  const TEST_URI_1 = NetUtil.newURI("http://mozilla.com/");
  const TEST_URI_2 = NetUtil.newURI("ftp://mozilla.com/");

  yield PlacesTestUtils.addVisits([
    { uri: TEST_URI_1, transition: TRANSITION_TYPED },
    { uri: TEST_URI_2, transition: TRANSITION_TYPED }
  ]);

  do_check_true(isHostInMozHosts(TEST_URI_1, true, null));
});

add_task(function test_moz_hosts_update_2()
{
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

  do_check_true(isHostInMozHosts(TEST_URI_1, true, "https://www."));
});

function run_test()
{
  run_next_test();
}
