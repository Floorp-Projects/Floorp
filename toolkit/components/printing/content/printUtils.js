// -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*-

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * PrintUtils is a utility for front-end code to trigger common print
 * operations (printing, show print preview, show page settings).
 *
 * Unfortunately, likely due to inconsistencies in how different operating
 * systems do printing natively, our XPCOM-level printing interfaces
 * are a bit confusing and the method by which we do something basic
 * like printing a page is quite circuitous.
 *
 * To compound that, we need to support remote browsers, and that means
 * kicking off the print jobs in the content process. This means we send
 * messages back and forth to that process. browser-content.js contains
 * the object that listens and responds to the messages that PrintUtils
 * sends.
 *
 * This also means that <xul:browser>'s that hope to use PrintUtils must have
 * their type attribute set to either "content", "content-targetable", or
 * "content-primary".
 *
 * PrintUtils sends messages at different points in its implementation, but
 * their documentation is consolidated here for ease-of-access.
 *
 *
 * Messages sent:
 *
 *   Printing:Print
 *     Kick off a print job for a nsIDOMWindow, passing the outer window ID as
 *     windowID.
 *
 *   Printing:Preview:Enter
 *     This message is sent to put content into print preview mode. We pass
 *     the content window of the browser we're showing the preview of, and
 *     the target of the message is the browser that we'll be showing the
 *     preview in.
 *
 *   Printing:Preview:Exit
 *     This message is sent to take content out of print preview mode.
 *
 *
 * Messages Received
 *
 *   Printing:Preview:Entered
 *     This message is sent by the content process once it has completed
 *     putting the content into print preview mode. We must wait for that to
 *     to complete before switching the chrome UI to print preview mode,
 *     otherwise we have layout issues.
 *
 *   Printing:Preview:StateChange, Printing:Preview:ProgressChange
 *     Due to a timing issue resulting in a main-process crash, we have to
 *     manually open the progress dialog for print preview. The progress
 *     dialog is opened here in PrintUtils, and then we listen for update
 *     messages from the child. Bug 1088061 has been filed to investigate
 *     other solutions.
 *
 */

var gPrintSettingsAreGlobal = false;
var gSavePrintSettings = false;
var gFocusedElement = null;

var PrintUtils = {
  /**
   * Shows the page setup dialog, and saves any settings changed in
   * that dialog if print.save_print_settings is set to true.
   *
   * @return true on success, false on failure
   */
  showPageSetup: function () {
    try {
      var printSettings = this.getPrintSettings();
      var PRINTPROMPTSVC = Components.classes["@mozilla.org/embedcomp/printingprompt-service;1"]
                                     .getService(Components.interfaces.nsIPrintingPromptService);
      PRINTPROMPTSVC.showPageSetup(window, printSettings, null);
      if (gSavePrintSettings) {
        // Page Setup data is a "native" setting on the Mac
        var PSSVC = Components.classes["@mozilla.org/gfx/printsettings-service;1"]
                              .getService(Components.interfaces.nsIPrintSettingsService);
        PSSVC.savePrintSettingsToPrefs(printSettings, true, printSettings.kInitSaveNativeData);
      }
    } catch (e) {
      dump("showPageSetup "+e+"\n");
      return false;
    }
    return true;
  },

  /**
   * Starts the process of printing the contents of a window.
   *
   * @param aWindowID
   *        The outer window ID of the nsIDOMWindow to print.
   * @param aBrowser
   *        The <xul:browser> that the nsIDOMWindow for aWindowID belongs to.
   */
  printWindow: function (aWindowID, aBrowser)
  {
    let mm = aBrowser.messageManager;
    mm.sendAsyncMessage("Printing:Print", {
      windowID: aWindowID,
    });
  },

  /**
   * Deprecated.
   *
   * Starts the process of printing the contents of window.content.
   *
   */
  print: function ()
  {
    if (gBrowser) {
      return this.printWindow(gBrowser.selectedBrowser.outerWindowID,
                              gBrowser.selectedBrowser);
    }

    if (this.usingRemoteTabs) {
      throw new Error("PrintUtils.print cannot be run in windows running with " +
                      "remote tabs. Use PrintUtils.printWindow instead.");
    }

    let domWindow = window.content;
    let ifReq = domWindow.QueryInterface(Components.interfaces.nsIInterfaceRequestor);
    let browser = ifReq.getInterface(Components.interfaces.nsIWebNavigation)
                       .QueryInterface(Components.interfaces.nsIDocShell)
                       .chromeEventHandler;
    if (!browser) {
      throw new Error("PrintUtils.print could not resolve content window " +
                      "to a browser.");
    }

    let windowID = ifReq.getInterface(Components.interfaces.nsIDOMWindowUtils)
                        .outerWindowID;

    let Deprecated = Components.utils.import("resource://gre/modules/Deprecated.jsm", {}).Deprecated;
    let msg = "PrintUtils.print is now deprecated. Please use PrintUtils.printWindow.";
    let url = "https://developer.mozilla.org/en-US/docs/Mozilla/Tech/XUL/Printing";
    Deprecated.warning(msg, url);

    this.printWindow(windowID, browser);
  },

  /**
   * Initializes print preview.
   *
   * @param aListenerObj
   *        An object that defines the following functions:
   *
   *        getPrintPreviewBrowser:
   *          Returns the <xul:browser> to display the print preview in. This
   *          <xul:browser> must have its type attribute set to "content",
   *          "content-targetable", or "content-primary".
   *
   *        getSourceBrowser:
   *          Returns the <xul:browser> that contains the document being
   *          printed. This <xul:browser> must have its type attribute set to
   *          "content", "content-targetable", or "content-primary".
   *
   *        getNavToolbox:
   *          Returns the primary toolbox for this window.
   *
   *        onEnter:
   *          Called upon entering print preview.
   *
   *        onExit:
   *          Called upon exiting print preview.
   *
   *        These methods must be defined. printPreview can be called
   *        with aListenerObj as null iff this window is already displaying
   *        print preview (in which case, the previous aListenerObj passed
   *        to it will be used).
   */
  printPreview: function (aListenerObj)
  {
    // if we're already in PP mode, don't set the listener; chances
    // are it is null because someone is calling printPreview() to
    // get us to refresh the display.
    if (!this.inPrintPreview) {
      this._listener = aListenerObj;
      this._sourceBrowser = aListenerObj.getSourceBrowser();
      this._originalTitle = this._sourceBrowser.contentTitle;
      this._originalURL = this._sourceBrowser.currentURI.spec;
    } else {
      // collapse the browser here -- it will be shown in
      // enterPrintPreview; this forces a reflow which fixes display
      // issues in bug 267422.
      this._sourceBrowser = this._listener.getPrintPreviewBrowser();
      this._sourceBrowser.collapsed = true;
    }

    this._webProgressPP = {};
    let ppParams        = {};
    let notifyOnOpen    = {};
    let printSettings   = this.getPrintSettings();
    // Here we get the PrintingPromptService so we can display the PP Progress from script
    // For the browser implemented via XUL with the PP toolbar we cannot let it be
    // automatically opened from the print engine because the XUL scrollbars in the PP window
    // will layout before the content window and a crash will occur.
    // Doing it all from script, means it lays out before hand and we can let printing do its own thing
    let PPROMPTSVC = Components.classes["@mozilla.org/embedcomp/printingprompt-service;1"]
                               .getService(Components.interfaces.nsIPrintingPromptService);
    // just in case we are already printing,
    // an error code could be returned if the Progress Dialog is already displayed
    try {
      PPROMPTSVC.showProgress(window, null, printSettings, this._obsPP, false,
                              this._webProgressPP, ppParams, notifyOnOpen);
      if (ppParams.value) {
        ppParams.value.docTitle = this._originalTitle;
        ppParams.value.docURL   = this._originalURL;
      }

      // this tells us whether we should continue on with PP or
      // wait for the callback via the observer
      if (!notifyOnOpen.value.valueOf() || this._webProgressPP.value == null) {
        this.enterPrintPreview();
      }
    } catch (e) {
      this.enterPrintPreview();
    }
  },

  /**
   * Returns the nsIWebBrowserPrint associated with some content window.
   * This method is being kept here for compatibility reasons, but should not
   * be called by code hoping to support e10s / remote browsers.
   *
   * @param aWindow
   *        The window from which to get the nsIWebBrowserPrint from.
   * @return nsIWebBrowserPrint
   */
  getWebBrowserPrint: function (aWindow)
  {
    let Deprecated = Components.utils.import("resource://gre/modules/Deprecated.jsm", {}).Deprecated;
    let text = "getWebBrowserPrint is now deprecated, and fully unsupported for " +
               "multi-process browsers. Please use a frame script to get " +
               "access to nsIWebBrowserPrint from content.";
    let url = "https://developer.mozilla.org/en-US/docs/Printing_from_a_XUL_App";
    Deprecated.warning(text, url);

    if (this.usingRemoteTabs) {
      return {};
    }

    var contentWindow = aWindow || window.content;
    return contentWindow.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                        .getInterface(Components.interfaces.nsIWebBrowserPrint);
  },

  /**
   * Returns the nsIWebBrowserPrint from the print preview browser's docShell.
   * This method is being kept here for compatibility reasons, but should not
   * be called by code hoping to support e10s / remote browsers.
   *
   * @return nsIWebBrowserPrint
   */
  getPrintPreview: function() {
    let Deprecated = Components.utils.import("resource://gre/modules/Deprecated.jsm", {}).Deprecated;
    let text = "getPrintPreview is now deprecated, and fully unsupported for " +
               "multi-process browsers. Please use a frame script to get " +
               "access to nsIWebBrowserPrint from content.";
    let url = "https://developer.mozilla.org/en-US/docs/Printing_from_a_XUL_App";
    Deprecated.warning(text, url);

    if (this.usingRemoteTabs) {
      return {};
    }

    return this._listener.getPrintPreviewBrowser().docShell.printPreview;
  },

  get inPrintPreview() {
    return document.getElementById("print-preview-toolbar") != null;
  },

  ////////////////////////////////////////////////////
  // "private" methods and members. Don't use them. //
  ///////////////////////////////////////////////////

  _listener: null,
  _closeHandlerPP: null,
  _webProgressPP: null,
  _sourceBrowser: null,
  _originalTitle: "",
  _originalURL: "",

  get usingRemoteTabs() {
    // We memoize this, since it's highly unlikely to change over the lifetime
    // of the window.
    let usingRemoteTabs =
      window.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
            .getInterface(Components.interfaces.nsIWebNavigation)
            .QueryInterface(Components.interfaces.nsILoadContext)
            .useRemoteTabs;
    delete this.usingRemoteTabs;
    return this.usingRemoteTabs = usingRemoteTabs;
  },

  receiveMessage(aMessage) {
    if (!this._webProgressPP.value) {
      // We somehow didn't get a nsIWebProgressListener to be updated...
      // I guess there's nothing to do.
      return;
    }

    let listener = this._webProgressPP.value;
    let mm = aMessage.target.messageManager;
    let data = aMessage.data;

    switch (aMessage.name) {
      case "Printing:Preview:ProgressChange": {
        return listener.onProgressChange(null, null,
                                         data.curSelfProgress,
                                         data.maxSelfProgress,
                                         data.curTotalProgress,
                                         data.maxTotalProgress);
        break;
      }

      case "Printing:Preview:StateChange": {
        if (data.stateFlags & Components.interfaces.nsIWebProgressListener.STATE_STOP) {
          // Strangely, the printing engine sends 2 STATE_STOP messages when
          // print preview is finishing. One has the STATE_IS_DOCUMENT flag,
          // the other has the STATE_IS_NETWORK flag. However, the webProgressPP
          // listener stops listening once the first STATE_STOP is sent.
          // Any subsequent messages result in NS_ERROR_FAILURE errors getting
          // thrown. This should all get torn out once bug 1088061 is fixed.
          mm.removeMessageListener("Printing:Preview:StateChange", this);
          mm.removeMessageListener("Printing:Preview:ProgressChange", this);
        }

        return listener.onStateChange(null, null,
                                      data.stateFlags,
                                      data.status);
        break;
      }
    }
  },

  setPrinterDefaultsForSelectedPrinter: function (aPSSVC, aPrintSettings)
  {
    if (!aPrintSettings.printerName)
      aPrintSettings.printerName = aPSSVC.defaultPrinterName;

    // First get any defaults from the printer 
    aPSSVC.initPrintSettingsFromPrinter(aPrintSettings.printerName, aPrintSettings);
    // now augment them with any values from last time
    aPSSVC.initPrintSettingsFromPrefs(aPrintSettings, true,  aPrintSettings.kInitSaveAll);
  },

  getPrintSettings: function ()
  {
    var pref = Components.classes["@mozilla.org/preferences-service;1"]
                         .getService(Components.interfaces.nsIPrefBranch);
    if (pref) {
      gPrintSettingsAreGlobal = pref.getBoolPref("print.use_global_printsettings", false);
      gSavePrintSettings = pref.getBoolPref("print.save_print_settings", false);
    }

    var printSettings;
    try {
      var PSSVC = Components.classes["@mozilla.org/gfx/printsettings-service;1"]
                            .getService(Components.interfaces.nsIPrintSettingsService);
      if (gPrintSettingsAreGlobal) {
        printSettings = PSSVC.globalPrintSettings;
        this.setPrinterDefaultsForSelectedPrinter(PSSVC, printSettings);
      } else {
        printSettings = PSSVC.newPrintSettings;
      }
    } catch (e) {
      dump("getPrintSettings: "+e+"\n");
    }
    return printSettings;
  },

  // This observer is called once the progress dialog has been "opened"
  _obsPP:
  {
    observe: function(aSubject, aTopic, aData)
    {
      // delay the print preview to show the content of the progress dialog
      setTimeout(function () { PrintUtils.enterPrintPreview(); }, 0);
    },

    QueryInterface : function(iid)
    {
      if (iid.equals(Components.interfaces.nsIObserver) ||
          iid.equals(Components.interfaces.nsISupportsWeakReference) ||
          iid.equals(Components.interfaces.nsISupports))
        return this;
      throw Components.results.NS_NOINTERFACE;
    }
  },

  enterPrintPreview: function ()
  {
    // Send a message to the print preview browser to initialize
    // print preview. If we happen to have gotten a print preview
    // progress listener from nsIPrintingPromptService.showProgress
    // in printPreview, we add listeners to feed that progress
    // listener.
    let ppBrowser = this._listener.getPrintPreviewBrowser();
    let mm = ppBrowser.messageManager;
    mm.sendAsyncMessage("Printing:Preview:Enter", {
      windowID: this._sourceBrowser.outerWindowID,
    });

    if (this._webProgressPP.value) {
      mm.addMessageListener("Printing:Preview:StateChange", this);
      mm.addMessageListener("Printing:Preview:ProgressChange", this);
    }

    let onEntered = (message) => {
      mm.removeMessageListener("Printing:PrintPreview:Entered", onEntered);

      if (message.data.failed) {
        // Something went wrong while putting the document into print preview
        // mode. Bail out.
        this._listener.onEnter();
        this._listener.onExit();
        return;
      }

      // Stash the focused element so that we can return to it after exiting
      // print preview.
      gFocusedElement = document.commandDispatcher.focusedElement;

      let printPreviewTB = document.getElementById("print-preview-toolbar");
      if (printPreviewTB) {
        printPreviewTB.updateToolbar();
        ppBrowser.collapsed = false;
        ppBrowser.focus();
        return;
      }

      // Set the original window as an active window so any mozPrintCallbacks can
      // run without delayed setTimeouts.
      this._sourceBrowser.docShellIsActive = true;

      // show the toolbar after we go into print preview mode so
      // that we can initialize the toolbar with total num pages
      const XUL_NS =
        "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
      printPreviewTB = document.createElementNS(XUL_NS, "toolbar");
      printPreviewTB.setAttribute("printpreview", true);
      printPreviewTB.id = "print-preview-toolbar";

      let navToolbox = this._listener.getNavToolbox();
      navToolbox.parentNode.insertBefore(printPreviewTB, navToolbox);
      printPreviewTB.initialize(ppBrowser);

      // copy the window close handler
      if (document.documentElement.hasAttribute("onclose"))
        this._closeHandlerPP = document.documentElement.getAttribute("onclose");
      else
        this._closeHandlerPP = null;
      document.documentElement.setAttribute("onclose", "PrintUtils.exitPrintPreview(); return false;");

      // disable chrome shortcuts...
      window.addEventListener("keydown", this.onKeyDownPP, true);
      window.addEventListener("keypress", this.onKeyPressPP, true);

      ppBrowser.collapsed = false;
      ppBrowser.focus();
      // on Enter PP Call back
      this._listener.onEnter();
    };

    mm.addMessageListener("Printing:Preview:Entered", onEntered);
  },

  exitPrintPreview: function ()
  {
    let ppBrowser = this._listener.getPrintPreviewBrowser();
    let browserMM = ppBrowser.messageManager;
    browserMM.sendAsyncMessage("Printing:Preview:Exit");
    window.removeEventListener("keydown", this.onKeyDownPP, true);
    window.removeEventListener("keypress", this.onKeyPressPP, true);

    // restore the old close handler
    document.documentElement.setAttribute("onclose", this._closeHandlerPP);
    this._closeHandlerPP = null;

    // remove the print preview toolbar
    let printPreviewTB = document.getElementById("print-preview-toolbar");
    this._listener.getNavToolbox().parentNode.removeChild(printPreviewTB);

    let fm = Components.classes["@mozilla.org/focus-manager;1"]
                       .getService(Components.interfaces.nsIFocusManager);
    if (gFocusedElement)
      fm.setFocus(gFocusedElement, fm.FLAG_NOSCROLL);
    else
      this._sourceBrowser.focus();
    gFocusedElement = null;

    this._listener.onExit();
  },

  onKeyDownPP: function (aEvent)
  {
    // Esc exits the PP
    if (aEvent.keyCode == aEvent.DOM_VK_ESCAPE) {
      PrintUtils.exitPrintPreview();
    }
  },

  onKeyPressPP: function (aEvent)
  {
    var closeKey;
    try {
      closeKey = document.getElementById("key_close")
                         .getAttribute("key");
      closeKey = aEvent["DOM_VK_"+closeKey];
    } catch (e) {}
    var isModif = aEvent.ctrlKey || aEvent.metaKey;
    // Ctrl-W exits the PP
    if (isModif &&
        (aEvent.charCode == closeKey || aEvent.charCode == closeKey + 32)) {
      PrintUtils.exitPrintPreview();
    }
    else if (isModif) {
      var printPreviewTB = document.getElementById("print-preview-toolbar");
      var printKey = document.getElementById("printKb").getAttribute("key").toUpperCase();
      var pressedKey = String.fromCharCode(aEvent.charCode).toUpperCase();
      if (printKey == pressedKey) {
        printPreviewTB.print();
      }
    }
    // cancel shortkeys
    if (isModif) {
      aEvent.preventDefault();
      aEvent.stopPropagation();
    }
  }
}
