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
   * @return The newly created canvas.
   */
  createCanvas: function (aWindow) {
    let doc = (aWindow || Services.appShell.hiddenDOMWindow).document;
    let canvas = doc.createElementNS(this.HTML_NAMESPACE, "canvas");
    canvas.mozOpaque = true;
    canvas.mozImageSmoothingEnabled = true;
    let [thumbnailWidth, thumbnailHeight] = this.getThumbnailSize();
    canvas.width = thumbnailWidth;
    canvas.height = thumbnailHeight;
    return canvas;
  },

  /**
   * Calculates a preferred initial thumbnail size based on current desktop
   * dimensions. The resulting dims will generally be about 1/3 the
   * size of the desktop. (jimm: why??)
   *
   * @return The calculated thumbnail size or a default if unable to calculate.
   */
  getThumbnailSize: function () {
    if (!this._thumbnailWidth || !this._thumbnailHeight) {
      let screenManager = Cc["@mozilla.org/gfx/screenmanager;1"]
                            .getService(Ci.nsIScreenManager);
      let left = {}, top = {}, width = {}, height = {};
      screenManager.primaryScreen.GetRectDisplayPix(left, top, width, height);
      this._thumbnailWidth = Math.round(width.value / 3);
      this._thumbnailHeight = Math.round(height.value / 3);
    }
    return [this._thumbnailWidth, this._thumbnailHeight];
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
    let utils = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIDOMWindowUtils);
    // aWindow may be a cpow, add exposed props security values.
    let sbWidth = {__exposedProps__: {"value": "rw"}},
      sbHeight = {__exposedProps__: {"value": "rw"}};

    try {
      utils.getScrollbarSize(false, sbWidth, sbHeight);
    } catch (e) {
      // This might fail if the window does not have a presShell.
      Cu.reportError("Unable to get scrollbar size in determineCropSize.");
      sbWidth.value = sbHeight.value = 0;
    }

    // Even in RTL mode, scrollbars are always on the right.
    // So there's no need to determine a left offset.
    let sw = aWindow.innerWidth - sbWidth.value;
    let sh = aWindow.innerHeight - sbHeight.value;

    let {width: thumbnailWidth, height: thumbnailHeight} = aCanvas;
    let scale = Math.min(Math.max(thumbnailWidth / sw, thumbnailHeight / sh), 1);
    let scaledWidth = sw * scale;
    let scaledHeight = sh * scale;

    if (scaledHeight > thumbnailHeight)
      sh -= Math.floor(Math.abs(scaledHeight - thumbnailHeight) * scale);

    if (scaledWidth > thumbnailWidth)
      sw -= Math.floor(Math.abs(scaledWidth - thumbnailWidth) * scale);

    return [sw, sh, scale];
  }
};
