/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { gBrowser, PrintUtils } = window.docShell.chromeEventHandler.ownerGlobal;

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
    this.settings = PrintUtils.getPrintSettings();
    this.updatePrintPreview();

    document.addEventListener("print", e => this.print({ silent: true }));
    document.addEventListener("update-print-setting", e =>
      this.updateSetting(e.detail)
    );
    document.addEventListener("cancel-print", () => this.cancelPrint());
    document.addEventListener("open-system-dialog", () =>
      this.print({ silent: false })
    );
    document.dispatchEvent(
      new CustomEvent("available-destinations", {
        detail: this.getPrintDestinations(),
      })
    );

    this.settingValues = {
      printRange: printRange =>
        printRange == "all"
          ? Ci.nsIPrintSettings.kRangeAllPages
          : Ci.nsIPrintSettings.kRangeSpecifiedPageRange,
      // TODO: There's also kRangeSelection, which should come into play
      // once we have a text box where the user can specify a range
    };
    this.settingFlags = {
      orientation: Ci.nsIPrintSettings.kInitSaveOrientation,
      printerName: Ci.nsIPrintSettings.kInitSaveAll,
      scaling: Ci.nsIPrintSettings.kInitSaveScaling,
      shrinkToFit: Ci.nsIPrintSettings.kInitSaveShrinkToFit,
    };

    document.dispatchEvent(
      new CustomEvent("print-settings", {
        detail: this.settings,
      })
    );
  },

  print({ printerName, silent } = {}) {
    let settings = this.settings;
    if (silent) {
      settings.printSilent = true;
    }
    if (printerName) {
      settings.printerName = printerName;
    }
    PrintUtils.printWindow(this.sourceBrowser.browsingContext, settings);
  },

  cancelPrint() {
    window.close();
  },

  updateSetting({ setting, value }) {
    let settingValue =
      setting in this.settingValues
        ? this.settingValues[setting](value)
        : value;

    if (this.settings[setting] != settingValue) {
      this.settings[setting] = settingValue;

      let PSSVC = Cc["@mozilla.org/gfx/printsettings-service;1"].getService(
        Ci.nsIPrintSettingsService
      );

      let flag = this.settingFlags[setting];
      if (flag) {
        PSSVC.savePrintSettingsToPrefs(this.settings, true, flag);
      }

      this.updatePrintPreview();

      document.dispatchEvent(
        new CustomEvent("print-settings", {
          detail: this.settings,
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

  getPrintDestinations() {
    let printerList = Cc["@mozilla.org/gfx/printerlist;1"].createInstance(
      Ci.nsIPrinterList
    );
    let currentPrinterName = PrintUtils._getLastUsedPrinterName();
    let destinations = printerList.printers.map(printer => {
      return {
        name: printer.name,
        value: printer.name,
        selected: printer.name == currentPrinterName,
      };
    });
    return destinations;
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

    updateSetting(setting, value) {
      this.dispatchEvent(
        new CustomEvent("update-print-setting", {
          bubbles: true,
          detail: { setting, value },
        })
      );
    }

    handleEvent(event) {}
  };
}

class DestinationPicker extends HTMLSelectElement {
  constructor() {
    super();
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
      this.dispatchEvent(
        new CustomEvent("update-print-setting", {
          bubbles: true,
          detail: {
            setting: "printerName",
            value: e.target.value,
          },
        })
      );
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
    this.updateSetting("orientation", e.target.value);
  }
}
customElements.define("orientation-input", OrientationInput);

class CopiesInput extends PrintUIControlMixin(HTMLElement) {
  get templateId() {
    return "copy-count-template";
  }

  update(settings) {
    this.querySelector("input").value = settings.numCopies;
  }

  handleEvent(e) {
    this.updateSetting("numCopies", e.target.value);
  }
}
customElements.define("copy-count-input", CopiesInput);

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
    this.addEventListener("input", this);
    this._percentScale = this.querySelector("#percent-scale");
    this._shrinkToFit = this.querySelector("#fit-choice");
  }

  update(settings) {
    // TODO: .scaling only goes from 0-1. Need validation mechanism
    let { scaling, shrinkToFit } = settings;
    this._shrinkToFit.checked = shrinkToFit;
    this.querySelector("#percent-scale-choice").checked = !shrinkToFit;
    this._percentScale.disabled = shrinkToFit;

    // Only allow whole numbers. 0.14 * 100 would have decimal places, etc.
    this._percentScale.value = parseInt(scaling * 100, 10);
  }

  handleEvent(e) {
    this.updateSetting("shrinkToFit", this._shrinkToFit.checked);
    this.updateSetting("scaling", this._percentScale.value / 100);
  }
}
customElements.define("scale-input", ScaleInput);

class PageRangeInput extends PrintUIControlMixin(HTMLElement) {
  get templateId() {
    return "page-range-template";
  }

  update(settings) {
    let rangePicker = document.querySelector("#range-picker");
    rangePicker.value = settings.printRange == 0 ? "all" : "custom";
  }

  handleEvent(e) {
    this.updateSetting("printRange", e.target.value);
  }
}
customElements.define("page-range-input", PageRangeInput);

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

class PageCount extends HTMLParagraphElement {
  constructor() {
    super();
    document.addEventListener("page-count", this);
  }

  handleEvent(e) {
    let { numPages } = e.detail;
    document.l10n.setAttributes(this, "printui-sheets-count", {
      sheetCount: numPages,
    });
    if (this.hidden) {
      document.l10n.translateElements([this]).then(() => (this.hidden = false));
    }
  }
}
customElements.define("page-count", PageCount, { extends: "p" });
