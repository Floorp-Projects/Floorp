/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {
  gBrowser,
  PrintUtils,
  Services,
} = window.docShell.chromeEventHandler.ownerGlobal;

const INVALID_INPUT_DELAY_MS = 500;

document.addEventListener(
  "DOMContentLoaded",
  e => {
    PrintEventHandler.init();
  },
  { once: true }
);

window.addEventListener(
  "unload",
  e => {
    document.textContent = "";
  },
  { once: true }
);

var PrintEventHandler = {
  init() {
    this.sourceBrowser = this.getSourceBrowser();
    this.previewBrowser = this.getPreviewBrowser();
    this.settings = PrintUtils.getPrintSettings();
    this.updatePrintPreview();

    document.addEventListener("print", e => this.print({ silent: true }));
    document.addEventListener("update-print-settings", e =>
      this.updateSettings(e.detail)
    );
    document.addEventListener("cancel-print", () => this.cancelPrint());
    document.addEventListener("open-system-dialog", () =>
      this.print({ silent: false })
    );
    this.getPrintDestinations().then(destinations => {
      document.dispatchEvent(
        new CustomEvent("available-destinations", {
          detail: destinations,
        })
      );
    });

    // Some settings are only used by the UI
    // assigning new values should update the underlying settings
    this.viewSettings = new Proxy(this.settings, PrintSettingsViewProxy);

    this.settingFlags = {
      orientation: Ci.nsIPrintSettings.kInitSaveOrientation,
      printerName: Ci.nsIPrintSettings.kInitSaveAll,
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
    };

    document.dispatchEvent(
      new CustomEvent("print-settings", {
        detail: this.viewSettings,
      })
    );
  },

  print({ printerName, silent } = {}) {
    let settings = this.settings;
    settings.printSilent = silent;

    if (printerName) {
      settings.printerName = printerName;
    }
    PrintUtils.printWindow(this.previewBrowser.browsingContext, settings);
  },

  cancelPrint() {
    window.close();
  },

  updateSettings(changedSettings = {}) {
    let isChanged = false;
    let flags = 0;
    for (let [setting, value] of Object.entries(changedSettings)) {
      if (this.viewSettings[setting] != value) {
        this.viewSettings[setting] = value;

        if (setting in this.settingFlags) {
          flags |= this.settingFlags[setting];
        }
        isChanged = true;
        Services.telemetry.keyedScalarAdd(
          "printing.settings_changed",
          setting,
          1
        );
      }
    }

    if (isChanged) {
      let PSSVC = Cc["@mozilla.org/gfx/printsettings-service;1"].getService(
        Ci.nsIPrintSettingsService
      );

      if (flags) {
        PSSVC.savePrintSettingsToPrefs(this.settings, true, flags);
        this.updatePrintPreview();
      }

      document.dispatchEvent(
        new CustomEvent("print-settings", {
          detail: this.viewSettings,
        })
      );
    }
  },

  async updatePrintPreview() {
    if (this._previewUpdatingPromise) {
      if (!this._queuedPreviewUpdatePromise) {
        this._queuedPreviewUpdatePromise = this._previewUpdatingPromise.then(
          () => this._updatePrintPreview()
        );
      }
      // else there's already an update queued.
    } else {
      this._previewUpdatingPromise = this._updatePrintPreview();
    }
  },

  async _updatePrintPreview() {
    let numPages = await PrintUtils.updatePrintPreview(
      this.sourceBrowser,
      this.previewBrowser,
      this.settings
    );
    document.dispatchEvent(
      new CustomEvent("page-count", { detail: { numPages } })
    );

    if (this._queuedPreviewUpdatePromise) {
      // Now that we're done, the queued update (if there is one) will start.
      this._previewUpdatingPromise = this._queuedPreviewUpdatePromise;
      this._queuedPreviewUpdatePromise = null;
    } else {
      // Nothing queued so throw our promise away.
      this._previewUpdatingPromise = null;
    }
  },

  getSourceBrowser() {
    let params = new URLSearchParams(location.search);
    let browsingContextId = params.get("browsingContextId");
    if (!browsingContextId) {
      return null;
    }
    let browsingContext = BrowsingContext.get(browsingContextId);
    if (!browsingContext) {
      return null;
    }
    return browsingContext.embedderElement;
  },

  getPreviewBrowser() {
    let container = gBrowser.getBrowserContainer(this.sourceBrowser);
    return container.querySelector(".printPreviewBrowser");
  },

  async getPrintDestinations() {
    const printerList = Cc["@mozilla.org/gfx/printerlist;1"].createInstance(
      Ci.nsIPrinterList
    );

    const lastUsedPrinterName = PrintUtils._getLastUsedPrinterName();
    const defaultPrinterName = printerList.systemDefaultPrinterName;
    const printers = await printerList.printers;

    let defaultIndex = 0;
    let foundSelected = false;
    let i = 0;
    let destinations = printers.map(printer => {
      printer.QueryInterface(Ci.nsIPrinter);
      const name = printer.name;
      const value = name;
      const selected = name == lastUsedPrinterName;
      if (selected) {
        foundSelected = true;
      }
      if (name == defaultPrinterName) {
        defaultIndex = i;
      }
      ++i;
      return { name, value, selected };
    });

    // If there's no valid last selected printer, select the system default, or
    // the first on the list otherwise.
    if (destinations.length && !foundSelected) {
      destinations[defaultIndex].selected = true;
    }

    return destinations;
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

  get(target, name) {
    switch (name) {
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
    }
    return target[name];
  },

  set(target, name, value) {
    switch (name) {
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
        // TODO: There's also kRangeSelection, which should come into play
        // once we have a text box where the user can specify a range
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

    handleEvent(event) {}
  };
}

class DestinationPicker extends PrintUIControlMixin(HTMLSelectElement) {
  initialize() {
    this.addEventListener("change", this);
    document.addEventListener("available-destinations", this);
  }

  setOptions(optionValues = []) {
    this._options = optionValues;
    this.textContent = "";
    for (let optionData of this._options) {
      let opt = new Option(
        optionData.name,
        "value" in optionData ? optionData.value : optionData.name
      );
      if (optionData.selected) {
        this._currentPrinter = optionData.value;
        opt.selected = true;
      }
      this.options.add(opt);
    }
  }

  handleEvent(e) {
    if (e.type == "change") {
      this._currentPrinter = e.target.value;
      this.dispatchSettingsChange({
        printerName: e.target.value,
      });
    }

    if (e.type == "available-destinations") {
      this.setOptions(e.detail);
    }
  }
}
customElements.define("destination-picker", DestinationPicker, {
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
  connectedCallback() {
    this.type = "number";
    super.connectedCallback();
  }

  update(settings) {
    this.value = settings.numCopies;
  }

  handleEvent(e) {
    this.dispatchSettingsChange({
      numCopies: e.target.value,
    });
  }
}
customElements.define("copy-count-input", CopiesInput, {
  extends: "input",
});

class PrintUIForm extends PrintUIControlMixin(HTMLElement) {
  initialize() {
    super.initialize();

    this.addEventListener("submit", this);
    this.addEventListener("click", this);
  }

  handleEvent(e) {
    if (e.target.id == "open-dialog-link") {
      this.dispatchEvent(new Event("open-system-dialog", { bubbles: true }));
    }

    if (e.type == "submit") {
      e.preventDefault();
      switch (e.submitter.name) {
        case "print":
          this.dispatchEvent(new Event("print", { bubbles: true }));
          break;
        case "cancel":
          this.dispatchEvent(new Event("cancel-print", { bubbles: true }));
          break;
      }
    }
  }
}
customElements.define("print-form", PrintUIForm);

class ScaleInput extends PrintUIControlMixin(HTMLElement) {
  get templateId() {
    return "scale-template";
  }

  initialize() {
    super.initialize();

    this._percentScale = this.querySelector("#percent-scale");
    this._percentScale.addEventListener("input", this);
    this._shrinkToFitChoice = this.querySelector("#fit-choice");
    this._scaleChoice = this.querySelector("#percent-scale-choice");

    this.addEventListener("input", this);
  }

  update(settings) {
    let { scaling, shrinkToFit } = settings;
    this._shrinkToFitChoice.checked = shrinkToFit;
    this._scaleChoice.checked = !shrinkToFit;
    this._percentScale.disabled = shrinkToFit;

    // If the user had an invalid input and switches back to "fit to page",
    // we repopulate the scale field with the stored, valid scaling value.
    if (!this._percentScale.value || this._shrinkToFitChoice.checked) {
      // Only allow whole numbers. 0.14 * 100 would have decimal places, etc.
      this._percentScale.value = parseInt(scaling * 100, 10);
    }
  }

  handleEvent(e) {
    e.stopPropagation();

    if (e.target == this._shrinkToFitChoice || e.target == this._scaleChoice) {
      this.dispatchSettingsChange({
        shrinkToFit: this._shrinkToFitChoice.checked,
      });
      return;
    }

    window.clearTimeout(this.invalidTimeoutId);

    if (this._percentScale.checkValidity() && e.type == "input") {
      // TODO: set the customError element to hidden ( Bug 1656057 )

      this.invalidTimeoutId = window.setTimeout(() => {
        this.dispatchSettingsChange({
          scaling: Number(this._percentScale.value / 100),
        });
      }, INVALID_INPUT_DELAY_MS);
    }
    // TODO: populate a customError element with erorMessage contents
  }
}
customElements.define("scale-input", ScaleInput);

class PageRangeInput extends PrintUIControlMixin(HTMLElement) {
  get templateId() {
    return "page-range-template";
  }

  update(settings) {
    let rangePicker = this.querySelector("#range-picker");
    rangePicker.value = settings.printAllOrCustomRange;
  }

  handleEvent(e) {
    this.dispatchSettingsChange({
      printAllOrCustomRange: e.target.value,
    });
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
    if (this.hidden) {
      document.l10n.translateElements([this]).then(() => (this.hidden = false));
    }
  }

  handleEvent(e) {
    let { numPages } = e.detail;
    this.numPages = numPages;
    this.render();
  }
}
customElements.define("page-count", PageCount);
