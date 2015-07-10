/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;
let Cr = Components.results;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

var global = this;


// Lazily load the finder code
addMessageListener("Finder:Initialize", function () {
  let {RemoteFinderListener} = Cu.import("resource://gre/modules/RemoteFinder.jsm", {});
  new RemoteFinderListener(global);
});

let ClickEventHandler = {
  init: function init() {
    this._scrollable = null;
    this._scrolldir = "";
    this._startX = null;
    this._startY = null;
    this._screenX = null;
    this._screenY = null;
    this._lastFrame = null;

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
          this._scrollable.scrollLeftMax &&
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
      if (this._scrollable.scrollMaxX > 0) {
        this._scrolldir = this._scrollable.scrollMaxY > 0 ? "NSEW" : "EW";
      } else if (this._scrollable.scrollMaxY > 0) {
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
    this._lastFrame = content.mozAnimationStartTime;

    content.mozRequestAnimationFrame(this);
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

    if (this._scrollable instanceof content.Window) {
      this._scrollable.scrollBy(actualScrollX, actualScrollY);
    } else { // an element with overflow
      this._scrollable.scrollLeft += actualScrollX;
      this._scrollable.scrollTop += actualScrollY;
    }
    content.mozRequestAnimationFrame(this);
  },

  sample: function(timestamp) {
    this.autoscrollLoop(timestamp);
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

let PopupBlocking = {
  popupData: null,
  popupDataInternal: null,

  init: function() {
    addEventListener("DOMPopupBlocked", this, true);
    addEventListener("pageshow", this, true);
    addEventListener("pagehide", this, true);

    addMessageListener("PopupBlocking:UnblockPopup", this);
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
            dwi.open(data.popupWindowURI, data.popupWindowName, data.popupWindowFeatures);
          }
        }
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
  },

  onPopupBlocked: function(ev) {
    if (!this.popupData) {
      this.popupData = new Array();
      this.popupDataInternal = new Array();
    }

    let obj = {
      popupWindowURI: ev.popupWindowURI.spec,
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
                     {blockedPopups: this.popupData, freshPopup: freshPopup});
  },
};
PopupBlocking.init();

XPCOMUtils.defineLazyGetter(this, "console", () => {
  // Set up console.* for frame scripts.
  let Console = Components.utils.import("resource://gre/modules/devtools/Console.jsm", {});
  return new Console.ConsoleAPI();
});

let Printing = {
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
    "Printing:Preview:UpdatePageCount",
    "Printing:Print",
  ],

  init() {
    this.MESSAGES.forEach(msgName => addMessageListener(msgName, this));
  },

  get shouldSavePrintSettings() {
    return Services.prefs.getBoolPref("print.use_global_printsettings", false) &&
           Services.prefs.getBoolPref("print.save_print_settings", false);
  },

  receiveMessage(message) {
    let objects = message.objects;
    let data = message.data;
    switch(message.name) {
      case "Printing:Preview:Enter": {
        this.enterPrintPreview(Services.wm.getOuterWindowWithId(data.windowID));
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

      case "Printing:Preview:UpdatePageCount": {
        this.updatePageCount();
        break;
      }

      case "Printing:Print": {
        this.print(Services.wm.getOuterWindowWithId(data.windowID));
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

  enterPrintPreview(contentWindow) {
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

  print(contentWindow) {
    let printSettings = this.getPrintSettings();
    let rv = Cr.NS_OK;
    try {
      let print = contentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                               .getInterface(Ci.nsIWebBrowserPrint);
      print.print(printSettings, null);
    } catch(e) {
      // Pressing cancel is expressed as an NS_ERROR_ABORT return value,
      // causing an exception to be thrown which we catch here.
      if (e.result != Cr.NS_ERROR_ABORT) {
        Cu.reportError(`In Printing:Print:Done handler, got unexpected rv
                        ${e.result}.`);
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

let FindBar = {
  /* Please keep in sync with toolkit/content/widgets/findbar.xml */
  FIND_NORMAL: 0,
  FIND_TYPEAHEAD: 1,
  FIND_LINKS: 2,
  FAYT_LINKS_KEY: "'".charCodeAt(0),
  FAYT_TEXT_KEY: "/".charCodeAt(0),

  _findMode: 0,
  get _findAsYouType() {
    return Services.prefs.getBoolPref("accessibility.typeaheadfind");
  },

  init() {
    addMessageListener("Findbar:UpdateState", this);
    Services.els.addSystemEventListener(global, "keypress", this, false);
    Services.els.addSystemEventListener(global, "mouseup", this, false);
  },

  receiveMessage(msg) {
    switch (msg.name) {
      case "Findbar:UpdateState":
        this._findMode = msg.data.findMode;
        this._findAsYouType = msg.data.findAsYouType;
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
  _shouldFastFind() {
    //XXXgijs: why all these shenanigans? Why not use the event's target?
    let focusedWindow = {};
    let elt = Services.focus.getFocusedElementForWindow(content, true, focusedWindow);
    let win = focusedWindow.value;
    let {BrowserUtils} = Cu.import("resource://gre/modules/BrowserUtils.jsm", {});
    return BrowserUtils.shouldFastFind(elt, win);
  },

  _onKeypress(event) {
    // Useless keys:
    if (event.ctrlKey || event.altKey || event.metaKey || event.defaultPrevented) {
      return;
    }
    // Not interested in random keypresses most of the time:
    if (this._findMode == this.FIND_NORMAL && !this._findAsYouType &&
        event.charCode != this.FAYT_LINKS_KEY && event.charCode != this.FAYT_TEXT_KEY) {
      return;
    }

    // Check the focused element etc.
    if (!this._shouldFastFind()) {
      return;
    }

    let fakeEvent = {};
    for (let k in event) {
      if (typeof event[k] != "object" && typeof event[k] != "function") {
        fakeEvent[k] = event[k];
      }
    }
    // sendSyncMessage returns an array of the responses from all listeners
    let rv = sendSyncMessage("Findbar:Keypress", fakeEvent);
    if (rv.indexOf(false) !== -1) {
      event.preventDefault();
      return false;
    }
  },

  _onMouseup(event) {
    if (this._findMode != this.FIND_NORMAL)
      sendAsyncMessage("Findbar:Mouseup");
  },
};
FindBar.init();

// An event listener for custom "WebChannelMessageToChrome" events on pages.
addEventListener("WebChannelMessageToChrome", function (e) {
  // If target is window then we want the document principal, otherwise fallback to target itself.
  let principal = e.target.nodePrincipal ? e.target.nodePrincipal : e.target.document.nodePrincipal;

  if (e.detail) {
    sendAsyncMessage("WebChannelMessageToChrome", e.detail, { eventTarget: e.target }, principal);
  } else  {
    Cu.reportError("WebChannel message failed. No message detail.");
  }
}, true, true);

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
