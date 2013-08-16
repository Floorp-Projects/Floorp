/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

(function () { // bug 673569 workaround :(

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/PageThumbs.jsm");

const backgroundPageThumbsContent = {

  init: function () {
    // Arrange to prevent (most) popup dialogs for this window - popups done
    // in the parent (eg, auth) aren't prevented, but alert() etc are.
    let dwu = content.
                QueryInterface(Ci.nsIInterfaceRequestor).
                getInterface(Ci.nsIDOMWindowUtils);
    dwu.preventFurtherDialogs();

    // We want a low network priority for this service - lower than b/g tabs
    // etc - so set it to the lowest priority available.
    this._webNav.QueryInterface(Ci.nsIDocumentLoader).
      loadGroup.QueryInterface(Ci.nsISupportsPriority).
      priority = Ci.nsISupportsPriority.PRIORITY_LOWEST;

    docShell.allowMedia = false;
    docShell.allowPlugins = false;

    addMessageListener("BackgroundPageThumbs:capture",
                       this._onCapture.bind(this));
  },

  get _webNav() {
    return docShell.QueryInterface(Ci.nsIWebNavigation);
  },

  _onCapture: function (msg) {
    this._webNav.stop(Ci.nsIWebNavigation.STOP_NETWORK);
    if (this._onLoad)
      removeEventListener("load", this._onLoad, true);

    this._onLoad = function onLoad(event) {
      if (event.target != content.document)
        return;
      let pageLoadTime = new Date() - loadDate;
      removeEventListener("load", this._onLoad, true);
      delete this._onLoad;

      let canvas = PageThumbs._createCanvas(content);
      let captureDate = new Date();
      PageThumbs._captureToCanvas(content, canvas);
      let captureTime = new Date() - captureDate;

      let finalURL = this._webNav.currentURI.spec;
      let fileReader = Cc["@mozilla.org/files/filereader;1"].
                       createInstance(Ci.nsIDOMFileReader);
      fileReader.onloadend = function onArrayBufferLoad() {
        sendAsyncMessage("BackgroundPageThumbs:didCapture", {
          id: msg.json.id,
          imageData: fileReader.result,
          finalURL: finalURL,
          telemetry: {
            CAPTURE_PAGE_LOAD_TIME_MS: pageLoadTime,
            CAPTURE_CANVAS_DRAW_TIME_MS: captureTime,
          },
        });
      };
      canvas.toBlob(blob => fileReader.readAsArrayBuffer(blob));

      // Load about:blank to cause the captured window to be collected...
      // eventually.
      this._webNav.loadURI("about:blank", Ci.nsIWebNavigation.LOAD_FLAGS_NONE,
                           null, null, null);
    }.bind(this);

    addEventListener("load", this._onLoad, true);
    this._webNav.loadURI(msg.json.url, Ci.nsIWebNavigation.LOAD_FLAGS_NONE,
                         null, null, null);
    let loadDate = new Date();
  },
};

backgroundPageThumbsContent.init();

})();
