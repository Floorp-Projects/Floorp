/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  DeferredTask: "resource://gre/modules/DeferredTask.sys.mjs",
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
});

const OBSERVER_DEBOUNCE_RATE_MS = 500;
const OBSERVER_DEBOUNCE_TIMEOUT_MS = 5000;

/**
 * A collection of bookmarks that internally stays up-to-date in order to
 * efficiently query whether certain URLs are bookmarked.
 */
export class BookmarkList {
  /**
   * The set of hashed URLs that need to be fetched from the database.
   *
   * @type {Set<string>}
   */
  #urlsToFetch = new Set();

  /**
   * The function to call when changes are made.
   *
   * @type {function}
   */
  #observer;

  /**
   * Cached mapping of hashed URLs to how many bookmarks they are used in.
   *
   * @type {Map<string, number>}
   */
  #bookmarkCount = new Map();

  /**
   * Cached mapping of bookmark GUIDs to their respective URL hashes.
   *
   * @type {Map<string, string>}
   */
  #guidToUrl = new Map();

  /**
   * @type {DeferredTask}
   */
  #observerTask;

  /**
   * Construct a new BookmarkList.
   *
   * @param {string[]} urls
   *   The initial set of URLs to track.
   * @param {function} [observer]
   *   The function to call when changes are made.
   * @param {number} [debounceRate]
   *   Time between observer executions, in milliseconds.
   * @param {number} [debounceTimeout]
   *   The maximum time to wait for an idle callback, in milliseconds.
   */
  constructor(urls, observer, debounceRate, debounceTimeout) {
    this.setTrackedUrls(urls);
    this.#observer = observer;
    this.handlePlacesEvents = this.handlePlacesEvents.bind(this);
    this.addListeners(debounceRate, debounceTimeout);
  }

  /**
   * Add places listeners to this bookmark list. The observer (if one was
   * provided) will be called after processing any events.
   *
   * @param {number} [debounceRate]
   *   Time between observer executions, in milliseconds.
   * @param {number} [debounceTimeout]
   *   The maximum time to wait for an idle callback, in milliseconds.
   */
  addListeners(
    debounceRate = OBSERVER_DEBOUNCE_RATE_MS,
    debounceTimeout = OBSERVER_DEBOUNCE_TIMEOUT_MS
  ) {
    lazy.PlacesUtils.observers.addListener(
      ["bookmark-added", "bookmark-removed", "bookmark-url-changed"],
      this.handlePlacesEvents
    );
    this.#observerTask = new lazy.DeferredTask(
      () => this.#observer?.(),
      debounceRate,
      debounceTimeout
    );
  }

  /**
   * Update the set of URLs to track.
   *
   * @param {string[]} urls
   */
  async setTrackedUrls(urls) {
    const updatedBookmarkCount = new Map();
    for (const url of urls) {
      // Use cached value if possible. Otherwise, it must be fetched from db.
      const urlHash = lazy.PlacesUtils.history.hashURL(url);
      const count = this.#bookmarkCount.get(urlHash);
      if (count != undefined) {
        updatedBookmarkCount.set(urlHash, count);
      } else {
        this.#urlsToFetch.add(urlHash);
      }
    }
    this.#bookmarkCount = updatedBookmarkCount;

    const updateGuidToUrl = new Map();
    for (const [guid, urlHash] of this.#guidToUrl.entries()) {
      if (updatedBookmarkCount.has(urlHash)) {
        updateGuidToUrl.set(guid, urlHash);
      }
    }
    this.#guidToUrl = updateGuidToUrl;
  }

  /**
   * Check whether the given URL is bookmarked.
   *
   * @param {string} url
   * @returns {boolean}
   *   The result, or `undefined` if the URL is not tracked.
   */
  async isBookmark(url) {
    if (this.#urlsToFetch.size) {
      await this.#fetchTrackedUrls();
    }
    const urlHash = lazy.PlacesUtils.history.hashURL(url);
    const count = this.#bookmarkCount.get(urlHash);
    return count != undefined ? Boolean(count) : count;
  }

  /**
   * Run the database query and populate the bookmarks cache with the URLs
   * that are waiting to be fetched.
   */
  async #fetchTrackedUrls() {
    const urls = [...this.#urlsToFetch];
    this.#urlsToFetch = new Set();
    for (const urlHash of urls) {
      this.#bookmarkCount.set(urlHash, 0);
    }
    const db = await lazy.PlacesUtils.promiseDBConnection();
    for (const chunk of lazy.PlacesUtils.chunkArray(urls, db.variableLimit)) {
      // Note that this query does not *explicitly* filter out tags, but we
      // should not expect to find any, unless the db is somehow malformed.
      const sql = `SELECT b.guid, p.url_hash
        FROM moz_bookmarks b
        JOIN moz_places p
        ON b.fk = p.id
        WHERE p.url_hash IN (${Array(chunk.length).fill("?").join(",")})`;
      const rows = await db.executeCached(sql, chunk);
      for (const row of rows) {
        this.#cacheBookmark(
          row.getResultByName("guid"),
          row.getResultByName("url_hash")
        );
      }
    }
  }

  /**
   * Handle bookmark events and update the cache accordingly.
   *
   * @param {PlacesEvent[]} events
   */
  async handlePlacesEvents(events) {
    let cacheUpdated = false;
    let needsFetch = false;
    for (const { guid, type, url } of events) {
      const urlHash = lazy.PlacesUtils.history.hashURL(url);
      if (this.#urlsToFetch.has(urlHash)) {
        needsFetch = true;
        continue;
      }
      const isUrlTracked = this.#bookmarkCount.has(urlHash);
      switch (type) {
        case "bookmark-added":
          if (isUrlTracked) {
            this.#cacheBookmark(guid, urlHash);
            cacheUpdated = true;
          }
          break;
        case "bookmark-removed":
          if (isUrlTracked) {
            this.#removeCachedBookmark(guid, urlHash);
            cacheUpdated = true;
          }
          break;
        case "bookmark-url-changed": {
          const oldUrlHash = this.#guidToUrl.get(guid);
          if (oldUrlHash) {
            this.#removeCachedBookmark(guid, oldUrlHash);
            cacheUpdated = true;
          }
          if (isUrlTracked) {
            this.#cacheBookmark(guid, urlHash);
            cacheUpdated = true;
          }
          break;
        }
      }
    }
    if (needsFetch) {
      await this.#fetchTrackedUrls();
      cacheUpdated = true;
    }
    if (cacheUpdated) {
      this.#observerTask.arm();
    }
  }

  /**
   * Remove places listeners from this bookmark list. URLs are no longer
   * tracked.
   *
   * In order to resume tracking, you must call `setTrackedUrls()` followed by
   * `addListeners()`.
   */
  removeListeners() {
    lazy.PlacesUtils.observers.removeListener(
      ["bookmark-added", "bookmark-removed", "bookmark-url-changed"],
      this.handlePlacesEvents
    );
    if (!this.#observerTask.isFinalized) {
      this.#observerTask.disarm();
      this.#observerTask.finalize();
    }
    this.setTrackedUrls([]);
  }

  /**
   * Store a bookmark in the cache.
   *
   * @param {string} guid
   * @param {string} urlHash
   */
  #cacheBookmark(guid, urlHash) {
    const count = this.#bookmarkCount.get(urlHash);
    this.#bookmarkCount.set(urlHash, count + 1);
    this.#guidToUrl.set(guid, urlHash);
  }

  /**
   * Remove a bookmark from the cache.
   *
   * @param {string} guid
   * @param {string} urlHash
   */
  #removeCachedBookmark(guid, urlHash) {
    const count = this.#bookmarkCount.get(urlHash);
    this.#bookmarkCount.set(urlHash, count - 1);
    this.#guidToUrl.delete(guid);
  }
}
