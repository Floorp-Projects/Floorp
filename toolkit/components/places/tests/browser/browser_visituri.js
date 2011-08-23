/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

gBrowser.selectedTab = gBrowser.addTab();

function finishAndCleanUp()
{
  gBrowser.removeCurrentTab();
  waitForClearHistory(finish);
}

/**
 * One-time observer callback.
 */
function waitForObserve(name, callback)
{
  var observerService = Cc["@mozilla.org/observer-service;1"]
                        .getService(Ci.nsIObserverService);
  var observer = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),
    observe: function(subject, topic, data) {
      observerService.removeObserver(observer, name);
      observer = null;
      callback(subject, topic, data);
    }
  };

  observerService.addObserver(observer, name, false);
}

/**
 * One-time DOMContentLoaded callback.
 */
function waitForLoad(callback)
{
  gBrowser.addEventListener("DOMContentLoaded", function() {
    gBrowser.removeEventListener("DOMContentLoaded", arguments.callee, true);
    callback();
  }, true);
}

var conn = PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase).DBConnection;

/**
 * Gets a single column value from either the places or historyvisits table.
 */
function getColumn(table, column, fromColumnName, fromColumnValue)
{
  let sql = "SELECT " + column + " " +
            "FROM " + table + " " +
            "WHERE " + fromColumnName + " = :val " +
            "LIMIT 1";
  let stmt = conn.createStatement(sql);
  try {
    stmt.params.val = fromColumnValue;
    ok(stmt.executeStep(), "Expect to get a row");
    return stmt.row[column];
  }
  finally {
    stmt.reset();
  }
}

function test()
{
  // Make sure places visit chains are saved correctly with a redirect
  // transitions.

  waitForExplicitFinish();

  // Part 1: observe history events that fire when a visit occurs.
  // Make sure visits appear in order, and that the visit chain is correct.
  var expectedUrls = [
    "http://example.com/tests/toolkit/components/places/tests/browser/begin.html",
    "http://example.com/tests/toolkit/components/places/tests/browser/redirect_twice.sjs",
    "http://example.com/tests/toolkit/components/places/tests/browser/redirect_once.sjs",
    "http://example.com/tests/toolkit/components/places/tests/browser/final.html"
  ];
  var currentIndex = 0;

  waitForObserve("uri-visit-saved", function(subject, topic, data) {
    var uri = subject.QueryInterface(Ci.nsIURI);
    var expected = expectedUrls[currentIndex];
    is(uri.spec, expected, "Saved URL visit " + uri.spec);

    var placeId = getColumn("moz_places", "id", "url", uri.spec);
    var fromVisitId = getColumn("moz_historyvisits", "from_visit", "place_id", placeId);

    if (currentIndex == 0) {
      is(fromVisitId, 0, "First visit has no from visit");
    }
    else {
      var lastVisitId = getColumn("moz_historyvisits", "place_id", "id", fromVisitId);
      var fromVisitUrl = getColumn("moz_places", "url", "id", lastVisitId);
      is(fromVisitUrl, expectedUrls[currentIndex - 1],
         "From visit was " + expectedUrls[currentIndex - 1]);
    }

    currentIndex++;
    if (currentIndex < expectedUrls.length) {
      waitForObserve("uri-visit-saved", arguments.callee);
    }
    else {
      finishAndCleanUp();
    }
  });

  // Load begin page, click link on page to record visits.
  content.location.href = "http://example.com/tests/toolkit/components/places/tests/browser/begin.html";
  waitForLoad(function() {
    EventUtils.sendMouseEvent({type: 'click'}, 'clickme', content.window);
    waitForLoad(function() {
      content.location.href = "about:blank";
    });
  });
}
