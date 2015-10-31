/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Common thumbnailing routines used by various consumers, including
 * PageThumbs and backgroundPageThumbsContent.
 */

this.EXPORTED_SYMBOLS = ["PageThumbUtils"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Promise.jsm", this);
Cu.import("resource://gre/modules/AppConstants.jsm");

this.PageThumbUtils = {
  // The default background color for page thumbnails.
  THUMBNAIL_BG_COLOR: "#fff",
  // The namespace for thumbnail canvas elements.
  HTML_NAMESPACE: "http://www.w3.org/1999/xhtml",

  /**
   * Creates a new canvas element in the context of aWindow, or if aWindow
   * is undefined, in the context of hiddenDOMWindow.
   *
   * @param aWindow (optional) The document of this window will be used to
   *  create the canvas.  If not given, the hidden window will be used.
   * @param aWidth (optional) width of the canvas to create
   * @param aHeight (optional) height of the canvas to create
   * @return The newly created canvas.
   */
  createCanvas: function (aWindow, aWidth = 0, aHeight = 0) {
    let doc = (aWindow || Services.appShell.hiddenDOMWindow).document;
    let canvas = doc.createElementNS(this.HTML_NAMESPACE, "canvas");
    canvas.mozOpaque = true;
    canvas.mozImageSmoothingEnabled = true;
    let [thumbnailWidth, thumbnailHeight] = this.getThumbnailSize(aWindow);
    canvas.width = aWidth ? aWidth : thumbnailWidth;
    canvas.height = aHeight ? aHeight : thumbnailHeight;
    return canvas;
  },

  /**
   * Calculates a preferred initial thumbnail size based based on newtab.css
   * sizes or a preference for other applications. The sizes should be the same
   * as set for the tile sizes in newtab.
   *
   * @param aWindow (optional) aWindow that is used to calculate the scaling size.
   * @return The calculated thumbnail size or a default if unable to calculate.
   */
  getThumbnailSize: function (aWindow = null) {
    if (!this._thumbnailWidth || !this._thumbnailHeight) {
      let screenManager = Cc["@mozilla.org/gfx/screenmanager;1"]
                            .getService(Ci.nsIScreenManager);
      let left = {}, top = {}, screenWidth = {}, screenHeight = {};
      screenManager.primaryScreen.GetRectDisplayPix(left, top, screenWidth, screenHeight);

      /***
       * The system default scale might be different than
       * what is reported by the window. For example,
       * retina displays have 1:1 system scales, but 2:1 window
       * scale as 1 pixel system wide == 2 device pixels.
       * To get the best image quality, query both and take the highest one.
       */
      let systemScale = screenManager.systemDefaultScale;
      let windowScale = aWindow ? aWindow.devicePixelRatio : systemScale;
      let scale = Math.max(systemScale, windowScale);

      /***
       * On retina displays, we can sometimes go down this path
       * without a window object. In those cases, force 2x scaling
       * as the system scale doesn't represent the 2x scaling
       * on OS X.
       */
      if (AppConstants.platform == "macosx" && !aWindow) {
        scale = 2;
      }

      /***
       * THESE VALUES ARE DEFINED IN newtab.css and hard coded.
       * If you change these values from the prefs,
       * ALSO CHANGE THEM IN newtab.css
       */
      let prefWidth = Services.prefs.getIntPref("toolkit.pageThumbs.minWidth");
      let prefHeight = Services.prefs.getIntPref("toolkit.pageThumbs.minHeight");
      let divisor = Services.prefs.getIntPref("toolkit.pageThumbs.screenSizeDivisor");

      prefWidth *= scale;
      prefHeight *= scale;

      this._thumbnailWidth = Math.max(Math.round(screenWidth.value / divisor), prefWidth);;
      this._thumbnailHeight = Math.max(Math.round(screenHeight.value / divisor), prefHeight);
    }

    return [this._thumbnailWidth, this._thumbnailHeight];
  },

  /***
   * Given a browser window, return the size of the content
   * minus the scroll bars.
   */
  getContentSize: function(aWindow) {
    let utils = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIDOMWindowUtils);
    // aWindow may be a cpow, add exposed props security values.
    let sbWidth = {}, sbHeight = {};

    try {
      utils.getScrollbarSize(false, sbWidth, sbHeight);
    } catch (e) {
      // This might fail if the window does not have a presShell.
      Cu.reportError("Unable to get scrollbar size in determineCropSize.");
      sbWidth.value = sbHeight.value = 0;
    }

    // Even in RTL mode, scrollbars are always on the right.
    // So there's no need to determine a left offset.
    let width = aWindow.innerWidth - sbWidth.value;
    let height = aWindow.innerHeight - sbHeight.value;

    return [width, height];
  },

  /***
   * Given a browser window, this creates a snapshot of the content
   * and returns a canvas with the resulting snapshot of the content
   * at the thumbnail size. It has to do this through a two step process:
   *
   * 1) Render the content at the window size to a canvas that is 2x the thumbnail size
   * 2) Downscale the canvas from (1) down to the thumbnail size
   *
   * This is because the thumbnail size is too small to render at directly,
   * causing pages to believe the browser is a small resolution. Also,
   * at that resolution, graphical artifacts / text become very jagged.
   * It's actually better to the eye to have small blurry text than sharp
   * jagged pixels to represent text.
   *
   * @params aWindow - the window to create a snapshot of.
   * @params aDestCanvas (optional) a destination canvas to draw the final snapshot to.
   * @return Canvas with a scaled thumbnail of the window.
   */
  createSnapshotThumbnail: function(aWindow, aDestCanvas = null) {
    if (Cu.isCrossProcessWrapper(aWindow)) {
      throw new Error('Do not pass cpows here.');
    }

    let [contentWidth, contentHeight] = this.getContentSize(aWindow);
    let [thumbnailWidth, thumbnailHeight] = aDestCanvas ?
                                            [aDestCanvas.width, aDestCanvas.height] :
                                            this.getThumbnailSize(aWindow);
    let intermediateWidth = thumbnailWidth * 2;
    let intermediateHeight = thumbnailHeight * 2;
    let skipDownscale = false;
    let snapshotCanvas = undefined;

    // Our intermediate thumbnail is bigger than content,
    // which can happen on hiDPI devices like a retina macbook pro.
    // In those cases, just render at the final size.
    if ((intermediateWidth >= contentWidth) ||
        (intermediateHeight >= contentHeight)) {
      intermediateWidth = thumbnailWidth;
      intermediateHeight = thumbnailHeight;
      skipDownscale = true;
      snapshotCanvas = aDestCanvas;
    }

    // If we've been given a large preallocated canvas, so
    // just render once into the destination canvas.
    if (aDestCanvas &&
        ((aDestCanvas.width >= intermediateWidth) ||
        (aDestCanvas.height >= intermediateHeight))) {
      intermediateWidth = aDestCanvas.width;
      intermediateHeight = aDestCanvas.height;
      skipDownscale = true;
      snapshotCanvas = aDestCanvas;
    }

    if (!snapshotCanvas) {
      snapshotCanvas = this.createCanvas(aWindow, intermediateWidth, intermediateHeight);
    }

    // This is step 1.
    // Also by default, canvas does not draw the scrollbars, so no need to
    // remove the scrollbar sizes.
    let scale = Math.min(Math.max(intermediateWidth / contentWidth,
                                  intermediateHeight / contentHeight), 1);
    let snapshotCtx = snapshotCanvas.getContext("2d");
    snapshotCtx.save();
    snapshotCtx.scale(scale, scale);
    snapshotCtx.drawWindow(aWindow, 0, 0, contentWidth, contentHeight,
                           PageThumbUtils.THUMBNAIL_BG_COLOR,
                           snapshotCtx.DRAWWINDOW_DO_NOT_FLUSH);
    snapshotCtx.restore();
    if (skipDownscale) {
      return snapshotCanvas;
    }

    // Part 2: Assumes that the snapshot is 2x the thumbnail size
    let finalCanvas = aDestCanvas || this.createCanvas(aWindow, thumbnailWidth, thumbnailHeight);

    let finalCtx = finalCanvas.getContext("2d");
    finalCtx.save();
    finalCtx.scale(0.5, 0.5);
    finalCtx.drawImage(snapshotCanvas, 0, 0);
    finalCtx.restore();
    return finalCanvas;
  },

  /**
   * Determine a good thumbnail crop size and scale for a given content
   * window.
   *
   * @param aWindow The content window.
   * @param aCanvas The target canvas.
   * @return An array containing width, height and scale.
   */
  determineCropSize: function (aWindow, aCanvas) {
    if (Cu.isCrossProcessWrapper(aWindow)) {
      throw new Error('Do not pass cpows here.');
    }
    let utils = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIDOMWindowUtils);
    // aWindow may be a cpow, add exposed props security values.
    let sbWidth = {}, sbHeight = {};

    try {
      utils.getScrollbarSize(false, sbWidth, sbHeight);
    } catch (e) {
      // This might fail if the window does not have a presShell.
      Cu.reportError("Unable to get scrollbar size in determineCropSize.");
      sbWidth.value = sbHeight.value = 0;
    }

    // Even in RTL mode, scrollbars are always on the right.
    // So there's no need to determine a left offset.
    let width = aWindow.innerWidth - sbWidth.value;
    let height = aWindow.innerHeight - sbHeight.value;

    let {width: thumbnailWidth, height: thumbnailHeight} = aCanvas;
    let scale = Math.min(Math.max(thumbnailWidth / width, thumbnailHeight / height), 1);
    let scaledWidth = width * scale;
    let scaledHeight = height * scale;

    if (scaledHeight > thumbnailHeight)
      height -= Math.floor(Math.abs(scaledHeight - thumbnailHeight) * scale);

    if (scaledWidth > thumbnailWidth)
      width -= Math.floor(Math.abs(scaledWidth - thumbnailWidth) * scale);

    return [width, height, scale];
  },

  shouldStoreContentThumbnail: function (aDocument, aDocShell) {
    // FIXME Bug 720575 - Don't capture thumbnails for SVG or XML documents as
    //       that currently regresses Talos SVG tests.
    if (aDocument instanceof Ci.nsIDOMXMLDocument) {
      return false;
    }

    let webNav = aDocShell.QueryInterface(Ci.nsIWebNavigation);

    // Don't take screenshots of about: pages.
    if (webNav.currentURI.schemeIs("about")) {
      return false;
    }

    // There's no point in taking screenshot of loading pages.
    if (aDocShell.busyFlags != Ci.nsIDocShell.BUSY_FLAGS_NONE) {
      return false;
    }

    let channel = aDocShell.currentDocumentChannel;

    // No valid document channel. We shouldn't take a screenshot.
    if (!channel) {
      return false;
    }

    // Don't take screenshots of internally redirecting about: pages.
    // This includes error pages.
    let uri = channel.originalURI;
    if (uri.schemeIs("about")) {
      return false;
    }

    let httpChannel;
    try {
      httpChannel = channel.QueryInterface(Ci.nsIHttpChannel);
    } catch (e) { /* Not an HTTP channel. */ }

    if (httpChannel) {
      // Continue only if we have a 2xx status code.
      try {
        if (Math.floor(httpChannel.responseStatus / 100) != 2) {
        return false;
        }
      } catch (e) {
        // Can't get response information from the httpChannel
        // because mResponseHead is not available.
        return false;
      }

      // Cache-Control: no-store.
      if (httpChannel.isNoStoreResponse()) {
        return false;
      }

      // Don't capture HTTPS pages unless the user explicitly enabled it.
      if (uri.schemeIs("https") &&
          !Services.prefs.getBoolPref("browser.cache.disk_cache_ssl")) {
        return false;
      }
    } // httpChannel
    return true;
  }
};
