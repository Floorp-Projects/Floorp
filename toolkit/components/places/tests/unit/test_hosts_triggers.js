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
    "SELECT url "
    + "FROM moz_places "
    + "WHERE url = :host"
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
    "SELECT host, typed, prefix "
    + "FROM moz_hosts "
    + "WHERE host = fixup_url(:host) "
    + "AND frecency NOTNULL "
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

function VisitInfo(aTransitionType, aVisitTime)
{
  this.transitionType =
    aTransitionType === undefined ? TRANSITION_LINK : aTransitionType;
  this.visitDate = aVisitTime || Date.now() * 1000;
}

const NEW_URL = "http://different.mozilla.org/";

function test_moz_hosts_update()
{
  let places = [];
  urls.forEach(function(url) {
    let place = {
                  uri: url.uri,
                  title: "test for " + url.url,
                  visits: [
                    new VisitInfo(url.typed ? TRANSITION_TYPED : undefined),
                  ],
    };
    places.push(place);
  });

  gHistory.updatePlaces(places, {
    handleResult: function () {
    },
    handleError: function () {
      do_throw("gHistory.updatePlaces() failed");
    },
    handleCompletion: function () {
      do_check_true(isHostInMozHosts(urls[0].uri, urls[0].typed, urls[0].prefix));
      do_check_true(isHostInMozHosts(urls[1].uri, urls[1].typed, urls[1].prefix));
      do_check_true(isHostInMozHosts(urls[2].uri, urls[2].typed, urls[2].prefix));
      run_next_test();
    }
  });
}

function test_remove_places()
{
  for (let idx in urls) {
    PlacesUtils.history.removePage(urls[idx].uri);
  }

  waitForClearHistory(function (){
    for (let idx in urls) {
      do_check_false(isHostInMozHosts(urls[idx].uri, urls[idx].typed, urls[idx].prefix));
    }
    run_next_test();
  });
}

function test_bookmark_changes()
{
  let testUri = NetUtil.newURI("http://test.mozilla.org");

  let itemId = PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                                     testUri,
                                                     PlacesUtils.bookmarks.DEFAULT_INDEX,
                                                     "bookmark title");

  do_check_true(isHostInMozPlaces(testUri));

  // Change the hostname
  PlacesUtils.bookmarks.changeBookmarkURI(itemId, NetUtil.newURI(NEW_URL));

  waitForClearHistory(function (){
    let newUri = NetUtil.newURI(NEW_URL);
    do_check_true(isHostInMozPlaces(newUri));
    do_check_true(isHostInMozHosts(newUri, false, null));
    do_check_false(isHostInMozHosts(NetUtil.newURI("http://test.mozilla.org"), false, null));
    run_next_test();
  });
}

function test_bookmark_removal()
{
  let itemId = PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.unfiledBookmarksFolderId,
                                                    PlacesUtils.bookmarks.DEFAULT_INDEX);
  let newUri = NetUtil.newURI(NEW_URL);
  PlacesUtils.bookmarks.removeItem(itemId);
  waitForClearHistory(function (){
    do_check_false(isHostInMozHosts(newUri, false, null));
    run_next_test();
  });
}

function test_moz_hosts_typed_update()
{
  const TEST_URI = NetUtil.newURI("http://typed.mozilla.com");
  let places = [{ uri: TEST_URI
                , title: "test for " + TEST_URI.spec
                , visits: [ new VisitInfo(TRANSITION_LINK)
                          , new VisitInfo(TRANSITION_TYPED)
                          ]
                }];

  gHistory.updatePlaces(places, {
    handleResult: function () {
    },
    handleError: function () {
      do_throw("gHistory.updatePlaces() failed");
    },
    handleCompletion: function () {
      do_check_true(isHostInMozHosts(TEST_URI, true, null));
      run_next_test();
    }
  });
}

function test_moz_hosts_www_remove()
{
  function test_removal(aURIToRemove, aURIToKeep, aCallback) {
    let places = [{ uri: aURIToRemove
                  , title: "test for " + aURIToRemove.spec
                  , visits: [ new VisitInfo() ]
                  },
                  { uri: aURIToKeep
                  , title: "test for " + aURIToKeep.spec
                  , visits: [ new VisitInfo() ]
                  }];

    gHistory.updatePlaces(places, {
      handleResult: function () {
      },
      handleError: function () {
        do_throw("gHistory.updatePlaces() failed");
      },
      handleCompletion: function () {
        PlacesUtils.history.removePage(aURIToRemove);
        let prefix = /www/.test(aURIToKeep.spec) ? "www." : null;
        do_check_true(isHostInMozHosts(aURIToKeep, false, prefix));
        waitForClearHistory(aCallback);
      }
    });
  }

  const TEST_URI = NetUtil.newURI("http://rem.mozilla.com");
  const TEST_WWW_URI = NetUtil.newURI("http://www.rem.mozilla.com");
  test_removal(TEST_URI, TEST_WWW_URI, function() {
    test_removal(TEST_WWW_URI, TEST_URI, function() {
      waitForClearHistory(run_next_test);
    });
  });
}

////////////////////////////////////////////////////////////////////////////////
//// Test Runner

[
  test_moz_hosts_update,
  test_remove_places,
  test_bookmark_changes,
  test_bookmark_removal,
  test_moz_hosts_typed_update,
  test_moz_hosts_www_remove,
].forEach(add_test);

function run_test()
{
  run_next_test();
}
