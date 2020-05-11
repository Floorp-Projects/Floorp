/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["PrintingChild"];

const { ActorChild } = ChromeUtils.import(
  "resource://gre/modules/ActorChild.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "setTimeout",
  "resource://gre/modules/Timer.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "ReaderMode",
  "resource://gre/modules/ReaderMode.jsm"
);

class PrintingChild extends ActorChild {
  // Bug 1088061: nsPrintJob's DoCommonPrint currently expects the
  // progress listener passed to it to QI to an nsIPrintingPromptService
  // in order to know that a printing progress dialog has been shown. That's
  // really all the interface is used for, hence the fact that I don't actually
  // implement the interface here. Bug 1088061 has been filed to remove
  // this hackery.

  get shouldSavePrintSettings() {
    return Services.prefs.getBoolPref("print.save_print_settings");
  }

  handleEvent(event) {
    switch (event.type) {
      case "PrintingError": {
        let win = event.target.defaultView;
        let wbp = win.getInterface(Ci.nsIWebBrowserPrint);
        let nsresult = event.detail;
        this.mm.sendAsyncMessage("Printing:Error", {
          isPrinting: wbp.doingPrint,
          nsresult,
        });
        break;
      }

      case "printPreviewUpdate": {
        let info = this.printPreviewInitializingInfo;
        if (!info) {
          // If there is no printPreviewInitializingInfo then we did not
          // initiate the preview so ignore this event.
          return;
        }

        // Only send Printing:Preview:Entered message on first update, indicated
        // by printPreviewInitializingInfo.entered not being set.
        if (!info.entered) {
          info.entered = true;
          this.mm.sendAsyncMessage("Printing:Preview:Entered", {
            failed: false,
            changingBrowsers: info.changingBrowsers,
          });

          // If we have another request waiting, dispatch it now.
          if (info.nextRequest) {
            Services.tm.dispatchToMainThread(info.nextRequest);
          }
        }

        // Always send page count update.
        this.updatePageCount(this.mm);
        break;
      }
    }
  }

  receiveMessage(message) {
    let data = message.data;
    switch (message.name) {
      case "Printing:Preview:Enter": {
        this.enterPrintPreview(
          Services.wm.getOuterWindowWithId(data.windowID),
          data.simplifiedMode,
          data.changingBrowsers,
          data.defaultPrinterName
        );
        break;
      }

      case "Printing:Preview:Exit": {
        this.exitPrintPreview();
        break;
      }

      case "Printing:Preview:Navigate": {
        this.navigate(data.navType, data.pageNum);
        break;
      }

      case "Printing:Preview:ParseDocument": {
        this.parseDocument(
          data.URL,
          Services.wm.getOuterWindowWithId(data.windowID)
        );
        break;
      }

      case "Printing:Print": {
        this.print(
          Services.wm.getOuterWindowWithId(data.windowID),
          data.simplifiedMode,
          data.defaultPrinterName
        );
        break;
      }
    }
  }

  getPrintSettings(defaultPrinterName) {
    try {
      let PSSVC = Cc["@mozilla.org/gfx/printsettings-service;1"].getService(
        Ci.nsIPrintSettingsService
      );

      let printSettings = PSSVC.globalPrintSettings;
      if (!printSettings.printerName) {
        printSettings.printerName = defaultPrinterName;
      }
      // First get any defaults from the printer
      PSSVC.initPrintSettingsFromPrinter(
        printSettings.printerName,
        printSettings
      );
      // now augment them with any values from last time
      PSSVC.initPrintSettingsFromPrefs(
        printSettings,
        true,
        printSettings.kInitSaveAll
      );

      return printSettings;
    } catch (e) {
      Cu.reportError(e);
    }

    return null;
  }

  parseDocument(URL, contentWindow) {
    // By using ReaderMode primitives, we parse given document and place the
    // resulting JS object into the DOM of current browser.
    let articlePromise = ReaderMode.parseDocument(contentWindow.document).catch(
      Cu.reportError
    );
    articlePromise.then(article => {
      // We make use of a web progress listener in order to know when the content we inject
      // into the DOM has finished rendering. If our layout engine is still painting, we
      // will wait for MozAfterPaint event to be fired.
      let { mm } = this;
      let webProgressListener = {
        onStateChange(webProgress, req, flags, status) {
          if (flags & Ci.nsIWebProgressListener.STATE_STOP) {
            webProgress.removeProgressListener(webProgressListener);
            let domUtils = contentWindow.windowUtils;
            // Here we tell the parent that we have parsed the document successfully
            // using ReaderMode primitives and we are able to enter on preview mode.
            if (domUtils.isMozAfterPaintPending) {
              let onPaint = function() {
                mm.removeEventListener("MozAfterPaint", onPaint);
                mm.sendAsyncMessage("Printing:Preview:ReaderModeReady");
              };
              contentWindow.addEventListener("MozAfterPaint", onPaint);
              // This timer need when display list invalidation doesn't invalidate.
              setTimeout(() => {
                mm.removeEventListener("MozAfterPaint", onPaint);
                mm.sendAsyncMessage("Printing:Preview:ReaderModeReady");
              }, 100);
            } else {
              mm.sendAsyncMessage("Printing:Preview:ReaderModeReady");
            }
          }
        },

        QueryInterface: ChromeUtils.generateQI([
          Ci.nsIWebProgressListener,
          Ci.nsISupportsWeakReference,
          Ci.nsIObserver,
        ]),
      };

      const { content, docShell } = this.mm;

      // Here we QI the docShell into a nsIWebProgress passing our web progress listener in.
      let webProgress = docShell
        .QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIWebProgress);
      webProgress.addProgressListener(
        webProgressListener,
        Ci.nsIWebProgress.NOTIFY_STATE_REQUEST
      );

      content.document.head.innerHTML = "";

      // Set base URI of document. Print preview code will read this value to
      // populate the URL field in print settings so that it doesn't show
      // "about:blank" as its URI.
      let headBaseElement = content.document.createElement("base");
      headBaseElement.setAttribute("href", URL);
      content.document.head.appendChild(headBaseElement);

      // Create link element referencing aboutReader.css and append it to head
      let headStyleElement = content.document.createElement("link");
      headStyleElement.setAttribute("rel", "stylesheet");
      headStyleElement.setAttribute(
        "href",
        "chrome://global/skin/aboutReader.css"
      );
      headStyleElement.setAttribute("type", "text/css");
      content.document.head.appendChild(headStyleElement);

      // Create link element referencing simplifyMode.css and append it to head
      headStyleElement = content.document.createElement("link");
      headStyleElement.setAttribute("rel", "stylesheet");
      headStyleElement.setAttribute(
        "href",
        "chrome://global/content/simplifyMode.css"
      );
      headStyleElement.setAttribute("type", "text/css");
      content.document.head.appendChild(headStyleElement);

      content.document.body.innerHTML = "";

      // Create container div (main element) and append it to body
      let containerElement = content.document.createElement("div");
      containerElement.setAttribute("id", "container");
      content.document.body.appendChild(containerElement);

      // Reader Mode might return null if there's a failure when parsing the document.
      // We'll render the error message for the Simplify Page document when that happens.
      if (article) {
        // Set title of document
        content.document.title = article.title;

        // Create header div and append it to container
        let headerElement = content.document.createElement("div");
        headerElement.setAttribute("id", "reader-header");
        headerElement.setAttribute("class", "header");
        containerElement.appendChild(headerElement);

        // Jam the article's title and byline into header div
        let titleElement = content.document.createElement("h1");
        titleElement.setAttribute("id", "reader-title");
        titleElement.textContent = article.title;
        headerElement.appendChild(titleElement);

        let bylineElement = content.document.createElement("div");
        bylineElement.setAttribute("id", "reader-credits");
        bylineElement.setAttribute("class", "credits");
        bylineElement.textContent = article.byline;
        headerElement.appendChild(bylineElement);

        // Display header element
        headerElement.style.display = "block";

        // Create content div and append it to container
        let contentElement = content.document.createElement("div");
        contentElement.setAttribute("class", "content");
        containerElement.appendChild(contentElement);

        // Jam the article's content into content div
        let readerContent = content.document.createElement("div");
        readerContent.setAttribute("id", "moz-reader-content");
        contentElement.appendChild(readerContent);

        let articleUri = Services.io.newURI(article.url);
        let parserUtils = Cc["@mozilla.org/parserutils;1"].getService(
          Ci.nsIParserUtils
        );
        let contentFragment = parserUtils.parseFragment(
          article.content,
          Ci.nsIParserUtils.SanitizerDropForms |
            Ci.nsIParserUtils.SanitizerAllowStyle,
          false,
          articleUri,
          readerContent
        );

        readerContent.appendChild(contentFragment);

        // Display reader content element
        readerContent.style.display = "block";
      } else {
        let aboutReaderStrings = Services.strings.createBundle(
          "chrome://global/locale/aboutReader.properties"
        );
        let errorMessage = aboutReaderStrings.GetStringFromName(
          "aboutReader.loadError"
        );

        content.document.title = errorMessage;

        // Create reader message div and append it to body
        let readerMessageElement = content.document.createElement("div");
        readerMessageElement.setAttribute("class", "reader-message");
        readerMessageElement.textContent = errorMessage;
        containerElement.appendChild(readerMessageElement);

        // Display reader message element
        readerMessageElement.style.display = "block";
      }
    });
  }

  enterPrintPreview(
    contentWindow,
    simplifiedMode,
    changingBrowsers,
    defaultPrinterName
  ) {
    const { docShell } = this;
    try {
      let printSettings = this.getPrintSettings(defaultPrinterName);

      // If we happen to be on simplified mode, we need to set docURL in order
      // to generate header/footer content correctly, since simplified tab has
      // "about:blank" as its URI.
      if (printSettings && simplifiedMode) {
        printSettings.docURL = contentWindow.document.baseURI;
      }

      // The print preview docshell will be in a different TabGroup, so
      // printPreviewInitialize must be run in a separate runnable to avoid
      // touching a different TabGroup in our own runnable.
      let printPreviewInitialize = () => {
        try {
          let listener = new PrintingListener(this.mm);

          this.printPreviewInitializingInfo = { changingBrowsers };
          docShell
            .initOrReusePrintPreviewViewer()
            .printPreview(printSettings, contentWindow, listener);
        } catch (error) {
          // This might fail if we, for example, attempt to print a XUL document.
          // In that case, we inform the parent to bail out of print preview.
          Cu.reportError(error);
          this.printPreviewInitializingInfo = null;
          this.mm.sendAsyncMessage("Printing:Preview:Entered", {
            failed: true,
          });
        }
      };

      // If printPreviewInitializingInfo.entered is not set we are still in the
      // initial setup of a previous preview request. We delay this one until
      // that has finished because running them at the same time will almost
      // certainly cause failures.
      if (
        this.printPreviewInitializingInfo &&
        !this.printPreviewInitializingInfo.entered
      ) {
        this.printPreviewInitializingInfo.nextRequest = printPreviewInitialize;
      } else {
        Services.tm.dispatchToMainThread(printPreviewInitialize);
      }
    } catch (error) {
      // This might fail if we, for example, attempt to print a XUL document.
      // In that case, we inform the parent to bail out of print preview.
      Cu.reportError(error);
      this.mm.sendAsyncMessage("Printing:Preview:Entered", { failed: true });
    }
  }

  exitPrintPreview(glo) {
    this.printPreviewInitializingInfo = null;
    this.docShell.initOrReusePrintPreviewViewer().exitPrintPreview();
  }

  print(contentWindow, simplifiedMode, defaultPrinterName) {
    let printSettings = this.getPrintSettings(defaultPrinterName);
    // Set the title so that the print dialog can pick it up and
    // use it to generate the filename for save-to-PDF.
    printSettings.title = contentWindow.document.title;

    // If we happen to be on simplified mode, we need to set docURL in order
    // to generate header/footer content correctly, since simplified tab has
    // "about:blank" as its URI.
    if (printSettings && simplifiedMode) {
      printSettings.docURL = contentWindow.document.baseURI;
    }

    try {
      contentWindow
        .getInterface(Ci.nsIWebBrowserPrint)
        .print(printSettings, null);
    } catch (e) {
      // Pressing cancel is expressed as an NS_ERROR_ABORT return value,
      // causing an exception to be thrown which we catch here.
      if (e.result != Cr.NS_ERROR_ABORT) {
        Cu.reportError(`In Printing:Print:Done handler, got unexpected rv
                        ${e.result}.`);
        this.mm.sendAsyncMessage("Printing:Error", {
          isPrinting: true,
          nsresult: e.result,
        });
      }
    }
  }

  updatePageCount() {
    let numPages = this.docShell.initOrReusePrintPreviewViewer()
      .printPreviewNumPages;
    this.mm.sendAsyncMessage("Printing:Preview:UpdatePageCount", {
      numPages,
    });
  }

  navigate(navType, pageNum) {
    this.docShell
      .initOrReusePrintPreviewViewer()
      .printPreviewScrollToPage(navType, pageNum);
  }
}

PrintingChild.prototype.QueryInterface = ChromeUtils.generateQI([
  Ci.nsIPrintingPromptService,
]);

function PrintingListener(global) {
  this.global = global;
}
PrintingListener.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIWebProgressListener]),

  onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
    this.global.sendAsyncMessage("Printing:Preview:StateChange", {
      stateFlags: aStateFlags,
      status: aStatus,
    });
  },

  onProgressChange(
    aWebProgress,
    aRequest,
    aCurSelfProgress,
    aMaxSelfProgress,
    aCurTotalProgress,
    aMaxTotalProgress
  ) {
    this.global.sendAsyncMessage("Printing:Preview:ProgressChange", {
      curSelfProgress: aCurSelfProgress,
      maxSelfProgress: aMaxSelfProgress,
      curTotalProgress: aCurTotalProgress,
      maxTotalProgress: aMaxTotalProgress,
    });
  },

  onLocationChange(aWebProgress, aRequest, aLocation, aFlags) {},
  onStatusChange(aWebProgress, aRequest, aStatus, aMessage) {},
  onSecurityChange(aWebProgress, aRequest, aState) {},
  onContentBlockingEvent(aWebProgress, aRequest, aEvent) {},
};
