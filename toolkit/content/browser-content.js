/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;
var Cr = Components.results;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ReaderMode",
  "resource://gre/modules/ReaderMode.jsm");

var global = this;


// Lazily load the finder code
addMessageListener("Finder:Initialize", function () {
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
    this.autoscrollLoop = this.autoscrollLoop.bind(this);

    Services.els.addSystemEventListener(global, "mousedown", this, true);

    addMessageListener("Autoscroll:Stop", this);
  },

  isAutoscrollBlocker: function(node) {
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

  findNearestScrollableElement: function(aNode) {
    // this is a list of overflow property values that allow scrolling
    const scrollingAllowed = ['scroll', 'auto'];

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

      var overflowx = this._scrollable.ownerDocument.defaultView
                          .getComputedStyle(this._scrollable, '')
                          .getPropertyValue('overflow-x');
      var overflowy = this._scrollable.ownerDocument.defaultView
                          .getComputedStyle(this._scrollable, '')
                          .getPropertyValue('overflow-y');
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
      this._scrollable = aNode.ownerDocument.defaultView;
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

  startScroll: function(event) {

    this.findNearestScrollableElement(event.originalTarget);

    if (!this._scrollable)
      return;

    let [enabled] = sendSyncMessage("Autoscroll:Start",
                                    {scrolldir: this._scrolldir,
                                     screenX: event.screenX,
                                     screenY: event.screenY});
    if (!enabled) {
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
    this._lastFrame = content.performance.now();

    content.requestAnimationFrame(this.autoscrollLoop);
  },

  stopScroll: function() {
    if (this._scrollable) {
      this._scrollable.mozScrollSnap();
      this._scrollable = null;

      Services.els.removeSystemEventListener(global, "mousemove", this, true);
      removeEventListener("pagehide", this, true);
    }
  },

  accelerate: function(curr, start) {
    const speed = 12;
    var val = (curr - start) / speed;

    if (val > 1)
      return val * Math.sqrt(val) - 1;
    if (val < -1)
      return val * Math.sqrt(-val) + 1;
    return 0;
  },

  roundToZero: function(num) {
    if (num > 0)
      return Math.floor(num);
    return Math.ceil(num);
  },

  autoscrollLoop: function(timestamp) {
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
    if (this._scrolldir != 'EW') {
      var y = this.accelerate(this._screenY, this._startY) * timeCompensation;
      var desiredScrollY = this._scrollErrorY + y;
      actualScrollY = this.roundToZero(desiredScrollY);
      this._scrollErrorY = (desiredScrollY - actualScrollY);
    }
    if (this._scrolldir != 'NS') {
      var x = this.accelerate(this._screenX, this._startX) * timeCompensation;
      var desiredScrollX = this._scrollErrorX + x;
      actualScrollX = this.roundToZero(desiredScrollX);
      this._scrollErrorX = (desiredScrollX - actualScrollX);
    }

    const kAutoscroll = 15;  // defined in mozilla/layers/ScrollInputMethods.h
    Services.telemetry.getHistogramById("SCROLL_INPUT_METHODS").add(kAutoscroll);

    if (this._scrollable instanceof content.Window) {
      this._scrollable.scrollBy(actualScrollX, actualScrollY);
    } else { // an element with overflow
      this._scrollable.scrollLeft += actualScrollX;
      this._scrollable.scrollTop += actualScrollY;
    }
    content.requestAnimationFrame(this.autoscrollLoop);
  },

  handleEvent: function(event) {
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

  receiveMessage: function(msg) {
    switch (msg.name) {
      case "Autoscroll:Stop": {
        this.stopScroll();
        break;
      }
    }
  },
};
ClickEventHandler.init();

var PopupBlocking = {
  popupData: null,
  popupDataInternal: null,

  init: function() {
    addEventListener("DOMPopupBlocked", this, true);
    addEventListener("pageshow", this, true);
    addEventListener("pagehide", this, true);

    addMessageListener("PopupBlocking:UnblockPopup", this);
    addMessageListener("PopupBlocking:GetBlockedPopupList", this);
  },

  receiveMessage: function(msg) {
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
            popupWindowURIspec = popupWindowURIspec.substring(0, 500)
          }

          popupData.push({popupWindowURIspec});
        }

        sendAsyncMessage("PopupBlocking:ReplyGetBlockedPopupList", {popupData});
        break;
      }
    }
  },

  handleEvent: function(ev) {
    switch (ev.type) {
      case "DOMPopupBlocked":
        return this.onPopupBlocked(ev);
      case "pageshow":
        return this.onPageShow(ev);
      case "pagehide":
        return this.onPageHide(ev);
    }
    return undefined;
  },

  onPopupBlocked: function(ev) {
    if (!this.popupData) {
      this.popupData = new Array();
      this.popupDataInternal = new Array();
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

  onPageShow: function(ev) {
    if (this.popupData) {
      let i = 0;
      while (i < this.popupData.length) {
        // Filter out irrelevant reports.
        if (this.popupDataInternal[i].requestingWindow &&
            (this.popupDataInternal[i].requestingWindow.document ==
             this.popupDataInternal[i].requestingDocument)) {
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
      this.updateBlockedPopups(false);
    }
  },

  onPageHide: function(ev) {
    if (this.popupData) {
      this.popupData = null;
      this.popupDataInternal = null;
      this.updateBlockedPopups(false);
    }
  },

  updateBlockedPopups: function(freshPopup) {
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
    "Printing:Preview:UpdatePageCount",
    "Printing:Print",
  ],

  init() {
    this.MESSAGES.forEach(msgName => addMessageListener(msgName, this));
    addEventListener("PrintingError", this, true);
  },

  get shouldSavePrintSettings() {
    return Services.prefs.getBoolPref("print.use_global_printsettings", false) &&
           Services.prefs.getBoolPref("print.save_print_settings", false);
  },

  handleEvent(event) {
    if (event.type == "PrintingError") {
      let win = event.target.defaultView;
      let wbp = win.QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIWebBrowserPrint);
      let nsresult = event.detail;
      sendAsyncMessage("Printing:Error", {
        isPrinting: wbp.doingPrint,
        nsresult: nsresult,
      });
    }
  },

  receiveMessage(message) {
    let objects = message.objects;
    let data = message.data;
    switch(message.name) {
      case "Printing:Preview:Enter": {
        this.enterPrintPreview(Services.wm.getOuterWindowWithId(data.windowID), data.simplifiedMode);
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

      case "Printing:Preview:UpdatePageCount": {
        this.updatePageCount();
        break;
      }

      case "Printing:Print": {
        this.print(Services.wm.getOuterWindowWithId(data.windowID), data.simplifiedMode);
        break;
      }
    }
  },

  getPrintSettings() {
    try {
      let PSSVC = Cc["@mozilla.org/gfx/printsettings-service;1"]
                    .getService(Ci.nsIPrintSettingsService);

      let printSettings = PSSVC.globalPrintSettings;
      if (!printSettings.printerName) {
        printSettings.printerName = PSSVC.defaultPrinterName;
      }
      // First get any defaults from the printer
      PSSVC.initPrintSettingsFromPrinter(printSettings.printerName,
                                         printSettings);
      // now augment them with any values from last time
      PSSVC.initPrintSettingsFromPrefs(printSettings, true,
                                       printSettings.kInitSaveAll);

      return printSettings;
    } catch(e) {
      Components.utils.reportError(e);
    }

    return null;
  },

  parseDocument(URL, contentWindow) {
    // By using ReaderMode primitives, we parse given document and place the
    // resulting JS object into the DOM of current browser.
    let articlePromise = ReaderMode.parseDocument(contentWindow.document).catch(Cu.reportError);
    articlePromise.then(function (article) {
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

      // Create style element for header div and import simplifyMode.css
      let controlHeaderStyle = content.document.createElement("style");
      controlHeaderStyle.setAttribute("scoped", "");
      controlHeaderStyle.textContent = "@import url(\"chrome://global/content/simplifyMode.css\");";
      headerElement.appendChild(controlHeaderStyle);

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

      let articleUri = Services.io.newURI(article.url, null, null);
      let parserUtils = Cc["@mozilla.org/parserutils;1"].getService(Ci.nsIParserUtils);
      let contentFragment = parserUtils.parseFragment(article.content,
        Ci.nsIParserUtils.SanitizerDropForms | Ci.nsIParserUtils.SanitizerAllowStyle,
        false, articleUri, readerContent);

      readerContent.appendChild(contentFragment);

      // Display reader content element
      readerContent.style.display = "block";

      // Here we tell the parent that we have parsed the document successfully
      // using ReaderMode primitives and we are able to enter on preview mode.
      sendAsyncMessage("Printing:Preview:ReaderModeReady");
    });
  },

  enterPrintPreview(contentWindow, simplifiedMode) {
    // We'll call this whenever we've finished reflowing the document, or if
    // we errored out while attempting to print preview (in which case, we'll
    // notify the parent that we've failed).
    let notifyEntered = (error) => {
      removeEventListener("printPreviewUpdate", onPrintPreviewReady);
      sendAsyncMessage("Printing:Preview:Entered", {
        failed: !!error,
      });
    };

    let onPrintPreviewReady = () => {
      notifyEntered();
    };

    // We have to wait for the print engine to finish reflowing all of the
    // documents and subdocuments before we can tell the parent to flip to
    // the print preview UI - otherwise, the print preview UI might ask for
    // information (like the number of pages in the document) before we have
    // our PresShells set up.
    addEventListener("printPreviewUpdate", onPrintPreviewReady);

    try {
      let printSettings = this.getPrintSettings();

      // If we happen to be on simplified mode, we need to set docURL in order
      // to generate header/footer content correctly, since simplified tab has
      // "about:blank" as its URI.
      if (printSettings && simplifiedMode)
        printSettings.docURL = contentWindow.document.baseURI;

      docShell.printPreview.printPreview(printSettings, contentWindow, this);
    } catch(error) {
      // This might fail if we, for example, attempt to print a XUL document.
      // In that case, we inform the parent to bail out of print preview.
      Components.utils.reportError(error);
      notifyEntered(error);
    }
  },

  exitPrintPreview() {
    docShell.printPreview.exitPrintPreview();
  },

  print(contentWindow, simplifiedMode) {
    let printSettings = this.getPrintSettings();
    let rv = Cr.NS_OK;

    // If we happen to be on simplified mode, we need to set docURL in order
    // to generate header/footer content correctly, since simplified tab has
    // "about:blank" as its URI.
    if (printSettings && simplifiedMode) {
      printSettings.docURL = contentWindow.document.baseURI;
    }

    try {
      let print = contentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                               .getInterface(Ci.nsIWebBrowserPrint);
      print.print(printSettings, null);

      let histogram = Services.telemetry.getKeyedHistogramById("PRINT_COUNT");
      if (print.doingPrintPreview) {
        if (simplifiedMode) {
          histogram.add("SIMPLIFIED");
        } else {
          histogram.add("WITH_PREVIEW");
        }
      } else {
        histogram.add("WITHOUT_PREVIEW");
      }
    } catch(e) {
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

  updatePageCount() {
    let numPages = docShell.printPreview.printPreviewNumPages;
    sendAsyncMessage("Printing:Preview:UpdatePageCount", {
      numPages: numPages,
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
}
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
    let {BrowserUtils} = Cu.import("resource://gre/modules/BrowserUtils.jsm", {});
    let should = false;
    let can = BrowserUtils.canFastFind(content);
    if (can) {
      //XXXgijs: why all these shenanigans? Why not use the event's target?
      let focusedWindow = {};
      let elt = Services.focus.getFocusedElementForWindow(content, true, focusedWindow);
      let win = focusedWindow.value;
      should = BrowserUtils.shouldFastFind(elt, win);
    }
    return { can, should }
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
      fakeEvent: fakeEvent,
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
      if (typeof e.detail != 'string') {
        // Check if the principal is one of the ones that's allowed to send
        // non-string values for e.detail.
        let objectsAllowed = this._getWhitelistedPrincipals().some(whitelisted =>
          principal.originNoSuffix == whitelisted.originNoSuffix);
        if (!objectsAllowed) {
          Cu.reportError("WebChannelMessageToChrome sent with an object from a non-whitelisted principal");
          return;
        }
      }
      sendAsyncMessage("WebChannelMessageToChrome", e.detail, { eventTarget: e.target }, principal);
    } else  {
      Cu.reportError("WebChannel message failed. No message detail.");
    }
  }
};

WebChannelMessageToChromeListener.init();

// This should be kept in sync with /browser/base/content.js.
// Add message listener for "WebChannelMessageToContent" messages from chrome scripts.
addMessageListener("WebChannelMessageToContent", function (e) {
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
      let targetWindow = eventTarget instanceof Ci.nsIDOMWindow ? eventTarget : eventTarget.ownerDocument.defaultView;

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
    Services.obs.addObserver(this, "audio-playback", false);
    Services.obs.addObserver(this, "AudioFocusChanged", false);
    Services.obs.addObserver(this, "MediaControl", false);

    addMessageListener("AudioPlayback", this);
    addEventListener("unload", () => {
      AudioPlaybackListener.uninit();
    });
  },

  uninit() {
    Services.obs.removeObserver(this, "audio-playback");
    Services.obs.removeObserver(this, "AudioFocusChanged");
    Services.obs.removeObserver(this, "MediaControl");

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
      case "blockInactivePageMedia":
        utils.mediaSuspend = suspendTypes.SUSPENDED_BLOCK;
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
        name += (data === "active") ? "Start" : "Stop";
        sendAsyncMessage(name);
      }
    } else if (topic == "AudioFocusChanged" || topic == "MediaControl") {
      this.handleMediaControlMessage(data);
    }
  },

  receiveMessage(msg) {
    if (msg.name == "AudioPlayback") {
      this.handleMediaControlMessage(msg.data.type);
    }
  },
};
AudioPlaybackListener.init();

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
  init: function () {
    addMessageListener("ViewSource:GetSelection", this);
  },

  receiveMessage: function(message) {
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
  getPath: function(ancestor, node) {
    var n = node;
    var p = n.parentNode;
    if (n == ancestor || !p)
      return null;
    var path = new Array();
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

  getSelection: function () {
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
      for (i = startPath ? startPath.length-1 : -1; i >= 0; i--) {
        startContainer = startContainer.childNodes.item(startPath[i]);
      }
      for (i = endPath ? endPath.length-1 : -1; i >= 0; i--) {
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
      }
      else {
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
      }
      else {
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
  getMathMLSelection: function(node) {
    var Node = node.ownerDocument.defaultView.Node;
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
    const VIEW_SOURCE_CSS = "resource://gre-resources/viewsource.css";
    const BUNDLE_URL = "chrome://global/locale/viewSource.properties";

    let bundle = Services.strings.createBundle(BUNDLE_URL);
    var title = bundle.GetStringFromName("viewMathMLSourceTitle");
    var wrapClass = this.wrapLongLines ? ' class="wrap"' : '';
    var source =
      '<!DOCTYPE html>'
    + '<html>'
    + '<head><title>' + title + '</title>'
    + '<link rel="stylesheet" type="text/css" href="' + VIEW_SOURCE_CSS + '">'
    + '<style type="text/css">'
    + '#target { border: dashed 1px; background-color: lightyellow; }'
    + '</style>'
    + '</head>'
    + '<body id="viewsource"' + wrapClass
    +        ' onload="document.title=\''+title+'\'; document.getElementById(\'target\').scrollIntoView(true)">'
    + '<pre>'
    + this.getOuterMarkup(topNode, 0)
    + '</pre></body></html>'
    ; // end

    return { uri: "data:text/html;charset=utf-8," + encodeURIComponent(source),
             drawSelection: false, baseURI: node.ownerDocument.baseURI };
  },

  get wrapLongLines() {
    return Services.prefs.getBoolPref("view_source.wrap_long_lines");
  },

  getInnerMarkup: function(node, indent) {
    var str = '';
    for (var i = 0; i < node.childNodes.length; i++) {
      str += this.getOuterMarkup(node.childNodes.item(i), indent);
    }
    return str;
  },

  getOuterMarkup: function(node, indent) {
    var Node = node.ownerDocument.defaultView.Node;
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
          +  '&lt;<span class="start-tag">' + node.nodeName + '</span>';
      for (var i = 0; i < node.attributes.length; i++) {
        var attr = node.attributes.item(i);
        if (attr.nodeName.match(/^[-_]moz/)) {
          continue;
        }
        str += ' <span class="attribute-name">'
            +  attr.nodeName
            +  '</span>=<span class="attribute-value">"'
            +  this.unicodeToEntity(attr.nodeValue)
            +  '"</span>';
      }
      if (!node.hasChildNodes()) {
        str += "/&gt;";
      }
      else {
        str += "&gt;";
        var oldLine = this._lineCount;
        str += this.getInnerMarkup(node, indent + 2);
        if (oldLine == this._lineCount) {
          newline = "";
          padding = "";
        }
        else {
          newline = (this._lineCount == this._endTargetLine) ? "" : "\n";
          this._lineCount++;
        }
        str += newline + padding
            +  '&lt;/<span class="end-tag">' + node.nodeName + '</span>&gt;';
      }
      break;
    case Node.TEXT_NODE: // Text
      var tmp = node.nodeValue;
      tmp = tmp.replace(/(\n|\r|\t)+/g, " ");
      tmp = tmp.replace(/^ +/, "");
      tmp = tmp.replace(/ +$/, "");
      if (tmp.length != 0) {
        str += '<span class="text">' + this.unicodeToEntity(tmp) + '</span>';
      }
      break;
    default:
      break;
    }

    if (node == this._targetNode) {
      this._endTargetLine = this._lineCount;
      str += '</pre><pre>';
    }
    return str;
  },

  unicodeToEntity: function(text) {
    const charTable = {
      '&': '&amp;<span class="entity">amp;</span>',
      '<': '&amp;<span class="entity">lt;</span>',
      '>': '&amp;<span class="entity">gt;</span>',
      '"': '&amp;<span class="entity">quot;</span>'
    };

    function charTableLookup(letter) {
      return charTable[letter];
    }

    function convertEntity(letter) {
      try {
        var unichar = this._entityConverter
                          .ConvertToEntity(letter, entityVersion);
        var entity = unichar.substring(1); // extract '&'
        return '&amp;<span class="entity">' + entity + '</span>';
      } catch (ex) {
        return letter;
      }
    }

    if (!this._entityConverter) {
      try {
        this._entityConverter = Cc["@mozilla.org/intl/entityconverter;1"]
                                  .createInstance(Ci.nsIEntityConverter);
      } catch(e) { }
    }

    const entityVersion = Ci.nsIEntityConverter.entityW3C;

    var str = text;

    // replace chars in our charTable
    str = str.replace(/[<>&"]/g, charTableLookup);

    // replace chars > 0x7f via nsIEntityConverter
    str = str.replace(/[^\0-\u007f]/g, convertEntity);

    return str;
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

