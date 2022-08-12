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
  "SHOW_PAGE_SETUP_MENU",
  "print.show_page_setup_menu",
  false
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "PRINT_ALWAYS_SILENT",
  "print.always_print_silent",
  false
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "PREFER_SYSTEM_DIALOG",
  "print.prefer_system_dialog",
  false
);

ChromeUtils.defineModuleGetter(
  this,
  "PromptUtils",
  "resource://gre/modules/SharedPromptUtils.jsm"
);

var PrintUtils = {
  SAVE_TO_PDF_PRINTER: "Mozilla Save to PDF",

  get _bundle() {
    delete this._bundle;
    return (this._bundle = Services.strings.createBundle(
      "chrome://global/locale/printing.properties"
    ));
  },

  async checkForSelection(browsingContext) {
    try {
      let sourceActor = browsingContext.currentWindowGlobal.getActor(
        "PrintingSelection"
      );
      // Need the await for the try to trigger...
      return await sourceActor.sendQuery("PrintingSelection:HasSelection", {});
    } catch (e) {
      Cu.reportError(e);
    }
    return false;
  },

  /**
   * Updates the hidden state of the "Page Setup" menu items in the File menu,
   * depending on the value of the `print.show_page_setup_menu` pref.
   * Note: not all platforms have a "Page Setup" menu item (or Page Setup
   * window).
   */
  updatePrintSetupMenuHiddenState() {
    let pageSetupMenuItem = document.getElementById("menu_printSetup");
    if (pageSetupMenuItem) {
      pageSetupMenuItem.hidden = !SHOW_PAGE_SETUP_MENU;
    }
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
      var PRINTDIALOGSVC = Cc[
        "@mozilla.org/widget/printdialog-service;1"
      ].getService(Ci.nsIPrintDialogService);
      PRINTDIALOGSVC.showPageSetupDialog(window, printSettings, null);
    } catch (e) {
      dump("showPageSetup " + e + "\n");
      return false;
    }
    return true;
  },

  /**
   * This call exists in a separate method so it can be easily overridden where
   * `gBrowser` doesn't exist (e.g. Thunderbird).
   *
   * @see getTabDialogBox in tabbrowser.js
   */
  getTabDialogBox(sourceBrowser) {
    return gBrowser.getTabDialogBox(sourceBrowser);
  },

  getPreviewBrowser(sourceBrowser) {
    let dialogBox = this.getTabDialogBox(sourceBrowser);
    for (let dialog of dialogBox.getTabDialogManager()._dialogs) {
      let browser = dialog._box.querySelector(".printPreviewBrowser");
      if (browser) {
        return browser;
      }
    }
    return null;
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
  _openTabModalPrint(
    aBrowsingContext,
    aOpenWindowInfo,
    aPrintInitiationTime,
    aPrintSelectionOnly,
    aPrintFrameOnly
  ) {
    let sourceBrowser = aBrowsingContext.top.embedderElement;
    let previewBrowser = this.getPreviewBrowser(sourceBrowser);
    if (previewBrowser) {
      // Don't open another dialog if we're already printing.
      //
      // XXX This can be racy can't it? getPreviewBrowser looks at browser that
      // we set up after opening the dialog. But I guess worst case we just
      // open two dialogs so...
      Cu.reportError("Tab-modal print UI already open");
      return null;
    }

    // Create the print preview dialog.
    let args = PromptUtils.objectToPropBag({
      printSelectionOnly: !!aPrintSelectionOnly,
      isArticle: sourceBrowser.isArticle,
      printFrameOnly: !!aPrintFrameOnly,
    });
    let dialogBox = this.getTabDialogBox(sourceBrowser);
    let { closedPromise, dialog } = dialogBox.open(
      `chrome://global/content/print.html?printInitiationTime=${aPrintInitiationTime}`,
      { features: "resizable=no", sizeTo: "available" },
      args
    );
    closedPromise.catch(e => {
      Cu.reportError(e);
    });

    let settingsBrowser = dialog._frame;
    let printPreview = new PrintPreview({
      sourceBrowsingContext: aBrowsingContext,
      settingsBrowser,
      topBrowsingContext: aBrowsingContext.top,
      activeBrowsingContext: aBrowsingContext,
      openWindowInfo: aOpenWindowInfo,
      printFrameOnly: aPrintFrameOnly,
    });
    // This will create the source browser in connectedCallback() if we sent
    // openWindowInfo. Otherwise the browser will be null.
    settingsBrowser.parentElement.insertBefore(printPreview, settingsBrowser);
    return printPreview.sourceBrowser;
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
   *        {windowDotPrintOpenWindowInfo}
   *                              Non-null if this call comes from window.print().
   *                              This is the nsIOpenWindowInfo object that has to
   *                              be passed down to createBrowser in order for the
   *                              static clone that has been cretaed in the child
   *                              process to be linked to the browser it creates
   *                              in the parent process.
   *        {printSelectionOnly}  Whether to print only the active selection of
   *                              the given browsing context.
   *        {printFrameOnly}      Whether to print the selected frame.
   */
  startPrintWindow(aBrowsingContext, aOptions) {
    const printInitiationTime = Date.now();

    // At most, one of these is set.
    let { printSelectionOnly, printFrameOnly, windowDotPrintOpenWindowInfo } =
      aOptions || {};

    if (
      windowDotPrintOpenWindowInfo &&
      !windowDotPrintOpenWindowInfo.isForWindowDotPrint
    ) {
      throw new Error("Only expect openWindowInfo for window.print()");
    }

    let browsingContext = aBrowsingContext;
    if (printSelectionOnly) {
      // Ensure that we use the window with focus/selection if the context menu
      // (from which 'Print selection' was selected) happens to have been opened
      // over a different frame.
      let focusedBc = Services.focus.focusedContentBrowsingContext;
      if (
        focusedBc &&
        focusedBc.top.embedderElement == browsingContext.top.embedderElement
      ) {
        browsingContext = focusedBc;
      }
    }

    if (!PRINT_ALWAYS_SILENT && !PREFER_SYSTEM_DIALOG) {
      return this._openTabModalPrint(
        browsingContext,
        windowDotPrintOpenWindowInfo,
        printInitiationTime,
        printSelectionOnly,
        printFrameOnly
      );
    }

    const useSystemDialog = PREFER_SYSTEM_DIALOG && !PRINT_ALWAYS_SILENT;

    let browser = null;
    if (windowDotPrintOpenWindowInfo) {
      // When we're called by handleStaticCloneCreatedForPrint(), we must
      // return this browser.
      browser = this.createParentBrowserForStaticClone(
        browsingContext,
        windowDotPrintOpenWindowInfo
      );
      browsingContext = browser.browsingContext;
    }

    // This code is wrapped in an async function so that we can await the async
    // functions that it calls.
    async function makePrintSettingsAndInvokePrint() {
      let settings = PrintUtils.getPrintSettings(
        /*aPrinterName*/ "",
        /*aDefaultsOnly*/ false,
        /*aAllowPseudoPrinter*/ !useSystemDialog
      );
      settings.printSelectionOnly = printSelectionOnly;
      if (
        settings.outputDestination ==
          Ci.nsIPrintSettings.kOutputDestinationFile &&
        !settings.toFileName
      ) {
        // TODO(bug 1748004): We should consider generating the file name
        // from the document's title as we do in print.js's pickFileName
        // (including using DownloadPaths.sanitize!).
        // For now, the following is for consistency with the behavior
        // prior to bug 1669149 part 3.
        let dest = undefined;
        try {
          dest = Services.dirsvc.get("CurWorkD", Ci.nsIFile).path;
        } catch (e) {}
        if (!dest) {
          dest = Services.dirsvc.get("Home", Ci.nsIFile).path;
        }
        settings.toFileName = PathUtils.join(dest, "mozilla.pdf");
      }

      if (useSystemDialog) {
        const hasSelection = await PrintUtils.checkForSelection(
          browsingContext
        );

        // Prompt the user to choose a printer and make any desired print
        // settings changes.
        try {
          await Cc["@mozilla.org/widget/printdialog-service;1"]
            .getService(Ci.nsIPrintDialogService)
            .showPrintDialog(
              browsingContext.topChromeWindow,
              hasSelection,
              settings
            );
        } catch (e) {
          if (browser) {
            browser.remove(); // don't leak this
          }
          if (e.result == Cr.NS_ERROR_ABORT) {
            return; // user cancelled
          }
          throw e;
        }

        // Update the saved last used printer name and print settings:
        Services.prefs.setStringPref("print_printer", settings.printerName);
        var PSSVC = Cc["@mozilla.org/gfx/printsettings-service;1"].getService(
          Ci.nsIPrintSettingsService
        );
        PSSVC.savePrintSettingsToPrefs(
          settings,
          true,
          Ci.nsIPrintSettings.kInitSaveAll
        );
      }

      // At some point we should handle the Promise that this returns (at
      // least report rejection to telemetry).
      browsingContext.print(settings);
    }

    // We need to return to the event loop before calling
    // makePrintSettingsAndInvokePrint() if we were called for `window.print()`.
    // That's because if that function synchronously calls `browser.remove()`
    // or `browsingContext.print()` before we return `browser`, the nested
    // event loop that is being spun in the content process under the
    // OpenInternal call in nsGlobalWindowOuter::Print will still be active.
    // In the case of `browser.remove()`, nsGlobalWindowOuter::Print would then
    // get unhappy once OpenInternal does return since it will fail to return
    // a BrowsingContext. In the case of `browsingContext.print()`, we would
    // re-enter nsGlobalWindowOuter::Print under the nested event loop and
    // printing would then fail since the outer nsGlobalWindowOuter::Print call
    // wouldn't yet have created the static clone.
    setTimeout(makePrintSettingsAndInvokePrint, 0);

    return browser;
  },

  togglePrintPreview(aBrowsingContext) {
    let dialogBox = this.getTabDialogBox(aBrowsingContext.top.embedderElement);
    let dialogs = dialogBox.getTabDialogManager().dialogs;
    let previewDialog = dialogs.find(d =>
      d._box.querySelector(".printSettingsBrowser")
    );
    if (previewDialog) {
      previewDialog.close();
      return;
    }
    this.startPrintWindow(aBrowsingContext);
  },

  /**
   * Called when a content process has created a new BrowsingContext for a
   * static clone of a document that is to be printed, but we do NOT yet have a
   * CanonicalBrowsingContext counterpart in the parent process. This only
   * happens in the following cases:
   *
   *   - content script invoked window.print() in the content process, or:
   *   - silent printing is enabled, and UI code previously invoked
   *     startPrintWindow which called BrowsingContext.print(), and we're now
   *     being called back by the content process to parent the static clone.
   *
   * In the latter case we only need to create the CanonicalBrowsingContext,
   * link it to it's content process counterpart, and inserted it into
   * the document tree; the print in the content process has already been
   * initiated.
   *
   * In the former case we additionally need to check if we should open the
   * tab modal print UI (if not silent printing), obtain a valid
   * nsIPrintSettings object, and tell the content process to initiate the
   * print with this settings object.
   */
  handleStaticCloneCreatedForPrint(aOpenWindowInfo) {
    let browsingContext = aOpenWindowInfo.parent;
    if (aOpenWindowInfo.isForWindowDotPrint) {
      return this.startPrintWindow(browsingContext, {
        windowDotPrintOpenWindowInfo: aOpenWindowInfo,
      });
    }
    return this.createParentBrowserForStaticClone(
      browsingContext,
      aOpenWindowInfo
    );
  },

  createParentBrowserForStaticClone(aBrowsingContext, aOpenWindowInfo) {
    // XXX This code is only called when silent printing, so we're really
    // abusing PrintPreview here. See bug 1768020.
    let printPreview = new PrintPreview({
      sourceBrowsingContext: aBrowsingContext,
      openWindowInfo: aOpenWindowInfo,
    });
    let browser = printPreview.createPreviewBrowser("source");
    document.documentElement.append(browser);
    return browser;
  },

  // "private" methods and members. Don't use them.

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

  getPrintSettings(aPrinterName, aDefaultsOnly, aAllowPseudoPrinter = true) {
    var printSettings;
    try {
      var PSSVC = Cc["@mozilla.org/gfx/printsettings-service;1"].getService(
        Ci.nsIPrintSettingsService
      );

      function isValidPrinterName(aPrinterName) {
        return (
          aPrinterName &&
          (aAllowPseudoPrinter ||
            aPrinterName != PrintUtils.SAVE_TO_PDF_PRINTER)
        );
      }

      // We must not try to print using an nsIPrintSettings without a printer
      // name set.
      const printerName = (function() {
        if (isValidPrinterName(aPrinterName)) {
          return aPrinterName;
        }
        if (isValidPrinterName(PSSVC.lastUsedPrinterName)) {
          return PSSVC.lastUsedPrinterName;
        }
        return Cc["@mozilla.org/gfx/printerlist;1"].getService(
          Ci.nsIPrinterList
        ).systemDefaultPrinterName;
      })();

      printSettings = PSSVC.createNewPrintSettings();
      printSettings.printerName = printerName;

      // First get any defaults from the printer. We want to skip this for Save
      // to PDF since it isn't a real printer and will throw.
      if (printSettings.printerName != this.SAVE_TO_PDF_PRINTER) {
        PSSVC.initPrintSettingsFromPrinter(
          printSettings.printerName,
          printSettings
        );
      }

      if (!aDefaultsOnly) {
        // Apply any settings that have been saved for this printer.
        PSSVC.initPrintSettingsFromPrefs(
          printSettings,
          true,
          printSettings.kInitSaveAll
        );
      }
    } catch (e) {
      Cu.reportError("PrintUtils.getPrintSettings failed: " + e + "\n");
    }
    return printSettings;
  },
};

class PrintPreview extends MozElements.BaseControl {
  constructor({
    sourceBrowsingContext,
    settingsBrowser,
    topBrowsingContext,
    activeBrowsingContext,
    openWindowInfo,
    printFrameOnly,
  }) {
    super();
    this.sourceBrowsingContext = sourceBrowsingContext;
    this.settingsBrowser = settingsBrowser;
    this.topBrowsingContext = topBrowsingContext;
    this.activeBrowsingContext = activeBrowsingContext;
    this.openWindowInfo = openWindowInfo;
    this.printFrameOnly = printFrameOnly;

    this.printSelectionOnly = false;
    this.simplifyPage = false;
    this.sourceBrowser = null;
    this.selectionBrowser = null;
    this.simplifiedBrowser = null;
    this.lastPreviewBrowser = null;
  }

  connectedCallback() {
    if (this.childElementCount > 0) {
      return;
    }
    this.setAttribute("flex", "1");
    this.append(
      MozXULElement.parseXULToFragment(`
        <stack class="previewStack" rendering="true" flex="1" previewtype="primary">
          <vbox class="previewRendering" flex="1">
            <h1 class="print-pending-label" data-l10n-id="printui-loading"></h1>
          </vbox>
          <html:printpreview-pagination class="printPreviewNavigation"></html:printpreview-pagination>
        </stack>
    `)
    );
    this.stack = this.firstElementChild;
    this.paginator = this.querySelector("printpreview-pagination");

    if (this.openWindowInfo) {
      // For window.print() we need a browser right away for the contents to be
      // cloned into, create it now.
      this.createPreviewBrowser("source");
    }
  }

  disconnectedCallback() {
    this.exitPrintPreview();
  }

  getSourceBrowsingContext() {
    if (this.openWindowInfo) {
      // If openWindowInfo is set this was for window.print() and the source
      // contents have already been cloned into the preview browser.
      return this.sourceBrowser.browsingContext;
    }
    return this.sourceBrowsingContext;
  }

  get currentBrowsingContext() {
    return this.lastPreviewBrowser.browsingContext;
  }

  exitPrintPreview() {
    this.sourceBrowser?.frameLoader?.exitPrintPreview();
    this.simplifiedBrowser?.frameLoader?.exitPrintPreview();
    this.selectionBrowser?.frameLoader?.exitPrintPreview();

    this.textContent = "";
  }

  async printPreview(settings, { sourceVersion, sourceURI }) {
    this.stack.setAttribute("rendering", true);

    let result = await this._printPreview(settings, {
      sourceVersion,
      sourceURI,
    });

    let browser = this.lastPreviewBrowser;
    this.stack.setAttribute("previewtype", browser.getAttribute("previewtype"));
    browser.setAttribute("sheet-count", result.sheetCount);
    // The view resets to the top of the document on update bug 1686737.
    browser.setAttribute("current-page", 1);
    this.paginator.observePreviewBrowser(browser);
    await document.l10n.translateElements([browser]);

    this.stack.removeAttribute("rendering");

    return result;
  }

  async _printPreview(settings, { sourceVersion, sourceURI }) {
    let printSelectionOnly = sourceVersion == "selection";
    let simplifyPage = sourceVersion == "simplified";
    let selectionTypeBrowser;
    let previewBrowser;

    // Select the existing preview browser elements, these could be null.
    if (printSelectionOnly) {
      selectionTypeBrowser = this.selectionBrowser;
      previewBrowser = this.selectionBrowser;
    } else {
      selectionTypeBrowser = this.sourceBrowser;
      previewBrowser = simplifyPage
        ? this.simplifiedBrowser
        : this.sourceBrowser;
    }

    settings.docURL = sourceURI;

    if (previewBrowser) {
      this.lastPreviewBrowser = previewBrowser;
      if (this.openWindowInfo) {
        // We only want to use openWindowInfo for the window.print() browser,
        // we can get rid of it now.
        this.openWindowInfo = null;
      }
      // This browser has been rendered already, just update it.
      return previewBrowser.frameLoader.printPreview(settings, null);
    }

    if (!selectionTypeBrowser) {
      // Need to create a non-simplified browser.
      selectionTypeBrowser = this.createPreviewBrowser(
        simplifyPage ? "source" : sourceVersion
      );
      let browsingContext =
        printSelectionOnly || this.printFrameOnly
          ? this.activeBrowsingContext
          : this.topBrowsingContext;
      let result = await selectionTypeBrowser.frameLoader.printPreview(
        settings,
        browsingContext
      );
      // If this isn't simplified then we're done.
      if (!simplifyPage) {
        this.lastPreviewBrowser = selectionTypeBrowser;
        return result;
      }
    }

    // We have the base selection/primary browser but need to simplify.
    previewBrowser = this.createPreviewBrowser(sourceVersion);
    await previewBrowser.browsingContext.currentWindowGlobal
      .getActor("Printing")
      .sendQuery("Printing:Preview:ParseDocument", {
        URL: sourceURI,
        windowID:
          selectionTypeBrowser.browsingContext.currentWindowGlobal
            .outerWindowId,
      });

    // We've parsed a simplified version into the preview browser. Convert that to
    // a print preview as usual.
    this.lastPreviewBrowser = previewBrowser;
    return previewBrowser.frameLoader.printPreview(
      settings,
      previewBrowser.browsingContext
    );
  }

  createPreviewBrowser(sourceVersion) {
    let browser = document.createXULElement("browser");
    let browsingContext =
      sourceVersion == "selection" ||
      this.printFrameOnly ||
      (sourceVersion == "source" && this.openWindowInfo)
        ? this.sourceBrowsingContext
        : this.sourceBrowsingContext.top;
    if (sourceVersion == "source" && this.openWindowInfo) {
      browser.openWindowInfo = this.openWindowInfo;
    } else {
      let userContextId = browsingContext.originAttributes.userContextId;
      if (userContextId) {
        browser.setAttribute("usercontextid", userContextId);
      }
      browser.setAttribute(
        "initialBrowsingContextGroupId",
        browsingContext.group.id
      );
    }
    browser.setAttribute("type", "content");
    let remoteType = browsingContext.currentRemoteType;
    if (remoteType) {
      browser.setAttribute("remoteType", remoteType);
      browser.setAttribute("remote", "true");
    }
    // When the print process finishes, we get closed by
    // nsDocumentViewer::OnDonePrinting, or by the print preview code.
    //
    // When that happens, we should remove us from the DOM if connected.
    browser.addEventListener("DOMWindowClose", function(e) {
      if (this.isConnected) {
        this.remove();
      }
      e.stopPropagation();
      e.preventDefault();
    });

    if (this.settingsBrowser) {
      browser.addEventListener("contextmenu", function(e) {
        e.preventDefault();
      });

      browser.setAttribute("previewtype", sourceVersion);
      browser.classList.add("printPreviewBrowser");
      browser.setAttribute("flex", "1");
      browser.setAttribute("printpreview", "true");
      browser.setAttribute("nodefaultsrc", "true");
      document.l10n.setAttributes(browser, "printui-preview-label");

      this.stack.insertBefore(browser, this.paginator);

      if (sourceVersion == "source") {
        this.sourceBrowser = browser;
      } else if (sourceVersion == "selection") {
        this.selectionBrowser = browser;
      } else if (sourceVersion == "simplified") {
        this.simplifiedBrowser = browser;
      }
    } else {
      browser.style.visibility = "collapse";
    }
    return browser;
  }
}
customElements.define("print-preview", PrintPreview);
