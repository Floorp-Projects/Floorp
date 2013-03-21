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

/**
 * Name of the directory in the profile that contains the thumbnails.
 */
const THUMBNAIL_DIRECTORY = "thumbnails";

/**
 * The default background color for page thumbnails.
 */
const THUMBNAIL_BG_COLOR = "#fff";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

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

    let canvas = this._createCanvas();
    this.captureToCanvas(aWindow, canvas);

    // Fetch the canvas data on the next event loop tick so that we allow
    // some event processing in between drawing to the canvas and encoding
    // its data. We want to block the UI as short as possible. See bug 744100.
    Services.tm.currentThread.dispatch(function () {
      canvas.mozFetchAsStream(aCallback, this.contentType);
    }.bind(this), Ci.nsIThread.DISPATCH_NORMAL);
  },

  /**
   * Captures a thumbnail from a given window and draws it to the given canvas.
   * @param aWindow The DOM window to capture a thumbnail from.
   * @param aCanvas The canvas to draw to.
   */
  captureToCanvas: function PageThumbs_captureToCanvas(aWindow, aCanvas) {
    let telemetryCaptureTime = new Date();
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

    let telemetry = Services.telemetry;
    telemetry.getHistogramById("FX_THUMBNAILS_CAPTURE_TIME_MS")
      .add(new Date() - telemetryCaptureTime);
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

    this.capture(aBrowser.contentWindow, function (aInputStream) {
      let telemetryStoreTime = new Date();

      function finish(aSuccessful) {
        if (aSuccessful) {
          Services.telemetry.getHistogramById("FX_THUMBNAILS_STORE_TIME_MS")
            .add(new Date() - telemetryStoreTime);

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
          if (url != originalURL)
            PageThumbsStorage.copy(url, originalURL);
        }

        if (aCallback)
          aCallback(aSuccessful);
      }

      PageThumbsStorage.write(url, aInputStream, finish);
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
   * @return The newly created canvas.
   */
  _createCanvas: function PageThumbs_createCanvas() {
    let doc = Services.appShell.hiddenDOMWindow.document;
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
      screenManager.primaryScreen.GetRect(left, top, width, height);
      this._thumbnailWidth = Math.round(width.value / 3);
      this._thumbnailHeight = Math.round(height.value / 3);
    }
    return [this._thumbnailWidth, this._thumbnailHeight];
  },

  _prefEnabled: function PageThumbs_prefEnabled() {
    try {
      return Services.prefs.getBoolPref("browser.pageThumbs.enabled");
    }
    catch (e) {
      return true;
    }
  },
};

this.PageThumbsStorage = {
  getDirectory: function Storage_getDirectory(aCreate = true) {
    return FileUtils.getDir("ProfLD", [THUMBNAIL_DIRECTORY], aCreate);
  },

  getLeafNameForURL: function Storage_getLeafNameForURL(aURL) {
    let hash = this._calculateMD5Hash(aURL);
    return hash + ".png";
  },

  getFileForURL: function Storage_getFileForURL(aURL) {
    let file = this.getDirectory();
    file.append(this.getLeafNameForURL(aURL));
    return file;
  },

  write: function Storage_write(aURL, aDataStream, aCallback) {
    let file = this.getFileForURL(aURL);
    let fos = FileUtils.openSafeFileOutputStream(file);

    NetUtil.asyncCopy(aDataStream, fos, function (aResult) {
      FileUtils.closeSafeFileOutputStream(fos);
      aCallback(Components.isSuccessCode(aResult));
    });
  },

  copy: function Storage_copy(aSourceURL, aTargetURL) {
    let sourceFile = this.getFileForURL(aSourceURL);
    let targetFile = this.getFileForURL(aTargetURL);

    try {
      sourceFile.copyTo(targetFile.parent, targetFile.leafName);
    } catch (e) {
      /* We might not be permitted to write to the file. */
    }
  },

  remove: function Storage_remove(aURL) {
    let file = this.getFileForURL(aURL);
    PageThumbsWorker.postMessage({type: "removeFile", path: file.path});
  },

  wipe: function Storage_wipe() {
    let dir = this.getDirectory(false);
    dir.followLinks = false;
    try {
      dir.remove(true);
    } catch (e) {
      /* The directory might not exist or we're not permitted to remove it. */
    }
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
   */
  migrateToVersion3: function Migrator_migrateToVersion3() {
    let local = FileUtils.getDir("ProfLD", [THUMBNAIL_DIRECTORY], true);
    let roaming = FileUtils.getDir("ProfD", [THUMBNAIL_DIRECTORY]);

    if (!roaming.equals(local)) {
      PageThumbsWorker.postMessage({
        type: "moveOrDeleteAllThumbnails",
        from: roaming.path,
        to: local.path
      });
    }
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

  expireThumbnails: function Expiration_expireThumbnails(aURLsToKeep, aCallback) {
    PageThumbsWorker.postMessage({
      type: "expireFilesInDirectory",
      minChunkSize: EXPIRATION_MIN_CHUNK_SIZE,
      path: PageThumbsStorage.getDirectory().path,
      filesToKeep: [PageThumbsStorage.getLeafNameForURL(url) for (url of aURLsToKeep)]
    }, aCallback);
  }
};

/**
 * Interface to a dedicated thread handling I/O
 */
let PageThumbsWorker = {
  /**
   * A (fifo) queue of callbacks registered for execution
   * upon completion of calls to the worker.
   */
  _callbacks: [],

  /**
   * Get the worker, spawning it if necessary.
   * Code of the worker is in companion file PageThumbsWorker.js
   */
  get _worker() {
    delete this._worker;
    this._worker = new ChromeWorker("resource://gre/modules/PageThumbsWorker.js");
    this._worker.addEventListener("message", this);
    return this._worker;
  },

  /**
   * Post a message to the dedicated thread, registering a callback
   * to be executed once the reply has been received.
   *
   * See PageThumbsWorker.js for the format of messages and replies.
   *
   * @param {*} message A JSON message.
   * @param {Function=} callback An optional callback.
   */
  postMessage: function Worker_postMessage(message, callback) {
    this._callbacks.push(callback);
    this._worker.postMessage(message);
  },

  /**
   * Handle a message from the dedicated thread.
   */
  handleEvent: function Worker_handleEvent(aEvent) {
    let callback = this._callbacks.shift();
    if (callback)
      callback(aEvent.data);
  }
};

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
