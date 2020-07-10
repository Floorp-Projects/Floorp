var { PlacesUtils } = ChromeUtils.import(
  "resource://gre/modules/PlacesUtils.jsm"
);
var { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "PlacesTestUtils",
  "resource://testing-common/PlacesTestUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "BrowserTestUtils",
  "resource://testing-common/BrowserTestUtils.jsm"
);

const TRANSITION_LINK = PlacesUtils.history.TRANSITION_LINK;
const TRANSITION_TYPED = PlacesUtils.history.TRANSITION_TYPED;
const TRANSITION_BOOKMARK = PlacesUtils.history.TRANSITION_BOOKMARK;
const TRANSITION_REDIRECT_PERMANENT =
  PlacesUtils.history.TRANSITION_REDIRECT_PERMANENT;
const TRANSITION_REDIRECT_TEMPORARY =
  PlacesUtils.history.TRANSITION_REDIRECT_TEMPORARY;
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
  let stmt = PlacesUtils.history.DBConnection.createAsyncStatement(
    `SELECT ${aFieldName} FROM moz_places WHERE url_hash = hash(:page_url) AND url = :page_url`
  );
  stmt.params.page_url = url;
  stmt.executeAsync({
    _value: -1,
    handleResult(aResultSet) {
      let row = aResultSet.getNextRow();
      if (!row) {
        ok(false, "The page should exist in the database");
      }
      this._value = row.getResultByName(aFieldName);
    },
    handleError() {},
    handleCompletion(aReason) {
      if (aReason != Ci.mozIStorageStatementCallback.REASON_FINISHED) {
        ok(false, "The statement should properly succeed");
      }
      aCallback(this._value);
    },
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
  onTitleChanged() {},
  onDeleteURI() {},
  onClearHistory() {},
  onPageChanged() {},
  onDeleteVisits() {},
  QueryInterface: ChromeUtils.generateQI(["nsINavHistoryObserver"]),
};

function whenNewWindowLoaded(aOptions, aCallback) {
  BrowserTestUtils.waitForNewWindow().then(aCallback);
  OpenBrowserWindow(aOptions);
}
