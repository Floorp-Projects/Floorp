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
 * - uri: (URL)
 *     or (nsIURI)
 *     or (string)
 *     The full URI of the page. Note that `PageInfo` values passed as
 *     argument may hold `nsIURI` or `string` values for property `uri`,
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
   *        - either a property `guid` or a property `uri`, as specified
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
   *      If the `uri` specified was for a protocol that should not be
   *      stored (e.g. "chrome:", "mailbox:", "about:", "imap:", "news:",
   *      "moz-anno:", "view-source:", "resource:", "data:", "wyciwyg:",
   *      "javascript:", "blob:").
   * @throws (Error)
   *      If `infos` has an unexpected type.
   * @throws (Error)
   *      If a `PageInfo` has neither `guid` nor `uri`,
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
   * @throws (Error)
   *       If `pages` has an unexpected type or if a string provided
   *       is neither a valid GUID nor a valid URI.
   */
  remove: function (pages, onResult) {
    throw new Error("Method not implemented");
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

