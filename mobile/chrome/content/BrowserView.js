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
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roy Frostig <rfrostig@mozilla.com>
 *   Stuart Parmenter <stuart@mozilla.com>
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

let Ci = Components.interfaces;

const kBrowserViewZoomLevelMin = 0.2;
const kBrowserViewZoomLevelMax = 4.0;
const kBrowserViewZoomLevelPrecision = 10000;


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
 * hopefully no one but us will touch.  See BrowserView.Util.getViewportStateFromBrowser()
 * for the property name.
 */
function BrowserView(container, visibleRectFactory) {
  Util.bindAll(this);
  this.init(container, visibleRectFactory);
}


// -----------------------------------------------------------
// Util/convenience functions.
//
// These are mostly for use by BrowserView itself, but if you find them handy anywhere
// else, feel free.
//

BrowserView.Util = {
  visibleRectToCriticalRect: function visibleRectToCriticalRect(visibleRect, browserViewportState) {
    return visibleRect.intersect(browserViewportState.viewportRect);
  },

  clampZoomLevel: function clampZoomLevel(zl) {
    let bounded = Math.min(Math.max(kBrowserViewZoomLevelMin, zl), kBrowserViewZoomLevelMax);
    let rounded = Math.round(bounded * kBrowserViewZoomLevelPrecision) / kBrowserViewZoomLevelPrecision;
    return (rounded) ? rounded : 1.0;
  },

  pageZoomLevel: function pageZoomLevel(visibleRect, browserW, browserH) {
    return BrowserView.Util.clampZoomLevel(visibleRect.width / browserW);
  },

  createBrowserViewportState: function createBrowserViewportState() {
    return new BrowserView.BrowserViewportState(new wsRect(0, 0, 1, 1), 0, 0, 1);
  },

  getViewportStateFromBrowser: function getViewportStateFromBrowser(browser) {
    return browser.__BrowserView__vps;
  },

  getBrowserDimensions: function getBrowserDimensions(browser) {
    let cdoc = browser.contentDocument;
    if (cdoc instanceof SVGDocument) {
      let rect = cdoc.rootElement.getBoundingClientRect();
      return [Math.ceil(rect.width), Math.ceil(rect.height)];
    }

    // These might not exist yet depending on page load state
    let body = cdoc.body || {};
    let html = cdoc.documentElement || {};
    let w = Math.max(body.scrollWidth || 1, html.scrollWidth);
    let h = Math.max(body.scrollHeight || 1, html.scrollHeight);

    return [w, h];
  },

  getContentScrollValues: function getContentScrollValues(browser) {
    let cwu = BrowserView.Util.getBrowserDOMWindowUtils(browser);
    let scrollX = {};
    let scrollY = {};
    cwu.getScrollXY(false, scrollX, scrollY);

    return [scrollX.value, scrollY.value];
  },

  getBrowserDOMWindowUtils: function getBrowserDOMWindowUtils(browser) {
    return browser.contentWindow
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIDOMWindowUtils);
  },

  getNewBatchOperationState: function getNewBatchOperationState() {
    return {
      viewportSizeChanged: false,
      dirtyAll: false
    };
  },

  initContainer: function initContainer(container, visibleRect) {
    container.style.width    = visibleRect.width  + 'px';
    container.style.height   = visibleRect.height + 'px';
    container.style.overflow = '-moz-hidden-unscrollable';
  },

  resizeContainerToViewport: function resizeContainerToViewport(container, viewportRect) {
    container.style.width  = viewportRect.width  + 'px';
    container.style.height = viewportRect.height + 'px';
  }
};


BrowserView.prototype = {

  // -----------------------------------------------------------
  // Public instance methods
  //

  init: function init(container, visibleRectFactory) {
    this._batchOps = [];
    this._container = container;
    this._browser = null;
    this._browserViewportState = null;
    this._contentWindow = null;
    this._renderMode = 0;
    this._tileManager = new TileManager(this._appendTile, this._removeTile, this);
    this._visibleRectFactory = visibleRectFactory;
  },

  getVisibleRect: function getVisibleRect() {
    return this._visibleRectFactory();
  },

  setViewportDimensions: function setViewportDimensions(width, height, causedByZoom) {
    let bvs = this._browserViewportState;

    if (!bvs)
      return;

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

    let newZL = BrowserView.Util.clampZoomLevel(zl);

    if (newZL != bvs.zoomLevel) {
      let browserW = this.viewportToBrowser(bvs.viewportRect.right);
      let browserH = this.viewportToBrowser(bvs.viewportRect.bottom);
      bvs.zoomLevel = newZL; // side-effect: now scale factor in transformations is newZL
      this.setViewportDimensions(this.browserToViewport(browserW),
                                 this.browserToViewport(browserH),
                                 true);
      this.zoomChanged = true;
    }
  },

  getZoomLevel: function getZoomLevel() {
    let bvs = this._browserViewportState;
    if (!bvs)
      return undefined;

    return bvs.zoomLevel;
  },

  beginBatchOperation: function beginBatchOperation() {
    this._batchOps.push(BrowserView.Util.getNewBatchOperationState());
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
      this.renderNow();
  },

  /**
   * Called while rendering is paused to allow update of critical area
   */
  renderNow: function renderNow() {
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
    let vr = this.getVisibleRect();

    let destCR = BrowserView.Util.visibleRectToCriticalRect(vr.translate(dx, dy), vs);

    this._tileManager.beginCriticalMove(destCR);
  },

  onAfterVisibleMove: function onAfterVisibleMove() {
    let vs = this._browserViewportState;
    let vr = this.getVisibleRect();

    vs.visibleX = vr.left;
    vs.visibleY = vr.top;

    let cr = BrowserView.Util.visibleRectToCriticalRect(vr, vs);

    this._tileManager.endCriticalMove(cr, this.isRendering());
  },

  /**
   * Swap out the current browser and browser viewport state with a new pair.
   */
  setBrowser: function setBrowser(browser, browserViewportState, doZoom) {
    if (browser && !browserViewportState) {
      throw "Cannot set non-null browser with null BrowserViewportState";
    }

    let currentBrowser = this._browser;

    let browserChanged = (currentBrowser !== browser);

    if (currentBrowser) {
      currentBrowser.removeEventListener("MozAfterPaint", this.handleMozAfterPaint, false);
      currentBrowser.removeEventListener("scroll", this.handlePageScroll, false);

      // !!! --- RESIZE HACK BEGIN -----
      // change to the real event type and perhaps refactor the handler function name
      currentBrowser.removeEventListener("FakeMozAfterSizeChange", this.handleMozAfterSizeChange, false);
      // !!! --- RESIZE HACK END -------

      currentBrowser.setAttribute("type", "content");
      currentBrowser.docShell.isOffScreenBrowser = false;
    }

    this._browser = browser;
    this._contentWindow = (browser) ? browser.contentWindow : null;
    this._browserViewportState = browserViewportState;

    if (browser) {
      browser.setAttribute("type", "content-primary");

      this.beginBatchOperation();

      browser.addEventListener("MozAfterPaint", this.handleMozAfterPaint, false);
      browser.addEventListener("scroll", this.handlePageScroll, false);

      // !!! --- RESIZE HACK BEGIN -----
      // change to the real event type and perhaps refactor the handler function name
      browser.addEventListener("FakeMozAfterSizeChange", this.handleMozAfterSizeChange, false);
      // !!! --- RESIZE HACK END -------

      if (doZoom) {
        browser.docShell.isOffScreenBrowser = true;
        this.zoomToPage();
      }

      this._viewportChanged(browserChanged, browserChanged);

      this.commitBatchOperation();
    }
  },

  getBrowser: function getBrowser() {
    return this._browser;
  },

  handleMozAfterPaint: function handleMozAfterPaint(ev) {
    let browser = this._browser;
    let tm = this._tileManager;
    let vs = this._browserViewportState;

    let [scrollX, scrollY] = BrowserView.Util.getContentScrollValues(browser);
    let clientRects = ev.clientRects;

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

      try {
        r.restrictTo(vs.viewportRect);
        rects.push(r);
      } catch(ex) { /* dump("fail\n"); */ }
    }

    tm.dirtyRects(rects, this.isRendering());
  },

  handlePageScroll: function handlePageScroll(aEvent) {
    if (aEvent.target != this._browser.contentDocument)
      return;

    let [scrollX, scrollY] = BrowserView.Util.getContentScrollValues(this._browser);
    Browser.contentScrollboxScroller.scrollTo(this.browserToViewport(scrollX), 
                                              this.browserToViewport(scrollY));
    this.onAfterVisibleMove();
  },

  // !!! --- RESIZE HACK BEGIN -----
  simulateMozAfterSizeChange: function simulateMozAfterSizeChange() {
    let [w, h] = BrowserView.Util.getBrowserDimensions(this._browser);
    let ev = document.createEvent("MouseEvents");
    ev.initMouseEvent("FakeMozAfterSizeChange", false, false, window, 0, w, h, 0, 0, false, false, false, false, 0, null);
    this._browser.dispatchEvent(ev);
  },
  // !!! --- RESIZE HACK END -------

  handleMozAfterSizeChange: function handleMozAfterSizeChange(ev) {
    // !!! --- RESIZE HACK BEGIN -----
    // get the correct properties off of the event, these are wrong because
    // we're using a MouseEvent, as it has an X and Y prop of some sort and
    // we piggyback on that.
    let w = ev.screenX;
    let h = ev.screenY;
    // !!! --- RESIZE HACK END -------
    this.setViewportDimensions(this.browserToViewport(w), this.browserToViewport(h));
  },

  zoomToPage: function zoomToPage() {
    let browser = this._browser;

    if (!browser)
      return;

    var windowUtils = browser.contentWindow
                             .QueryInterface(Ci.nsIInterfaceRequestor)
                             .getInterface(Ci.nsIDOMWindowUtils);
    var handheldFriendly = windowUtils.getDocumentMetadata("HandheldFriendly");
    
    if (handheldFriendly == "true") {
      browser.handheld = true;
      browser.setAttribute("style", "width: " + window.screen.width + "px;");
      this.setZoomLevel(1);
      browser.markupDocumentViewer.textZoom = 1;
    } else {
      delete  browser.handheld;
      let [w, h] = BrowserView.Util.getBrowserDimensions(browser);
      this.setZoomLevel(BrowserView.Util.pageZoomLevel(this.getVisibleRect(), w, h));
    }
  },

  zoom: function zoom(aDirection) {
    let bvs = this._browserViewportState;

    if (!bvs)
      throw "No browser is set";

    if (aDirection == 0)
      return;

    var zoomDelta = 0.05; // 1/20
    if (aDirection >= 0)
      zoomDelta *= -1;

    this.setZoomLevel(bvs.zoomLevel + zoomDelta);
  },

  /**
   * Render a rectangle within the browser viewport to the destination canvas
   * under the given scale.
   *
   * @param destCanvas The destination canvas into which the image is rendered.
   * @param destWidth Destination width
   * @param destHeight Destination height
   * @param srcRect [optional] The source rectangle in BrowserView coordinates.
   * This defaults to the visible rect rooted at the x,y of the critical rect.
   */
  renderToCanvas: function renderToCanvas(destCanvas, destWidth, destHeight, srcRect) {
    let bvs = this._browserViewportState;
    if (!bvs) {
      throw "Browser viewport state null in call to renderToCanvas (probably no browser set on BrowserView).";
    }

    if (!srcRect) {
      let vr = this.getVisibleRect();
      let cr = BrowserView.Util.visibleRectToCriticalRect(vr, bvs);
      vr.x = cr.left;
      vr.y = cr.top;
      srcRect = vr;
    }

    let scalex = (destWidth / srcRect.width) || 1;
    let scaley = (destHeight / srcRect.height) || 1;

    srcRect.restrictTo(bvs.viewportRect);
    this._tileManager.renderRectToCanvas(srcRect, destCanvas, scalex, scaley);
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

  forceContainerResize: function forceContainerResize() {
    let bvs = this._browserViewportState;
    if (bvs)
      BrowserView.Util.resizeContainerToViewport(this._container, bvs.viewportRect);
  },

  // -----------------------------------------------------------
  // Private instance methods
  //

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

    if (bvs) {
      BrowserView.Util.resizeContainerToViewport(this._container, bvs.viewportRect);

      if (dirtyAll) {
        // We're about to mark the entire viewport dirty, so we can clear any
        // queued afterPaint events that will cause redundant draws
        BrowserView.Util.getBrowserDOMWindowUtils(this._browser).clearMozAfterPaintEvents();
      }

      this._tileManager.viewportChangeHandler(bvs.viewportRect,
                                              BrowserView.Util.visibleRectToCriticalRect(this.getVisibleRect(), bvs),
                                              viewportSizeChanged,
                                              dirtyAll);
    }
  },

  _appendTile: function _appendTile(tile) {
    let canvas = tile.getContentImage();

    //canvas.style.position = "absolute";
    //canvas.style.left = tile.x + "px";
    //canvas.style.top  = tile.y + "px";

    // XXX The above causes a trace abort, and this function is called back in the tight
    // render-heavy loop in TileManager, so even though what we do below isn't so proper
    // and takes longer on the Platform/C++ emd, it's better than causing a trace abort
    // in our tight loop.
    //
    // But this also overwrites some style already set on the canvas in Tile constructor.
    // Hack fail...
    //
    canvas.setAttribute("style", "position: absolute; left: " + tile.boundRect.left + "px; " + "top: " + tile.boundRect.top + "px;");

    this._container.appendChild(canvas);
  },

  _removeTile: function _removeTile(tile) {
    let canvas = tile.getContentImage();

    this._container.removeChild(canvas);
  }

};


// -----------------------------------------------------------
// Helper structures
//

/**
 * A BrowserViewportState maintains viewport state information that is unique to each
 * browser.  It does not hold *all* viewport state maintained by BrowserView.  For
 * instance, it does not maintain width and height of the visible rectangle (but it
 * does keep the top and left coordinates (cf visibleX, visibleY)), since those are not
 * characteristic of the current browser in view.
 */
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
    this.zoomChanged  = false;
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

