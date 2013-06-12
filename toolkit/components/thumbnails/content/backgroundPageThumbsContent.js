/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

(function () { // bug 673569 workaround :(

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/PageThumbs.jsm");

const backgroundPageThumbsContent = {

  init: function () {
    // Stop about:blank from loading.  If it finishes loading after a capture
    // request is received, it could trigger the capture's load listener.
    this._webNav.stop(Ci.nsIWebNavigation.STOP_NETWORK);
    addMessageListener("BackgroundPageThumbs:capture",
                       this._onCapture.bind(this));
  },

  get _webNav() {
    return docShell.QueryInterface(Ci.nsIWebNavigation);
  },

  _onCapture: function (msg) {
    if (this._onLoad) {
      this._webNav.stop(Ci.nsIWebNavigation.STOP_NETWORK);
      removeEventListener("load", this._onLoad, true);
    }

    this._onLoad = function onLoad(event) {
      if (event.target != content.document)
        return;
      removeEventListener("load", this._onLoad, true);
      delete this._onLoad;

      let canvas = PageThumbs._createCanvas(content);
      PageThumbs._captureToCanvas(content, canvas);

      let finalURL = this._webNav.currentURI.spec;
      let fileReader = Cc["@mozilla.org/files/filereader;1"].
                       createInstance(Ci.nsIDOMFileReader);
      fileReader.onloadend = function onArrayBufferLoad() {
        sendAsyncMessage("BackgroundPageThumbs:didCapture", {
          id: msg.json.id,
          imageData: fileReader.result,
          finalURL: finalURL,
        });
      };
      canvas.toBlob(blob => fileReader.readAsArrayBuffer(blob));
    }.bind(this);

    addEventListener("load", this._onLoad, true);
    this._webNav.loadURI(msg.json.url, Ci.nsIWebNavigation.LOAD_FLAGS_NONE,
                         null, null, null);
  },
};

backgroundPageThumbsContent.init();

})();
