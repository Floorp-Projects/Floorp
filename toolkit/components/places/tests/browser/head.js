Components.utils.import("resource://gre/modules/PlacesUtils.jsm");
Components.utils.import("resource://gre/modules/NetUtil.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesTestUtils",
                                  "resource://testing-common/PlacesTestUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "BrowserTestUtils",
                                  "resource://testing-common/BrowserTestUtils.jsm");

const TRANSITION_LINK = PlacesUtils.history.TRANSITION_LINK;
const TRANSITION_TYPED = PlacesUtils.history.TRANSITION_TYPED;
const TRANSITION_BOOKMARK = PlacesUtils.history.TRANSITION_BOOKMARK;
const TRANSITION_REDIRECT_PERMANENT = PlacesUtils.history.TRANSITION_REDIRECT_PERMANENT;
const TRANSITION_REDIRECT_TEMPORARY = PlacesUtils.history.TRANSITION_REDIRECT_TEMPORARY;
const TRANSITION_EMBED = PlacesUtils.history.TRANSITION_EMBED;
const TRANSITION_FRAMED_LINK = PlacesUtils.history.TRANSITION_FRAMED_LINK;
const TRANSITION_DOWNLOAD = PlacesUtils.history.TRANSITION_DOWNLOAD;

/**
 * Returns a moz_places field value for a url.
 *
 * @param aURI
 *        The URI or spec to get field for.
 * param aCallback
 *        Callback function that will get the property value.
 */
function fieldForUrl(aURI, aFieldName, aCallback)
{
  let url = aURI instanceof Ci.nsIURI ? aURI.spec : aURI;
  let stmt = PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase)
                                .DBConnection.createAsyncStatement(
    `SELECT ${aFieldName} FROM moz_places WHERE url_hash = hash(:page_url) AND url = :page_url`
  );
  stmt.params.page_url = url;
  stmt.executeAsync({
    _value: -1,
    handleResult: function(aResultSet) {
      let row = aResultSet.getNextRow();
      if (!row)
        ok(false, "The page should exist in the database");
      this._value = row.getResultByName(aFieldName);
    },
    handleError: function() {},
    handleCompletion: function(aReason) {
      if (aReason != Ci.mozIStorageStatementCallback.REASON_FINISHED)
         ok(false, "The statement should properly succeed");
      aCallback(this._value);
    }
  });
  stmt.finalize();
}

/**
 * Generic nsINavHistoryObserver that doesn't implement anything, but provides
 * dummy methods to prevent errors about an object not having a certain method.
 */
function NavHistoryObserver() {}

NavHistoryObserver.prototype = {
  onBeginUpdateBatch: function () {},
  onEndUpdateBatch: function () {},
  onVisit: function () {},
  onTitleChanged: function () {},
  onDeleteURI: function () {},
  onClearHistory: function () {},
  onPageChanged: function () {},
  onDeleteVisits: function () {},
  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsINavHistoryObserver,
  ])
};

/**
 * Waits for the first OnPageChanged notification for ATTRIBUTE_FAVICON, and
 * verifies that it matches the expected page URI and associated favicon URI.
 *
 * This function also double-checks the GUID parameter of the notification.
 *
 * @param aExpectedPageURI
 *        nsIURI object of the page whose favicon should change.
 * @param aExpectedFaviconURI
 *        nsIURI object of the newly associated favicon.
 * @param aCallback
 *        This function is called after the check finished.
 */
function waitForFaviconChanged(aExpectedPageURI, aExpectedFaviconURI, aWindow,
                               aCallback) {
  let historyObserver = {
    __proto__: NavHistoryObserver.prototype,
    onPageChanged: function WFFC_onPageChanged(aURI, aWhat, aValue, aGUID) {
      if (aWhat != Ci.nsINavHistoryObserver.ATTRIBUTE_FAVICON) {
        return;
      }
      aWindow.PlacesUtils.history.removeObserver(this);

      ok(aURI.equals(aExpectedPageURI),
        "Check URIs are equal for the page which favicon changed");
      is(aValue, aExpectedFaviconURI.spec,
        "Check changed favicon URI is the expected");
      checkGuidForURI(aURI, aGUID);

      if (aCallback) {
        aCallback();
      }
    }
  };
  aWindow.PlacesUtils.history.addObserver(historyObserver, false);
}

/**
 * Asynchronously adds visits to a page, invoking a callback function when done.
 *
 * @param aPlaceInfo
 *        Either an nsIURI, in such a case a single LINK visit will be added.
 *        Or can be an object describing the visit to add, or an array
 *        of these objects:
 *          { uri: nsIURI of the page,
 *            transition: one of the TRANSITION_* from nsINavHistoryService,
 *            [optional] title: title of the page,
 *            [optional] visitDate: visit date in microseconds from the epoch
 *            [optional] referrer: nsIURI of the referrer for this visit
 *          }
 * @param [optional] aCallback
 *        Function to be invoked on completion.
 * @param [optional] aStack
 *        The stack frame used to report errors.
 */
function addVisits(aPlaceInfo, aWindow, aCallback, aStack) {
  let stack = aStack || Components.stack.caller;
  let places = [];
  if (aPlaceInfo instanceof Ci.nsIURI) {
    places.push({ uri: aPlaceInfo });
  }
  else if (Array.isArray(aPlaceInfo)) {
    places = places.concat(aPlaceInfo);
  } else {
    places.push(aPlaceInfo)
  }

  // Create mozIVisitInfo for each entry.
  let now = Date.now();
  for (let place of places) {
    if (!place.title) {
      place.title = "test visit for " + place.uri.spec;
    }
    place.visits = [{
      transitionType: place.transition === undefined ? TRANSITION_LINK
                                                     : place.transition,
      visitDate: place.visitDate || (now++) * 1000,
      referrerURI: place.referrer
    }];
  }

  aWindow.PlacesUtils.asyncHistory.updatePlaces(
    places,
    {
      handleError: function AAV_handleError() {
        throw ("Unexpected error in adding visit.");
      },
      handleResult: function () {},
      handleCompletion: function UP_handleCompletion() {
        if (aCallback)
          aCallback();
      }
    }
  );
}

/**
 * Checks that the favicon for the given page matches the provided data.
 *
 * @param aPageURI
 *        nsIURI object for the page to check.
 * @param aExpectedMimeType
 *        Expected MIME type of the icon, for example "image/png".
 * @param aExpectedData
 *        Expected icon data, expressed as an array of byte values.
 * @param aCallback
 *        This function is called after the check finished.
 */
function checkFaviconDataForPage(aPageURI, aExpectedMimeType, aExpectedData,
  aWindow, aCallback) {
  aWindow.PlacesUtils.favicons.getFaviconDataForPage(aPageURI,
    function (aURI, aDataLen, aData, aMimeType) {
      is(aExpectedMimeType, aMimeType, "Check expected MimeType");
      is(aExpectedData.length, aData.length,
        "Check favicon data for the given page matches the provided data");
      checkGuidForURI(aPageURI);
      aCallback();
    });
}

/**
 * Tests that a guid was set in moz_places for a given uri.
 *
 * @param aURI
 *        The uri to check.
 * @param [optional] aGUID
 *        The expected guid in the database.
 */
function checkGuidForURI(aURI, aGUID) {
  let guid = doGetGuidForURI(aURI);
  if (aGUID) {
    doCheckValidPlacesGuid(aGUID);
    is(guid, aGUID, "Check equal guid for URIs");
  }
}

/**
 * Retrieves the guid for a given uri.
 *
 * @param aURI
 *        The uri to check.
 * @return the associated the guid.
 */
function doGetGuidForURI(aURI) {
  let stmt = DBConn().createStatement(
    `SELECT guid
       FROM moz_places
       WHERE url_hash = hash(:url) AND url = :url`
  );
  stmt.params.url = aURI.spec;
  ok(stmt.executeStep(), "Check get guid for uri from moz_places");
  let guid = stmt.row.guid;
  stmt.finalize();
  doCheckValidPlacesGuid(guid);
  return guid;
}

/**
 * Tests if a given guid is valid for use in Places or not.
 *
 * @param aGuid
 *        The guid to test.
 */
function doCheckValidPlacesGuid(aGuid) {
  ok(/^[a-zA-Z0-9\-_]{12}$/.test(aGuid), "Check guid for valid places");
}

/**
 * Gets the database connection.  If the Places connection is invalid it will
 * try to create a new connection.
 *
 * @param [optional] aForceNewConnection
 *        Forces creation of a new connection to the database.  When a
 *        connection is asyncClosed it cannot anymore schedule async statements,
 *        though connectionReady will keep returning true (Bug 726990).
 *
 * @return The database connection or null if unable to get one.
 */
function DBConn(aForceNewConnection) {
  let gDBConn;
  if (!aForceNewConnection) {
    let db = PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase)
      .DBConnection;
    if (db.connectionReady)
      return db;
  }

  // If the Places database connection has been closed, create a new connection.
  if (!gDBConn || aForceNewConnection) {
    let file = Services.dirsvc.get('ProfD', Ci.nsIFile);
    file.append("places.sqlite");
    let dbConn = gDBConn = Services.storage.openDatabase(file);

    // Be sure to cleanly close this connection.
    Services.obs.addObserver(function DBCloseCallback(aSubject, aTopic, aData) {
      Services.obs.removeObserver(DBCloseCallback, aTopic);
      dbConn.asyncClose();
    }, "profile-before-change", false);
  }

  return gDBConn.connectionReady ? gDBConn : null;
}

function whenNewWindowLoaded(aOptions, aCallback) {
  BrowserTestUtils.waitForNewWindow().then(aCallback);
  OpenBrowserWindow(aOptions);
}

/**
 * Asynchronously check a url is visited.
 *
 * @param aURI The URI.
 * @param aExpectedValue The expected value.
 * @return {Promise}
 * @resolves When the check has been added successfully.
 * @rejects JavaScript exception.
 */
function promiseIsURIVisited(aURI, aExpectedValue) {
  return new Promise(resolve => {
    PlacesUtils.asyncHistory.isURIVisited(aURI, function(unused, aIsVisited) {
      resolve(aIsVisited);
    });
  });
}

function waitForCondition(condition, nextTest, errorMsg) {
  let tries = 0;
  let interval = setInterval(function() {
    if (tries >= 30) {
      ok(false, errorMsg);
      moveOn();
    }
    let conditionPassed;
    try {
      conditionPassed = condition();
    } catch (e) {
      ok(false, e + "\n" + e.stack);
      conditionPassed = false;
    }
    if (conditionPassed) {
      moveOn();
    }
    tries++;
  }, 200);
  function moveOn() {
    clearInterval(interval);
    nextTest();
  }
}
