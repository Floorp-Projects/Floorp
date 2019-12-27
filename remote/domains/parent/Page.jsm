/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Page"];

const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
const { clearInterval, setInterval } = ChromeUtils.import(
  "resource://gre/modules/Timer.jsm"
);

const { DialogHandler } = ChromeUtils.import(
  "chrome://remote/content/domains/parent/page/DialogHandler.jsm"
);
const { Domain } = ChromeUtils.import(
  "chrome://remote/content/domains/Domain.jsm"
);
const { UnsupportedError } = ChromeUtils.import(
  "chrome://remote/content/Error.jsm"
);
const { streamRegistry } = ChromeUtils.import(
  "chrome://remote/content/domains/parent/IO.jsm"
);
const { TabManager } = ChromeUtils.import(
  "chrome://remote/content/TabManager.jsm"
);
const { WindowManager } = ChromeUtils.import(
  "chrome://remote/content/WindowManager.jsm"
);

const PRINT_MAX_SCALE_VALUE = 2.0;
const PRINT_MIN_SCALE_VALUE = 0.1;

const PDF_TRANSFER_MODES = {
  base64: "ReturnAsBase64",
  stream: "ReturnAsStream",
};

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
   * Print page as PDF.
   *
   * @param {Object} options
   * @param {boolean=} options.displayHeaderFooter
   *     Display header and footer. Defaults to false.
   * @param {string=} options.footerTemplate (not supported)
   *     HTML template for the print footer.
   * @param {string=} options.headerTemplate (not supported)
   *     HTML template for the print header. Should use the same format
   *     as the footerTemplate.
   * @param {boolean=} options.ignoreInvalidPageRanges
   *     Whether to silently ignore invalid but successfully parsed page ranges,
   *     such as '3-2'. Defaults to false.
   * @param {boolean=} options.landscape
   *     Paper orientation. Defaults to false.
   * @param {number=} options.marginBottom
   *     Bottom margin in inches. Defaults to 1cm (~0.4 inches).
   * @param {number=} options.marginLeft
   *     Left margin in inches. Defaults to 1cm (~0.4 inches).
   * @param {number=} options.marginRight
   *     Right margin in inches. Defaults to 1cm (~0.4 inches).
   * @param {number=} options.marginTop
   *     Top margin in inches. Defaults to 1cm (~0.4 inches).
   * @param {string=} options.pageRanges (not supported)
   *     Paper ranges to print, e.g., '1-5, 8, 11-13'.
   *     Defaults to the empty string, which means print all pages.
   * @param {number=} options.paperHeight
   *     Paper height in inches. Defaults to 11 inches.
   * @param {number=} options.paperWidth
   *     Paper width in inches. Defaults to 8.5 inches.
   * @param {boolean=} options.preferCSSPageSize
   *     Whether or not to prefer page size as defined by CSS.
   *     Defaults to false, in which case the content will be scaled
   *     to fit the paper size.
   * @param {boolean=} options.printBackground
   *     Print background graphics. Defaults to false.
   * @param {number=} options.scale
   *     Scale of the webpage rendering. Defaults to 1.
   * @param {string=} options.transferMode
   *     Return as base64-encoded string (ReturnAsBase64),
   *     or stream (ReturnAsStream). Defaults to ReturnAsBase64.
   *
   * @return {Promise<{data:string, stream:string}>
   *     Based on the transferMode setting data is a base64-encoded string,
   *     or stream is a handle to a OS.File stream.
   */
  async printToPDF(options = {}) {
    const {
      displayHeaderFooter = false,
      // Bug 1601570 - Implement templates for header and footer
      // headerTemplate = "",
      // footerTemplate = "",
      landscape = false,
      marginBottom = 0.39,
      marginLeft = 0.39,
      marginRight = 0.39,
      marginTop = 0.39,
      // Bug 1601571 - Implement handling of page ranges
      // TODO: pageRanges = "",
      // TODO: ignoreInvalidPageRanges = false,
      paperHeight = 11.0,
      paperWidth = 8.5,
      preferCSSPageSize = false,
      printBackground = false,
      scale = 1.0,
      transferMode = PDF_TRANSFER_MODES.base64,
    } = options;

    if (marginBottom < 0) {
      throw new TypeError("marginBottom is negative");
    }
    if (marginLeft < 0) {
      throw new TypeError("marginLeft is negative");
    }
    if (marginRight < 0) {
      throw new TypeError("marginRight is negative");
    }
    if (marginTop < 0) {
      throw new TypeError("marginTop is negative");
    }
    if (scale < PRINT_MIN_SCALE_VALUE || scale > PRINT_MAX_SCALE_VALUE) {
      throw new TypeError("scale is outside [0.1 - 2] range");
    }
    if (paperHeight <= 0) {
      throw new TypeError("paperHeight is zero or negative");
    }
    if (paperWidth <= 0) {
      throw new TypeError("paperWidth is zero or negative");
    }

    // Create a unique filename for the temporary PDF file
    const basePath = OS.Path.join(OS.Constants.Path.tmpDir, "remote-agent.pdf");
    const { file, path: filePath } = await OS.File.openUnique(basePath);
    await file.close();

    const psService = Cc["@mozilla.org/gfx/printsettings-service;1"].getService(
      Ci.nsIPrintSettingsService
    );

    const printSettings = psService.newPrintSettings;
    printSettings.isInitializedFromPrinter = true;
    printSettings.isInitializedFromPrefs = true;
    printSettings.outputFormat = Ci.nsIPrintSettings.kOutputFormatPDF;
    printSettings.printerName = "";
    printSettings.printSilent = true;
    printSettings.printToFile = true;
    printSettings.showPrintProgress = false;
    printSettings.toFileName = filePath;

    printSettings.paperSizeUnit = Ci.nsIPrintSettings.kPaperSizeInches;
    printSettings.paperWidth = paperWidth;
    printSettings.paperHeight = paperHeight;

    printSettings.marginBottom = marginBottom;
    printSettings.marginLeft = marginLeft;
    printSettings.marginRight = marginRight;
    printSettings.marginTop = marginTop;

    printSettings.printBGColors = printBackground;
    printSettings.printBGImages = printBackground;
    printSettings.scaling = scale;
    printSettings.shrinkToFit = preferCSSPageSize;

    if (!displayHeaderFooter) {
      printSettings.headerStrCenter = "";
      printSettings.headerStrLeft = "";
      printSettings.headerStrRight = "";
      printSettings.footerStrCenter = "";
      printSettings.footerStrLeft = "";
      printSettings.footerStrRight = "";
    }

    if (landscape) {
      printSettings.orientation = Ci.nsIPrintSettings.kLandscapeOrientation;
    }

    await new Promise(resolve => {
      // Bug 1603739 - With e10s enabled the WebProgressListener states
      // STOP too early, which means the file hasn't been completely written.
      const waitForFileWritten = () => {
        const DELAY_CHECK_FILE_COMPLETELY_WRITTEN = 100;

        let lastSize = 0;
        const timerId = setInterval(async () => {
          const fileInfo = await OS.File.stat(filePath);
          if (lastSize > 0 && fileInfo.size == lastSize) {
            clearInterval(timerId);
            resolve();
          }
          lastSize = fileInfo.size;
        }, DELAY_CHECK_FILE_COMPLETELY_WRITTEN);
      };

      const printProgressListener = {
        onStateChange(webProgress, request, flags, status) {
          if (
            flags & Ci.nsIWebProgressListener.STATE_STOP &&
            flags & Ci.nsIWebProgressListener.STATE_IS_NETWORK
          ) {
            waitForFileWritten();
          }
        },
        QueryInterface: ChromeUtils.generateQI([Ci.nsIWebProgressListener]),
      };

      const { tab } = this.session.target;
      tab.linkedBrowser.print(
        tab.linkedBrowser.outerWindowID,
        printSettings,
        printProgressListener
      );
    });

    const fp = await OS.File.open(filePath);

    const retval = { data: null, stream: null };
    if (transferMode == PDF_TRANSFER_MODES.stream) {
      retval.stream = streamRegistry.add(fp);
    } else {
      // return all data as a base64 encoded string
      let bytes;
      try {
        bytes = await fp.read();
      } finally {
        fp.close();
        await OS.File.remove(filePath);
      }

      // Each UCS2 character has an upper byte of 0 and a lower byte matching
      // the binary data
      retval.data = btoa(String.fromCharCode.apply(null, bytes));
    }

    return retval;
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
