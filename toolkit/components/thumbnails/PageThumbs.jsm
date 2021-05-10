/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["PageThumbs", "PageThumbsStorage"];

const PREF_STORAGE_VERSION = "browser.pagethumbnails.storage_version";
const LATEST_STORAGE_VERSION = 3;

const EXPIRATION_MIN_CHUNK_SIZE = 50;
const EXPIRATION_INTERVAL_SECS = 3600;

// If a request for a thumbnail comes in and we find one that is "stale"
// (or don't find one at all) we automatically queue a request to generate a
// new one.
const MAX_THUMBNAIL_AGE_SECS = 172800; // 2 days == 60*60*24*2 == 172800 secs.

/**
 * Name of the directory in the profile that contains the thumbnails.
 */
const THUMBNAIL_DIRECTORY = "thumbnails";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { BasePromiseWorker } = ChromeUtils.import(
  "resource://gre/modules/PromiseWorker.jsm"
);

XPCOMUtils.defineLazyGlobalGetters(this, ["FileReader"]);

XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.jsm",
  PageThumbUtils: "resource://gre/modules/PageThumbUtils.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
});

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gUpdateTimerManager",
  "@mozilla.org/updates/timer-manager;1",
  "nsIUpdateTimerManager"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "PageThumbsStorageService",
  "@mozilla.org/thumbnails/pagethumbs-service;1",
  "nsIPageThumbsStorageService"
);

/**
 * Utilities for dealing with promises.
 */
const TaskUtils = {
  /**
   * Read the bytes from a blob, asynchronously.
   *
   * @return {Promise}
   * @resolve {ArrayBuffer} In case of success, the bytes contained in the blob.
   * @reject {DOMException} In case of error, the underlying DOMException.
   */
  readBlob: function readBlob(blob) {
    return new Promise((resolve, reject) => {
      let reader = new FileReader();
      reader.onloadend = function onloadend() {
        if (reader.readyState != FileReader.DONE) {
          reject(reader.error);
        } else {
          resolve(reader.result);
        }
      };
      reader.readAsArrayBuffer(blob);
    });
  },
};

/**
 * Singleton providing functionality for capturing web page thumbnails and for
 * accessing them if already cached.
 */
var PageThumbs = {
  _initialized: false,

  /**
   * The calculated width and height of the thumbnails.
   */
  _thumbnailWidth: 0,
  _thumbnailHeight: 0,

  /**
   * The scheme to use for thumbnail urls.
   */
  get scheme() {
    return "moz-page-thumb";
  },

  /**
   * The static host to use for thumbnail urls.
   */
  get staticHost() {
    return "thumbnails";
  },

  /**
   * The thumbnails' image type.
   */
  get contentType() {
    return "image/png";
  },

  init: function PageThumbs_init() {
    if (!this._initialized) {
      this._initialized = true;

      this._placesObserver = new PlacesWeakCallbackWrapper(
        this.handlePlacesEvents.bind(this)
      );
      PlacesObservers.addListener(
        ["history-cleared", "page-removed"],
        this._placesObserver
      );

      // Migrate the underlying storage, if needed.
      PageThumbsStorageMigrator.migrate();
      PageThumbsExpiration.init();
    }
  },

  handlePlacesEvents(events) {
    for (const event of events) {
      switch (event.type) {
        case "history-cleared": {
          PageThumbsStorage.wipe();
          break;
        }
        case "page-removed": {
          if (event.isRemovedFromStore) {
            PageThumbsStorage.remove(event.url);
          }
          break;
        }
      }
    }
  },

  uninit: function PageThumbs_uninit() {
    if (this._initialized) {
      this._initialized = false;
    }
  },

  /**
   * Gets the thumbnail image's url for a given web page's url.
   * @param aUrl The web page's url that is depicted in the thumbnail.
   * @return The thumbnail image's url.
   */
  getThumbnailURL: function PageThumbs_getThumbnailURL(aUrl) {
    return (
      this.scheme +
      "://" +
      this.staticHost +
      "/?url=" +
      encodeURIComponent(aUrl) +
      "&revision=" +
      PageThumbsStorage.getRevision(aUrl)
    );
  },

  /**
   * Gets the path of the thumbnail file for a given web page's
   * url. This file may or may not exist depending on whether the
   * thumbnail has been captured or not.
   *
   * @param aUrl The web page's url.
   * @return The path of the thumbnail file.
   */
  getThumbnailPath: function PageThumbs_getThumbnailPath(aUrl) {
    return PageThumbsStorageService.getFilePathForURL(aUrl);
  },

  /**
   * Asynchronously returns a thumbnail as a blob for the given
   * window.
   *
   * @param aBrowser The <browser> to capture a thumbnail from.
   * @return {Promise}
   * @resolve {Blob} The thumbnail, as a Blob.
   */
  captureToBlob: function PageThumbs_captureToBlob(aBrowser) {
    if (!this._prefEnabled()) {
      return null;
    }

    return new Promise(resolve => {
      let canvas = this.createCanvas(aBrowser.ownerGlobal);
      this.captureToCanvas(aBrowser, canvas)
        .then(() => {
          canvas.toBlob(blob => {
            resolve(blob, this.contentType);
          });
        })
        .catch(e => Cu.reportError(e));
    });
  },

  /**
   * Captures a thumbnail from a given window and draws it to the given canvas.
   * Note, when dealing with remote content, this api draws into the passed
   * canvas asynchronously. Pass aCallback to receive an async callback after
   * canvas painting has completed.
   * @param aBrowser The browser to capture a thumbnail from.
   * @param aCanvas The canvas to draw to. The thumbnail will be scaled to match
   *   the dimensions of this canvas. If callers pass a 0x0 canvas, the canvas
   *   will be resized to default thumbnail dimensions just prior to painting.
   * @param aArgs (optional) Additional named parameters:
   *   fullScale - request that a non-downscaled image be returned.
   *   isImage - indicate that this should be treated as an image url.
   *   backgroundColor - background color to draw behind images.
   *   targetWidth - desired width for images.
   *   isBackgroundThumb - true if request is from the background thumb service.
   * @param aSkipTelemetry skip recording telemetry
   */
  async captureToCanvas(aBrowser, aCanvas, aArgs, aSkipTelemetry = false) {
    let telemetryCaptureTime = new Date();
    let args = {
      fullScale: aArgs ? aArgs.fullScale : false,
      isImage: aArgs ? aArgs.isImage : false,
      backgroundColor:
        aArgs?.backgroundColor ?? PageThumbUtils.THUMBNAIL_BG_COLOR,
      targetWidth: aArgs?.targetWidth ?? PageThumbUtils.THUMBNAIL_DEFAULT_SIZE,
      isBackgroundThumb: aArgs ? aArgs.isBackgroundThumb : false,
    };

    return this._captureToCanvas(aBrowser, aCanvas, args).then(() => {
      if (!aSkipTelemetry) {
        Services.telemetry
          .getHistogramById("FX_THUMBNAILS_CAPTURE_TIME_MS")
          .add(new Date() - telemetryCaptureTime);
      }
      return aCanvas;
    });
  },

  /**
   * Asynchronously check the state of aBrowser to see if it passes a set of
   * predefined security checks. Consumers should refrain from storing
   * thumbnails if these checks fail. Note the final result of this call is
   * transitory as it is based on current navigation state and the type of
   * content being displayed.
   *
   * @param aBrowser The target browser
   */
  async shouldStoreThumbnail(aBrowser) {
    // Don't capture in private browsing mode.
    if (PrivateBrowsingUtils.isBrowserPrivate(aBrowser)) {
      return false;
    }
    if (aBrowser.isRemoteBrowser) {
      if (aBrowser.browsingContext.currentWindowGlobal) {
        let thumbnailsActor = aBrowser.browsingContext.currentWindowGlobal.getActor(
          "Thumbnails"
        );
        return thumbnailsActor
          .sendQuery("Browser:Thumbnail:CheckState")
          .catch(err => {
            return false;
          });
      }
      return false;
    }
    return PageThumbUtils.shouldStoreContentThumbnail(
      aBrowser.contentDocument,
      aBrowser.docShell
    );
  },

  // The background thumbnail service captures to canvas but doesn't want to
  // participate in this service's telemetry, which is why this method exists.
  async _captureToCanvas(aBrowser, aCanvas, aArgs) {
    if (aBrowser.isRemoteBrowser) {
      let thumbnail = await this._captureRemoteThumbnail(
        aBrowser,
        aCanvas.width,
        aCanvas.height,
        aArgs
      );

      // 'thumbnail' can be null if the browser has navigated away after starting
      // the thumbnail request, so we check it here.
      if (thumbnail) {
        let ctx = thumbnail.getContext("2d");
        let imgData = ctx.getImageData(0, 0, thumbnail.width, thumbnail.height);
        aCanvas.width = thumbnail.width;
        aCanvas.height = thumbnail.height;
        aCanvas.getContext("2d").putImageData(imgData, 0, 0);
      }

      return aCanvas;
    }
    // The content is a local page, grab a thumbnail sync.
    await PageThumbUtils.createSnapshotThumbnail(aBrowser, aCanvas, aArgs);
    return aCanvas;
  },

  /**
   * Asynchrnously render an appropriately scaled thumbnail to canvas.
   *
   * @param aBrowser The browser to capture a thumbnail from.
   * @param aWidth The desired canvas width.
   * @param aHeight The desired canvas height.
   * @param aArgs (optional) Additional named parameters:
   *   fullScale - request that a non-downscaled image be returned.
   *   isImage - indicate that this should be treated as an image url.
   *   backgroundColor - background color to draw behind images.
   *   targetWidth - desired width for images.
   *   isBackgroundThumb - true if request is from the background thumb service.
   * @return a promise
   */
  async _captureRemoteThumbnail(aBrowser, aWidth, aHeight, aArgs) {
    if (!aBrowser.browsingContext || !aBrowser.parentElement) {
      return null;
    }

    let thumbnailsActor = aBrowser.browsingContext.currentWindowGlobal.getActor(
      aArgs.isBackgroundThumb ? "BackgroundThumbnails" : "Thumbnails"
    );
    let contentInfo = await thumbnailsActor.sendQuery(
      "Browser:Thumbnail:ContentInfo",
      {
        isImage: aArgs.isImage,
        targetWidth: aArgs.targetWidth,
        backgroundColor: aArgs.backgroundColor,
      }
    );

    let contentWidth = contentInfo.width;
    let contentHeight = contentInfo.height;
    if (contentWidth == 0 || contentHeight == 0) {
      throw new Error("IMAGE_ZERO_DIMENSION");
    }

    let doc = aBrowser.parentElement.ownerDocument;
    let thumbnail = doc.createElementNS(
      PageThumbUtils.HTML_NAMESPACE,
      "canvas"
    );

    let image;
    if (contentInfo.imageData) {
      thumbnail.width = contentWidth;
      thumbnail.height = contentHeight;

      image = new aBrowser.ownerGlobal.Image();
      await new Promise(resolve => {
        image.onload = resolve;
        image.src = contentInfo.imageData;
      });
    } else {
      let fullScale = aArgs ? aArgs.fullScale : false;
      let scale = fullScale
        ? 1
        : Math.min(Math.max(aWidth / contentWidth, aHeight / contentHeight), 1);

      image = await aBrowser.drawSnapshot(
        0,
        0,
        contentWidth,
        contentHeight,
        scale,
        aArgs.backgroundColor
      );

      thumbnail.width = fullScale ? contentWidth : aWidth;
      thumbnail.height = fullScale ? contentHeight : aHeight;
    }

    thumbnail.getContext("2d").drawImage(image, 0, 0);

    return thumbnail;
  },

  /**
   * Captures a thumbnail for the given browser and stores it to the cache.
   * @param aBrowser The browser to capture a thumbnail for.
   */
  captureAndStore: async function PageThumbs_captureAndStore(aBrowser) {
    if (!this._prefEnabled()) {
      return;
    }

    let url = aBrowser.currentURI.spec;
    let originalURL;
    let channelError = false;

    if (!aBrowser.isRemoteBrowser) {
      let channel = aBrowser.docShell.currentDocumentChannel;
      originalURL = channel.originalURI.spec;
      // see if this was an error response.
      channelError = PageThumbUtils.isChannelErrorResponse(channel);
    } else {
      let thumbnailsActor = aBrowser.browsingContext.currentWindowGlobal.getActor(
        "Thumbnails"
      );
      let resp = await thumbnailsActor.sendQuery(
        "Browser:Thumbnail:GetOriginalURL"
      );

      originalURL = resp.originalURL || url;
      channelError = resp.channelError;
    }

    try {
      let blob = await this.captureToBlob(aBrowser);
      let buffer = await TaskUtils.readBlob(blob);
      await this._store(originalURL, url, buffer, channelError);
    } catch (ex) {
      Cu.reportError("Exception thrown during thumbnail capture: '" + ex + "'");
    }
  },

  /**
   * Checks if an existing thumbnail for the specified URL is either missing
   * or stale, and if so, captures and stores it.  Once the thumbnail is stored,
   * an observer service notification will be sent, so consumers should observe
   * such notifications if they want to be notified of an updated thumbnail.
   *
   * @param aBrowser The content window of this browser will be captured.
   */
  captureAndStoreIfStale: async function PageThumbs_captureAndStoreIfStale(
    aBrowser
  ) {
    if (!aBrowser.currentURI) {
      return false;
    }
    let url = aBrowser.currentURI.spec;
    let recent;
    try {
      recent = await PageThumbsStorage.isFileRecentForURL(url);
    } catch {
      return false;
    }
    if (
      !recent &&
      // Careful, the call to PageThumbsStorage is async, so the browser may
      // have navigated away from the URL or even closed.
      aBrowser.currentURI &&
      aBrowser.currentURI.spec == url
    ) {
      await this.captureAndStore(aBrowser);
    }
    return true;
  },

  /**
   * Stores data to disk for the given URLs.
   *
   * NB: The background thumbnail service calls this, too.
   *
   * @param aOriginalURL The URL with which the capture was initiated.
   * @param aFinalURL The URL to which aOriginalURL ultimately resolved.
   * @param aData An ArrayBuffer containing the image data.
   * @param aNoOverwrite If true and files for the URLs already exist, the files
   *                     will not be overwritten.
   */
  _store: async function PageThumbs__store(
    aOriginalURL,
    aFinalURL,
    aData,
    aNoOverwrite
  ) {
    let telemetryStoreTime = new Date();
    await PageThumbsStorage.writeData(aFinalURL, aData, aNoOverwrite);
    Services.telemetry
      .getHistogramById("FX_THUMBNAILS_STORE_TIME_MS")
      .add(new Date() - telemetryStoreTime);

    Services.obs.notifyObservers(null, "page-thumbnail:create", aFinalURL);
    // We've been redirected. Create a copy of the current thumbnail for
    // the redirect source. We need to do this because:
    //
    // 1) Users can drag any kind of links onto the newtab page. If those
    //    links redirect to a different URL then we want to be able to
    //    provide thumbnails for both of them.
    //
    // 2) The newtab page should actually display redirect targets, only.
    //    Because of bug 559175 this information can get lost when using
    //    Sync and therefore also redirect sources appear on the newtab
    //    page. We also want thumbnails for those.
    if (aFinalURL != aOriginalURL) {
      await PageThumbsStorage.copy(aFinalURL, aOriginalURL, aNoOverwrite);
      Services.obs.notifyObservers(null, "page-thumbnail:create", aOriginalURL);
    }
  },

  /**
   * Register an expiration filter.
   *
   * When thumbnails are going to expire, each registered filter is asked for a
   * list of thumbnails to keep.
   *
   * The filter (if it is a callable) or its filterForThumbnailExpiration method
   * (if the filter is an object) is called with a single argument.  The
   * argument is a callback function.  The filter must call the callback
   * function and pass it an array of zero or more URLs.  (It may do so
   * asynchronously.)  Thumbnails for those URLs will be except from expiration.
   *
   * @param aFilter callable, or object with filterForThumbnailExpiration method
   */
  addExpirationFilter: function PageThumbs_addExpirationFilter(aFilter) {
    PageThumbsExpiration.addFilter(aFilter);
  },

  /**
   * Unregister an expiration filter.
   * @param aFilter A filter that was previously passed to addExpirationFilter.
   */
  removeExpirationFilter: function PageThumbs_removeExpirationFilter(aFilter) {
    PageThumbsExpiration.removeFilter(aFilter);
  },

  /**
   * Creates a new hidden canvas element.
   * @param aWindow The document of this window will be used to create the
   *                canvas.  If not given, the hidden window will be used.
   * @return The newly created canvas.
   */
  createCanvas: function PageThumbs_createCanvas(aWindow) {
    return PageThumbUtils.createCanvas(aWindow);
  },

  _prefEnabled: function PageThumbs_prefEnabled() {
    try {
      return !Services.prefs.getBoolPref(
        "browser.pagethumbnails.capturing_disabled"
      );
    } catch (e) {
      return true;
    }
  },
};

var PageThumbsStorage = {
  ensurePath: function Storage_ensurePath() {
    // Create the directory (ignore any error if the directory
    // already exists). As all writes are done from the PageThumbsWorker
    // thread, which serializes its operations, this ensures that
    // future operations can proceed without having to check whether
    // the directory exists.
    return PageThumbsWorker.post("makeDir", [
      PageThumbsStorageService.path,
      { ignoreExisting: true },
    ]).catch(function onError(aReason) {
      Cu.reportError("Could not create thumbnails directory" + aReason);
    });
  },

  _revisionTable: {},

  // Generate an arbitrary revision tag, i.e. one that can't be used to
  // infer URL frecency.
  _updateRevision(aURL) {
    // Initialize with a random value and increment on each update. Wrap around
    // modulo _revisionRange, so that even small values carry no meaning.
    let rev = this._revisionTable[aURL];
    if (rev == null) {
      rev = Math.floor(Math.random() * this._revisionRange);
    }
    this._revisionTable[aURL] = (rev + 1) % this._revisionRange;
  },

  // If two thumbnails with the same URL and revision are in cache at the
  // same time, the image loader may pick the stale thumbnail in some cases.
  // Therefore _revisionRange must be large enough to prevent this, e.g.
  // in the pathological case image.cache.size (5MB by default) could fill
  // with (abnormally small) 10KB thumbnail images if the browser session
  // runs long enough (though this is unlikely as thumbnails are usually
  // only updated every MAX_THUMBNAIL_AGE_SECS).
  _revisionRange: 8192,

  /**
   * Return a revision tag for the thumbnail stored for a given URL.
   *
   * @param aURL The URL spec string
   * @return A revision tag for the corresponding thumbnail. Returns a changed
   * value whenever the stored thumbnail changes.
   */
  getRevision(aURL) {
    let rev = this._revisionTable[aURL];
    if (rev == null) {
      this._updateRevision(aURL);
      rev = this._revisionTable[aURL];
    }
    return rev;
  },

  /**
   * Write the contents of a thumbnail, off the main thread.
   *
   * @param {string} aURL The url for which to store a thumbnail.
   * @param {ArrayBuffer} aData The data to store in the thumbnail, as
   * an ArrayBuffer. This array buffer will be detached and cannot be
   * reused after the copy.
   * @param {boolean} aNoOverwrite If true and the thumbnail's file already
   * exists, the file will not be overwritten.
   *
   * @return {Promise}
   */
  writeData: function Storage_writeData(aURL, aData, aNoOverwrite) {
    let path = PageThumbsStorageService.getFilePathForURL(aURL);
    this.ensurePath();
    aData = new Uint8Array(aData);
    let msg = [
      path,
      aData,
      {
        tmpPath: path + ".tmp",
        mode: aNoOverwrite ? "create" : "overwrite",
      },
    ];
    return PageThumbsWorker.post(
      "writeAtomic",
      msg,
      msg /* we don't want that message garbage-collected,
           as OS.Shared.Type.void_t.in_ptr.toMsg uses C-level
           memory tricks to enforce zero-copy*/
    ).then(
      () => this._updateRevision(aURL),
      this._eatNoOverwriteError(aNoOverwrite)
    );
  },

  /**
   * Copy a thumbnail, off the main thread.
   *
   * @param {string} aSourceURL The url of the thumbnail to copy.
   * @param {string} aTargetURL The url of the target thumbnail.
   * @param {boolean} aNoOverwrite If true and the target file already exists,
   * the file will not be overwritten.
   *
   * @return {Promise}
   */
  copy: function Storage_copy(aSourceURL, aTargetURL, aNoOverwrite) {
    this.ensurePath();
    let sourceFile = PageThumbsStorageService.getFilePathForURL(aSourceURL);
    let targetFile = PageThumbsStorageService.getFilePathForURL(aTargetURL);
    let options = { noOverwrite: aNoOverwrite };
    return PageThumbsWorker.post("copy", [
      sourceFile,
      targetFile,
      options,
    ]).then(
      () => this._updateRevision(aTargetURL),
      this._eatNoOverwriteError(aNoOverwrite)
    );
  },

  /**
   * Remove a single thumbnail, off the main thread.
   *
   * @return {Promise}
   */
  remove: function Storage_remove(aURL) {
    return PageThumbsWorker.post("remove", [
      PageThumbsStorageService.getFilePathForURL(aURL),
    ]);
  },

  /**
   * Remove all thumbnails, off the main thread.
   *
   * @return {Promise}
   */
  wipe: async function Storage_wipe() {
    //
    // This operation may be launched during shutdown, so we need to
    // take a few precautions to ensure that:
    //
    // 1. it is not interrupted by shutdown, in which case we
    //    could be leaving privacy-sensitive files on disk;
    // 2. it is not launched too late during shutdown, in which
    //    case this could cause shutdown freezes (see bug 1005487,
    //    which will eventually be fixed by bug 965309)
    //

    let blocker = () => promise;

    // The following operation will rise an error if we have already
    // reached profileBeforeChange, in which case it is too late
    // to clear the thumbnail wipe.
    AsyncShutdown.profileBeforeChange.addBlocker(
      "PageThumbs: removing all thumbnails",
      blocker
    );

    // Start the work only now that `profileBeforeChange` has had
    // a chance to throw an error.

    let promise = PageThumbsWorker.post("wipe", [
      PageThumbsStorageService.path,
    ]);
    try {
      await promise;
    } finally {
      // Generally, we will be done much before profileBeforeChange,
      // so let's not hoard blockers.
      if ("removeBlocker" in AsyncShutdown.profileBeforeChange) {
        // `removeBlocker` was added with bug 985655. In the interest
        // of backporting, let's degrade gracefully if `removeBlocker`
        // doesn't exist.
        AsyncShutdown.profileBeforeChange.removeBlocker(blocker);
      }
    }
  },

  fileExistsForURL: function Storage_fileExistsForURL(aURL) {
    return PageThumbsWorker.post("exists", [
      PageThumbsStorageService.getFilePathForURL(aURL),
    ]);
  },

  isFileRecentForURL: function Storage_isFileRecentForURL(aURL) {
    return PageThumbsWorker.post("isFileRecent", [
      PageThumbsStorageService.getFilePathForURL(aURL),
      MAX_THUMBNAIL_AGE_SECS,
    ]);
  },

  /**
   * For functions that take a noOverwrite option, IOUtils throws an error if
   * the target file exists and noOverwrite is true.  We don't consider that an
   * error, and we don't want such errors propagated.
   *
   * @param {aNoOverwrite} The noOverwrite option used in the IOUtils operation.
   *
   * @return {function} A function that should be passed as the second argument
   * to then() (the `onError` argument).
   */
  _eatNoOverwriteError: function Storage__eatNoOverwriteError(aNoOverwrite) {
    return function onError(err) {
      if (
        !aNoOverwrite ||
        !(err instanceof DOMException) ||
        err.name !== "TypeMismatchError"
      ) {
        throw err;
      }
    };
  },
};

var PageThumbsStorageMigrator = {
  get currentVersion() {
    try {
      return Services.prefs.getIntPref(PREF_STORAGE_VERSION);
    } catch (e) {
      // The pref doesn't exist, yet. Return version 0.
      return 0;
    }
  },

  set currentVersion(aVersion) {
    Services.prefs.setIntPref(PREF_STORAGE_VERSION, aVersion);
  },

  migrate: function Migrator_migrate() {
    let version = this.currentVersion;

    // Storage version 1 never made it to beta.
    // At the time of writing only Windows had (ProfD != ProfLD) and we
    // needed to move thumbnails from the roaming profile to the locale
    // one so that they're not needlessly included in backups and/or
    // written via SMB.

    // Storage version 2 also never made it to beta.
    // The thumbnail folder structure has been changed and old thumbnails
    // were not migrated. Instead, we just renamed the current folder to
    // "<name>-old" and will remove it later.

    if (version < 3) {
      this.migrateToVersion3();
    }

    this.currentVersion = LATEST_STORAGE_VERSION;
  },

  /**
   * Bug 239254 added support for having the disk cache and thumbnail
   * directories on a local path (i.e. ~/.cache/) under Linux. We'll first
   * try to move the old thumbnails to their new location. If that's not
   * possible (because ProfD might be on a different file system than
   * ProfLD) we'll just discard them.
   *
   * @param {string*} local The path to the local profile directory.
   * Used for testing. Default argument is good for all non-testing uses.
   * @param {string*} roaming The path to the roaming profile directory.
   * Used for testing. Default argument is good for all non-testing uses.
   */
  migrateToVersion3: function Migrator_migrateToVersion3(
    local = Services.dirsvc.get("ProfLD", Ci.nsIFile).path,
    roaming = Services.dirsvc.get("ProfD", Ci.nsIFile).path
  ) {
    PageThumbsWorker.post("moveOrDeleteAllThumbnails", [
      PathUtils.join(roaming, THUMBNAIL_DIRECTORY),
      PathUtils.join(local, THUMBNAIL_DIRECTORY),
    ]);
  },
};

var PageThumbsExpiration = {
  _filters: [],

  init: function Expiration_init() {
    gUpdateTimerManager.registerTimer(
      "browser-cleanup-thumbnails",
      this,
      EXPIRATION_INTERVAL_SECS
    );
  },

  addFilter: function Expiration_addFilter(aFilter) {
    this._filters.push(aFilter);
  },

  removeFilter: function Expiration_removeFilter(aFilter) {
    let index = this._filters.indexOf(aFilter);
    if (index > -1) {
      this._filters.splice(index, 1);
    }
  },

  notify: function Expiration_notify(aTimer) {
    let urls = [];
    let filtersToWaitFor = this._filters.length;

    let expire = () => {
      this.expireThumbnails(urls);
    };

    // No registered filters.
    if (!filtersToWaitFor) {
      expire();
      return;
    }

    function filterCallback(aURLs) {
      urls = urls.concat(aURLs);
      if (--filtersToWaitFor == 0) {
        expire();
      }
    }

    for (let filter of this._filters) {
      if (typeof filter == "function") {
        filter(filterCallback);
      } else {
        filter.filterForThumbnailExpiration(filterCallback);
      }
    }
  },

  expireThumbnails: function Expiration_expireThumbnails(aURLsToKeep) {
    let keep = aURLsToKeep.map(url =>
      PageThumbsStorageService.getLeafNameForURL(url)
    );
    let msg = [PageThumbsStorageService.path, keep, EXPIRATION_MIN_CHUNK_SIZE];

    return PageThumbsWorker.post("expireFilesInDirectory", msg);
  },
};

/**
 * Interface to a dedicated thread handling I/O
 */
var PageThumbsWorker = new BasePromiseWorker(
  "resource://gre/modules/PageThumbsWorker.js"
);
