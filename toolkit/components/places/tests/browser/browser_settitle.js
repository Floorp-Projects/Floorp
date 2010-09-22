/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

gBrowser.selectedTab = gBrowser.addTab();

function finishAndCleanUp()
{
  gBrowser.removeCurrentTab();
  waitForClearHistory(finish);
}

/**
 * One-time DOMContentLoaded callback.
 */
function load(href, callback)
{
  content.location.href = href;
  gBrowser.addEventListener("load", function() {
    if (content.location.href == href) {
      gBrowser.removeEventListener("load", arguments.callee, true);
      if (callback)
        callback();
    }
  }, true);
}

var conn = PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase).DBConnection;

/**
 * Gets a single column value from either the places or historyvisits table.
 */
function getColumn(table, column, fromColumnName, fromColumnValue)
{
  var stmt = conn.createStatement(
    "SELECT " + column + " FROM " + table + "_temp WHERE " + fromColumnName + "=:val " +
    "UNION ALL " +
    "SELECT " + column + " FROM " + table + " WHERE " + fromColumnName + "=:val " +
    "LIMIT 1");
  try {
    stmt.params.val = fromColumnValue;
    stmt.executeStep();
    return stmt.row[column];
  }
  finally {
    stmt.finalize();
  }
}

/**
 * Clears history invoking callback when done.
 */
function waitForClearHistory(aCallback) {
  const TOPIC_EXPIRATION_FINISHED = "places-expiration-finished";
  let observer = {
    observe: function(aSubject, aTopic, aData) {
      Services.obs.removeObserver(this, TOPIC_EXPIRATION_FINISHED);
      aCallback();
    }
  };
  Services.obs.addObserver(observer, TOPIC_EXPIRATION_FINISHED, false);

  let hs = Cc["@mozilla.org/browser/nav-history-service;1"].
           getService(Ci.nsINavHistoryService);
  hs.QueryInterface(Ci.nsIBrowserHistory).removeAllPages();
}

function test()
{
  // Make sure titles are correctly saved for a URI with the proper
  // notifications.

  waitForExplicitFinish();

  // Create and add history observer.
  var historyObserver = {
    data: [],
    onBeginUpdateBatch: function() {},
    onEndUpdateBatch: function() {},
    onVisit: function(aURI, aVisitID, aTime, aSessionID, aReferringID,
                      aTransitionType) {
    },
    onTitleChanged: function(aURI, aPageTitle) {
      this.data.push({ uri: aURI, title: aPageTitle });

      // We only expect one title change.
      //
      // Although we are loading two different pages, the first page does not
      // have a title.  Since the title starts out as empty and then is set
      // to empty, there is no title change notification.

      PlacesUtils.history.removeObserver(this);
      confirmResults(this.data);
    },
    onBeforeDeleteURI: function() {},
    onDeleteURI: function() {},
    onClearHistory: function() {},
    onPageChanged: function() {},
    onDeleteVisits: function() {},
    QueryInterface: XPCOMUtils.generateQI([Ci.nsINavHistoryObserver])
  };
  PlacesUtils.history.addObserver(historyObserver, false);

  load("http://example.com/tests/toolkit/components/places/tests/browser/title1.html", function() {
    load("http://example.com/tests/toolkit/components/places/tests/browser/title2.html");
  });

  function confirmResults(data) {
    is(data[0].uri.spec, "http://example.com/tests/toolkit/components/places/tests/browser/title2.html");
    is(data[0].title, "Some title");

    data.forEach(function(item) {
      var title = getColumn("moz_places", "title", "url", data[0].uri.spec);
      is(title, item.title);
    });

    finishAndCleanUp();
  }
}
