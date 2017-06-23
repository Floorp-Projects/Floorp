/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["PageThumbs", "PageThumbsStorage"];

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

const PREF_STORAGE_VERSION = "browser.pagethumbnails.storage_version";
const LATEST_STORAGE_VERSION = 3;

const EXPIRATION_MIN_CHUNK_SIZE = 50;
const EXPIRATION_INTERVAL_SECS = 3600;

var gRemoteThumbId = 0;

// If a request for a thumbnail comes in and we find one that is "stale"
// (or don't find one at all) we automatically queue a request to generate a
// new one.
const MAX_THUMBNAIL_AGE_SECS = 172800; // 2 days == 60*60*24*2 == 172800 secs.

/**
 * Name of the directory in the profile that contains the thumbnails.
 */
const THUMBNAIL_DIRECTORY = "thumbnails";

Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://gre/modules/PromiseWorker.jsm", this);
Cu.import("resource://gre/modules/osfile.jsm", this);

Cu.importGlobalProperties(["FileReader"]);

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

XPCOMUtils.defineLazyGetter(this, "gCryptoHash", function() {
  return Cc["@mozilla.org/security/hash;1"].createInstance(Ci.nsICryptoHash);
});

XPCOMUtils.defineLazyGetter(this, "gUnicodeConverter", function() {
  let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                    .createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = "utf8";
  return converter;
});

XPCOMUtils.defineLazyModuleGetter(this, "Deprecated",
  "resource://gre/modules/Deprecated.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AsyncShutdown",
  "resource://gre/modules/AsyncShutdown.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PageThumbUtils",
  "resource://gre/modules/PageThumbUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm");

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
    return "thumbnail";
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
      PlacesUtils.history.addObserver(PageThumbsHistoryObserver, true);

      // Migrate the underlying storage, if needed.
      PageThumbsStorageMigrator.migrate();
      PageThumbsExpiration.init();
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
    return this.scheme + "://" + this.staticHost +
           "/?url=" + encodeURIComponent(aUrl) +
           "&revision=" + PageThumbsStorage.getRevision(aUrl);
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

      let canvas = this.createCanvas(aBrowser.contentWindow);
      this.captureToCanvas(aBrowser, canvas, () => {
        canvas.toBlob(blob => {
          resolve(blob, this.contentType);
        });
      });

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
   * @param aCallback (optional) A callback invoked once the thumbnail has been
   *   rendered to aCanvas.
   * @param aArgs (optional) Additional named parameters:
   *   fullScale - request that a non-downscaled image be returned.
   */
  captureToCanvas(aBrowser, aCanvas, aCallback, aArgs) {
    let telemetryCaptureTime = new Date();
    let args = {
      fullScale: aArgs ? aArgs.fullScale : false
    };
    this._captureToCanvas(aBrowser, aCanvas, args, (aCanvas) => {
      Services.telemetry
              .getHistogramById("FX_THUMBNAILS_CAPTURE_TIME_MS")
              .add(new Date() - telemetryCaptureTime);
      if (aCallback) {
        aCallback(aCanvas);
      }
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
   * @param aCallback(aResult) A callback invoked once security checks have
   *   completed. aResult is a boolean indicating the combined result of the
   *   security checks performed.
   */
  shouldStoreThumbnail(aBrowser, aCallback) {
    // Don't capture in private browsing mode.
    if (PrivateBrowsingUtils.isBrowserPrivate(aBrowser)) {
      aCallback(false);
      return;
    }
    if (aBrowser.isRemoteBrowser) {
      let mm = aBrowser.messageManager;
      let resultFunc = function(aMsg) {
        mm.removeMessageListener("Browser:Thumbnail:CheckState:Response", resultFunc);
        aCallback(aMsg.data.result);
      }
      mm.addMessageListener("Browser:Thumbnail:CheckState:Response", resultFunc);
      try {
        mm.sendAsyncMessage("Browser:Thumbnail:CheckState");
      } catch (ex) {
        Cu.reportError(ex);
        // If the message manager is not able send our message, taking a content
        // screenshot is also not going to work: return false.
        resultFunc({ data: { result: false } });
      }
    } else {
      aCallback(PageThumbUtils.shouldStoreContentThumbnail(aBrowser.contentDocument,
                                                           aBrowser.docShell));
    }
  },

  // The background thumbnail service captures to canvas but doesn't want to
  // participate in this service's telemetry, which is why this method exists.
  _captureToCanvas(aBrowser, aCanvas, aArgs, aCallback) {
    if (aBrowser.isRemoteBrowser) {
      (async () => {
        let data =
          await this._captureRemoteThumbnail(aBrowser, aCanvas.width,
                                             aCanvas.height, aArgs);
        let canvas = data.thumbnail;
        let ctx = canvas.getContext("2d");
        let imgData = ctx.getImageData(0, 0, canvas.width, canvas.height);
        aCanvas.width = canvas.width;
        aCanvas.height = canvas.height;
        aCanvas.getContext("2d").putImageData(imgData, 0, 0);
        if (aCallback) {
          aCallback(aCanvas);
        }
      })();
      return;
    }
    // The content is a local page, grab a thumbnail sync.
    PageThumbUtils.createSnapshotThumbnail(aBrowser.contentWindow,
                                           aCanvas,
                                           aArgs);

    if (aCallback) {
      aCallback(aCanvas);
    }
  },

  /**
   * Asynchrnously render an appropriately scaled thumbnail to canvas.
   *
   * @param aBrowser The browser to capture a thumbnail from.
   * @param aWidth The desired canvas width.
   * @param aHeight The desired canvas height.
   * @param aArgs (optional) Additional named parameters:
   *   fullScale - request that a non-downscaled image be returned.
   * @return a promise
   */
  _captureRemoteThumbnail(aBrowser, aWidth, aHeight, aArgs) {
    return new Promise(resolve => {

      // The index we send with the request so we can identify the
      // correct response.
      let index = gRemoteThumbId++;

      // Thumbnail request response handler
      let mm = aBrowser.messageManager;

      // Browser:Thumbnail:Response handler
      let thumbFunc = function(aMsg) {
        // Ignore events unrelated to our request
        if (aMsg.data.id != index) {
          return;
        }

        mm.removeMessageListener("Browser:Thumbnail:Response", thumbFunc);
        let imageBlob = aMsg.data.thumbnail;
        let doc = aBrowser.parentElement.ownerDocument;
        let reader = new FileReader();
        reader.addEventListener("loadend", function() {
          let image = doc.createElementNS(PageThumbUtils.HTML_NAMESPACE, "img");
          image.onload = function() {
            let thumbnail = doc.createElementNS(PageThumbUtils.HTML_NAMESPACE, "canvas");
            thumbnail.width = image.naturalWidth;
            thumbnail.height = image.naturalHeight;
            let ctx = thumbnail.getContext("2d");
            ctx.drawImage(image, 0, 0);
            resolve({
              thumbnail
            });
          }
          image.src = reader.result;
        });
        // xxx wish there was a way to skip this encoding step
        reader.readAsDataURL(imageBlob);
      }

      // Send a thumbnail request
      mm.addMessageListener("Browser:Thumbnail:Response", thumbFunc);
      mm.sendAsyncMessage("Browser:Thumbnail:Request", {
        canvasWidth: aWidth,
        canvasHeight: aHeight,
        background: PageThumbUtils.THUMBNAIL_BG_COLOR,
        id: index,
        additionalArgs: aArgs
      });

    });
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
    let originalURL;
    let channelError = false;

    (async () => {
      if (!aBrowser.isRemoteBrowser) {
        let channel = aBrowser.docShell.currentDocumentChannel;
        originalURL = channel.originalURI.spec;
        // see if this was an error response.
        channelError = PageThumbUtils.isChannelErrorResponse(channel);
      } else {
        let resp = await new Promise(resolve => {
          let mm = aBrowser.messageManager;
          let respName = "Browser:Thumbnail:GetOriginalURL:Response";
          mm.addMessageListener(respName, function onResp(msg) {
            mm.removeMessageListener(respName, onResp);
            resolve(msg.data);
          });
          mm.sendAsyncMessage("Browser:Thumbnail:GetOriginalURL");
        });
        originalURL = resp.originalURL || url;
        channelError = resp.channelError;
      }

      let isSuccess = true;
      try {
        let blob = await this.captureToBlob(aBrowser);
        let buffer = await TaskUtils.readBlob(blob);
        await this._store(originalURL, url, buffer, channelError);
      } catch (ex) {
        Components.utils.reportError("Exception thrown during thumbnail capture: '" + ex + "'");
        isSuccess = false;
      }
      if (aCallback) {
        aCallback(isSuccess);
      }
    })();
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
    return (async function() {
      let telemetryStoreTime = new Date();
      await PageThumbsStorage.writeData(aFinalURL, aData, aNoOverwrite);
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
        await PageThumbsStorage.copy(aFinalURL, aOriginalURL, aNoOverwrite);
        Services.obs.notifyObservers(null, "page-thumbnail:create", aOriginalURL);
      }
    })();
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
      return !Services.prefs.getBoolPref("browser.pagethumbnails.capturing_disabled");
    } catch (e) {
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
      [this.path, {ignoreExisting: true}]).catch(function onError(aReason) {
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

  _revisionTable: {},

  // Generate an arbitrary revision tag, i.e. one that can't be used to
  // infer URL frecency.
  _updateRevision(aURL) {
    // Initialize with a random value and increment on each update. Wrap around
    // modulo _revisionRange, so that even small values carry no meaning.
    let rev = this._revisionTable[aURL];
    if (rev == null)
      rev = Math.floor(Math.random() * this._revisionRange);
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
        flush: false /* thumbnails do not require the level of guarantee provided by flush*/
      }];
    return PageThumbsWorker.post("writeAtomic", msg,
      msg /* we don't want that message garbage-collected,
           as OS.Shared.Type.void_t.in_ptr.toMsg uses C-level
           memory tricks to enforce zero-copy*/).
      then(() => this._updateRevision(aURL), this._eatNoOverwriteError(aNoOverwrite));
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
      then(() => this._updateRevision(aTargetURL), this._eatNoOverwriteError(aNoOverwrite));
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
      blocker);

    // Start the work only now that `profileBeforeChange` has had
    // a chance to throw an error.

    let promise = PageThumbsWorker.post("wipe", [this.path]);
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
    local = OS.Constants.Path.localProfileDir,
    roaming = OS.Constants.Path.profileDir) {
    PageThumbsWorker.post(
      "moveOrDeleteAllThumbnails",
      [OS.Path.join(roaming, THUMBNAIL_DIRECTORY),
       OS.Path.join(local, THUMBNAIL_DIRECTORY)]
    );
  }
};

var PageThumbsExpiration = {
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
    let keep = aURLsToKeep.map(url => PageThumbsStorage.getLeafNameForURL(url));
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
var PageThumbsWorker = new BasePromiseWorker("resource://gre/modules/PageThumbsWorker.js");
// As the PageThumbsWorker performs I/O, we can receive instances of
// OS.File.Error, so we need to install a decoder.
PageThumbsWorker.ExceptionHandlers["OS.File.Error"] = OS.File.Error.fromMsg;

var PageThumbsHistoryObserver = {
  onDeleteURI(aURI, aGUID) {
    PageThumbsStorage.remove(aURI.spec);
  },

  onClearHistory() {
    PageThumbsStorage.wipe();
  },

  onTitleChanged() {},
  onBeginUpdateBatch() {},
  onEndUpdateBatch() {},
  onVisit() {},
  onPageChanged() {},
  onDeleteVisits() {},

  QueryInterface: XPCOMUtils.generateQI([Ci.nsINavHistoryObserver,
                                         Ci.nsISupportsWeakReference])
};
