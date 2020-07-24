/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { gBrowser, PrintUtils } = window.docShell.chromeEventHandler.ownerGlobal;

const PrintUI = {
  initialize(sourceBrowser) {
    this.sourceBrowser = sourceBrowser;

    this.form = document.querySelector("form#print");
    this._openDialogLink = document.querySelector("#open-dialog-link");
    this._settingsSection = document.querySelector("#settings");
    this._printerPicker = document.querySelector("#printer-picker");
    this._sheetsCount = document.querySelector("#sheets-count");

    this.form.addEventListener("submit", this);
    this._openDialogLink.addEventListener("click", event => {
      event.preventDefault();
      PrintUtils.printWindow(sourceBrowser.browsingContext);
    });
    this._printerPicker.addEventListener("change", this);
    this._settingsSection.addEventListener("change", this);

    this.printDestinations = [];
    this.printSettings = PrintUtils.getPrintSettings();
    // TODO: figure out where this data comes from
    this.numSheets = 1;

    this.render();
  },

  render() {
    console.log(
      "TODO: populate the UI with printer listing with printDestinations:",
      this.printDestinations
    );
    //  TODO: populate the settings controls with an nsIPrintSettings

    document.l10n.setAttributes(
      this._sheetsCount,
      this._sheetsCount.getAttribute("data-deferred-l10n-id"),
      {
        sheetCount: this.numSheets,
      }
    );
  },

  handleEvent(event) {
    if (event.type == "submit" && event.target == this.form) {
      event.preventDefault();
      switch (event.submitter.name) {
        case "print":
          PrintUtils.printWindow(this.sourceBrowser.browsingContext, {
            printSilent: true,
            printerName: this._printerPicker.value,
          });
          break;
        case "cancel":
          window.close();
          break;
      }
    }
    /* TODO:
     * handle clicks to the system dialog link
     * handle change of the selected printer/destination
     * handle changes from each of the print settings controls
     */
  },
};

function getSourceBrowser() {
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
}

document.addEventListener("DOMContentLoaded", e => {
  let sourceBrowser = getSourceBrowser();
  if (sourceBrowser) {
    PrintUI.initialize(sourceBrowser);
  } else {
    console.error("No source browser");
  }
});

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
    }
    render() {}
  };
}

class DestinationPicker extends HTMLSelectElement {
  setOptions(optionValues = []) {
    console.log("DestinationPicker, setOptions:", optionValues);
    this.textContent = "";
    for (let optionData of optionValues) {
      let opt = new Option(
        optionData.name,
        "value" in optionData ? optionData.value : optionData.name
      );
      if (optionData.selected) {
        opt.selected = true;
      }
      this.options.add(opt);
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

  constructor() {
    super();
    this._orientation = null;
  }

  render() {
    console.log(
      "TODO: populate/set orientation state from the current print settings"
    );
  }
}
customElements.define("orientation-input", OrientationInput);

class ScaleInput extends PrintUIControlMixin(HTMLElement) {
  get templateId() {
    return "scale-template";
  }

  render() {
    console.log(
      "TODO: populate/set print scale state from the current print settings"
    );
  }
}
customElements.define("scale-input", ScaleInput);

class PageRangeInput extends PrintUIControlMixin(HTMLElement) {
  get templateId() {
    return "page-range-template";
  }

  render() {
    console.log(
      "TODO: populate/set page-range state from the current print settings"
    );
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

  handleEvent(event) {
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
