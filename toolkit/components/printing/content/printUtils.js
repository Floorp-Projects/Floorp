// This file is loaded into the browser window scope.
/* eslint-env mozilla/browser-window */

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
 * messages back and forth to that process via the Printing actor.
 *
 * This also means that <xul:browser>'s that hope to use PrintUtils must have
 * their type attribute set to "content".
 *
 * Messages sent:
 *
 *   Printing:Preview:Enter
 *     This message is sent to put content into print preview mode. We pass
 *     the content window of the browser we're showing the preview of, and
 *     the target of the message is the browser that we'll be showing the
 *     preview in.
 *
 *   Printing:Preview:Exit
 *     This message is sent to take content out of print preview mode.
 */

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "PRINT_TAB_MODAL",
  "print.tab_modal.enabled",
  false
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "PRINT_ALWAYS_SILENT",
  "print.always_print_silent",
  false
);

ChromeUtils.defineModuleGetter(
  this,
  "PromptUtils",
  "resource://gre/modules/SharedPromptUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "PrintingParent",
  "resource://gre/actors/PrintingParent.jsm"
);

var gFocusedElement = null;

var gPendingPrintPreviews = new Map();

var PrintUtils = {
  SAVE_TO_PDF_PRINTER: "Mozilla Save to PDF",

  get _bundle() {
    delete this._bundle;
    return (this._bundle = Services.strings.createBundle(
      "chrome://global/locale/printing.properties"
    ));
  },

  /**
   * Shows the page setup dialog, and saves any settings changed in
   * that dialog if print.save_print_settings is set to true.
   *
   * @return true on success, false on failure
   */
  showPageSetup() {
    let printSettings = this.getPrintSettings();
    // If we come directly from the Page Setup menu, the hack in
    // _enterPrintPreview will not have been invoked to set the last used
    // printer name. For the reasons outlined at that hack, we want that set
    // here too.
    let PSSVC = Cc["@mozilla.org/gfx/printsettings-service;1"].getService(
      Ci.nsIPrintSettingsService
    );
    if (!PSSVC.lastUsedPrinterName) {
      if (printSettings.printerName) {
        PSSVC.savePrintSettingsToPrefs(
          printSettings,
          false,
          Ci.nsIPrintSettings.kInitSavePrinterName
        );
        PSSVC.savePrintSettingsToPrefs(
          printSettings,
          true,
          Ci.nsIPrintSettings.kInitSaveAll
        );
      }
    }
    try {
      var PRINTPROMPTSVC = Cc[
        "@mozilla.org/embedcomp/printingprompt-service;1"
      ].getService(Ci.nsIPrintingPromptService);
      PRINTPROMPTSVC.showPageSetupDialog(window, printSettings, null);
    } catch (e) {
      dump("showPageSetup " + e + "\n");
      return false;
    }
    return true;
  },

  getPreviewBrowser(sourceBrowser) {
    let dialogBox = gBrowser.getTabDialogBox(sourceBrowser);
    for (let dialog of dialogBox.getTabDialogManager()._dialogs) {
      let browser = dialog._box.querySelector(".printPreviewBrowser");
      if (browser) {
        return browser;
      }
    }
    return null;
  },

  /**
   * Updates the hidden state of the "Print preview" and "Page Setup"
   * menu items in the file menu depending on the print tab modal pref.
   * The print preview menu item is not available on mac.
   */
  updatePrintPreviewMenuHiddenState() {
    let printPreviewMenuItem = document.getElementById("menu_printPreview");
    if (printPreviewMenuItem) {
      printPreviewMenuItem.hidden = PRINT_TAB_MODAL;
    }
    let pageSetupMenuItem = document.getElementById("menu_printSetup");
    if (pageSetupMenuItem) {
      pageSetupMenuItem.hidden = PRINT_TAB_MODAL;
    }
  },

  createPreviewBrowsers(aBrowsingContext, aDialogBrowser, aPrintFrameOnly) {
    let _createPreviewBrowser = previewType => {
      // When we're not previewing the selection or printing only the frame, we
      // want to make sure that the top-level browser is being printed.
      let browsingContext =
        previewType == "selection" || aPrintFrameOnly
          ? aBrowsingContext
          : aBrowsingContext.top.embedderElement.browsingContext;
      let browser = gBrowser.createBrowser({
        remoteType: browsingContext.currentRemoteType,
        userContextId: browsingContext.originAttributes.userContextId,
        initialBrowsingContextGroupId: browsingContext.group.id,
        skipLoad: true,
        initiallyActive: true,
      });
      browser.addEventListener("DOMWindowClose", function(e) {
        // Ignore close events from printing, see the code creating browsers in
        // printUtils.js and nsDocumentViewer::OnDonePrinting.
        //
        // When we print with the new print UI we don't bother creating a new
        // <browser> element, so the close event gets dispatched to us.
        //
        // Ignoring it is harmless (and doesn't cause correctness issues, because
        // the preview document can't run script anyways).
        e.preventDefault();
        e.stopPropagation();
      });
      browser.addEventListener("contextmenu", function(e) {
        e.preventDefault();
      });
      browser.classList.add("printPreviewBrowser");
      browser.setAttribute("flex", "1");
      browser.setAttribute("printpreview", "true");
      browser.setAttribute("previewtype", previewType);
      document.l10n.setAttributes(browser, "printui-preview-label");
      return browser;
    };

    let previewStack = document.importNode(
      document.getElementById("printPreviewStackTemplate").content,
      true
    ).firstElementChild;

    let previewBrowser = _createPreviewBrowser("primary");
    previewStack.append(previewBrowser);
    let selectionPreviewBrowser;
    if (aBrowsingContext.currentRemoteType) {
      selectionPreviewBrowser = _createPreviewBrowser("selection");
      previewStack.append(selectionPreviewBrowser);
    }

    // show the toolbar after we go into print preview mode so
    // that we can initialize the toolbar with total num pages
    let previewPagination = document.createElement("printpreview-pagination");
    previewPagination.classList.add("printPreviewNavigation");
    previewStack.append(previewPagination);

    aDialogBrowser.parentElement.prepend(previewStack);
    return { previewBrowser, selectionPreviewBrowser };
  },

  /**
   * Opens the tab modal version of the print UI for the current tab.
   *
   * @param aBrowsingContext
   *        The BrowsingContext of the window to print.
   * @param aExistingPreviewBrowser
   *        An existing browser created for printing from window.print().
   * @param aPrintInitiationTime
   *        The time the print was initiated (typically by the user) as obtained
   *        from `Date.now()`.  That is, the initiation time as the number of
   *        milliseconds since January 1, 1970.
   * @param aPrintSelectionOnly
   *        Whether to print only the active selection of the given browsing
   *        context.
   * @param aPrintFrameOnly
   *        Whether to print the selected frame only
   * @return promise resolving when the dialog is open, rejected if the preview
   *         fails.
   */
  async _openTabModalPrint(
    aBrowsingContext,
    aExistingPreviewBrowser,
    aPrintInitiationTime,
    aPrintSelectionOnly,
    aPrintFrameOnly
  ) {
    let hasSelection = aPrintSelectionOnly;
    if (!aPrintSelectionOnly) {
      let sourceActor = aBrowsingContext.currentWindowGlobal.getActor(
        "PrintingSelection"
      );
      try {
        hasSelection = await sourceActor.sendQuery(
          "PrintingSelection:HasSelection"
        );
      } catch (ex) {}
    }

    let sourceBrowser = aBrowsingContext.top.embedderElement;
    let previewBrowser = this.getPreviewBrowser(sourceBrowser);
    if (previewBrowser) {
      // Don't open another dialog if we're already printing.
      //
      // XXX This can be racy can't it? getPreviewBrowser looks at browser that
      // we set up after opening the dialog. But I guess worst case we just
      // open two dialogs so...
      if (aExistingPreviewBrowser) {
        aExistingPreviewBrowser.remove();
      }
      return Promise.reject();
    }

    // Create a preview browser.
    let args = PromptUtils.objectToPropBag({
      previewBrowser: aExistingPreviewBrowser,
      printSelectionOnly: !!aPrintSelectionOnly,
      hasSelection,
      printFrameOnly: !!aPrintFrameOnly,
    });
    let dialogBox = gBrowser.getTabDialogBox(sourceBrowser);
    return dialogBox.open(
      `chrome://global/content/print.html?browsingContextId=${aBrowsingContext.id}&printInitiationTime=${aPrintInitiationTime}`,
      { features: "resizable=no", sizeTo: "available" },
      args
    );
  },

  /**
   * Initialize a print, this will open the tab modal UI if it is enabled or
   * defer to the native dialog/silent print.
   *
   * @param aBrowsingContext
   *        The BrowsingContext of the window to print.
   *        Note that the browsing context could belong to a subframe of the
   *        tab that called window.print, or similar shenanigans.
   * @param aOptions
   *        {openWindowInfo}      Non-null if this call comes from window.print().
   *                              This is the nsIOpenWindowInfo object that has to
   *                              be passed down to createBrowser in order for the
   *                              child process to clone into it.
   *        {printSelectionOnly}  Whether to print only the active selection of
   *                              the given browsing context.
   *        {printFrameOnly}      Whether to print the selected frame.
   */
  startPrintWindow(aBrowsingContext, aOptions) {
    const printInitiationTime = Date.now();
    let openWindowInfo, printSelectionOnly, printFrameOnly;
    if (aOptions) {
      ({ openWindowInfo, printSelectionOnly, printFrameOnly } = aOptions);
    }
    let browser = null;
    if (openWindowInfo) {
      browser = document.createXULElement("browser");
      browser.openWindowInfo = openWindowInfo;
      browser.setAttribute("type", "content");
      let remoteType = aBrowsingContext.currentRemoteType;
      if (remoteType) {
        browser.setAttribute("remoteType", remoteType);
        browser.setAttribute("remote", "true");
      }
      // When the print process finishes, we get closed by
      // nsDocumentViewer::OnDonePrinting, or by the print preview code.
      //
      // When that happens, we should remove us from the DOM if connected.
      browser.addEventListener("DOMWindowClose", function(e) {
        if (browser.isConnected) {
          browser.remove();
        }
        e.stopPropagation();
        e.preventDefault();
      });
      browser.style.visibility = "collapse";
      document.documentElement.appendChild(browser);
    }

    if (
      PRINT_TAB_MODAL &&
      !PRINT_ALWAYS_SILENT &&
      (!openWindowInfo || openWindowInfo.isForWindowDotPrint)
    ) {
      let browsingContext = aBrowsingContext;
      let focusedBc = Services.focus.focusedContentBrowsingContext;
      if (
        focusedBc &&
        focusedBc.top.embedderElement == browsingContext.top.embedderElement &&
        (!openWindowInfo || !openWindowInfo.isForWindowDotPrint) &&
        !printFrameOnly
      ) {
        browsingContext = focusedBc;
      }
      this._openTabModalPrint(
        browsingContext,
        browser,
        printInitiationTime,
        printSelectionOnly,
        printFrameOnly
      ).catch(e => {
        Cu.reportError(e);
        if (browser) {
          browser.remove();
        }
      });
      return browser;
    }

    if (browser) {
      // Legacy print dialog or silent printing, the content process will print
      // in this <browser>.
      return browser;
    }

    let settings = this.getPrintSettings();
    settings.printSelectionOnly = printSelectionOnly;
    this.printWindow(aBrowsingContext, settings);
    return null;
  },

  /**
   * Starts the process of printing the contents of a window.
   *
   * @param aBrowsingContext
   *        The BrowsingContext of the window to print.
   * @param {Object?} aPrintSettings
   *        Optional print settings for the print operation
   */
  printWindow(aBrowsingContext, aPrintSettings) {
    let wg = aBrowsingContext.currentWindowGlobal;

    const printPreviewIsOpen = !!document.getElementById(
      "print-preview-toolbar"
    );

    if (printPreviewIsOpen) {
      this._logKeyedTelemetry("PRINT_DIALOG_OPENED_COUNT", "FROM_PREVIEW");
    } else {
      this._logKeyedTelemetry("PRINT_DIALOG_OPENED_COUNT", "FROM_PAGE");
    }

    // Use the passed in settings if provided, otherwise pull the saved ones.
    let printSettings = aPrintSettings || this.getPrintSettings();

    // Set the title so that the print dialog can pick it up and
    // use it to generate the filename for save-to-PDF.
    printSettings.title = this._originalTitle || wg.documentTitle;

    if (this._shouldSimplify) {
      // The generated document for simplified print preview has "about:blank"
      // as its URL. We need to set docURL here so that the print header/footer
      // can be given the original document's URL.
      printSettings.docURL = this._originalURL || wg.documentURI;
    }

    // At some point we should handle the Promise that this returns (report
    // rejection to telemetry?)
    let promise = aBrowsingContext.print(printSettings);

    if (printPreviewIsOpen) {
      if (this._shouldSimplify) {
        this._logKeyedTelemetry("PRINT_COUNT", "SIMPLIFIED");
      } else {
        this._logKeyedTelemetry("PRINT_COUNT", "WITH_PREVIEW");
      }
    } else {
      this._logKeyedTelemetry("PRINT_COUNT", "WITHOUT_PREVIEW");
    }

    return promise;
  },

  /**
   * Initializes print preview.
   *
   * @param aListenerObj
   *        An object that defines the following functions:
   *
   *        getPrintPreviewBrowser:
   *          Returns the <xul:browser> to display the print preview in. This
   *          <xul:browser> must have its type attribute set to "content".
   *
   *        getSimplifiedPrintPreviewBrowser:
   *          Returns the <xul:browser> to display the simplified print preview
   *          in. This <xul:browser> must have its type attribute set to
   *          "content".
   *
   *        getSourceBrowser:
   *          Returns the <xul:browser> that contains the document being
   *          printed. This <xul:browser> must have its type attribute set to
   *          "content".
   *
   *        getSimplifiedSourceBrowser:
   *          Returns the <xul:browser> that contains the simplified version
   *          of the document being printed. This <xul:browser> must have its
   *          type attribute set to "content".
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
   *
   *        Due to a timing issue resulting in a main-process crash, we have to
   *        manually open the progress dialog for print preview. The progress
   *        dialog is opened here in PrintUtils, and then we listen for update
   *        messages from the child. Bug 1558588 is about removing this.
   */
  printPreview(aListenerObj) {
    if (PRINT_TAB_MODAL) {
      let browsingContext = gBrowser.selectedBrowser.browsingContext;
      let focusedBc = Services.focus.focusedContentBrowsingContext;
      if (
        focusedBc &&
        focusedBc.top.embedderElement == browsingContext.top.embedderElement
      ) {
        browsingContext = focusedBc;
      }
      return this._openTabModalPrint(
        browsingContext,
        /* aExistingPreviewBrowser = */ undefined,
        Date.now()
      );
    }

    // If we already have a toolbar someone is calling printPreview() to get us
    // to refresh the display and aListenerObj won't be passed.
    let printPreviewTB = document.getElementById("print-preview-toolbar");
    if (!printPreviewTB) {
      this._listener = aListenerObj;
      this._sourceBrowser = aListenerObj.getSourceBrowser();
      this._originalTitle = this._sourceBrowser.contentTitle;
      this._originalURL = this._sourceBrowser.currentURI.spec;

      // Here we log telemetry data for when the user enters print preview.
      this.logTelemetry("PRINT_PREVIEW_OPENED_COUNT");
    } else {
      // Disable toolbar elements that can cause another update to be triggered
      // during this update.
      printPreviewTB.disableUpdateTriggers(true);

      // collapse the browser here -- it will be shown in
      // _enterPrintPreview; this forces a reflow which fixes display
      // issues in bug 267422.
      // We use the print preview browser as the source browser to avoid
      // re-initializing print preview with a document that might now have changed.
      this._sourceBrowser = this._shouldSimplify
        ? this._listener.getSimplifiedPrintPreviewBrowser()
        : this._listener.getPrintPreviewBrowser();
      this._sourceBrowser.collapsed = true;

      // If the user transits too quickly within preview and we have a pending
      // progress dialog, we will close it before opening a new one.
      this.ensureProgressDialogClosed();
    }

    this._webProgressPP = {};
    let ppParams = {};
    let notifyOnOpen = {};
    let printSettings = this.getPrintSettings();
    // Here we get the PrintingPromptService so we can display the PP Progress from script
    // For the browser implemented via XUL with the PP toolbar we cannot let it be
    // automatically opened from the print engine because the XUL scrollbars in the PP window
    // will layout before the content window and a crash will occur.
    // Doing it all from script, means it lays out before hand and we can let printing do its own thing
    let PPROMPTSVC = Cc[
      "@mozilla.org/embedcomp/printingprompt-service;1"
    ].getService(Ci.nsIPrintingPromptService);

    let promise = new Promise((resolve, reject) => {
      this._onEntered.push({ resolve, reject });
    });

    // just in case we are already printing,
    // an error code could be returned if the Progress Dialog is already displayed
    try {
      PPROMPTSVC.showPrintProgressDialog(
        window,
        printSettings,
        this._obsPP,
        false,
        this._webProgressPP,
        ppParams,
        notifyOnOpen
      );
      if (ppParams.value) {
        ppParams.value.docTitle = this._originalTitle;
        ppParams.value.docURL = this._originalURL;
      }

      // this tells us whether we should continue on with PP or
      // wait for the callback via the observer
      if (!notifyOnOpen.value.valueOf() || this._webProgressPP.value == null) {
        this._enterPrintPreview();
      }
    } catch (e) {
      this._enterPrintPreview();
    }
    return promise;
  },

  // "private" methods and members. Don't use them.

  _listener: null,
  _closeHandlerPP: null,
  _webProgressPP: null,
  _sourceBrowser: null,
  _originalTitle: "",
  _originalURL: "",
  _shouldSimplify: false,

  _getErrorCodeForNSResult(nsresult) {
    const MSG_CODES = [
      "GFX_PRINTER_NO_PRINTER_AVAILABLE",
      "GFX_PRINTER_NAME_NOT_FOUND",
      "GFX_PRINTER_COULD_NOT_OPEN_FILE",
      "GFX_PRINTER_STARTDOC",
      "GFX_PRINTER_ENDDOC",
      "GFX_PRINTER_STARTPAGE",
      "GFX_PRINTER_DOC_IS_BUSY",
      "ABORT",
      "NOT_AVAILABLE",
      "NOT_IMPLEMENTED",
      "OUT_OF_MEMORY",
      "UNEXPECTED",
    ];

    for (let code of MSG_CODES) {
      let nsErrorResult = "NS_ERROR_" + code;
      if (Cr[nsErrorResult] == nsresult) {
        return code;
      }
    }

    // PERR_FAILURE is the catch-all error message if we've gotten one that
    // we don't recognize.
    return "FAILURE";
  },

  _displayPrintingError(nsresult, isPrinting) {
    // The nsresults from a printing error are mapped to strings that have
    // similar names to the errors themselves. For example, for error
    // NS_ERROR_GFX_PRINTER_NO_PRINTER_AVAILABLE, the name of the string
    // for the error message is: PERR_GFX_PRINTER_NO_PRINTER_AVAILABLE. What's
    // more, if we're in the process of doing a print preview, it's possible
    // that there are strings specific for print preview for these errors -
    // if so, the names of those strings have _PP as a suffix. It's possible
    // that no print preview specific strings exist, in which case it is fine
    // to fall back to the original string name.
    let msgName = "PERR_" + this._getErrorCodeForNSResult(nsresult);
    let msg, title;
    if (!isPrinting) {
      // Try first with _PP suffix.
      let ppMsgName = msgName + "_PP";
      try {
        msg = this._bundle.GetStringFromName(ppMsgName);
      } catch (e) {
        // We allow localizers to not have the print preview error string,
        // and just fall back to the printing error string.
      }
    }

    if (!msg) {
      msg = this._bundle.GetStringFromName(msgName);
    }

    title = this._bundle.GetStringFromName(
      isPrinting
        ? "print_error_dialog_title"
        : "printpreview_error_dialog_title"
    );

    Services.prompt.alert(window, title, msg);

    Services.telemetry.keyedScalarAdd(
      "printing.error",
      this._getErrorCodeForNSResult(nsresult),
      1
    );
  },

  _setPrinterDefaultsForSelectedPrinter(
    aPSSVC,
    aPrintSettings,
    defaultsOnly = false
  ) {
    if (!aPrintSettings.printerName) {
      aPrintSettings.printerName = aPSSVC.lastUsedPrinterName;
      if (!aPrintSettings.printerName) {
        // It is important to try to avoid passing settings over to the
        // content process in the old print UI by saving to unprefixed prefs.
        // To avoid that we try to get the name of a printer we can use.
        let printerList = Cc["@mozilla.org/gfx/printerlist;1"].getService(
          Ci.nsIPrinterList
        );
        aPrintSettings.printerName = printerList.systemDefaultPrinterName;
      }
    }

    // First get any defaults from the printer. We want to skip this for Save to
    // PDF since it isn't a real printer and will throw.
    if (aPrintSettings.printerName != this.SAVE_TO_PDF_PRINTER) {
      aPSSVC.initPrintSettingsFromPrinter(
        aPrintSettings.printerName,
        aPrintSettings
      );
    }

    if (!defaultsOnly) {
      // now augment them with any values from last time
      aPSSVC.initPrintSettingsFromPrefs(
        aPrintSettings,
        true,
        aPrintSettings.kInitSaveAll
      );
    }
  },

  getPrintSettings(aPrinterName, defaultsOnly) {
    var printSettings;
    try {
      var PSSVC = Cc["@mozilla.org/gfx/printsettings-service;1"].getService(
        Ci.nsIPrintSettingsService
      );
      printSettings = PSSVC.newPrintSettings;
      if (aPrinterName) {
        printSettings.printerName = aPrinterName;
      }
      this._setPrinterDefaultsForSelectedPrinter(
        PSSVC,
        printSettings,
        defaultsOnly
      );
    } catch (e) {
      dump("getPrintSettings: " + e + "\n");
    }
    return printSettings;
  },

  // This observer is called once the progress dialog has been "opened"
  _obsPP: {
    observe(aSubject, aTopic, aData) {
      // Only process a null topic which means the progress dialog is open.
      if (aTopic) {
        return;
      }

      // delay the print preview to show the content of the progress dialog
      setTimeout(function() {
        PrintUtils._enterPrintPreview();
      }, 0);
    },

    QueryInterface: ChromeUtils.generateQI([
      "nsIObserver",
      "nsISupportsWeakReference",
    ]),
  },

  get shouldSimplify() {
    return this._shouldSimplify;
  },

  setSimplifiedMode(shouldSimplify) {
    this._shouldSimplify = shouldSimplify;
  },

  _onEntered: [],

  /**
   * Currently, we create a new print preview browser to host the simplified
   * cloned-document when Simplify Page option is used on preview. To accomplish
   * this, we need to keep track of what browser should be presented, based on
   * whether the 'Simplify page' checkbox is checked.
   *
   * _ppBrowsers
   *        Set of print preview browsers.
   * _currentPPBrowser
   *        References the current print preview browser that is being presented.
   */
  _ppBrowsers: new Set(),
  _currentPPBrowser: null,

  _enterPrintPreview() {
    // Send a message to the print preview browser to initialize
    // print preview. If we happen to have gotten a print preview
    // progress listener from nsIPrintingPromptService.showPrintProgressDialog
    // in printPreview, we add listeners to feed that progress
    // listener.
    let ppBrowser = this._shouldSimplify
      ? this._listener.getSimplifiedPrintPreviewBrowser()
      : this._listener.getPrintPreviewBrowser();
    this._ppBrowsers.add(ppBrowser);

    // If we're switching from 'normal' print preview to 'simplified' print
    // preview, we will want to run reader mode against the 'normal' print
    // preview browser's content:
    let oldPPBrowser = null;
    let changingPrintPreviewBrowsers = false;
    if (this._currentPPBrowser && ppBrowser != this._currentPPBrowser) {
      changingPrintPreviewBrowsers = true;
      oldPPBrowser = this._currentPPBrowser;
    }
    this._currentPPBrowser = ppBrowser;

    let waitForPrintProgressToEnableToolbar = false;
    if (this._webProgressPP.value) {
      waitForPrintProgressToEnableToolbar = true;
    }

    gPendingPrintPreviews.set(ppBrowser, waitForPrintProgressToEnableToolbar);

    // If we happen to have gotten simplify page checked, we will lazily
    // instantiate a new tab that parses the original page using ReaderMode
    // primitives. When it's ready, and in order to enter on preview, we send
    // over a message to print preview browser passing up the simplified tab as
    // reference. If not, we pass the original tab instead as content source.
    if (this._shouldSimplify) {
      let simplifiedBrowser = this._listener.getSimplifiedSourceBrowser();
      if (!simplifiedBrowser) {
        simplifiedBrowser = this._listener.createSimplifiedBrowser();

        // Here, we send down a message to simplified browser in order to parse
        // the original page. After we have parsed it, content will tell parent
        // that the document is ready for print previewing.
        simplifiedBrowser.sendMessageToActor(
          "Printing:Preview:ParseDocument",
          {
            URL: this._originalURL,
            windowID: oldPPBrowser.outerWindowID,
          },
          "Printing"
        );

        // Here we log telemetry data for when the user enters simplify mode.
        this.logTelemetry("PRINT_PREVIEW_SIMPLIFY_PAGE_OPENED_COUNT");

        return;
      }
    }

    this.sendEnterPrintPreviewToChild(
      ppBrowser,
      this._sourceBrowser,
      this._shouldSimplify,
      changingPrintPreviewBrowsers
    );
  },

  sendEnterPrintPreviewToChild(
    ppBrowser,
    sourceBrowser,
    simplifiedMode,
    changingBrowsers
  ) {
    ppBrowser.sendMessageToActor(
      "Printing:Preview:Enter",
      {
        browsingContextId: sourceBrowser.browsingContext.id,
        simplifiedMode,
        changingBrowsers,
        lastUsedPrinterName: this.getLastUsedPrinterName(),
      },
      "Printing"
    );
  },

  printPreviewEntered(ppBrowser, previewResult) {
    let waitForPrintProgressToEnableToolbar = gPendingPrintPreviews.get(
      ppBrowser
    );
    gPendingPrintPreviews.delete(ppBrowser);

    for (let { resolve, reject } of this._onEntered) {
      if (previewResult.failed) {
        reject();
      } else {
        resolve();
      }
    }

    this._onEntered = [];
    if (previewResult.failed) {
      // Something went wrong while putting the document into print preview
      // mode. Bail out.
      this._ppBrowsers.clear();
      this._listener.onEnter();
      this._listener.onExit();
      return;
    }

    // Stash the focused element so that we can return to it after exiting
    // print preview.
    gFocusedElement = document.commandDispatcher.focusedElement;

    let printPreviewTB = document.getElementById("print-preview-toolbar");
    if (printPreviewTB) {
      if (previewResult.changingBrowsers) {
        printPreviewTB.destroy();
        printPreviewTB.initialize(ppBrowser);
      } else {
        // printPreviewTB.initialize above already calls updateToolbar.
        printPreviewTB.updateToolbar();
      }

      // If we don't have a progress listener to enable the toolbar do it now.
      if (!waitForPrintProgressToEnableToolbar) {
        printPreviewTB.disableUpdateTriggers(false);
      }

      ppBrowser.collapsed = false;
      ppBrowser.focus();
      return;
    }

    // Set the original window as an active window so any mozPrintCallbacks can
    // run without delayed setTimeouts.
    if (this._listener.activateBrowser) {
      this._listener.activateBrowser(this._sourceBrowser);
    } else {
      this._sourceBrowser.docShellIsActive = true;
    }

    // show the toolbar after we go into print preview mode so
    // that we can initialize the toolbar with total num pages
    printPreviewTB = document.createXULElement("toolbar", {
      is: "printpreview-toolbar",
    });
    printPreviewTB.setAttribute("fullscreentoolbar", true);
    printPreviewTB.setAttribute("flex", "1");
    printPreviewTB.id = "print-preview-toolbar";

    let navToolbox = this._listener.getNavToolbox();
    navToolbox.parentNode.insertBefore(printPreviewTB, navToolbox);
    printPreviewTB.initialize(ppBrowser);

    // The print preview processing may not have fully completed, so if we
    // have a progress listener, disable the toolbar elements that can trigger
    // updates and it will enable them when completed.
    if (waitForPrintProgressToEnableToolbar) {
      printPreviewTB.disableUpdateTriggers(true);
    }

    // Enable simplify page checkbox when the page is an article
    if (this._sourceBrowser.isArticle) {
      printPreviewTB.enableSimplifyPage();
    } else {
      this.logTelemetry("PRINT_PREVIEW_SIMPLIFY_PAGE_UNAVAILABLE_COUNT");
      printPreviewTB.disableSimplifyPage();
    }

    // copy the window close handler
    if (window.onclose) {
      this._closeHandlerPP = window.onclose;
    } else {
      this._closeHandlerPP = null;
    }
    window.onclose = function() {
      PrintUtils.exitPrintPreview();
      return false;
    };

    // disable chrome shortcuts...
    window.addEventListener("keydown", this.onKeyDownPP, true);
    window.addEventListener("keypress", this.onKeyPressPP, true);

    ppBrowser.collapsed = false;
    ppBrowser.focus();
    // on Enter PP Call back
    this._listener.onEnter();
  },

  readerModeReady(sourceBrowser) {
    let ppBrowser = this._listener.getSimplifiedPrintPreviewBrowser();
    this.sendEnterPrintPreviewToChild(ppBrowser, sourceBrowser, true, true);
  },

  getLastUsedPrinterName() {
    let PSSVC = Cc["@mozilla.org/gfx/printsettings-service;1"].getService(
      Ci.nsIPrintSettingsService
    );
    let lastUsedPrinterName = PSSVC.lastUsedPrinterName;
    if (!lastUsedPrinterName) {
      // We "pass" print settings over to the content process by saving them to
      // prefs (yuck!). It is important to try to avoid saving to prefs without
      // prefixing them with a printer name though, so this hack tries to make
      // sure that (in the common case) we have set the "last used" printer,
      // which makes us save to prefs prefixed with its name, and makes sure
      // the content process will pick settings up from those prefixed prefs
      // too.
      let settings = this.getPrintSettings();
      if (settings.printerName) {
        PSSVC.savePrintSettingsToPrefs(
          settings,
          false,
          Ci.nsIPrintSettings.kInitSavePrinterName
        );
        PSSVC.savePrintSettingsToPrefs(
          settings,
          true,
          Ci.nsIPrintSettings.kInitSaveAll
        );
        lastUsedPrinterName = settings.printerName;
      }
    }

    return lastUsedPrinterName;
  },

  exitPrintPreview() {
    for (let browser of this._ppBrowsers) {
      browser.sendMessageToActor("Printing:Preview:Exit", {}, "Printing");
    }
    this._ppBrowsers.clear();
    this._currentPPBrowser = null;
    window.removeEventListener("keydown", this.onKeyDownPP, true);
    window.removeEventListener("keypress", this.onKeyPressPP, true);

    // restore the old close handler
    if (this._closeHandlerPP) {
      window.onclose = this._closeHandlerPP;
    } else {
      window.onclose = null;
    }
    this._closeHandlerPP = null;

    // remove the print preview toolbar
    let printPreviewTB = document.getElementById("print-preview-toolbar");
    printPreviewTB.destroy();
    printPreviewTB.remove();

    if (gFocusedElement) {
      Services.focus.setFocus(gFocusedElement, Services.focus.FLAG_NOSCROLL);
    } else {
      this._sourceBrowser.focus();
    }
    gFocusedElement = null;

    this.setSimplifiedMode(false);

    this.ensureProgressDialogClosed();

    this._listener.onExit();

    this._originalTitle = "";
    this._originalURL = "";
  },

  logTelemetry(ID) {
    let histogram = Services.telemetry.getHistogramById(ID);
    histogram.add(true);
  },

  _logKeyedTelemetry(id, key) {
    let histogram = Services.telemetry.getKeyedHistogramById(id);
    histogram.add(key);
  },

  onKeyDownPP(aEvent) {
    // Esc exits the PP
    if (aEvent.keyCode == aEvent.DOM_VK_ESCAPE) {
      PrintUtils.exitPrintPreview();
    }
  },

  onKeyPressPP(aEvent) {
    var closeKey;
    try {
      closeKey = document.getElementById("key_close").getAttribute("key");
      closeKey = aEvent["DOM_VK_" + closeKey];
    } catch (e) {}
    var isModif = aEvent.ctrlKey || aEvent.metaKey;
    // Ctrl-W exits the PP
    if (
      isModif &&
      (aEvent.charCode == closeKey || aEvent.charCode == closeKey + 32)
    ) {
      PrintUtils.exitPrintPreview();
    } else if (isModif) {
      var printPreviewTB = document.getElementById("print-preview-toolbar");
      var printKey = document
        .getElementById("printKb")
        .getAttribute("key")
        .toUpperCase();
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
  },

  /**
   * If there's a printing or print preview progress dialog displayed, force
   * it to close now.
   */
  ensureProgressDialogClosed() {
    if (this._webProgressPP && this._webProgressPP.value) {
      this._webProgressPP.value.onStateChange(
        null,
        null,
        Ci.nsIWebProgressListener.STATE_STOP,
        0
      );
    }
  },
};
