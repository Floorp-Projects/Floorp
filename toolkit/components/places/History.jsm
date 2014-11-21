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
XPCOMUtils.defineLazyServiceGetter(this, "gNotifier",
                                   "@mozilla.org/browser/nav-history-service;1",
                                   Ci.nsPIPlacesHistoryListenersNotifier);
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");
Cu.importGlobalProperties(["URL"]);

/**
 * Whenever we update or remove numerous pages, it is preferable
 * to yield time to the main thread every so often to avoid janking.
 * This constant determines the maximal number of notifications we
 * may emit before we yield.
 */
const NOTIFICATION_CHUNK_SIZE = 300;

/**
 * Private shutdown barrier blocked by ongoing operations.
 */
XPCOMUtils.defineLazyGetter(this, "operationsBarrier", () =>
  new AsyncShutdown.Barrier("Sqlite.jsm: wait until all connections are closed")
);

/**
 * Shared connection
 */
XPCOMUtils.defineLazyGetter(this, "DBConnPromised",
  () => new Promise((resolve) => {
    Sqlite.wrapStorageConnection({ connection: PlacesUtils.history.DBConnection } )
          .then(db => {
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
          }));
      } catch (ex) {
        // It's too late to block shutdown of Sqlite, so close the connection
        // immediately.
        db.close();
        throw ex;
      }
      resolve(db);
    });
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
   *      A promise resoled once the operation is complete.
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
   * Determine if a page has been visited.
   *
   * @param pages: (URL or nsIURI)
   *      The full URI of the page.
   *            or (string)
   *      The full URI of the page or the GUID of the page.
   *
   * @return (Promise)
   *      A promise resoled once the operation is complete.
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


// Inner implementation of History.remove.
let remove = Task.async(function*({guids, urls}, onResult = null) {
  let db = yield DBConnPromised;

  // 1. Find out what needs to be removed
  let query =
    `SELECT id, url, guid, foreign_count, title, frecency FROM moz_places
     WHERE guid IN (${ sqlList(guids) })
        OR url  IN (${ sqlList(urls)  })
     `;

  let pages = [];
  let hasPagesToKeep = false;
  let hasPagesToRemove = false;
  yield db.execute(query, null, Task.async(function*(row) {
    let toRemove = row.getResultByName("foreign_count") == 0;
    if (toRemove) {
      hasPagesToRemove = true;
    } else {
      hasPagesToKeep = true;
    }
    let id = row.getResultByName("id");
    let guid = row.getResultByName("guid");
    let url = row.getResultByName("url");
    let page = {
      id: id,
      guid: guid,
      toRemove: toRemove,
      uri: NetUtil.newURI(url),
    };
    pages.push(page);
    if (onResult) {
      let pageInfo = {
        guid: guid,
        title: row.getResultByName("title"),
        frecency: row.getResultByName("frecency"),
        url: new URL(url)
      };
      try {
        yield onResult(pageInfo);
      } catch (ex) {
        // Errors should be reported but should not stop `remove`.
        Promise.reject(ex);
      }
    }
  }));

  if (pages.length == 0) {
    // Nothing to do
    return false;
  }

  yield db.executeTransaction(function*() {
    // 2. Remove all visits to these pages.
    yield db.execute(`DELETE FROM moz_historyvisits
                      WHERE place_id IN (${ sqlList([p.id for (p of pages)]) })
                     `);

     // 3. For pages that should not be removed, invalidate frecencies.
    if (hasPagesToKeep) {
      yield invalidateFrecencies(db, [p.id for (p of pages) if (!p.toRemove)]);
    }

    // 4. For pages that should be removed, remove page.
    if (hasPagesToRemove) {
      let ids = [p.id for (p of pages) if (p.toRemove)];
      yield db.execute(`DELETE FROM moz_places
                        WHERE id IN (${ sqlList(ids) })
                       `);
    }

    // 5. Notify observers.
    for (let {guid, uri, toRemove} of pages) {
      gNotifier.notifyOnPageExpired(
        uri, // uri
        0, // visitTime - There are no more visits
        toRemove, // wholeEntry
        guid, // guid
        Ci.nsINavHistoryObserver.REASON_DELETED, // reason
        -1 // transition
      );
    }
  });

  PlacesUtils.history.clearEmbedVisits();

  return hasPagesToRemove;
});
