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
 *  - annotations: (Map)
 *     A map containing key/value pairs of the annotations for this page, if any.
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
 * Each successful operation notifies through the PlacesObservers. To listen to such
 * notifications you must register using
 * PlacesObservers `addListener` and `removeListener` methods.
 * @see PlacesObservers
 */

var EXPORTED_SYMBOLS = ["History"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

XPCOMUtils.defineLazyServiceGetter(
  this,
  "asyncHistory",
  "@mozilla.org/browser/history;1",
  "mozIAsyncHistory"
);

XPCOMUtils.defineLazyGlobalGetters(this, ["URL"]);

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
    } catch (ex) {
      if (
        ex.result != Cr.NS_ERROR_XPC_JSOBJECT_HAS_NO_FUNCTION_NAMED &&
        (AppConstants.DEBUG || Cu.isInAutomation)
      ) {
        Cu.reportError(ex);
      }
    }
  }
}

var History = Object.freeze({
  ANNOTATION_EXPIRE_NEVER: 4,
  // Constants for the type of annotation.
  ANNOTATION_TYPE_STRING: 3,
  ANNOTATION_TYPE_INT64: 5,

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
   *        - `includeAnnotations` (boolean) set this to true to fetch any
   *           annotations that are associated with the page.
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

    let hasIncludeAnnotations = "includeAnnotations" in options;
    if (
      hasIncludeAnnotations &&
      typeof options.includeAnnotations !== "boolean"
    ) {
      throw new TypeError("includeAnnotations should be a boolean if exists");
    }

    return PlacesUtils.promiseDBConnection().then(db =>
      fetch(db, guidOrURI, options)
    );
  },

  /**
   * Fetches all pages which have one or more of the specified annotations.
   *
   * @param annotations: An array of strings containing the annotation names to
   *                     find.
   * @return (Promise)
   *      A promise resolved once the operation is complete.
   * @resolves (Map)
   *      A Map containing the annotations, pages and their contents, e.g.
   *      Map("anno1" => [{page, content}, {page, content}]), "anno2" => ....);
   * @rejects (Error) XXX
   *      Rejects if the insert was unsuccessful.
   */
  fetchAnnotatedPages(annotations) {
    // See if options exists and make sense
    if (!annotations || !Array.isArray(annotations)) {
      throw new TypeError("annotations should be an Array and not null");
    }
    if (annotations.some(name => typeof name !== "string")) {
      throw new TypeError("all annotation values should be strings");
    }

    return PlacesUtils.promiseDBConnection().then(db =>
      fetchAnnotatedPages(db, annotations)
    );
  },

  /**
   * Adds a number of visits for a single page.
   *
   * Any change may be observed through PlacesObservers.
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

    return PlacesUtils.withConnectionWrapper("History.jsm: insert", db =>
      insert(db, info)
    );
  },

  /**
   * Adds a number of visits for a number of pages.
   *
   * Any change may be observed through PlacesObservers.
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

    return PlacesUtils.withConnectionWrapper("History.jsm: insertMany", db =>
      insertMany(db, infos, onResult, onError)
    );
  },

  /**
   * Remove pages from the database.
   *
   * Any change may be observed through PlacesObservers.
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
      if (!pages.length) {
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

        let pages = { guids: guidsSlice, urls: urlsSlice };

        let result = await PlacesUtils.withConnectionWrapper(
          "History.jsm: remove",
          db => remove(db, pages, onResult)
        );

        removedPages = removedPages || result;
      }
      return removedPages;
    })();
  },

  /**
   * Remove visits matching specific characteristics.
   *
   * Any change may be observed through PlacesObservers.
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
   *          - transition: (Integer)
   *                The type of the transition (see TRANSITIONS below)
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
    let hasTransition = "transition" in filter;
    if (hasBeginDate) {
      this.ensureDate(filter.beginDate);
    }
    if (hasEndDate) {
      this.ensureDate(filter.endDate);
    }
    if (hasBeginDate && hasEndDate && filter.beginDate > filter.endDate) {
      throw new TypeError("`beginDate` should be at least as old as `endDate`");
    }
    if (hasTransition && !this.isValidTransition(filter.transition)) {
      throw new TypeError("`transition` should be valid");
    }
    if (
      !hasBeginDate &&
      !hasEndDate &&
      !hasURL &&
      !hasLimit &&
      !hasTransition
    ) {
      throw new TypeError("Expected a non-empty filter");
    }

    if (
      hasURL &&
      !(filter.url instanceof URL) &&
      typeof filter.url != "string" &&
      !(filter.url instanceof Ci.nsIURI)
    ) {
      throw new TypeError("Expected a valid URL for `url`");
    }

    if (
      hasLimit &&
      (typeof filter.limit != "number" ||
        filter.limit <= 0 ||
        !Number.isInteger(filter.limit))
    ) {
      throw new TypeError("Expected a non-zero positive integer as a limit");
    }

    if (onResult && typeof onResult != "function") {
      throw new TypeError("Invalid function: " + onResult);
    }

    return PlacesUtils.withConnectionWrapper(
      "History.jsm: removeVisitsByFilter",
      db => removeVisitsByFilter(db, filter, onResult)
    );
  },

  /**
   * Remove pages from the database based on a filter.
   *
   * Any change may be observed through PlacesObservers
   *
   *
   * @param filter: An object containing a non empty subset of the following
   * properties:
   * - host: (string)
   *     Hostname with or without subhost. Examples:
   *       "mozilla.org" removes pages from mozilla.org but not its subdomains
   *       ".mozilla.org" removes pages from mozilla.org and its subdomains
   *       "." removes local files
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

    let hasHost = filter.host;
    if (hasHost) {
      if (typeof filter.host !== "string") {
        throw new TypeError("`host` should be a string");
      }
      filter.host = filter.host.toLowerCase();
      if (filter.host.length > 1 && filter.host.lastIndexOf(".") == 0) {
        // The input contains only an initial period, thus it may be a
        // wildcarded local host, like ".localhost". Ideally the consumer should
        // pass just "localhost", because there is no concept of subhosts for
        // it, but we are being more lenient to allow for simpler input.
        // Anyway, in this case we remove the wildcard to avoid clearing too
        // much if the consumer wrongly passes in things like ".com".
        filter.host = filter.host.slice(1);
      }
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

    // Check the host format.
    // Either it has no dots, or has multiple dots, or it's a single dot char.
    if (
      hasHost &&
      (!/^(\.?([.a-z0-9-]+\.[a-z0-9-]+)?|[a-z0-9-]+)$/.test(filter.host) ||
        filter.host.includes(".."))
    ) {
      throw new TypeError(
        "Expected well formed hostname string for `host` with atmost 1 wildcard."
      );
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
        asyncHistory.isURIVisited(guidOrURI, (aURI, aIsVisited) => {
          resolve(aIsVisited);
        });
      });
    }

    guidOrURI = PlacesUtils.normalizeToURLOrGUID(guidOrURI);
    let isGuid = typeof guidOrURI == "string";
    let sqlFragment = isGuid
      ? "guid = :val"
      : "url_hash = hash(:val) AND url = :val ";

    return PlacesUtils.promiseDBConnection().then(async db => {
      let rows = await db.executeCached(
        `SELECT 1 FROM moz_places
                                         WHERE ${sqlFragment}
                                         AND last_visit_date NOTNULL`,
        { val: isGuid ? guidOrURI : guidOrURI.href }
      );
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
    return PlacesUtils.withConnectionWrapper("History.jsm: clear", clear);
  },

  /**
   * Is a value a valid transition type?
   *
   * @param transition: (String)
   * @return (Boolean)
   */
  isValidTransition(transition) {
    return Object.values(History.TRANSITIONS).includes(transition);
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
   * Currently, it supports updating the description, preview image URL and annotations
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
   *      If a property `annotations` is provided, the annotations will be
   *      updated. Note that:
   *      1). It should be a Map containing key/value pairs to be updated.
   *      2). If the value is falsy, the annotation will be removed.
   *      3). If the value is non-falsy, the annotation will be added or updated.
   *      For `annotations` the keys must all be strings, the values should be
   *      Boolean, Number or Strings. null and undefined are supported as falsy values.
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

    if (
      info.description === undefined &&
      info.previewImageURL === undefined &&
      info.annotations === undefined
    ) {
      throw new TypeError(
        "pageInfo object must at least have either a description, previewImageURL or annotations property."
      );
    }

    return PlacesUtils.withConnectionWrapper("History.jsm: update", db =>
      update(db, info)
    );
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
      referrerURI: inVisit.referrer
        ? PlacesUtils.toURI(inVisit.referrer)
        : undefined,
    };
    info.visits.push(visit);
  }
  return info;
}

/**
 * Generates a list of "?" SQL bindings based on input array length.
 * @param {array} values an array of values.
 * @param {string} [prefix] a string to prefix to the placeholder.
 * @param {string} [suffix] a string to suffix to the placeholder.
 * @returns {string} placeholders is a string made of question marks and commas,
 *          one per value.
 */
function sqlBindPlaceholders(values, prefix = "", suffix = "") {
  return new Array(values.length).fill(prefix + "?" + suffix).join(",");
}

/**
 * Invalidate and recompute the frecency of a list of pages,
 * informing frecency observers.
 *
 * @param {OpenConnection} db an Sqlite connection
 * @param {Array} idList The `moz_places` identifiers to invalidate.
 * @returns {Promise} resolved when done
 */
var invalidateFrecencies = async function(db, idList) {
  if (!idList.length) {
    return;
  }
  for (let chunk of PlacesUtils.chunkArray(idList, db.variableLimit)) {
    await db.execute(
      `UPDATE moz_places
       SET frecency = CALCULATE_FRECENCY(id)
       WHERE id in (${sqlBindPlaceholders(chunk)})`,
      chunk
    );
    await db.execute(
      `UPDATE moz_places
       SET hidden = 0
       WHERE id in (${sqlBindPlaceholders(chunk)})
       AND frecency <> 0`,
      chunk
    );
  }

  PlacesObservers.notifyListeners([new PlacesRanking()]);

  // Trigger frecency updates for all affected origins.
  await db.execute(`DELETE FROM moz_updateoriginsupdate_temp`);
};

// Inner implementation of History.clear().
var clear = async function(db) {
  await db.executeTransaction(async function() {
    // Remove all non-bookmarked places entries first, this will speed up the
    // triggers work.
    await db.execute(`DELETE FROM moz_places WHERE foreign_count = 0`);
    await db.execute(`DELETE FROM moz_updateoriginsdelete_temp`);

    // Expire orphan icons.
    await db.executeCached(`DELETE FROM moz_pages_w_icons
                            WHERE page_url_hash NOT IN (SELECT url_hash FROM moz_places)`);
    await removeOrphanIcons(db);

    // Expire annotations.
    await db.execute(`DELETE FROM moz_annos WHERE NOT EXISTS (
                        SELECT 1 FROM moz_places WHERE id = place_id
                      )`);

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

  PlacesObservers.notifyListeners([
    new PlacesHistoryCleared(),
    new PlacesRanking(),
  ]);

  // Trigger frecency updates for all affected origins.
  await db.execute(`DELETE FROM moz_updateoriginsupdate_temp`);
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
  await invalidateFrecencies(
    db,
    pages.filter(p => p.hasForeign || p.hasVisits).map(p => p.id)
  );

  let pagesToRemove = pages.filter(p => !p.hasForeign && !p.hasVisits);
  if (!pagesToRemove.length) {
    return;
  }

  // Note, we are already in a transaction, since callers create it.
  // Check relations regardless, to avoid creating orphans in case of
  // async race conditions.
  for (let chunk of PlacesUtils.chunkArray(pagesToRemove, db.variableLimit)) {
    let idsToRemove = chunk.map(p => p.id);
    await db.execute(
      `DELETE FROM moz_places
       WHERE id IN ( ${sqlBindPlaceholders(idsToRemove)} )
         AND foreign_count = 0 AND last_visit_date ISNULL`,
      idsToRemove
    );

    // Expire orphans.
    let hashesToRemove = chunk.map(p => p.hash);
    await db.executeCached(
      `DELETE FROM moz_pages_w_icons
       WHERE page_url_hash IN (${sqlBindPlaceholders(hashesToRemove)})`,
      hashesToRemove
    );

    await db.execute(
      `DELETE FROM moz_annos
       WHERE place_id IN ( ${sqlBindPlaceholders(idsToRemove)} )`,
      idsToRemove
    );
    await db.execute(
      `DELETE FROM moz_inputhistory
       WHERE place_id IN ( ${sqlBindPlaceholders(idsToRemove)} )`,
      idsToRemove
    );
  }
  // Hosts accumulated during the places delete are updated through a trigger
  // (see nsPlacesTriggers.h).
  await db.executeCached(`DELETE FROM moz_updateoriginsdelete_temp`);

  await removeOrphanIcons(db);
};

/**
 * Remove icons whose origin is not in moz_origins, unless referenced.
 * @param db: (Sqlite connection)
 *      The database.
 */
function removeOrphanIcons(db) {
  return db.executeCached(`
    DELETE FROM moz_icons WHERE id IN (
      SELECT id FROM moz_icons WHERE root = 0
      UNION ALL
      SELECT id FROM moz_icons
      WHERE root = 1
        AND get_host_and_port(icon_url) NOT IN (SELECT host FROM moz_origins)
        AND fixup_url(get_host_and_port(icon_url)) NOT IN (SELECT host FROM moz_origins)
      EXCEPT
      SELECT icon_id FROM moz_icons_to_pages
    )`);
}

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
 * @param transitionType: (Number)
 *      Set to a valid TRANSITIONS value to indicate all transitions of a
 *      certain type have been removed, otherwise defaults to 0 (unknown value).
 * @return (Promise)
 */
var notifyCleanup = async function(db, pages, transitionType = 0) {
  const notifications = [];
  let notifiedCount = 0;
  let bookmarkObservers = PlacesUtils.bookmarks.getObservers();

  for (let page of pages) {
    const isRemovedFromStore = !page.hasVisits && !page.hasForeign;
    notifications.push(
      new PlacesVisitRemoved({
        url: Services.io.newURI(page.url.href).spec,
        pageGuid: page.guid,
        reason: PlacesVisitRemoved.REASON_DELETED,
        transitionType,
        isRemovedFromStore,
        isPartialVisistsRemoval: !isRemovedFromStore && page.hasVisits > 0,
      })
    );

    if (page.hasForeign && !page.hasVisits) {
      PlacesUtils.bookmarks
        .fetch({ url: page.url }, async bookmark => {
          let itemId = await PlacesUtils.promiseItemId(bookmark.guid);
          let parentId = await PlacesUtils.promiseItemId(bookmark.parentGuid);
          notify(
            bookmarkObservers,
            "onItemChanged",
            [
              itemId,
              "cleartime",
              false,
              "",
              0,
              PlacesUtils.bookmarks.TYPE_BOOKMARK,
              parentId,
              bookmark.guid,
              bookmark.parentGuid,
              "",
              PlacesUtils.bookmarks.SOURCES.DEFAULT,
            ],
            { concurrent: true }
          );

          if (++notifiedCount % NOTIFICATION_CHUNK_SIZE == 0) {
            // Every few notifications, yield time back to the main
            // thread to avoid jank.
            await Promise.resolve();
          }
        })
        .catch(Cu.reportError);
    }
  }

  PlacesObservers.notifyListeners(notifications);
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
  let placeId = null;
  await db.executeCached(query, params, row => {
    if (pageInfo === null) {
      // This means we're on the first row, so we need to get all the page info.
      pageInfo = {
        guid: row.getResultByName("guid"),
        url: new URL(row.getResultByName("url")),
        frecency: row.getResultByName("frecency"),
        title: row.getResultByName("title") || "",
      };
      placeId = row.getResultByName("id");
    }
    if (options.includeMeta) {
      pageInfo.description = row.getResultByName("description") || "";
      let previewImageURL = row.getResultByName("preview_image_url");
      pageInfo.previewImageURL = previewImageURL
        ? new URL(previewImageURL)
        : null;
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

  // Only try to get annotations if requested, and if there's an actual page found.
  if (pageInfo && options.includeAnnotations) {
    let rows = await db.executeCached(
      `
      SELECT n.name, a.content FROM moz_anno_attributes n
      JOIN moz_annos a ON n.id = a.anno_attribute_id
      WHERE a.place_id = :placeId
    `,
      { placeId }
    );

    pageInfo.annotations = new Map(
      rows.map(row => [
        row.getResultByName("name"),
        row.getResultByName("content"),
      ])
    );
  }
  return pageInfo;
};

// Inner implementation of History.fetchAnnotatedPages.
var fetchAnnotatedPages = async function(db, annotations) {
  let result = new Map();
  let rows = await db.execute(
    `
    SELECT n.name, h.url, a.content FROM moz_anno_attributes n
    JOIN moz_annos a ON n.id = a.anno_attribute_id
    JOIN moz_places h ON h.id = a.place_id
    WHERE n.name IN (${new Array(annotations.length).fill("?").join(",")})
  `,
    annotations
  );

  for (let row of rows) {
    let uri;
    try {
      uri = new URL(row.getResultByName("url"));
    } catch (ex) {
      Cu.reportError("Invalid URL read from database in fetchAnnotatedPages");
      continue;
    }

    let anno = {
      uri,
      content: row.getResultByName("content"),
    };
    let annoName = row.getResultByName("name");
    let pageAnnos = result.get(annoName);
    if (!pageAnnos) {
      pageAnnos = [];
      result.set(annoName, pageAnnos);
    }
    pageAnnos.push(anno);
  }

  return result;
};

// Inner implementation of History.removeVisitsByFilter.
var removeVisitsByFilter = async function(db, filter, onResult = null) {
  // 1. Determine visits that took place during the interval.  Note
  // that the database uses microseconds, while JS uses milliseconds,
  // so we need to *1000 one way and /1000 the other way.
  let conditions = [];
  let args = {};
  let transition = -1;
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
  if ("transition" in filter) {
    conditions.push("v.visit_type = :transition");
    args.transition = filter.transition;
    transition = filter.transition;
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
             WHERE ${conditions.join(" AND ")}${
      args.limit ? " LIMIT :limit" : ""
    }`,
    args,
    row => {
      let id = row.getResultByName("id");
      let place_id = row.getResultByName("place_id");
      visitsToRemove.push(id);
      pagesToInspect.add(place_id);

      if (onResult) {
        onResultData.push({
          date: new Date(row.getResultByName("date")),
          transition: row.getResultByName("visit_type"),
        });
      }
    }
  );

  if (!visitsToRemove.length) {
    // Nothing to do
    return false;
  }

  let pages = [];
  await db.executeTransaction(async function() {
    // 2. Remove all offending visits.
    for (let chunk of PlacesUtils.chunkArray(
      visitsToRemove,
      db.variableLimit
    )) {
      await db.execute(
        `DELETE FROM moz_historyvisits
         WHERE id IN (${sqlBindPlaceholders(chunk)})`,
        chunk
      );
    }

    // 3. Find out which pages have been orphaned
    for (let chunk of PlacesUtils.chunkArray(
      [...pagesToInspect],
      db.variableLimit
    )) {
      await db.execute(
        `SELECT id, url, url_hash, guid,
          (foreign_count != 0) AS has_foreign,
          (last_visit_date NOTNULL) as has_visits
         FROM moz_places
         WHERE id IN (${sqlBindPlaceholders(chunk)})`,
        chunk,
        row => {
          let page = {
            id: row.getResultByName("id"),
            guid: row.getResultByName("guid"),
            hasForeign: row.getResultByName("has_foreign"),
            hasVisits: row.getResultByName("has_visits"),
            url: new URL(row.getResultByName("url")),
            hash: row.getResultByName("url_hash"),
          };
          pages.push(page);
        }
      );
    }

    // 4. Clean up and notify
    await cleanupPages(db, pages);
  });

  notifyCleanup(db, pages, transition);
  notifyOnResult(onResultData, onResult); // don't wait

  return !!visitsToRemove.length;
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
    dateFilterSQLFragment = `EXISTS
         (SELECT id FROM moz_historyvisits v WHERE v.place_id = h.id AND
         ${conditions.join(" AND ")}
         LIMIT 1)`;
  }

  // 2. Create fragment for host and subhost filtering
  let hostFilterSQLFragment = "";
  if (filter.host) {
    // There are four cases that we need to consider:
    // mozilla.org, .mozilla.org, localhost, and local files
    let revHost = filter.host
      .split("")
      .reverse()
      .join("");
    if (filter.host == ".") {
      // Local files.
      hostFilterSQLFragment = `h.rev_host = :revHost`;
    } else if (filter.host.startsWith(".")) {
      // Remove the subhost wildcard.
      revHost = revHost.slice(0, -1);
      hostFilterSQLFragment = `h.rev_host between :revHost || "." and :revHost || "/"`;
    } else {
      // This covers non-wildcarded hosts (e.g.: mozilla.org, localhost)
      hostFilterSQLFragment = `h.rev_host = :revHost || "."`;
    }
    params.revHost = revHost;
  }

  // 3. Find out what needs to be removed
  let fragmentArray = [hostFilterSQLFragment, dateFilterSQLFragment];
  let query = `SELECT h.id, url, url_hash, rev_host, guid, title, frecency, foreign_count
       FROM moz_places h WHERE
       (${fragmentArray.filter(f => f !== "").join(") AND (")})`;
  let onResultData = onResult ? [] : null;
  let pages = [];
  let hasPagesToRemove = false;

  await db.executeCached(query, params, row => {
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
        url: new URL(url),
      });
    }
  });

  if (pages.length === 0) {
    // Nothing to do
    return false;
  }

  await db.executeTransaction(async function() {
    // 4. Actually remove visits
    let pageIds = pages.map(p => p.id);
    for (let chunk of PlacesUtils.chunkArray(pageIds, db.variableLimit)) {
      await db.execute(
        `DELETE FROM moz_historyvisits
         WHERE place_id IN(${sqlBindPlaceholders(chunk)})`,
        chunk
      );
    }
    // 5. Clean up and notify
    await cleanupPages(db, pages);
  });

  notifyCleanup(db, pages);
  notifyOnResult(onResultData, onResult);

  return hasPagesToRemove;
};

// Inner implementation of History.remove.
var remove = async function(db, { guids, urls }, onResult = null) {
  // 1. Find out what needs to be removed
  let onResultData = onResult ? [] : null;
  let pages = [];
  let hasPagesToRemove = false;
  function onRow(row) {
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
        url: new URL(url),
      });
    }
  }
  for (let chunk of PlacesUtils.chunkArray(guids, db.variableLimit)) {
    let query = `SELECT id, url, url_hash, guid, foreign_count, title, frecency
       FROM moz_places
       WHERE guid IN (${sqlBindPlaceholders(guids)})
      `;
    await db.execute(query, chunk, onRow);
  }
  for (let chunk of PlacesUtils.chunkArray(urls, db.variableLimit)) {
    // Make an array of variables like `["?1", "?2", ...]`, up to the length of
    // the chunk. This lets us bind each URL once, reusing the binding for the
    // `url_hash IN (...)` and `url IN (...)` clauses. We add 1 because indexed
    // parameters start at 1, not 0.
    let variables = Array.from(
      { length: chunk.length },
      (_, i) => "?" + (i + 1)
    );
    let query = `SELECT id, url, url_hash, guid, foreign_count, title, frecency
       FROM moz_places
       WHERE url_hash IN (${variables.map(v => `hash(${v})`).join(",")}) AND
             url IN (${variables.join(",")})
      `;
    await db.execute(query, chunk, onRow);
  }

  if (!pages.length) {
    // Nothing to do
    return false;
  }

  await db.executeTransaction(async function() {
    // 2. Remove all visits to these pages.
    let pageIds = pages.map(p => p.id);
    for (let chunk of PlacesUtils.chunkArray(pageIds, db.variableLimit)) {
      await db.execute(
        `DELETE FROM moz_historyvisits
         WHERE place_id IN (${sqlBindPlaceholders(chunk)})`,
        chunk
      );
    }

    // 3. Clean up and notify
    await cleanupPages(db, pages);
  });

  notifyCleanup(db, pages);
  notifyOnResult(onResultData, onResult); // don't wait

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
        referrer: visit.referrerURI ? new URL(visit.referrerURI.spec) : null,
      };
    });
  }
  return pageInfo;
}

// Inner implementation of History.insert.
var insert = function(db, pageInfo) {
  let info = convertForUpdatePlaces(pageInfo);

  return new Promise((resolve, reject) => {
    asyncHistory.updatePlaces(info, {
      handleError: error => {
        reject(error);
      },
      handleResult: result => {
        pageInfo = mergeUpdateInfoIntoPageInfo(result, pageInfo);
      },
      handleCompletion: () => {
        resolve(pageInfo);
      },
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
    asyncHistory.updatePlaces(infos, {
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
      handleCompletion: updatedCount => {
        notifyOnResult(onResultData, onResult);
        notifyOnResult(onErrorData, onError);
        if (updatedCount > 0) {
          resolve();
        } else {
          reject({ message: "No items were added to history." });
        }
      },
    });
  });
};

// Inner implementation of History.update.
var update = async function(db, pageInfo) {
  // Check for page existence first; we can skip most of the work if it doesn't
  // exist and anyway we'll need the place id multiple times later.
  // Prefer GUID over url if it's present.
  let id;
  if (typeof pageInfo.guid === "string") {
    let rows = await db.executeCached(
      "SELECT id FROM moz_places WHERE guid = :guid",
      { guid: pageInfo.guid }
    );
    id = rows.length ? rows[0].getResultByName("id") : null;
  } else {
    let rows = await db.executeCached(
      "SELECT id FROM moz_places WHERE url_hash = hash(:url) AND url = :url",
      { url: pageInfo.url.href }
    );
    id = rows.length ? rows[0].getResultByName("id") : null;
  }
  if (!id) {
    return;
  }

  let updateFragments = [];
  let params = {};
  if ("description" in pageInfo) {
    updateFragments.push("description");
    params.description = pageInfo.description;
  }
  if ("previewImageURL" in pageInfo) {
    updateFragments.push("preview_image_url");
    params.preview_image_url = pageInfo.previewImageURL
      ? pageInfo.previewImageURL.href
      : null;
  }
  if (updateFragments.length) {
    // Since this data may be written at every visit and is textual, avoid
    // overwriting the existing record if it didn't change.
    await db.execute(
      `
      UPDATE moz_places
      SET ${updateFragments.map(v => `${v} = :${v}`).join(", ")}
      WHERE id = :id
        AND (${updateFragments
          .map(v => `IFNULL(${v}, '') <> IFNULL(:${v}, '')`)
          .join(" OR ")})
    `,
      { id, ...params }
    );
  }

  if (pageInfo.annotations) {
    let annosToRemove = [];
    let annosToUpdate = [];

    for (let anno of pageInfo.annotations) {
      anno[1] ? annosToUpdate.push(anno[0]) : annosToRemove.push(anno[0]);
    }

    await db.executeTransaction(async function() {
      if (annosToUpdate.length) {
        await db.execute(
          `
          INSERT OR IGNORE INTO moz_anno_attributes (name)
          VALUES ${Array.from(annosToUpdate.keys())
            .map(k => `(:${k})`)
            .join(", ")}
        `,
          Object.assign({}, annosToUpdate)
        );

        for (let anno of annosToUpdate) {
          let content = pageInfo.annotations.get(anno);
          // TODO: We only really need to save the type whilst we still support
          // accessing page annotations via the annotation service.
          let type =
            typeof content == "string"
              ? History.ANNOTATION_TYPE_STRING
              : History.ANNOTATION_TYPE_INT64;
          let date = PlacesUtils.toPRTime(new Date());

          // This will replace the id every time an annotation is updated. This is
          // not currently an issue as we're not joining on the id field.
          await db.execute(
            `
            INSERT OR REPLACE INTO moz_annos
              (place_id, anno_attribute_id, content, flags,
               expiration, type, dateAdded, lastModified)
            VALUES (:id,
                    (SELECT id FROM moz_anno_attributes WHERE name = :anno_name),
                    :content, 0, :expiration, :type, :date_added,
                    :last_modified)
          `,
            {
              id,
              anno_name: anno,
              content,
              expiration: History.ANNOTATION_EXPIRE_NEVER,
              type,
              // The date fields are unused, so we just set them both to the latest.
              date_added: date,
              last_modified: date,
            }
          );
        }
      }

      for (let anno of annosToRemove) {
        // We don't remove anything from the moz_anno_attributes table. If we
        // delete the last item of a given name, that item really should go away.
        // It will be cleaned up by expiration.
        await db.execute(
          `
          DELETE FROM moz_annos
          WHERE place_id = :id
          AND anno_attribute_id =
            (SELECT id FROM moz_anno_attributes WHERE name = :anno_name)
        `,
          { id, anno_name: anno }
        );
      }
    });
  }
};
