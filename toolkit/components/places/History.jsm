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
 * - description: (string)
 *     The description of the page, if any.
 * - previewImageURL: (URL)
 *     or (nsIURI)
 *     or (string)
 *     The preview image URL of the page, if any.
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
 *     How the user reached the page. See constants `TRANSITIONS.*`
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

// This constant determines the maximum number of remove pages before we cycle.
const REMOVE_PAGES_CHUNKLEN = 300;

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
   * @param guidOrURI: (string) or (URL, nsIURI or href)
   *      Either the full URI of the page or the GUID of the page.
   * @param [optional] options (object)
   *      An optional object whose properties describe options:
   *        - `includeVisits` (boolean) set this to true if `visits` in the
   *           PageInfo needs to contain VisitInfo in a reverse chronological order.
   *           By default, `visits` is undefined inside the returned `PageInfo`.
   *        - `includeMeta` (boolean) set this to true to fetch page meta fields,
   *           i.e. `description` and `preview_image_url`.
   *
   * @return (Promise)
   *      A promise resolved once the operation is complete.
   * @resolves (PageInfo | null) If the page could be found, the information
   *      on that page.
   * @note the VisitInfo objects returned while fetching visits do not
   *       contain the property `referrer`.
   *       TODO: Add `referrer` to VisitInfo. See Bug #1365913.
   * @note the visits returned will not contain `TRANSITION_EMBED` visits.
   *
   * @throws (Error)
   *      If `guidOrURI` does not have the expected type or if it is a string
   *      that may be parsed neither as a valid URL nor as a valid GUID.
   */
  fetch(guidOrURI, options = {}) {
    // First, normalize to guid or string, and throw if not possible
    guidOrURI = PlacesUtils.normalizeToURLOrGUID(guidOrURI);

    // See if options exists and make sense
    if (!options || typeof options !== "object") {
      throw new TypeError("options should be an object and not null");
    }

    let hasIncludeVisits = "includeVisits" in options;
    if (hasIncludeVisits && typeof options.includeVisits !== "boolean") {
      throw new TypeError("includeVisits should be a boolean if exists");
    }

    let hasIncludeMeta = "includeMeta" in options;
    if (hasIncludeMeta && typeof options.includeMeta !== "boolean") {
      throw new TypeError("includeMeta should be a boolean if exists");
    }

    return PlacesUtils.promiseDBConnection()
                      .then(db => fetch(db, guidOrURI, options));
  },

  /**
   * Adds a number of visits for a single page.
   *
   * Any change may be observed through nsINavHistoryObserver
   *
   * @param pageInfo: (PageInfo)
   *      Information on a page. This `PageInfo` MUST contain
   *        - a property `url`, as specified by the definition of `PageInfo`.
   *        - a property `visits`, as specified by the definition of
   *          `PageInfo`, which MUST contain at least one visit.
   *      If a property `title` is provided, the title of the page
   *      is updated.
   *      If the `date` of a visit is not provided, it defaults
   *      to now.
   *      If the `transition` of a visit is not provided, it defaults to
   *      TRANSITION_LINK.
   *
   * @return (Promise)
   *      A promise resolved once the operation is complete.
   * @resolves (PageInfo)
   *      A PageInfo object populated with data after the insert is complete.
   * @rejects (Error)
   *      Rejects if the insert was unsuccessful.
   *
   * @throws (Error)
   *      If the `url` specified was for a protocol that should not be
   *      stored (@see nsNavHistory::CanAddURI).
   * @throws (Error)
   *      If `pageInfo` has an unexpected type.
   * @throws (Error)
   *      If `pageInfo` does not have a `url`.
   * @throws (Error)
   *      If `pageInfo` does not have a `visits` property or if the
   *      value of `visits` is ill-typed or is an empty array.
   * @throws (Error)
   *      If an element of `visits` has an invalid `date`.
   * @throws (Error)
   *      If an element of `visits` has an invalid `transition`.
   */
  insert(pageInfo) {
    let info = PlacesUtils.validatePageInfo(pageInfo);

    return PlacesUtils.withConnectionWrapper("History.jsm: insert",
      db => insert(db, info));
  },

  /**
   * Adds a number of visits for a number of pages.
   *
   * Any change may be observed through nsINavHistoryObserver
   *
   * @param pageInfos: (Array<PageInfo>)
   *      Information on a page. This `PageInfo` MUST contain
   *        - a property `url`, as specified by the definition of `PageInfo`.
   *        - a property `visits`, as specified by the definition of
   *          `PageInfo`, which MUST contain at least one visit.
   *      If a property `title` is provided, the title of the page
   *      is updated.
   *      If the `date` of a visit is not provided, it defaults
   *      to now.
   *      If the `transition` of a visit is not provided, it defaults to
   *      TRANSITION_LINK.
   * @param onResult: (function(PageInfo))
   *      A callback invoked for each page inserted.
   * @param onError: (function(PageInfo))
   *      A callback invoked for each page which generated an error
   *      when an insert was attempted.
   *
   * @return (Promise)
   *      A promise resolved once the operation is complete.
   * @resolves (null)
   * @rejects (Error)
   *      Rejects if all of the inserts were unsuccessful.
   *
   * @throws (Error)
   *      If the `url` specified was for a protocol that should not be
   *      stored (@see nsNavHistory::CanAddURI).
   * @throws (Error)
   *      If `pageInfos` has an unexpected type.
   * @throws (Error)
   *      If a `pageInfo` does not have a `url`.
   * @throws (Error)
   *      If a `PageInfo` does not have a `visits` property or if the
   *      value of `visits` is ill-typed or is an empty array.
   * @throws (Error)
   *      If an element of `visits` has an invalid `date`.
   * @throws (Error)
   *      If an element of `visits` has an invalid `transition`.
   */
  insertMany(pageInfos, onResult, onError) {
    let infos = [];

    if (!Array.isArray(pageInfos)) {
      throw new TypeError("pageInfos must be an array");
    }
    if (!pageInfos.length) {
      throw new TypeError("pageInfos may not be an empty array");
    }

    if (onResult && typeof onResult != "function") {
      throw new TypeError(`onResult: ${onResult} is not a valid function`);
    }
    if (onError && typeof onError != "function") {
      throw new TypeError(`onError: ${onError} is not a valid function`);
    }

    for (let pageInfo of pageInfos) {
      let info = PlacesUtils.validatePageInfo(pageInfo);
      infos.push(info);
    }

    return PlacesUtils.withConnectionWrapper("History.jsm: insertMany",
      db => insertMany(db, infos, onResult, onError));
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
  remove(pages, onResult = null) {
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
      let normalized = PlacesUtils.normalizeToURLOrGUID(page);
      if (typeof normalized === "string") {
        guids.push(normalized);
      } else {
        urls.push(normalized.href);
      }
    }

    // At this stage, we know that either `guids` is not-empty
    // or `urls` is not-empty.

    if (onResult && typeof onResult != "function") {
      throw new TypeError("Invalid function: " + onResult);
    }

    return (async function() {
      let removedPages = false;
      let count = 0;
      while (guids.length || urls.length) {
        if (count && count % 2 == 0) {
          // Every few cycles, yield time back to the main
          // thread to avoid jank.
          await Promise.resolve();
        }
        count++;
        let guidsSlice = guids.splice(0, REMOVE_PAGES_CHUNKLEN);
        let urlsSlice = [];
        if (guidsSlice.length < REMOVE_PAGES_CHUNKLEN) {
          urlsSlice = urls.splice(0, REMOVE_PAGES_CHUNKLEN - guidsSlice.length);
        }

        let pages = {guids: guidsSlice, urls: urlsSlice};

        let result =
          await PlacesUtils.withConnectionWrapper("History.jsm: remove",
                                                  db => remove(db, pages, onResult));

        removedPages = removedPages || result;
      }
      return removedPages;
    })();
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
   *          - limit: (Number) Limit the number of visits
   *                we remove to this number
   *          - url: (URL) Only remove visits to this URL
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
  removeVisitsByFilter(filter, onResult = null) {
    if (!filter || typeof filter != "object") {
      throw new TypeError("Expected a filter");
    }

    let hasBeginDate = "beginDate" in filter;
    let hasEndDate = "endDate" in filter;
    let hasURL = "url" in filter;
    let hasLimit = "limit" in filter;
    if (hasBeginDate) {
      this.ensureDate(filter.beginDate);
    }
    if (hasEndDate) {
      this.ensureDate(filter.endDate);
    }
    if (hasBeginDate && hasEndDate && filter.beginDate > filter.endDate) {
      throw new TypeError("`beginDate` should be at least as old as `endDate`");
    }
    if (!hasBeginDate && !hasEndDate && !hasURL && !hasLimit) {
      throw new TypeError("Expected a non-empty filter");
    }

    if (hasURL && !(filter.url instanceof URL) && typeof filter.url != "string" &&
        !(filter.url instanceof Ci.nsIURI)) {
      throw new TypeError("Expected a valid URL for `url`");
    }

    if (hasLimit &&
        (typeof filter.limit != "number" ||
         filter.limit <= 0 ||
         !Number.isInteger(filter.limit))) {
      throw new TypeError("Expected a non-zero positive integer as a limit");
    }

    if (onResult && typeof onResult != "function") {
      throw new TypeError("Invalid function: " + onResult);
    }

    return PlacesUtils.withConnectionWrapper("History.jsm: removeVisitsByFilter",
      db => removeVisitsByFilter(db, filter, onResult)
    );
  },

  /**
   * Remove pages from the database based on a filter.
   *
   * Any change may be observed through nsINavHistoryObserver
   *
   *
   * @param filter: An object containing a non empty subset of the following
   * properties:
   * - host: (string)
   *     Hostname with subhost wildcard (at most one *), or empty for local files.
   *     The * can be used only if it is the first character in the url, and not the host.
   *     For example, *.mozilla.org is allowed, *.org, www.*.org or * is not allowed.
   * - beginDate: (Date)
   *     The first time the page was visited (inclusive)
   * - endDate: (Date)
   *     The last time the page was visited (inclusive)
   * @param [optional] onResult: (function(PageInfo))
   *      A callback invoked for each page found.
   *
   * @note This removes pages with at least one visit inside the timeframe.
   *       Any visits outside the timeframe will also be removed with the page.
   * @return (Promise)
   *      A promise resolved once the operation is complete.
   * @resolve (bool)
   *      `true` if at least one page was removed, `false` otherwise.
   * @throws (TypeError)
   *       if `filter` does not have the expected type, in particular
   *       if the `object` is empty, or its components do not satisfy the
   *       criteria given above
   */
  removeByFilter(filter, onResult) {
    if (!filter || typeof filter !== "object") {
      throw new TypeError("Expected a filter object");
    }

    let hasHost = "host" in filter;
    if (hasHost) {
      if (typeof filter.host !== "string") {
        throw new TypeError("`host` should be a string");
      }
      filter.host = filter.host.toLowerCase();
    }

    let hasBeginDate = "beginDate" in filter;
    if (hasBeginDate) {
      this.ensureDate(filter.beginDate);
    }

    let hasEndDate = "endDate" in filter;
    if (hasEndDate) {
      this.ensureDate(filter.endDate);
    }

    if (hasBeginDate && hasEndDate && filter.beginDate > filter.endDate) {
      throw new TypeError("`beginDate` should be at least as old as `endDate`");
    }

    if (!hasBeginDate && !hasEndDate && !hasHost) {
      throw new TypeError("Expected a non-empty filter");
    }

    // Host should follow one of these formats
    // The first one matches `localhost` or any other custom set in hostsfile
    // The second one matches *.mozilla.org or mozilla.com etc
    // The third one is for local files
    if (hasHost &&
        !((/^[a-z0-9-]+$/).test(filter.host)) &&
        !((/^(\*\.)?([a-z0-9-]+)(\.[a-z0-9-]+)+$/).test(filter.host)) &&
        (filter.host !== "")) {
      throw new TypeError("Expected well formed hostname string for `host` with atmost 1 wildcard.");
    }

    if (onResult && typeof onResult != "function") {
      throw new TypeError("Invalid function: " + onResult);
    }

    return PlacesUtils.withConnectionWrapper(
      "History.jsm: removeByFilter",
      db => removeByFilter(db, filter, onResult)
    );
  },

  /**
   * Determine if a page has been visited.
   *
   * @param guidOrURI: (string) or (URL, nsIURI or href)
   *      Either the full URI of the page or the GUID of the page.
   * @return (Promise)
   *      A promise resolved once the operation is complete.
   * @resolve (bool)
   *      `true` if the page has been visited, `false` otherwise.
   * @throws (Error)
   *      If `guidOrURI` has an unexpected type or if a string provided
   *      is neither not a valid GUID nor a valid URI.
   */
  hasVisits(guidOrURI) {
    // Quick fallback to the cpp version.
    if (guidOrURI instanceof Ci.nsIURI) {
      return new Promise(resolve => {
        PlacesUtils.asyncHistory.isURIVisited(guidOrURI, (aURI, aIsVisited) => {
          resolve(aIsVisited);
        });
      });
    }

    guidOrURI = PlacesUtils.normalizeToURLOrGUID(guidOrURI);
    let isGuid = typeof guidOrURI == "string";
    let sqlFragment = isGuid ? "guid = :val"
                             : "url_hash = hash(:val) AND url = :val ";

    return PlacesUtils.promiseDBConnection().then(async db => {
      let rows = await db.executeCached(`SELECT 1 FROM moz_places
                                         WHERE ${sqlFragment}
                                         AND last_visit_date NOTNULL`,
                                        { val: isGuid ? guidOrURI : guidOrURI.href });
      return !!rows.length;
    });
  },

  /**
   * Clear all history.
   *
   * @return (Promise)
   *      A promise resolved once the operation is complete.
   */
  clear() {
    return PlacesUtils.withConnectionWrapper("History.jsm: clear",
      clear
    );
  },

  /**
   * Is a value a valid transition type?
   *
   * @param transitionType: (String)
   * @return (Boolean)
   */
  isValidTransition(transitionType) {
    return Object.values(History.TRANSITIONS).includes(transitionType);
  },

  /**
   * Throw if an object is not a Date object.
   */
  ensureDate(arg) {
    if (!arg || typeof arg != "object" || arg.constructor.name != "Date") {
      throw new TypeError("Expected a Date, got " + arg);
    }
  },

   /**
   * Update information for a page.
   *
   * Currently, it supports updating the description and the preview image URL
   * for a page, any other fields will be ignored.
   *
   * Note that this function will ignore the update if the target page has not
   * yet been stored in the database. `History.fetch` could be used to check
   * whether the page and its meta information exist or not. Beware that
   * fetch&update might fail as they are not executed in a single transaction.
   *
   * @param pageInfo: (PageInfo)
   *      pageInfo must contain a URL of the target page. It will be ignored
   *      if a valid page `guid` is also provided.
   *
   *      If a property `description` is provided, the description of the
   *      page is updated. Note that:
   *      1). An empty string or null `description` will clear the existing
   *          value in the database.
   *      2). Descriptions longer than DB_DESCRIPTION_LENGTH_MAX will be
   *          truncated.
   *
   *      If a property `previewImageURL` is provided, the preview image
   *      URL of the page is updated. Note that:
   *      1). A null `previewImageURL` will clear the existing value in the
   *          database.
   *      2). It throws if its length is greater than DB_URL_LENGTH_MAX
   *          defined in PlacesUtils.jsm.
   *
   * @return (Promise)
   *      A promise resolved once the update is complete.
   * @rejects (Error)
   *      Rejects if the update was unsuccessful.
   *
   * @throws (Error)
   *      If `pageInfo` has an unexpected type.
   * @throws (Error)
   *      If `pageInfo` has an invalid `url` or an invalid `guid`.
   * @throws (Error)
   *      If `pageInfo` has neither `description` nor `previewImageURL`.
   * @throws (Error)
   *      If the length of `pageInfo.previewImageURL` is greater than
   *      DB_URL_LENGTH_MAX defined in PlacesUtils.jsm.
   */
  update(pageInfo) {
    let info = PlacesUtils.validatePageInfo(pageInfo, false);

    if (info.description === undefined && info.previewImageURL === undefined) {
      throw new TypeError("pageInfo object must at least have either a description or a previewImageURL property");
    }

    return PlacesUtils.withConnectionWrapper("History.jsm: update", db => update(db, info));
  },


  /**
   * Possible values for the `transition` property of `VisitInfo`
   * objects.
   */

  TRANSITIONS: {
    /**
     * The user followed a link and got a new toplevel window.
     */
    LINK: Ci.nsINavHistoryService.TRANSITION_LINK,

    /**
     * The user typed the page's URL in the URL bar or selected it from
     * URL bar autocomplete results, clicked on it from a history query
     * (from the History sidebar, History menu, or history query in the
     * personal toolbar or Places organizer.
     */
    TYPED: Ci.nsINavHistoryService.TRANSITION_TYPED,

    /**
     * The user followed a bookmark to get to the page.
     */
    BOOKMARK: Ci.nsINavHistoryService.TRANSITION_BOOKMARK,

    /**
     * Some inner content is loaded. This is true of all images on a
     * page, and the contents of the iframe. It is also true of any
     * content in a frame if the user did not explicitly follow a link
     * to get there.
     */
    EMBED: Ci.nsINavHistoryService.TRANSITION_EMBED,

    /**
     * Set when the transition was a permanent redirect.
     */
    REDIRECT_PERMANENT: Ci.nsINavHistoryService.TRANSITION_REDIRECT_PERMANENT,

    /**
     * Set when the transition was a temporary redirect.
     */
    REDIRECT_TEMPORARY: Ci.nsINavHistoryService.TRANSITION_REDIRECT_TEMPORARY,

    /**
     * Set when the transition is a download.
     */
    DOWNLOAD: Ci.nsINavHistoryService.TRANSITION_DOWNLOAD,

    /**
     * The user followed a link and got a visit in a frame.
     */
    FRAMED_LINK: Ci.nsINavHistoryService.TRANSITION_FRAMED_LINK,

    /**
     * The user reloaded a page.
     */
    RELOAD: Ci.nsINavHistoryService.TRANSITION_RELOAD,
  },
});

/**
 * Convert a PageInfo object into the format expected by updatePlaces.
 *
 * Note: this assumes that the PageInfo object has already been validated
 * via PlacesUtils.validatePageInfo.
 *
 * @param pageInfo: (PageInfo)
 * @return (info)
 */
function convertForUpdatePlaces(pageInfo) {
  let info = {
    guid: pageInfo.guid,
    uri: PlacesUtils.toURI(pageInfo.url),
    title: pageInfo.title,
    visits: [],
  };

  for (let inVisit of pageInfo.visits) {
    let visit = {
      visitDate: PlacesUtils.toPRTime(inVisit.date),
      transitionType: inVisit.transition,
      referrerURI: (inVisit.referrer) ? PlacesUtils.toURI(inVisit.referrer) : undefined,
    };
    info.visits.push(visit);
  }
  return info;
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
var invalidateFrecencies = async function(db, idList) {
  if (idList.length == 0) {
    return;
  }
  let ids = sqlList(idList);
  await db.execute(
    `UPDATE moz_places
     SET frecency = NOTIFY_FRECENCY(
       CALCULATE_FRECENCY(id), url, guid, hidden, last_visit_date
     ) WHERE id in (${ ids })`
  );
  await db.execute(
    `UPDATE moz_places
     SET hidden = 0
     WHERE id in (${ ids })
     AND frecency <> 0`
  );
};

// Inner implementation of History.clear().
var clear = async function(db) {
  await db.executeTransaction(async function() {
    // Remove all non-bookmarked places entries first, this will speed up the
    // triggers work.
    await db.execute(`DELETE FROM moz_places WHERE foreign_count = 0`);
    await db.execute(`DELETE FROM moz_updatehostsdelete_temp`);

    // Expire orphan icons.
    await db.executeCached(`DELETE FROM moz_pages_w_icons
                            WHERE page_url_hash NOT IN (SELECT url_hash FROM moz_places)`);
    await db.executeCached(`DELETE FROM moz_icons WHERE id IN (
                              SELECT id FROM moz_icons WHERE root = 0
                              EXCEPT
                              SELECT icon_id FROM moz_icons_to_pages
                            )`);

    // Expire annotations.
    await db.execute(`DELETE FROM moz_items_annos WHERE expiration = :expire_session`,
                     { expire_session: Ci.nsIAnnotationService.EXPIRE_SESSION });
    await db.execute(`DELETE FROM moz_annos WHERE id in (
                        SELECT a.id FROM moz_annos a
                        LEFT JOIN moz_places h ON a.place_id = h.id
                        WHERE h.id IS NULL
                           OR expiration = :expire_session
                           OR (expiration = :expire_with_history
                               AND h.last_visit_date ISNULL)
                      )`, { expire_session: Ci.nsIAnnotationService.EXPIRE_SESSION,
                            expire_with_history: Ci.nsIAnnotationService.EXPIRE_WITH_HISTORY });

    // Expire inputhistory.
    await db.execute(`DELETE FROM moz_inputhistory WHERE place_id IN (
                        SELECT i.place_id FROM moz_inputhistory i
                        LEFT JOIN moz_places h ON h.id = i.place_id
                        WHERE h.id IS NULL)`);

    // Remove all history.
    await db.execute("DELETE FROM moz_historyvisits");

    // Invalidate frecencies for the remaining places.
    await db.execute(`UPDATE moz_places SET frecency =
                        (CASE
                          WHEN url_hash BETWEEN hash("place", "prefix_lo") AND
                                                hash("place", "prefix_hi")
                          THEN 0
                          ELSE -1
                          END)
                        WHERE frecency > 0`);
  });

  // Clear the registered embed visits.
  PlacesUtils.history.clearEmbedVisits();

  let observers = PlacesUtils.history.getObservers();
  notify(observers, "onClearHistory");
  // Notify frecency change observers.
  notify(observers, "onManyFrecenciesChanged");
};

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
var cleanupPages = async function(db, pages) {
  await invalidateFrecencies(db, pages.filter(p => p.hasForeign || p.hasVisits).map(p => p.id));

  let pagesToRemove = pages.filter(p => !p.hasForeign && !p.hasVisits);
  if (pagesToRemove.length == 0)
    return;

  let idsList = sqlList(pagesToRemove.map(p => p.id));
  // Note, we are already in a transaction, since callers create it.
  // Check relations regardless, to avoid creating orphans in case of
  // async race conditions.
  await db.execute(`DELETE FROM moz_places WHERE id IN ( ${ idsList } )
                    AND foreign_count = 0 AND last_visit_date ISNULL`);
  // Hosts accumulated during the places delete are updated through a trigger
  // (see nsPlacesTriggers.h).
  await db.executeCached(`DELETE FROM moz_updatehostsdelete_temp`);

  // Expire orphans.
  let hashesToRemove = pagesToRemove.map(p => p.hash);
  await db.executeCached(`DELETE FROM moz_pages_w_icons
                          WHERE page_url_hash IN (${sqlList(hashesToRemove)})`);
  await db.executeCached(`DELETE FROM moz_icons WHERE id IN (
                            SELECT id FROM moz_icons WHERE root = 0
                            EXCEPT
                            SELECT icon_id FROM moz_icons_to_pages
                          )`);

  await db.execute(`DELETE FROM moz_annos
                    WHERE place_id IN ( ${ idsList } )`);
  await db.execute(`DELETE FROM moz_inputhistory
                    WHERE place_id IN ( ${ idsList } )`);
};

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
var notifyCleanup = async function(db, pages) {
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
        [uri, /* last visit*/0, guid, reason, -1]);
    } else {
      // The page has been entirely removed.
      notify(observers, "onDeleteURI",
        [uri, guid, reason]);
    }
    if (++notifiedCount % NOTIFICATION_CHUNK_SIZE == 0) {
      // Every few notifications, yield time back to the main
      // thread to avoid jank.
      await Promise.resolve();
    }
  }
};

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
var notifyOnResult = async function(data, onResult) {
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
      await Promise.resolve();
    }
  }
};

// Inner implementation of History.fetch.
var fetch = async function(db, guidOrURL, options) {
  let whereClauseFragment = "";
  let params = {};
  if (guidOrURL instanceof URL) {
    whereClauseFragment = "WHERE h.url_hash = hash(:url) AND h.url = :url";
    params.url = guidOrURL.href;
  } else {
    whereClauseFragment = "WHERE h.guid = :guid";
    params.guid = guidOrURL;
  }

  let visitSelectionFragment = "";
  let joinFragment = "";
  let visitOrderFragment = "";
  if (options.includeVisits) {
    visitSelectionFragment = ", v.visit_date, v.visit_type";
    joinFragment = "JOIN moz_historyvisits v ON h.id = v.place_id";
    visitOrderFragment = "ORDER BY v.visit_date DESC";
  }

  let pageMetaSelectionFragment = "";
  if (options.includeMeta) {
    pageMetaSelectionFragment = ", description, preview_image_url";
  }

  let query = `SELECT h.id, guid, url, title, frecency
               ${pageMetaSelectionFragment} ${visitSelectionFragment}
               FROM moz_places h ${joinFragment}
               ${whereClauseFragment}
               ${visitOrderFragment}`;
  let pageInfo = null;
  await db.executeCached(
    query,
    params,
    row => {
      if (pageInfo === null) {
        // This means we're on the first row, so we need to get all the page info.
        pageInfo = {
          guid: row.getResultByName("guid"),
          url: new URL(row.getResultByName("url")),
          frecency: row.getResultByName("frecency"),
          title: row.getResultByName("title") || ""
        };
      }
      if (options.includeMeta) {
        pageInfo.description = row.getResultByName("description") || "";
        let previewImageURL = row.getResultByName("preview_image_url");
        pageInfo.previewImageURL = previewImageURL ? new URL(previewImageURL) : null;
      }
      if (options.includeVisits) {
        // On every row (not just the first), we need to collect visit data.
        if (!("visits" in pageInfo)) {
          pageInfo.visits = [];
        }
        let date = PlacesUtils.toDate(row.getResultByName("visit_date"));
        let transition = row.getResultByName("visit_type");

        // TODO: Bug #1365913 add referrer URL to the `VisitInfo` data as well.
        pageInfo.visits.push({ date, transition });
      }
    });
  return pageInfo;
};

// Inner implementation of History.removeVisitsByFilter.
var removeVisitsByFilter = async function(db, filter, onResult = null) {
  // 1. Determine visits that took place during the interval.  Note
  // that the database uses microseconds, while JS uses milliseconds,
  // so we need to *1000 one way and /1000 the other way.
  let conditions = [];
  let args = {};
  if ("beginDate" in filter) {
    conditions.push("v.visit_date >= :begin * 1000");
    args.begin = Number(filter.beginDate);
  }
  if ("endDate" in filter) {
    conditions.push("v.visit_date <= :end * 1000");
    args.end = Number(filter.endDate);
  }
  if ("limit" in filter) {
    args.limit = Number(filter.limit);
  }

  let optionalJoin = "";
  if ("url" in filter) {
    let url = filter.url;
    if (url instanceof Ci.nsIURI) {
      url = filter.url.spec;
    } else {
      url = new URL(url).href;
    }
    optionalJoin = `JOIN moz_places h ON h.id = v.place_id`;
    conditions.push("h.url_hash = hash(:url)", "h.url = :url");
    args.url = url;
  }


  let visitsToRemove = [];
  let pagesToInspect = new Set();
  let onResultData = onResult ? [] : null;

  await db.executeCached(
     `SELECT v.id, place_id, visit_date / 1000 AS date, visit_type FROM moz_historyvisits v
             ${optionalJoin}
             WHERE ${ conditions.join(" AND ") }${ args.limit ? " LIMIT :limit" : "" }`,
     args,
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
    await db.executeTransaction(async function() {
      // 2. Remove all offending visits.
      await db.execute(`DELETE FROM moz_historyvisits
                        WHERE id IN (${ sqlList(visitsToRemove) } )`);

      // 3. Find out which pages have been orphaned
      await db.execute(
        `SELECT id, url, url_hash, guid,
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
             hash: row.getResultByName("url_hash"),
           };
           pages.push(page);
         });

      // 4. Clean up and notify
      await cleanupPages(db, pages);
    });

    notifyCleanup(db, pages);
    notifyOnResult(onResultData, onResult); // don't wait
  } finally {
    // Ensure we cleanup embed visits, even if we bailed out early.
    PlacesUtils.history.clearEmbedVisits();
  }

  return visitsToRemove.length != 0;
};

// Inner implementation of History.removeByFilter
var removeByFilter = async function(db, filter, onResult = null) {
  // 1. Create fragment for date filtration
  let dateFilterSQLFragment = "";
  let conditions = [];
  let params = {};
  if ("beginDate" in filter) {
    conditions.push("v.visit_date >= :begin");
    params.begin = PlacesUtils.toPRTime(filter.beginDate);
  }
  if ("endDate" in filter) {
    conditions.push("v.visit_date <= :end");
    params.end = PlacesUtils.toPRTime(filter.endDate);
  }

  if (conditions.length !== 0) {
    dateFilterSQLFragment =
      `EXISTS
         (SELECT id FROM moz_historyvisits v WHERE v.place_id = h.id AND
         ${ conditions.join(" AND ") }
         LIMIT 1)`;
  }

  // 2. Create fragment for host and subhost filtering
  let hostFilterSQLFragment = "";
  if (filter.host || filter.host === "") {
    // There are four cases that we need to consider,
    // mozilla.org, *.mozilla.org, localhost, and local files

    if (filter.host.indexOf("*") === 0) {
      // Case 1: subhost wildcard is specified (*.mozilla.org)
      let revHost = filter.host.slice(2).split("").reverse().join("");
      hostFilterSQLFragment =
        `h.rev_host between :revHostStart and :revHostEnd`;
      params.revHostStart = revHost + ".";
      params.revHostEnd = revHost + "/";
    } else {
      // This covers the rest (mozilla.org, localhost and local files)
      let revHost = filter.host.split("").reverse().join("") + ".";
      hostFilterSQLFragment =
        `h.rev_host = :hostName`;
      params.hostName = revHost;
    }
  }

  // 3. Find out what needs to be removed
  let fragmentArray = [hostFilterSQLFragment, dateFilterSQLFragment];
  let query =
      `SELECT h.id, url, url_hash, rev_host, guid, title, frecency, foreign_count
       FROM moz_places h WHERE
       (${ fragmentArray.filter(f => f !== "").join(") AND (") })`;
  let onResultData = onResult ? [] : null;
  let pages = [];
  let hasPagesToRemove = false;

  await db.executeCached(
    query,
    params,
    row => {
      let hasForeign = row.getResultByName("foreign_count") != 0;
      if (!hasForeign) {
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
        hash: row.getResultByName("url_hash"),
      };
      pages.push(page);
      if (onResult) {
        onResultData.push({
          guid,
          title: row.getResultByName("title"),
          frecency: row.getResultByName("frecency"),
          url: new URL(url)
        });
      }
    });

  if (pages.length === 0) {
    // Nothing to do
    return false;
  }

  try {
    await db.executeTransaction(async function() {
      // 4. Actually remove visits
      await db.execute(`DELETE FROM moz_historyvisits
                        WHERE place_id IN(${ sqlList(pages.map(p => p.id)) })`);
      // 5. Clean up and notify
      await cleanupPages(db, pages);
    });

    notifyCleanup(db, pages);
    notifyOnResult(onResultData, onResult);
  } finally {
    PlacesUtils.history.clearEmbedVisits();
  }

  return hasPagesToRemove;
};

// Inner implementation of History.remove.
var remove = async function(db, {guids, urls}, onResult = null) {
  // 1. Find out what needs to be removed
  let query =
    `SELECT id, url, url_hash, guid, foreign_count, title, frecency
     FROM moz_places
     WHERE guid IN (${ sqlList(guids) })
        OR (url_hash IN (${ urls.map(u => "hash(" + JSON.stringify(u) + ")").join(",") })
            AND url IN (${ sqlList(urls) }))
    `;

  let onResultData = onResult ? [] : null;
  let pages = [];
  let hasPagesToRemove = false;
  await db.execute(query, null, function(row) {
    let hasForeign = row.getResultByName("foreign_count") != 0;
    if (!hasForeign) {
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
      hash: row.getResultByName("url_hash"),
    };
    pages.push(page);
    if (onResult) {
      onResultData.push({
        guid,
        title: row.getResultByName("title"),
        frecency: row.getResultByName("frecency"),
        url: new URL(url)
      });
    }
  });

  try {
    if (pages.length == 0) {
      // Nothing to do
      return false;
    }

    await db.executeTransaction(async function() {
      // 2. Remove all visits to these pages.
      await db.execute(`DELETE FROM moz_historyvisits
                        WHERE place_id IN (${ sqlList(pages.map(p => p.id)) })
                       `);

      // 3. Clean up and notify
      await cleanupPages(db, pages);
    });

    notifyCleanup(db, pages);
    notifyOnResult(onResultData, onResult); // don't wait
  } finally {
    // Ensure we cleanup embed visits, even if we bailed out early.
    PlacesUtils.history.clearEmbedVisits();
  }

  return hasPagesToRemove;
};

/**
 * Merges an updateInfo object, as returned by asyncHistory.updatePlaces
 * into a PageInfo object as defined in this file.
 *
 * @param updateInfo: (Object)
 *      An object that represents a page that is generated by
 *      asyncHistory.updatePlaces.
 * @param pageInfo: (PageInfo)
 *      An PageInfo object into which to merge the data from updateInfo.
 *      Defaults to an empty object so that this method can be used
 *      to simply convert an updateInfo object into a PageInfo object.
 *
 * @return (PageInfo)
 *      A PageInfo object populated with data from updateInfo.
 */
function mergeUpdateInfoIntoPageInfo(updateInfo, pageInfo = {}) {
  pageInfo.guid = updateInfo.guid;
  pageInfo.title = updateInfo.title;
  if (!pageInfo.url) {
    pageInfo.url = new URL(updateInfo.uri.spec);
    pageInfo.title = updateInfo.title;
    pageInfo.visits = updateInfo.visits.map(visit => {
      return {
        date: PlacesUtils.toDate(visit.visitDate),
        transition: visit.transitionType,
        referrer: (visit.referrerURI) ? new URL(visit.referrerURI.spec) : null
      };
    });
  }
  return pageInfo;
}

// Inner implementation of History.insert.
var insert = function(db, pageInfo) {
  let info = convertForUpdatePlaces(pageInfo);

  return new Promise((resolve, reject) => {
    PlacesUtils.asyncHistory.updatePlaces(info, {
      handleError: error => {
        reject(error);
      },
      handleResult: result => {
        pageInfo = mergeUpdateInfoIntoPageInfo(result, pageInfo);
      },
      handleCompletion: () => {
        resolve(pageInfo);
      }
    });
  });
};

// Inner implementation of History.insertMany.
var insertMany = function(db, pageInfos, onResult, onError) {
  let infos = [];
  let onResultData = [];
  let onErrorData = [];

  for (let pageInfo of pageInfos) {
    let info = convertForUpdatePlaces(pageInfo);
    infos.push(info);
  }

  return new Promise((resolve, reject) => {
    PlacesUtils.asyncHistory.updatePlaces(infos, {
      handleError: (resultCode, result) => {
        let pageInfo = mergeUpdateInfoIntoPageInfo(result);
        onErrorData.push(pageInfo);
      },
      handleResult: result => {
        let pageInfo = mergeUpdateInfoIntoPageInfo(result);
        onResultData.push(pageInfo);
      },
      ignoreErrors: !onError,
      ignoreResults: !onResult,
      handleCompletion: (updatedCount) => {
        notifyOnResult(onResultData, onResult);
        notifyOnResult(onErrorData, onError);
        if (updatedCount > 0) {
          resolve();
        } else {
          reject({message: "No items were added to history."});
        }
      }
    }, true);
  });
};

// Inner implementation of History.update.
var update = async function(db, pageInfo) {
  let updateFragments = [];
  let whereClauseFragment = "";
  let params = {};

  // Prefer GUID over url if it's present
  if (typeof pageInfo.guid === "string") {
    whereClauseFragment = "guid = :guid";
    params.guid = pageInfo.guid;
  } else {
    whereClauseFragment = "url_hash = hash(:url) AND url = :url";
    params.url = pageInfo.url.href;
  }

  if (pageInfo.description || pageInfo.description === null) {
    updateFragments.push("description");
    params.description = pageInfo.description;
  }
  if (pageInfo.previewImageURL || pageInfo.previewImageURL === null) {
    updateFragments.push("preview_image_url");
    params.preview_image_url = pageInfo.previewImageURL ? pageInfo.previewImageURL.href : null;
  }
  // Since this data may be written at every visit and is textual, avoid
  // overwriting the existing record if it didn't change.
  await db.execute(`
    UPDATE moz_places
    SET ${updateFragments.map(v => `${v} = :${v}`).join(", ")}
    WHERE ${whereClauseFragment}
      AND (${updateFragments.map(v => `IFNULL(${v}, "") <> IFNULL(:${v}, "")`).join(" OR ")})
  `, params);
};
