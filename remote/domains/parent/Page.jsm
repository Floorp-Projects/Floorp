/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Page"];

const { DialogHandler } = ChromeUtils.import(
  "chrome://remote/content/domains/parent/page/DialogHandler.jsm"
);
const { Domain } = ChromeUtils.import(
  "chrome://remote/content/domains/Domain.jsm"
);
const { UnsupportedError } = ChromeUtils.import(
  "chrome://remote/content/Error.jsm"
);
const { TabManager } = ChromeUtils.import(
  "chrome://remote/content/TabManager.jsm"
);
const { WindowManager } = ChromeUtils.import(
  "chrome://remote/content/WindowManager.jsm"
);

class Page extends Domain {
  constructor(session) {
    super(session);

    this._onDialogLoaded = this._onDialogLoaded.bind(this);

    this.enabled = false;
  }

  destructor() {
    // Flip a flag to avoid to disable the content domain from this.disable()
    this._isDestroyed = false;
    this.disable();

    super.destructor();
  }

  // commands

  /**
   * Capture page screenshot.
   *
   * @param {Object} options
   * @param {Viewport=} options.clip (not supported)
   *     Capture the screenshot of a given region only.
   * @param {string=} options.format
   *     Image compression format. Defaults to "png".
   * @param {number=} options.quality
   *     Compression quality from range [0..100] (jpeg only). Defaults to 80.
   *
   * @return {string}
   *     Base64-encoded image data.
   */
  async captureScreenshot(options = {}) {
    const { format = "png", quality = 80 } = options;

    if (options.clip) {
      throw new UnsupportedError("clip not supported");
    }
    if (options.fromSurface) {
      throw new UnsupportedError("fromSurface not supported");
    }

    const MAX_CANVAS_DIMENSION = 32767;
    const MAX_CANVAS_AREA = 472907776;

    // Retrieve the browsing context of the content browser
    const { browsingContext, window } = this.session.target;
    const scale = window.devicePixelRatio;

    const rect = await this.executeInChild("_layoutViewport");

    let canvasWidth = rect.clientWidth * scale;
    let canvasHeight = rect.clientHeight * scale;

    // Cap the screenshot size based on maximum allowed canvas sizes.
    // Using higher dimensions would trigger exceptions in Gecko.
    //
    // See: https://developer.mozilla.org/en-US/docs/Web/HTML/Element/canvas#Maximum_canvas_size
    if (canvasWidth > MAX_CANVAS_DIMENSION) {
      rect.clientWidth = Math.floor(MAX_CANVAS_DIMENSION / scale);
      canvasWidth = rect.clientWidth * scale;
    }
    if (canvasHeight > MAX_CANVAS_DIMENSION) {
      rect.clientHeight = Math.floor(MAX_CANVAS_DIMENSION / scale);
      canvasHeight = rect.clientHeight * scale;
    }
    // If the area is larger, reduce the height to keep the full width.
    if (canvasWidth * canvasHeight > MAX_CANVAS_AREA) {
      rect.clientHeight = Math.floor(MAX_CANVAS_AREA / (canvasWidth * scale));
      canvasHeight = rect.clientHeight * scale;
    }

    const captureRect = new DOMRect(
      rect.pageX,
      rect.pageY,
      rect.clientWidth,
      rect.clientHeight
    );
    const snapshot = await browsingContext.currentWindowGlobal.drawSnapshot(
      captureRect,
      scale,
      "rgb(255,255,255)"
    );

    const canvas = window.document.createElementNS(
      "http://www.w3.org/1999/xhtml",
      "canvas"
    );
    canvas.width = canvasWidth;
    canvas.height = canvasHeight;

    const ctx = canvas.getContext("2d");
    ctx.drawImage(snapshot, 0, 0);

    // Bug 1574935 - Huge dimensions can trigger an OOM because multiple copies
    // of the bitmap will exist in memory. Force the removal of the snapshot
    // because it is no longer needed.
    snapshot.close();

    const url = canvas.toDataURL(`image/${format}`, quality / 100);
    if (!url.startsWith(`data:image/${format}`)) {
      throw new UnsupportedError(`Unsupported MIME type: image/${format}`);
    }

    // only return the base64 encoded data without the data URL prefix
    const data = url.substring(url.indexOf(",") + 1);

    return { data };
  }

  async enable() {
    if (this.enabled) {
      return;
    }

    this.enabled = true;

    const { browser } = this.session.target;
    this._dialogHandler = new DialogHandler(browser);
    this._dialogHandler.on("dialog-loaded", this._onDialogLoaded);
    await this.executeInChild("enable");
  }

  async disable() {
    if (!this.enabled) {
      return;
    }

    this._dialogHandler.destructor();
    this._dialogHandler = null;
    this.enabled = false;

    if (!this._isDestroyed) {
      // Only call disable in the content domain if we are not destroying the domain.
      // If we are destroying the domain, the content domains will be destroyed
      // independently after firing the remote:destroy event.
      await this.executeInChild("disable");
    }
  }

  async bringToFront() {
    const { tab, window } = this.session.target;

    // Focus the window, and select the corresponding tab
    await WindowManager.focus(window);
    TabManager.selectTab(tab);
  }

  /**
   * Return metrics relating to the layouting of the page.
   *
   * The returned object contains the following entries:
   *
   * layoutViewport:
   *     {number} pageX
   *         Horizontal offset relative to the document (CSS pixels)
   *     {number} pageY
   *         Vertical offset relative to the document (CSS pixels)
   *     {number} clientWidth
   *         Width (CSS pixels), excludes scrollbar if present
   *     {number} clientHeight
   *         Height (CSS pixels), excludes scrollbar if present
   *
   * visualViewport:
   *     {number} offsetX
   *         Horizontal offset relative to the layout viewport (CSS pixels)
   *     {number} offsetY
   *         Vertical offset relative to the layout viewport (CSS pixels)
   *     {number} pageX
   *         Horizontal offset relative to the document (CSS pixels)
   *     {number} pageY
   *         Vertical offset relative to the document (CSS pixels)
   *     {number} clientWidth
   *         Width (CSS pixels), excludes scrollbar if present
   *     {number} clientHeight
   *         Height (CSS pixels), excludes scrollbar if present
   *     {number} scale
   *         Scale relative to the ideal viewport (size at width=device-width)
   *     {number} zoom
   *         Page zoom factor (CSS to device independent pixels ratio)
   *
   * contentSize:
   *     {number} x
   *         X coordinate
   *     {number} y
   *         Y coordinate
   *     {number} width
   *         Width of scrollable area
   *     {number} height
   *         Height of scrollable area
   *
   * @return {Promise}
   * @resolves {layoutViewport, visualViewport, contentSize}
   */
  async getLayoutMetrics() {
    return {
      layoutViewport: await this.executeInChild("_layoutViewport"),
      contentSize: await this.executeInChild("_contentSize"),
    };
  }

  /**
   * Interact with the currently opened JavaScript dialog (alert, confirm,
   * prompt) for this page. This will always close the dialog, either accepting
   * or rejecting it, with the optional prompt filled.
   *
   * @param {Object}
   *        - {Boolean} accept: For "confirm", "prompt", "beforeunload" dialogs
   *          true will accept the dialog, false will cancel it. For "alert"
   *          dialogs, true or false closes the dialog in the same way.
   *        - {String} promptText: for "prompt" dialogs, used to fill the prompt
   *          input.
   */
  async handleJavaScriptDialog({ accept, promptText }) {
    if (!this.enabled) {
      throw new Error("Page domain is not enabled");
    }
    await this._dialogHandler.handleJavaScriptDialog({ accept, promptText });
  }

  /**
   * Emit the proper CDP event javascriptDialogOpening when a javascript dialog
   * opens for the current target.
   */
  _onDialogLoaded(e, data) {
    const { message, type } = data;
    // XXX: We rely on the tabmodal-dialog-loaded event (see DialogHandler.jsm)
    // which is inconsistent with the name "javascriptDialogOpening".
    // For correctness we should rely on an event fired _before_ the prompt is
    // visible, such as DOMWillOpenModalDialog. However the payload of this
    // event does not contain enough data to populate javascriptDialogOpening.
    //
    // Since the event is fired asynchronously, this should not have an impact
    // on the actual tests relying on this API.
    this.emit("Page.javascriptDialogOpening", { message, type });
  }
}
