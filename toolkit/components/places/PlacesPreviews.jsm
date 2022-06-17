/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["PlacesPreviews", "PlacesPreviewsHelperService"];

const { ComponentUtils } = ChromeUtils.import(
  "resource://gre/modules/ComponentUtils.jsm"
);
const { EventEmitter } = ChromeUtils.import(
  "resource://gre/modules/EventEmitter.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  clearTimeout: "resource://gre/modules/Timer.jsm",
  BackgroundPageThumbs: "resource://gre/modules/BackgroundPageThumbs.jsm",
  PageThumbsStorage: "resource://gre/modules/PageThumbs.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  setTimeout: "resource://gre/modules/Timer.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "logConsole", function() {
  return console.createInstance({
    prefix: "PlacesPreviews",
    maxLogLevel: Services.prefs.getBoolPref("places.previews.log", false)
      ? "Debug"
      : "Warn",
  });
});

// Toggling Places previews requires a restart, because a database trigger
// filling up tombstones is enabled on the database only when the pref is set
// on startup.
XPCOMUtils.defineLazyGetter(lazy, "previewsEnabled", function() {
  return Services.prefs.getBoolPref("places.previews.enabled", false);
});

// Preview deletions are done in chunks of this size.
const DELETE_CHUNK_SIZE = 50;
// This is the time between deletion chunks.
const DELETE_TIMEOUT_MS = 60000;

// The folder inside the profile folder where to store previews.
const PREVIEWS_DIRECTORY = "places-previews";

// How old a preview file should be before we replace it.
const DAYS_BEFORE_REPLACEMENT = 30;

/**
 * This extends Set to only keep the latest 100 entries.
 */
class LimitedSet extends Set {
  #limit = 100;
  add(key) {
    super.add(key);
    let oversize = this.size - this.#limit;
    if (oversize > 0) {
      for (let entry of this) {
        if (oversize-- <= 0) {
          break;
        }
        this.delete(entry);
      }
    }
  }
}

/**
 * This class handles previews deletion from tombstones in the database.
 * Deletion happens in chunks, each chunk runs after DELETE_TIMEOUT_MS, and
 * the process is interrupted once there's nothing more to delete.
 * Any page removal operations on the Places database will restart the timer.
 */
class DeletionHandler {
  #timeoutId = null;
  #shutdownProgress = {};

  /**
   * This can be set by tests to speed up the deletion process, otherwise the
   * product should just use the default value.
   */
  #timeout = DELETE_TIMEOUT_MS;
  get timeout() {
    return this.#timeout;
  }
  set timeout(val) {
    if (this.#timeoutId) {
      lazy.clearTimeout(this.#timeoutId);
      this.#timeoutId = null;
    }
    this.#timeout = val;
    this.ensureRunning();
  }

  constructor() {
    // Clear any pending timeouts on shutdown.
    lazy.PlacesUtils.history.shutdownClient.jsclient.addBlocker(
      "PlacesPreviews.jsm::DeletionHandler",
      async () => {
        this.#shutdownProgress.shuttingDown = true;
        lazy.clearTimeout(this.#timeoutId);
        this.#timeoutId = null;
      },
      { fetchState: () => this.#shutdownProgress }
    );
  }

  /**
   * This should be invoked everytime we expect there are tombstones to
   * handle. If deletion is already pending, this is a no-op.
   */
  ensureRunning() {
    if (this.#timeoutId || this.#shutdownProgress.shuttingDown) {
      return;
    }
    this.#timeoutId = lazy.setTimeout(() => {
      this.#timeoutId = null;
      ChromeUtils.idleDispatch(() => {
        this.#deleteChunk().catch(ex =>
          lazy.logConsole.error("Error during previews deletion:" + ex)
        );
      });
    }, this.timeout);
  }

  /**
   * Deletes a chunk of previews.
   */
  async #deleteChunk() {
    if (this.#shutdownProgress.shuttingDown) {
      return;
    }
    // Select tombstones, delete images, then delete tombstones. This order
    // ensures that in case of problems we'll try again in the future.
    let db = await lazy.PlacesUtils.promiseDBConnection();
    let count;
    let hashes = (
      await db.executeCached(
        `SELECT hash, (SELECT count(*) FROM moz_previews_tombstones) AS count
         FROM moz_previews_tombstones LIMIT ${DELETE_CHUNK_SIZE}`
      )
    ).map(r => {
      if (count === undefined) {
        count = r.getResultByName("count");
      }
      return r.getResultByName("hash");
    });
    if (!count || this.#shutdownProgress.shuttingDown) {
      // There's nothing to delete, or it's too late.
      return;
    }

    let deleted = [];
    for (let hash of hashes) {
      let filePath = PlacesPreviews.getPathForHash(hash);
      try {
        await IOUtils.remove(filePath);
        PlacesPreviews.onDelete(filePath);
        deleted.push(hash);
      } catch (ex) {
        if (DOMException.isInstance(ex) && ex.name == "NotFoundError") {
          deleted.push(hash);
        } else {
          lazy.logConsole.error("Unable to delete file: " + filePath);
        }
      }
      if (this.#shutdownProgress.shuttingDown) {
        return;
      }
    }
    // Delete hashes from tombstones.
    let params = deleted.reduce((p, c, i) => {
      p["hash" + i] = c;
      return p;
    }, {});
    await lazy.PlacesUtils.withConnectionWrapper(
      "PlacesPreviews.jsm::ExpirePreviews",
      async db => {
        await db.execute(
          `DELETE FROM moz_previews_tombstones WHERE hash in
            (${Object.keys(params)
              .map(p => `:${p}`)
              .join(",")})`,
          params
        );
      }
    );

    if (count > DELETE_CHUNK_SIZE) {
      this.ensureRunning();
    }
  }
}

/**
 * Handles previews for Places urls.
 * Previews are stored in WebP format, using MD5 hash of the page url in hex
 * format. All the previews are saved into a "places-previews" folder under
 * the roaming profile folder.
 */
const PlacesPreviews = new (class extends EventEmitter {
  #placesObserver = null;
  #deletionHandler = null;
  // This is used as a cache to avoid fetching the same preview multiple
  // times in a short timeframe.
  #recentlyUpdatedPreviews = new LimitedSet();

  fileExtension = ".webp";
  fileContentType = "image/webp";

  constructor() {
    super();
    // Observe page removals and delete previews when necessary.
    this.#placesObserver = new PlacesWeakCallbackWrapper(
      this.handlePlacesEvents.bind(this)
    );
    PlacesObservers.addListener(
      ["history-cleared", "page-removed"],
      this.#placesObserver
    );

    // Start deletion in case it was interruped during the previous session,
    // it will end once there's nothing more to delete.
    this.#deletionHandler = new DeletionHandler();
    this.#deletionHandler.ensureRunning();
  }

  handlePlacesEvents(events) {
    for (const event of events) {
      if (
        event.type == "history-cleared" ||
        (event.type == "page-removed" && event.isRemovedFromStore)
      ) {
        this.#deletionHandler.ensureRunning();
        return;
      }
    }
  }

  /**
   * Whether the feature is enabled. Use this instead of directly checking
   * the pref, since it requires a restart.
   */
  get enabled() {
    return lazy.previewsEnabled;
  }

  /**
   * Returns the path to the previews folder.
   * @returns {string} The path to the previews folder.
   */
  getPath() {
    return PathUtils.join(
      Services.dirsvc.get("ProfD", Ci.nsIFile).path,
      PREVIEWS_DIRECTORY
    );
  }

  /**
   * Returns the file path of the preview for the given url.
   * This doesn't guarantee the file exists.
   * @param {string} url Address of the page.
   * @returns {string} File path of the preview for the given url.
   */
  getPathForUrl(url) {
    return PathUtils.join(
      this.getPath(),
      lazy.PlacesUtils.md5(url, { format: "hex" }) + this.fileExtension
    );
  }

  /**
   * Returns the file path of the preview having the given hash.
   * @param {string} hash md5 hash in hex format.
   * @returns {string } File path of the preview having the given hash.
   */
  getPathForHash(hash) {
    return PathUtils.join(this.getPath(), hash + this.fileExtension);
  }

  /**
   * Returns the moz-page-thumb: url to show the preview for the given url.
   * @param {string} url Address of the page.
   * @returns {string} Preview url for the given page url.
   */
  getPageThumbURL(url) {
    return (
      "moz-page-thumb://" +
      "places-previews" +
      "/?url=" +
      encodeURIComponent(url) +
      "&revision=" +
      lazy.PageThumbsStorage.getRevision(url)
    );
  }

  /**
   * Updates the preview for the given page url. The update happens in
   * background, using a windowless browser with very conservative privacy
   * settings. Due to this, it may not look exactly like the page that the user
   * is normally facing when logged in. See BackgroundPageThumbs.jsm for
   * additional details.
   * Unless `forceUpdate` is set, the preview is not updated if:
   *  - It was already fetched recently
   *  - The stored preview is younger than DAYS_BEFORE_REPLACEMENT
   * The previem image is encoded using WebP.
   * @param {string} url The address of the page.
   * @param {boolean} [forceUpdate] Whether to update the preview regardless.
   * @returns {boolean} Whether a preview is available and ready.
   */
  async update(url, { forceUpdate = false } = {}) {
    if (!this.enabled) {
      return false;
    }
    let filePath = this.getPathForUrl(url);
    if (!forceUpdate) {
      if (this.#recentlyUpdatedPreviews.has(filePath)) {
        lazy.logConsole.debug("Skipping update because recently updated");
        return true;
      }
      try {
        let fileInfo = await IOUtils.stat(filePath);
        if (
          fileInfo.lastModified >
          Date.now() - DAYS_BEFORE_REPLACEMENT * 86400000
        ) {
          // File is recent enough.
          this.#recentlyUpdatedPreviews.add(filePath);
          lazy.logConsole.debug("Skipping update because file is recent");
          return true;
        }
      } catch (ex) {
        // If the file doesn't exist, we always update it.
        if (!DOMException.isInstance(ex) || ex.name != "NotFoundError") {
          lazy.logConsole.error("Error while trying to stat() preview" + ex);
          return false;
        }
      }
    }

    let buffer = await new Promise(resolve => {
      let observer = (subject, topic, errorUrl) => {
        if (errorUrl == url) {
          resolve(null);
        }
      };
      Services.obs.addObserver(observer, "page-thumbnail:error");
      lazy.BackgroundPageThumbs.capture(url, {
        dontStore: true,
        contentType: this.fileContentType,
        onDone: (url, reason, handle) => {
          Services.obs.removeObserver(observer, "page-thumbnail:error");
          resolve(handle?.data);
        },
      });
    });
    if (!buffer) {
      lazy.logConsole.error("Unable to fetch preview: " + url);
      return false;
    }
    try {
      await IOUtils.makeDirectory(this.getPath(), { ignoreExisting: true });
      await IOUtils.write(filePath, new Uint8Array(buffer), {
        tmpPath: filePath + ".tmp",
      });
    } catch (ex) {
      lazy.logConsole.error(
        lazy.logConsole.error("Unable to create preview: " + ex)
      );
      return false;
    }
    this.#recentlyUpdatedPreviews.add(filePath);
    return true;
  }

  /**
   * Removes orphan previews that are not tracked by Places.
   * Orphaning should normally not happen, but unexpected manipulation (e.g. the
   * user touching the profile folder, or third party applications) could cause
   * it.
   * This method is slow, because it has to go through all the Places stored
   * pages, thus it's suggested to only run it as periodic maintenance.
   * @returns {boolean} Whether orphans deletion ran.
   */
  async deleteOrphans() {
    if (!this.enabled) {
      return false;
    }

    // From the previews directory, get all the files whose name matches our
    // format.  Avoid any other filenames, also for safety reasons, since we are
    // injecting them into SQL.
    let files = await IOUtils.getChildren(this.getPath());
    let hashes = files
      .map(f => PathUtils.filename(f))
      .filter(n => /^[a-f0-9]{32}\.webp$/)
      .map(n => n.substring(0, n.lastIndexOf(".")));

    await lazy.PlacesUtils.withConnectionWrapper(
      "PlacesPreviews.jsm::deleteOrphans",
      async db => {
        await db.execute(
          `
          WITH files(hash) AS (
            VALUES ${hashes.map(h => `('${h}')`).join(", ")}
          )
          INSERT OR IGNORE INTO moz_previews_tombstones
            SELECT hash FROM files
            EXCEPT
            SELECT md5hex(url) FROM moz_places
          `
        );
      }
    );
    this.#deletionHandler.ensureRunning();
    return true;
  }

  /**
   * This is invoked by #deletionHandler every time a preview file is removed.
   * @param {string} filePath The path of the deleted file.
   */
  onDelete(filePath) {
    this.#recentlyUpdatedPreviews.delete(filePath);
    this.emit("places-preview-deleted", filePath);
  }

  /**
   * Used by tests to change the deletion timeout between chunks.
   * @param {integer} timeout New timeout in milliseconds.
   */
  testSetDeletionTimeout(timeout) {
    if (timeout === null) {
      this.#deletionHandler.timeout = DELETE_TIMEOUT_MS;
    } else {
      this.#deletionHandler.timeout = timeout;
    }
  }
})();

/**
 * Used to exposes nsIPlacesPreviewsHelperService to the moz-page-thumb protocol
 * cpp implementation.
 */
function PlacesPreviewsHelperService() {}
PlacesPreviewsHelperService.prototype = {
  classID: Components.ID("{bd0a4d3b-ff26-4d4d-9a62-a513e1c1bf92}"),
  QueryInterface: ChromeUtils.generateQI(["nsIPlacesPreviewsHelperService"]),
  _xpcom_factory: ComponentUtils.generateSingletonFactory(
    PlacesPreviewsHelperService
  ),

  getFilePathForURL(url) {
    return PlacesPreviews.getPathForUrl(url);
  },
};
