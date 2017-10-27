/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */
/* global sendAsyncMessage */

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;
var Cr = Components.results;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ReaderMode",
  "resource://gre/modules/ReaderMode.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "BrowserUtils",
  "resource://gre/modules/BrowserUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "SelectContentHelper",
  "resource://gre/modules/SelectContentHelper.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FindContent",
  "resource://gre/modules/FindContent.jsm");

var global = this;


// Lazily load the finder code
addMessageListener("Finder:Initialize", function() {
  let {RemoteFinderListener} = Cu.import("resource://gre/modules/RemoteFinder.jsm", {});
  new RemoteFinderListener(global);
});

var ClickEventHandler = {
  init: function init() {
    this._scrollable = null;
    this._scrolldir = "";
    this._startX = null;
    this._startY = null;
    this._screenX = null;
    this._screenY = null;
    this._lastFrame = null;
    this._autoscrollHandledByApz = false;
    this._scrollId = null;
    this.autoscrollLoop = this.autoscrollLoop.bind(this);

    Services.els.addSystemEventListener(global, "mousedown", this, true);

    addMessageListener("Autoscroll:Stop", this);
  },

  isAutoscrollBlocker(node) {
    let mmPaste = Services.prefs.getBoolPref("middlemouse.paste");
    let mmScrollbarPosition = Services.prefs.getBoolPref("middlemouse.scrollbarPosition");

    while (node) {
      if ((node instanceof content.HTMLAnchorElement || node instanceof content.HTMLAreaElement) &&
          node.hasAttribute("href")) {
        return true;
      }

      if (mmPaste && (node instanceof content.HTMLInputElement ||
                      node instanceof content.HTMLTextAreaElement)) {
        return true;
      }

      if (node instanceof content.XULElement && mmScrollbarPosition
          && (node.localName == "scrollbar" || node.localName == "scrollcorner")) {
        return true;
      }

      node = node.parentNode;
    }
    return false;
  },

  findNearestScrollableElement(aNode) {
    // this is a list of overflow property values that allow scrolling
    const scrollingAllowed = ["scroll", "auto"];

    // go upward in the DOM and find any parent element that has a overflow
    // area and can therefore be scrolled
    for (this._scrollable = aNode; this._scrollable;
         this._scrollable = this._scrollable.parentNode) {
      // do not use overflow based autoscroll for <html> and <body>
      // Elements or non-html elements such as svg or Document nodes
      // also make sure to skip select elements that are not multiline
      if (!(this._scrollable instanceof content.HTMLElement) ||
          ((this._scrollable instanceof content.HTMLSelectElement) && !this._scrollable.multiple)) {
        continue;
      }

      var overflowx = this._scrollable.ownerGlobal
                          .getComputedStyle(this._scrollable)
                          .getPropertyValue("overflow-x");
      var overflowy = this._scrollable.ownerGlobal
                          .getComputedStyle(this._scrollable)
                          .getPropertyValue("overflow-y");
      // we already discarded non-multiline selects so allow vertical
      // scroll for multiline ones directly without checking for a
      // overflow property
      var scrollVert = this._scrollable.scrollTopMax &&
        (this._scrollable instanceof content.HTMLSelectElement ||
         scrollingAllowed.indexOf(overflowy) >= 0);

      // do not allow horizontal scrolling for select elements, it leads
      // to visual artifacts and is not the expected behavior anyway
      if (!(this._scrollable instanceof content.HTMLSelectElement) &&
          this._scrollable.scrollLeftMin != this._scrollable.scrollLeftMax &&
          scrollingAllowed.indexOf(overflowx) >= 0) {
        this._scrolldir = scrollVert ? "NSEW" : "EW";
        break;
      } else if (scrollVert) {
        this._scrolldir = "NS";
        break;
      }
    }

    if (!this._scrollable) {
      this._scrollable = aNode.ownerGlobal;
      if (this._scrollable.scrollMaxX != this._scrollable.scrollMinX) {
        this._scrolldir = this._scrollable.scrollMaxY !=
                          this._scrollable.scrollMinY ? "NSEW" : "EW";
      } else if (this._scrollable.scrollMaxY != this._scrollable.scrollMinY) {
        this._scrolldir = "NS";
      } else if (this._scrollable.frameElement) {
        this.findNearestScrollableElement(this._scrollable.frameElement);
      } else {
        this._scrollable = null; // abort scrolling
      }
    }
  },

  startScroll(event) {

    this.findNearestScrollableElement(event.originalTarget);

    if (!this._scrollable)
      return;

    // In some configurations like Print Preview, content.performance
    // (which we use below) is null. Autoscrolling is broken in Print
    // Preview anyways (see bug 1393494), so just don't start it at all.
    if (!content.performance)
      return;

    let domUtils = content.QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIDOMWindowUtils);
    let scrollable = this._scrollable;
    if (scrollable instanceof Ci.nsIDOMWindow) {
      // getViewId() needs an element to operate on.
      scrollable = scrollable.document.documentElement;
    }
    this._scrollId = null;
    try {
      this._scrollId = domUtils.getViewId(scrollable);
    } catch (e) {
      // No view ID - leave this._scrollId as null. Receiving side will check.
    }
    let presShellId = domUtils.getPresShellId();
    let [result] = sendSyncMessage("Autoscroll:Start",
                                   {scrolldir: this._scrolldir,
                                    screenX: event.screenX,
                                    screenY: event.screenY,
                                    scrollId: this._scrollId,
                                    presShellId});
    if (!result.autoscrollEnabled) {
      this._scrollable = null;
      return;
    }

    Services.els.addSystemEventListener(global, "mousemove", this, true);
    addEventListener("pagehide", this, true);

    this._ignoreMouseEvents = true;
    this._startX = event.screenX;
    this._startY = event.screenY;
    this._screenX = event.screenX;
    this._screenY = event.screenY;
    this._scrollErrorX = 0;
    this._scrollErrorY = 0;
    this._autoscrollHandledByApz = result.usingApz;

    if (!result.usingApz) {
      // If the browser didn't hand the autoscroll off to APZ,
      // scroll here in the main thread.
      this.startMainThreadScroll();
    } else {
      // Even if the browser did hand the autoscroll to APZ,
      // APZ might reject it in which case it will notify us
      // and we need to take over.
      Services.obs.addObserver(this, "autoscroll-rejected-by-apz");
    }
  },

  startMainThreadScroll() {
    this._lastFrame = content.performance.now();
    content.requestAnimationFrame(this.autoscrollLoop);
  },

  stopScroll() {
    if (this._scrollable) {
      this._scrollable.mozScrollSnap();
      this._scrollable = null;

      Services.els.removeSystemEventListener(global, "mousemove", this, true);
      removeEventListener("pagehide", this, true);
      if (this._autoscrollHandledByApz) {
        Services.obs.removeObserver(this, "autoscroll-rejected-by-apz");
      }
    }
  },

  accelerate(curr, start) {
    const speed = 12;
    var val = (curr - start) / speed;

    if (val > 1)
      return val * Math.sqrt(val) - 1;
    if (val < -1)
      return val * Math.sqrt(-val) + 1;
    return 0;
  },

  roundToZero(num) {
    if (num > 0)
      return Math.floor(num);
    return Math.ceil(num);
  },

  autoscrollLoop(timestamp) {
    if (!this._scrollable) {
      // Scrolling has been canceled
      return;
    }

    // avoid long jumps when the browser hangs for more than
    // |maxTimeDelta| ms
    const maxTimeDelta = 100;
    var timeDelta = Math.min(maxTimeDelta, timestamp - this._lastFrame);
    // we used to scroll |accelerate()| pixels every 20ms (50fps)
    var timeCompensation = timeDelta / 20;
    this._lastFrame = timestamp;

    var actualScrollX = 0;
    var actualScrollY = 0;
    // don't bother scrolling vertically when the scrolldir is only horizontal
    // and the other way around
    if (this._scrolldir != "EW") {
      var y = this.accelerate(this._screenY, this._startY) * timeCompensation;
      var desiredScrollY = this._scrollErrorY + y;
      actualScrollY = this.roundToZero(desiredScrollY);
      this._scrollErrorY = (desiredScrollY - actualScrollY);
    }
    if (this._scrolldir != "NS") {
      var x = this.accelerate(this._screenX, this._startX) * timeCompensation;
      var desiredScrollX = this._scrollErrorX + x;
      actualScrollX = this.roundToZero(desiredScrollX);
      this._scrollErrorX = (desiredScrollX - actualScrollX);
    }

    const kAutoscroll = 15; // defined in mozilla/layers/ScrollInputMethods.h
    Services.telemetry.getHistogramById("SCROLL_INPUT_METHODS").add(kAutoscroll);

    this._scrollable.scrollBy({
      left: actualScrollX,
      top: actualScrollY,
      behavior: "instant"
    });

    content.requestAnimationFrame(this.autoscrollLoop);
  },

  handleEvent(event) {
    if (event.type == "mousemove") {
      this._screenX = event.screenX;
      this._screenY = event.screenY;
    } else if (event.type == "mousedown") {
      if (event.isTrusted &
          !event.defaultPrevented &&
          event.button == 1 &&
          !this._scrollable &&
          !this.isAutoscrollBlocker(event.originalTarget)) {
        this.startScroll(event);
      }
    } else if (event.type == "pagehide") {
      if (this._scrollable) {
        var doc =
          this._scrollable.ownerDocument || this._scrollable.document;
        if (doc == event.target) {
          sendAsyncMessage("Autoscroll:Cancel");
        }
      }
    }
  },

  receiveMessage(msg) {
    switch (msg.name) {
      case "Autoscroll:Stop": {
        this.stopScroll();
        break;
      }
    }
  },

  observe(subject, topic, data) {
    if (topic === "autoscroll-rejected-by-apz") {
      // The caller passes in the scroll id via 'data'.
      if (data == this._scrollId) {
        this._autoscrollHandledByApz = false;
        this.startMainThreadScroll();
        Services.obs.removeObserver(this, "autoscroll-rejected-by-apz");
      }
    }
  },
};
ClickEventHandler.init();

var PopupBlocking = {
  popupData: null,
  popupDataInternal: null,

  init() {
    addEventListener("DOMPopupBlocked", this, true);
    addEventListener("pageshow", this, true);
    addEventListener("pagehide", this, true);

    addMessageListener("PopupBlocking:UnblockPopup", this);
    addMessageListener("PopupBlocking:GetBlockedPopupList", this);
  },

  receiveMessage(msg) {
    switch (msg.name) {
      case "PopupBlocking:UnblockPopup": {
        let i = msg.data.index;
        if (this.popupData && this.popupData[i]) {
          let data = this.popupData[i];
          let internals = this.popupDataInternal[i];
          let dwi = internals.requestingWindow;

          // If we have a requesting window and the requesting document is
          // still the current document, open the popup.
          if (dwi && dwi.document == internals.requestingDocument) {
            dwi.open(data.popupWindowURIspec, data.popupWindowName, data.popupWindowFeatures);
          }
        }
        break;
      }

      case "PopupBlocking:GetBlockedPopupList": {
        let popupData = [];
        let length = this.popupData ? this.popupData.length : 0;

        // Limit 15 popup URLs to be reported through the UI
        length = Math.min(length, 15);

        for (let i = 0; i < length; i++) {
          let popupWindowURIspec = this.popupData[i].popupWindowURIspec;

          if (popupWindowURIspec == global.content.location.href) {
            popupWindowURIspec = "<self>";
          } else {
            // Limit 500 chars to be sent because the URI will be cropped
            // by the UI anyway, and data: URIs can be significantly larger.
            popupWindowURIspec = popupWindowURIspec.substring(0, 500);
          }

          popupData.push({popupWindowURIspec});
        }

        sendAsyncMessage("PopupBlocking:ReplyGetBlockedPopupList", {popupData});
        break;
      }
    }
  },

  handleEvent(ev) {
    switch (ev.type) {
      case "DOMPopupBlocked":
        return this.onPopupBlocked(ev);
      case "pageshow":
        return this._removeIrrelevantPopupData();
      case "pagehide":
        return this._removeIrrelevantPopupData(ev.target);
    }
    return undefined;
  },

  onPopupBlocked(ev) {
    if (!this.popupData) {
      this.popupData = [];
      this.popupDataInternal = [];
    }

    let obj = {
      popupWindowURIspec: ev.popupWindowURI ? ev.popupWindowURI.spec : "about:blank",
      popupWindowFeatures: ev.popupWindowFeatures,
      popupWindowName: ev.popupWindowName
    };

    let internals = {
      requestingWindow: ev.requestingWindow,
      requestingDocument: ev.requestingWindow.document,
    };

    this.popupData.push(obj);
    this.popupDataInternal.push(internals);
    this.updateBlockedPopups(true);
  },

  _removeIrrelevantPopupData(removedDoc = null) {
    if (this.popupData) {
      let i = 0;
      let oldLength = this.popupData.length;
      while (i < this.popupData.length) {
        let {requestingWindow, requestingDocument} = this.popupDataInternal[i];
        // Filter out irrelevant reports.
        if (requestingWindow && requestingWindow.document == requestingDocument &&
            requestingDocument != removedDoc) {
          i++;
        } else {
          this.popupData.splice(i, 1);
          this.popupDataInternal.splice(i, 1);
        }
      }
      if (this.popupData.length == 0) {
        this.popupData = null;
        this.popupDataInternal = null;
      }
      if (!this.popupData || oldLength > this.popupData.length) {
        this.updateBlockedPopups(false);
      }
    }
  },

  updateBlockedPopups(freshPopup) {
    sendAsyncMessage("PopupBlocking:UpdateBlockedPopups",
      {
        count: this.popupData ? this.popupData.length : 0,
        freshPopup
      });
  },
};
PopupBlocking.init();

XPCOMUtils.defineLazyGetter(this, "console", () => {
  // Set up console.* for frame scripts.
  let Console = Components.utils.import("resource://gre/modules/Console.jsm", {});
  return new Console.ConsoleAPI();
});

var Printing = {
  // Bug 1088061: nsPrintEngine's DoCommonPrint currently expects the
  // progress listener passed to it to QI to an nsIPrintingPromptService
  // in order to know that a printing progress dialog has been shown. That's
  // really all the interface is used for, hence the fact that I don't actually
  // implement the interface here. Bug 1088061 has been filed to remove
  // this hackery.
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                         Ci.nsIPrintingPromptService]),

  MESSAGES: [
    "Printing:Preview:Enter",
    "Printing:Preview:Exit",
    "Printing:Preview:Navigate",
    "Printing:Preview:ParseDocument",
    "Printing:Print",
  ],

  init() {
    this.MESSAGES.forEach(msgName => addMessageListener(msgName, this));
    addEventListener("PrintingError", this, true);
    addEventListener("printPreviewUpdate", this, true);
  },

  get shouldSavePrintSettings() {
    return Services.prefs.getBoolPref("print.use_global_printsettings") &&
           Services.prefs.getBoolPref("print.save_print_settings");
  },

  printPreviewInitializingInfo: null,

  handleEvent(event) {
    switch (event.type) {
      case "PrintingError": {
        let win = event.target.defaultView;
        let wbp = win.QueryInterface(Ci.nsIInterfaceRequestor)
                     .getInterface(Ci.nsIWebBrowserPrint);
        let nsresult = event.detail;
        sendAsyncMessage("Printing:Error", {
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
          sendAsyncMessage("Printing:Preview:Entered", {
            failed: false,
            changingBrowsers: info.changingBrowsers
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
    }
  },

  receiveMessage(message) {
    let data = message.data;
    switch (message.name) {
      case "Printing:Preview:Enter": {
        this.enterPrintPreview(Services.wm.getOuterWindowWithId(data.windowID), data.simplifiedMode, data.changingBrowsers, data.defaultPrinterName);
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
        this.parseDocument(data.URL, Services.wm.getOuterWindowWithId(data.windowID));
        break;
      }

      case "Printing:Print": {
        this.print(Services.wm.getOuterWindowWithId(data.windowID), data.simplifiedMode, data.defaultPrinterName);
        break;
      }
    }
  },

  getPrintSettings(defaultPrinterName) {
    try {
      let PSSVC = Cc["@mozilla.org/gfx/printsettings-service;1"]
                    .getService(Ci.nsIPrintSettingsService);

      let printSettings = PSSVC.globalPrintSettings;
      if (!printSettings.printerName) {
        printSettings.printerName = defaultPrinterName;
      }
      // First get any defaults from the printer
      PSSVC.initPrintSettingsFromPrinter(printSettings.printerName,
                                         printSettings);
      // now augment them with any values from last time
      PSSVC.initPrintSettingsFromPrefs(printSettings, true,
                                       printSettings.kInitSaveAll);

      return printSettings;
    } catch (e) {
      Components.utils.reportError(e);
    }

    return null;
  },

  parseDocument(URL, contentWindow) {
    // By using ReaderMode primitives, we parse given document and place the
    // resulting JS object into the DOM of current browser.
    let articlePromise = ReaderMode.parseDocument(contentWindow.document).catch(Cu.reportError);
    articlePromise.then(function(article) {
      // We make use of a web progress listener in order to know when the content we inject
      // into the DOM has finished rendering. If our layout engine is still painting, we
      // will wait for MozAfterPaint event to be fired.
      let webProgressListener = {
        onStateChange(webProgress, req, flags, status) {
          if (flags & Ci.nsIWebProgressListener.STATE_STOP) {
            webProgress.removeProgressListener(webProgressListener);
            let domUtils = content.QueryInterface(Ci.nsIInterfaceRequestor)
                                  .getInterface(Ci.nsIDOMWindowUtils);
            // Here we tell the parent that we have parsed the document successfully
            // using ReaderMode primitives and we are able to enter on preview mode.
            if (domUtils.isMozAfterPaintPending) {
              addEventListener("MozAfterPaint", function onPaint() {
                removeEventListener("MozAfterPaint", onPaint);
                sendAsyncMessage("Printing:Preview:ReaderModeReady");
              });
            } else {
              sendAsyncMessage("Printing:Preview:ReaderModeReady");
            }
          }
        },

        QueryInterface: XPCOMUtils.generateQI([
          Ci.nsIWebProgressListener,
          Ci.nsISupportsWeakReference,
          Ci.nsIObserver,
        ]),
      };

      // Here we QI the docShell into a nsIWebProgress passing our web progress listener in.
      let webProgress =  docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                                 .getInterface(Ci.nsIWebProgress);
      webProgress.addProgressListener(webProgressListener, Ci.nsIWebProgress.NOTIFY_STATE_REQUEST);

      content.document.head.innerHTML = "";

      // Set title of document
      content.document.title = article.title;

      // Set base URI of document. Print preview code will read this value to
      // populate the URL field in print settings so that it doesn't show
      // "about:blank" as its URI.
      let headBaseElement = content.document.createElement("base");
      headBaseElement.setAttribute("href", URL);
      content.document.head.appendChild(headBaseElement);

      // Create link element referencing aboutReader.css and append it to head
      let headStyleElement = content.document.createElement("link");
      headStyleElement.setAttribute("rel", "stylesheet");
      headStyleElement.setAttribute("href", "chrome://global/skin/aboutReader.css");
      headStyleElement.setAttribute("type", "text/css");
      content.document.head.appendChild(headStyleElement);

      // Create link element referencing simplifyMode.css and append it to head
      headStyleElement = content.document.createElement("link");
      headStyleElement.setAttribute("rel", "stylesheet");
      headStyleElement.setAttribute("href", "chrome://global/content/simplifyMode.css");
      headStyleElement.setAttribute("type", "text/css");
      content.document.head.appendChild(headStyleElement);

      content.document.body.innerHTML = "";

      // Create container div (main element) and append it to body
      let containerElement = content.document.createElement("div");
      containerElement.setAttribute("id", "container");
      content.document.body.appendChild(containerElement);

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

      // Create style element for content div and import aboutReaderContent.css
      let controlContentStyle = content.document.createElement("style");
      controlContentStyle.setAttribute("scoped", "");
      controlContentStyle.textContent = "@import url(\"chrome://global/skin/aboutReaderContent.css\");";
      contentElement.appendChild(controlContentStyle);

      // Jam the article's content into content div
      let readerContent = content.document.createElement("div");
      readerContent.setAttribute("id", "moz-reader-content");
      contentElement.appendChild(readerContent);

      let articleUri = Services.io.newURI(article.url);
      let parserUtils = Cc["@mozilla.org/parserutils;1"].getService(Ci.nsIParserUtils);
      let contentFragment = parserUtils.parseFragment(article.content,
        Ci.nsIParserUtils.SanitizerDropForms | Ci.nsIParserUtils.SanitizerAllowStyle,
        false, articleUri, readerContent);

      readerContent.appendChild(contentFragment);

      // Display reader content element
      readerContent.style.display = "block";
    });
  },

  enterPrintPreview(contentWindow, simplifiedMode, changingBrowsers, defaultPrinterName) {
    try {
      let printSettings = this.getPrintSettings(defaultPrinterName);

      // If we happen to be on simplified mode, we need to set docURL in order
      // to generate header/footer content correctly, since simplified tab has
      // "about:blank" as its URI.
      if (printSettings && simplifiedMode)
        printSettings.docURL = contentWindow.document.baseURI;

      // The print preview docshell will be in a different TabGroup, so
      // printPreviewInitialize must be run in a separate runnable to avoid
      // touching a different TabGroup in our own runnable.
      let printPreviewInitialize = () => {
        try {
          this.printPreviewInitializingInfo = { changingBrowsers };
          docShell.printPreview.printPreview(printSettings, contentWindow, this);
        } catch (error) {
          // This might fail if we, for example, attempt to print a XUL document.
          // In that case, we inform the parent to bail out of print preview.
          Components.utils.reportError(error);
          this.printPreviewInitializingInfo = null;
          sendAsyncMessage("Printing:Preview:Entered", { failed: true });
        }
      };

      // If printPreviewInitializingInfo.entered is not set we are still in the
      // initial setup of a previous preview request. We delay this one until
      // that has finished because running them at the same time will almost
      // certainly cause failures.
      if (this.printPreviewInitializingInfo &&
          !this.printPreviewInitializingInfo.entered) {
        this.printPreviewInitializingInfo.nextRequest = printPreviewInitialize;
      } else {
        Services.tm.dispatchToMainThread(printPreviewInitialize);
      }
    } catch (error) {
      // This might fail if we, for example, attempt to print a XUL document.
      // In that case, we inform the parent to bail out of print preview.
      Components.utils.reportError(error);
      sendAsyncMessage("Printing:Preview:Entered", { failed: true });
    }
  },

  exitPrintPreview() {
    this.printPreviewInitializingInfo = null;
    docShell.printPreview.exitPrintPreview();
  },

  print(contentWindow, simplifiedMode, defaultPrinterName) {
    let printSettings = this.getPrintSettings(defaultPrinterName);

    // If we happen to be on simplified mode, we need to set docURL in order
    // to generate header/footer content correctly, since simplified tab has
    // "about:blank" as its URI.
    if (printSettings && simplifiedMode) {
      printSettings.docURL = contentWindow.document.baseURI;
    }

    try {
      let print = contentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                               .getInterface(Ci.nsIWebBrowserPrint);

      if (print.doingPrintPreview) {
        this.logKeyedTelemetry("PRINT_DIALOG_OPENED_COUNT", "FROM_PREVIEW");
      } else {
        this.logKeyedTelemetry("PRINT_DIALOG_OPENED_COUNT", "FROM_PAGE");
      }

      print.print(printSettings, null);

      if (print.doingPrintPreview) {
        if (simplifiedMode) {
          this.logKeyedTelemetry("PRINT_COUNT", "SIMPLIFIED");
        } else {
          this.logKeyedTelemetry("PRINT_COUNT", "WITH_PREVIEW");
        }
      } else {
        this.logKeyedTelemetry("PRINT_COUNT", "WITHOUT_PREVIEW");
      }
    } catch (e) {
      // Pressing cancel is expressed as an NS_ERROR_ABORT return value,
      // causing an exception to be thrown which we catch here.
      if (e.result != Cr.NS_ERROR_ABORT) {
        Cu.reportError(`In Printing:Print:Done handler, got unexpected rv
                        ${e.result}.`);
        sendAsyncMessage("Printing:Error", {
          isPrinting: true,
          nsresult: e.result,
        });
      }
    }

    if (this.shouldSavePrintSettings) {
      let PSSVC = Cc["@mozilla.org/gfx/printsettings-service;1"]
                    .getService(Ci.nsIPrintSettingsService);

      PSSVC.savePrintSettingsToPrefs(printSettings, true,
                                     printSettings.kInitSaveAll);
      PSSVC.savePrintSettingsToPrefs(printSettings, false,
                                     printSettings.kInitSavePrinterName);
    }
  },

  logKeyedTelemetry(id, key) {
    let histogram = Services.telemetry.getKeyedHistogramById(id);
    histogram.add(key);
  },

  updatePageCount() {
    let numPages = docShell.printPreview.printPreviewNumPages;
    sendAsyncMessage("Printing:Preview:UpdatePageCount", {
      numPages,
    });
  },

  navigate(navType, pageNum) {
    docShell.printPreview.printPreviewNavigate(navType, pageNum);
  },

  /* nsIWebProgressListener for print preview */

  onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
    sendAsyncMessage("Printing:Preview:StateChange", {
      stateFlags: aStateFlags,
      status: aStatus,
    });
  },

  onProgressChange(aWebProgress, aRequest, aCurSelfProgress,
                   aMaxSelfProgress, aCurTotalProgress,
                   aMaxTotalProgress) {
    sendAsyncMessage("Printing:Preview:ProgressChange", {
      curSelfProgress: aCurSelfProgress,
      maxSelfProgress: aMaxSelfProgress,
      curTotalProgress: aCurTotalProgress,
      maxTotalProgress: aMaxTotalProgress,
    });
  },

  onLocationChange(aWebProgress, aRequest, aLocation, aFlags) {},
  onStatusChange(aWebProgress, aRequest, aStatus, aMessage) {},
  onSecurityChange(aWebProgress, aRequest, aState) {},
};
Printing.init();

function SwitchDocumentDirection(aWindow) {
 // document.dir can also be "auto", in which case it won't change
  if (aWindow.document.dir == "ltr" || aWindow.document.dir == "") {
    aWindow.document.dir = "rtl";
  } else if (aWindow.document.dir == "rtl") {
    aWindow.document.dir = "ltr";
  }
  for (let run = 0; run < aWindow.frames.length; run++) {
    SwitchDocumentDirection(aWindow.frames[run]);
  }
}

addMessageListener("SwitchDocumentDirection", () => {
  SwitchDocumentDirection(content.window);
});

var FindBar = {
  /* Please keep in sync with toolkit/content/widgets/findbar.xml */
  FIND_NORMAL: 0,
  FIND_TYPEAHEAD: 1,
  FIND_LINKS: 2,

  _findMode: 0,

  init() {
    addMessageListener("Findbar:UpdateState", this);
    Services.els.addSystemEventListener(global, "keypress", this, false);
    Services.els.addSystemEventListener(global, "mouseup", this, false);
  },

  receiveMessage(msg) {
    switch (msg.name) {
      case "Findbar:UpdateState":
        this._findMode = msg.data.findMode;
        break;
    }
  },

  handleEvent(event) {
    switch (event.type) {
      case "keypress":
        this._onKeypress(event);
        break;
      case "mouseup":
        this._onMouseup(event);
        break;
    }
  },

  /**
   * Returns whether FAYT can be used for the given event in
   * the current content state.
   */
  _canAndShouldFastFind() {
    let should = false;
    let can = BrowserUtils.canFastFind(content);
    if (can) {
      // XXXgijs: why all these shenanigans? Why not use the event's target?
      let focusedWindow = {};
      let elt = Services.focus.getFocusedElementForWindow(content, true, focusedWindow);
      let win = focusedWindow.value;
      should = BrowserUtils.shouldFastFind(elt, win);
    }
    return { can, should };
  },

  _onKeypress(event) {
    // Useless keys:
    if (event.ctrlKey || event.altKey || event.metaKey || event.defaultPrevented) {
      return undefined;
    }

    // Check the focused element etc.
    let fastFind = this._canAndShouldFastFind();

    // Can we even use find in this page at all?
    if (!fastFind.can) {
      return undefined;
    }

    let fakeEvent = {};
    for (let k in event) {
      if (typeof event[k] != "object" && typeof event[k] != "function" &&
          !(k in content.KeyboardEvent)) {
        fakeEvent[k] = event[k];
      }
    }
    // sendSyncMessage returns an array of the responses from all listeners
    let rv = sendSyncMessage("Findbar:Keypress", {
      fakeEvent,
      shouldFastFind: fastFind.should
    });
    if (rv.indexOf(false) !== -1) {
      event.preventDefault();
      return false;
    }
    return undefined;
  },

  _onMouseup(event) {
    if (this._findMode != this.FIND_NORMAL)
      sendAsyncMessage("Findbar:Mouseup");
  },
};
FindBar.init();

let WebChannelMessageToChromeListener = {
  // Preference containing the list (space separated) of origins that are
  // allowed to send non-string values through a WebChannel, mainly for
  // backwards compatability. See bug 1238128 for more information.
  URL_WHITELIST_PREF: "webchannel.allowObject.urlWhitelist",

  // Cached list of whitelisted principals, we avoid constructing this if the
  // value in `_lastWhitelistValue` hasn't changed since we constructed it last.
  _cachedWhitelist: [],
  _lastWhitelistValue: "",

  init() {
    addEventListener("WebChannelMessageToChrome", e => {
      this._onMessageToChrome(e);
    }, true, true);
  },

  _getWhitelistedPrincipals() {
    let whitelist = Services.prefs.getCharPref(this.URL_WHITELIST_PREF);
    if (whitelist != this._lastWhitelistValue) {
      let urls = whitelist.split(/\s+/);
      this._cachedWhitelist = urls.map(origin =>
        Services.scriptSecurityManager.createCodebasePrincipalFromOrigin(origin));
    }
    return this._cachedWhitelist;
  },

  _onMessageToChrome(e) {
    // If target is window then we want the document principal, otherwise fallback to target itself.
    let principal = e.target.nodePrincipal ? e.target.nodePrincipal : e.target.document.nodePrincipal;

    if (e.detail) {
      if (typeof e.detail != "string") {
        // Check if the principal is one of the ones that's allowed to send
        // non-string values for e.detail.  They're whitelisted by site origin,
        // so we compare on originNoSuffix in order to avoid other origin attributes
        // that are not relevant here, such as containers or private browsing.
        let objectsAllowed = this._getWhitelistedPrincipals().some(whitelisted =>
          principal.originNoSuffix == whitelisted.originNoSuffix);
        if (!objectsAllowed) {
          Cu.reportError("WebChannelMessageToChrome sent with an object from a non-whitelisted principal");
          return;
        }
      }
      sendAsyncMessage("WebChannelMessageToChrome", e.detail, { eventTarget: e.target }, principal);
    } else {
      Cu.reportError("WebChannel message failed. No message detail.");
    }
  }
};

WebChannelMessageToChromeListener.init();

// This should be kept in sync with /browser/base/content.js.
// Add message listener for "WebChannelMessageToContent" messages from chrome scripts.
addMessageListener("WebChannelMessageToContent", function(e) {
  if (e.data) {
    // e.objects.eventTarget will be defined if sending a response to
    // a WebChannelMessageToChrome event. An unsolicited send
    // may not have an eventTarget defined, in this case send to the
    // main content window.
    let eventTarget = e.objects.eventTarget || content;

    // Use nodePrincipal if available, otherwise fallback to document principal.
    let targetPrincipal = eventTarget instanceof Ci.nsIDOMWindow ? eventTarget.document.nodePrincipal : eventTarget.nodePrincipal;

    if (e.principal.subsumes(targetPrincipal)) {
      // If eventTarget is a window, use it as the targetWindow, otherwise
      // find the window that owns the eventTarget.
      let targetWindow = eventTarget instanceof Ci.nsIDOMWindow ? eventTarget : eventTarget.ownerGlobal;

      eventTarget.dispatchEvent(new targetWindow.CustomEvent("WebChannelMessageToContent", {
        detail: Cu.cloneInto({
          id: e.data.id,
          message: e.data.message,
        }, targetWindow),
      }));
    } else {
      Cu.reportError("WebChannel message failed. Principal mismatch.");
    }
  } else {
    Cu.reportError("WebChannel message failed. No message data.");
  }
});

var AudioPlaybackListener = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  init() {
    Services.obs.addObserver(this, "audio-playback");

    addMessageListener("AudioPlayback", this);
    addEventListener("unload", () => {
      AudioPlaybackListener.uninit();
    });
  },

  uninit() {
    Services.obs.removeObserver(this, "audio-playback");

    removeMessageListener("AudioPlayback", this);
  },

  handleMediaControlMessage(msg) {
    let utils = global.content.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIDOMWindowUtils);
    let suspendTypes = Ci.nsISuspendedTypes;
    switch (msg) {
      case "mute":
        utils.audioMuted = true;
        break;
      case "unmute":
        utils.audioMuted = false;
        break;
      case "lostAudioFocus":
        utils.mediaSuspend = suspendTypes.SUSPENDED_PAUSE_DISPOSABLE;
        break;
      case "lostAudioFocusTransiently":
        utils.mediaSuspend = suspendTypes.SUSPENDED_PAUSE;
        break;
      case "gainAudioFocus":
        utils.mediaSuspend = suspendTypes.NONE_SUSPENDED;
        break;
      case "mediaControlPaused":
        utils.mediaSuspend = suspendTypes.SUSPENDED_PAUSE_DISPOSABLE;
        break;
      case "mediaControlStopped":
        utils.mediaSuspend = suspendTypes.SUSPENDED_STOP_DISPOSABLE;
        break;
      case "resumeMedia":
        utils.mediaSuspend = suspendTypes.NONE_SUSPENDED;
        break;
      default:
        dump("Error : wrong media control msg!\n");
        break;
    }
  },

  observe(subject, topic, data) {
    if (topic === "audio-playback") {
      if (subject && subject.top == global.content) {
        let name = "AudioPlayback:";
        if (data === "activeMediaBlockStart") {
          name += "ActiveMediaBlockStart";
        } else if (data === "activeMediaBlockStop") {
          name += "ActiveMediaBlockStop";
        } else if (data == "mediaBlockStop") {
          name += "MediaBlockStop";
        } else {
          name += (data === "active") ? "Start" : "Stop";
        }
        sendAsyncMessage(name);
      }
    }
  },

  receiveMessage(msg) {
    if (msg.name == "AudioPlayback") {
      this.handleMediaControlMessage(msg.data.type);
    }
  },
};
AudioPlaybackListener.init();

var UnselectedTabHoverObserver = {
  init() {
    addMessageListener("Browser:UnselectedTabHover", this);
    addEventListener("UnselectedTabHover:Enable", this);
    addEventListener("UnselectedTabHover:Disable", this);
  },
  receiveMessage(message) {
    Services.obs.notifyObservers(content.window, "unselected-tab-hover",
                                 message.data.hovered);
  },
  handleEvent(event) {
    sendAsyncMessage("UnselectedTabHover:Toggle",
                     { enable: event.type == "UnselectedTabHover:Enable" });
  }
};
UnselectedTabHoverObserver.init();

addMessageListener("Browser:PurgeSessionHistory", function BrowserPurgeHistory() {
  let sessionHistory = docShell.QueryInterface(Ci.nsIWebNavigation).sessionHistory;
  if (!sessionHistory) {
    return;
  }

  // place the entry at current index at the end of the history list, so it won't get removed
  if (sessionHistory.index < sessionHistory.count - 1) {
    let indexEntry = sessionHistory.getEntryAtIndex(sessionHistory.index, false);
    sessionHistory.QueryInterface(Components.interfaces.nsISHistoryInternal);
    indexEntry.QueryInterface(Components.interfaces.nsISHEntry);
    sessionHistory.addEntry(indexEntry, true);
  }

  let purge = sessionHistory.count;
  if (global.content.location.href != "about:blank") {
    --purge; // Don't remove the page the user's staring at from shistory
  }

  if (purge > 0) {
    sessionHistory.PurgeHistory(purge);
  }
});

var ViewSelectionSource = {
  init() {
    addMessageListener("ViewSource:GetSelection", this);
  },

  receiveMessage(message) {
    if (message.name == "ViewSource:GetSelection") {
      let selectionDetails;
      try {
        selectionDetails = message.objects.target ? this.getMathMLSelection(message.objects.target)
                                                  : this.getSelection();
      } finally {
        sendAsyncMessage("ViewSource:GetSelectionDone", selectionDetails);
      }
    }
  },

  /**
   * A helper to get a path like FIXptr, but with an array instead of the
   * "tumbler" notation.
   * See FIXptr: http://lists.w3.org/Archives/Public/www-xml-linking-comments/2001AprJun/att-0074/01-NOTE-FIXptr-20010425.htm
   */
  getPath(ancestor, node) {
    var n = node;
    var p = n.parentNode;
    if (n == ancestor || !p)
      return null;
    var path = [];
    if (!path)
      return null;
    do {
      for (var i = 0; i < p.childNodes.length; i++) {
        if (p.childNodes.item(i) == n) {
          path.push(i);
          break;
        }
      }
      n = p;
      p = n.parentNode;
    } while (n != ancestor && p);
    return path;
  },

  getSelection() {
    // These are markers used to delimit the selection during processing. They
    // are removed from the final rendering.
    // We use noncharacter Unicode codepoints to minimize the risk of clashing
    // with anything that might legitimately be present in the document.
    // U+FDD0..FDEF <noncharacters>
    const MARK_SELECTION_START = "\uFDD0";
    const MARK_SELECTION_END = "\uFDEF";

    var focusedWindow = Services.focus.focusedWindow || content;
    var selection = focusedWindow.getSelection();

    var range = selection.getRangeAt(0);
    var ancestorContainer = range.commonAncestorContainer;
    var doc = ancestorContainer.ownerDocument;

    var startContainer = range.startContainer;
    var endContainer = range.endContainer;
    var startOffset = range.startOffset;
    var endOffset = range.endOffset;

    // let the ancestor be an element
    var Node = doc.defaultView.Node;
    if (ancestorContainer.nodeType == Node.TEXT_NODE ||
        ancestorContainer.nodeType == Node.CDATA_SECTION_NODE)
      ancestorContainer = ancestorContainer.parentNode;

    // for selectAll, let's use the entire document, including <html>...</html>
    // @see nsDocumentViewer::SelectAll() for how selectAll is implemented
    try {
      if (ancestorContainer == doc.body)
        ancestorContainer = doc.documentElement;
    } catch (e) { }

    // each path is a "child sequence" (a.k.a. "tumbler") that
    // descends from the ancestor down to the boundary point
    var startPath = this.getPath(ancestorContainer, startContainer);
    var endPath = this.getPath(ancestorContainer, endContainer);

    // clone the fragment of interest and reset everything to be relative to it
    // note: it is with the clone that we operate/munge from now on.  Also note
    // that we clone into a data document to prevent images in the fragment from
    // loading and the like.  The use of importNode here, as opposed to adoptNode,
    // is _very_ important.
    // XXXbz wish there were a less hacky way to create an untrusted document here
    var isHTML = (doc.createElement("div").tagName == "DIV");
    var dataDoc = isHTML ?
      ancestorContainer.ownerDocument.implementation.createHTMLDocument("") :
      ancestorContainer.ownerDocument.implementation.createDocument("", "", null);
    ancestorContainer = dataDoc.importNode(ancestorContainer, true);
    startContainer = ancestorContainer;
    endContainer = ancestorContainer;

    // Only bother with the selection if it can be remapped. Don't mess with
    // leaf elements (such as <isindex>) that secretly use anynomous content
    // for their display appearance.
    var canDrawSelection = ancestorContainer.hasChildNodes();
    var tmpNode;
    if (canDrawSelection) {
      var i;
      for (i = startPath ? startPath.length - 1 : -1; i >= 0; i--) {
        startContainer = startContainer.childNodes.item(startPath[i]);
      }
      for (i = endPath ? endPath.length - 1 : -1; i >= 0; i--) {
        endContainer = endContainer.childNodes.item(endPath[i]);
      }

      // add special markers to record the extent of the selection
      // note: |startOffset| and |endOffset| are interpreted either as
      // offsets in the text data or as child indices (see the Range spec)
      // (here, munging the end point first to keep the start point safe...)
      if (endContainer.nodeType == Node.TEXT_NODE ||
          endContainer.nodeType == Node.CDATA_SECTION_NODE) {
        // do some extra tweaks to try to avoid the view-source output to look like
        // ...<tag>]... or ...]</tag>... (where ']' marks the end of the selection).
        // To get a neat output, the idea here is to remap the end point from:
        // 1. ...<tag>]...   to   ...]<tag>...
        // 2. ...]</tag>...  to   ...</tag>]...
        if ((endOffset > 0 && endOffset < endContainer.data.length) ||
            !endContainer.parentNode || !endContainer.parentNode.parentNode)
          endContainer.insertData(endOffset, MARK_SELECTION_END);
        else {
          tmpNode = dataDoc.createTextNode(MARK_SELECTION_END);
          endContainer = endContainer.parentNode;
          if (endOffset === 0)
            endContainer.parentNode.insertBefore(tmpNode, endContainer);
          else
            endContainer.parentNode.insertBefore(tmpNode, endContainer.nextSibling);
        }
      } else {
        tmpNode = dataDoc.createTextNode(MARK_SELECTION_END);
        endContainer.insertBefore(tmpNode, endContainer.childNodes.item(endOffset));
      }

      if (startContainer.nodeType == Node.TEXT_NODE ||
          startContainer.nodeType == Node.CDATA_SECTION_NODE) {
        // do some extra tweaks to try to avoid the view-source output to look like
        // ...<tag>[... or ...[</tag>... (where '[' marks the start of the selection).
        // To get a neat output, the idea here is to remap the start point from:
        // 1. ...<tag>[...   to   ...[<tag>...
        // 2. ...[</tag>...  to   ...</tag>[...
        if ((startOffset > 0 && startOffset < startContainer.data.length) ||
            !startContainer.parentNode || !startContainer.parentNode.parentNode ||
            startContainer != startContainer.parentNode.lastChild)
          startContainer.insertData(startOffset, MARK_SELECTION_START);
        else {
          tmpNode = dataDoc.createTextNode(MARK_SELECTION_START);
          startContainer = startContainer.parentNode;
          if (startOffset === 0)
            startContainer.parentNode.insertBefore(tmpNode, startContainer);
          else
            startContainer.parentNode.insertBefore(tmpNode, startContainer.nextSibling);
        }
      } else {
        tmpNode = dataDoc.createTextNode(MARK_SELECTION_START);
        startContainer.insertBefore(tmpNode, startContainer.childNodes.item(startOffset));
      }
    }

    // now extract and display the syntax highlighted source
    tmpNode = dataDoc.createElementNS("http://www.w3.org/1999/xhtml", "div");
    tmpNode.appendChild(ancestorContainer);

    return { uri: (isHTML ? "view-source:data:text/html;charset=utf-8," :
                            "view-source:data:application/xml;charset=utf-8,")
                  + encodeURIComponent(tmpNode.innerHTML),
             drawSelection: canDrawSelection,
             baseURI: doc.baseURI };
  },

  /**
   * Reformat the source of a MathML node to highlight the node that was targetted.
   *
   * @param node
   *        Some element within the fragment of interest.
   */
  getMathMLSelection(node) {
    var Node = node.ownerGlobal.Node;
    this._lineCount = 0;
    this._startTargetLine = 0;
    this._endTargetLine = 0;
    this._targetNode = node;
    if (this._targetNode && this._targetNode.nodeType == Node.TEXT_NODE)
      this._targetNode = this._targetNode.parentNode;

    // walk up the tree to the top-level element (e.g., <math>, <svg>)
    var topTag = "math";
    var topNode = this._targetNode;
    while (topNode && topNode.localName != topTag) {
      topNode = topNode.parentNode;
    }
    if (!topNode)
      return undefined;

    // serialize
    const VIEW_SOURCE_CSS = "resource://content-accessible/viewsource.css";
    const BUNDLE_URL = "chrome://global/locale/viewSource.properties";

    let bundle = Services.strings.createBundle(BUNDLE_URL);
    var title = bundle.GetStringFromName("viewMathMLSourceTitle");
    var wrapClass = this.wrapLongLines ? ' class="wrap"' : "";
    var source =
      "<!DOCTYPE html>"
    + "<html>"
    + "<head><title>" + title + "</title>"
    + '<link rel="stylesheet" type="text/css" href="' + VIEW_SOURCE_CSS + '">'
    + '<style type="text/css">'
    + "#target { border: dashed 1px; background-color: lightyellow; }"
    + "</style>"
    + "</head>"
    + '<body id="viewsource"' + wrapClass
    + ' onload="document.title=\'' + title + '\'; document.getElementById(\'target\').scrollIntoView(true)">'
    + "<pre>"
    + this.getOuterMarkup(topNode, 0)
    + "</pre></body></html>"
    ; // end

    return { uri: "data:text/html;charset=utf-8," + encodeURIComponent(source),
             drawSelection: false, baseURI: node.ownerDocument.baseURI };
  },

  get wrapLongLines() {
    return Services.prefs.getBoolPref("view_source.wrap_long_lines");
  },

  getInnerMarkup(node, indent) {
    var str = "";
    for (var i = 0; i < node.childNodes.length; i++) {
      str += this.getOuterMarkup(node.childNodes.item(i), indent);
    }
    return str;
  },

  getOuterMarkup(node, indent) {
    var Node = node.ownerGlobal.Node;
    var newline = "";
    var padding = "";
    var str = "";
    if (node == this._targetNode) {
      this._startTargetLine = this._lineCount;
      str += '</pre><pre id="target">';
    }

    switch (node.nodeType) {
    case Node.ELEMENT_NODE: // Element
      // to avoid the wide gap problem, '\n' is not emitted on the first
      // line and the lines before & after the <pre id="target">...</pre>
      if (this._lineCount > 0 &&
          this._lineCount != this._startTargetLine &&
          this._lineCount != this._endTargetLine) {
        newline = "\n";
      }
      this._lineCount++;
      for (var k = 0; k < indent; k++) {
        padding += " ";
      }
      str += newline + padding
          + '&lt;<span class="start-tag">' + node.nodeName + "</span>";
      for (var i = 0; i < node.attributes.length; i++) {
        var attr = node.attributes.item(i);
        if (attr.nodeName.match(/^[-_]moz/)) {
          continue;
        }
        str += ' <span class="attribute-name">'
            + attr.nodeName
            + '</span>=<span class="attribute-value">"'
            + this.unicodeToEntity(attr.nodeValue)
            + '"</span>';
      }
      if (!node.hasChildNodes()) {
        str += "/&gt;";
      } else {
        str += "&gt;";
        var oldLine = this._lineCount;
        str += this.getInnerMarkup(node, indent + 2);
        if (oldLine == this._lineCount) {
          newline = "";
          padding = "";
        } else {
          newline = (this._lineCount == this._endTargetLine) ? "" : "\n";
          this._lineCount++;
        }
        str += newline + padding
            + '&lt;/<span class="end-tag">' + node.nodeName + "</span>&gt;";
      }
      break;
    case Node.TEXT_NODE: // Text
      var tmp = node.nodeValue;
      tmp = tmp.replace(/(\n|\r|\t)+/g, " ");
      tmp = tmp.replace(/^ +/, "");
      tmp = tmp.replace(/ +$/, "");
      if (tmp.length != 0) {
        str += '<span class="text">' + this.unicodeToEntity(tmp) + "</span>";
      }
      break;
    default:
      break;
    }

    if (node == this._targetNode) {
      this._endTargetLine = this._lineCount;
      str += "</pre><pre>";
    }
    return str;
  },

  unicodeToEntity(text) {
    const charTable = {
      "&": '&amp;<span class="entity">amp;</span>',
      "<": '&amp;<span class="entity">lt;</span>',
      ">": '&amp;<span class="entity">gt;</span>',
      '"': '&amp;<span class="entity">quot;</span>'
    };

    function charTableLookup(letter) {
      return charTable[letter];
    }

    // replace chars in our charTable
    return text.replace(/[<>&"]/g, charTableLookup);
  }
};

ViewSelectionSource.init();

addEventListener("MozApplicationManifest", function(e) {
  let doc = e.target;
  let info = {
    uri: doc.documentURI,
    characterSet: doc.characterSet,
    manifest: doc.documentElement.getAttribute("manifest"),
    principal: doc.nodePrincipal,
  };
  sendAsyncMessage("MozApplicationManifest", info);
}, false);

let AutoCompletePopup = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAutoCompletePopup]),

  _connected: false,

  MESSAGES: [
    "FormAutoComplete:HandleEnter",
    "FormAutoComplete:PopupClosed",
    "FormAutoComplete:PopupOpened",
    "FormAutoComplete:RequestFocus",
  ],

  init() {
    addEventListener("unload", this);
    addEventListener("DOMContentLoaded", this);
    // WebExtension browserAction is preloaded and does not receive DCL, wait
    // on pageshow so we can hookup the formfill controller.
    addEventListener("pageshow", this, true);

    for (let messageName of this.MESSAGES) {
      addMessageListener(messageName, this);
    }

    this._input = null;
    this._popupOpen = false;
  },

  destroy() {
    if (this._connected) {
      let controller = Cc["@mozilla.org/satchel/form-fill-controller;1"]
                         .getService(Ci.nsIFormFillController);
      controller.detachFromBrowser(docShell);
      this._connected = false;
    }

    removeEventListener("pageshow", this);
    removeEventListener("unload", this);
    removeEventListener("DOMContentLoaded", this);

    for (let messageName of this.MESSAGES) {
      removeMessageListener(messageName, this);
    }
  },

  connect() {
    if (this._connected) {
      return;
    }
    // We need to wait for a content viewer to be available
    // before we can attach our AutoCompletePopup handler,
    // since nsFormFillController assumes one will exist
    // when we call attachToBrowser.

    // Hook up the form fill autocomplete controller.
    let controller = Cc["@mozilla.org/satchel/form-fill-controller;1"]
                       .getService(Ci.nsIFormFillController);
    controller.attachToBrowser(docShell,
                               this.QueryInterface(Ci.nsIAutoCompletePopup));
    this._connected = true;
  },

  handleEvent(event) {
    switch (event.type) {
      case "pageshow": {
        removeEventListener("pageshow", this);
        this.connect();
        break;
      }

      case "DOMContentLoaded": {
        removeEventListener("DOMContentLoaded", this);
        this.connect();
        break;
      }

      case "unload": {
        this.destroy();
        break;
      }
    }
  },

  receiveMessage(message) {
    switch (message.name) {
      case "FormAutoComplete:HandleEnter": {
        this.selectedIndex = message.data.selectedIndex;

        let controller = Cc["@mozilla.org/autocomplete/controller;1"]
                           .getService(Ci.nsIAutoCompleteController);
        controller.handleEnter(message.data.isPopupSelection);
        break;
      }

      case "FormAutoComplete:PopupClosed": {
        this._popupOpen = false;
        break;
      }

      case "FormAutoComplete:PopupOpened": {
        this._popupOpen = true;
        break;
      }

      case "FormAutoComplete:RequestFocus": {
        if (this._input) {
          this._input.focus();
        }
        break;
      }
    }
  },

  get input() { return this._input; },
  get overrideValue() { return null; },
  set selectedIndex(index) {
    sendAsyncMessage("FormAutoComplete:SetSelectedIndex", { index });
  },
  get selectedIndex() {
    // selectedIndex getter must be synchronous because we need the
    // correct value when the controller is in controller::HandleEnter.
    // We can't easily just let the parent inform us the new value every
    // time it changes because not every action that can change the
    // selectedIndex is trivial to catch (e.g. moving the mouse over the
    // list).
    return sendSyncMessage("FormAutoComplete:GetSelectedIndex", {});
  },
  get popupOpen() {
    return this._popupOpen;
  },

  openAutocompletePopup(input, element) {
    if (this._popupOpen || !input) {
      return;
    }

    let rect = BrowserUtils.getElementBoundingScreenRect(element);
    let window = element.ownerGlobal;
    let dir = window.getComputedStyle(element).direction;
    let results = this.getResultsFromController(input);

    sendAsyncMessage("FormAutoComplete:MaybeOpenPopup",
                     { results, rect, dir });
    this._input = input;
  },

  closePopup() {
    // We set this here instead of just waiting for the
    // PopupClosed message to do it so that we don't end
    // up in a state where the content thinks that a popup
    // is open when it isn't (or soon won't be).
    this._popupOpen = false;
    sendAsyncMessage("FormAutoComplete:ClosePopup", {});
  },

  invalidate() {
    if (this._popupOpen) {
      let results = this.getResultsFromController(this._input);
      sendAsyncMessage("FormAutoComplete:Invalidate", { results });
    }
  },

  selectBy(reverse, page) {
    this._index = sendSyncMessage("FormAutoComplete:SelectBy", {
      reverse,
      page
    });
  },

  getResultsFromController(inputField) {
    let results = [];

    if (!inputField) {
      return results;
    }

    let controller = inputField.controller;
    if (!(controller instanceof Ci.nsIAutoCompleteController)) {
      return results;
    }

    for (let i = 0; i < controller.matchCount; ++i) {
      let result = {};
      result.value = controller.getValueAt(i);
      result.label = controller.getLabelAt(i);
      result.comment = controller.getCommentAt(i);
      result.style = controller.getStyleAt(i);
      result.image = controller.getImageAt(i);
      results.push(result);
    }

    return results;
  },
};

AutoCompletePopup.init();

/**
 * DateTimePickerListener is the communication channel between the input box
 * (content) for date/time input types and its picker (chrome).
 */
let DateTimePickerListener = {
  /**
   * On init, just listen for the event to open the picker, once the picker is
   * opened, we'll listen for update and close events.
   */
  init() {
    addEventListener("MozOpenDateTimePicker", this);
    this._inputElement = null;

    addEventListener("unload", () => {
      this.uninit();
    });
  },

  uninit() {
    removeEventListener("MozOpenDateTimePicker", this);
    this._inputElement = null;
  },

  /**
   * Cleanup function called when picker is closed.
   */
  close() {
    this.removeListeners();
    this._inputElement.setDateTimePickerState(false);
    this._inputElement = null;
  },

  /**
   * Called after picker is opened to start listening for input box update
   * events.
   */
  addListeners() {
    addEventListener("MozUpdateDateTimePicker", this);
    addEventListener("MozCloseDateTimePicker", this);
    addEventListener("pagehide", this);

    addMessageListener("FormDateTime:PickerValueChanged", this);
    addMessageListener("FormDateTime:PickerClosed", this);
  },

  /**
   * Stop listeneing for events when picker is closed.
   */
  removeListeners() {
    removeEventListener("MozUpdateDateTimePicker", this);
    removeEventListener("MozCloseDateTimePicker", this);
    removeEventListener("pagehide", this);

    removeMessageListener("FormDateTime:PickerValueChanged", this);
    removeMessageListener("FormDateTime:PickerClosed", this);
  },

  /**
   * Helper function that returns the CSS direction property of the element.
   */
  getComputedDirection(aElement) {
    return aElement.ownerGlobal.getComputedStyle(aElement)
      .getPropertyValue("direction");
  },

  /**
   * Helper function that returns the rect of the element, which is the position
   * relative to the left/top of the content area.
   */
  getBoundingContentRect(aElement) {
    return BrowserUtils.getElementBoundingRect(aElement);
  },

  getTimePickerPref() {
    return Services.prefs.getBoolPref("dom.forms.datetime.timepicker");
  },

  /**
   * nsIMessageListener.
   */
  receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "FormDateTime:PickerClosed": {
        this.close();
        break;
      }
      case "FormDateTime:PickerValueChanged": {
        this._inputElement.updateDateTimeInputBox(aMessage.data);
        break;
      }
      default:
        break;
    }
  },

  /**
   * nsIDOMEventListener, for chrome events sent by the input element and other
   * DOM events.
   */
  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "MozOpenDateTimePicker": {
        // Time picker is disabled when preffed off
        if (!(aEvent.originalTarget instanceof content.HTMLInputElement) ||
            (aEvent.originalTarget.type == "time" && !this.getTimePickerPref())) {
          return;
        }

        if (this._inputElement) {
          // This happens when we're trying to open a picker when another picker
          // is still open. We ignore this request to let the first picker
          // close gracefully.
          return;
        }

        this._inputElement = aEvent.originalTarget;
        this._inputElement.setDateTimePickerState(true);
        this.addListeners();

        let value = this._inputElement.getDateTimeInputBoxValue();
        sendAsyncMessage("FormDateTime:OpenPicker", {
          rect: this.getBoundingContentRect(this._inputElement),
          dir: this.getComputedDirection(this._inputElement),
          type: this._inputElement.type,
          detail: {
            // Pass partial value if it's available, otherwise pass input
            // element's value.
            value: Object.keys(value).length > 0 ? value
                                                 : this._inputElement.value,
            min: this._inputElement.getMinimum(),
            max: this._inputElement.getMaximum(),
            step: this._inputElement.getStep(),
            stepBase: this._inputElement.getStepBase(),
          },
        });
        break;
      }
      case "MozUpdateDateTimePicker": {
        let value = this._inputElement.getDateTimeInputBoxValue();
        value.type = this._inputElement.type;
        sendAsyncMessage("FormDateTime:UpdatePicker", { value });
        break;
      }
      case "MozCloseDateTimePicker": {
        sendAsyncMessage("FormDateTime:ClosePicker");
        this.close();
        break;
      }
      case "pagehide": {
        if (this._inputElement &&
            this._inputElement.ownerDocument == aEvent.target) {
          sendAsyncMessage("FormDateTime:ClosePicker");
          this.close();
        }
        break;
      }
      default:
        break;
    }
  },
};

DateTimePickerListener.init();

addEventListener("mozshowdropdown", event => {
  if (!event.isTrusted)
    return;

  if (!SelectContentHelper.open) {
    new SelectContentHelper(event.target, {isOpenedViaTouch: false}, this);
  }
});

addEventListener("mozshowdropdown-sourcetouch", event => {
  if (!event.isTrusted)
    return;

  if (!SelectContentHelper.open) {
    new SelectContentHelper(event.target, {isOpenedViaTouch: true}, this);
  }
});

let ExtFind = {
  init() {
    addMessageListener("ext-Finder:CollectResults", this);
    addMessageListener("ext-Finder:HighlightResults", this);
    addMessageListener("ext-Finder:clearHighlighting", this);
  },

  _findContent: null,

  async receiveMessage(message) {
    if (!this._findContent) {
      this._findContent = new FindContent(docShell);
    }

    let data;
    switch (message.name) {
      case "ext-Finder:CollectResults":
        this.finderInited = true;
        data = await this._findContent.findRanges(message.data);
        sendAsyncMessage("ext-Finder:CollectResultsFinished", data);
        break;
      case "ext-Finder:HighlightResults":
        data = this._findContent.highlightResults(message.data);
        sendAsyncMessage("ext-Finder:HighlightResultsFinished", data);
        break;
      case "ext-Finder:clearHighlighting":
        this._findContent.highlighter.highlight(false);
        break;
    }
  },
};

ExtFind.init();
