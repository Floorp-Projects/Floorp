ChromeUtils.import("resource://gre/modules/PlacesUtils.jsm");
ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

ChromeUtils.defineModuleGetter(this, "PlacesTestUtils",
                               "resource://testing-common/PlacesTestUtils.jsm");
ChromeUtils.defineModuleGetter(this, "BrowserTestUtils",
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
 * @param {nsIURI|String} aURI
 *        The URI or spec to get field for.
 * @param {String} aFieldName
 *        The field name to get the value of.
 * @param {Function} aCallback
 *        Callback function that will get the property value.
 */
function fieldForUrl(aURI, aFieldName, aCallback) {
  let url = aURI instanceof Ci.nsIURI ? aURI.spec : aURI;
  let stmt = PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase)
                                .DBConnection.createAsyncStatement(
    `SELECT ${aFieldName} FROM moz_places WHERE url_hash = hash(:page_url) AND url = :page_url`
  );
  stmt.params.page_url = url;
  stmt.executeAsync({
    _value: -1,
    handleResult(aResultSet) {
      let row = aResultSet.getNextRow();
      if (!row)
        ok(false, "The page should exist in the database");
      this._value = row.getResultByName(aFieldName);
    },
    handleError() {},
    handleCompletion(aReason) {
      if (aReason != Ci.mozIStorageStatementCallback.REASON_FINISHED)
         ok(false, "The statement should properly succeed");
      aCallback(this._value);
    }
  });
  stmt.finalize();
}

/**
 * Promise wrapper for fieldForUrl.
 *
 * @param {nsIURI|String} aURI
 *        The URI or spec to get field for.
 * @param {String} aFieldName
 *        The field name to get the value of.
 * @return {Promise}
 *        A promise that is resolved with the value of the field.
 */
function promiseFieldForUrl(aURI, aFieldName) {
  return new Promise(resolve => {
    function callback(result) {
      resolve(result);
    }
    fieldForUrl(aURI, aFieldName, callback);
  });
}

/**
 * Generic nsINavHistoryObserver that doesn't implement anything, but provides
 * dummy methods to prevent errors about an object not having a certain method.
 */
function NavHistoryObserver() {}

NavHistoryObserver.prototype = {
  onBeginUpdateBatch() {},
  onEndUpdateBatch() {},
  onVisits() {},
  onTitleChanged() {},
  onDeleteURI() {},
  onClearHistory() {},
  onPageChanged() {},
  onDeleteVisits() {},
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsINavHistoryObserver,
  ])
};

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
  let places = [];
  if (aPlaceInfo instanceof Ci.nsIURI) {
    places.push({ uri: aPlaceInfo });
  } else if (Array.isArray(aPlaceInfo)) {
    places = places.concat(aPlaceInfo);
  } else {
    places.push(aPlaceInfo);
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
      handleResult() {},
      handleCompletion: function UP_handleCompletion() {
        if (aCallback)
          aCallback();
      }
    }
  );
}

function whenNewWindowLoaded(aOptions, aCallback) {
  BrowserTestUtils.waitForNewWindow().then(aCallback);
  OpenBrowserWindow(aOptions);
}
