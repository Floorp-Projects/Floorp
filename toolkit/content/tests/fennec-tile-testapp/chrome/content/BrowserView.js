// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let Ci = Components.interfaces;

// --- REMOVE ---
let noop = function() {};
let endl = '\n';
// --------------

function BrowserView(container, visibleRect) {
  bindAll(this);
  this.init(container, visibleRect);
}

/**
 * A BrowserView maintains state of the viewport (browser, zoom level,
 * dimensions) and the visible rectangle into the viewport, for every
 * browser it is given (cf setBrowser()).  In updates to the viewport state,
 * a BrowserView (using its TileManager) renders parts of the page quasi-
 * intelligently, with guarantees of having rendered and appended all of the
 * visible browser content (aka the "critical rectangle").
 *
 * State is characterized in large part by two rectangles (and an implicit third):
 * - Viewport: Always rooted at the origin, ie with (left, top) at (0, 0).  The
 *     width and height (right and bottom) of this rectangle are that of the
 *     current viewport, which corresponds more or less to the transformed
 *     browser content (scaled by zoom level).
 * - Visible: Corresponds to the client's viewing rectangle in viewport
 *     coordinates.  Has (top, left) corresponding to position, and width & height
 *     corresponding to the clients viewing dimensions.  Take note that the top
 *     and left of the visible rect are per-browser state, but that the width
 *     and height persist across setBrowser() calls.  This is best explained by
 *     a simple example: user views browser A, pans to position (x0, y0), switches
 *     to browser B, where she finds herself at position (x1, y1), tilts her
 *     device so that visible rectangle's width and height change, and switches
 *     back to browser A.  She expects to come back to position (x0, y0), but her
 *     device remains tilted.
 * - Critical (the implicit one): The critical rectangle is the (possibly null)
 *     intersection of the visible and viewport rectangles.  That is, it is that
 *     region of the viewport which is visible to the user.  We care about this
 *     because it tells us which region must be rendered as soon as it is dirtied.
 *     The critical rectangle is mostly state that we do not keep in BrowserView
 *     but that our TileManager maintains.
 *
 * Example rectangle state configurations:
 *
 *
 *        +-------------------------------+
 *        |A                              |
 *        |                               |
 *        |                               |
 *        |                               |
 *        |        +----------------+     |
 *        |        |B,C             |     |
 *        |        |                |     |
 *        |        |                |     |
 *        |        |                |     |
 *        |        +----------------+     |
 *        |                               |
 *        |                               |
 *        |                               |
 *        |                               |
 *        |                               |
 *        +-------------------------------+
 *
 *
 * A = viewport ; at (0, 0)
 * B = visible  ; at (x, y) where x > 0, y > 0
 * C = critical ; at (x, y)
 *
 *
 *
 *        +-------------------------------+
 *        |A                              |
 *        |                               |
 *        |                               |
 *        |                               |
 *   +----+-----------+                   |
 *   |B   .C          |                   |
 *   |    .           |                   |
 *   |    .           |                   |
 *   |    .           |                   |
 *   +----+-----------+                   |
 *        |                               |
 *        |                               |
 *        |                               |
 *        |                               |
 *        |                               |
 *        +-------------------------------+
 *
 *
 * A = viewport ; at (0, 0)
 * B = visible  ; at (x, y) where x < 0, y > 0
 * C = critical ; at (0, y)
 *
 *
 * Maintaining per-browser state is a little bit of a hack involving attaching
 * an object as the obfuscated dynamic JS property of the browser object, that
 * hopefully no one but us will touch.  See getViewportStateFromBrowser() for
 * the property name.
 */
BrowserView.prototype = (
function() {

  // -----------------------------------------------------------
  // Privates
  //

  const kZoomLevelMin = 0.2;
  const kZoomLevelMax = 4.0;
  const kZoomLevelPrecision = 10000;

  function visibleRectToCriticalRect(visibleRect, browserViewportState) {
    return visibleRect.intersect(browserViewportState.viewportRect);
  }

  function clampZoomLevel(zl) {
    let bounded = Math.min(Math.max(kZoomLevelMin, zl), kZoomLevelMax);
    return Math.round(bounded * kZoomLevelPrecision) / kZoomLevelPrecision;
  }

  function pageZoomLevel(visibleRect, browserW, browserH) {
    return clampZoomLevel(visibleRect.width / browserW);
  }

  function seenBrowser(browser) {
    return !!(browser.__BrowserView__vps);
  }

  function initBrowserState(browser, visibleRect) {
    let [browserW, browserH] = getBrowserDimensions(browser);

    let zoomLevel = pageZoomLevel(visibleRect, browserW, browserH);
    let viewportRect = (new wsRect(0, 0, browserW, browserH)).scale(zoomLevel, zoomLevel);

    dump('--- initing browser to ---' + endl);
    browser.__BrowserView__vps = new BrowserView.BrowserViewportState(viewportRect,
                                                                      visibleRect.x,
                                                                      visibleRect.y,
                                                                      zoomLevel);
    dump(browser.__BrowserView__vps.toString() + endl);
    dump('--------------------------' + endl);
  }

  function getViewportStateFromBrowser(browser) {
    return browser.__BrowserView__vps;
  }

  function getBrowserDimensions(browser) {
    return [browser.scrollWidth, browser.scrollHeight];
  }

  function getContentScrollValues(browser) {
    let cwu = getBrowserDOMWindowUtils(browser);
    let scrollX = {};
    let scrollY = {};
    cwu.getScrollXY(false, scrollX, scrollY);

    return [scrollX.value, scrollY.value];
  }

  function getBrowserDOMWindowUtils(browser) {
    return browser.contentWindow
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIDOMWindowUtils);
  }

  function getNewBatchOperationState() {
    return {
      viewportSizeChanged: false,
      dirtyAll: false
    };
  }

  function clampViewportWH(width, height, visibleRect) {
    let minW = visibleRect.width;
    let minH = visibleRect.height;
    return [Math.max(width, minW), Math.max(height, minH)];
  }

  function initContainer(container, visibleRect) {
    container.style.width    = visibleRect.width  + 'px';
    container.style.height   = visibleRect.height + 'px';
    container.style.overflow = '-moz-hidden-unscrollable';
  }

  function resizeContainerToViewport(container, viewportRect) {
    container.style.width  = viewportRect.width  + 'px';
    container.style.height = viewportRect.height + 'px';
  }

  // !!! --- RESIZE HACK BEGIN -----
  function simulateMozAfterSizeChange(browser, width, height) {
    let ev = document.createElement("MouseEvents");
    ev.initEvent("FakeMozAfterSizeChange", false, false, window, 0, width, height);
    browser.dispatchEvent(ev);
  }
  // !!! --- RESIZE HACK END -------

  // --- Change of coordinates functions --- //


  // The following returned object becomes BrowserView.prototype
  return {

    // -----------------------------------------------------------
    // Public instance methods
    //

    init: function init(container, visibleRect) {
      this._batchOps = [];
      this._container = container;
      this._browserViewportState = null;
      this._renderMode = 0;
      this._tileManager = new TileManager(this._appendTile, this._removeTile, this);
      this.setVisibleRect(visibleRect);

      // !!! --- RESIZE HACK BEGIN -----
      // remove this eventually
      this._resizeHack = {
        maxSeenW: 0,
        maxSeenH: 0
      };
      // !!! --- RESIZE HACK END -------
    },

    setVisibleRect: function setVisibleRect(r) {
      let bvs = this._browserViewportState;
      let vr  = this._visibleRect;

      if (!vr)
        this._visibleRect = vr = r.clone();
      else
        vr.copyFrom(r);

      if (bvs) {
        bvs.visibleX = vr.left;
        bvs.visibleY = vr.top;

        // reclamp minimally to the new visible rect
        //this.setViewportDimensions(bvs.viewportRect.right, bvs.viewportRect.bottom);
      } else
        this._viewportChanged(false, false);
    },

    getVisibleRect: function getVisibleRect() {
      return this._visibleRect.clone();
    },

    getVisibleRectX: function getVisibleRectX() { return this._visibleRect.x; },
    getVisibleRectY: function getVisibleRectY() { return this._visibleRect.y; },
    getVisibleRectWidth: function getVisibleRectWidth() { return this._visibleRect.width; },
    getVisibleRectHeight: function getVisibleRectHeight() { return this._visibleRect.height; },

    setViewportDimensions: function setViewportDimensions(width, height, causedByZoom) {
      let bvs = this._browserViewportState;
      let vis = this._visibleRect;

      if (!bvs)
        return;

      //[width, height] = clampViewportWH(width, height, vis);
      bvs.viewportRect.right  = width;
      bvs.viewportRect.bottom = height;

      // XXX we might not want the user's page to disappear from under them
      // at this point, which could happen if the container gets resized such
      // that visible rect becomes entirely outside of viewport rect.  might
      // be wise to define what UX should be in this case, like a move occurs.
      // then again, we could also argue this is the responsibility of the
      // caller who would do such a thing...

      this._viewportChanged(true, !!causedByZoom);
    },

    setZoomLevel: function setZoomLevel(zl) {
      let bvs = this._browserViewportState;

      if (!bvs)
        return;

      let newZL = clampZoomLevel(zl);

      if (newZL != bvs.zoomLevel) {
        let browserW = this.viewportToBrowser(bvs.viewportRect.right);
        let browserH = this.viewportToBrowser(bvs.viewportRect.bottom);
        bvs.zoomLevel = newZL; // side-effect: now scale factor in transformations is newZL
        this.setViewportDimensions(this.browserToViewport(browserW),
                                   this.browserToViewport(browserH));
      }
    },

    getZoomLevel: function getZoomLevel() {
      let bvs = this._browserViewportState;
      if (!bvs)
        return undefined;

      return bvs.zoomLevel;
    },

    beginBatchOperation: function beginBatchOperation() {
      this._batchOps.push(getNewBatchOperationState());
      this.pauseRendering();
    },

    commitBatchOperation: function commitBatchOperation() {
      let bops = this._batchOps;

      if (bops.length == 0)
        return;

      let opState = bops.pop();
      this._viewportChanged(opState.viewportSizeChanged, opState.dirtyAll);
      this.resumeRendering();
    },

    discardBatchOperation: function discardBatchOperation() {
      let bops = this._batchOps;
      bops.pop();
      this.resumeRendering();
    },

    discardAllBatchOperations: function discardAllBatchOperations() {
      let bops = this._batchOps;
      while (bops.length > 0)
        this.discardBatchOperation();
    },

    moveVisibleBy: function moveVisibleBy(dx, dy) {
      let vr = this._visibleRect;
      let vs = this._browserViewportState;

      this.onBeforeVisibleMove(dx, dy);
      this.onAfterVisibleMove(dx, dy);
    },

    moveVisibleTo: function moveVisibleTo(x, y) {
      let visibleRect = this._visibleRect;
      let dx = x - visibleRect.x;
      let dy = y - visibleRect.y;
      this.moveBy(dx, dy);
    },

    /**
     * Calls to this function need to be one-to-one with calls to
     * resumeRendering()
     */
    pauseRendering: function pauseRendering() {
      this._renderMode++;
    },

    /**
     * Calls to this function need to be one-to-one with calls to
     * pauseRendering()
     */
    resumeRendering: function resumeRendering(renderNow) {
      if (this._renderMode > 0)
        this._renderMode--;

      if (renderNow || this._renderMode == 0)
        this._tileManager.criticalRectPaint();
    },

    isRendering: function isRendering() {
      return (this._renderMode == 0);
    },

    /**
     * @param dx Guess delta to destination x coordinate
     * @param dy Guess delta to destination y coordinate
     */
    onBeforeVisibleMove: function onBeforeVisibleMove(dx, dy) {
      let vs = this._browserViewportState;
      let vr = this._visibleRect;

      let destCR = visibleRectToCriticalRect(vr.clone().translate(dx, dy), vs);

      this._tileManager.beginCriticalMove(destCR);
    },

    /**
     * @param dx Actual delta to destination x coordinate
     * @param dy Actual delta to destination y coordinate
     */
    onAfterVisibleMove: function onAfterVisibleMove(dx, dy) {
      let vs = this._browserViewportState;
      let vr = this._visibleRect;

      vr.translate(dx, dy);
      vs.visibleX = vr.left;
      vs.visibleY = vr.top;

      let cr = visibleRectToCriticalRect(vr, vs);

      this._tileManager.endCriticalMove(cr, this.isRendering());
    },

    setBrowser: function setBrowser(browser, skipZoom) {
      let currentBrowser = this._browser;

      let browserChanged = (currentBrowser !== browser);

      if (currentBrowser) {
        currentBrowser.removeEventListener("MozAfterPaint", this.handleMozAfterPaint, false);

        // !!! --- RESIZE HACK BEGIN -----
        // change to the real event type and perhaps refactor the handler function name
        currentBrowser.removeEventListener("FakeMozAfterSizeChange", this.handleMozAfterSizeChange, false);
        // !!! --- RESIZE HACK END -------

        this.discardAllBatchOperations();

        currentBrowser.setAttribute("type", "content");
        currentBrowser.docShell.isOffScreenBrowser = false;
      }

      this._restoreBrowser(browser);

      browser.setAttribute("type", "content-primary");

      this.beginBatchOperation();

      browser.addEventListener("MozAfterPaint", this.handleMozAfterPaint, false);

      // !!! --- RESIZE HACK BEGIN -----
      // change to the real event type and perhaps refactor the handler function name
      browser.addEventListener("FakeMozAfterSizeChange", this.handleMozAfterSizeChange, false);
      // !!! --- RESIZE HACK END -------

      if (!skipZoom) {
        browser.docShell.isOffScreenBrowser = true;
        this.zoomToPage();
      }

      this._viewportChanged(browserChanged, browserChanged);

      this.commitBatchOperation();
    },

    handleMozAfterPaint: function handleMozAfterPaint(ev) {
      let browser = this._browser;
      let tm = this._tileManager;
      let vs = this._browserViewportState;

      let [scrollX, scrollY] = getContentScrollValues(browser);
      let clientRects = ev.clientRects;

      // !!! --- RESIZE HACK BEGIN -----
      // remove this, cf explanation in loop below
      let hack = this._resizeHack;
      let hackSizeChanged = false;
      // !!! --- RESIZE HACK END -------

      let rects = [];
      // loop backwards to avoid xpconnect penalty for .length
      for (let i = clientRects.length - 1; i >= 0; --i) {
        let e = clientRects.item(i);
        let r = new wsRect(e.left + scrollX,
                           e.top + scrollY,
                           e.width, e.height);

        this.browserToViewportRect(r);
        r.round();

        if (r.right < 0 || r.bottom < 0)
          continue;

        // !!! --- RESIZE HACK BEGIN -----
        // remove this.  this is where we make 'lazy' calculations
        // that hint at a browser size change and fake the size change
        // event dispach
        if (r.right > hack.maxW) {
          hack.maxW = rect.right;
          hackSizeChanged = true;
        }
        if (r.bottom > hack.maxH) {
          hack.maxH = rect.bottom;
          hackSizeChanged = true;
        }
        // !!! --- RESIZE HACK END -------

        r.restrictTo(vs.viewportRect);
        rects.push(r);
      }

      // !!! --- RESIZE HACK BEGIN -----
      // remove this, cf explanation in loop above
      if (hackSizeChanged)
        simulateMozAfterSizeChange(browser, hack.maxW, hack.maxH);
      // !!! --- RESIZE HACK END -------

      tm.dirtyRects(rects, this.isRendering());
    },

    handleMozAfterSizeChange: function handleMozAfterPaint(ev) {
      // !!! --- RESIZE HACK BEGIN -----
      // get the correct properties off of the event, these are wrong because
      // we're using a MouseEvent since it has an X and Y prop of some sort and
      // we piggyback on that.
      let w = ev.screenX;
      let h = ev.screenY;
      // !!! --- RESIZE HACK END -------

      this.setViewportDimensions(w, h);
    },

    zoomToPage: function zoomToPage() {
      let browser = this._browser;

      if (!browser)
        return;

      let [w, h] = getBrowserDimensions(browser);
      this.setZoomLevel(pageZoomLevel(this._visibleRect, w, h));
    },

    zoom: function zoom(aDirection) {
      if (aDirection == 0)
        return;

      var zoomDelta = 0.05; // 1/20
      if (aDirection >= 0)
        zoomDelta *= -1;

      this.zoomLevel = this._zoomLevel + zoomDelta;
    },

    viewportToBrowser: function viewportToBrowser(x) {
      let bvs = this._browserViewportState;

      if (!bvs)
        throw "No browser is set";

      return x / bvs.zoomLevel;
    },

    browserToViewport: function browserToViewport(x) {
      let bvs = this._browserViewportState;

      if (!bvs)
        throw "No browser is set";

      return x * bvs.zoomLevel;
    },

    viewportToBrowserRect: function viewportToBrowserRect(rect) {
      let f = this.viewportToBrowser(1.0);
      return rect.scale(f, f);
    },

    browserToViewportRect: function browserToViewportRect(rect) {
      let f = this.browserToViewport(1.0);
      return rect.scale(f, f);
    },

    browserToViewportCanvasContext: function browserToViewportCanvasContext(ctx) {
      let f = this.browserToViewport(1.0);
      ctx.scale(f, f);
    },


    // -----------------------------------------------------------
    // Private instance methods
    //

    _restoreBrowser: function _restoreBrowser(browser) {
      let vr = this._visibleRect;

      if (!seenBrowser(browser))
        initBrowserState(browser, vr);

      let bvs = getViewportStateFromBrowser(browser);

      this._contentWindow = browser.contentWindow;
      this._browser = browser;
      this._browserViewportState = bvs;
      vr.left = bvs.visibleX;
      vr.top  = bvs.visibleY;
      this._tileManager.setBrowser(browser);
    },

    _viewportChanged: function _viewportChanged(viewportSizeChanged, dirtyAll) {
      let bops = this._batchOps;

      if (bops.length > 0) {
        let opState = bops[bops.length - 1];

        if (viewportSizeChanged)
          opState.viewportSizeChanged = viewportSizeChanged;

        if (dirtyAll)
          opState.dirtyAll = dirtyAll;

        return;
      }

      let bvs = this._browserViewportState;
      let vis = this._visibleRect;

      // !!! --- RESIZE HACK BEGIN -----
      // We want to uncomment this for perf, but we can't with the hack in place
      // because the mozAfterPaint gives us rects that we use to create the
      // fake mozAfterResize event, so we can't just clear things.
      /*
      if (dirtyAll) {
        // We're about to mark the entire viewport dirty, so we can clear any
        // queued afterPaint events that will cause redundant draws
        getBrowserDOMWindowUtils(this._browser).clearMozAfterPaintEvents();
      }
      */
      // !!! --- RESIZE HACK END -------

      if (bvs) {
        resizeContainerToViewport(this._container, bvs.viewportRect);

        this._tileManager.viewportChangeHandler(bvs.viewportRect,
                                                visibleRectToCriticalRect(vis, bvs),
                                                viewportSizeChanged,
                                                dirtyAll);
      }
    },

    _appendTile: function _appendTile(tile) {
      let canvas = tile.getContentImage();

      /*
      canvas.style.position = "absolute";
      canvas.style.left = tile.x + "px";
      canvas.style.top  = tile.y + "px";
      */

      canvas.setAttribute("style", "position: absolute; left: " + tile.boundRect.left + "px; " + "top: " + tile.boundRect.top + "px;");

      this._container.appendChild(canvas);

      //dump('++ ' + tile.toString(true) + endl);
    },

    _removeTile: function _removeTile(tile) {
      let canvas = tile.getContentImage();

      this._container.removeChild(canvas);

      //dump('-- ' + tile.toString(true) + endl);
    }

  };

}
)();


// -----------------------------------------------------------
// Helper structures
//

BrowserView.BrowserViewportState = function(viewportRect,
                                            visibleX,
                                            visibleY,
                                            zoomLevel) {

  this.init(viewportRect, visibleX, visibleY, zoomLevel);
};

BrowserView.BrowserViewportState.prototype = {

  init: function init(viewportRect, visibleX, visibleY, zoomLevel) {
    this.viewportRect = viewportRect;
    this.visibleX     = visibleX;
    this.visibleY     = visibleY;
    this.zoomLevel    = zoomLevel;
  },

  clone: function clone() {
    return new BrowserView.BrowserViewportState(this.viewportRect,
                                                this.visibleX,
                                                this.visibleY,
						                                    this.zoomLevel);
  },

  toString: function toString() {
    let props = ['\tviewportRect=' + this.viewportRect.toString(),
                 '\tvisibleX='     + this.visibleX,
                 '\tvisibleY='     + this.visibleY,
                 '\tzoomLevel='    + this.zoomLevel];

    return '[BrowserViewportState] {\n' + props.join(',\n') + '\n}';
  }

};

