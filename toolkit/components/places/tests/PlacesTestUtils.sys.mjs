"use strict";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
});
ChromeUtils.defineModuleGetter(
  lazy,
  "TestUtils",
  "resource://testing-common/TestUtils.jsm"
);

export var PlacesTestUtils = Object.freeze({
  /**
   * Asynchronously adds visits to a page.
   *
   * @param {*} aPlaceInfo
   *        A string URL, nsIURI, Window.URL object, info object (explained
   *        below), or an array of any of those.  Info objects describe the
   *        visits to add more fully than URLs/URIs alone and look like this:
   *
   *          {
   *            uri|url: href, URL or nsIURI of the page,
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

    if (Array.isArray(placeInfo)) {
      places.push(...placeInfo);
    } else {
      places.push(placeInfo);
    }

    // Create a PageInfo for each entry.
    let lastStoredVisit;
    for (let obj of places) {
      let place;
      if (
        obj instanceof Ci.nsIURI ||
        URL.isInstance(obj) ||
        typeof obj == "string"
      ) {
        place = { uri: obj };
      } else if (typeof obj == "object" && (obj.uri || obj.url)) {
        place = obj;
      } else {
        throw new Error("Unsupported type passed to addVisits");
      }

      let referrer;
      let info = { url: place.uri || place.url };
      let spec =
        info.url instanceof Ci.nsIURI ? info.url.spec : new URL(info.url).href;
      info.title = "title" in place ? place.title : "test visit for " + spec;
      if (typeof place.referrer == "string") {
        referrer = Services.io.newURI(place.referrer);
      } else if (place.referrer) {
        referrer = URL.isInstance(place.referrer)
          ? Services.io.newURI(place.referrer.href)
          : place.referrer;
      }
      let visitDate = place.visitDate;
      if (visitDate) {
        if (visitDate.constructor.name != "Date") {
          // visitDate should be in microseconds. It's easy to do the wrong thing
          // and pass milliseconds, so we lazily check for that.
          // While it's not easily distinguishable, since both are integers, we
          // can check if the value is very far in the past, and assume it's
          // probably a mistake.
          if (visitDate <= Date.now()) {
            throw new Error(
              "AddVisits expects a Date object or _micro_seconds!"
            );
          }
          visitDate = lazy.PlacesUtils.toDate(visitDate);
        }
      } else {
        visitDate = new Date();
      }
      info.visits = [
        {
          transition: place.transition,
          date: visitDate,
          referrer,
        },
      ];
      infos.push(info);
      if (
        !place.transition ||
        place.transition != lazy.PlacesUtils.history.TRANSITIONS.EMBED
      ) {
        lastStoredVisit = info;
      }
    }
    await lazy.PlacesUtils.history.insertMany(infos);
    if (lastStoredVisit) {
      await lazy.TestUtils.waitForCondition(
        () => lazy.PlacesUtils.history.fetch(lastStoredVisit.url),
        "Ensure history has been updated and is visible to read-only connections"
      );
    }
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
      faviconPromises.push(
        new Promise((resolve, reject) => {
          let uri = Services.io.newURI(key);
          let faviconURI = Services.io.newURI(val);
          try {
            lazy.PlacesUtils.favicons.setAndFetchFaviconForPage(
              uri,
              faviconURI,
              false,
              lazy.PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
              resolve,
              Services.scriptSecurityManager.getSystemPrincipal()
            );
          } catch (ex) {
            reject(ex);
          }
        })
      );
    }
    await Promise.all(faviconPromises);
  },

  /**
   * Clears any favicons stored in the database.
   */
  async clearFavicons() {
    return new Promise(resolve => {
      Services.obs.addObserver(function observer() {
        Services.obs.removeObserver(observer, "places-favicons-expired");
        resolve();
      }, "places-favicons-expired");
      lazy.PlacesUtils.favicons.expireAllFavicons();
    });
  },

  /**
   * Adds a bookmark to the database. This should only be used when you need to
   * add keywords. Otherwise, use `PlacesUtils.bookmarks.insert()`.
   * @param {string} aBookmarkObj.uri
   * @param {string} [aBookmarkObj.title]
   * @param {string} [aBookmarkObj.keyword]
   */
  async addBookmarkWithDetails(aBookmarkObj) {
    await lazy.PlacesUtils.bookmarks.insert({
      parentGuid: lazy.PlacesUtils.bookmarks.unfiledGuid,
      title: aBookmarkObj.title || "A bookmark",
      url: aBookmarkObj.uri,
    });

    if (aBookmarkObj.keyword) {
      await lazy.PlacesUtils.keywords.insert({
        keyword: aBookmarkObj.keyword,
        url:
          aBookmarkObj.uri instanceof Ci.nsIURI
            ? aBookmarkObj.uri.spec
            : aBookmarkObj.uri,
        postData: aBookmarkObj.postData,
      });
    }

    if (aBookmarkObj.tags) {
      let uri =
        aBookmarkObj.uri instanceof Ci.nsIURI
          ? aBookmarkObj.uri
          : Services.io.newURI(aBookmarkObj.uri);
      lazy.PlacesUtils.tagging.tagURI(uri, aBookmarkObj.tags);
    }
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
    return lazy.PlacesUtils.withConnectionWrapper(
      "promiseAsyncUpdates",
      async function(db) {
        try {
          await db.executeCached("BEGIN EXCLUSIVE");
          await db.executeCached("COMMIT");
        } catch (ex) {
          // If we fail to start a transaction, it's because there is already one.
          // In such a case we should not try to commit the existing transaction.
        }
      }
    );
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
    let db = await lazy.PlacesUtils.promiseDBConnection();
    let rows = await db.executeCached(
      "SELECT id FROM moz_places WHERE url_hash = hash(:url) AND url = :url",
      { url }
    );
    return !!rows.length;
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
    let db = await lazy.PlacesUtils.promiseDBConnection();
    let rows = await db.executeCached(
      `SELECT count(*) FROM moz_historyvisits v
       JOIN moz_places h ON h.id = v.place_id
       WHERE url_hash = hash(:url) AND url = :url`,
      { url }
    );
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
  fieldInDB(aURI, field) {
    let url = aURI instanceof Ci.nsIURI ? new URL(aURI.spec) : new URL(aURI);
    return lazy.PlacesUtils.withConnectionWrapper(
      "PlacesTestUtils.jsm: fieldInDb",
      async db => {
        let rows = await db.executeCached(
          `SELECT ${field} FROM moz_places
        WHERE url_hash = hash(:url) AND url = :url`,
          { url: url.href }
        );
        return rows[0].getResultByIndex(0);
      }
    );
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
    return lazy.PlacesUtils.withConnectionWrapper(
      "PlacesTestUtils: markBookmarksAsSynced",
      function(db) {
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
            { syncStatus: lazy.PlacesUtils.bookmarks.SYNC_STATUS.NORMAL }
          );
          await db.executeCached("DELETE FROM moz_bookmarks_deleted");
        });
      }
    );
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
    return lazy.PlacesUtils.withConnectionWrapper(
      "PlacesTestUtils: setBookmarkSyncFields",
      function(db) {
        return db.executeTransaction(async function() {
          for (let info of aFieldInfos) {
            if (!lazy.PlacesUtils.isValidGuid(info.guid)) {
              throw new Error(`Invalid GUID: ${info.guid}`);
            }
            await db.executeCached(
              `UPDATE moz_bookmarks
             SET syncStatus = IFNULL(:syncStatus, syncStatus),
                 syncChangeCounter = IFNULL(:syncChangeCounter, syncChangeCounter),
                 lastModified = IFNULL(:lastModified, lastModified),
                 dateAdded = IFNULL(:dateAdded, dateAdded)
             WHERE guid = :guid`,
              {
                guid: info.guid,
                syncChangeCounter: info.syncChangeCounter,
                syncStatus: "syncStatus" in info ? info.syncStatus : null,
                lastModified:
                  "lastModified" in info
                    ? lazy.PlacesUtils.toPRTime(info.lastModified)
                    : null,
                dateAdded:
                  "dateAdded" in info
                    ? lazy.PlacesUtils.toPRTime(info.dateAdded)
                    : null,
              }
            );
          }
        });
      }
    );
  },

  async fetchBookmarkSyncFields(...aGuids) {
    let db = await lazy.PlacesUtils.promiseDBConnection();
    let results = [];
    for (let guid of aGuids) {
      let rows = await db.executeCached(
        `
        SELECT syncStatus, syncChangeCounter, lastModified, dateAdded
        FROM moz_bookmarks
        WHERE guid = :guid`,
        { guid }
      );
      if (!rows.length) {
        throw new Error(`Bookmark ${guid} does not exist`);
      }
      results.push({
        guid,
        syncStatus: rows[0].getResultByName("syncStatus"),
        syncChangeCounter: rows[0].getResultByName("syncChangeCounter"),
        lastModified: lazy.PlacesUtils.toDate(
          rows[0].getResultByName("lastModified")
        ),
        dateAdded: lazy.PlacesUtils.toDate(
          rows[0].getResultByName("dateAdded")
        ),
      });
    }
    return results;
  },

  async fetchSyncTombstones() {
    let db = await lazy.PlacesUtils.promiseDBConnection();
    let rows = await db.executeCached(`
      SELECT guid, dateRemoved
      FROM moz_bookmarks_deleted
      ORDER BY guid`);
    return rows.map(row => ({
      guid: row.getResultByName("guid"),
      dateRemoved: lazy.PlacesUtils.toDate(row.getResultByName("dateRemoved")),
    }));
  },

  waitForNotification(notification, conditionFn, type = "bookmarks") {
    if (type == "places") {
      return new Promise(resolve => {
        function listener(events) {
          if (!conditionFn || conditionFn(events)) {
            PlacesObservers.removeListener([notification], listener);
            resolve(events);
          }
        }
        PlacesObservers.addListener([notification], listener);
      });
    }

    return new Promise(resolve => {
      let proxifiedObserver = new Proxy(
        {},
        {
          get: (target, name) => {
            if (name == "QueryInterface") {
              return ChromeUtils.generateQI([Ci.nsINavBookmarkObserver]);
            }
            if (name == notification) {
              return (...args) => {
                if (!conditionFn || conditionFn.apply(this, args)) {
                  lazy.PlacesUtils[type].removeObserver(proxifiedObserver);
                  resolve();
                }
              };
            }
            if (name == "skipTags") {
              return false;
            }
            return () => false;
          },
        }
      );
      lazy.PlacesUtils[type].addObserver(proxifiedObserver);
    });
  },

  /**
   * A debugging helper that dumps the contents of an SQLite table.
   *
   * @param {Sqlite.OpenedConnection} db
   *        The mirror database connection.
   * @param {String} table
   *        The table name.
   */
  async dumpTable(db, table) {
    let columns = (await db.execute(`PRAGMA table_info('${table}')`)).map(r =>
      r.getResultByName("name")
    );
    let results = [columns.join("\t")];

    let rows = await db.execute(`SELECT * FROM ${table}`);
    dump(`>> Table ${table} contains ${rows.length} rows\n`);

    for (let row of rows) {
      let numColumns = row.numEntries;
      let rowValues = [];
      for (let i = 0; i < numColumns; ++i) {
        let value = "N/A";
        switch (row.getTypeOfIndex(i)) {
          case Ci.mozIStorageValueArray.VALUE_TYPE_NULL:
            value = "NULL";
            break;
          case Ci.mozIStorageValueArray.VALUE_TYPE_INTEGER:
            value = row.getInt64(i);
            break;
          case Ci.mozIStorageValueArray.VALUE_TYPE_FLOAT:
            value = row.getDouble(i);
            break;
          case Ci.mozIStorageValueArray.VALUE_TYPE_TEXT:
            value = JSON.stringify(row.getString(i));
            break;
        }
        rowValues.push(value.toString().padStart(columns[i].length, " "));
      }
      results.push(rowValues.join("\t"));
    }
    results.push("\n");
    dump(results.join("\n"));
  },

  /**
   * Removes all stored metadata.
   */
  clearMetadata() {
    return lazy.PlacesUtils.withConnectionWrapper(
      "PlacesTestUtils: clearMetadata",
      async db => {
        await db.execute(`DELETE FROM moz_meta`);
        lazy.PlacesUtils.metadata.cache.clear();
      }
    );
  },

  /**
   * Clear moz_inputhistory table.
   */
  async clearInputHistory() {
    await lazy.PlacesUtils.withConnectionWrapper(
      "test:clearInputHistory",
      db => {
        return db.executeCached("DELETE FROM moz_inputhistory");
      }
    );
  },

  /**
   * Clear moz_historyvisits table.
   */
  async clearHistoryVisits() {
    await lazy.PlacesUtils.withConnectionWrapper(
      "test:clearHistoryVisits",
      db => {
        return db.executeCached("DELETE FROM moz_historyvisits");
      }
    );
  },

  /**
   * Compares 2 place: URLs ignoring the order of their params.
   * @param url1 First URL to compare
   * @param url2 Second URL to compare
   * @return whether the URLs are the same
   */
  ComparePlacesURIs(url1, url2) {
    url1 = url1 instanceof Ci.nsIURI ? url1.spec : new URL(url1);
    if (url1.protocol != "place:") {
      throw new Error("Expected a place: uri, got " + url1.href);
    }
    url2 = url2 instanceof Ci.nsIURI ? url2.spec : new URL(url2);
    if (url2.protocol != "place:") {
      throw new Error("Expected a place: uri, got " + url2.href);
    }
    let tokens1 = url1.pathname
      .split("&")
      .sort()
      .join("&");
    let tokens2 = url2.pathname
      .split("&")
      .sort()
      .join("&");
    if (tokens1 != tokens2) {
      dump(`Failed comparison between:\n${tokens1}\n${tokens2}\n`);
      return false;
    }
    return true;
  },
});
