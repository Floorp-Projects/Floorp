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
  this._rgnPage = Cc["@mozilla.org/gfx/region;1"].createInstance(Ci.nsIScriptableRegion);
  this._pageBounds = new wsRect(0,0,0,0);
  this._visibleBounds = new wsRect(0,0,0,0);
}

CanvasBrowser.prototype = {
  _canvas: null,
  _zoomLevel: 1,
  _browser: null,
  _pageBounds: null,
  _screenX: 0,
  _screenY: 0,
  _visibleBounds: null,

  // during pageload: controls whether we poll document for size changing
  _lazyWidthChanged: false,
  _lazyHeightChanged: false,

  // if true, the page is currently loading
  _pageLoading: true,

  // 0,0 to contentW, contentH..is a list of dirty rectangles
  _rgnPage: null,

  // used to force paints to not be delayed during panning, otherwise things
  // some things seem to get drawn at the wrong offset, not sure why
  _isPanning: false,

  // if we have an outstanding paint timeout, its value is stored here
  // for cancelling when we end page loads
  _drawTimeout: 0,

  // the max right coordinate we've seen from paint events
  // while we were loading a page.  If we see something that's bigger than
  // our width, we'll trigger a page zoom.
  _maxRight: 0,
  _maxBottom: 0,
  
  // Tells us to pan to top before first draw 
  _needToPanToTop: false,

  get canvasDimensions() {
    if (!this._canvasRect) {
      let canvasRect = this._canvas.getBoundingClientRect();
      this._canvasRect = {
        width: canvasRect.width,
        height: canvasRect.height
      };
    }
    return [this._canvasRect.width, this._canvasRect.height];
  },

  get _effectiveCanvasDimensions() {
    return this.canvasDimensions.map(this._screenToPage, this);
  },

  get contentDOMWindowUtils() {
    if (!this._contentDOMWindowUtils) {
      this._contentDOMWindowUtils = this._browser.contentWindow
        .QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIDOMWindowUtils);
    }
    return this._contentDOMWindowUtils;
  },

  setCurrentBrowser: function(browser, skipZoom) {
    let currentBrowser = this._browser;
    if (currentBrowser) {
      // stop monitor paint events for this browser
      currentBrowser.removeEventListener("MozAfterPaint", this._paintHandler, false);
      currentBrowser.setAttribute("type", "content");
      currentBrowser.docShell.isOffScreenBrowser = false;
    }
    this._contentDOMWindowUtils = null;

    browser.setAttribute("type", "content-primary");
    if (!skipZoom)
      browser.docShell.isOffScreenBrowser = true;

    // start monitoring paint events for this browser
    var self = this;
    this._paintHandler = function(ev) { self._handleMozAfterPaint(ev); };

    browser.addEventListener("MozAfterPaint", this._paintHandler, false);

    this._browser = browser;

    // endLoading(and startLoading in most cases) calls zoom anyway
    if (!skipZoom) {
      self.zoomToPage();
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
    let updateBounds = null;
    let pixelsInRegion = 0;
    let inRects = rgn.getRects();
    if (!inRects)
      return;

    for (let i = 0; i < inRects.length; i+=4) {
      let rect = new wsRect(inRects[i], inRects[i+1],
                            inRects[i+2], inRects[i+3]);
      if (viewingBoundsOnly) {
        //only draw the visible area
        rect = rect.intersect(this._visibleBounds);
        if (!rect)
          continue;
      } else {
        clearRegion = true;
      }
      drawls.push(rect);

      if (updateBounds == null) {
        updateBounds = rect.clone();
      } else {
        updateBounds = updateBounds.union(rect);
      }

      pixelsInRegion += rect.width * rect.height;
    }

    // if we know that we're going to be clearing the region, we just
    // do it here; otherwise, we'll subtract the rects we update one by
    // one as we loop over them
    if (clearRegion)
      this.clearRegion();

    // XXX be smarter about this.  If the number of pixels
    // touched by each rect in the region is greater than the
    // number of pixels in the bounds of the region times some factor,
    // just repaint the bounds.  This uses 90%, based on a totally
    // random guess.
    //
    // The idea is to avoid a bunch of separate thin draws (either vertically
    // or horizontally), while still getting the region optimization
    // for things like opposite corners of a page being rendered.
    //
    // A possible better optimization would be to extend drawWindow to
    // take a region instead of a rect, because right now it'll call
    // Invalidate for each of the draw calls.  Gtk at least will often
    // process the first one quickly and queue up the rest, which can
    // lead to some odd looking rendering behaviour.

    if (drawls.length > 1 &&
        pixelsInRegion > (updateBounds.width * updateBounds.height * 0.90))
    {
      drawls = [updateBounds];
    }
    this._drawRects(drawls, !clearRegion);
  },

  _drawRects: function _drawRects(drawls, subtractRects) {
    let oldX = 0;
    let oldY = 0;
    var ctx = this._canvas.getContext("2d");
    ctx.save();
    ctx.scale(this._zoomLevel, this._zoomLevel);

    // do subtraction separately as it modifies the rect list
    for each (let rect in drawls) {
      if (subtractRects)
        this._rgnPage.subtractRect(rect.left, rect.top, rect.width, rect.height);

      // ensure that once scaled, the rect has whole pixels
      rect.round(this._zoomLevel);
      let x = rect.x - this._pageBounds.x;
      let y = rect.y - this._pageBounds.y;

      // translate is relative, so make up for that
      // XXX this could introduce some small fp errors, but it's
      // probably not enough to worry about.
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
    this._maxRight = 0;
    this._maxBottom = 0;
    this._pageLoading = true;
    this._needToPanToTop = true;
  },

  endLoading: function() {
    this._pageLoading = false;
    this._lazyWidthChanged = false;
    this._lazyHeightChanged = false;
    this.zoomToPage();
    // flush the region, to reduce startPanning delay
    // and to avoid getting a black border in tab thumbnail
    this.flushRegion();

    if (this._drawTimeout) {
      clearTimeout(this._drawTimeout);
      this._drawTimeout = 0;
    }
  },

  // flush outstanding dirty rects,
  // switch to unoptimized painting mode during panning
  startPanning: function startPanning() {
    this.flushRegion();

    // do not delay paints as that causes displaced painting bugs
    this._isPanning = true;
  },

  endPanning: function endPanning() {
    this.flushRegion();

    this._isPanning = false;
  },

  viewportHandler: function viewportHandler(bounds, boundsSizeChanged) {
    this._isPanning = false;
    let pageBounds = bounds.clone();
    let visibleBounds = ws.viewportVisibleRect;

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
    if (boundsSizeChanged) {
      this.clearRegion();
      // since we are going to repaint the whole browser
      // any outstanding paint events will cause redundant draws
      this.contentDOMWindowUtils.clearMozAfterPaintEvents();
    } else
      this.flushRegion();

    this._visibleBounds = visibleBounds;
    this._pageBounds = pageBounds;

    let dx = this._screenX - bounds.x;
    let dy = this._screenY - bounds.y;
    this._screenX = bounds.x;
    this._screenY = bounds.y;

    if (boundsSizeChanged) {
      this._redrawRects([pageBounds]);
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

    let rects = rgn.getRects();
    if (!rects)
      return;

    let rectsToDraw = [];
    for (let i = 0; i < rects.length; i+=4) {
      rectsToDraw.push(new wsRect(this._pageBounds.x + this._screenToPage(rects[i]),
                                  this._pageBounds.y + this._screenToPage(rects[i+1]),
                                  this._screenToPage(rects[i+2]),
                                  this._screenToPage(rects[i+3])));
    }
    this._redrawRects(rectsToDraw);
  },

  _handleMozAfterPaint: function(aEvent) {
    let [scrollX, scrollY] = this.contentScrollValues;
    let clientRects = aEvent.clientRects;

    let rects = [];
    // loop backwards to avoid xpconnect penalty for .length
    for (let i = clientRects.length - 1; i >= 0; --i) {
      let e = clientRects.item(i);
      let r = new wsRect(e.left + scrollX,
                         e.top + scrollY,
                         e.width, e.height);
      rects.push(r);
    }

    this._redrawRects(rects);
  },

  _redrawRects: function(rects) {
    // skip the region logic for basic paints
    if (!this._pageLoading && rects.length == 1
        && this._visibleBounds.contains(rects[0])) {
      this._drawRects(rects, false);
      return;
    }

    // check to see if the input coordinates are inside the visible destination
    // during pageload clip drawing to the visible viewport
    let realRectCount = 0;

    let zeroPageBounds = this._pageBounds.clone();
    zeroPageBounds.left = Math.max(zeroPageBounds.left, 0);
    zeroPageBounds.top = Math.max(zeroPageBounds.top, 0);

    // Go through each rect, checking whether it intersects the
    // visible part of the page bounds.  If it doesn't, skip it.
    for each (var rect in rects) {
      if (this._pageLoading)  {
        // if this rect would push us beyond our known size to the right
        // (since we zoom to page we don't care about bottom/height)
        if (rect.right > this._maxRight) {
          this._lazyWidthChanged = true;
          this._maxRight = rect.right;
        }
        if (rect.bottom > this._maxBottom) {
          this._lazyHeightChanged = true;
          this._maxBottom = rect.bottom;
        }
      }

      rect = rect.intersect(zeroPageBounds);

      if (!rect)
        continue;

      rect.round(1);

      this._rgnPage.unionRect(rect.x, rect.y, rect.width, rect.height);

      realRectCount++;
    }

    // did we end up with anything to do?
    if (realRectCount == 0)
      return;

    // a little helper function; we might decide to call it right away,
    // or we might decide to delay it if the page is still loading.
    function resizeAndPaint(self) {
      if (self._lazyWidthChanged) {
        // XXX rather than calling zoomToPage, which can trigger a browser reflow
        // just zoom to the max width we've seen so far.
        // we'll make sure to use the real page's bounds when the page finishes loading
        // but this should work fine for now.
        //self.zoomToPage();
        let contentW = self._maxRight;
        let [canvasW, ] = self.canvasDimensions;

        if (contentW > canvasW)
          this.zoomLevel = canvasW / contentW;

        self._lazyWidthChanged = false;
      } else if (self._lazyHeightChanged) {

        // Setting the zoomLevel will call updateViewportSize.
        // We need to handle height changes seperately.
        Browser.updateViewportSize();
        self._lazyHeightChanged = false;
      }

      // draw visible area..freeze during pans
      if (!self._isPanning)
        self.flushRegion(true);

      if (self._pageLoading) {
        // kick ourselves off 2s later while we're still loading
        self._drawTimeout = setTimeout(resizeAndPaint, 2000, self);
      } else {
        self._drawTimeout = 0;
      }
    }

    let flushNow = !this._pageLoading;

    // page is loading, and we don't have an existing draw here.  Note that
    // setTimeout is used (and reset in resizeAndPaint)
    if (this._pageLoading && !this._drawTimeout) {
      //always flush the first draw
      flushNow = true;
      this._lazyWidthChanged = true;
      this._drawTimeout = setTimeout(resizeAndPaint, 2000, this);
    }

    if (flushNow) {
      resizeAndPaint(this);
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
    let needToPanToTop = this._needToPanToTop;
    // Ensure pages are panned at the top before zooming/painting
    // combine the initial pan + zoom into a transaction
    if (needToPanToTop) {
      ws.beginUpdateBatch();      
      this._needToPanToTop = false;
      ws.panTo(0, 0);
    }
    // Adjust the zoomLevel to fit the page contents in our window
    // width
    let [contentW, ] = this._contentAreaDimensions;
    let [canvasW, ] = this.canvasDimensions;

    if (contentW > canvasW)
      this.zoomLevel = canvasW / contentW;
    
    if (needToPanToTop)
      ws.endUpdateBatch();
  },

  zoomToElement: function(aElement) {
    const margin = 15;

    let elRect = this._getPagePosition(aElement);
    let elWidth = elRect.width;
    let visibleViewportWidth = this._pageToScreen(this._visibleBounds.width);
    /* Try to set zoom-level such that once zoomed element is as wide
     *  as the visible viewport */
    let zoomLevel = visibleViewportWidth / (elWidth + (2 * margin));
    ws.beginUpdateBatch();

    this.zoomLevel = zoomLevel;

    // uncomment this to force the zoom level to 1.0 on zoom, instead of
    // doing a real zoom to element
    //this.zoomLevel = 1.0;

    /* If zoomLevel ends up clamped to less than asked for, calculate
     * how many more screen pixels will fit horizontally in addition to
     * element's width. This ensures that more of the webpage is
     * showing instead of the navbar. Bug 480595. */
    let xpadding = Math.max(margin, visibleViewportWidth - this._pageToScreen(elWidth));

    // pan to the element
    ws.panTo(Math.floor(Math.max(this._pageToScreen(elRect.x) - xpadding, 0)),
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
    let cwu = this.contentDOMWindowUtils;
    return cwu.elementFromPoint(x, y,
                                true,   /* ignore root scroll frame*/
                                false); /* don't flush layout */
  },

  /**
   * Retrieve the page position for a given element
   * (relative to the document origin).
   */
  _getPagePosition: function(aElement) {
    let [scrollX, scrollY] = this.contentScrollValues;
    let r = aElement.getBoundingClientRect();

    return {
      width: r.width,
      height: r.height,
      x: r.left + scrollX,
      y: r.top + scrollY
    };
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
    let [scrollX, scrollY] = this.contentScrollValues;
    return [clickOffsetX - scrollX,
            clickOffsetY - scrollY];
  },
  
  get contentScrollValues() {
    let cwu = this.contentDOMWindowUtils;
    let scrollX = {}, scrollY = {};
    cwu.getScrollXY(false, scrollX, scrollY);

    return [scrollX.value, scrollY.value];
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
    let curRect = this._visibleBounds;
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
