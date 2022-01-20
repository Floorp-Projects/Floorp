/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["PrintingChild"];

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

ChromeUtils.defineModuleGetter(
  this,
  "DeferredTask",
  "resource://gre/modules/DeferredTask.jsm"
);

let gPrintPreviewInitializingInfo = null;

let gPendingPreviewsMap = new Map();

class PrintingChild extends JSWindowActorChild {
  actorCreated() {
    // When the print preview page is loaded, the actor will change, so update
    // the state/progress listener to the new actor.
    let listener = gPendingPreviewsMap.get(this.browsingContext.id);
    if (listener) {
      listener.actor = this;
    }
    this.contentWindow.addEventListener("scroll", this);
  }

  didDestroy() {
    this._scrollTask?.disarm();
    this.contentWindow?.removeEventListener("scroll", this);
  }

  // Bug 1088061: nsPrintJob's DoCommonPrint currently expects the
  // progress listener passed to it to QI to an nsIPrintingPromptService
  // in order to know that a printing progress dialog has been shown. That's
  // really all the interface is used for, hence the fact that I don't actually
  // implement the interface here. Bug 1088061 has been filed to remove
  // this hackery.

  handleEvent(event) {
    switch (event.type) {
      case "PrintingError": {
        let win = event.target.defaultView;
        let wbp = win.getInterface(Ci.nsIWebBrowserPrint);
        let nsresult = event.detail;
        this.sendAsyncMessage("Printing:Error", {
          isPrinting: wbp.doingPrint,
          nsresult,
        });
        break;
      }

      case "printPreviewUpdate": {
        let info = gPrintPreviewInitializingInfo;
        if (!info) {
          // If there is no gPrintPreviewInitializingInfo then we did not
          // initiate the preview so ignore this event.
          return;
        }

        // Only send Printing:Preview:Entered message on first update, indicated
        // by gPrintPreviewInitializingInfo.entered not being set.
        if (!info.entered) {
          gPendingPreviewsMap.delete(this.browsingContext.id);

          info.entered = true;
          this.sendAsyncMessage("Printing:Preview:Entered", {
            failed: false,
            changingBrowsers: info.changingBrowsers,
          });

          // If we have another request waiting, dispatch it now.
          if (info.nextRequest) {
            Services.tm.dispatchToMainThread(info.nextRequest);
          }
        }

        // Always send page count update.
        this.updatePageCount();
        break;
      }

      case "scroll":
        if (!this._scrollTask) {
          this._scrollTask = new DeferredTask(
            () => this.updateCurrentPage(),
            16,
            16
          );
        }
        this._scrollTask.arm();
        break;
    }
  }

  receiveMessage(message) {
    let data = message.data;
    switch (message.name) {
      case "Printing:Preview:Enter": {
        this.enterPrintPreview(
          BrowsingContext.get(data.browsingContextId),
          data.simplifiedMode,
          data.changingBrowsers,
          data.lastUsedPrinterName
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
        return this.parseDocument(
          data.URL,
          Services.wm.getOuterWindowWithId(data.windowID)
        );
      }
    }

    return undefined;
  }

  getPrintSettings(lastUsedPrinterName) {
    try {
      let PSSVC = Cc["@mozilla.org/gfx/printsettings-service;1"].getService(
        Ci.nsIPrintSettingsService
      );

      let printSettings = PSSVC.newPrintSettings;
      if (!printSettings.printerName) {
        printSettings.printerName = lastUsedPrinterName;
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

  async parseDocument(URL, contentWindow) {
    // The document in 'contentWindow' will be simplified and the resulting nodes
    // will be inserted into this.contentWindow.
    let thisWindow = this.contentWindow;

    // By using ReaderMode primitives, we parse given document and place the
    // resulting JS object into the DOM of current browser.
    let article;
    try {
      article = await ReaderMode.parseDocument(contentWindow.document);
    } catch (ex) {
      Cu.reportError(ex);
    }

    await new Promise(resolve => {
      // We make use of a web progress listener in order to know when the content we inject
      // into the DOM has finished rendering. If our layout engine is still painting, we
      // will wait for MozAfterPaint event to be fired.
      let actor = thisWindow.windowGlobalChild.getActor("Printing");
      let webProgressListener = {
        onStateChange(webProgress, req, flags, status) {
          if (flags & Ci.nsIWebProgressListener.STATE_STOP) {
            webProgress.removeProgressListener(webProgressListener);
            let domUtils = contentWindow.windowUtils;
            // Here we tell the parent that we have parsed the document successfully
            // using ReaderMode primitives and we are able to enter on preview mode.
            if (domUtils.isMozAfterPaintPending) {
              let onPaint = function() {
                contentWindow.removeEventListener("MozAfterPaint", onPaint);
                actor.sendAsyncMessage("Printing:Preview:ReaderModeReady");
                resolve();
              };
              contentWindow.addEventListener("MozAfterPaint", onPaint);
              // This timer is needed for when display list invalidation doesn't invalidate.
              setTimeout(() => {
                contentWindow.removeEventListener("MozAfterPaint", onPaint);
                actor.sendAsyncMessage("Printing:Preview:ReaderModeReady");
                resolve();
              }, 100);
            } else {
              actor.sendAsyncMessage("Printing:Preview:ReaderModeReady");
              resolve();
            }
          }
        },

        QueryInterface: ChromeUtils.generateQI([
          "nsIWebProgressListener",
          "nsISupportsWeakReference",
          "nsIObserver",
        ]),
      };

      // Here we QI the docShell into a nsIWebProgress passing our web progress listener in.
      let webProgress = thisWindow.docShell
        .QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIWebProgress);
      webProgress.addProgressListener(
        webProgressListener,
        Ci.nsIWebProgress.NOTIFY_STATE_REQUEST
      );

      let document = thisWindow.document;
      document.head.innerHTML = "";

      // Set base URI of document. Print preview code will read this value to
      // populate the URL field in print settings so that it doesn't show
      // "about:blank" as its URI.
      let headBaseElement = document.createElement("base");
      headBaseElement.setAttribute("href", URL);
      document.head.appendChild(headBaseElement);

      // Create link element referencing aboutReader.css and append it to head
      let headStyleElement = document.createElement("link");
      headStyleElement.setAttribute("rel", "stylesheet");
      headStyleElement.setAttribute(
        "href",
        "chrome://global/skin/aboutReader.css"
      );
      headStyleElement.setAttribute("type", "text/css");
      document.head.appendChild(headStyleElement);

      // Create link element referencing simplifyMode.css and append it to head
      headStyleElement = document.createElement("link");
      headStyleElement.setAttribute("rel", "stylesheet");
      headStyleElement.setAttribute(
        "href",
        "chrome://global/content/simplifyMode.css"
      );
      headStyleElement.setAttribute("type", "text/css");
      document.head.appendChild(headStyleElement);

      document.body.innerHTML = "";

      // Create container div (main element) and append it to body
      let containerElement = document.createElement("div");
      containerElement.setAttribute("id", "container");
      document.body.appendChild(containerElement);

      // Reader Mode might return null if there's a failure when parsing the document.
      // We'll render the error message for the Simplify Page document when that happens.
      if (article) {
        // Set title of document
        document.title = article.title;

        // Create header div and append it to container
        let headerElement = document.createElement("div");
        headerElement.setAttribute("id", "reader-header");
        headerElement.setAttribute("class", "header");
        containerElement.appendChild(headerElement);

        // Jam the article's title and byline into header div
        let titleElement = document.createElement("h1");
        titleElement.setAttribute("id", "reader-title");
        titleElement.textContent = article.title;
        headerElement.appendChild(titleElement);

        let bylineElement = document.createElement("div");
        bylineElement.setAttribute("id", "reader-credits");
        bylineElement.setAttribute("class", "credits");
        bylineElement.textContent = article.byline;
        headerElement.appendChild(bylineElement);

        // Display header element
        headerElement.style.display = "block";

        // Create content div and append it to container
        let contentElement = document.createElement("div");
        contentElement.setAttribute("class", "content");
        containerElement.appendChild(contentElement);

        // Jam the article's content into content div
        let readerContent = document.createElement("div");
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

        document.title = errorMessage;

        // Create reader message div and append it to body
        let readerMessageElement = document.createElement("div");
        readerMessageElement.setAttribute("class", "reader-message");
        readerMessageElement.textContent = errorMessage;
        containerElement.appendChild(readerMessageElement);

        // Display reader message element
        readerMessageElement.style.display = "block";
      }
    });
  }

  enterPrintPreview(
    browsingContext,
    simplifiedMode,
    changingBrowsers,
    lastUsedPrinterName
  ) {
    const { docShell } = this;

    try {
      let contentWindow = browsingContext.window;
      let printSettings = this.getPrintSettings(lastUsedPrinterName);

      // Disable the progress dialog for generating previews.
      printSettings.showPrintProgress = !Services.prefs.getBoolPref(
        "print.tab_modal.enabled",
        false
      );

      // If we happen to be on simplified mode, we need to set docURL in order
      // to generate header/footer content correctly, since simplified tab has
      // "about:blank" as its URI.
      if (printSettings && simplifiedMode) {
        printSettings.docURL = contentWindow.document.baseURI;
      }

      // Get this early in case the actor goes away during print preview.
      let browserContextId = this.browsingContext.id;

      // The print preview docshell will be in a different TabGroup, so
      // printPreviewInitialize must be run in a separate runnable to avoid
      // touching a different TabGroup in our own runnable.
      let printPreviewInitialize = () => {
        // During dispatching this function to the main-thread, the docshell
        // might be destroyed, for example the print preview window gets closed
        // soon after it's opened, in such case we should just simply bail out.
        if (docShell.isBeingDestroyed()) {
          this.sendAsyncMessage("Printing:Preview:Entered", {
            failed: true,
          });
          return;
        }

        try {
          let listener = new PrintingListener(this);
          gPendingPreviewsMap.set(browserContextId, listener);

          gPrintPreviewInitializingInfo = { changingBrowsers };

          contentWindow.printPreview(printSettings, listener, docShell);
        } catch (error) {
          // This might fail if we, for example, attempt to print a XUL document.
          // In that case, we inform the parent to bail out of print preview.
          Cu.reportError(error);
          gPrintPreviewInitializingInfo = null;
          this.sendAsyncMessage("Printing:Preview:Entered", {
            failed: true,
          });
        }
      };

      // If gPrintPreviewInitializingInfo.entered is not set we are still in the
      // initial setup of a previous preview request. We delay this one until
      // that has finished because running them at the same time will almost
      // certainly cause failures.
      if (
        gPrintPreviewInitializingInfo &&
        !gPrintPreviewInitializingInfo.entered
      ) {
        gPrintPreviewInitializingInfo.nextRequest = printPreviewInitialize;
      } else {
        Services.tm.dispatchToMainThread(printPreviewInitialize);
      }
    } catch (error) {
      // This might fail if we, for example, attempt to print a XUL document.
      // In that case, we inform the parent to bail out of print preview.
      Cu.reportError(error);
      this.sendAsyncMessage("Printing:Preview:Entered", {
        failed: true,
      });
    }
  }

  exitPrintPreview() {
    gPrintPreviewInitializingInfo = null;
    this.docShell.exitPrintPreview();
  }

  updatePageCount() {
    let cv = this.docShell.contentViewer;
    cv.QueryInterface(Ci.nsIWebBrowserPrint);
    this.sendAsyncMessage("Printing:Preview:UpdatePageCount", {
      numPages: cv.printPreviewNumPages,
      totalPages: cv.rawNumPages,
    });
  }

  updateCurrentPage() {
    let cv = this.docShell.contentViewer;
    cv.QueryInterface(Ci.nsIWebBrowserPrint);
    this.sendAsyncMessage("Printing:Preview:CurrentPage", {
      currentPage: cv.printPreviewCurrentPageNumber,
    });
  }

  navigate(navType, pageNum) {
    let cv = this.docShell.contentViewer;
    cv.QueryInterface(Ci.nsIWebBrowserPrint);
    cv.printPreviewScrollToPage(navType, pageNum);
  }
}

PrintingChild.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsIPrintingPromptService",
]);

function PrintingListener(actor) {
  this.actor = actor;
}
PrintingListener.prototype = {
  QueryInterface: ChromeUtils.generateQI(["nsIWebProgressListener"]),

  onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
    this.actor.sendAsyncMessage("Printing:Preview:StateChange", {
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
    this.actor.sendAsyncMessage("Printing:Preview:ProgressChange", {
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
