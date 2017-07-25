"use strict";

this.EXPORTED_SYMBOLS = [
  "PlacesTestUtils",
];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.importGlobalProperties(["URL"]);

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");

this.PlacesTestUtils = Object.freeze({
  /**
   * Asynchronously adds visits to a page.
   *
   * @param aPlaceInfo
   *        Can be an nsIURI, in such a case a single LINK visit will be added.
   *        Otherwise can be an object describing the visit to add, or an array
   *        of these objects:
   *          { uri: href, URL or nsIURI of the page,
   *            [optional] transition: one of the TRANSITION_* from nsINavHistoryService,
   *            [optional] title: title of the page,
   *            [optional] visitDate: visit date, either in microseconds from the epoch or as a date object
   *            [optional] referrer: nsIURI of the referrer for this visit
   *          }
   *
   * @return {Promise}
   * @resolves When all visits have been added successfully.
   * @rejects JavaScript exception.
   */
  async addVisits(placeInfo) {
    let places = [];
    let infos = [];

    if (placeInfo instanceof Ci.nsIURI ||
        placeInfo instanceof URL ||
        typeof placeInfo == "string") {
      places.push({ uri: placeInfo });
    } else if (Array.isArray(placeInfo)) {
      places = places.concat(placeInfo);
    } else if (typeof placeInfo == "object" && placeInfo.uri) {
      places.push(placeInfo)
    } else {
      throw new Error("Unsupported type passed to addVisits");
    }

    // Create a PageInfo for each entry.
    for (let place of places) {
      let info = {url: place.uri};
      info.title = (typeof place.title === "string") ? place.title : "test visit for " + info.url.spec ;
      if (typeof place.referrer == "string") {
        place.referrer = NetUtil.newURI(place.referrer);
      } else if (place.referrer && place.referrer instanceof URL) {
        place.referrer = NetUtil.newURI(place.referrer.href);
      }
      let visitDate = place.visitDate;
      if (visitDate) {
        if (visitDate.constructor.name != "Date") {
          // visitDate should be in microseconds. It's easy to do the wrong thing
          // and pass milliseconds to updatePlaces, so we lazily check for that.
          // While it's not easily distinguishable, since both are integers, we
          // can check if the value is very far in the past, and assume it's
          // probably a mistake.
          if (visitDate <= Date.now()) {
            throw new Error("AddVisits expects a Date object or _micro_seconds!");
          }
          visitDate = PlacesUtils.toDate(visitDate);
        }
      } else {
        visitDate = new Date();
      }
      info.visits = [{
        transition: place.transition,
        date: visitDate,
        referrer: place.referrer
      }];
      infos.push(info);
    }
    return PlacesUtils.history.insertMany(infos);
  },

   /*
    * Add Favicons
    *
    * @param {Map} faviconURLs  keys are page URLs, values are their
    *                           associated favicon URLs.
    */

  async addFavicons(faviconURLs) {
    let faviconPromises = [];

    // If no favicons were provided, we do not want to continue on
    if (!faviconURLs) {
      throw new Error("No favicon URLs were provided");
    }
    for (let [key, val] of faviconURLs) {
      if (!val) {
        throw new Error("URL does not exist");
      }
      faviconPromises.push(new Promise((resolve, reject) => {
        let uri = NetUtil.newURI(key);
        let faviconURI = NetUtil.newURI(val);
        try {
          PlacesUtils.favicons.setAndFetchFaviconForPage(
            uri,
            faviconURI,
            false,
            PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
            resolve,
            Services.scriptSecurityManager.getSystemPrincipal());
        } catch (ex) {
          reject(ex);
        }
      }));
    }
    await Promise.all(faviconPromises);
  },

  /**
   * Clear all history.
   *
   * @return {Promise}
   * @resolves When history was cleared successfully.
   * @rejects JavaScript exception.
   *
   * @deprecated New consumers should directly use PlacesUtils.history.clear().
   */
  clearHistory() {
    return PlacesUtils.history.clear();
  },

  /**
   * Waits for all pending async statements on the default connection.
   *
   * @return {Promise}
   * @resolves When all pending async statements finished.
   * @rejects Never.
   *
   * @note The result is achieved by asynchronously executing a query requiring
   *       a write lock.  Since all statements on the same connection are
   *       serialized, the end of this write operation means that all writes are
   *       complete.  Note that WAL makes so that writers don't block readers, but
   *       this is a problem only across different connections.
   */
  promiseAsyncUpdates() {
    return PlacesUtils.withConnectionWrapper("promiseAsyncUpdates", async function(db) {
      try {
        await db.executeCached("BEGIN EXCLUSIVE");
        await db.executeCached("COMMIT");
      } catch (ex) {
        // If we fail to start a transaction, it's because there is already one.
        // In such a case we should not try to commit the existing transaction.
      }
    });
  },

  /**
   * Asynchronously checks if an address is found in the database.
   * @param aURI
   *        nsIURI or address to look for.
   *
   * @return {Promise}
   * @resolves Returns true if the page is found.
   * @rejects JavaScript exception.
   */
  async isPageInDB(aURI) {
    let url = aURI instanceof Ci.nsIURI ? aURI.spec : aURI;
    let db = await PlacesUtils.promiseDBConnection();
    let rows = await db.executeCached(
      "SELECT id FROM moz_places WHERE url_hash = hash(:url) AND url = :url",
      { url });
    return rows.length > 0;
  },

  /**
   * Asynchronously checks how many visits exist for a specified page.
   * @param aURI
   *        nsIURI or address to look for.
   *
   * @return {Promise}
   * @resolves Returns the number of visits found.
   * @rejects JavaScript exception.
   */
  async visitsInDB(aURI) {
    let url = aURI instanceof Ci.nsIURI ? aURI.spec : aURI;
    let db = await PlacesUtils.promiseDBConnection();
    let rows = await db.executeCached(
      `SELECT count(*) FROM moz_historyvisits v
       JOIN moz_places h ON h.id = v.place_id
       WHERE url_hash = hash(:url) AND url = :url`,
      { url });
    return rows[0].getResultByIndex(0);
  },

  /**
   * Asynchronously returns the required DB field for a specified page.
   * @param aURI
   *        nsIURI or address to look for.
   *
   * @return {Promise}
   * @resolves Returns the field value.
   * @rejects JavaScript exception.
   */
  async fieldInDB(aURI, field) {
    let url = aURI instanceof Ci.nsIURI ? new URL(aURI.spec) : new URL(aURI);
    let db = await PlacesUtils.promiseDBConnection();
    let rows = await db.executeCached(
      `SELECT ${field} FROM moz_places
       WHERE url_hash = hash(:url) AND url = :url`,
      { url: url.href });
    return rows[0].getResultByIndex(0);
  },

  /**
   * Marks all syncable bookmarks as synced by setting their sync statuses to
   * "NORMAL", resetting their change counters, and removing all tombstones.
   * Used by tests to avoid calling `PlacesSyncUtils.bookmarks.pullChanges`
   * and `PlacesSyncUtils.bookmarks.pushChanges`.
   *
   * @resolves When all bookmarks have been updated.
   * @rejects JavaScript exception.
   */
  markBookmarksAsSynced() {
    return PlacesUtils.withConnectionWrapper("PlacesTestUtils: markBookmarksAsSynced", function(db) {
      return db.executeTransaction(async function() {
        await db.executeCached(
          `WITH RECURSIVE
           syncedItems(id) AS (
             SELECT b.id FROM moz_bookmarks b
             WHERE b.guid IN ('menu________', 'toolbar_____', 'unfiled_____',
                              'mobile______')
             UNION ALL
             SELECT b.id FROM moz_bookmarks b
             JOIN syncedItems s ON b.parent = s.id
           )
           UPDATE moz_bookmarks
           SET syncChangeCounter = 0,
               syncStatus = :syncStatus
           WHERE id IN syncedItems`,
          { syncStatus: PlacesUtils.bookmarks.SYNC_STATUS.NORMAL });
        await db.executeCached("DELETE FROM moz_bookmarks_deleted");
      });
    });
  },

  /**
   * Sets sync fields for multiple bookmarks.
   * @param aStatusInfos
   *        One or more objects with the following properties:
   *          { [required] guid: The bookmark's GUID,
   *            syncStatus: An `nsINavBookmarksService::SYNC_STATUS_*` constant,
   *            syncChangeCounter: The sync change counter value,
   *            lastModified: The last modified time,
   *            dateAdded: The date added time.
   *          }
   *
   * @resolves When all bookmarks have been updated.
   * @rejects JavaScript exception.
   */
  setBookmarkSyncFields(...aFieldInfos) {
    return PlacesUtils.withConnectionWrapper("PlacesTestUtils: setBookmarkSyncFields", function(db) {
      return db.executeTransaction(async function() {
        for (let info of aFieldInfos) {
          if (!PlacesUtils.isValidGuid(info.guid)) {
            throw new Error(`Invalid GUID: ${info.guid}`);
          }
          await db.executeCached(
            `UPDATE moz_bookmarks
             SET syncStatus = IFNULL(:syncStatus, syncStatus),
                 syncChangeCounter = IFNULL(:syncChangeCounter, syncChangeCounter),
                 lastModified = IFNULL(:lastModified, lastModified),
                 dateAdded = IFNULL(:dateAdded, dateAdded)
             WHERE guid = :guid`,
             { guid: info.guid, syncChangeCounter: info.syncChangeCounter,
               syncStatus: "syncStatus" in info ? info.syncStatus : null,
               lastModified: "lastModified" in info ? PlacesUtils.toPRTime(info.lastModified) : null,
               dateAdded: "dateAdded" in info ? PlacesUtils.toPRTime(info.dateAdded) : null });
        }
      });
    });
  },

  async fetchBookmarkSyncFields(...aGuids) {
    let db = await PlacesUtils.promiseDBConnection();
    let results = [];
    for (let guid of aGuids) {
      let rows = await db.executeCached(`
        SELECT syncStatus, syncChangeCounter, lastModified, dateAdded
        FROM moz_bookmarks
        WHERE guid = :guid`,
        { guid });
      if (!rows.length) {
        throw new Error(`Bookmark ${guid} does not exist`);
      }
      results.push({
        guid,
        syncStatus: rows[0].getResultByName("syncStatus"),
        syncChangeCounter: rows[0].getResultByName("syncChangeCounter"),
        lastModified: PlacesUtils.toDate(rows[0].getResultByName("lastModified")),
        dateAdded: PlacesUtils.toDate(rows[0].getResultByName("dateAdded")),
      });
    }
    return results;
  },

  async fetchSyncTombstones() {
    let db = await PlacesUtils.promiseDBConnection();
    let rows = await db.executeCached(`
      SELECT guid, dateRemoved
      FROM moz_bookmarks_deleted
      ORDER BY guid`);
    return rows.map(row => ({
      guid: row.getResultByName("guid"),
      dateRemoved: PlacesUtils.toDate(row.getResultByName("dateRemoved")),
    }));
  },

  waitForNotification(notification, conditionFn = () => true, type = "bookmarks") {
    let iface = type == "bookmarks" ? Ci.nsINavBookmarkObserver
                                    : Ci.nsINavHistoryObserver;
    return new Promise(resolve => {
      let proxifiedObserver = new Proxy({}, {
        get: (target, name) => {
          if (name == "QueryInterface")
            return XPCOMUtils.generateQI([iface]);
          if (name == notification)
            return (...args) => {
              if (conditionFn.apply(this, args)) {
                PlacesUtils[type].removeObserver(proxifiedObserver);
                resolve();
              }
            }
          if (name == "skipTags" || name == "skipDescendantsOnItemRemoval") {
            return false;
          }
          return () => false;
        }
      });
      PlacesUtils[type].addObserver(proxifiedObserver);
    });
  },
});
