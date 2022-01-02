/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Common thumbnailing routines used by various consumers, including
 * PageThumbs and BackgroundPageThumbs.
 */

var EXPORTED_SYMBOLS = ["PageThumbUtils"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "BrowserUtils",
  "resource://gre/modules/BrowserUtils.jsm"
);

var PageThumbUtils = {
  // The default thumbnail size for images
  THUMBNAIL_DEFAULT_SIZE: 448,
  // The default background color for page thumbnails.
  THUMBNAIL_BG_COLOR: "#fff",
  // The namespace for thumbnail canvas elements.
  HTML_NAMESPACE: "http://www.w3.org/1999/xhtml",

  /**
   * Creates a new canvas element in the context of aWindow.
   *
   * @param aWindow The document of this window will be used to
   *  create the canvas.
   * @param aWidth (optional) width of the canvas to create
   * @param aHeight (optional) height of the canvas to create
   * @return The newly created canvas.
   */
  createCanvas(aWindow, aWidth = 0, aHeight = 0) {
    let doc = aWindow.document;
    let canvas = doc.createElementNS(this.HTML_NAMESPACE, "canvas");
    canvas.mozOpaque = true;
    canvas.imageSmoothingEnabled = true;
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
  getThumbnailSize(aWindow = null) {
    if (!this._thumbnailWidth || !this._thumbnailHeight) {
      let screenManager = Cc["@mozilla.org/gfx/screenmanager;1"].getService(
        Ci.nsIScreenManager
      );
      let left = {},
        top = {},
        screenWidth = {},
        screenHeight = {};
      screenManager.primaryScreen.GetRectDisplayPix(
        left,
        top,
        screenWidth,
        screenHeight
      );

      /**
       * The primary monitor default scale might be different than
       * what is reported by the window on mixed-DPI systems.
       * To get the best image quality, query both and take the highest one.
       */
      let primaryScale = screenManager.primaryScreen.defaultCSSScaleFactor;
      let windowScale = aWindow ? aWindow.devicePixelRatio : primaryScale;
      let scale = Math.max(primaryScale, windowScale);

      /** *
       * THESE VALUES ARE DEFINED IN newtab.css and hard coded.
       * If you change these values from the prefs,
       * ALSO CHANGE THEM IN newtab.css
       */
      let prefWidth = Services.prefs.getIntPref("toolkit.pageThumbs.minWidth");
      let prefHeight = Services.prefs.getIntPref(
        "toolkit.pageThumbs.minHeight"
      );
      let divisor = Services.prefs.getIntPref(
        "toolkit.pageThumbs.screenSizeDivisor"
      );

      prefWidth *= scale;
      prefHeight *= scale;

      this._thumbnailWidth = Math.max(
        Math.round(screenWidth.value / divisor),
        prefWidth
      );
      this._thumbnailHeight = Math.max(
        Math.round(screenHeight.value / divisor),
        prefHeight
      );
    }

    return [this._thumbnailWidth, this._thumbnailHeight];
  },

  /** *
   * Given a browser window, return the size of the content
   * minus the scroll bars.
   */
  getContentSize(aWindow) {
    let utils = aWindow.windowUtils;
    let sbWidth = {};
    let sbHeight = {};

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

  /**
   * Renders an image onto a new canvas of a given width and proportional
   * height. Uses an image that exists in the window and is loaded, or falls
   * back to loading the url into a new image element.
   */
  async createImageThumbnailCanvas(
    window,
    url,
    targetWidth = 448,
    backgroundColor = this.THUMBNAIL_BG_COLOR
  ) {
    // 224px is the width of cards in ActivityStream; capture thumbnails at 2x
    const doc = (window || Services.appShell.hiddenDOMWindow).document;

    let image = doc.querySelector("img");
    if (!image) {
      image = doc.createElementNS(this.HTML_NAMESPACE, "img");
      await new Promise((resolve, reject) => {
        image.onload = () => resolve();
        image.onerror = () => reject(new Error("LOAD_FAILED"));
        image.src = url;
      });
    }

    // <img src="*.svg"> has width/height but not naturalWidth/naturalHeight
    const imageWidth = image.naturalWidth || image.width;
    const imageHeight = image.naturalHeight || image.height;
    if (imageWidth === 0 || imageHeight === 0) {
      throw new Error("IMAGE_ZERO_DIMENSION");
    }
    const width = Math.min(targetWidth, imageWidth);
    const height = (imageHeight * width) / imageWidth;

    // As we're setting the width and maintaining the aspect ratio, if an image
    // is very tall we might get a very large thumbnail. Restricting the canvas
    // size to {width}x{width} solves this problem. Here we choose to clip the
    // image at the bottom rather than centre it vertically, based on an
    // estimate that the focus of a tall image is most likely to be near the top
    // (e.g., the face of a person).
    const canvasHeight = Math.min(height, width);
    const canvas = this.createCanvas(window, width, canvasHeight);
    const context = canvas.getContext("2d");
    context.fillStyle = backgroundColor;
    context.fillRect(0, 0, width, canvasHeight);
    context.drawImage(image, 0, 0, width, height);

    return {
      width,
      height: canvasHeight,
      imageData: canvas.toDataURL(),
    };
  },

  /**
   * Given a browser, this creates a snapshot of the content
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
   * @params aBrowser - the browser to create a snapshot of.
   * @params aDestCanvas destination canvas to draw the final
   *   snapshot to. Can be null.
   * @param aArgs (optional) Additional named parameters:
   *   fullScale - request that a non-downscaled image be returned.
   * @return Canvas with a scaled thumbnail of the window.
   */
  async createSnapshotThumbnail(aBrowser, aDestCanvas, aArgs) {
    const aWindow = aBrowser.contentWindow;
    let backgroundColor = aArgs
      ? aArgs.backgroundColor
      : PageThumbUtils.THUMBNAIL_BG_COLOR;
    let fullScale = aArgs ? aArgs.fullScale : false;
    let [contentWidth, contentHeight] = this.getContentSize(aWindow);
    let [thumbnailWidth, thumbnailHeight] = aDestCanvas
      ? [aDestCanvas.width, aDestCanvas.height]
      : this.getThumbnailSize(aWindow);

    // If the caller wants a fullscale image, set the desired thumbnail dims
    // to the dims of content and (if provided) size the incoming canvas to
    // support our results.
    if (fullScale) {
      thumbnailWidth = contentWidth;
      thumbnailHeight = contentHeight;
      if (aDestCanvas) {
        aDestCanvas.width = contentWidth;
        aDestCanvas.height = contentHeight;
      }
    }

    let intermediateWidth = thumbnailWidth * 2;
    let intermediateHeight = thumbnailHeight * 2;
    let skipDownscale = false;

    // If the intermediate thumbnail is larger than content dims (hiDPI
    // devices can experience this) or a full preview is requested render
    // at the final thumbnail size.
    if (
      intermediateWidth >= contentWidth ||
      intermediateHeight >= contentHeight ||
      fullScale
    ) {
      intermediateWidth = thumbnailWidth;
      intermediateHeight = thumbnailHeight;
      skipDownscale = true;
    }

    // Create an intermediate surface
    let snapshotCanvas = this.createCanvas(
      aWindow,
      intermediateWidth,
      intermediateHeight
    );

    // Step 1: capture the image at the intermediate dims. For thumbnails
    // this is twice the thumbnail size, for fullScale images this is at
    // content dims.
    // Also by default, canvas does not draw the scrollbars, so no need to
    // remove the scrollbar sizes.
    let scale = Math.min(
      Math.max(
        intermediateWidth / contentWidth,
        intermediateHeight / contentHeight
      ),
      1
    );

    let snapshotCtx = snapshotCanvas.getContext("2d");
    snapshotCtx.save();
    snapshotCtx.scale(scale, scale);
    const image = await aBrowser.drawSnapshot(
      0,
      0,
      contentWidth,
      contentHeight,
      scale,
      backgroundColor
    );
    snapshotCtx.drawImage(image, 0, 0, contentWidth, contentHeight);
    snapshotCtx.restore();

    // Part 2: Downscale from our intermediate dims to the final thumbnail
    // dims and copy the result to aDestCanvas. If the caller didn't
    // provide a target canvas, create a new canvas and return it.
    let finalCanvas =
      aDestCanvas ||
      this.createCanvas(aWindow, thumbnailWidth, thumbnailHeight);

    let finalCtx = finalCanvas.getContext("2d");
    finalCtx.save();
    if (!skipDownscale) {
      finalCtx.scale(0.5, 0.5);
    }
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
  determineCropSize(aWindow, aCanvas) {
    let utils = aWindow.windowUtils;
    let sbWidth = {};
    let sbHeight = {};

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

    let { width: thumbnailWidth, height: thumbnailHeight } = aCanvas;
    let scale = Math.min(
      Math.max(thumbnailWidth / width, thumbnailHeight / height),
      1
    );
    let scaledWidth = width * scale;
    let scaledHeight = height * scale;

    if (scaledHeight > thumbnailHeight) {
      height -= Math.floor(Math.abs(scaledHeight - thumbnailHeight) * scale);
    }

    if (scaledWidth > thumbnailWidth) {
      width -= Math.floor(Math.abs(scaledWidth - thumbnailWidth) * scale);
    }

    return [width, height, scale];
  },

  shouldStoreContentThumbnail(aDocument, aDocShell) {
    if (BrowserUtils.isFindbarVisible(aDocShell)) {
      return false;
    }

    // FIXME Bug 720575 - Don't capture thumbnails for SVG or XML documents as
    //       that currently regresses Talos SVG tests.
    if (ChromeUtils.getClassName(aDocument) === "XMLDocument") {
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
    } catch (e) {
      /* Not an HTTP channel. */
    }

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
      if (
        uri.schemeIs("https") &&
        !Services.prefs.getBoolPref("browser.cache.disk_cache_ssl")
      ) {
        return false;
      }
    } // httpChannel
    return true;
  },

  /**
   * Given a channel, returns true if it should be considered an "error
   * response", false otherwise.
   */
  isChannelErrorResponse(channel) {
    // No valid document channel sounds like an error to me!
    if (!channel) {
      return true;
    }
    if (!(channel instanceof Ci.nsIHttpChannel)) {
      // it might be FTP etc, so assume it's ok.
      return false;
    }
    try {
      return !channel.requestSucceeded;
    } catch (_) {
      // not being able to determine success is surely failure!
      return true;
    }
  },
};
