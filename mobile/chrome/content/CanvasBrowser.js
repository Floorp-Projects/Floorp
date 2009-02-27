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
  // during pageload: controls whether we poll document for size changing
  _maybeZoomToPage: false,
  // suggests to do clipped drawing in flushRegion
  _pageLoading: true,
  // 0,0 to contentW, contentH..is a list of dirty rectangles
  _rgnPage: Cc["@mozilla.org/gfx/region;1"].createInstance(Ci.nsIScriptableRegion),
  // used to force paints to not be delayed during panning, otherwise things
  // some things seem to get drawn at the wrong offset, not sure why
  _isPanning: false,
  
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

  /*Heuristic heaven: Either adds things to a queue + starts timer or paints immediately 
   * rect has to be all integers
   */
  addToRegion: function addToRegion(rect) {
    this._rgnPage.unionRect(rect.x, rect.y, rect.width, rect.height);

    function resizeAndPaint(self) {
      if (self._maybeZoomToPage) {
        self.zoomToPage();
      }
      // draw visible area..freeze during pans
      if (!self._isPanning)
        self.flushRegion(true);
    }
    
    let flushNow = !this._pageLoading && rect.intersects(this._visibleBounds);
    
    // TODO: only fire timer if there are paints to be done
    if (this._pageLoading && !this._drawInterval) {
      //always flush the first draw
      flushNow = true;
      this._maybeZoomToPage = true;
      this._drawInterval = setInterval(resizeAndPaint, 2000, this);
    }

    if (flushNow) {
      resizeAndPaint(this);
    }
  },

  // Change in zoom or offsets should require a clear
  // or a flush operation on the queue. XXX need tochanged justone to
  // be based on time taken..ie do as many paints as we can <200ms
  // returns true if q is cleared
  flushRegion: function flushRegion(viewingBoundsOnly) {
    let rgn = this._rgnPage;

    let clearRegion = false;
    let drawls = [];
    let outX = {}; let outY = {}; let outW = {}; let outH = {};
    let numRects = rgn.numRects;
    for (let i=0;i<numRects;i++) {
      rgn.getRect(i, outX, outY, outW, outH);
      let rect = new wsRect(outX.value, outY.value,
                            outW.value, outH.value);
      if (viewingBoundsOnly) {
        //only draw the visible area
        rect = rect.intersect(this._visibleBounds)
        if (!rect)
          continue;
      } else {
        clearRegion = true;
      }
      drawls.push(rect)
    }

    if (clearRegion)
      this.clearRegion();

    let oldX = 0;
    let oldY = 0;
    var ctx = this._canvas.getContext("2d");
    ctx.save();
    ctx.scale(this._zoomLevel, this._zoomLevel);

    // do subtraction separately as it modifies the rect list
    for each(let rect in drawls) {
      // avoid subtracting the rect if region was cleared
      if (!clearRegion)
        rgn.subtractRect(rect.left, rect.top,
                         rect.width, rect.height);
      // ensure that once scaled, the rect has whole pixels
      rect.round(this._zoomLevel);
      let x = rect.x - this._pageBounds.x
      let y = rect.y - this._pageBounds.y
      // translate is relative, so make up for that
      ctx.translate(x - oldX, y - oldY);
      oldX = x;
      oldY = y;
      ctx.drawWindow(this._browser.contentWindow,
                     rect.x, rect.y,
                     rect.width, rect.height,
                     "white",
                     (ctx.DRAWWINDOW_DO_NOT_FLUSH | ctx.DRAWWINDOW_DRAW_CARET));
    }
    // should get a way to figure out when there are no more valid rects left, and clear q
    ctx.restore();
  },

  clearRegion: function clearRegion() {
    // once all of the rectangles have been subtracted
    // region ends up in a funny state unless it's reset
    this._rgnPage.setToRect(0,0,0,0);
  },

  startLoading: function startLoading() {
    // Clear the whole canvas
    // we clear the whole canvas because the browser's width or height
    // could be less than the area we end up actually drawing.
    this.clearRegion();
    var ctx = this._canvas.getContext("2d");
    ctx.fillStyle = "rgb(255,255,255)";
    ctx.fillRect(0, 0, this._canvas.width, this._canvas.height);
    this._pageLoading = true;
  },

  endLoading: function() {
    this._pageLoading = false;
    this._maybeZoomToPage = false;
    this.zoomToPage();
    // flush the region, to reduce prepareForPanning delay
    this.flushRegion();
    
    if (this._drawInterval) {
      clearInterval(this._drawInterval);
      this._drawInterval = null;
    }
  },

  // flush outstanding dirty rects,
  // switch to unoptimized painting mode during panning
  prepareForPanning: function prepareForPanning() {
    this.flushRegion();
    
    // do not delay paints as that causes displaced painting bugs
    this._isPanning = true;
  },

  viewportHandler: function(bounds, boundsSizeChanged) {
    this._isPanning = false;
    let pageBounds = bounds.clone();
    let visibleBounds = ws.viewportVisibleRect;

    //should add a divide/multiply func to pageBounds

    // do not floor top/left, or the blit below will be off
    pageBounds.top = this._screenToPage(pageBounds.top);
    pageBounds.left = this._screenToPage(pageBounds.left);
    pageBounds.bottom = Math.ceil(this._screenToPage(pageBounds.bottom));
    pageBounds.right = Math.ceil(this._screenToPage(pageBounds.right));
        
    visibleBounds.top = Math.max(0, this._screenToPage(visibleBounds.top));
    visibleBounds.left = Math.max(0, this._screenToPage(visibleBounds.left));
    visibleBounds.bottom = Math.ceil(this._screenToPage(visibleBounds.bottom));
    visibleBounds.right = Math.ceil(this._screenToPage(visibleBounds.right));

    // if the page is being panned, flush the queue, so things blit correctly
    // this avoids incorrect offsets due to a change in _pageBounds.x/y
    // should probably check that (visibleBounds|pageBounds).(x|y) actually changed
    if (boundsSizeChanged)
      this.clearRegion();
    else
      this.flushRegion();
    
    this._visibleBounds = visibleBounds;
    this._pageBounds = pageBounds;

    let dx = this._screenX - bounds.x;
    let dy = this._screenY - bounds.y;
    this._screenX = bounds.x;
    this._screenY = bounds.y;

    if (boundsSizeChanged) {
      this._redrawRect(pageBounds);
      return;
    }

    // deal with repainting
    // we don't need to do anything if the source and destination are the same
    if (!dx && !dy) {
      // avoiding dumb paint
      return;
    }

    // blit what we can
    var ctx = this._canvas.getContext("2d");
    let cWidth = this._canvas.width;
    let cHeight = this._canvas.height;
    
    ctx.drawImage(this._canvas,
                  0, 0, cWidth, cHeight,
                  dx, dy, cWidth, cHeight);

    // redraw the rest

    // reuse rect to avoid overhead of creating a new one
    let rgn = this._rgnScratch;
    if (!rgn) {
      rgn = Cc["@mozilla.org/gfx/region;1"].createInstance(Ci.nsIScriptableRegion);
      this._rgnScratch = rgn;
    }
    rgn.setToRect(0, 0, cWidth, cHeight);
    rgn.subtractRect(dx, dy, cWidth, cHeight);
    
    let outX = {}; let outY = {}; let outW = {}; let outH = {};
    let rectCount = rgn.numRects
    for (let i = 0;i < rectCount;i++) {
      rgn.getRect(i, outX, outY, outW, outH);
      if (outW.value > 0 && outH.value > 0) {
        this._redrawRect(new wsRect(Math.floor(this._pageBounds.x +this._screenToPage(outX.value)),
                                    Math.floor(this._pageBounds.y +this._screenToPage(outY.value)),
                                    Math.ceil(this._screenToPage(outW.value)),
                                    Math.ceil(this._screenToPage(outH.value))));
      }
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
    if (this._pageLoading)  {
      if (rect.bottom > 0 && rect.right > this._visibleBounds.right)
        this._maybeZoomToPage = true;
    } 
    
    let r2 = this._pageBounds.clone();
    r2.left = Math.max(r2.left, 0);
    r2.top = Math.max(r2.top, 0);
    let dest = rect.intersect(r2);
    
    if (dest) {
      dest.round(1);
      this.addToRegion(dest);
    }
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
      this._maybeZoomToPage = false;
  },

  zoomToElement: function(aElement) {
    const margin = 15;

    // scale to the element's width
    let [canvasW, ] = this.canvasDimensions;

    let elRect = this._getPagePosition(aElement);
    let zoomLevel = canvasW / (elRect.width + (2 * margin));

    ws.beginUpdateBatch();

    this.zoomLevel = Math.min(zoomLevel, 10);

    // pan to the element
    ws.panTo(Math.floor(Math.max(this._pageToScreen(elRect.x) - margin, 0)),
             Math.floor(Math.max(this._pageToScreen(elRect.y) - margin, 0)));

    ws.endUpdateBatch();
  },

  zoomFromElement: function(aElement) {
    let elRect = this._getPagePosition(aElement);

    ws.beginUpdateBatch();

    // pan to the element
    // don't bother with x since we're zooming all the way out
    this.zoomToPage();

    // XXX have this center the element on the page
    ws.panTo(0, Math.floor(Math.max(0, this._pageToScreen(elRect.y))));

    ws.endUpdateBatch();
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
