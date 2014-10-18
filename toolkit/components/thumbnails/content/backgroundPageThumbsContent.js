/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

(function () { // bug 673569 workaround :(

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.importGlobalProperties(['Blob']);

Cu.import("resource://gre/modules/PageThumbs.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const STATE_LOADING = 1;
const STATE_CAPTURING = 2;
const STATE_CANCELED = 3;

const backgroundPageThumbsContent = {

  init: function () {
    Services.obs.addObserver(this, "document-element-inserted", true);

    // We want a low network priority for this service - lower than b/g tabs
    // etc - so set it to the lowest priority available.
    this._webNav.QueryInterface(Ci.nsIDocumentLoader).
      loadGroup.QueryInterface(Ci.nsISupportsPriority).
      priority = Ci.nsISupportsPriority.PRIORITY_LOWEST;

    docShell.allowMedia = false;
    docShell.allowPlugins = false;
    docShell.allowContentRetargeting = false;
    let defaultFlags = Ci.nsIRequest.LOAD_ANONYMOUS |
                       Ci.nsIRequest.LOAD_BYPASS_CACHE |
                       Ci.nsIRequest.INHIBIT_CACHING |
                       Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_HISTORY;
    docShell.defaultLoadFlags = defaultFlags;

    addMessageListener("BackgroundPageThumbs:capture",
                       this._onCapture.bind(this));
    docShell.
      QueryInterface(Ci.nsIInterfaceRequestor).
      getInterface(Ci.nsIWebProgress).
      addProgressListener(this, Ci.nsIWebProgress.NOTIFY_STATE_WINDOW);
  },

  observe: function (subj, topic, data) {
    // Arrange to prevent (most) popup dialogs for this window - popups done
    // in the parent (eg, auth) aren't prevented, but alert() etc are.
    // disableDialogs only works on the current inner window, so it has
    // to be called every page load, but before scripts run.
    if (subj == content.document) {
      content.
        QueryInterface(Ci.nsIInterfaceRequestor).
        getInterface(Ci.nsIDOMWindowUtils).
        disableDialogs();
    }
  },

  get _webNav() {
    return docShell.QueryInterface(Ci.nsIWebNavigation);
  },

  _onCapture: function (msg) {
    this._nextCapture = {
      id: msg.data.id,
      url: msg.data.url,
    };
    if (this._currentCapture) {
      if (this._state == STATE_LOADING) {
        // Cancel the current capture.
        this._state = STATE_CANCELED;
        this._loadAboutBlank();
      }
      // Let the current capture finish capturing, or if it was just canceled,
      // wait for onStateChange due to the about:blank load.
      return;
    }
    this._startNextCapture();
  },

  _startNextCapture: function () {
    if (!this._nextCapture)
      return;
    this._currentCapture = this._nextCapture;
    delete this._nextCapture;
    this._state = STATE_LOADING;
    this._currentCapture.pageLoadStartDate = new Date();

    try {
      this._webNav.loadURI(this._currentCapture.url,
                           Ci.nsIWebNavigation.LOAD_FLAGS_STOP_CONTENT,
                           null, null, null);
    } catch (e) {
      this._failCurrentCapture("BAD_URI");
      delete this._currentCapture;
      this._startNextCapture();
    }
  },

  onStateChange: function (webProgress, req, flags, status) {
    if (webProgress.isTopLevel &&
        (flags & Ci.nsIWebProgressListener.STATE_STOP) &&
        this._currentCapture) {
      if (req.name == "about:blank") {
        if (this._state == STATE_CAPTURING) {
          // about:blank has loaded, ending the current capture.
          this._finishCurrentCapture();
          delete this._currentCapture;
          this._startNextCapture();
        }
        else if (this._state == STATE_CANCELED) {
          // A capture request was received while the current capture's page
          // was still loading.
          delete this._currentCapture;
          this._startNextCapture();
        }
      }
      else if (this._state == STATE_LOADING) {
        // The requested page has loaded.  Capture it.
        this._state = STATE_CAPTURING;
        this._captureCurrentPage();
      }
    }
  },

  _captureCurrentPage: function () {
    let capture = this._currentCapture;
    capture.finalURL = this._webNav.currentURI.spec;
    capture.pageLoadTime = new Date() - capture.pageLoadStartDate;

    let canvas = PageThumbs.createCanvas(content);
    let canvasDrawDate = new Date();
    PageThumbs._captureToCanvas(content, canvas);
    capture.canvasDrawTime = new Date() - canvasDrawDate;

    canvas.toBlob(blob => {
      capture.imageBlob = new Blob([blob]);
      // Load about:blank to finish the capture and wait for onStateChange.
      this._loadAboutBlank();
    });
  },

  _finishCurrentCapture: function () {
    let capture = this._currentCapture;
    let fileReader = Cc["@mozilla.org/files/filereader;1"].
                     createInstance(Ci.nsIDOMFileReader);
    fileReader.onloadend = () => {
      sendAsyncMessage("BackgroundPageThumbs:didCapture", {
        id: capture.id,
        imageData: fileReader.result,
        finalURL: capture.finalURL,
        telemetry: {
          CAPTURE_PAGE_LOAD_TIME_MS: capture.pageLoadTime,
          CAPTURE_CANVAS_DRAW_TIME_MS: capture.canvasDrawTime,
        },
      });
    };
    fileReader.readAsArrayBuffer(capture.imageBlob);
  },

  _failCurrentCapture: function (reason) {
    let capture = this._currentCapture;
    sendAsyncMessage("BackgroundPageThumbs:didCapture", {
      id: capture.id,
      failReason: reason,
    });
  },

  // We load about:blank to finish all captures, even canceled captures.  Two
  // reasons: GC the captured page, and ensure it can't possibly load any more
  // resources.
  _loadAboutBlank: function _loadAboutBlank() {
    this._webNav.loadURI("about:blank",
                         Ci.nsIWebNavigation.LOAD_FLAGS_STOP_CONTENT,
                         null, null, null);
  },

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIWebProgressListener,
    Ci.nsISupportsWeakReference,
    Ci.nsIObserver,
  ]),
};

backgroundPageThumbsContent.init();

})();
