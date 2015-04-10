/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Asynchronous API for managing history.
 *
 *
 * The API makes use of `PageInfo` and `VisitInfo` objects, defined as follows.
 *
 * A `PageInfo` object is any object that contains A SUBSET of the
 * following properties:
 * - guid: (string)
 *     The globally unique id of the page.
 * - url: (URL)
 *     or (nsIURI)
 *     or (string)
 *     The full URI of the page. Note that `PageInfo` values passed as
 *     argument may hold `nsIURI` or `string` values for property `url`,
 *     but `PageInfo` objects returned by this module always hold `URL`
 *     values.
 * - title: (string)
 *     The title associated with the page, if any.
 * - frecency: (number)
 *     The frecency of the page, if any.
 *     See https://developer.mozilla.org/en-US/docs/Mozilla/Tech/Places/Frecency_algorithm
 *     Note that this property may not be used to change the actualy frecency
 *     score of a page, only to retrieve it. In other words, any `frecency` field
 *     passed as argument to a function of this API will be ignored.
 *  - visits: (Array<VisitInfo>)
 *     All the visits for this page, if any.
 *
 * See the documentation of individual methods to find out which properties
 * are required for `PageInfo` arguments or returned for `PageInfo` results.
 *
 * A `VisitInfo` object is any object that contains A SUBSET of the following
 * properties:
 * - date: (Date)
 *     The time the visit occurred.
 * - transition: (number)
 *     How the user reached the page. See constants `TRANSITION_*`
 *     for the possible transition types.
 * - referrer: (URL)
 *          or (nsIURI)
 *          or (string)
 *     The referring URI of this visit. Note that `VisitInfo` passed
 *     as argument may hold `nsIURI` or `string` values for property `referrer`,
 *     but `VisitInfo` objects returned by this module always hold `URL`
 *     values.
 * See the documentation of individual methods to find out which properties
 * are required for `VisitInfo` arguments or returned for `VisitInfo` results.
 *
 *
 *
 * Each successful operation notifies through the nsINavHistoryObserver
 * interface. To listen to such notifications you must register using
 * nsINavHistoryService `addObserver` and `removeObserver` methods.
 * @see nsINavHistoryObserver
 */

this.EXPORTED_SYMBOLS = [ "History" ];

const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AsyncShutdown",
                                  "resource://gre/modules/AsyncShutdown.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Sqlite",
                                  "resource://gre/modules/Sqlite.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");
Cu.importGlobalProperties(["URL"]);

/**
 * Whenever we update or remove numerous pages, it is preferable
 * to yield time to the main thread every so often to avoid janking.
 * These constants determine the maximal number of notifications we
 * may emit before we yield.
 */
const NOTIFICATION_CHUNK_SIZE = 300;
const ONRESULT_CHUNK_SIZE = 300;

/**
 * Private shutdown barrier blocked by ongoing operations.
 */
XPCOMUtils.defineLazyGetter(this, "operationsBarrier", () =>
  new AsyncShutdown.Barrier("History.jsm: wait until all connections are closed")
);

/**
 * Shared connection
 */
 XPCOMUtils.defineLazyGetter(this, "DBConnPromised", () =>
  Task.spawn(function*() {
    let db = yield PlacesUtils.promiseWrappedConnection();
    try {
      Sqlite.shutdown.addBlocker(
        "Places History.jsm: Closing database wrapper",
        Task.async(function*() {
          yield operationsBarrier.wait();
          gIsClosed = true;
          yield db.close();
        }),
        () => ({
          fetchState: () => ({
            isClosed: gIsClosed,
            operations: operationsBarrier.state,
          })
        })
      );
    } catch (ex) {
      // It's too late to block shutdown of Sqlite, so close the connection
      // immediately.
      db.close();
      throw ex;
    }
    return db;
  })
);

/**
 * `true` once this module has been shutdown.
 */
let gIsClosed = false;
function ensureModuleIsOpen() {
  if (gIsClosed) {
    throw new Error("History.jsm has been shutdown");
  }
}

/**
 * Sends a bookmarks notification through the given observers.
 *
 * @param observers
 *        array of nsINavBookmarkObserver objects.
 * @param notification
 *        the notification name.
 * @param args
 *        array of arguments to pass to the notification.
 */
function notify(observers, notification, args = []) {
  for (let observer of observers) {
    try {
      observer[notification](...args);
    } catch (ex) {}
  }
}

this.History = Object.freeze({
  /**
   * Fetch the available information for one page.
   *
   * @param guidOrURI: (URL or nsIURI)
   *      The full URI of the page.
   *            or (string)
   *      Either the full URI of the page or the GUID of the page.
   *
   * @return (Promise)
   *      A promise resolved once the operation is complete.
   * @resolves (PageInfo | null) If the page could be found, the information
   *      on that page. Note that this `PageInfo` does NOT contain the visit
   *      data (i.e. `visits` is `undefined`).
   *
   * @throws (Error)
   *      If `guidOrURI` does not have the expected type or if it is a string
   *      that may be parsed neither as a valid URL nor as a valid GUID.
   */
  fetch: function (guidOrURI) {
    throw new Error("Method not implemented");
  },

  /**
   * Adds a set of visits for one or more page.
   *
   * Any change may be observed through nsINavHistoryObserver
   *
   * @note This function recomputes the frecency of the page automatically,
   * regardless of the value of property `frecency` passed as argument.
   * @note If there is no entry for the page, the entry is created.
   *
   * @param infos: (PageInfo)
   *      Information on a page. This `PageInfo` MUST contain
   *        - either a property `guid` or a property `url`, as specified
   *          by the definition of `PageInfo`;
   *        - a property `visits`, as specified by the definition of
   *          `PageInfo`, which MUST contain at least one visit.
   *      If a property `title` is provided, the title of the page
   *      is updated.
   *      If the `visitDate` of a visit is not provided, it defaults
   *      to now.
   *            or (Array<PageInfo>)
   *      An array of the above, to batch requests.
   * @param onResult: (function(PageInfo), [optional])
   *      A callback invoked for each page, with the updated
   *      information on that page. Note that this `PageInfo`
   *      does NOT contain the visit data (i.e. `visits` is
   *      `undefined`).
   *
   * @return (Promise)
   *      A promise resolved once the operation is complete, including
   *      all calls to `onResult`.
   * @resolves (bool)
   *      `true` if at least one page entry was created, `false` otherwise
   *       (i.e. if page entries were updated but not created).
   *
   * @throws (Error)
   *      If the `url` specified was for a protocol that should not be
   *      stored (e.g. "chrome:", "mailbox:", "about:", "imap:", "news:",
   *      "moz-anno:", "view-source:", "resource:", "data:", "wyciwyg:",
   *      "javascript:", "blob:").
   * @throws (Error)
   *      If `infos` has an unexpected type.
   * @throws (Error)
   *      If a `PageInfo` has neither `guid` nor `url`.
   * @throws (Error)
   *      If a `guid` property provided is not a valid GUID.
   * @throws (Error)
   *      If a `PageInfo` does not have a `visits` property or if the
   *      value of `visits` is ill-typed or is an empty array.
   * @throws (Error)
   *      If an element of `visits` has an invalid `date`.
   * @throws (Error)
   *      If an element of `visits` is missing `transition` or if
   *      the value of `transition` is invalid.
   */
  update: function (infos, onResult) {
    throw new Error("Method not implemented");
  },

  /**
   * Remove pages from the database.
   *
   * Any change may be observed through nsINavHistoryObserver
   *
   *
   * @param page: (URL or nsIURI)
   *      The full URI of the page.
   *             or (string)
   *      Either the full URI of the page or the GUID of the page.
   *             or (Array<URL|nsIURI|string>)
   *      An array of the above, to batch requests.
   * @param onResult: (function(PageInfo))
   *      A callback invoked for each page found.
   *
   * @return (Promise)
   *      A promise resolved once the operation is complete.
   * @resolve (bool)
   *      `true` if at least one page was removed, `false` otherwise.
   * @throws (TypeError)
   *       If `pages` has an unexpected type or if a string provided
   *       is neither a valid GUID nor a valid URI or if `pages`
   *       is an empty array.
   */
  remove: function (pages, onResult = null) {
    ensureModuleIsOpen();

    // Normalize and type-check arguments
    if (Array.isArray(pages)) {
      if (pages.length == 0) {
        throw new TypeError("Expected at least one page");
      }
    } else {
      pages = [pages];
    }

    let guids = [];
    let urls = [];
    for (let page of pages) {
      // Normalize to URL or GUID, or throw if `page` cannot
      // be normalized.
      let normalized = normalizeToURLOrGUID(page);
      if (typeof normalized === "string") {
        guids.push(normalized);
      } else {
        urls.push(normalized.href);
      }
    }
    let normalizedPages = {guids: guids, urls: urls};

    // At this stage, we know that either `guids` is not-empty
    // or `urls` is not-empty.

    if (onResult && typeof onResult != "function") {
      throw new TypeError("Invalid function: " + onResult);
    }

    return Task.spawn(function*() {
      let promise = remove(normalizedPages, onResult);

      operationsBarrier.client.addBlocker(
        "History.remove",
        promise,
        {
          // In case of crash, we do not want to upload information on
          // which urls are being cleared, for privacy reasons. GUIDs
          // are safe wrt privacy, but useless.
          fetchState: () => ({
            guids: guids.length,
            urls: normalizedPages.urls.map(u => u.protocol),
          })
        });

      try {
        return (yield promise);
      } finally {
        // Cleanup the barrier.
        operationsBarrier.client.removeBlocker(promise);
      }
    });
  },

  /**
   * Remove visits matching specific characteristics.
   *
   * Any change may be observed through nsINavHistoryObserver.
   *
   * @param filter: (object)
   *      The `object` may contain some of the following
   *      properties:
   *          - beginDate: (Date) Remove visits that have
   *                been added since this date (inclusive).
   *          - endDate: (Date) Remove visits that have
   *                been added before this date (inclusive).
   *      If both `beginDate` and `endDate` are specified,
   *      visits between `beginDate` (inclusive) and `end`
   *      (inclusive) are removed.
   *
   * @param onResult: (function(VisitInfo), [optional])
   *     A callback invoked for each visit found and removed.
   *     Note that the referrer property of `VisitInfo`
   *     is NOT populated.
   *
   * @return (Promise)
   * @resolve (bool)
   *      `true` if at least one visit was removed, `false`
   *      otherwise.
   * @throws (TypeError)
   *      If `filter` does not have the expected type, in
   *      particular if the `object` is empty.
   */
  removeVisitsByFilter: function(filter, onResult = null) {
    ensureModuleIsOpen();

    if (!filter || typeof filter != "object") {
      throw new TypeError("Expected a filter");
    }

    let hasBeginDate = "beginDate" in filter;
    let hasEndDate = "endDate" in filter;
    if (hasBeginDate) {
      ensureDate(filter.beginDate);
    }
    if (hasEndDate) {
      ensureDate(filter.endDate);
    }
    if (hasBeginDate && hasEndDate && filter.beginDate > filter.endDate) {
      throw new TypeError("`beginDate` should be at least as old as `endDate`");
    }
    if (!hasBeginDate && !hasEndDate) {
      throw new TypeError("Expected a non-empty filter");
    }

    if (onResult && typeof onResult != "function") {
      throw new TypeError("Invalid function: " + onResult);
    }

    return Task.spawn(function*() {
      let promise = removeVisitsByFilter(filter, onResult);

      operationsBarrier.client.addBlocker(
        "History.removeVisitsByFilter",
        promise
      );

      try {
        return (yield promise);
      } finally {
        // Cleanup the barrier.
        operationsBarrier.client.removeBlocker(promise);
      }
    });
  },

  /**
   * Determine if a page has been visited.
   *
   * @param pages: (URL or nsIURI)
   *      The full URI of the page.
   *            or (string)
   *      The full URI of the page or the GUID of the page.
   *
   * @return (Promise)
   *      A promise resolved once the operation is complete.
   * @resolve (bool)
   *      `true` if the page has been visited, `false` otherwise.
   * @throws (Error)
   *      If `pages` has an unexpected type or if a string provided
   *      is neither not a valid GUID nor a valid URI.
   */
  hasVisits: function(page, onResult) {
    throw new Error("Method not implemented");
  },

  /**
   * Clear all history.
   *
   * @return (Promise)
   *      A promise resolved once the operation is complete.
   */
  clear() {
    ensureModuleIsOpen();

    return Task.spawn(function* () {
      let promise = clear();
      operationsBarrier.client.addBlocker("History.clear", promise);

      try {
        return (yield promise);
      } finally {
        // Cleanup the barrier.
        operationsBarrier.client.removeBlocker(promise);
      }
    });
  },

  /**
   * Possible values for the `transition` property of `VisitInfo`
   * objects.
   */

  /**
   * The user followed a link and got a new toplevel window.
   */
  TRANSITION_LINK: Ci.nsINavHistoryService.TRANSITION_LINK,

  /**
   * The user typed the page's URL in the URL bar or selected it from
   * URL bar autocomplete results, clicked on it from a history query
   * (from the History sidebar, History menu, or history query in the
   * personal toolbar or Places organizer.
   */
  TRANSITION_TYPED: Ci.nsINavHistoryService.TRANSITION_TYPED,

  /**
   * The user followed a bookmark to get to the page.
   */
  TRANSITION_BOOKMARK: Ci.nsINavHistoryService.TRANSITION_BOOKMARK,

  /**
   * Some inner content is loaded. This is true of all images on a
   * page, and the contents of the iframe. It is also true of any
   * content in a frame if the user did not explicitly follow a link
   * to get there.
   */
  TRANSITION_EMBED: Ci.nsINavHistoryService.TRANSITION_EMBED,

  /**
   * Set when the transition was a permanent redirect.
   */
  TRANSITION_REDIRECT_PERMANENT: Ci.nsINavHistoryService.TRANSITION_REDIRECT_PERMANENT,

  /**
   * Set when the transition was a temporary redirect.
   */
  TRANSITION_REDIRECT_TEMPORARY: Ci.nsINavHistoryService.TRANSITION_REDIRECT_TEMPORARY,

  /**
   * Set when the transition is a download.
   */
  TRANSITION_DOWNLOAD: Ci.nsINavHistoryService.TRANSITION_REDIRECT_DOWNLOAD,

  /**
   * The user followed a link and got a visit in a frame.
   */
  TRANSITION_FRAMED_LINK: Ci.nsINavHistoryService.TRANSITION_FRAMED_LINK,
});


/**
 * Normalize a key to either a string (if it is a valid GUID) or an
 * instance of `URL` (if it is a `URL`, `nsIURI`, or a string
 * representing a valid url).
 *
 * @throws (TypeError)
 *         If the key is neither a valid guid nor a valid url.
 */
function normalizeToURLOrGUID(key) {
  if (typeof key === "string") {
    // A string may be a URL or a guid
    if (/^[a-zA-Z0-9\-_]{12}$/.test(key)) {
      return key;
    }
    return new URL(key);
  }
  if (key instanceof URL) {
    return key;
  }
  if (key instanceof Ci.nsIURI) {
    return new URL(key.spec);
  }
  throw new TypeError("Invalid url or guid: " + key);
}

/**
 * Throw if an object is not a Date object.
 */
function ensureDate(arg) {
  if (!arg || typeof arg != "object" || arg.constructor.name != "Date") {
    throw new TypeError("Expected a Date, got " + arg);
  }
}

/**
 * Convert a list of strings or numbers to its SQL
 * representation as a string.
 */
function sqlList(list) {
  return list.map(JSON.stringify).join();
}

/**
 * Invalidate and recompute the frecency of a list of pages,
 * informing frecency observers.
 *
 * @param db: (Sqlite connection)
 * @param idList: (Array)
 *      The `moz_places` identifiers for the places to invalidate.
 * @return (Promise)
 */
let invalidateFrecencies = Task.async(function*(db, idList) {
  if (idList.length == 0) {
    return;
  }
  let ids = sqlList(idList);
  yield db.execute(
    `UPDATE moz_places
     SET frecency = NOTIFY_FRECENCY(
       CALCULATE_FRECENCY(id), url, guid, hidden, last_visit_date
     ) WHERE id in (${ ids })`
  );
  yield db.execute(
    `UPDATE moz_places
     SET hidden = 0
     WHERE id in (${ ids })
     AND frecency <> 0`
  );
});

// Inner implementation of History.clear().
let clear = Task.async(function* () {
  let db = yield DBConnPromised;

  // Remove all history.
  yield db.execute("DELETE FROM moz_historyvisits");

  // Clear the registered embed visits.
  PlacesUtils.history.clearEmbedVisits();

  // Expiration will take care of orphans.
  let observers = PlacesUtils.history.getObservers();
  notify(observers, "onClearHistory");

  // Invalidate frecencies for the remaining places. This must happen
  // after the notification to ensure it runs enqueued to expiration.
  yield db.execute(
    `UPDATE moz_places SET frecency =
     (CASE
      WHEN url BETWEEN 'place:' AND 'place;'
      THEN 0
      ELSE -1
      END)
     WHERE frecency > 0`);

  // Notify frecency change observers.
  notify(observers, "onManyFrecenciesChanged");
});

/**
 * Remove a list of pages from `moz_places` by their id.
 *
 * @param db: (Sqlite connection)
 *      The database.
 * @param idList: (Array of integers)
 *      The `moz_places` identifiers for the places to remove.
 * @return (Promise)
 */
let removePagesById = Task.async(function*(db, idList) {
  if (idList.length == 0) {
    return;
  }
  yield db.execute(`DELETE FROM moz_places
                    WHERE id IN ( ${ sqlList(idList) } )`);
});

/**
 * Clean up pages whose history has been modified, by either
 * removing them entirely (if they are marked for removal,
 * typically because all visits have been removed and there
 * are no more foreign keys such as bookmarks) or updating
 * their frecency (otherwise).
 *
 * @param db: (Sqlite connection)
 *      The database.
 * @param pages: (Array of objects)
 *      Pages that have been touched and that need cleaning up.
 *      Each object should have the following properties:
 *          - id: (number) The `moz_places` identifier for the place.
 *          - hasVisits: (boolean) If `true`, there remains at least one
 *              visit to this page, so the page should be kept and its
 *              frecency updated.
 *          - hasForeign: (boolean) If `true`, the page has at least
 *              one foreign reference (i.e. a bookmark), so the page should
 *              be kept and its frecency updated.
 * @return (Promise)
 */
let cleanupPages = Task.async(function*(db, pages) {
  yield invalidateFrecencies(db, [p.id for (p of pages) if (p.hasForeign || p.hasVisits)]);
  yield removePagesById(db, [p.id for (p of pages) if (!p.hasForeign && !p.hasVisits)]);
});

/**
 * Notify observers that pages have been removed/updated.
 *
 * @param db: (Sqlite connection)
 *      The database.
 * @param pages: (Array of objects)
 *      Pages that have been touched and that need cleaning up.
 *      Each object should have the following properties:
 *          - id: (number) The `moz_places` identifier for the place.
 *          - hasVisits: (boolean) If `true`, there remains at least one
 *              visit to this page, so the page should be kept and its
 *              frecency updated.
 *          - hasForeign: (boolean) If `true`, the page has at least
 *              one foreign reference (i.e. a bookmark), so the page should
 *              be kept and its frecency updated.
 * @return (Promise)
 */
let notifyCleanup = Task.async(function*(db, pages) {
  let notifiedCount = 0;
  let observers = PlacesUtils.history.getObservers();

  let reason = Ci.nsINavHistoryObserver.REASON_DELETED;

  for (let page of pages) {
    let uri = NetUtil.newURI(page.url.href);
    let guid = page.guid;
    if (page.hasVisits) {
      // For the moment, we do not have the necessary observer API
      // to notify when we remove a subset of visits, see bug 937560.
      continue;
    }
    if (page.hasForeign) {
      // We have removed all visits, but the page is still alive, e.g.
      // because of a bookmark.
      notify(observers, "onDeleteVisits",
        [uri, /*last visit*/0, guid, reason, -1]);
    } else {
      // The page has been entirely removed.
      notify(observers, "onDeleteURI",
        [uri, guid, reason]);
    }
    if (++notifiedCount % NOTIFICATION_CHUNK_SIZE == 0) {
      // Every few notifications, yield time back to the main
      // thread to avoid jank.
      yield Promise.resolve();
    }
  }
});

/**
 * Notify an `onResult` callback of a set of operations
 * that just took place.
 *
 * @param data: (Array)
 *      The data to send to the callback.
 * @param onResult: (function [optional])
 *      If provided, call `onResult` with `data[0]`, `data[1]`, etc.
 *      Otherwise, do nothing.
 */
let notifyOnResult = Task.async(function*(data, onResult) {
  if (!onResult) {
    return;
  }
  let notifiedCount = 0;
  for (let info of data) {
    try {
      onResult(info);
    } catch (ex) {
      // Errors should be reported but should not stop the operation.
      Promise.reject(ex);
    }
    if (++notifiedCount % ONRESULT_CHUNK_SIZE == 0) {
      // Every few notifications, yield time back to the main
      // thread to avoid jank.
      yield Promise.resolve();
    }
  }
});

// Inner implementation of History.removeVisitsByFilter.
let removeVisitsByFilter = Task.async(function*(filter, onResult = null) {
  let db = yield DBConnPromised;

  // 1. Determine visits that took place during the interval.  Note
  // that the database uses microseconds, while JS uses milliseconds,
  // so we need to *1000 one way and /1000 the other way.
  let dates = {
    conditions: [],
    args: {},
  };
  if ("beginDate" in filter) {
    dates.conditions.push("visit_date >= :begin * 1000");
    dates.args.begin = Number(filter.beginDate);
  }
  if ("endDate" in filter) {
    dates.conditions.push("visit_date <= :end * 1000");
    dates.args.end = Number(filter.endDate);
  }

  let visitsToRemove = [];
  let pagesToInspect = new Set();
  let onResultData = onResult ? [] : null;

  yield db.executeCached(
    `SELECT id, place_id, visit_date / 1000 AS date, visit_type FROM moz_historyvisits
     WHERE ${ dates.conditions.join(" AND ") }`,
     dates.args,
     row => {
       let id = row.getResultByName("id");
       let place_id = row.getResultByName("place_id");
       visitsToRemove.push(id);
       pagesToInspect.add(place_id);

       if (onResult) {
         onResultData.push({
           date: new Date(row.getResultByName("date")),
           transition: row.getResultByName("visit_type")
         });
       }
     }
  );

  try {
    if (visitsToRemove.length == 0) {
      // Nothing to do
      return false;
    }

    let pages = [];
    yield db.executeTransaction(function*() {
      // 2. Remove all offending visits.
      yield db.execute(`DELETE FROM moz_historyvisits
                        WHERE id IN (${ sqlList(visitsToRemove) } )`);

      // 3. Find out which pages have been orphaned
      yield db.execute(
        `SELECT id, url, guid,
          (foreign_count != 0) AS has_foreign,
          (last_visit_date NOTNULL) as has_visits
         FROM moz_places
         WHERE id IN (${ sqlList([...pagesToInspect]) })`,
         null,
         row => {
           let page = {
             id:  row.getResultByName("id"),
             guid: row.getResultByName("guid"),
             hasForeign: row.getResultByName("has_foreign"),
             hasVisits: row.getResultByName("has_visits"),
             url: new URL(row.getResultByName("url")),
           };
           pages.push(page);
         });

      // 4. Clean up and notify
      yield cleanupPages(db, pages);
    });

    notifyCleanup(db, pages);
    notifyOnResult(onResultData, onResult); // don't wait
  } finally {
    // Ensure we cleanup embed visits, even if we bailed out early.
    PlacesUtils.history.clearEmbedVisits();
  }

  return visitsToRemove.length != 0;
});


// Inner implementation of History.remove.
let remove = Task.async(function*({guids, urls}, onResult = null) {
  let db = yield DBConnPromised;
  // 1. Find out what needs to be removed
  let query =
    `SELECT id, url, guid, foreign_count, title, frecency FROM moz_places
     WHERE guid IN (${ sqlList(guids) })
        OR url  IN (${ sqlList(urls)  })
     `;

  let onResultData = onResult ? [] : null;
  let pages = [];
  let hasPagesToKeep = false;
  let hasPagesToRemove = false;
  yield db.execute(query, null, Task.async(function*(row) {
    let hasForeign = row.getResultByName("foreign_count") != 0;
    if (hasForeign) {
      hasPagesToKeep = true;
    } else {
      hasPagesToRemove = true;
    }
    let id = row.getResultByName("id");
    let guid = row.getResultByName("guid");
    let url = row.getResultByName("url");
    let page = {
      id,
      guid,
      hasForeign,
      hasVisits: false,
      url: new URL(url),
    };
    pages.push(page);
    if (onResult) {
      onResultData.push({
        guid: guid,
        title: row.getResultByName("title"),
        frecency: row.getResultByName("frecency"),
        url: new URL(url)
      });
    }
  }));

  try {
    if (pages.length == 0) {
      // Nothing to do
      return false;
    }

    yield db.executeTransaction(function*() {
      // 2. Remove all visits to these pages.
      yield db.execute(`DELETE FROM moz_historyvisits
                        WHERE place_id IN (${ sqlList([p.id for (p of pages)]) })
                       `);

      // 3. Clean up and notify
      yield cleanupPages(db, pages);
    });

    notifyCleanup(db, pages);
    notifyOnResult(onResultData, onResult); // don't wait
  } finally {
    // Ensure we cleanup embed visits, even if we bailed out early.
    PlacesUtils.history.clearEmbedVisits();
  }

  return hasPagesToRemove;
});
