/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const XHTML_NS = "http://www.w3.org/1999/xhtml";
const MAXIMUM_PIXELS = Math.pow(144, 2);

function ColorAnalyzer() {
  // a queue of callbacks for each job we give to the worker
  this.callbacks = [];

  this.hiddenWindowDoc = Cc["@mozilla.org/appshell/appShellService;1"].
                         getService(Ci.nsIAppShellService).
                         hiddenDOMWindow.document;

  this.worker = new ChromeWorker("resource://gre/modules/ColorAnalyzer_worker.js");
  this.worker.onmessage = this.onWorkerMessage.bind(this);
  this.worker.onerror = this.onWorkerError.bind(this);
}

ColorAnalyzer.prototype = {
  findRepresentativeColor: function ColorAnalyzer_frc(imageURI, callback) {
    function cleanup() {
      image.removeEventListener("load", loadListener);
      image.removeEventListener("error", errorListener);
    }
    let image = this.hiddenWindowDoc.createElementNS(XHTML_NS, "img");
    let loadListener = this.onImageLoad.bind(this, image, callback, cleanup);
    let errorListener = this.onImageError.bind(this, image, callback, cleanup);
    image.addEventListener("load", loadListener);
    image.addEventListener("error", errorListener);
    image.src = imageURI.spec;
  },

  onImageLoad: function ColorAnalyzer_onImageLoad(image, callback, cleanup) {
    if (image.naturalWidth * image.naturalHeight > MAXIMUM_PIXELS) {
      // this will probably take too long to process - fail
      callback.onComplete(false);
    } else {
      let canvas = this.hiddenWindowDoc.createElementNS(XHTML_NS, "canvas");
      canvas.width = image.naturalWidth;
      canvas.height = image.naturalHeight;
      let ctx = canvas.getContext("2d");
      ctx.drawImage(image, 0, 0);
      this.startJob(ctx.getImageData(0, 0, canvas.width, canvas.height),
                    callback);
    }
    cleanup();
  },

  onImageError: function ColorAnalyzer_onImageError(image, callback, cleanup) {
    Cu.reportError("ColorAnalyzer: image at " + image.src + " didn't load");
    callback.onComplete(false);
    cleanup();
  },

  startJob: function ColorAnalyzer_startJob(imageData, callback) {
    this.callbacks.push(callback);
    this.worker.postMessage({ imageData, maxColors: 1 });
  },

  onWorkerMessage: function ColorAnalyzer_onWorkerMessage(event) {
    // colors can be empty on failure
    if (event.data.colors.length < 1) {
      this.callbacks.shift().onComplete(false);
    } else {
      this.callbacks.shift().onComplete(true, event.data.colors[0]);
    }
  },

  onWorkerError: function ColorAnalyzer_onWorkerError(error) {
    // this shouldn't happen, but just in case
    error.preventDefault();
    Cu.reportError("ColorAnalyzer worker: " + error.message);
    this.callbacks.shift().onComplete(false);
  },

  classID: Components.ID("{d056186c-28a0-494e-aacc-9e433772b143}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.mozIColorAnalyzer])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([ColorAnalyzer]);
