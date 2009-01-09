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
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <stuart@mozilla.com>
 *   Brad Lassey <blassey@mozilla.com>
 *   Mark Finkle <mfinkle@mozilla.com>
 *   Gavin Sharp <gavin.sharp@gmail.com>
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

function CanvasBrowser(canvas) {
  this._canvas = canvas;
}

CanvasBrowser.prototype = {
  _canvas: null,
  _zoomLevel: 1,
  _browser: null,
  _pageX: 0,
  _pageY: 0,
  _screenX: 0,
  _screenY: 0,

  get canvasDimensions() {
    if (!this._canvasRect) {
      let canvasRect = this._canvas.getBoundingClientRect();
      this._canvasRect = {
        width: canvasRect.width,
        height: canvasRect.height
      }
    }
    return [this._canvasRect.width, this._canvasRect.height];
  },

  get _effectiveCanvasDimensions() {
    return this.canvasDimensions.map(this._screenToPage, this);
  },

  get _effectiveViewportDimensions() {
    // XXX
  },

  setCurrentBrowser: function(browser) {
    let currentBrowser = this._browser;
    if (currentBrowser) {
      // stop monitor paint events for this browser
      currentBrowser.removeEventListener("MozAfterPaint", this._paintHandler, false);
      currentBrowser.setAttribute("type", "content");
    }

    browser.setAttribute("type", "content-primary");

    // start monitoring paint events for this browser
    var self = this;
    this._paintHandler = function(ev) { self._handleMozAfterPaint(ev); }

    browser.addEventListener("MozAfterPaint", this._paintHandler, false);

    this._browser = browser;

    self.zoomToPage();
  },

  startLoading: function() {
    // Clear the whole canvas
    // we clear the whole canvas because the browser's width or height
    // could be less than the area we end up actually drawing.

    var ctx = this._canvas.getContext("2d");
    ctx.fillStyle = "rgb(255,255,255)";
    ctx.fillRect(0, 0, this._canvas.width, this._canvas.height);

    this._resizeInterval = setInterval(function(self) { self.zoomToPage(); }, 2000, this);
  },

  endLoading: function() {
    clearInterval(this._resizeInterval);
    this.zoomToPage();
  },

  viewportHandler: function(bounds, oldBounds) {
    let pageBounds = bounds.clone();
    pageBounds.top = Math.floor(this._screenToPage(bounds.top));
    pageBounds.left = Math.floor(this._screenToPage(bounds.left));
    pageBounds.bottom = Math.ceil(this._screenToPage(bounds.bottom));
    pageBounds.right = Math.ceil(this._screenToPage(bounds.right));

    if (0) {
      if (true /*!oldBounds*/) {
        this._pageX = pageBounds.x;
        this._pageY = pageBounds.y;

        var ctx = this._canvas.getContext("2d");

        ctx.save();
        ctx.scale(this._zoomLevel, this._zoomLevel);

        try {
          dump("drawWindow: " + pageBounds.x + " " + pageBounds.y + " " + pageBounds.width + " " + pageBounds.height + "\n");
          ctx.drawWindow(this._browser.contentWindow,
                         pageBounds.x, pageBounds.y, pageBounds.width, pageBounds.height,
                         "white",
                         ctx.DRAWWINDOW_DO_NOT_FLUSH | ctx.DRAWWINDOW_DRAW_CARET);
        } catch (e) {
          dump("DRAWWINDOW FAILED\n");
        }

        ctx.restore();
        return;
      }
    }

    if (!oldBounds) {
      // no old bounds means we resized the viewport, so redraw everything
      this._screenX = bounds.x;
      this._screenY = bounds.y;
      this._pageX = pageBounds.x;
      this._pageY = pageBounds.y;

      this._redrawRect(pageBounds.x, pageBounds.y,
                       pageBounds.width, pageBounds.height);
      return;
    }

    let dx = this._screenX - bounds.x;
    let dy = this._screenY - bounds.y;

    this._screenX = bounds.x;
    this._screenY = bounds.y;
    this._pageX = pageBounds.x;
    this._pageY = pageBounds.y;

    //dump("viewportHandler: " + bounds.toSource() + " " + oldBounds.toSource() + "\n");

    // deal with repainting
    let srcRect = { x: 0, y: 0,
                    width: this._canvas.width, height: this._canvas.height };
    let dstRect = { x: dx, y: dy,
                    width: this._canvas.width, height: this._canvas.height };

    // we don't need to do anything if the source and destination are the same
    if (srcRect.x == dstRect.x && srcRect.y == dstRect.y &&
        srcRect.width == dstRect.width && srcRect.height == dstRect.height) {
      //dump("avoiding dumb paint\n");
      return;
    }

    // blit what we can
    var ctx = this._canvas.getContext("2d");
    ctx.drawImage(this._canvas,
                  srcRect.x, srcRect.y,
                  srcRect.width, srcRect.height,
                  dstRect.x, dstRect.y,
                  dstRect.width, dstRect.height);

    //dump("blitting " + srcRect.toSource() + " to " + dstRect.toSource() + "\n");

    // redraw the rest
    var rgn = Cc["@mozilla.org/gfx/region;1"].createInstance(Ci.nsIScriptableRegion);
    rgn.setToRect(srcRect.x, srcRect.y, srcRect.width, srcRect.height);
    rgn.subtractRect(dstRect.x, dstRect.y, dstRect.width, dstRect.height);

    let outX = {}; let outY = {}; let outW = {}; let outH = {};
    rgn.getBoundingBox(outX, outY, outW, outH);
    dstRect = { x: outX.value, y: outY.value, width: outW.value, height: outH.value };

    if (dstRect.width > 0 && dstRect.height > 0) {
      dstRect.width += 1;
      dstRect.height += 1;


      //dump("redrawing: offset " + dstRect.x + " " + dstRect.y + "\n");

      ctx.save();
      ctx.translate(dstRect.x, dstRect.y);
      ctx.scale(this._zoomLevel, this._zoomLevel);

      let [offX, offY] = this._pageOffset;
      let scaledRect = { x: offX + this._screenToPage(dstRect.x),
                         y: offY + this._screenToPage(dstRect.y),
                         width: this._screenToPage(dstRect.width),
                         height: this._screenToPage(dstRect.height) };

      //dump("            rect " + scaledRect.toSource() + "\n");

      ctx.drawWindow(this._browser.contentWindow,
                     scaledRect.x, scaledRect.y,
                     scaledRect.width, scaledRect.height,
                     "white",
                     ctx.DRAWWINDOW_DO_NOT_FLUSH | ctx.DRAWWINDOW_DRAW_CARET);

      // for testing
      //ctx.fillStyle = "rgba(255,0,0,0.5)";
      //ctx.fillRect(0, 0, scaledRect.width, scaledRect.height);

      ctx.restore();
    }
  },

  _handleMozAfterPaint: function(aEvent) {
    let cwin = this._browser.contentWindow;

    for (let i = 0; i < aEvent.clientRects.length; i++) {
      let e = aEvent.clientRects.item(i);
      //dump(Math.floor(e.left + cwin.scrollX),
      //     Math.floor(e.top + cwin.scrollY),
      //     Math.ceil(e.width), Math.ceil(e.height));
      this._redrawRect(Math.floor(e.left + cwin.scrollX),
                       Math.floor(e.top + cwin.scrollY),
                       Math.ceil(e.width), Math.ceil(e.height));
    }
  },

  _redrawRect: function(x, y, width, height) {
    function intersect(r1, r2) {
      let xmost1 = r1.x + r1.width;
      let ymost1 = r1.y + r1.height;
      let xmost2 = r2.x + r2.width;
      let ymost2 = r2.y + r2.height;

      let x = Math.max(r1.x, r2.x);
      let y = Math.max(r1.y, r2.y);

      let temp = Math.min(xmost1, xmost2);
      if (temp <= x)
        return null;

      let width = temp - x;

      temp = Math.min(ymost1, ymost2);
      if (temp <= y)
        return null;

      let height = temp - y;

      return { x: x,
               y: y,
               width: width,
               height: height };
    }

    let r1 = { x : x,
               y : y,
               width : width,
               height: height };

    // check to see if the input coordinates are inside the visible destination
    let [canvasW, canvasH] = this._effectiveCanvasDimensions;
    let r2 = { x : this._pageX,
               y : this._pageY,
               width : canvasW,
               height: canvasH };

    let dest = intersect(r1, r2);

    if (!dest)
      return;

    //dump(dest.toSource() + "\n");

    var ctx = this._canvas.getContext("2d");

    ctx.save();
    ctx.scale(this._zoomLevel, this._zoomLevel);

    let [offX, offY] = this._pageOffset;
    ctx.translate(dest.x - offX, dest.y - offY);

    //dump("drawWindow#2: " + dest.x + " " + dest.y + " " + dest.width + " " + dest.height + " @ " + (dest.x - offX) + " " + (dest.y - offY) + "\n");
    ctx.drawWindow(this._browser.contentWindow,
                   dest.x, dest.y,
                   dest.width, dest.height,
                   "white",
                   ctx.DRAWWINDOW_DO_NOT_FLUSH | ctx.DRAWWINDOW_DRAW_CARET);

    ctx.restore();
  },

  _clampZoomLevel: function(aZoomLevel) {
    const min = 0.2;
    const max = 2.0;

    return Math.min(Math.max(min, aZoomLevel), max);
  },

  set zoomLevel(val) {
    this._zoomLevel = this._clampZoomLevel(val);
    Browser.updateViewportSize();
  },

  get zoomLevel() {
    return this._zoomLevel;
  },

  zoom: function(aDirection) {
    if (aDirection == 0)
      return;

    var zoomDelta = 0.05; // 1/20
    if (aDirection >= 0)
      zoomDelta *= -1;

    this.zoomLevel = this._zoomLevel + zoomDelta;
  },

  zoomToPage: function() {
    //dump("zoom to page\n");
    // Adjust the zoomLevel to fit the page contents in our window
    // width
    let [contentW, ] = this._contentAreaDimensions;
    let [canvasW, ] = this.canvasDimensions;

    if (contentW > canvasW)
      this.zoomLevel = canvasW / contentW;
  },

  zoomToElement: function(aElement) {
    const margin = 15;

    // XXX The widget stack code doesn't do what we want when you change
    // the viewport bounds to something smaller than your current position
    // so pan back to 0,0 before we resize and then pan to our destination
    ws.panTo(0, 0);

    // scale to the element's width
    let [canvasW, ] = this.canvasDimensions;

    let elRect = this._getPagePosition(aElement);
    let zoomLevel = canvasW / (elRect.width + (2 * margin));
    this.zoomLevel = Math.min(zoomLevel, 10);

    // pan to the element
    ws.panTo(Math.floor(Math.max(this._pageToScreen(elRect.x) - margin, 0)),
             Math.floor(Math.max(this._pageToScreen(elRect.y) - margin, 0)));
  },

  zoomFromElement: function(aElement) {
    let elRect = this._getPagePosition(aElement);

    // XXX The widget stack code doesn't do what we want when you change
    // the viewport bounds to something smaller than your current position
    // so pan back to 0,0 before we resize and then pan to our destination
    ws.panTo(0, 0);

    // pan to the element
    // don't bother with x since we're zooming all the way out
    this.zoomToPage();

    // XXX have this center the element on the page
    ws.panTo(0, Math.floor(Math.max(0, this._pageToScreen(elRect.y))));
  },

  /**
   * Retrieve the content element for a given point in client coordinates
   * (relative to the top left corner of the chrome window).
   */
  elementFromPoint: function(aX, aY) {
    let [x, y] = this._clientToContentCoords(aX, aY);
    let cwu = this._browser.contentWindow
                  .QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                  .getInterface(Components.interfaces.nsIDOMWindowUtils);

    let element = cwu.elementFromPoint(x, y,
                                       true,   /* ignore root scroll frame*/
                                       false); /* don't flush layout */

    return element;
  },

  /**
   * Retrieve the page position for a given element
   * (relative to the document origin).
   */
  _getPagePosition: function(aElement) {
    let r = aElement.getBoundingClientRect();
    let cwin = this._browser.contentWindow;
    let retVal = {
      width: r.width,
      height: r.height,
      x: r.left + cwin.scrollX,
      y: r.top + cwin.scrollY
    };

    return retVal;
  },

  get _pageOffset() {
    //    return [this._screenToPage(ws._viewport.viewportInnerBounds.x),
    //            this._screenToPage(ws._viewport.viewportInnerBounds.y)];
    return [this._pageX, this._pageY];
  },

  /* Given a set of client coordinates (relative to the app window),
   * returns the content coordinates relative to the viewport.
   */
  _clientToContentCoords: function(aClientX, aClientY) {
    // Determine position relative to the document origin
    // Need to adjust for the deckbrowser not being at 0,0
    // (e.g. due to other browser UI)

    let canvasRect = this._canvas.getBoundingClientRect();
    let clickOffsetX = this._screenToPage(aClientX - canvasRect.left) + this._pageX;
    let clickOffsetY = this._screenToPage(aClientY - canvasRect.top) + this._pageY;

    // Take scroll offset into account to return coordinates relative to the viewport
    let cwin = this._browser.contentWindow;
    return [clickOffsetX - cwin.scrollX,
            clickOffsetY - cwin.scrollY];
  },

  get _effectiveContentAreaDimensions() {
    return this._contentAreaDimensions.map(this._pageToScreen, this);
  },

  get _contentAreaDimensions() {
    var cdoc = this._browser.contentDocument;

    if (cdoc instanceof SVGDocument) {
      let rect = cdoc.rootElement.getBoundingClientRect();
      return [rect.width, rect.height];
    }

    // These might not exist yet depending on page load state
    var body = cdoc.body || {};
    var html = cdoc.documentElement || {};

    var w = Math.max(body.scrollWidth, html.scrollWidth);
    var h = Math.max(body.scrollHeight, html.scrollHeight);

    if (isNaN(w) || isNaN(h) || w == 0 || h == 0)
      return [this._canvas.width, this._canvas.height];

    return [w, h];
  },

  _screenToPage: function(aValue) {
    return aValue / this._zoomLevel;
  },

  _pageToScreen: function(aValue) {
    return aValue * this._zoomLevel;
  },

  /* ensures that a given content element is visible */
  ensureElementIsVisible: function(aElement) {
    // XXX
    // this method is broken because viewportDimensions getters are broken
    return;

    let elRect = this._getPagePosition(aElement);
    let [viewportW, viewportH] = this._effectiveViewportDimensions;
    let curRect = {
      x: this._pageX,
      y: this._pageY,
      width: viewportW,
      height: viewportH
    };

    // Adjust for part of our viewport being offscreen
    // XXX this assumes that the browser is meant to be fullscreen
    let browserRect = this._currentBrowser.getBoundingClientRect();
    curRect.height -= this._screenToPage(Math.abs(browserRect.top));
    if (browserRect.top < 0)
      curRect.y -= this._screenToPage(browserRect.top);
    curRect.width -= this._screenToPage(Math.abs(browserRect.left));
    if (browserRect.left < 0)
      curRect.x -= this._screenToPage(browserRect.left);

    let newx = curRect.x;
    let newy = curRect.y;

    if (elRect.x + elRect.width > curRect.x + curRect.width) {
      newx = curRect.x + ((elRect.x + elRect.width)-(curRect.x + curRect.width));
    } else if (elRect.x < curRect.x) {
      newx = elRect.x;
    }

    if (elRect.y + elRect.height > curRect.y + curRect.height) {
      newy = curRect.y + ((elRect.y + elRect.height)-(curRect.y + curRect.height));
    } else if (elRect.y < curRect.y) {
      newy = elRect.y;
    }

    this.panTo(newx, newy);
  },

  /* Pans directly to a given content element */
  panToElement: function(aElement) {
    var elRect = this._getPagePosition(aElement);

    this.panTo(elRect.x, elRect.y);
  },

  panTo: function(x, y) {
    ws.panTo(x, y);
  }
};
