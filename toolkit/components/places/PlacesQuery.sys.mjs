/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
  requestIdleCallback: "resource://gre/modules/Timer.sys.mjs",
});
XPCOMUtils.defineLazyModuleGetters(lazy, {
  ObjectUtils: "resource://gre/modules/ObjectUtils.jsm",
});

function isRedirectType(visitType) {
  const { TRANSITIONS } = lazy.PlacesUtils.history;
  return (
    visitType === TRANSITIONS.REDIRECT_PERMANENT ||
    visitType === TRANSITIONS.REDIRECT_TEMPORARY
  );
}

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
  /** @type HistoryVisit[] */
  #cachedHistory = null;
  /** @type object */
  #cachedHistoryOptions = null;
  /** @type function(PlacesEvent[]) */
  #historyListener = null;
  /** @type function(HistoryVisit[]) */
  #historyListenerCallback = null;

  /**
   * Get a snapshot of history visits at this moment.
   *
   * @param {object} [options]
   *   Options to apply to the database query.
   * @param {number} [options.daysOld]
   *   The maximum number of days to go back in history.
   * @returns {HistoryVisit[]}
   *   History visits obtained from the database query.
   */
  async getHistory({ daysOld = 60 } = {}) {
    const options = { daysOld };
    const cacheInvalid =
      this.#cachedHistory == null ||
      !lazy.ObjectUtils.deepEqual(options, this.#cachedHistoryOptions);
    if (cacheInvalid) {
      this.#cachedHistory = [];
      this.#cachedHistoryOptions = options;
      const db = await lazy.PlacesUtils.promiseDBConnection();
      const sql = `SELECT v.id, visit_date, title, url, visit_type, from_visit, hidden
        FROM moz_historyvisits v
        JOIN moz_places h
        ON v.place_id = h.id
        WHERE visit_date >= (strftime('%s','now','localtime','start of day','-${Number(
          daysOld
        )} days','utc') * 1000000)
        ORDER BY visit_date DESC`;
      const rows = await db.executeCached(sql);
      let lastUrl; // Avoid listing consecutive visits to the same URL.
      let lastRedirectFromVisitId; // Avoid listing redirecting visits.
      for (const row of rows) {
        const [
          id,
          visitDate,
          title,
          url,
          visitType,
          fromVisit,
          hidden,
        ] = Array.from({ length: row.numEntries }, (_, i) =>
          row.getResultByIndex(i)
        );
        if (isRedirectType(visitType) && fromVisit > 0) {
          lastRedirectFromVisitId = fromVisit;
        }
        if (!hidden && url !== lastUrl && id !== lastRedirectFromVisitId) {
          this.#cachedHistory.push({
            date: lazy.PlacesUtils.toDate(visitDate),
            id,
            title,
            url,
          });
          lastUrl = url;
        }
      }
    }
    if (!this.#historyListener) {
      this.#initHistoryListener();
    }
    return this.#cachedHistory;
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
    this.#cachedHistory = null;
    this.#cachedHistoryOptions = null;
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
        this.#cachedHistory = null;
      } else if (this.#cachedHistory != null) {
        for (const event of events) {
          switch (event.type) {
            case "page-visited":
              await this.#handlePageVisited(event);
              break;
            case "history-cleared":
              this.#cachedHistory = [];
              break;
            case "page-title-changed":
              this.#cachedHistory
                .filter(({ url }) => url === event.url)
                .forEach(visit => (visit.title = event.title));
              break;
          }
        }
      }
      if (typeof this.#historyListenerCallback === "function") {
        lazy.requestIdleCallback(async () => {
          const history = await this.getHistory(this.#cachedHistoryOptions);
          this.#historyListenerCallback(history);
        });
      }
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
   */
  async #handlePageVisited(event) {
    const lastVisit = this.#cachedHistory[0];
    if (
      lastVisit != null &&
      (event.url === lastVisit.url ||
        (isRedirectType(event.transitionType) &&
          event.referringVisitId === lastVisit.id))
    ) {
      // Remove the last visit if it duplicates this visit's URL, or if it
      // redirects to this visit.
      this.#cachedHistory.shift();
    }
    if (!event.hidden) {
      this.#cachedHistory.unshift({
        date: new Date(event.visitTime),
        id: event.visitId,
        title: event.lastKnownTitle,
        url: event.url,
      });
    }
  }
}
