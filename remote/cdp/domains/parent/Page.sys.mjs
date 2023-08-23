/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Domain } from "chrome://remote/content/cdp/domains/Domain.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  SessionStore: "resource:///modules/sessionstore/SessionStore.sys.mjs",

  DialogHandler:
    "chrome://remote/content/cdp/domains/parent/page/DialogHandler.sys.mjs",
  generateUUID: "chrome://remote/content/shared/UUID.sys.mjs",
  PollPromise: "chrome://remote/content/shared/Sync.sys.mjs",
  print: "chrome://remote/content/shared/PDF.sys.mjs",
  streamRegistry: "chrome://remote/content/cdp/domains/parent/IO.sys.mjs",
  Stream: "chrome://remote/content/cdp/StreamRegistry.sys.mjs",
  TabManager: "chrome://remote/content/shared/TabManager.sys.mjs",
  UnsupportedError: "chrome://remote/content/cdp/Error.sys.mjs",
  windowManager: "chrome://remote/content/shared/WindowManager.sys.mjs",
});

const MAX_CANVAS_DIMENSION = 32767;
const MAX_CANVAS_AREA = 472907776;

const PRINT_MAX_SCALE_VALUE = 2.0;
const PRINT_MIN_SCALE_VALUE = 0.1;

const PDF_TRANSFER_MODES = {
  base64: "ReturnAsBase64",
  stream: "ReturnAsStream",
};

const TIMEOUT_SET_HISTORY_INDEX = 1000;

export class Page extends Domain {
  constructor(session) {
    super(session);

    this._onDialogLoaded = this._onDialogLoaded.bind(this);
    this._onRequest = this._onRequest.bind(this);

    this.enabled = false;

    this.session.networkObserver.startTrackingBrowserNetwork(
      this.session.target.browser
    );
    this.session.networkObserver.on("request", this._onRequest);
  }

  destructor() {
    // Flip a flag to avoid to disable the content domain from this.disable()
    this._isDestroyed = false;
    this.disable();

    this.session.networkObserver.off("request", this._onRequest);
    this.session.networkObserver.stopTrackingBrowserNetwork(
      this.session.target.browser
    );
    super.destructor();
  }

  // commands

  /**
   * Navigates current page to given URL.
   *
   * @param {object} options
   * @param {string} options.url
   *     destination URL
   * @param {string=} options.frameId
   *     frame id to navigate (not supported),
   *     if not specified navigate top frame
   * @param {string=} options.referrer
   *     referred URL (optional)
   * @param {string=} options.transitionType
   *     intended transition type
   * @returns {object}
   *         - frameId {string} frame id that has navigated (or failed to)
   *         - errorText {string=} error message if navigation has failed
   *         - loaderId {string} (not supported)
   */
  async navigate(options = {}) {
    const { url, frameId, referrer, transitionType } = options;
    if (typeof url != "string") {
      throw new TypeError("url: string value expected");
    }
    let validURL;
    try {
      validURL = Services.io.newURI(url);
    } catch (e) {
      throw new Error("Error: Cannot navigate to invalid URL");
    }
    const topFrameId = this.session.browsingContext.id.toString();
    if (frameId && frameId != topFrameId) {
      throw new lazy.UnsupportedError("frameId not supported");
    }

    const hitsNetwork = ["https", "http"].includes(validURL.scheme);
    let networkLessLoaderId;
    if (!hitsNetwork) {
      // This navigation will not hit the network, use a randomly generated id.
      networkLessLoaderId = lazy.generateUUID();

      // Update the content process map of loader ids.
      await this.executeInChild("_updateLoaderId", {
        frameId: this.session.browsingContext.id,
        loaderId: networkLessLoaderId,
      });
    }

    const currentURI = this.session.browsingContext.currentURI;

    const isSameDocumentNavigation =
      // The "host", "query" and "ref" getters can throw if the URLs are not
      // http/https, so verify first that both currentURI and validURL are
      // using http/https.
      hitsNetwork &&
      ["https", "http"].includes(currentURI.scheme) &&
      currentURI.host === validURL.host &&
      currentURI.query === validURL.query &&
      !!validURL.ref;

    const requestDone = new Promise(resolve => {
      if (isSameDocumentNavigation) {
        // Per CDP documentation, same-document navigations should not emit any
        // loader id (https://chromedevtools.github.io/devtools-protocol/tot/Page/#method-navigate)
        resolve({});
        return;
      }

      if (!hitsNetwork) {
        // This navigation will not hit the network, use a randomly generated id.
        resolve({ navigationRequestId: networkLessLoaderId });
        return;
      }
      let navigationRequestId, redirectedRequestId;
      const _onNavigationRequest = function (_type, _ch, data) {
        const {
          url: requestURL,
          requestId,
          redirectedFrom = null,
          isNavigationRequest,
        } = data;
        if (!isNavigationRequest) {
          return;
        }
        if (validURL.spec === requestURL) {
          navigationRequestId = redirectedRequestId = requestId;
        } else if (redirectedFrom === redirectedRequestId) {
          redirectedRequestId = requestId;
        }
      };

      const _onRequestFinished = function (_type, _ch, data) {
        const { requestId, errorCode } = data;
        if (
          redirectedRequestId !== requestId ||
          errorCode == "NS_BINDING_REDIRECTED"
        ) {
          // handle next request in redirection chain
          return;
        }
        this.session.networkObserver.off("request", _onNavigationRequest);
        this.session.networkObserver.off("requestfinished", _onRequestFinished);
        resolve({ errorCode, navigationRequestId });
      }.bind(this);

      this.session.networkObserver.on("request", _onNavigationRequest);
      this.session.networkObserver.on("requestfinished", _onRequestFinished);
    });

    const opts = {
      loadFlags: transitionToLoadFlag(transitionType),
      referrerURI: referrer,
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    };
    this.session.browsingContext.loadURI(validURL, opts);
    // clients expect loaderId == requestId for a document navigation request
    const { navigationRequestId: loaderId, errorCode } = await requestDone;
    const result = {
      frameId: topFrameId,
      loaderId,
    };
    if (errorCode) {
      result.errorText = errorCode;
    }
    return result;
  }

  /**
   * Capture page screenshot.
   *
   * @param {object} options
   * @param {Viewport=} options.clip
   *     Capture the screenshot of a given region only.
   * @param {string=} options.format
   *     Image compression format. Defaults to "png".
   * @param {number=} options.quality
   *     Compression quality from range [0..100] (jpeg only). Defaults to 80.
   *
   * @returns {string}
   *     Base64-encoded image data.
   */
  async captureScreenshot(options = {}) {
    const { clip, format = "png", quality = 80 } = options;

    if (options.fromSurface) {
      throw new lazy.UnsupportedError("fromSurface not supported");
    }

    let rect;
    let scale = await this.executeInChild("_devicePixelRatio");

    if (clip) {
      for (const prop of ["x", "y", "width", "height", "scale"]) {
        if (clip[prop] == undefined) {
          throw new TypeError(`clip.${prop}: double value expected`);
        }
      }

      const contentRect = await this.executeInChild("_contentRect");

      // For invalid scale values default to full page
      if (clip.scale <= 0) {
        Object.assign(clip, {
          x: 0,
          y: 0,
          width: contentRect.width,
          height: contentRect.height,
          scale: 1,
        });
      } else {
        if (clip.x < 0 || clip.x > contentRect.width - 1) {
          clip.x = 0;
        }
        if (clip.y < 0 || clip.y > contentRect.height - 1) {
          clip.y = 0;
        }
        if (clip.width <= 0) {
          clip.width = contentRect.width;
        }
        if (clip.height <= 0) {
          clip.height = contentRect.height;
        }
      }

      rect = new DOMRect(clip.x, clip.y, clip.width, clip.height);
      scale *= clip.scale;
    } else {
      // If no specific clipping region has been specified,
      // fallback to the layout (fixed) viewport, and the
      // default pixel ratio.
      const { pageX, pageY, clientWidth, clientHeight } =
        await this.executeInChild("_layoutViewport");

      rect = new DOMRect(pageX, pageY, clientWidth, clientHeight);
    }

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

    const { browsingContext, window } = this.session.target;
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

    const url = canvas.toDataURL(`image/${format}`, quality / 100);
    if (!url.startsWith(`data:image/${format}`)) {
      throw new lazy.UnsupportedError(`Unsupported MIME type: image/${format}`);
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
    this._dialogHandler = new lazy.DialogHandler(browser);
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
    await lazy.windowManager.focusWindow(window);
    await lazy.TabManager.selectTab(tab);
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
   * @returns {Promise<object>}
   *     Promise which resolves with an object with the following properties
   *     layoutViewport and contentSize
   */
  async getLayoutMetrics() {
    return {
      layoutViewport: await this.executeInChild("_layoutViewport"),
      contentSize: await this.executeInChild("_contentRect"),
    };
  }

  /**
   * Returns navigation history for the current page.
   *
   * @returns {Promise<object>}
   *     Promise which resolves with an object with the following properties
   *     currentIndex (number) and entries (Array<NavigationEntry>).
   */
  async getNavigationHistory() {
    const { window } = this.session.target;

    return new Promise(resolve => {
      function updateSessionHistory(sessionHistory) {
        const entries = sessionHistory.entries.map(entry => {
          return {
            id: entry.ID,
            url: entry.url,
            userTypedURL: entry.originalURI || entry.url,
            title: entry.title,
            // TODO: Bug 1609514
            transitionType: null,
          };
        });

        resolve({
          currentIndex: sessionHistory.index,
          entries,
        });
      }

      lazy.SessionStore.getSessionHistory(
        window.gBrowser.selectedTab,
        updateSessionHistory
      );
    });
  }

  /**
   * Interact with the currently opened JavaScript dialog (alert, confirm,
   * prompt) for this page. This will always close the dialog, either accepting
   * or rejecting it, with the optional prompt filled.
   *
   * @param {object} options
   * @param {boolean=} options.accept
   *    for "confirm", "prompt", "beforeunload" dialogs true will accept
   *    the dialog, false will cancel it. For "alert" dialogs, true or
   *    false closes the dialog in the same way.
   * @param {string=} options.promptText
   *    for "prompt" dialogs, used to fill the prompt input.
   */
  async handleJavaScriptDialog(options = {}) {
    const { accept, promptText } = options;

    if (!this.enabled) {
      throw new Error("Page domain is not enabled");
    }
    await this._dialogHandler.handleJavaScriptDialog({ accept, promptText });
  }

  /**
   * Navigates current page to the given history entry.
   *
   * @param {object} options
   * @param {number} options.entryId
   *    Unique id of the entry to navigate to.
   */
  async navigateToHistoryEntry(options = {}) {
    const { entryId } = options;

    const index = await this._getIndexForHistoryEntryId(entryId);

    if (index == null) {
      throw new Error("No entry with passed id");
    }

    const { window } = this.session.target;
    window.gBrowser.gotoIndex(index);

    // On some platforms the requested index isn't set immediately.
    await lazy.PollPromise(
      async (resolve, reject) => {
        const currentIndex = await this._getCurrentHistoryIndex();
        if (currentIndex == index) {
          resolve();
        } else {
          reject();
        }
      },
      { timeout: TIMEOUT_SET_HISTORY_INDEX }
    );
  }

  /**
   * Print page as PDF.
   *
   * @param {object} options
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
   * @returns {Promise<{data:string, stream:Stream}>}
   *     Based on the transferMode setting data is a base64-encoded string,
   *     or stream is a Stream.
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

    const psService = Cc["@mozilla.org/gfx/printsettings-service;1"].getService(
      Ci.nsIPrintSettingsService
    );

    const printSettings = psService.createNewPrintSettings();
    printSettings.isInitializedFromPrinter = true;
    printSettings.isInitializedFromPrefs = true;
    printSettings.outputFormat = Ci.nsIPrintSettings.kOutputFormatPDF;
    printSettings.printerName = "";
    printSettings.printSilent = true;

    printSettings.paperSizeUnit = Ci.nsIPrintSettings.kPaperSizeInches;
    printSettings.paperWidth = paperWidth;
    printSettings.paperHeight = paperHeight;

    // Override any os-specific unwriteable margins
    printSettings.unwriteableMarginTop = 0;
    printSettings.unwriteableMarginLeft = 0;
    printSettings.unwriteableMarginBottom = 0;
    printSettings.unwriteableMarginRight = 0;

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

    const retval = { data: null, stream: null };
    const { linkedBrowser } = this.session.target.tab;

    if (transferMode === PDF_TRANSFER_MODES.stream) {
      // If we are returning a stream, we write the PDF to disk so that we don't
      // keep (potentially very large) PDFs in memory. We can then stream them
      // to the client via the returned Stream.
      //
      // NOTE: This is a potentially premature optimization -- it might be fine
      // to keep these PDFs in memory, but we don't have specifics on how CDP is
      // used in the field so it is possible that leaving the PDFs in memory
      // could cause a regression.
      const path = await IOUtils.createUniqueFile(
        PathUtils.tempDir,
        "remote-agent.pdf"
      );

      printSettings.outputDestination =
        Ci.nsIPrintSettings.kOutputDestinationFile;
      printSettings.toFileName = path;

      await linkedBrowser.browsingContext.print(printSettings);

      retval.stream = lazy.streamRegistry.add(new lazy.Stream(path));
    } else {
      const binaryString = await lazy.print.printToBinaryString(
        linkedBrowser.browsingContext,
        printSettings
      );

      retval.data = btoa(binaryString);
    }

    return retval;
  }

  /**
   * Intercept file chooser requests and transfer control to protocol clients.
   *
   * When file chooser interception is enabled,
   * the native file chooser dialog is not shown.
   * Instead, a protocol event Page.fileChooserOpened is emitted.
   *
   * @param {object} options
   * @param {boolean=} options.enabled
   *     Enabled state of file chooser interception.
   */
  setInterceptFileChooserDialog(options = {}) {}

  _getCurrentHistoryIndex() {
    const { window } = this.session.target;

    return new Promise(resolve => {
      lazy.SessionStore.getSessionHistory(
        window.gBrowser.selectedTab,
        history => {
          resolve(history.index);
        }
      );
    });
  }

  _getIndexForHistoryEntryId(id) {
    const { window } = this.session.target;

    return new Promise(resolve => {
      function updateSessionHistory(sessionHistory) {
        sessionHistory.entries.forEach((entry, index) => {
          if (entry.ID == id) {
            resolve(index);
          }
        });

        resolve(null);
      }

      lazy.SessionStore.getSessionHistory(
        window.gBrowser.selectedTab,
        updateSessionHistory
      );
    });
  }

  /**
   * Emit the proper CDP event javascriptDialogOpening when a javascript dialog
   * opens for the current target.
   */
  _onDialogLoaded(e, data) {
    const { message, type } = data;
    // XXX: We rely on the common-dialog-loaded event (see DialogHandler.jsm)
    // which is inconsistent with the name "javascriptDialogOpening".
    // For correctness we should rely on an event fired _before_ the prompt is
    // visible, such as DOMWillOpenModalDialog. However the payload of this
    // event does not contain enough data to populate javascriptDialogOpening.
    //
    // Since the event is fired asynchronously, this should not have an impact
    // on the actual tests relying on this API.
    this.emit("Page.javascriptDialogOpening", { message, type });
  }

  /**
   * Handles HTTP request to propagate loaderId to events emitted from
   * content process
   */
  _onRequest(_type, _ch, data) {
    if (!data.loaderId) {
      return;
    }
    this.executeInChild("_updateLoaderId", {
      loaderId: data.loaderId,
      frameId: data.frameId,
    });
  }
}

function transitionToLoadFlag(transitionType) {
  switch (transitionType) {
    case "reload":
      return Ci.nsIWebNavigation.LOAD_FLAGS_IS_REFRESH;
    case "link":
    default:
      return Ci.nsIWebNavigation.LOAD_FLAGS_IS_LINK;
  }
}
