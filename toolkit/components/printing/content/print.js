/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {
  gBrowser,
  PrintUtils,
  Services,
  AppConstants,
} = window.docShell.chromeEventHandler.ownerGlobal;

ChromeUtils.defineModuleGetter(
  this,
  "DownloadPaths",
  "resource://gre/modules/DownloadPaths.jsm"
);

const INPUT_DELAY_MS = 500;
const MM_PER_POINT = 25.4 / 72;
const INCHES_PER_POINT = 1 / 72;
const ourBrowser = window.docShell.chromeEventHandler;

document.addEventListener(
  "DOMContentLoaded",
  e => {
    PrintEventHandler.init();
    ourBrowser.setAttribute("flex", "0");
    ourBrowser.classList.add("printSettingsBrowser");
    ourBrowser.closest(".dialogBox")?.classList.add("printDialogBox");
  },
  { once: true }
);

window.addEventListener("dialogclosing", () => {
  PrintEventHandler.unload();
});

window.addEventListener(
  "unload",
  e => {
    document.textContent = "";
  },
  { once: true }
);

var PrintEventHandler = {
  settings: null,
  defaultSettings: null,
  _printerSettingsChangedFlags: 0,
  _nonFlaggedChangedSettings: {},
  settingFlags: {
    margins: Ci.nsIPrintSettings.kInitSaveMargins,
    orientation: Ci.nsIPrintSettings.kInitSaveOrientation,
    paperName:
      Ci.nsIPrintSettings.kInitSavePaperSize |
      Ci.nsIPrintSettings.kInitSaveUnwriteableMargins,
    printInColor: Ci.nsIPrintSettings.kInitSaveInColor,
    printerName: Ci.nsIPrintSettings.kInitSavePrinterName,
    scaling: Ci.nsIPrintSettings.kInitSaveScaling,
    shrinkToFit: Ci.nsIPrintSettings.kInitSaveShrinkToFit,
    printFootersHeaders:
      Ci.nsIPrintSettings.kInitSaveHeaderLeft |
      Ci.nsIPrintSettings.kInitSaveHeaderCenter |
      Ci.nsIPrintSettings.kInitSaveHeaderRight |
      Ci.nsIPrintSettings.kInitSaveFooterLeft |
      Ci.nsIPrintSettings.kInitSaveFooterCenter |
      Ci.nsIPrintSettings.kInitSaveFooterRight,
    printBackgrounds:
      Ci.nsIPrintSettings.kInitSaveBGColors |
      Ci.nsIPrintSettings.kInitSaveBGImages,
  },
  originalSourceContentTitle: null,
  originalSourceCurrentURI: null,
  previewBrowser: null,

  // These settings do not have an associated pref value or flag, but
  // changing them requires us to update the print preview.
  _nonFlaggedUpdatePreviewSettings: [
    "printAllOrCustomRange",
    "startPageRange",
    "endPageRange",
  ],

  async init() {
    Services.telemetry.scalarAdd("printing.preview_opened_tm", 1);

    // Do not keep a reference to source browser, it may mutate after printing
    // is initiated and the print preview clone must be a snapshot from the
    // time that the print was started.
    let sourceBrowsingContext = this.getSourceBrowsingContext();
    this.previewBrowser = this._createPreviewBrowser(sourceBrowsingContext);

    // Get the temporary browser that will previously have been created for the
    // platform code to generate the static clone printing doc into if this
    // print is for a window.print() call.  In that case we steal the browser's
    // docshell to get the static clone, then discard it.
    let existingBrowser = window.arguments[0].getProperty("previewBrowser");
    if (existingBrowser) {
      sourceBrowsingContext = existingBrowser.browsingContext;
      this.previewBrowser.swapDocShells(existingBrowser);
      existingBrowser.remove();
    } else {
      this.previewBrowser.loadURI("about:printpreview", {
        triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      });
    }

    this.originalSourceContentTitle =
      sourceBrowsingContext.currentWindowContext.documentTitle;
    this.originalSourceCurrentURI =
      sourceBrowsingContext.currentWindowContext.documentURI.spec;

    // Let the dialog appear before doing any potential main thread work.
    await ourBrowser._dialogReady;

    // First check the available destinations to ensure we get settings for an
    // accessible printer.
    let {
      destinations,
      defaultSystemPrinter,
      fallbackPaperList,
      selectedPrinter,
      printersByName,
    } = await this.getPrintDestinations();
    PrintSettingsViewProxy.availablePrinters = printersByName;
    PrintSettingsViewProxy.fallbackPaperList = fallbackPaperList;
    PrintSettingsViewProxy.defaultSystemPrinter = defaultSystemPrinter;

    document.addEventListener("print", e => this.print());
    document.addEventListener("update-print-settings", e =>
      this.updateSettings(e.detail)
    );
    document.addEventListener("cancel-print", () => this.cancelPrint());
    document.addEventListener("open-system-dialog", () => {
      // This file in only used if pref print.always_print_silent is false, so
      // no need to check that here.

      // Use our settings to prepopulate the system dialog.
      // The system print dialog won't recognize our internal save-to-pdf
      // pseudo-printer.  We need to pass it a settings object from any
      // system recognized printer.
      let settings =
        this.settings.printerName == PrintUtils.SAVE_TO_PDF_PRINTER
          ? PrintUtils.getPrintSettings(this.viewSettings.defaultSystemPrinter)
          : this.settings.clone();
      const PRINTPROMPTSVC = Cc[
        "@mozilla.org/embedcomp/printingprompt-service;1"
      ].getService(Ci.nsIPrintingPromptService);
      try {
        Services.telemetry.scalarAdd(
          "printing.dialog_opened_via_preview_tm",
          1
        );
        PRINTPROMPTSVC.showPrintDialog(window, settings);
      } catch (e) {
        if (e.result == Cr.NS_ERROR_ABORT) {
          Services.telemetry.scalarAdd(
            "printing.dialog_via_preview_cancelled_tm",
            1
          );
          return; // user cancelled
        }
        throw e;
      }
      this.print(settings);
    });

    await this.refreshSettings(selectedPrinter.value);
    this.updatePrintPreview(sourceBrowsingContext);

    document.dispatchEvent(
      new CustomEvent("available-destinations", {
        detail: destinations,
      })
    );

    document.dispatchEvent(
      new CustomEvent("print-settings", {
        detail: this.viewSettings,
      })
    );

    await document.l10n.translateElements([this.previewBrowser]);

    document.body.removeAttribute("loading");

    window.requestAnimationFrame(() => {
      window.focus();
      // Now that we're showing the form, select the destination select.
      document.getElementById("printer-picker").focus();
    });
  },

  unload() {
    this.previewBrowser.frameLoader.exitPrintPreview();
  },

  _createPreviewBrowser(sourceBrowsingContext) {
    // Create a preview browser.
    let printPreviewBrowser = gBrowser.createBrowser({
      remoteType: sourceBrowsingContext.currentRemoteType,
      userContextId: sourceBrowsingContext.originAttributes.userContextId,
      initialBrowsingContextGroupId: sourceBrowsingContext.group.id,
      skipLoad: false,
    });
    printPreviewBrowser.classList.add("printPreviewBrowser");
    printPreviewBrowser.setAttribute("flex", "1");
    printPreviewBrowser.setAttribute("printpreview", "true");
    // Disable the context menu for this browser. This is set as an attribute
    // on the browser instead of using addEventListener since the latter
    // was causing memory leaks.
    printPreviewBrowser.setAttribute("oncontextmenu", "return false;");
    document.l10n.setAttributes(printPreviewBrowser, "printui-preview-label");

    // Create the stack for the loading indicator.
    let doc = ourBrowser.ownerDocument;
    let previewStack = doc.importNode(
      doc.getElementById("printPreviewStackTemplate").content,
      true
    ).firstElementChild;

    previewStack.append(printPreviewBrowser);
    ourBrowser.parentElement.prepend(previewStack);

    return printPreviewBrowser;
  },

  async refreshSettings(printerName) {
    let currentPrinter = await PrintSettingsViewProxy.resolvePropertiesForPrinter(
      printerName
    );
    this.settings = currentPrinter.settings;
    this.defaultSettings = currentPrinter.defaultSettings;
    // restore settings which do not have a corresponding flag
    for (let key of Object.keys(this._nonFlaggedChangedSettings)) {
      if (key in this.settings) {
        this.settings[key] = this._nonFlaggedChangedSettings[key];
      }
    }

    // Some settings are only used by the UI
    // assigning new values should update the underlying settings
    this.viewSettings = new Proxy(this.settings, PrintSettingsViewProxy);

    // Ensure the output format is set properly
    this.viewSettings.printerName = printerName;

    // Ensure the color option is correct, if either of the supportsX flags are
    // false then the user cannot change the value through the UI.
    let flags = 0;
    if (!this.viewSettings.supportsColor) {
      flags |= this.settingFlags.printInColor;
      this.viewSettings.printInColor = false;
    } else if (!this.viewSettings.supportsMonochrome) {
      flags |= this.settingFlags.printInColor;
      this.viewSettings.printInColor = true;
    }

    // See if the paperName needs to change
    let paperName = this.viewSettings.paperName;
    let matchedPaper = PrintSettingsViewProxy.getBestPaperMatch(
      paperName,
      this.viewSettings.paperWidth,
      this.viewSettings.paperHeight,
      this.viewSettings.paperSizeUnit
    );
    if (!matchedPaper) {
      // We didn't find a good match. Take the first paper size, but clear the
      // global flag for carrying the paper size over.
      paperName = Object.keys(PrintSettingsViewProxy.availablePaperSizes)[0];
      this._printerSettingsChangedFlags ^= this.settingFlags.paperName;
    } else if (matchedPaper.name !== paperName) {
      // The exact paper name doesn't exist for this printer, update it
      flags |= this.settingFlags.paperName;
      paperName = matchedPaper.name;
      console.log(
        `Initial settings.paperName: "${this.viewSettings.paperName}" missing, using: ${paperName} instead`
      );
    }
    // Compute and cache the margins for the current paper size
    await PrintSettingsViewProxy.fetchPaperMargins(paperName);
    this.viewSettings.paperName = paperName;

    if (flags) {
      this.saveSettingsToPrefs(flags);
    }
  },

  async print(systemDialogSettings) {
    let settings = systemDialogSettings || this.settings;

    if (settings.printerName == PrintUtils.SAVE_TO_PDF_PRINTER) {
      try {
        settings.toFileName = await pickFileName(
          this.originalSourceContentTitle,
          this.originalSourceCurrentURI
        );
      } catch (e) {
        // Don't care why just yet.
        return;
      }
    }

    // This seems like it should be handled automatically but it isn't.
    Services.prefs.setStringPref("print_printer", settings.printerName);

    PrintUtils.printWindow(this.previewBrowser.browsingContext, settings);
  },

  cancelPrint() {
    Services.telemetry.scalarAdd("printing.preview_cancelled_tm", 1);
    window.close();
  },

  async updateSettings(changedSettings = {}) {
    let didSettingsChange = false;
    let updatePreviewWithoutFlag = false;
    let flags = 0;

    if (changedSettings.paperName) {
      // The paper's margin properties are async,
      // so resolve those now before we update the settings
      await PrintSettingsViewProxy.fetchPaperMargins(changedSettings.paperName);
    }

    for (let [setting, value] of Object.entries(changedSettings)) {
      if (this.viewSettings[setting] != value) {
        this.viewSettings[setting] = value;

        if (setting in this.settingFlags) {
          flags |= this.settingFlags[setting];
        } else {
          // some settings have no corresponding flag,
          // but we may want to restore them if the current printer changes
          this._nonFlaggedChangedSettings[setting] = value;
        }
        didSettingsChange = true;
        updatePreviewWithoutFlag |= this._nonFlaggedUpdatePreviewSettings.includes(
          setting
        );

        Services.telemetry.keyedScalarAdd(
          "printing.settings_changed",
          setting,
          1
        );
      }
    }

    let printerChanged = flags & this.settingFlags.printerName;
    if (didSettingsChange) {
      this._printerSettingsChangedFlags |= flags;

      if (printerChanged) {
        // If the user has changed settings with the old printer, stash them all
        // so they can be restored on top of the new printer's settings
        flags |= this._printerSettingsChangedFlags;
      }

      if (flags) {
        this.saveSettingsToPrefs(flags);
      }
      if (printerChanged) {
        await this.refreshSettings(this.settings.printerName);
      }
      if (flags || printerChanged || updatePreviewWithoutFlag) {
        this.updatePrintPreview();
      }

      document.dispatchEvent(
        new CustomEvent("print-settings", {
          detail: this.viewSettings,
        })
      );
    }
  },

  saveSettingsToPrefs(flags) {
    let PSSVC = Cc["@mozilla.org/gfx/printsettings-service;1"].getService(
      Ci.nsIPrintSettingsService
    );
    PSSVC.savePrintSettingsToPrefs(this.settings, true, flags);
  },

  /**
   * Prepare the print preview. A browsingContext must be provided on the
   * first call to initialize the preview, if no browsingContext is provided
   * then this.previewBrowser.browsingContext will be used.
   *
   * @param browsingContext {BrowsingContext} (optional)
   *        The BrowsingContext to initialize the preview from.
   */
  async updatePrintPreview(browsingContext) {
    if (this._previewUpdatingPromise) {
      if (!this._queuedPreviewUpdatePromise) {
        this._queuedPreviewUpdatePromise = this._previewUpdatingPromise.then(
          () => this._updatePrintPreview(browsingContext)
        );
      }
      // else there's already an update queued.
    } else {
      this._previewUpdatingPromise = this._updatePrintPreview(browsingContext);
    }
  },

  /**
   * Create a print preview for the provided source browsingContext, or refresh
   * the preview with new settings when omitted.
   *
   * @param sourceBrowsingContext {BrowsingContext} [optional]
   *        The source BrowsingContext (the one associated with a tab or
   *        subdocument) that should be previewed.
   *
   * @return {Promise} Resolves when the preview has been updated.
   */
  async _updatePrintPreview(sourceBrowsingContext) {
    let { previewBrowser, settings } = this;

    // We never want the progress dialog to show
    settings.showPrintProgress = false;

    let stack = previewBrowser.parentElement;
    stack.setAttribute("rendering", true);
    document.body.setAttribute("rendering", true);

    let sourceWinId;
    if (sourceBrowsingContext) {
      sourceWinId = sourceBrowsingContext.currentWindowGlobal.outerWindowId;
    }
    // This resolves with a PrintPreviewSuccessInfo dictionary.  That also has
    // a `sheetCount` property available which we should use (bug 1662331).
    let {
      totalPageCount,
      hasSelection,
    } = await previewBrowser.frameLoader.printPreview(settings, sourceWinId);

    if (this._queuedPreviewUpdatePromise) {
      // Now that we're done, the queued update (if there is one) will start.
      this._previewUpdatingPromise = this._queuedPreviewUpdatePromise;
      this._queuedPreviewUpdatePromise = null;
    } else {
      // No other update queued, send the page count and show the preview.
      let numPages = totalPageCount;
      // Adjust number of pages if the user specifies the pages they want printed
      if (settings.printRange == Ci.nsIPrintSettings.kRangeSpecifiedPageRange) {
        numPages = settings.endPageRange - settings.startPageRange + 1;
      }
      // Update the settings print options on whether there is a selection.
      settings.SetPrintOptions(
        Ci.nsIPrintSettings.kEnableSelectionRB,
        hasSelection
      );
      document.dispatchEvent(
        new CustomEvent("page-count", {
          detail: { numPages, totalPages: totalPageCount },
        })
      );

      stack.removeAttribute("rendering");
      document.body.removeAttribute("rendering");
      this._previewUpdatingPromise = null;
    }
  },

  getSourceBrowsingContext() {
    let params = new URLSearchParams(location.search);
    let browsingContextId = params.get("browsingContextId");
    if (!browsingContextId) {
      return null;
    }
    return BrowsingContext.get(browsingContextId);
  },

  async getPrintDestinations() {
    const printerList = Cc["@mozilla.org/gfx/printerlist;1"].createInstance(
      Ci.nsIPrinterList
    );
    let printers;

    if (Cu.isInAutomation) {
      printers = [];
    } else {
      printers = await printerList.printers;
    }

    const fallbackPaperList = await printerList.fallbackPaperList;
    const lastUsedPrinterName = PrintUtils._getLastUsedPrinterName();
    const defaultPrinterName = printerList.systemDefaultPrinterName;
    const printersByName = {};

    let lastUsedPrinter;
    let defaultSystemPrinter;

    let saveToPdfPrinter = {
      nameId: "printui-destination-pdf-label",
      value: PrintUtils.SAVE_TO_PDF_PRINTER,
    };
    printersByName[PrintUtils.SAVE_TO_PDF_PRINTER] = {
      supportsColor: true,
      name: PrintUtils.SAVE_TO_PDF_PRINTER,
    };

    if (lastUsedPrinterName == PrintUtils.SAVE_TO_PDF_PRINTER) {
      lastUsedPrinter = saveToPdfPrinter;
    }

    let destinations = [
      saveToPdfPrinter,
      ...printers.map(printer => {
        printer.QueryInterface(Ci.nsIPrinter);
        const { name } = printer;
        printersByName[printer.name] = { printer };
        const destination = { name, value: name };

        if (name == lastUsedPrinterName) {
          lastUsedPrinter = destination;
        }
        if (name == defaultPrinterName) {
          defaultSystemPrinter = destination;
        }

        return destination;
      }),
    ];

    let selectedPrinter =
      lastUsedPrinter || defaultSystemPrinter || saveToPdfPrinter;

    return {
      destinations,
      fallbackPaperList,
      selectedPrinter,
      printersByName,
      defaultSystemPrinter,
    };
  },

  getMarginPresets(marginSize, paper) {
    switch (marginSize) {
      case "minimum":
        return {
          marginTop: (paper || this.defaultSettings).unwriteableMarginTop,
          marginLeft: (paper || this.defaultSettings).unwriteableMarginLeft,
          marginBottom: (paper || this.defaultSettings).unwriteableMarginBottom,
          marginRight: (paper || this.defaultSettings).unwriteableMarginRight,
        };
      case "none":
        return {
          marginTop: 0,
          marginLeft: 0,
          marginBottom: 0,
          marginRight: 0,
        };
      default:
        return {
          marginTop: this.defaultSettings.marginTop,
          marginLeft: this.defaultSettings.marginLeft,
          marginBottom: this.defaultSettings.marginBottom,
          marginRight: this.defaultSettings.marginRight,
        };
    }
  },
};

const PrintSettingsViewProxy = {
  get defaultHeadersAndFooterValues() {
    const defaultBranch = Services.prefs.getDefaultBranch("");
    let settingValues = {};
    for (let [name, pref] of Object.entries(this.headerFooterSettingsPrefs)) {
      settingValues[name] = defaultBranch.getStringPref(pref);
    }
    // We only need to retrieve these defaults once and they will not change
    Object.defineProperty(this, "defaultHeadersAndFooterValues", {
      value: settingValues,
    });
    return settingValues;
  },

  headerFooterSettingsPrefs: {
    footerStrCenter: "print.print_footercenter",
    footerStrLeft: "print.print_footerleft",
    footerStrRight: "print.print_footerright",
    headerStrCenter: "print.print_headercenter",
    headerStrLeft: "print.print_headerleft",
    headerStrRight: "print.print_headerright",
  },

  // This list was taken from nsDeviceContextSpecWin.cpp which records telemetry on print target type
  knownSaveToFilePrinters: new Set([
    "Microsoft Print to PDF",
    "Adobe PDF",
    "Bullzip PDF Printer",
    "CutePDF Writer",
    "doPDF",
    "Foxit Reader PDF Printer",
    "Nitro PDF Creator",
    "novaPDF",
    "PDF-XChange",
    "PDF24 PDF",
    "PDFCreator",
    "PrimoPDF",
    "Soda PDF",
    "Solid PDF Creator",
    "Universal Document Converter",
    "Microsoft XPS Document Writer",
  ]),

  getBestPaperMatch(paperName, paperWidth, paperHeight, paperSizeUnit) {
    let matchedPaper = paperName && this.availablePaperSizes[paperName];
    if (matchedPaper) {
      return matchedPaper;
    }
    let paperSizes = Object.values(this.availablePaperSizes);
    if (!(paperWidth && paperHeight)) {
      return null;
    }
    // first try to match on the paper dimensions using the current units
    let unitsPerPoint;
    let altUnitsPerPoint;
    if (paperSizeUnit == PrintEventHandler.settings.kPaperSizeMillimeters) {
      unitsPerPoint = MM_PER_POINT;
      altUnitsPerPoint = INCHES_PER_POINT;
    } else {
      unitsPerPoint = INCHES_PER_POINT;
      altUnitsPerPoint = MM_PER_POINT;
    }
    // equality to 1pt.
    const equal = (a, b) => Math.abs(a - b) < 1;
    const findMatch = (widthPts, heightPts) =>
      paperSizes.find(paperInfo => {
        // the dimensions on the nsIPaper object are in points
        let result =
          equal(widthPts, paperInfo.paper.width) &&
          equal(heightPts, paperInfo.paper.height);
        return result;
      });
    // Look for a paper with matching dimensions, using the current printer's
    // paper size unit, then the alternate unit
    matchedPaper =
      findMatch(paperWidth / unitsPerPoint, paperHeight / unitsPerPoint) ||
      findMatch(paperWidth / altUnitsPerPoint, paperHeight / altUnitsPerPoint);

    if (matchedPaper) {
      return matchedPaper;
    }
    return null;
  },

  async fetchPaperMargins(paperName) {
    // resolve any async and computed properties we need on the paper
    let paperInfo = this.availablePaperSizes[paperName];
    if (!paperInfo) {
      throw new Error("Can't fetchPaperMargins: " + paperName);
    }
    if (paperInfo._resolved) {
      // We've already resolved and calculated these values
      return;
    }
    let margins = await paperInfo.paper.unwriteableMargin;
    margins.QueryInterface(Ci.nsIPaperMargin);

    // margin dimenions are given on the paper in points, setting values need to be in inches
    paperInfo.unwriteableMarginTop = margins.top * INCHES_PER_POINT;
    paperInfo.unwriteableMarginRight = margins.right * INCHES_PER_POINT;
    paperInfo.unwriteableMarginBottom = margins.bottom * INCHES_PER_POINT;
    paperInfo.unwriteableMarginLeft = margins.left * INCHES_PER_POINT;
    // No need to re-resolve static properties
    paperInfo._resolved = true;
  },

  async resolvePropertiesForPrinter(printerName) {
    // resolve any async properties we need on the printer
    let printerInfo = this.availablePrinters[printerName];
    if (printerInfo._resolved) {
      // Store a convenience reference
      this.availablePaperSizes = printerInfo.availablePaperSizes;
      return printerInfo;
    }

    const PSSVC = Cc["@mozilla.org/gfx/printsettings-service;1"].getService(
      Ci.nsIPrintSettingsService
    );

    // Await the async printer data.
    if (printerInfo.printer) {
      [
        printerInfo.supportsColor,
        printerInfo.paperList,
        printerInfo.defaultSettings,
      ] = await Promise.all([
        printerInfo.printer.supportsColor,
        printerInfo.printer.paperList,
        // get a set of default settings for this printer
        printerInfo.printer.createDefaultSettings(printerName),
      ]);
      printerInfo.defaultSettings.QueryInterface(Ci.nsIPrintSettings);
    } else if (printerName == PrintUtils.SAVE_TO_PDF_PRINTER) {
      // The Mozilla PDF pseudo-printer has no actual nsIPrinter implementation
      printerInfo.defaultSettings = PSSVC.newPrintSettings;
      printerInfo.defaultSettings.printerName = printerName;
      printerInfo.paperList = this.fallbackPaperList;
    }
    printerInfo.settings = printerInfo.defaultSettings.clone();
    // Apply any user values
    PSSVC.initPrintSettingsFromPrefs(
      printerInfo.settings,
      true,
      printerInfo.settings.kInitSaveAll
    );
    // We set `isInitializedFromPrinter` to make sure that that's set on the
    // SAVE_TO_PDF_PRINTER settings.  The naming is poor, but that tells the
    // platform code that the settings object is complete.
    printerInfo.settings.isInitializedFromPrinter = true;

    // prepare the available paper sizes for this printer
    let unitsPerPoint =
      printerInfo.settings.paperSizeUnit ==
      printerInfo.settings.kPaperSizeMillimeters
        ? MM_PER_POINT
        : INCHES_PER_POINT;

    let papersByName = (printerInfo.availablePaperSizes = {});
    // Store a convenience reference
    this.availablePaperSizes = papersByName;

    for (let paper of printerInfo.paperList) {
      paper.QueryInterface(Ci.nsIPaper);
      // Bug 1662239: I'm seeing multiple duplicate entries for each paper size
      // so ensure we have one entry per name
      if (!papersByName[paper.name]) {
        papersByName[paper.name] = {
          paper,
          name: paper.name,
          // Prepare dimension values in the correct unit for the settings. Paper dimensions
          // are given in points, so we multiply with the units-per-pt to get dimensions
          // in the correct unit for the current printer
          width: paper.width * unitsPerPoint,
          height: paper.height * unitsPerPoint,
          unitsPerPoint,
        };
      }
    }
    // The printer properties don't change, mark this as resolved for next time
    printerInfo._resolved = true;
    return printerInfo;
  },

  get(target, name) {
    switch (name) {
      case "currentPaper": {
        let paperName = this.get(target, "paperName");
        return this.availablePaperSizes[paperName];
      }

      case "margins":
        let marginSettings = {
          marginTop: target.marginTop,
          marginLeft: target.marginLeft,
          marginBottom: target.marginBottom,
          marginRight: target.marginRight,
        };
        // see if they match the minimum first
        let paperSize = this.get(target, "currentPaper");
        for (let presetName of ["minimum", "none"]) {
          let marginPresets = PrintEventHandler.getMarginPresets(
            presetName,
            paperSize
          );
          if (
            Object.keys(marginSettings).every(
              name => marginSettings[name] == marginPresets[name]
            )
          ) {
            return presetName;
          }
        }
        // Fall back to the default for any other values
        return "default";

      case "paperSizes":
        return Object.values(this.availablePaperSizes).map(paper => {
          return {
            name: paper.name,
            value: paper.name,
          };
        });

      case "printBackgrounds":
        return target.printBGImages || target.printBGColors;

      case "printFootersHeaders":
        // if any of the footer and headers settings have a non-empty string value
        // we consider that "enabled"
        return Object.keys(this.headerFooterSettingsPrefs).some(
          name => !!target[name]
        );

      case "printAllOrCustomRange":
        return target.printRange == Ci.nsIPrintSettings.kRangeAllPages
          ? "all"
          : "custom";

      case "supportsColor":
        return this.availablePrinters[target.printerName].supportsColor;

      case "willSaveToFile":
        return (
          target.outputFormat == Ci.nsIPrintSettings.kOutputFormatPDF ||
          this.knownSaveToFilePrinters.has(target.printerName)
        );
      // Black and white is always supported except:
      //
      //  * For PDF printing, where it'd require rasterization and thus bad
      //    quality.
      //
      //  * For Mac, where there's no API to print in monochrome.
      //
      case "supportsMonochrome":
        return (
          !this.get(target, "supportsColor") ||
          (target.printerName != PrintUtils.SAVE_TO_PDF_PRINTER &&
            AppConstants.platform !== "macosx")
        );
      case "defaultSystemPrinter":
        return (
          this.defaultSystemPrinter?.value ||
          Object.getOwnPropertyNames(this.availablePrinters).find(
            p => p.name != PrintUtils.SAVE_TO_PDF_PRINTER
          )?.value
        );
    }
    return target[name];
  },

  set(target, name, value) {
    switch (name) {
      case "margins":
        if (!["default", "minimum", "none"].includes(value)) {
          console.warn("Unexpected margin preset name: ", value);
          value = "default";
        }
        let paperSize = this.get(target, "currentPaper");
        let marginPresets = PrintEventHandler.getMarginPresets(
          value,
          paperSize
        );
        for (let [settingName, presetValue] of Object.entries(marginPresets)) {
          target[settingName] = presetValue;
        }
        break;

      case "paperName": {
        let paperName = value;
        let paperSize = this.availablePaperSizes[paperName];
        target.paperWidth = paperSize.width;
        target.paperHeight = paperSize.height;
        target.paperName = value;
        // pull new margin values for the new paperName
        this.set(target, "margins", this.get(target, "margins"));
        break;
      }

      case "printBackgrounds":
        target.printBGImages = value;
        target.printBGColors = value;
        break;

      case "printFootersHeaders":
        // To disable header & footers, set them all to empty.
        // To enable, restore default values for each of the header & footer settings.
        for (let [settingName, defaultValue] of Object.entries(
          this.defaultHeadersAndFooterValues
        )) {
          target[settingName] = value ? defaultValue : "";
        }
        break;

      case "printAllOrCustomRange":
        target.printRange =
          value == "all"
            ? Ci.nsIPrintSettings.kRangeAllPages
            : Ci.nsIPrintSettings.kRangeSpecifiedPageRange;
        break;

      case "printerName":
        target.printerName = value;
        target.toFileName = "";
        if (value == PrintUtils.SAVE_TO_PDF_PRINTER) {
          target.outputFormat = Ci.nsIPrintSettings.kOutputFormatPDF;
          target.printToFile = true;
        } else {
          target.outputFormat = Ci.nsIPrintSettings.kOutputFormatNative;
          target.printToFile = false;
        }
        break;

      default:
        target[name] = value;
    }
  },
};

/*
 * Custom elements ----------------------------------------------------
 */

function PrintUIControlMixin(superClass) {
  return class PrintUIControl extends superClass {
    connectedCallback() {
      this.initialize();
      this.render();
    }

    initialize() {
      if (this._initialized) {
        return;
      }
      this._initialized = true;
      if (this.templateId) {
        let template = this.ownerDocument.getElementById(this.templateId);
        let templateContent = template.content;
        this.appendChild(templateContent.cloneNode(true));
      }

      document.addEventListener("print-settings", ({ detail: settings }) => {
        this.update(settings);
      });

      this.addEventListener("change", this);
    }

    render() {}

    update(settings) {}

    dispatchSettingsChange(changedSettings) {
      this.dispatchEvent(
        new CustomEvent("update-print-settings", {
          bubbles: true,
          detail: changedSettings,
        })
      );
    }

    handleKeypress(e) {
      let char = String.fromCharCode(e.charCode);
      if (
        !char.match(/^[0-9]$/) &&
        !char.match("\x00") &&
        !e.ctrlKey &&
        !e.metaKey
      ) {
        e.preventDefault();
      }
    }

    handleEvent(event) {}
  };
}

class PrintSettingSelect extends PrintUIControlMixin(HTMLSelectElement) {
  connectedCallback() {
    this.settingName = this.dataset.settingName;
    super.connectedCallback();
  }

  setOptions(optionValues = []) {
    this.textContent = "";
    for (let optionData of optionValues) {
      let opt = new Option(
        optionData.name,
        "value" in optionData ? optionData.value : optionData.name
      );
      if (optionData.nameId) {
        document.l10n.setAttributes(opt, optionData.nameId);
      }
      // option selectedness is set via update() and assignment to this.value
      this.options.add(opt);
    }
  }

  update(settings) {
    this.value = settings[this.settingName];
  }

  handleEvent(e) {
    if (e.type == "change") {
      this.dispatchSettingsChange({
        [this.settingName]: e.target.value,
      });
    }
  }
}
customElements.define("setting-select", PrintSettingSelect, {
  extends: "select",
});

class DestinationPicker extends PrintSettingSelect {
  initialize() {
    super.initialize();
    document.addEventListener("available-destinations", this);
  }

  update(settings) {
    super.update(settings);
    let isPdf = settings.outputFormat == Ci.nsIPrintSettings.kOutputFormatPDF;
    this.setAttribute("output", isPdf ? "pdf" : "paper");
  }

  handleEvent(e) {
    super.handleEvent(e);

    if (e.type == "available-destinations") {
      this.setOptions(e.detail);
    }
  }
}
customElements.define("destination-picker", DestinationPicker, {
  extends: "select",
});

class ColorModePicker extends PrintSettingSelect {
  update(settings) {
    this.value = settings[this.settingName] ? "color" : "bw";
    let canSwitch = settings.supportsColor && settings.supportsMonochrome;
    this.toggleAttribute("disallowed", !canSwitch);
    this.disabled = !canSwitch;
  }

  handleEvent(e) {
    if (e.type == "change") {
      // turn our string value into the expected boolean
      this.dispatchSettingsChange({
        [this.settingName]: this.value == "color",
      });
    }
  }
}
customElements.define("color-mode-select", ColorModePicker, {
  extends: "select",
});

class PaperSizePicker extends PrintSettingSelect {
  initialize() {
    super.initialize();
    this._printerName = null;
  }

  update(settings) {
    if (settings.printerName !== this._printerName) {
      this._printerName = settings.printerName;
      this.setOptions(settings.paperSizes);
    }
    this.value = settings.paperName;
  }
}
customElements.define("paper-size-select", PaperSizePicker, {
  extends: "select",
});

class OrientationInput extends PrintUIControlMixin(HTMLElement) {
  get templateId() {
    return "orientation-template";
  }

  update(settings) {
    for (let input of this.querySelectorAll("input")) {
      input.checked = settings.orientation == input.value;
    }
  }

  handleEvent(e) {
    this.dispatchSettingsChange({
      orientation: e.target.value,
    });
  }
}
customElements.define("orientation-input", OrientationInput);

class CopiesInput extends PrintUIControlMixin(HTMLInputElement) {
  initialize() {
    super.initialize();
    this.addEventListener("input", this);
    this.addEventListener("keypress", this);
  }

  update(settings) {
    this.value = settings.numCopies;
  }

  handleEvent(e) {
    if (e.type == "keypress") {
      this.handleKeypress(e);
      return;
    }

    if (this.checkValidity()) {
      this.dispatchSettingsChange({
        numCopies: e.target.value,
      });
    }
  }
}
customElements.define("copy-count-input", CopiesInput, {
  extends: "input",
});

class PrintUIForm extends PrintUIControlMixin(HTMLFormElement) {
  initialize() {
    super.initialize();

    this.setAttribute("platform", AppConstants.platform);

    this.addEventListener("change", this);
    this.addEventListener("submit", this);
    this.addEventListener("click", this);
    this.addEventListener("input", this);
  }

  update(settings) {
    // If there are no default system printers available and we are not on mac,
    // we should hide the system dialog because it won't be populated with
    // the correct settings. Mac supports save to pdf functionality
    // in the native dialog, so it can be shown regardless.
    this.querySelector("#system-print").hidden =
      !settings.defaultSystemPrinter && AppConstants.platform !== "macosx";

    this.querySelector("#copies").hidden = settings.willSaveToFile;
  }

  handleEvent(e) {
    if (e.target.id == "open-dialog-link") {
      this.dispatchEvent(new Event("open-system-dialog", { bubbles: true }));
      return;
    }

    if (e.type == "submit") {
      e.preventDefault();
      switch (e.submitter.name) {
        case "print":
          if (!this.checkValidity()) {
            return;
          }
          this.dispatchEvent(new Event("print", { bubbles: true }));
          break;
        case "cancel":
          this.dispatchEvent(new Event("cancel-print", { bubbles: true }));
          break;
      }
    } else if (e.type == "change" || e.type == "input") {
      let isValid = this.checkValidity();
      let section = e.target.closest(".section-block");
      document.body.toggleAttribute("invalid", !isValid);
      if (isValid) {
        // aria-describedby will usually cause the first value to be reported.
        // Unfortunately, screen readers don't pick up description changes from
        // dialogs, so we must use a live region. To avoid double reporting of
        // the first value, we don't set aria-live initially. We only set it for
        // subsequent updates.
        // aria-live is set on the parent because sheetCount itself might be
        // hidden and then shown, and updates are only reported for live
        // regions that were already visible.
        document
          .querySelector("#sheet-count")
          .parentNode.setAttribute("aria-live", "polite");
      } else {
        // We're hiding the sheet count and aria-describedby includes the
        // content of hidden elements, so remove aria-describedby.
        document.body.removeAttribute("aria-describedby");
      }
      for (let element of this.elements) {
        // If we're valid, enable all inputs.
        // Otherwise, disable the valid inputs other than the cancel button and the elements
        // in the invalid section.
        element.disabled =
          element.hasAttribute("disallowed") ||
          (!isValid &&
            element.validity.valid &&
            element.name != "cancel" &&
            element.closest(".section-block") != section);
      }
    }
  }
}
customElements.define("print-form", PrintUIForm, { extends: "form" });

class ScaleInput extends PrintUIControlMixin(HTMLElement) {
  get templateId() {
    return "scale-template";
  }

  initialize() {
    super.initialize();

    this._percentScale = this.querySelector("#percent-scale");
    this._shrinkToFitChoice = this.querySelector("#fit-choice");
    this._scaleChoice = this.querySelector("#percent-scale-choice");
    this._scaleError = this.querySelector("#error-invalid-scale");

    this._percentScale.addEventListener("input", this);
    this._percentScale.addEventListener("keypress", this);
    this.addEventListener("input", this);
  }

  update(settings) {
    let { scaling, shrinkToFit } = settings;
    this._shrinkToFitChoice.checked = shrinkToFit;
    this._scaleChoice.checked = !shrinkToFit;
    this._percentScale.disabled = shrinkToFit;
    this._percentScale.toggleAttribute("disallowed", shrinkToFit);

    // If the user had an invalid input and switches back to "fit to page",
    // we repopulate the scale field with the stored, valid scaling value.
    if (
      !this._percentScale.value ||
      (this._shrinkToFitChoice.checked && !this._percentScale.checkValidity())
    ) {
      // Only allow whole numbers. 0.14 * 100 would have decimal places, etc.
      this._percentScale.value = parseInt(scaling * 100, 10);
    }
  }

  handleEvent(e) {
    if (e.type == "keypress") {
      this.handleKeypress(e);
      return;
    }

    if (e.target == this._shrinkToFitChoice || e.target == this._scaleChoice) {
      if (!this._percentScale.checkValidity()) {
        this._percentScale.value = 100;
      }
      let scale =
        e.target == this._shrinkToFitChoice
          ? 1
          : Number(this._percentScale.value / 100);
      this.dispatchSettingsChange({
        shrinkToFit: this._shrinkToFitChoice.checked,
        scaling: scale,
      });
      this._scaleError.hidden = true;
    } else if (e.type == "input") {
      window.clearTimeout(this.updateSettingsTimeoutId);

      if (this._percentScale.checkValidity()) {
        this.updateSettingsTimeoutId = window.setTimeout(() => {
          this.dispatchSettingsChange({
            scaling: Number(this._percentScale.value / 100),
          });
        }, INPUT_DELAY_MS);
      }
    }

    window.clearTimeout(this.showErrorTimeoutId);
    if (this._percentScale.validity.valid) {
      this._scaleError.hidden = true;
    } else {
      this.showErrorTimeoutId = window.setTimeout(() => {
        this._scaleError.hidden = false;
      }, INPUT_DELAY_MS);
    }
  }
}
customElements.define("scale-input", ScaleInput);

class PageRangeInput extends PrintUIControlMixin(HTMLElement) {
  initialize() {
    super.initialize();

    this._startRange = this.querySelector("#custom-range-start");
    this._endRange = this.querySelector("#custom-range-end");
    this._rangePicker = this.querySelector("#range-picker");
    this._rangeError = this.querySelector("#error-invalid-range");
    this._startRangeOverflowError = this.querySelector(
      "#error-invalid-start-range-overflow"
    );

    this.addEventListener("input", this);
    this.addEventListener("keypress", this);
    document.addEventListener("page-count", this);
  }

  get templateId() {
    return "page-range-template";
  }

  update(settings) {
    this.toggleAttribute("all-pages", settings.printRange == 0);
  }

  handleEvent(e) {
    if (e.type == "keypress") {
      this.handleKeypress(e);
      return;
    }

    if (e.type == "page-count") {
      this._startRange.max = this._endRange.max = this._numPages =
        e.detail.totalPages;
      this._startRange.disabled = this._endRange.disabled = false;
      if (!this._endRange.checkValidity()) {
        this._endRange.value = this._numPages;
        this.dispatchSettingsChange({
          endPageRange: this._endRange.value,
        });
        this._endRange.dispatchEvent(new Event("change", { bubbles: true }));
      }
      return;
    }

    if (e.target == this._rangePicker) {
      let printAll = e.target.value == "all";
      this._startRange.required = this._endRange.required = !printAll;
      this.querySelector(".range-group").hidden = printAll;
      this._startRange.value = 1;
      this._endRange.value = this._numPages || 1;

      this.dispatchSettingsChange({
        printAllOrCustomRange: e.target.value,
        startPageRange: this._startRange.value,
        endPageRange: this._endRange.value,
      });
      this._rangeError.hidden = true;
      this._startRangeOverflowError.hidden = true;
      return;
    }

    if (e.target == this._startRange || e.target == this._endRange) {
      if (this._startRange.checkValidity()) {
        this._endRange.min = this._startRange.value;
      }
      if (this._endRange.checkValidity()) {
        this._startRange.max = this._endRange.value;
      }
      if (this._startRange.checkValidity() && this._endRange.checkValidity()) {
        if (this._startRange.value && this._endRange.value) {
          this.dispatchSettingsChange({
            startPageRange: this._startRange.value,
            endPageRange: this._endRange.value,
          });
        }
      }
    }
    document.l10n.setAttributes(
      this._rangeError,
      "printui-error-invalid-range",
      {
        numPages: this._numPages,
      }
    );

    window.clearTimeout(this.showErrorTimeoutId);
    let hasShownOverflowError = false;
    let startValidity = this._startRange.validity;
    let endValidity = this._endRange.validity;

    // Display the startRangeOverflowError if the start range exceeds
    // the end range. This means either the start range is greater than its
    // max constraint, whiich is determined by the end range, or the end range
    // is less than its minimum constraint, determined by the start range.
    if (
      !(
        (startValidity.rangeOverflow && endValidity.valid) ||
        (endValidity.rangeUnderflow && startValidity.valid)
      )
    ) {
      this._startRangeOverflowError.hidden = true;
    } else {
      hasShownOverflowError = true;
      this.showErrorTimeoutId = window.setTimeout(() => {
        this._startRangeOverflowError.hidden = false;
      }, INPUT_DELAY_MS);
    }

    // Display the generic error if the startRangeOverflowError is not already
    // showing and a range input is invalid.
    if (hasShownOverflowError || (startValidity.valid && endValidity.valid)) {
      this._rangeError.hidden = true;
    } else {
      this.showErrorTimeoutId = window.setTimeout(() => {
        this._rangeError.hidden = false;
      }, INPUT_DELAY_MS);
    }
  }
}
customElements.define("page-range-input", PageRangeInput);

class PrintSettingNumber extends PrintUIControlMixin(HTMLInputElement) {
  connectedCallback() {
    this.type = "number";
    this.settingName = this.dataset.settingName;
    super.connectedCallback();
  }
  update(settings) {
    this.value = settings[this.settingName];
  }

  handleEvent(e) {
    this.dispatchSettingsChange({
      [this.settingName]: this.value,
    });
  }
}
customElements.define("setting-number", PrintSettingNumber, {
  extends: "input",
});

class PrintSettingCheckbox extends PrintUIControlMixin(HTMLInputElement) {
  connectedCallback() {
    this.type = "checkbox";
    this.settingName = this.dataset.settingName;
    super.connectedCallback();
  }

  update(settings) {
    this.checked = settings[this.settingName];
  }

  handleEvent(e) {
    this.dispatchSettingsChange({
      [this.settingName]: this.checked,
    });
  }
}
customElements.define("setting-checkbox", PrintSettingCheckbox, {
  extends: "input",
});

class TwistySummary extends PrintUIControlMixin(HTMLElement) {
  get isOpen() {
    return this.closest("details")?.hasAttribute("open");
  }

  get templateId() {
    return "twisty-summary-template";
  }

  initialize() {
    if (this._initialized) {
      return;
    }
    super.initialize();
    this.label = this.querySelector(".label");

    this.addEventListener("click", this);
    this.updateSummary();
  }

  handleEvent(e) {
    let willOpen = !this.isOpen;
    this.updateSummary(willOpen);
  }

  updateSummary(open = false) {
    document.l10n.setAttributes(
      this.label,
      open
        ? this.getAttribute("data-open-l10n-id")
        : this.getAttribute("data-closed-l10n-id")
    );
  }
}
customElements.define("twisty-summary", TwistySummary);

class PageCount extends PrintUIControlMixin(HTMLElement) {
  initialize() {
    super.initialize();
    document.addEventListener("page-count", this);
  }

  update(settings) {
    this.numCopies = settings.numCopies;
    this.render();
  }

  render() {
    if (!this.numCopies || !this.numPages) {
      return;
    }
    document.l10n.setAttributes(this, "printui-sheets-count", {
      sheetCount: this.numPages * this.numCopies,
    });
    if (this.id) {
      // We're showing the sheet count, so let it describe the dialog.
      document.body.setAttribute("aria-describedby", this.id);
    }
  }

  handleEvent(e) {
    let { numPages } = e.detail;
    this.numPages = numPages;
    this.render();
  }
}
customElements.define("page-count", PageCount);

class PrintButton extends PrintUIControlMixin(HTMLButtonElement) {
  update(settings) {
    let l10nId =
      settings.printerName == PrintUtils.SAVE_TO_PDF_PRINTER
        ? "printui-primary-button-save"
        : "printui-primary-button";
    document.l10n.setAttributes(this, l10nId);
  }
}
customElements.define("print-button", PrintButton, { extends: "button" });

async function pickFileName(contentTitle, currentURI) {
  let picker = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
  let [title] = await document.l10n.formatMessages([
    { id: "printui-save-to-pdf-title" },
  ]);
  title = title.value;

  let filename;
  if (contentTitle != "") {
    filename = contentTitle;
  } else {
    let url = new URL(currentURI);
    let path = decodeURIComponent(url.pathname);
    path = path.replace(/\/$/, "");
    filename = path.split("/").pop();
    if (filename == "") {
      filename = url.hostname;
    }
  }
  if (!filename.endsWith(".pdf")) {
    // macOS and linux don't set the extension based on the default extension.
    // Windows won't add the extension a second time, fortunately.
    // If it already ends with .pdf though, adding it again isn't needed.
    filename += ".pdf";
  }
  filename = DownloadPaths.sanitize(filename);

  picker.init(
    window.docShell.chromeEventHandler.ownerGlobal,
    title,
    Ci.nsIFilePicker.modeSave
  );
  picker.appendFilter("PDF", "*.pdf");
  picker.defaultExtension = "pdf";
  picker.defaultString = filename;

  let retval = await new Promise(resolve => picker.open(resolve));

  if (retval == 1) {
    throw new Error({ reason: "cancelled" });
  } else {
    // OK clicked (retval == 0) or replace confirmed (retval == 2)

    // Workaround: When trying to replace an existing file that is open in another application (i.e. a locked file),
    // the print progress listener is never called. This workaround ensures that a correct status is always returned.
    try {
      let fstream = Cc[
        "@mozilla.org/network/file-output-stream;1"
      ].createInstance(Ci.nsIFileOutputStream);
      fstream.init(picker.file, 0x2a, 0o666, 0); // ioflags = write|create|truncate, file permissions = rw-rw-rw-
      fstream.close();
    } catch (e) {
      throw new Error({ reason: retval == 0 ? "not_saved" : "not_replaced" });
    }
  }

  return picker.file.path;
}
