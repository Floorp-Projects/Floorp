// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/*
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Mobile Browser.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Benjamin Stover <bstover@mozilla.com>
 *   Matt Brubeck <mbrubeck@mozilla.com>
 *   Jaakko Kiviluoto <jaakko.kiviluoto@digia.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;

/**
 * Responsible for zooming in to a given view rectangle
 * @param aBrowserView BrowserView instance
 * @param aZoomRect Optional. Zoom rectangle to be configured
 */
function AnimatedZoom(aBrowserView) {
  return;
  this.bv = aBrowserView;

  this.snapshot = AnimatedZoom.createCanvas();
  if (this.snapshot.pending_render)
    return;

  // Render a snapshot of the viewport contents around the visible rect
  let [w, h] = this.bv.getViewportDimensions();
  let viewportRect = new Rect(0, 0, w, h);
  this.zoomFrom = this.bv.getVisibleRect().translateInside(viewportRect);

  // try to cover twice the size of the current visible rect
  this.snapshotRect = this.bv.getVisibleRect().inflate(2);

  // sanitize the snapshot rectangle to fit inside viewport
  this.snapshotRect.translateInside(viewportRect).restrictTo(viewportRect).expandToIntegers();
  this.snapshotRect.width = Math.min(this.snapshotRect.width, this.snapshot.width);
  this.snapshotRect.height = Math.min(this.snapshotRect.height, this.snapshot.height);

  let snapshotCtx = this.snapshot.MozGetIPCContext("2d")
  snapshotCtx.clearRect(0, 0, this.snapshotRect.width, this.snapshotRect.height);
  this.bv.renderToCanvas(this.snapshot, this.snapshotRect.width, this.snapshotRect.height, this.snapshotRect.clone());

  let remote = !this.bv.getBrowser().contentWindow;
  if (remote) {
    this.snapshot.addEventListener("MozAsyncCanvasRender", this, false);
    this.snapshot.pending_render = true;
  } else {
    this.setupCanvas();
  }
}

AnimatedZoom.prototype.handleEvent = function(aEvent) {
  if (aEvent.type == "MozAsyncCanvasRender") {
    let snapshot = aEvent.originalTarget;
    snapshot.pending_render = false;
    snapshot.removeEventListener("MozAsyncCanvasRender", this, false);

    if (this.snapshot == snapshot) {
      this.setupCanvas();
      this.startTimer();
    }
  }
};

/** Creating a canvas element of width and height. */
AnimatedZoom.createCanvas = function(aRemote) {
  if (!this._canvas) {
    let canvas = document.createElementNS(kXHTMLNamespaceURI, "canvas");
    canvas.width = Math.max(window.innerWidth, window.innerHeight) * 2;
    canvas.height = Math.max(window.innerWidth, window.innerHeight) * 2;
    canvas.mozOpaque = true;
    this._canvas = canvas;
  }
  return this._canvas;
};

AnimatedZoom.prototype.setupCanvas = function() {
  return;

  // stop live rendering during zooming
  this.bv.pauseRendering();

  // hide ui elements to avoid undefined states after zoom
  Browser.hideTitlebar();
  Browser.hideSidebars();

  let clientVis = Browser.browserViewToClientRect(this.bv.getCriticalRect());
  let viewBuffer = Elements.viewBuffer;
  viewBuffer.left = clientVis.left;
  viewBuffer.top = clientVis.top;
  viewBuffer.width = this.zoomFrom.width;
  viewBuffer.height = this.zoomFrom.height;
  viewBuffer.style.display = "block";

  // configure defaults for the canvas' drawing context
  let ctx = viewBuffer.getContext("2d");

  // disable smoothing and use the fastest composition operation
  ctx.mozImageSmoothingEnabled = false;
  ctx.globalCompositeOperation = "copy";

  // set background fill pattern
  let backgroundImage = new Image();
  backgroundImage.src = "chrome://browser/content/checkerboard.png";
  ctx.fillStyle = ctx.createPattern(backgroundImage, "repeat");

  this.canvasReady = true;
};

AnimatedZoom.prototype.startTimer = function() {
  if (this.zoomTo && this.canvasReady && !this.timer) {
    this.updateTo(this.zoomFrom);

    // start animation timer
    this.counter = 0;
    this.inc = 1.0 / Services.prefs.getIntPref("browser.ui.zoom.animationDuration");
    this.timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    this.interval = 1000 / Services.prefs.getIntPref("browser.ui.zoom.animationFps");
    this.timer.initWithCallback(Util.bind(this._callback, this), this.interval, this.timer.TYPE_REPEATING_PRECISE);

    // force first update to be according to FPS even though first callback would take longer
    this.lastTime = 0;
  }
};

/** Updates the zoom to new rect. */
AnimatedZoom.prototype.updateTo = function(nextRect) {
  this.zoomRect = nextRect;

  if (this.snapshot.pending_render)
    return;

  // prepare to draw to the zoom canvas
  let canvasRect = new Rect(0, 0, Elements.viewBuffer.width, Elements.viewBuffer.height);
  let ctx = Elements.viewBuffer.getContext("2d");
  ctx.save();

  // srcRect = area inside this.snapshot to copy from
  let srcRect = nextRect.intersect(this.snapshotRect);
  if (srcRect.isEmpty())
    return;

  // destRect = respective area inside canvas to paint to. The dimensions of
  // destRect within canvas equals those of srcRect within nextRect.
  let s = canvasRect.width / nextRect.width;
  let destRect = srcRect.clone().translate(-nextRect.x, -nextRect.y).scale(s, s);

  // adjust from viewport coordinates to snapshot canvas coordinates
  srcRect.translate(-this.snapshotRect.left, -this.snapshotRect.top);

  // fill background and draw the (possibly scaled) image
  destRect.restrictTo(canvasRect).expandToIntegers();

  ctx.drawImage(this.snapshot,
                Math.floor(srcRect.left), Math.floor(srcRect.top),
                Math.floor(srcRect.width), Math.floor(srcRect.height),
                Math.floor(destRect.left), Math.floor(destRect.top),
                Math.floor(destRect.width), Math.floor(destRect.height));

  // clip the over leftover area and fill it with checkerboard
  let unknowns = canvasRect.subtract(destRect);
  if (unknowns.length > 0) {
    ctx.beginPath();
    unknowns.forEach(function(r) { ctx.rect(r.x, r.y, r.width, r.height); });
    ctx.clip();
    ctx.fill();
  }

  ctx.restore();
};

/** Starts an animated zoom to zoomRect. */
AnimatedZoom.prototype.animateTo = function(aZoomRect) {
  this.zoomTo = aZoomRect;
  this.startTimer();
};

/** Callback for the animation. */
AnimatedZoom.prototype._callback = function() {
  try {
    if (this.counter < 1) {
      // increase animation position according to elapsed time
      let now = Date.now();
      if (this.lastTime == 0)
        this.lastTime = now - this.interval; // fix lastTime if not yet set (first frame)
      this.counter += this.inc * (now - this.lastTime);
      this.lastTime = now;

      // update scaled image to interpolated rectangle
      let rect = this.zoomFrom.blend(this.zoomTo, Math.min(this.counter, 1));
      this.updateTo(rect);
    }
    else {
      // last cycle already rendered final scaled image, now clean up
      this.finish();
    }
  }
  catch(e) {
    Util.dumpLn("Error while zooming. Please report error at:", e);
    this.finish();
    throw e;
  }
};

/** Stop animation, zoom to point, and clean up. */
AnimatedZoom.prototype.finish = function() {
  return;
  try {
    Elements.viewBuffer.style.display = "none";

    // resume live rendering
    this.bv.resumeRendering(true);

    // if we actually zoomed somewhere, clean up the UI to normal
    if (this.zoomRect)
      Browser.setVisibleRect(this.zoomRect);
  }
  finally {
    if (this.timer) {
      this.timer.cancel();
      this.timer = null;
    }
    this.snapshot = null;
    this.zoomTo = null;
  }
};

