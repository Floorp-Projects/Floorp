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
  _pageBounds: new wsRect(0,0,0,0),
  _screenX: 0,
  _screenY: 0,
  _visibleBounds:new wsRect(0,0,0,0),
  _drawQ: [],
  // during pageload: controls whether we poll document for size changing
  _maybeZoomToPage: false,
  // true during page loading(but not panning), restricts paints to visible part of the canvas
  _clippedPageDrawing: true,

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

  setCurrentBrowser: function(browser, skipZoom) {
    let currentBrowser = this._browser;
    if (currentBrowser) {
      // stop monitor paint events for this browser
      currentBrowser.removeEventListener("MozAfterPaint", this._paintHandler, false);
      currentBrowser.setAttribute("type", "content");
      currentBrowser.docShell.isOffScreenBrowser = false;
    }

    browser.setAttribute("type", "content-primary");
    if (!skipZoom)
      browser.docShell.isOffScreenBrowser = true;

    // start monitoring paint events for this browser
    var self = this;
    this._paintHandler = function(ev) { self._handleMozAfterPaint(ev); }

    browser.addEventListener("MozAfterPaint", this._paintHandler, false);

    this._browser = browser;

    // endLoading(and startLoading in most cases) calls zoom anyway
    if (!skipZoom) {
      self.zoomToPage();
    }
  },

  /*Heuristic heaven: Either adds things to a queue + starts timer or paints immediately */
  addToDrawQ: function addToDrawQ(rect) {
    let q = this._drawQ;
    function resizeAndPaint(self) {
      if (self._maybeZoomToPage) {
        self.zoomToPage();
      }
      // flush the whole queue when aren't loading page
      self.flushDrawQ(self._clippedPageDrawing);
    }
    for(let i = q.length - 1;i>=0;i--) {
      let old = q[i];
      if (!old)
        continue;
      //in the future do an intersect first, then intersect/trim/union
      if (old.contains(rect)) {
        //new paint is already in a queue
        return;
      } else if(rect.contains(old)) {
        //new paint is bigger than the one in the queue
        q[i] = null;
      }
    }

    /* During pageload:
     * do the paint immediately if it is something that can be drawn fast(and there aren't things queued up to be painted already */
    let flushNow = !this._clippedPageDrawing;

    if (this._clippedPageDrawing) {
      if (!this._drawInterval) {
        //always flush the first draw
        flushNow = true;
        this._maybeZoomToPage = true;
        this._drawInterval = setInterval(resizeAndPaint, 2000, this);
      }
    }

    q.push(rect);

    if (flushNow) {
      resizeAndPaint(this);
    }
  },

  // Change in zoom or offsets should require a clear
  // or a flush operation on the queue. XXX need tochanged justone to
  // be based on time taken..ie do as many paints as we can <200ms
  flushDrawQ: function flushDrawQ(justOne) {
    var ctx = this._canvas.getContext("2d");
    ctx.save();
    ctx.scale(this._zoomLevel, this._zoomLevel);
    while (this._drawQ.length) {
      let dest = this._drawQ.pop();
      if (!dest)
        continue;
      // ensure the rect is pixel-aligned once scaled
      dest.round(this._zoomLevel);
      ctx.translate(dest.x - this._pageBounds.x, dest.y - this._pageBounds.y);
      ctx.drawWindow(this._browser.contentWindow,
                     dest.x, dest.y,
                     dest.width, dest.height,
                     "white",
                     ctx.DRAWWINDOW_DO_NOT_FLUSH | ctx.DRAWWINDOW_DRAW_CARET);
      if (justOne)
        break;
    }
    ctx.restore();
  },

  clearDrawQ: function clearDrawQ() {
    this._drawQ = [];
  },

  startLoading: function() {
    // Clear the whole canvas
    // we clear the whole canvas because the browser's width or height
    // could be less than the area we end up actually drawing.
    this.clearDrawQ();
    var ctx = this._canvas.getContext("2d");
    ctx.fillStyle = "rgb(255,255,255)";
    ctx.fillRect(0, 0, this._canvas.width, this._canvas.height);
    this._clippedPageDrawing = true;
  },

  endLoading: function() {
    this._clippedPageDrawing = false;
    this._maybeZoomToPage = false;
    this.zoomToPage();
    this.ensureFullCanvasIsDrawn();

    if (this._drawInterval) {
      clearInterval(this._drawInterval);
      this._drawInterval = null;
    }
  },

  // ensure that the canvas outside of the viewport is also drawn
  ensureFullCanvasIsDrawn: function ensureFullCanvasIsDrawn() {
    if (this._partiallyDrawn) {
      let v = this._visibleBounds
      let r_above = new wsRect(this._pageBounds.x, this._pageBounds.y,
                               this._pageBounds.width, v.y - this._pageBounds.y)
      let r_left = new wsRect(this._pageBounds.x, v.y,
                              v.x - this._pageBounds.x,
                              v.height)
      let r_right = new wsRect(v.x + v.width, v.y,
                               this._pageBounds.width - v.x - v.width,
                               v.height)
      let r_below = new wsRect(this._pageBounds.x, v.y+v.height,
                               this._pageBounds.width,
                               this._pageBounds.height - v.y - v.height)
      this._redrawRect(r_above);
      this._redrawRect(r_left);
      this._redrawRect(r_right);
      this._redrawRect(r_below);
      this._partiallyDrawn = false;
    }
    // flush all pending draws
    this.flushDrawQ()
  },


  // Turn off incremental mode if it is on.
  // Switch _redrawRect to drawing the full canvas
  prepareForPanning: function prepareForPanning() {
    if (!this._clippedPageDrawing) 
      return;

    // keep checking page size
    this._maybeZoomToPage = true;

    // draw the rest of the canvas
    this._clippedPageDrawing = false;
    this.ensureFullCanvasIsDrawn();
  },

  viewportHandler: function(bounds, boundsSizeChanged) {
    let pageBounds = bounds.clone();
    let visibleBounds = ws.viewportVisibleRect;
    // top/left should not get rounded otherwise the blit below would have to use decimals
    pageBounds.top = this._screenToPage(pageBounds.top);
    pageBounds.left = this._screenToPage(pageBounds.left);
    pageBounds.bottom = Math.ceil(this._screenToPage(pageBounds.bottom));
    pageBounds.right = Math.ceil(this._screenToPage(pageBounds.right));

    // viewingRect property returns a new bounds object
    visibleBounds.top = Math.max(0, this._screenToPage(visibleBounds.top));
    visibleBounds.left = Math.max(0, this._screenToPage(visibleBounds.left));
    visibleBounds.bottom = this._screenToPage(visibleBounds.bottom);
    visibleBounds.right = this._screenToPage(visibleBounds.right);

    // if the page is being panned, flush the queue, so things blit correctly
    // this avoids incorrect offsets due to a change in _pageBounds.x/y
    // should probably check that (visibleBounds|pageBounds).(x|y) actually changed
    if (!boundsSizeChanged)
      this.flushDrawQ();

    this._visibleBounds = visibleBounds;
    this._pageBounds = pageBounds;

    let dx = this._screenX - bounds.x;
    let dy = this._screenY - bounds.y;
    this._screenX = bounds.x;
    this._screenY = bounds.y;

    if (boundsSizeChanged) {
      // we resized the viewport, so redraw everything
      // In theory this shouldn't be needed since adding a big rect to the draw queue
      // should remove the prior draws
      this.clearDrawQ();

      // make sure that ensureFullCanvasIsDrawn doesn't draw after a full redraw due to zoom
      if (!this._clippedPageDrawing)
        this._partiallyDrawn = false;

      this._redrawRect(pageBounds);
      return;
    }

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
    let rectCount = rgn.numRects
    for (let i = 0;i < rectCount;i++) {
      rgn.getRect(i, outX, outY, outW, outH);
      dstRect = { x: outX.value, y: outY.value, width: outW.value, height: outH.value };      
      dstRect.width += 1;
      dstRect.height += 1;

      ctx.save();
      ctx.translate(dstRect.x, dstRect.y);
      ctx.scale(this._zoomLevel, this._zoomLevel);

      let scaledRect = { x: this._pageBounds.x + this._screenToPage(dstRect.x),
                         y: this._pageBounds.y + this._screenToPage(dstRect.y),
                         width: this._screenToPage(dstRect.width),
                         height: this._screenToPage(dstRect.height) };

      ctx.drawWindow(this._browser.contentWindow,
                     scaledRect.x, scaledRect.y,
                     scaledRect.width, scaledRect.height,
                     "white",
                     ctx.DRAWWINDOW_DO_NOT_FLUSH | ctx.DRAWWINDOW_DRAW_CARET);
      ctx.restore();
    }
  },

  _handleMozAfterPaint: function(aEvent) {
    let cwin = this._browser.contentWindow;

    for (let i = 0; i < aEvent.clientRects.length; i++) {
      let e = aEvent.clientRects.item(i);
      let r = new wsRect(e.left + cwin.scrollX,
                         e.top + cwin.scrollY,
                         e.width, e.height);
      this._redrawRect(r);
    }
  },

  _redrawRect: function(rect) {
    // check to see if the input coordinates are inside the visible destination
    // during pageload clip drawing to the visible viewport
    if (this._clippedPageDrawing)  {
      r2 = this._visibleBounds;
      this._partiallyDrawn = true;
      // heuristic to throttle zoomToPage during page load
      if (rect.bottom > 0 && rect.right > r2.right)
        this._maybeZoomToPage = true;
    } else {
      let [canvasW, canvasH] = this._effectiveCanvasDimensions;
      r2 =  new wsRect(Math.max(this._pageBounds.x,0),
                       Math.max(this._pageBounds.y,0),
                       canvasW, canvasH);
    }

    let dest = rect.intersect(r2);

    if (dest)
      this.addToDrawQ(dest);
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

    if (this._clippedPageDrawing)
      this._maybeZoomToPage = false
  },

  zoomToElement: function(aElement) {
    const margin = 15;

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

  /* Given a set of client coordinates (relative to the app window),
   * returns the content coordinates relative to the viewport.
   */
  _clientToContentCoords: function(aClientX, aClientY) {
    // Determine position relative to the document origin
    // Need to adjust for the deckbrowser not being at 0,0
    // (e.g. due to other browser UI)

    let canvasRect = this._canvas.getBoundingClientRect();
    let clickOffsetX = this._screenToPage(aClientX - canvasRect.left) + this._pageBounds.x;
    let clickOffsetY = this._screenToPage(aClientY - canvasRect.top) + this._pageBounds.y;

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
    let elRect = this._getPagePosition(aElement);
    let curRect = this._visibleBounds
    let newx = curRect.x;
    let newy = curRect.y;

    if (elRect.x < curRect.x || elRect.width > curRect.width) {
      newx = elRect.x;
    } else if (elRect.x + elRect.width > curRect.x + curRect.width) {
      newx = elRect.x - curRect.width + elRect.width;
    }

    if (elRect.y < curRect.y || elRect.height > curRect.height) {
      newy = elRect.y;
    } else if (elRect.y + elRect.height > curRect.y + curRect.height) {
      newy = elRect.y - curRect.height + elRect.height;
    }

    ws.panTo(this._pageToScreen(newx), this._pageToScreen(newy));
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
