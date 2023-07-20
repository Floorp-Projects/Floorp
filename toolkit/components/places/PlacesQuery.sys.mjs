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
 * @property {number} id
 *   Visit ID from the database.
 * @property {string} title
 *   The page's title.
 * @property {string} url
 *   The page's URL.
 */

/**
 * Queries the places database using an async read only connection. Maintains
 * an internal cache of query results which is live-updated by adding listeners
 * to `PlacesObservers`. When the results are no longer needed, call `close` to
 * remove the listeners.
 */
export class PlacesQuery {
  /** @type {Map<any, HistoryVisit[]>} */
  cachedHistory = null;
  /** @type {object} */
  cachedHistoryOptions = null;
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
  }

  /**
   * Run the database query and populate the history cache.
   */
  async fetchHistory() {
    const { daysOld, limit } = this.cachedHistoryOptions;
    const db = await lazy.PlacesUtils.promiseDBConnection();
    const sql = `SELECT v.id, visit_date, title, url
      FROM moz_historyvisits v
      JOIN moz_places h
      ON v.place_id = h.id
      WHERE visit_date >= (strftime('%s','now','localtime','start of day','-${Number(
        daysOld
      )} days','utc') * 1000000)
      AND hidden = 0
      ORDER BY visit_date DESC
      LIMIT ${limit > 0 ? limit : -1}`;
    const rows = await db.executeCached(sql);
    for (const row of rows) {
      this.appendToCache({
        date: lazy.PlacesUtils.toDate(row.getResultByName("visit_date")),
        id: row.getResultByName("id"),
        title: row.getResultByName("title"),
        url: row.getResultByName("url"),
      });
    }
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
    let mapKey;
    switch (this.cachedHistoryOptions.sortBy) {
      case "date":
        mapKey = this.getStartOfDayTimestamp(visit.date);
        break;
      case "site":
        const { protocol } = new URL(visit.url);
        mapKey =
          protocol === "http:" || protocol === "https:"
            ? lazy.BrowserUtils.formatURIStringForDisplay(visit.url)
            : "";
        break;
    }
    if (!this.cachedHistory.has(mapKey)) {
      this.cachedHistory.set(mapKey, []);
    }
    return this.cachedHistory.get(mapKey);
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
              this.cachedHistory.forEach(visits =>
                visits
                  .filter(({ url }) => url === event.url)
                  .forEach(visit => (visit.title = event.title))
              );
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
      id: event.visitId,
      title: event.lastKnownTitle,
      url: event.url,
    };
    this.insertSortedIntoCache(visit);
    return visit;
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
