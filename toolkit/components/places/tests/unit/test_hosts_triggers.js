/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests the validity of various triggers that add remove hosts from moz_hosts
 */

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

function isHostInMozHosts(aURI)
{
  let stmt = DBConn().createStatement(
    "SELECT host "
    + "FROM moz_hosts "
    + "WHERE host = :host"
  );
  let result = false;
  stmt.params.host = aURI.host;
  while(stmt.executeStep()) {
    if (stmt.row.host == aURI.host) {
      result = true;
      break;
    }
  }
  stmt.finalize();
  return result;
}

let urls = [{uri: NetUtil.newURI("http://visit1.mozilla.org"),
             expected: "visit1.mozilla.org",
            },
            {uri: NetUtil.newURI("http://visit2.mozilla.org"),
             expected: "visit2.mozilla.org",
            },
            {uri: NetUtil.newURI("http://www.foo.mozilla.org"),
             expected: "foo.mozilla.org",
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
                    new VisitInfo(),
                  ],
    };
    places.push(place);
  });

  XPCOMUtils.defineLazyServiceGetter(this, "gHistory",
                                     "@mozilla.org/browser/history;1",
                                     "mozIAsyncHistory");

  gHistory.updatePlaces(places, {
    handleResult: function () {
    },
    handleError: function () {
      do_throw("gHistory.updatePlaces() failed");
    },
    handleCompletion: function () {
      do_check_true(isHostInMozHosts(urls[0].uri));
      do_check_true(isHostInMozHosts(urls[1].uri));
      // strip the WWW from the url before testing...
      do_check_true(isHostInMozHosts(NetUtil.newURI("http://foo.mozilla.org")));
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
      do_check_false(isHostInMozHosts(urls[idx].uri));
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
    do_check_true(isHostInMozHosts(newUri));
    do_check_false(isHostInMozHosts(NetUtil.newURI("http://test.mozilla.org")));
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
    do_check_false(isHostInMozHosts(newUri));
    run_next_test();
  });

}

////////////////////////////////////////////////////////////////////////////////
//// Test Runner

[
  test_moz_hosts_update,
  test_remove_places,
  test_bookmark_changes,
  test_bookmark_removal,
].forEach(add_test);

function run_test()
{
  run_next_test();
}
