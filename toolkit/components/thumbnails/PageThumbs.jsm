/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["PageThumbs", "PageThumbsStorage"];

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

const HTML_NAMESPACE = "http://www.w3.org/1999/xhtml";
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

/**
 * The default background color for page thumbnails.
 */
const THUMBNAIL_BG_COLOR = "#fff";

Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://gre/modules/PromiseWorker.jsm", this);
Cu.import("resource://gre/modules/Promise.jsm", this);
Cu.import("resource://gre/modules/osfile.jsm", this);

XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
  "resource://gre/modules/NetUtil.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
  "resource://gre/modules/FileUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "gUpdateTimerManager",
  "@mozilla.org/updates/timer-manager;1", "nsIUpdateTimerManager");

XPCOMUtils.defineLazyGetter(this, "gCryptoHash", function () {
  return Cc["@mozilla.org/security/hash;1"].createInstance(Ci.nsICryptoHash);
});

XPCOMUtils.defineLazyGetter(this, "gUnicodeConverter", function () {
  let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                    .createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = 'utf8';
  return converter;
});

XPCOMUtils.defineLazyModuleGetter(this, "Task",
  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Deprecated",
  "resource://gre/modules/Deprecated.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AsyncShutdown",
  "resource://gre/modules/AsyncShutdown.jsm");

/**
 * Utilities for dealing with promises and Task.jsm
 */
const TaskUtils = {
  /**
   * Read the bytes from a blob, asynchronously.
   *
   * @return {Promise}
   * @resolve {ArrayBuffer} In case of success, the bytes contained in the blob.
   * @reject {DOMError} In case of error, the underlying DOMError.
   */
  readBlob: function readBlob(blob) {
    let deferred = Promise.defer();
    let reader = Cc["@mozilla.org/files/filereader;1"].createInstance(Ci.nsIDOMFileReader);
    reader.onloadend = function onloadend() {
      if (reader.readyState != Ci.nsIDOMFileReader.DONE) {
        deferred.reject(reader.error);
      } else {
        deferred.resolve(reader.result);
      }
    };
    reader.readAsArrayBuffer(blob);
    return deferred.promise;
  }
};




/**
 * Singleton providing functionality for capturing web page thumbnails and for
 * accessing them if already cached.
 */
this.PageThumbs = {
  _initialized: false,

  /**
   * The calculated width and height of the thumbnails.
   */
  _thumbnailWidth : 0,
  _thumbnailHeight : 0,

  /**
   * The scheme to use for thumbnail urls.
   */
  get scheme() "moz-page-thumb",

  /**
   * The static host to use for thumbnail urls.
   */
  get staticHost() "thumbnail",

  /**
   * The thumbnails' image type.
   */
  get contentType() "image/png",

  init: function PageThumbs_init() {
    if (!this._initialized) {
      this._initialized = true;
      PlacesUtils.history.addObserver(PageThumbsHistoryObserver, false);

      // Migrate the underlying storage, if needed.
      PageThumbsStorageMigrator.migrate();
      PageThumbsExpiration.init();
    }
  },

  uninit: function PageThumbs_uninit() {
    if (this._initialized) {
      this._initialized = false;
      PlacesUtils.history.removeObserver(PageThumbsHistoryObserver);
    }
  },

  /**
   * Gets the thumbnail image's url for a given web page's url.
   * @param aUrl The web page's url that is depicted in the thumbnail.
   * @return The thumbnail image's url.
   */
  getThumbnailURL: function PageThumbs_getThumbnailURL(aUrl) {
    return this.scheme + "://" + this.staticHost +
           "?url=" + encodeURIComponent(aUrl);
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
     return PageThumbsStorage.getFilePathForURL(aUrl);
   },

  /**
   * Captures a thumbnail for the given window.
   * @param aWindow The DOM window to capture a thumbnail from.
   * @param aCallback The function to be called when the thumbnail has been
   *                  captured. The first argument will be the data stream
   *                  containing the image data.
   */
  capture: function PageThumbs_capture(aWindow, aCallback) {
    if (!this._prefEnabled()) {
      return;
    }

    let canvas = this.createCanvas();
    this.captureToCanvas(aWindow, canvas);

    // Fetch the canvas data on the next event loop tick so that we allow
    // some event processing in between drawing to the canvas and encoding
    // its data. We want to block the UI as short as possible. See bug 744100.
    Services.tm.currentThread.dispatch(function () {
      canvas.mozFetchAsStream(aCallback, this.contentType);
    }.bind(this), Ci.nsIThread.DISPATCH_NORMAL);
  },


  /**
   * Captures a thumbnail for the given window.
   *
   * @param aWindow The DOM window to capture a thumbnail from.
   * @return {Promise}
   * @resolve {Blob} The thumbnail, as a Blob.
   */
  captureToBlob: function PageThumbs_captureToBlob(aWindow) {
    if (!this._prefEnabled()) {
      return null;
    }

    let canvas = this.createCanvas();
    this.captureToCanvas(aWindow, canvas);

    let deferred = Promise.defer();
    let type = this.contentType;
    // Fetch the canvas data on the next event loop tick so that we allow
    // some event processing in between drawing to the canvas and encoding
    // its data. We want to block the UI as short as possible. See bug 744100.
    canvas.toBlob(function asBlob(blob) {
      deferred.resolve(blob, type);
    });
    return deferred.promise;
  },

  /**
   * Captures a thumbnail from a given window and draws it to the given canvas.
   * @param aWindow The DOM window to capture a thumbnail from.
   * @param aCanvas The canvas to draw to.
   */
  captureToCanvas: function PageThumbs_captureToCanvas(aWindow, aCanvas) {
    let telemetryCaptureTime = new Date();
    this._captureToCanvas(aWindow, aCanvas);
    let telemetry = Services.telemetry;
    telemetry.getHistogramById("FX_THUMBNAILS_CAPTURE_TIME_MS")
      .add(new Date() - telemetryCaptureTime);
  },

  // The background thumbnail service captures to canvas but doesn't want to
  // participate in this service's telemetry, which is why this method exists.
  _captureToCanvas: function PageThumbs__captureToCanvas(aWindow, aCanvas) {
    let [sw, sh, scale] = this._determineCropSize(aWindow, aCanvas);
    let ctx = aCanvas.getContext("2d");

    // Scale the canvas accordingly.
    ctx.save();
    ctx.scale(scale, scale);

    try {
      // Draw the window contents to the canvas.
      ctx.drawWindow(aWindow, 0, 0, sw, sh, THUMBNAIL_BG_COLOR,
                     ctx.DRAWWINDOW_DO_NOT_FLUSH);
    } catch (e) {
      // We couldn't draw to the canvas for some reason.
    }

    ctx.restore();
  },

  /**
   * Captures a thumbnail for the given browser and stores it to the cache.
   * @param aBrowser The browser to capture a thumbnail for.
   * @param aCallback The function to be called when finished (optional).
   */
  captureAndStore: function PageThumbs_captureAndStore(aBrowser, aCallback) {
    if (!this._prefEnabled()) {
      return;
    }

    let url = aBrowser.currentURI.spec;
    let channel = aBrowser.docShell.currentDocumentChannel;
    let originalURL = channel.originalURI.spec;

    // see if this was an error response.
    let wasError = this._isChannelErrorResponse(channel);

    Task.spawn((function task() {
      let isSuccess = true;
      try {
        let blob = yield this.captureToBlob(aBrowser.contentWindow);
        let buffer = yield TaskUtils.readBlob(blob);
        yield this._store(originalURL, url, buffer, wasError);
      } catch (_) {
        isSuccess = false;
      }
      if (aCallback) {
        aCallback(isSuccess);
      }
    }).bind(this));
  },

  /**
   * Checks if an existing thumbnail for the specified URL is either missing
   * or stale, and if so, captures and stores it.  Once the thumbnail is stored,
   * an observer service notification will be sent, so consumers should observe
   * such notifications if they want to be notified of an updated thumbnail.
   *
   * @param aBrowser The content window of this browser will be captured.
   * @param aCallback The function to be called when finished (optional).
   */
  captureAndStoreIfStale: function PageThumbs_captureAndStoreIfStale(aBrowser, aCallback) {
    let url = aBrowser.currentURI.spec;
    PageThumbsStorage.isFileRecentForURL(url).then(recent => {
      if (!recent &&
          // Careful, the call to PageThumbsStorage is async, so the browser may
          // have navigated away from the URL or even closed.
          aBrowser.currentURI &&
          aBrowser.currentURI.spec == url) {
        this.captureAndStore(aBrowser, aCallback);
      } else if (aCallback) {
        aCallback(true);
      }
    }, err => {
      if (aCallback)
        aCallback(false);
    });
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
   * @return {Promise}
   */
  _store: function PageThumbs__store(aOriginalURL, aFinalURL, aData, aNoOverwrite) {
    return Task.spawn(function () {
      let telemetryStoreTime = new Date();
      yield PageThumbsStorage.writeData(aFinalURL, aData, aNoOverwrite);
      Services.telemetry.getHistogramById("FX_THUMBNAILS_STORE_TIME_MS")
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
        yield PageThumbsStorage.copy(aFinalURL, aOriginalURL, aNoOverwrite);
        Services.obs.notifyObservers(null, "page-thumbnail:create", aOriginalURL);
      }
    });
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
   * Determines the crop size for a given content window.
   * @param aWindow The content window.
   * @param aCanvas The target canvas.
   * @return An array containing width, height and scale.
   */
  _determineCropSize: function PageThumbs_determineCropSize(aWindow, aCanvas) {
    let utils = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIDOMWindowUtils);
    let sbWidth = {}, sbHeight = {};

    try {
      utils.getScrollbarSize(false, sbWidth, sbHeight);
    } catch (e) {
      // This might fail if the window does not have a presShell.
      Cu.reportError("Unable to get scrollbar size in _determineCropSize.");
      sbWidth.value = sbHeight.value = 0;
    }

    // Even in RTL mode, scrollbars are always on the right.
    // So there's no need to determine a left offset.
    let sw = aWindow.innerWidth - sbWidth.value;
    let sh = aWindow.innerHeight - sbHeight.value;

    let {width: thumbnailWidth, height: thumbnailHeight} = aCanvas;
    let scale = Math.min(Math.max(thumbnailWidth / sw, thumbnailHeight / sh), 1);
    let scaledWidth = sw * scale;
    let scaledHeight = sh * scale;

    if (scaledHeight > thumbnailHeight)
      sh -= Math.floor(Math.abs(scaledHeight - thumbnailHeight) * scale);

    if (scaledWidth > thumbnailWidth)
      sw -= Math.floor(Math.abs(scaledWidth - thumbnailWidth) * scale);

    return [sw, sh, scale];
  },

  /**
   * Creates a new hidden canvas element.
   * @param aWindow The document of this window will be used to create the
   *                canvas.  If not given, the hidden window will be used.
   * @return The newly created canvas.
   */
  createCanvas: function PageThumbs_createCanvas(aWindow) {
    let doc = (aWindow || Services.appShell.hiddenDOMWindow).document;
    let canvas = doc.createElementNS(HTML_NAMESPACE, "canvas");
    canvas.mozOpaque = true;
    canvas.mozImageSmoothingEnabled = true;
    let [thumbnailWidth, thumbnailHeight] = this._getThumbnailSize();
    canvas.width = thumbnailWidth;
    canvas.height = thumbnailHeight;
    return canvas;
  },

  /**
   * Calculates the thumbnail size based on current desktop's dimensions.
   * @return The calculated thumbnail size or a default if unable to calculate.
   */
  _getThumbnailSize: function PageThumbs_getThumbnailSize() {
    if (!this._thumbnailWidth || !this._thumbnailHeight) {
      let screenManager = Cc["@mozilla.org/gfx/screenmanager;1"]
                            .getService(Ci.nsIScreenManager);
      let left = {}, top = {}, width = {}, height = {};
      screenManager.primaryScreen.GetRectDisplayPix(left, top, width, height);
      this._thumbnailWidth = Math.round(width.value / 3);
      this._thumbnailHeight = Math.round(height.value / 3);
    }
    return [this._thumbnailWidth, this._thumbnailHeight];
  },

  /**
   * Given a channel, returns true if it should be considered an "error
   * response", false otherwise.
   */
  _isChannelErrorResponse: function(channel) {
    // No valid document channel sounds like an error to me!
    if (!channel)
      return true;
    if (!(channel instanceof Ci.nsIHttpChannel))
      // it might be FTP etc, so assume it's ok.
      return false;
    try {
      return !channel.requestSucceeded;
    } catch (_) {
      // not being able to determine success is surely failure!
      return true;
    }
  },

  _prefEnabled: function PageThumbs_prefEnabled() {
    try {
      return !Services.prefs.getBoolPref("browser.pagethumbnails.capturing_disabled");
    }
    catch (e) {
      return true;
    }
  },
};

this.PageThumbsStorage = {
  // The path for the storage
  _path: null,
  get path() {
    if (!this._path) {
      this._path = OS.Path.join(OS.Constants.Path.localProfileDir, THUMBNAIL_DIRECTORY);
    }
    return this._path;
  },

  ensurePath: function Storage_ensurePath() {
    // Create the directory (ignore any error if the directory
    // already exists). As all writes are done from the PageThumbsWorker
    // thread, which serializes its operations, this ensures that
    // future operations can proceed without having to check whether
    // the directory exists.
    return PageThumbsWorker.post("makeDir",
      [this.path, {ignoreExisting: true}]).then(
        null,
        function onError(aReason) {
          Components.utils.reportError("Could not create thumbnails directory" + aReason);
        });
  },

  getLeafNameForURL: function Storage_getLeafNameForURL(aURL) {
    if (typeof aURL != "string") {
      throw new TypeError("Expecting a string");
    }
    let hash = this._calculateMD5Hash(aURL);
    return hash + ".png";
  },

  getFilePathForURL: function Storage_getFilePathForURL(aURL) {
    return OS.Path.join(this.path, this.getLeafNameForURL(aURL));
  },

  /**
   * Write the contents of a thumbnail, off the main thread.
   *
   * @param {string} aURL The url for which to store a thumbnail.
   * @param {ArrayBuffer} aData The data to store in the thumbnail, as
   * an ArrayBuffer. This array buffer is neutered and cannot be
   * reused after the copy.
   * @param {boolean} aNoOverwrite If true and the thumbnail's file already
   * exists, the file will not be overwritten.
   *
   * @return {Promise}
   */
  writeData: function Storage_writeData(aURL, aData, aNoOverwrite) {
    let path = this.getFilePathForURL(aURL);
    this.ensurePath();
    aData = new Uint8Array(aData);
    let msg = [
      path,
      aData,
      {
        tmpPath: path + ".tmp",
        bytes: aData.byteLength,
        noOverwrite: aNoOverwrite,
        flush: false /*thumbnails do not require the level of guarantee provided by flush*/
      }];
    return PageThumbsWorker.post("writeAtomic", msg,
      msg /*we don't want that message garbage-collected,
           as OS.Shared.Type.void_t.in_ptr.toMsg uses C-level
           memory tricks to enforce zero-copy*/).
      then(null, this._eatNoOverwriteError(aNoOverwrite));
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
    let sourceFile = this.getFilePathForURL(aSourceURL);
    let targetFile = this.getFilePathForURL(aTargetURL);
    let options = { noOverwrite: aNoOverwrite };
    return PageThumbsWorker.post("copy", [sourceFile, targetFile, options]).
      then(null, this._eatNoOverwriteError(aNoOverwrite));
  },

  /**
   * Remove a single thumbnail, off the main thread.
   *
   * @return {Promise}
   */
  remove: function Storage_remove(aURL) {
    return PageThumbsWorker.post("remove", [this.getFilePathForURL(aURL)]);
  },

  /**
   * Remove all thumbnails, off the main thread.
   *
   * @return {Promise}
   */
  wipe: Task.async(function* Storage_wipe() {
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
      blocker);

    // Start the work only now that `profileBeforeChange` has had
    // a chance to throw an error.

    let promise = PageThumbsWorker.post("wipe", [this.path]);
    try {
      yield promise;
    }  finally {
       // Generally, we will be done much before profileBeforeChange,
       // so let's not hoard blockers.
       if ("removeBlocker" in AsyncShutdown.profileBeforeChange) {
         // `removeBlocker` was added with bug 985655. In the interest
         // of backporting, let's degrade gracefully if `removeBlocker`
         // doesn't exist.
         AsyncShutdown.profileBeforeChange.removeBlocker(blocker);
       }
    }
  }),

  fileExistsForURL: function Storage_fileExistsForURL(aURL) {
    return PageThumbsWorker.post("exists", [this.getFilePathForURL(aURL)]);
  },

  isFileRecentForURL: function Storage_isFileRecentForURL(aURL) {
    return PageThumbsWorker.post("isFileRecent",
                                 [this.getFilePathForURL(aURL),
                                  MAX_THUMBNAIL_AGE_SECS]);
  },

  _calculateMD5Hash: function Storage_calculateMD5Hash(aValue) {
    let hash = gCryptoHash;
    let value = gUnicodeConverter.convertToByteArray(aValue);

    hash.init(hash.MD5);
    hash.update(value, value.length);
    return this._convertToHexString(hash.finish(false));
  },

  _convertToHexString: function Storage_convertToHexString(aData) {
    let hex = "";
    for (let i = 0; i < aData.length; i++)
      hex += ("0" + aData.charCodeAt(i).toString(16)).slice(-2);
    return hex;
  },

  /**
   * For functions that take a noOverwrite option, OS.File throws an error if
   * the target file exists and noOverwrite is true.  We don't consider that an
   * error, and we don't want such errors propagated.
   *
   * @param {aNoOverwrite} The noOverwrite option used in the OS.File operation.
   *
   * @return {function} A function that should be passed as the second argument
   * to then() (the `onError` argument).
   */
  _eatNoOverwriteError: function Storage__eatNoOverwriteError(aNoOverwrite) {
    return function onError(err) {
      if (!aNoOverwrite ||
          !(err instanceof OS.File.Error) ||
          !err.becauseExists) {
        throw err;
      }
    };
  },

  // Deprecated, please do not use
  getFileForURL: function Storage_getFileForURL_DEPRECATED(aURL) {
    Deprecated.warning("PageThumbs.getFileForURL is deprecated. Please use PageThumbs.getFilePathForURL and OS.File",
                       "https://developer.mozilla.org/docs/JavaScript_OS.File");
    // Note: Once this method has been removed, we can get rid of the dependency towards FileUtils
    return new FileUtils.File(PageThumbsStorage.getFilePathForURL(aURL));
  }
};

let PageThumbsStorageMigrator = {
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
    local = OS.Constants.Path.localProfileDir,
    roaming = OS.Constants.Path.profileDir) {
    PageThumbsWorker.post(
      "moveOrDeleteAllThumbnails",
      [OS.Path.join(roaming, THUMBNAIL_DIRECTORY),
       OS.Path.join(local, THUMBNAIL_DIRECTORY)]
    );
  }
};

let PageThumbsExpiration = {
  _filters: [],

  init: function Expiration_init() {
    gUpdateTimerManager.registerTimer("browser-cleanup-thumbnails", this,
                                      EXPIRATION_INTERVAL_SECS);
  },

  addFilter: function Expiration_addFilter(aFilter) {
    this._filters.push(aFilter);
  },

  removeFilter: function Expiration_removeFilter(aFilter) {
    let index = this._filters.indexOf(aFilter);
    if (index > -1)
      this._filters.splice(index, 1);
  },

  notify: function Expiration_notify(aTimer) {
    let urls = [];
    let filtersToWaitFor = this._filters.length;

    let expire = function expire() {
      this.expireThumbnails(urls);
    }.bind(this);

    // No registered filters.
    if (!filtersToWaitFor) {
      expire();
      return;
    }

    function filterCallback(aURLs) {
      urls = urls.concat(aURLs);
      if (--filtersToWaitFor == 0)
        expire();
    }

    for (let filter of this._filters) {
      if (typeof filter == "function")
        filter(filterCallback)
      else
        filter.filterForThumbnailExpiration(filterCallback);
    }
  },

  expireThumbnails: function Expiration_expireThumbnails(aURLsToKeep) {
    let path = this.path;
    let keep = [PageThumbsStorage.getLeafNameForURL(url) for (url of aURLsToKeep)];
    let msg = [
      PageThumbsStorage.path,
      keep,
      EXPIRATION_MIN_CHUNK_SIZE
    ];

    return PageThumbsWorker.post(
      "expireFilesInDirectory",
      msg
    );
  }
};

/**
 * Interface to a dedicated thread handling I/O
 */
let PageThumbsWorker = new BasePromiseWorker("resource://gre/modules/PageThumbsWorker.js");
// As the PageThumbsWorker performs I/O, we can receive instances of
// OS.File.Error, so we need to install a decoder.
PageThumbsWorker.ExceptionHandlers["OS.File.Error"] = OS.File.Error.fromMsg;

let PageThumbsHistoryObserver = {
  onDeleteURI: function Thumbnails_onDeleteURI(aURI, aGUID) {
    PageThumbsStorage.remove(aURI.spec);
  },

  onClearHistory: function Thumbnails_onClearHistory() {
    PageThumbsStorage.wipe();
  },

  onTitleChanged: function () {},
  onBeginUpdateBatch: function () {},
  onEndUpdateBatch: function () {},
  onVisit: function () {},
  onPageChanged: function () {},
  onDeleteVisits: function () {},

  QueryInterface: XPCOMUtils.generateQI([Ci.nsINavHistoryObserver])
};
