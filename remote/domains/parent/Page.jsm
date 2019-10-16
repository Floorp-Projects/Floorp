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
   * @param {string=} options.format (not supported)
   *     Image compression format. Defaults to "png".
   * @param {number=} options.quality (not supported)
   *     Compression quality from range [0..100] (jpeg only). Defaults to 100.
   *
   * @return {string}
   *     Base64-encoded image data.
   */
  async captureScreenshot(options = {}) {
    if (options.clip) {
      throw new UnsupportedError("clip not supported");
    }
    if (options.format) {
      throw new UnsupportedError("format not supported");
    }
    if (options.fromSurface) {
      throw new UnsupportedError("fromSurface not supported");
    }
    if (options.quality) {
      throw new UnsupportedError("quality not supported");
    }

    const MAX_CANVAS_DIMENSION = 32767;
    const MAX_CANVAS_AREA = 472907776;

    // Retrieve the browsing context of the content browser
    const { browsingContext, window } = this.session.target;
    const scale = window.devicePixelRatio;

    const rect = await this.executeInChild("_viewportRect");

    let canvasWidth = rect.width * scale;
    let canvasHeight = rect.height * scale;

    // Cap the screenshot size based on maximum allowed canvas sizes.
    // Using higher dimensions would trigger exceptions in Gecko.
    //
    // See: https://developer.mozilla.org/en-US/docs/Web/HTML/Element/canvas#Maximum_canvas_size
    if (canvasWidth > MAX_CANVAS_DIMENSION) {
      rect.width = Math.floor(MAX_CANVAS_DIMENSION / scale);
      canvasWidth = rect.width * scale;
    }
    if (canvasHeight > MAX_CANVAS_DIMENSION) {
      rect.height = Math.floor(MAX_CANVAS_DIMENSION / scale);
      canvasHeight = rect.height * scale;
    }
    // If the area is larger, reduce the height to keep the full width.
    if (canvasWidth * canvasHeight > MAX_CANVAS_AREA) {
      rect.height = Math.floor(MAX_CANVAS_AREA / (canvasWidth * scale));
      canvasHeight = rect.height * scale;
    }

    const snapshot = await browsingContext.currentWindowGlobal.drawSnapshot(
      rect,
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

    return canvas.toDataURL();
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

  bringToFront() {
    const { browser } = this.session.target;
    const navigator = browser.ownerGlobal;
    const { gBrowser } = navigator;

    // Focus the window responsible for this page.
    navigator.focus();

    // Select the corresponding tab
    gBrowser.selectedTab = gBrowser.getTabForBrowser(browser);
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
