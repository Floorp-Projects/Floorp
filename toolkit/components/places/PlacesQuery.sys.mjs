/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  BinarySearch: "resource://gre/modules/BinarySearch.sys.mjs",
  BrowserUtils: "resource://gre/modules/BrowserUtils.sys.mjs",
  ObjectUtils: "resource://gre/modules/ObjectUtils.sys.mjs",
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
  requestIdleCallback: "resource://gre/modules/Timer.sys.mjs",
});

const BULK_PLACES_EVENTS_THRESHOLD = 50;

/**
 * An object that contains details of a page visit.
 *
 * @typedef {object} HistoryVisit
 *
 * @property {Date} date
 *   When this page was visited.
 * @property {number} frecency
 *   The page's frecency score.
 * @property {number} id
 *   Visit ID from the database.
 * @property {string} title
 *   The page's title.
 * @property {string} url
 *   The page's URL.
 */

/**
 * Cache key type depends on how visits are currently being grouped.
 *
 * By date: number - The start of day timestamp of the visit.
 * By site: string - The domain name of the visit.
 *
 * @typedef {number | string} CacheKey
 */

/**
 * Queries the places database using an async read only connection. Maintains
 * an internal cache of query results which is live-updated by adding listeners
 * to `PlacesObservers`. When the results are no longer needed, call `close` to
 * remove the listeners.
 */
export class PlacesQuery {
  /** @type {Map<CacheKey, HistoryVisit[]>} */
  cachedHistory = null;
  /** @type {object} */
  cachedHistoryOptions = null;
  /** @type {Map<CacheKey, { [baseUrl: string]: { [title: string]: HistoryVisit[] } }>} */
  #cachedBaseUrls = null;
  /** @type {function(PlacesEvent[])} */
  #historyListener = null;
  /** @type {function(HistoryVisit[])} */
  #historyListenerCallback = null;

  /**
   * Get a snapshot of history visits at this moment.
   *
   * @param {object} [options]
   *   Options to apply to the database query.
   * @param {number} [options.daysOld]
   *   The maximum number of days to go back in history.
   * @param {number} [options.limit]
   *   The maximum number of visits to return.
   * @param {string} [options.sortBy]
   *   The sorting order of history visits:
   *   - "date": Group visits based on the date they occur.
   *   - "site": Group visits based on host, excluding any "www." prefix.
   * @returns {Map<any, HistoryVisit[]>}
   *   History visits obtained from the database query.
   */
  async getHistory({ daysOld = 60, limit, sortBy = "date" } = {}) {
    const options = { daysOld, limit, sortBy };
    const cacheInvalid =
      this.cachedHistory == null ||
      !lazy.ObjectUtils.deepEqual(options, this.cachedHistoryOptions);
    if (cacheInvalid) {
      this.initializeCache(options);
      await this.fetchHistory();
    }
    if (!this.#historyListener) {
      this.#initHistoryListener();
    }
    return this.cachedHistory;
  }

  /**
   * Clear existing cache and store options for the new query.
   *
   * @param {object} options
   *   The database query options.
   */
  initializeCache(options = this.cachedHistoryOptions) {
    this.cachedHistory = new Map();
    this.cachedHistoryOptions = options;
    this.#cachedBaseUrls = new Map();
  }

  /**
   * Run the database query and populate the history cache.
   */
  async fetchHistory() {
    const { daysOld, limit, sortBy } = this.cachedHistoryOptions;
    const db = await lazy.PlacesUtils.promiseDBConnection();
    let groupBy;
    switch (sortBy) {
      case "date":
        groupBy = "url, date(visit_date / 1000000, 'unixepoch', 'localtime')";
        break;
      case "site":
        groupBy = "url";
        break;
    }
    const sql = `SELECT v.id, visit_date, title, url, frecency
      FROM moz_historyvisits v
      JOIN moz_places h
      ON v.place_id = h.id
      WHERE visit_date >= (strftime('%s','now','localtime','start of day','-${Number(
        daysOld
      )} days','utc') * 1000000)
      AND hidden = 0
      GROUP BY ${groupBy}
      ORDER BY visit_date DESC
      LIMIT ${limit > 0 ? limit : -1}`;
    const rows = await db.executeCached(sql);
    for (const row of rows) {
      const visit = {
        date: lazy.PlacesUtils.toDate(row.getResultByName("visit_date")),
        frecency: row.getResultByName("frecency"),
        id: row.getResultByName("id"),
        title: row.getResultByName("title"),
        url: row.getResultByName("url"),
      };
      if (this.#maybeCacheBaseUrlForVisit(visit)) {
        this.appendToCache(visit);
      }
    }
  }

  /**
   * Check whether this visit is a duplicate entry in the container it belongs
   * in. For the purposes of this API, duplicates are defined as visits having
   * the same "base" URL (URL without query (?) or fragment (#)) and document
   * title.
   *
   * If this visit is not a duplicate entry, it is cached for future reference.
   *
   * @param {HistoryVisit} visit
   *   The visit to check.
   * @returns {boolean}
   *   `true` if there was no duplicate entry and this visit was cached.
   *   `false` if there was a duplicate entry and this visit was not cached.
   */
  #maybeCacheBaseUrlForVisit(visit) {
    const mapKey = this.#getMapKeyForVisit(visit);
    let container = this.#cachedBaseUrls.get(mapKey);
    if (!container) {
      container = {};
      this.#cachedBaseUrls.set(mapKey, container);
    }
    const baseUrl = this.#getBaseUrl(visit.url);
    if (!Object.hasOwn(container, baseUrl)) {
      container[baseUrl] = {};
    }
    const title = visit.title ?? visit.url;
    // Get a list of visits for this combination of base URL + title.
    const visits = container[baseUrl][title];
    if (!visits?.length) {
      container[baseUrl][title] = [visit];
      return true;
    }
    // There is an existing duplicate visit. Replace it with this one, if it
    // has a higher frecency score...
    const existingVisit = visits[visits.length - 1];
    if (visit.frecency > existingVisit.frecency) {
      const historyContainer = this.#getContainerForVisit(existingVisit);
      historyContainer.splice(historyContainer.indexOf(existingVisit), 1);
      visits.push(visit);
      return true;
    }
    // ...otherwise, place it "behind" the existing visit.
    const insertionPoint = lazy.BinarySearch.insertionIndexOf(
      (a, b) => a.frecency - b.frecency,
      visits,
      visit
    );
    visits.splice(insertionPoint, 0, visit);
    return false;
  }

  #getBaseUrl(urlString) {
    const uri = Services.io.newURI(urlString);
    return uri.prePath + uri.filePath;
  }

  /**
   * Append a visit into the container it belongs to.
   *
   * @param {HistoryVisit} visit
   *   The visit to append.
   */
  appendToCache(visit) {
    this.#getContainerForVisit(visit).push(visit);
  }

  /**
   * Insert a visit into the container it belongs to, ensuring to maintain
   * sorted order. Used for handling `page-visited` events after the initial
   * fetch of history data.
   *
   * @param {HistoryVisit} visit
   *   The visit to insert.
   */
  insertSortedIntoCache(visit) {
    const container = this.#getContainerForVisit(visit);
    let insertionPoint = 0;
    if (visit.date.getTime() < container[0]?.date.getTime()) {
      insertionPoint = lazy.BinarySearch.insertionIndexOf(
        (a, b) => b.date.getTime() - a.date.getTime(),
        container,
        visit
      );
    }
    container.splice(insertionPoint, 0, visit);
  }

  /**
   * Retrieve the corresponding container for this visit.
   *
   * @param {HistoryVisit} visit
   *   The visit to check.
   * @returns {HistoryVisit[]}
   *   The container it belongs to.
   */
  #getContainerForVisit(visit) {
    const mapKey = this.#getMapKeyForVisit(visit);
    let container = this.cachedHistory?.get(mapKey);
    if (!container) {
      container = [];
      this.cachedHistory?.set(mapKey, container);
    }
    return container;
  }

  #getMapKeyForVisit(visit) {
    switch (this.cachedHistoryOptions.sortBy) {
      case "date":
        return this.getStartOfDayTimestamp(visit.date);
      case "site":
        const { protocol } = new URL(visit.url);
        return protocol === "http:" || protocol === "https:"
          ? lazy.BrowserUtils.formatURIStringForDisplay(visit.url)
          : "";
    }
    return null;
  }

  /**
   * Observe changes to the visits table. When changes are made, the callback
   * is given the new list of visits. Only one callback can be active at a time
   * (per instance). If one already exists, it will be replaced.
   *
   * @param {function(HistoryVisit[])} callback
   *   The function to call when changes are made.
   */
  observeHistory(callback) {
    this.#historyListenerCallback = callback;
  }

  /**
   * Close this query. Caches are cleared and listeners are removed.
   */
  close() {
    this.cachedHistory = null;
    this.cachedHistoryOptions = null;
    this.#cachedBaseUrls = null;
    PlacesObservers.removeListener(
      ["page-removed", "page-visited", "history-cleared", "page-title-changed"],
      this.#historyListener
    );
    this.#historyListener = null;
    this.#historyListenerCallback = null;
  }

  /**
   * Listen for changes to the visits table and update caches accordingly.
   */
  #initHistoryListener() {
    this.#historyListener = async events => {
      if (
        events.length >= BULK_PLACES_EVENTS_THRESHOLD ||
        events.some(({ type }) => type === "page-removed")
      ) {
        // Accounting for cascading deletes, or handling places events in bulk,
        // can be expensive. In this case, we invalidate the cache once rather
        // than handling each event individually.
        this.cachedHistory = null;
      } else if (this.cachedHistory != null) {
        for (const event of events) {
          switch (event.type) {
            case "page-visited":
              this.handlePageVisited(event);
              break;
            case "history-cleared":
              this.initializeCache();
              break;
            case "page-title-changed":
              this.handlePageTitleChanged(event);
              break;
          }
        }
      }
      lazy.requestIdleCallback(async () => {
        if (typeof this.#historyListenerCallback === "function") {
          const history = await this.getHistory(this.cachedHistoryOptions);
          this.#historyListenerCallback(history);
        }
      });
    };
    PlacesObservers.addListener(
      ["page-removed", "page-visited", "history-cleared", "page-title-changed"],
      this.#historyListener
    );
  }

  /**
   * Handle a page visited event.
   *
   * @param {PlacesEvent} event
   *   The event.
   * @return {HistoryVisit}
   *   The visit that was inserted, or `null` if no visit was inserted.
   */
  handlePageVisited(event) {
    if (event.hidden) {
      return null;
    }
    const visit = {
      date: new Date(event.visitTime),
      frecency: event.frecency,
      id: event.visitId,
      title: event.lastKnownTitle,
      url: event.url,
    };
    if (this.#maybeCacheBaseUrlForVisit(visit)) {
      this.insertSortedIntoCache(visit);
      return visit;
    }
    return null;
  }

  /**
   * Handle a page title changed event.
   *
   * @param {PlacesEvent} event
   *   The event.
   */
  handlePageTitleChanged(event) {
    const visitsToReinsert = [];
    this.#cachedBaseUrls.forEach(baseUrlMap => {
      const titleMap = baseUrlMap[this.#getBaseUrl(event.url)];
      if (titleMap == null) {
        // No visits match base URL, nothing to update from this container.
        return;
      }
      const entriesToUpdate = Object.entries(titleMap).filter(([, visits]) =>
        visits.some(v => v.url === event.url)
      );
      for (const [title, visits] of entriesToUpdate) {
        // The last visit has the highest frecency, i.e. it is the "winning"
        // visit out of others with the same base URL + title, and therefore is
        // the visit that is currently displayed in history.

        // Since some visits will be updated to have a new title, they will be
        // moved out of its old container, and thus, the "winning" visit may
        // change. In that case, we need to update history accordingly.
        const originalLastVisit = visits[visits.length - 1];
        const visitsToKeep = [];
        visits.forEach(visit => {
          if (visit.url === event.url) {
            visit.title = event.title;
            visitsToReinsert.push(visit);
          } else {
            visitsToKeep.push(visit);
          }
        });
        const newLastVisit = visitsToKeep[visitsToKeep.length - 1];
        if (newLastVisit !== originalLastVisit) {
          const container = this.#getContainerForVisit(originalLastVisit);
          container.splice(container.indexOf(originalLastVisit), 1);
          if (newLastVisit != null) {
            // No need to run the visit through maybeCacheBaseUrlForVisit().
            // Updating titleMap[title] implicitly caches it in the right place.
            this.insertSortedIntoCache(newLastVisit);
          }
        }
        titleMap[title] = visitsToKeep;
      }
      for (const visit of visitsToReinsert) {
        if (this.#maybeCacheBaseUrlForVisit(visit)) {
          this.insertSortedIntoCache(visit);
        }
      }
    });
  }

  /**
   * Get timestamp from a date by only considering its year, month, and date
   * (so that it can be used as a date-based key).
   *
   * @param {Date} date
   *   The date to truncate.
   * @returns {number}
   *   The corresponding timestamp.
   */
  getStartOfDayTimestamp(date) {
    return new Date(
      date.getFullYear(),
      date.getMonth(),
      date.getDate()
    ).getTime();
  }

  /**
   * Get timestamp from a date by only considering its year and month (so that
   * it can be used as a month-based key).
   *
   * @param {Date} date
   *   The date to truncate.
   * @returns {number}
   *   The corresponding timestamp.
   */
  getStartOfMonthTimestamp(date) {
    return new Date(date.getFullYear(), date.getMonth()).getTime();
  }
}
