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
 *   Matt Brubeck <mbrubeck@mozilla.com>
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

const kBrowserFormZoomLevelMin = 1.0;
const kBrowserFormZoomLevelMax = 2.0;
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
 *     region of the viewport which is visible to the user.
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

  createBrowserViewportState: function createBrowserViewportState() {
    return new BrowserView.BrowserViewportState(new Rect(0, 0, 800, 800), 0, 0, 1);
  },

  ensureMozScrolledAreaEvent: function ensureMozScrolledAreaEvent(aBrowser, aWidth, aHeight) {
    let message = {};
    message.target = aBrowser;
    message.name = "Browser:MozScrolledAreaChanged";
    message.json = { width: aWidth, height: aHeight };

    Browser._browserView.updateScrolledArea(message);
  }
};

BrowserView.prototype = {

  // -----------------------------------------------------------
  // Public instance methods
  //

  init: function init(container, visibleRectFactory) {
    this._container = container;
    this._browser = null;
    this._browserViewportState = null;
    this._visibleRectFactory = visibleRectFactory;
    messageManager.addMessageListener("Browser:MozScrolledAreaChanged", this);
  },

  uninit: function uninit() {
  },

  getVisibleRect: function getVisibleRect() {
    return this._visibleRectFactory();
  },

  getCriticalRect: function getCriticalRect() {
    let bvs = this._browserViewportState;
    let vr = this.getVisibleRect();
    return BrowserView.Util.visibleRectToCriticalRect(vr, bvs);
  },

  /**
   * @return [width, height]
   */
  getViewportDimensions: function getViewportDimensions() {
    let bvs = this._browserViewportState;
    if (!bvs)
      throw "Cannot get viewport dimensions when no browser is set";

    return [bvs.viewportRect.right, bvs.viewportRect.bottom];
  },

  setZoomLevel: function setZoomLevel(zoomLevel) {
    return;

    let bvs = this._browserViewportState;
    if (!bvs)
      return;

    let newZoomLevel = this.clampZoomLevel(zoomLevel);
    if (newZoomLevel != bvs.zoomLevel) {
      let browserW = this.viewportToBrowser(bvs.viewportRect.right);
      let browserH = this.viewportToBrowser(bvs.viewportRect.bottom);
      bvs.zoomLevel = newZoomLevel; // side-effect: now scale factor in transformations is newZoomLevel
      bvs.viewportRect.right  = this.browserToViewport(browserW);
      bvs.viewportRect.bottom = this.browserToViewport(browserH);
      this._viewportChanged();

      if (this._browser) {
        let event = document.createEvent("Events");
        event.initEvent("ZoomChanged", true, false);
        this._browser.dispatchEvent(event);
      }
    }
  },

  getZoomLevel: function getZoomLevel() {
    let bvs = this._browserViewportState;
    if (!bvs)
      return undefined;

    return bvs.zoomLevel;
  },

  clampZoomLevel: function clampZoomLevel(zl) {
    let bounded = Math.min(Math.max(ZoomManager.MIN, zl), ZoomManager.MAX);

    let bvs = this._browserViewportState;
    if (bvs) {
      let md = bvs.metaData;
      if (md && md.minZoom)
        bounded = Math.max(bounded, md.minZoom);
      if (md && md.maxZoom)
        bounded = Math.min(bounded, md.maxZoom);

      bounded = Math.max(bounded, this.getPageZoomLevel());
    }

    let rounded = Math.round(bounded * kBrowserViewZoomLevelPrecision) / kBrowserViewZoomLevelPrecision;
    return rounded || 1.0;
  },

  /**
   * Swap out the current browser and browser viewport state with a new pair.
   */
  setBrowser: function setBrowser(browser, browserViewportState) {
    if (browser && !browserViewportState) {
      throw "Cannot set non-null browser with null BrowserViewportState";
    }

    let oldBrowser = this._browser;
    let browserChanged = (oldBrowser !== browser);

    if (oldBrowser) {
      oldBrowser.setAttribute("type", "content");
      oldBrowser.setAttribute("style", "display: none;");
      oldBrowser.messageManager.sendAsyncMessage("Browser:Blur", {});
    }

    this._browser = browser;
    this._browserViewportState = browserViewportState;

    if (browser) {
      browser.setAttribute("type", "content-primary");
      browser.setAttribute("style", "display: block;");
      browser.messageManager.sendAsyncMessage("Browser:Focus", {});
    }
  },

  getBrowser: function getBrowser() {
    return this._browser;
  },

  receiveMessage: function receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "Browser:MozScrolledAreaChanged":
        this.updateScrolledArea(aMessage);
        break;
    }
  },

  updateScrolledArea: function updateScrolledArea(aMessage) {
    let browser = aMessage.target;
    let tab = Browser.getTabForBrowser(browser);
    if (!browser || !tab)
      return;

    let json = aMessage.json;
    let bvs = tab.browserViewportState;

    let vis = this.getVisibleRect();
    let viewport = bvs.viewportRect;
    let oldRight = viewport.right;
    let oldBottom = viewport.bottom;
    viewport.right  = bvs.zoomLevel * json.width;
    viewport.bottom = bvs.zoomLevel * json.height;

    if (browser == this._browser) {
      this._viewportChanged();
      this.updateDefaultZoom();
    }
  },

  /** Call when default zoom level may change. */
  updateDefaultZoom: function updateDefaultZoom() {
    let bvs = this._browserViewportState;
    if (!bvs)
      return false;

    let isDefault = (bvs.zoomLevel == bvs.defaultZoomLevel);
    bvs.defaultZoomLevel = this.getDefaultZoomLevel();
    if (isDefault)
      this.setZoomLevel(bvs.defaultZoomLevel);
    return isDefault;
  },

  isDefaultZoom: function isDefaultZoom() {
    let bvs = this._browserViewportState;
    if (!bvs)
      return true;
    return bvs.zoomLevel == bvs.defaultZoomLevel;
  },

  getDefaultZoomLevel: function getDefaultZoomLevel() {
    let bvs = this._browserViewportState;
    if (!bvs)
      return 0;

    let md = bvs.metaData;
    if (md && md.defaultZoom)
      return this.clampZoomLevel(md.defaultZoom);

    let pageZoom = this.getPageZoomLevel();

    // If pageZoom is "almost" 100%, zoom in to exactly 100% (bug 454456).
    let granularity = Services.prefs.getIntPref("browser.ui.zoom.pageFitGranularity");
    let threshold = 1 - 1 / granularity;
    if (threshold < pageZoom && pageZoom < 1)
      pageZoom = 1;

    return this.clampZoomLevel(pageZoom);
  },

  getPageZoomLevel: function getPageZoomLevel() {
    let bvs = this._browserViewportState;  // browser exists, so bvs must as well

    // for xul pages, bvs.viewportRect.right can be 0
    let browserW = this.viewportToBrowser(bvs.viewportRect.right) || 1.0;
    return this.getVisibleRect().width / browserW;
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

  get allowZoom() {
    let bvs = this._browserViewportState;
    if (!bvs || !bvs.metaData)
      return true;
    return bvs.metaData.allowZoom;
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
    return;

    let bvs = this._browserViewportState;
    if (!bvs) {
      throw "Browser viewport state null in call to renderToCanvas (probably no browser set on BrowserView).";
    }

    if (!srcRect) {
      let vr = this.getVisibleRect();
      vr.x = bvs.viewportRect.left;
      vr.y = bvs.viewportRect.top;
      srcRect = vr;
    }

    let scalex = (destWidth / srcRect.width) || 1;
    let scaley = (destHeight / srcRect.height) || 1;

    srcRect.restrictTo(bvs.viewportRect);
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

  _viewportChanged: function() {
    getBrowser().style.MozTransformOrigin = "left top";
    Browser.contentScrollboxScroller.updateTransition();
  },
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
BrowserView.BrowserViewportState = function(viewportRect, visibleX, visibleY, zoomLevel) {
  this.init(viewportRect, visibleX, visibleY, zoomLevel);
};

BrowserView.BrowserViewportState.prototype = {

  init: function init(viewportRect, visibleX, visibleY, zoomLevel) {
    this.viewportRect = viewportRect;
    this.visibleX     = visibleX;
    this.visibleY     = visibleY;
    this.zoomLevel    = zoomLevel;
    this.defaultZoomLevel = 1;
  },

  toString: function toString() {
    let props = ["\tviewportRect=" + this.viewportRect.toString(),
                 "\tvisibleX="     + this.visibleX,
                 "\tvisibleY="     + this.visibleY,
                 "\tzoomLevel="    + this.zoomLevel];

    return "[BrowserViewportState] {\n" + props.join(",\n") + "\n}";
  }
};
