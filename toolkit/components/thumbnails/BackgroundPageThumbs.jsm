/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const EXPORTED_SYMBOLS = [
  "BackgroundPageThumbs",
];

const DEFAULT_CAPTURE_TIMEOUT = 30000; // ms
const DESTROY_BROWSER_TIMEOUT = 60000; // ms
const FRAME_SCRIPT_URL = "chrome://global/content/backgroundPageThumbsContent.js";

const TELEMETRY_HISTOGRAM_ID_PREFIX = "FX_THUMBNAILS_BG_";

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const HTML_NS = "http://www.w3.org/1999/xhtml";

const ABOUT_NEWTAB_SEGREGATION_PREF = "privacy.usercontext.about_newtab_segregation.enabled";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://gre/modules/PageThumbs.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");

// possible FX_THUMBNAILS_BG_CAPTURE_DONE_REASON_2 telemetry values
const TEL_CAPTURE_DONE_OK = 0;
const TEL_CAPTURE_DONE_TIMEOUT = 1;
// 2 and 3 were used when we had special handling for private-browsing.
const TEL_CAPTURE_DONE_CRASHED = 4;
const TEL_CAPTURE_DONE_BAD_URI = 5;

// These are looked up on the global as properties below.
XPCOMUtils.defineConstant(this, "TEL_CAPTURE_DONE_OK", TEL_CAPTURE_DONE_OK);
XPCOMUtils.defineConstant(this, "TEL_CAPTURE_DONE_TIMEOUT", TEL_CAPTURE_DONE_TIMEOUT);
XPCOMUtils.defineConstant(this, "TEL_CAPTURE_DONE_CRASHED", TEL_CAPTURE_DONE_CRASHED);
XPCOMUtils.defineConstant(this, "TEL_CAPTURE_DONE_BAD_URI", TEL_CAPTURE_DONE_BAD_URI);

XPCOMUtils.defineLazyModuleGetter(this, "ContextualIdentityService",
                                  "resource://gre/modules/ContextualIdentityService.jsm");
const global = this;

const BackgroundPageThumbs = {

  /**
   * Asynchronously captures a thumbnail of the given URL.
   *
   * The page is loaded anonymously, and plug-ins are disabled.
   *
   * @param url      The URL to capture.
   * @param options  An optional object that configures the capture.  Its
   *                 properties are the following, and all are optional:
   * @opt onDone     A function that will be asynchronously called when the
   *                 capture is complete or times out.  It's called as
   *                   onDone(url),
   *                 where `url` is the captured URL.
   * @opt timeout    The capture will time out after this many milliseconds have
   *                 elapsed after the capture has progressed to the head of
   *                 the queue and started.  Defaults to 30000 (30 seconds).
   */
  capture(url, options = {}) {
    if (!PageThumbs._prefEnabled()) {
      if (options.onDone)
        schedule(() => options.onDone(url));
      return;
    }
    this._captureQueue = this._captureQueue || [];
    this._capturesByURL = this._capturesByURL || new Map();

    tel("QUEUE_SIZE_ON_CAPTURE", this._captureQueue.length);

    // We want to avoid duplicate captures for the same URL.  If there is an
    // existing one, we just add the callback to that one and we are done.
    let existing = this._capturesByURL.get(url);
    if (existing) {
      if (options.onDone)
        existing.doneCallbacks.push(options.onDone);
      // The queue is already being processed, so nothing else to do...
      return;
    }
    let cap = new Capture(url, this._onCaptureOrTimeout.bind(this), options);
    this._captureQueue.push(cap);
    this._capturesByURL.set(url, cap);
    this._processCaptureQueue();
  },

  /**
   * Asynchronously captures a thumbnail of the given URL if one does not
   * already exist.  Otherwise does nothing.
   *
   * @param url      The URL to capture.
   * @param options  An optional object that configures the capture.  See
   *                 capture() for description.
   * @return {Promise} A Promise that resolves when this task completes
   */
  captureIfMissing: Task.async(function* (url, options = {}) {
    // The fileExistsForURL call is an optimization, potentially but unlikely
    // incorrect, and no big deal when it is.  After the capture is done, we
    // atomically test whether the file exists before writing it.
    let exists = yield PageThumbsStorage.fileExistsForURL(url);
    if (exists) {
      if (options.onDone) {
        options.onDone(url);
      }
      return url;
    }
    let thumbPromise = new Promise((resolve, reject) => {
      function observe(subject, topic, data) { // jshint ignore:line
        if (data === url) {
          switch (topic) {
            case "page-thumbnail:create":
              resolve();
              break;
            case "page-thumbnail:error":
              reject(new Error("page-thumbnail:error"));
              break;
          }
          Services.obs.removeObserver(observe, "page-thumbnail:create");
          Services.obs.removeObserver(observe, "page-thumbnail:error");
        }
      }
      Services.obs.addObserver(observe, "page-thumbnail:create");
      Services.obs.addObserver(observe, "page-thumbnail:error");
    });
    try {
      this.capture(url, options);
      yield thumbPromise;
    } catch (err) {
      if (options.onDone) {
        options.onDone(url);
      }
      throw err;
    }
    return url;
  }),

  /**
   * Tell the service that the thumbnail browser should be recreated at next
   * call of _ensureBrowser().
   */
  renewThumbnailBrowser() {
    this._renewThumbBrowser = true;
  },

  /**
   * Ensures that initialization of the thumbnail browser's parent window has
   * begun.
   *
   * @return  True if the parent window is completely initialized and can be
   *          used, and false if initialization has started but not completed.
   */
  _ensureParentWindowReady() {
    if (this._parentWin)
      // Already fully initialized.
      return true;
    if (this._startedParentWinInit)
      // Already started initializing.
      return false;

    this._startedParentWinInit = true;

    // Create a windowless browser and load our hosting
    // (privileged) document in it.
    let wlBrowser = Services.appShell.createWindowlessBrowser(true);
    wlBrowser.QueryInterface(Ci.nsIInterfaceRequestor);
    let webProgress = wlBrowser.getInterface(Ci.nsIWebProgress);
    this._listener = {
      QueryInterface: XPCOMUtils.generateQI([
        Ci.nsIWebProgressListener, Ci.nsIWebProgressListener2,
        Ci.nsISupportsWeakReference]),
    };
    this._listener.onStateChange = (wbp, request, stateFlags, status) => {
      if (!request) {
        return;
      }
      if (stateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
          stateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK) {
        webProgress.removeProgressListener(this._listener);
        delete this._listener;
        // Get the window reference via the document.
        this._parentWin = wlBrowser.document.defaultView;
        this._processCaptureQueue();
      }
    };
    webProgress.addProgressListener(this._listener, Ci.nsIWebProgress.NOTIFY_STATE_ALL);
    wlBrowser.loadURI("chrome://global/content/backgroundPageThumbs.xhtml", 0, null, null, null);
    this._windowlessContainer = wlBrowser;

    return false;
  },

  /**
   * Destroys the service.  Queued and pending captures will never complete, and
   * their consumer callbacks will never be called.
   */
  _destroy() {
    if (this._captureQueue)
      this._captureQueue.forEach(cap => cap.destroy());
    this._destroyBrowser();
    if (this._windowlessContainer)
      this._windowlessContainer.close();
    delete this._captureQueue;
    delete this._windowlessContainer;
    delete this._startedParentWinInit;
    delete this._parentWin;
    delete this._listener;
  },

  /**
   * Creates the thumbnail browser if it doesn't already exist.
   */
  _ensureBrowser() {
    if (this._thumbBrowser && !this._renewThumbBrowser)
      return;

    this._destroyBrowser();
    this._renewThumbBrowser = false;

    let browser = this._parentWin.document.createElementNS(XUL_NS, "browser");
    browser.setAttribute("type", "content");
    browser.setAttribute("remote", "true");
    browser.setAttribute("disableglobalhistory", "true");

    if (Services.prefs.getBoolPref(ABOUT_NEWTAB_SEGREGATION_PREF)) {
      // Use the private container for thumbnails.
      let privateIdentity =
        ContextualIdentityService.getPrivateIdentity("userContextIdInternal.thumbnail");
      browser.setAttribute("usercontextid", privateIdentity.userContextId);
    }

    // Size the browser.  Make its aspect ratio the same as the canvases' that
    // the thumbnails are drawn into; the canvases' aspect ratio is the same as
    // the screen's, so use that.  Aim for a size in the ballpark of 1024x768.
    let [swidth, sheight] = [{}, {}];
    Cc["@mozilla.org/gfx/screenmanager;1"].
      getService(Ci.nsIScreenManager).
      primaryScreen.
      GetRectDisplayPix({}, {}, swidth, sheight);
    let bwidth = Math.min(1024, swidth.value);
    // Setting the width and height attributes doesn't work -- the resulting
    // thumbnails are blank and transparent -- but setting the style does.
    browser.style.width = bwidth + "px";
    browser.style.height = (bwidth * sheight.value / swidth.value) + "px";

    this._parentWin.document.documentElement.appendChild(browser);

    // an event that is sent if the remote process crashes - no need to remove
    // it as we want it to be there as long as the browser itself lives.
    browser.addEventListener("oop-browser-crashed", () => {
      Cu.reportError("BackgroundThumbnails remote process crashed - recovering");
      this._destroyBrowser();
      let curCapture = this._captureQueue.length ? this._captureQueue[0] : null;
      // we could retry the pending capture, but it's possible the crash
      // was due directly to it, so trying again might just crash again.
      // We could keep a flag to indicate if it previously crashed, but
      // "resetting" the capture requires more work - so for now, we just
      // discard it.
      if (curCapture && curCapture.pending) {
        // Continue queue processing by calling curCapture._done().  Do it after
        // this crashed listener returns, though.  A new browser will be created
        // immediately (on the same stack as the _done call stack) if there are
        // any more queued-up captures, and that seems to mess up the new
        // browser's message manager if it happens on the same stack as the
        // listener.  Trying to send a message to the manager in that case
        // throws NS_ERROR_NOT_INITIALIZED.
        Services.tm.dispatchToMainThread(() => {
          curCapture._done(null, TEL_CAPTURE_DONE_CRASHED);
        });
      }
      // else: we must have been idle and not currently doing a capture (eg,
      // maybe a GC or similar crashed) - so there's no need to attempt a
      // queue restart - the next capture request will set everything up.
    });

    browser.messageManager.loadFrameScript(FRAME_SCRIPT_URL, false);
    this._thumbBrowser = browser;
  },

  _destroyBrowser() {
    if (!this._thumbBrowser)
      return;
    this._thumbBrowser.remove();
    delete this._thumbBrowser;
  },

  /**
   * Starts the next capture if the queue is not empty and the service is fully
   * initialized.
   */
  _processCaptureQueue() {
    if (!this._captureQueue.length ||
        this._captureQueue[0].pending ||
        !this._ensureParentWindowReady())
      return;

    // Ready to start the first capture in the queue.
    this._ensureBrowser();
    this._captureQueue[0].start(this._thumbBrowser.messageManager);
    if (this._destroyBrowserTimer) {
      this._destroyBrowserTimer.cancel();
      delete this._destroyBrowserTimer;
    }
  },

  /**
   * Called when the current capture completes or fails (eg, times out, remote
   * process crashes.)
   */
  _onCaptureOrTimeout(capture) {
    // Since timeouts start as an item is being processed, only the first
    // item in the queue can be passed to this method.
    if (capture !== this._captureQueue[0])
      throw new Error("The capture should be at the head of the queue.");
    this._captureQueue.shift();
    this._capturesByURL.delete(capture.url);
    if (capture.doneReason != TEL_CAPTURE_DONE_OK) {
      Services.obs.notifyObservers(null, "page-thumbnail:error", capture.url);
    }

    // Start the destroy-browser timer *before* processing the capture queue.
    let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    timer.initWithCallback(this._destroyBrowser.bind(this),
                           this._destroyBrowserTimeout,
                           Ci.nsITimer.TYPE_ONE_SHOT);
    this._destroyBrowserTimer = timer;

    this._processCaptureQueue();
  },

  _destroyBrowserTimeout: DESTROY_BROWSER_TIMEOUT,
};

Services.prefs.addObserver(ABOUT_NEWTAB_SEGREGATION_PREF,
  function(aSubject, aTopic, aData) {
    if (aTopic == "nsPref:changed" && aData == ABOUT_NEWTAB_SEGREGATION_PREF) {
      BackgroundPageThumbs.renewThumbnailBrowser();
    }
  });

Object.defineProperty(this, "BackgroundPageThumbs", {
  value: BackgroundPageThumbs,
  enumerable: true,
  writable: false
});

/**
 * Represents a single capture request in the capture queue.
 *
 * @param url              The URL to capture.
 * @param captureCallback  A function you want called when the capture
 *                         completes.
 * @param options          The capture options.
 */
function Capture(url, captureCallback, options) {
  this.url = url;
  this.captureCallback = captureCallback;
  this.options = options;
  this.id = Capture.nextID++;
  this.creationDate = new Date();
  this.doneCallbacks = [];
  this.doneReason;
  if (options.onDone)
    this.doneCallbacks.push(options.onDone);
}

Capture.prototype = {

  get pending() {
    return !!this._msgMan;
  },

  /**
   * Sends a message to the content script to start the capture.
   *
   * @param messageManager  The nsIMessageSender of the thumbnail browser.
   */
  start(messageManager) {
    this.startDate = new Date();
    tel("CAPTURE_QUEUE_TIME_MS", this.startDate - this.creationDate);

    // timeout timer
    let timeout = typeof(this.options.timeout) == "number" ?
                  this.options.timeout :
                  DEFAULT_CAPTURE_TIMEOUT;
    this._timeoutTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    this._timeoutTimer.initWithCallback(this, timeout,
                                        Ci.nsITimer.TYPE_ONE_SHOT);

    // didCapture registration
    this._msgMan = messageManager;
    this._msgMan.sendAsyncMessage("BackgroundPageThumbs:capture",
                                  { id: this.id, url: this.url });
    this._msgMan.addMessageListener("BackgroundPageThumbs:didCapture", this);
  },

  /**
   * The only intended external use of this method is by the service when it's
   * uninitializing and doing things like destroying the thumbnail browser.  In
   * that case the consumer's completion callback will never be called.
   */
  destroy() {
    // This method may be called for captures that haven't started yet, so
    // guard against not yet having _timeoutTimer, _msgMan etc properties...
    if (this._timeoutTimer) {
      this._timeoutTimer.cancel();
      delete this._timeoutTimer;
    }
    if (this._msgMan) {
      this._msgMan.removeMessageListener("BackgroundPageThumbs:didCapture",
                                         this);
      delete this._msgMan;
    }
    delete this.captureCallback;
    delete this.doneCallbacks;
    delete this.options;
  },

  // Called when the didCapture message is received.
  receiveMessage(msg) {
    if (msg.data.imageData)
      tel("CAPTURE_SERVICE_TIME_MS", new Date() - this.startDate);

    // A different timed-out capture may have finally successfully completed, so
    // discard messages that aren't meant for this capture.
    if (msg.data.id != this.id)
      return;

    if (msg.data.failReason) {
      let reason = global["TEL_CAPTURE_DONE_" + msg.data.failReason];
      this._done(null, reason);
      return;
    }

    this._done(msg.data, TEL_CAPTURE_DONE_OK);
  },

  // Called when the timeout timer fires.
  notify() {
    this._done(null, TEL_CAPTURE_DONE_TIMEOUT);
  },

  _done(data, reason) {
    // Note that _done will be called only once, by either receiveMessage or
    // notify, since it calls destroy here, which cancels the timeout timer and
    // removes the didCapture message listener.
    let { captureCallback, doneCallbacks, options } = this;
    this.destroy();
    this.doneReason = reason;

    if (typeof(reason) != "number") {
      throw new Error("A done reason must be given.");
    }
    tel("CAPTURE_DONE_REASON_2", reason);
    if (data && data.telemetry) {
      // Telemetry is currently disabled in the content process (bug 680508).
      for (let id in data.telemetry) {
        tel(id, data.telemetry[id]);
      }
    }

    let done = () => {
      captureCallback(this);
      for (let callback of doneCallbacks) {
        try {
          callback.call(options, this.url);
        } catch (err) {
          Cu.reportError(err);
        }
      }

      if (Services.prefs.getBoolPref(ABOUT_NEWTAB_SEGREGATION_PREF)) {
        // Clear the data in the private container for thumbnails.
        let privateIdentity =
          ContextualIdentityService.getPrivateIdentity("userContextIdInternal.thumbnail");
        Services.obs.notifyObservers(null, "clear-origin-attributes-data",
          JSON.stringify({ userContextId: privateIdentity.userContextId }));
      }
    };

    if (!data) {
      done();
      return;
    }

    PageThumbs._store(this.url, data.finalURL, data.imageData, true)
              .then(done, done);
  },
};

Capture.nextID = 0;

/**
 * Adds a value to one of this module's telemetry histograms.
 *
 * @param histogramID  This is prefixed with this module's ID.
 * @param value        The value to add.
 */
function tel(histogramID, value) {
  let id = TELEMETRY_HISTOGRAM_ID_PREFIX + histogramID;
  Services.telemetry.getHistogramById(id).add(value);
}

function schedule(callback) {
  Services.tm.dispatchToMainThread(callback);
}
