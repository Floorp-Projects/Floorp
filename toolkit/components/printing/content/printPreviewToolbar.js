// This file is loaded into the browser window scope.
/* eslint-env mozilla/browser-window */

// -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*-

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

customElements.define("printpreview-toolbar", class PrintPreviewToolbar extends MozXULElement {
  constructor() {
    super();
    this.disconnectedCallback = this.disconnectedCallback.bind(this);
  }
  connectedCallback() {
    window.addEventListener("unload", this.disconnectedCallback, { once: true });

    MozXULElement.insertFTLIfNeeded("toolkit/printing/printPreview.ftl");
    this.appendChild(MozXULElement.parseXULToFragment(`
      <button id="print-preview-print" oncommand="this.parentNode.print();" data-l10n-id="printpreview-print"/>
      <button id="print-preview-pageSetup" oncommand="this.parentNode.doPageSetup();" data-l10n-id="printpreview-page-setup"/>
      <vbox align="center" pack="center">
        <label control="print-preview-pageNumber" data-l10n-id="printpreview-page"/>
      </vbox>
      <toolbarbutton id="print-preview-navigateHome" class="print-preview-navigate-button tabbable" oncommand="parentNode.navigate(0, 0, 'home');" data-l10n-id="printpreview-homearrow"/>
      <toolbarbutton id="print-preview-navigatePrevious" class="print-preview-navigate-button tabbable" oncommand="parentNode.navigate(-1, 0, 0);" data-l10n-id="printpreview-previousarrow"/>
      <hbox align="center" pack="center">
        <html:input id="print-preview-pageNumber" hidespinbuttons="true" type="number" value="1" min="1"/>
        <label data-l10n-id="printpreview-of"/>
        <label id="print-preview-totalPages" value="1"/>
      </hbox>
      <toolbarbutton id="print-preview-navigateNext" class="print-preview-navigate-button tabbable" oncommand="parentNode.navigate(1, 0, 0);" data-l10n-id="printpreview-nextarrow"/>
      <toolbarbutton id="print-preview-navigateEnd" class="print-preview-navigate-button tabbable" oncommand="parentNode.navigate(0, 0, 'end');" data-l10n-id="printpreview-endarrow"/>
      <toolbarseparator class="toolbarseparator-primary"/>
      <vbox align="center" pack="center">
        <label id="print-preview-scale-label" control="print-preview-scale" data-l10n-id="printpreview-scale"/>
      </vbox>
      <hbox align="center" pack="center">
        <menulist id="print-preview-scale" crop="none" oncommand="parentNode.parentNode.scale(this.selectedItem.value);">
          <menupopup>
            <menuitem value="0.3" />
            <menuitem value="0.4" />
            <menuitem value="0.5" />
            <menuitem value="0.6" />
            <menuitem value="0.7" />
            <menuitem value="0.8" />
            <menuitem value="0.9" />
            <menuitem value="1" />
            <menuitem value="1.25" />
            <menuitem value="1.5" />
            <menuitem value="1.75" />
            <menuitem value="2" />
            <menuseparator/>
            <menuitem flex="1" value="ShrinkToFit" data-l10n-id="printpreview-shrink-to-fit"/>
            <menuitem value="Custom" data-l10n-id="printpreview-custom"/>
          </menupopup>
        </menulist>
      </hbox>
      <toolbarseparator class="toolbarseparator-primary"/>
      <hbox align="center" pack="center">
        <toolbarbutton id="print-preview-portrait-button" checked="true" type="radio" group="orient" class="tabbable" oncommand="parentNode.parentNode.orient('portrait');" data-l10n-id="printpreview-portrait"/>
        <toolbarbutton id="print-preview-landscape-button" type="radio" group="orient" class="tabbable" oncommand="parentNode.parentNode.orient('landscape');" data-l10n-id="printpreview-landscape"/>
      </hbox>
      <toolbarseparator class="toolbarseparator-primary"/>
      <checkbox id="print-preview-simplify" checked="false" disabled="true" oncommand="this.parentNode.simplify();" data-l10n-id="printpreview-simplify-page-checkbox"/>
      <toolbarseparator class="toolbarseparator-primary"/>
      <button id="print-preview-toolbar-close-button" oncommand="PrintUtils.exitPrintPreview();" data-l10n-id="printpreview-close"/>
      <data id="print-preview-prompt-title" data-l10n-id="printpreview-custom-prompt"/>
        `)
    );




    this.mPrintButton = document.getElementById("print-preview-print");

    this.mPageSetupButton = document.getElementById("print-preview-pageSetup");

    this.mNavigateHomeButton = document.getElementById("print-preview-navigateHome");

    this.mNavigatePreviousButton = document.getElementById("print-preview-navigatePrevious");

    this.mPageTextBox = document.getElementById("print-preview-pageNumber");

    this.mNavigateNextButton = document.getElementById("print-preview-navigateNext");

    this.mNavigateEndButton = document.getElementById("print-preview-navigateEnd");

    this.mTotalPages = document.getElementById("print-preview-totalPages");

    this.mScaleCombobox = document.getElementById("print-preview-scale");

    this.mPortaitButton = document.getElementById("print-preview-portrait-button");

    this.mLandscapeButton = document.getElementById("print-preview-landscape-button");

    this.mSimplifyPageCheckbox = document.getElementById("print-preview-simplify");

    this.mSimplifyPageNotAllowed = this.mSimplifyPageCheckbox.disabled;

    this.mSimplifyPageToolbarSeparator = this.mSimplifyPageCheckbox.nextElementSibling;

    this.mPrintPreviewObs = "";

    this.mWebProgress = "";

    this.mPPBrowser = null;

    this.mMessageManager = null;

    this.mOnPageTextBoxChange = () => {
      this.navigate(0, Number(this.mPageTextBox.value), 0);
    };
    this.mPageTextBox.addEventListener("change", this.mOnPageTextBoxChange);


    let dropdown = document.getElementById("print-preview-scale").menupopup;
    for (let menuitem of dropdown.children) {
      let value = menuitem.getAttribute("value");
      if (!isNaN(parseFloat(value))) {
        document.l10n.setAttributes(menuitem, "printpreview-percentage-value",
            {"percent": Math.round(parseFloat(value) * 100)});
      }
    }
  }

  initialize(aPPBrowser) {
    let { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
    if (!Services.prefs.getBoolPref("print.use_simplify_page")) {
      this.mSimplifyPageCheckbox.hidden = true;
      this.mSimplifyPageToolbarSeparator.hidden = true;
    }
    this.mPPBrowser = aPPBrowser;
    this.mMessageManager = aPPBrowser.messageManager;
    this.mMessageManager.addMessageListener("Printing:Preview:UpdatePageCount", this);
    this.updateToolbar();

    let ltr = document.documentElement.matches(":root:-moz-locale-dir(ltr)");
    // Windows 7 doesn't support ⏮ and ⏭ by default, and fallback doesn't
    // always work (bug 1343330).
    let { AppConstants } = ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
    let useCompatCharacters = AppConstants.isPlatformAndVersionAtMost("win", "6.1");
    let leftEnd = useCompatCharacters ? "\u23EA" : "\u23EE";
    let rightEnd = useCompatCharacters ? "\u23E9" : "\u23ED";
    document.l10n.setAttributes(this.mNavigateHomeButton, "printpreview-homearrow", {"arrow": (ltr ? leftEnd : rightEnd)});
    document.l10n.setAttributes(this.mNavigatePreviousButton, "printpreview-previousarrow", {"arrow": (ltr ? "\u25C2" : "\u25B8")});
    document.l10n.setAttributes(this.mNavigateNextButton, "printpreview-nextarrow", {"arrow": (ltr ? "\u25B8" : "\u25C2")});
    document.l10n.setAttributes(this.mNavigateEndButton, "printpreview-endarrow", {"arrow": (ltr ? rightEnd : leftEnd)});
  }

  destroy() {
    if (this.mMessageManager) {
      this.mMessageManager.removeMessageListener("Printing:Preview:UpdatePageCount", this);
      delete this.mMessageManager;
      delete this.mPPBrowser;
    }
  }

  disconnectedCallback() {
    window.removeEventListener("unload", this.disconnectedCallback);
    this.mPageTextBox.removeEventListener("change", this.mOnPageTextBoxChange);
    this.destroy();
  }

  disableUpdateTriggers(aDisabled) {
    this.mPrintButton.disabled = aDisabled;
    this.mPageSetupButton.disabled = aDisabled;
    this.mNavigateHomeButton.disabled = aDisabled;
    this.mNavigatePreviousButton.disabled = aDisabled;
    this.mPageTextBox.disabled = aDisabled;
    this.mNavigateNextButton.disabled = aDisabled;
    this.mNavigateEndButton.disabled = aDisabled;
    this.mScaleCombobox.disabled = aDisabled;
    this.mPortaitButton.disabled = aDisabled;
    this.mLandscapeButton.disabled = aDisabled;
    this.mSimplifyPageCheckbox.disabled = this.mSimplifyPageNotAllowed || aDisabled;
  }

  doPageSetup() {
    /* import-globals-from printUtils.js */
    var didOK = PrintUtils.showPageSetup();
    if (didOK) {
      // the changes that effect the UI
      this.updateToolbar();

      // Now do PrintPreview
      PrintUtils.printPreview();
    }
  }

  navigate(aDirection, aPageNum, aHomeOrEnd) {
    const nsIWebBrowserPrint = Ci.nsIWebBrowserPrint;
    let navType, pageNum;

    // we use only one of aHomeOrEnd, aDirection, or aPageNum
    if (aHomeOrEnd) {
      // We're going to either the very first page ("home"), or the
      // very last page ("end").
      if (aHomeOrEnd == "home") {
        navType = nsIWebBrowserPrint.PRINTPREVIEW_HOME;
        this.mPageTextBox.value = 1;
      } else {
        navType = nsIWebBrowserPrint.PRINTPREVIEW_END;
        this.mPageTextBox.value = this.mPageTextBox.max;
      }
      pageNum = 0;
    } else if (aDirection) {
      // aDirection is either +1 or -1, and allows us to increment
      // or decrement our currently viewed page.
      this.mPageTextBox.value = Number(this.mPageTextBox.value) + aDirection;
      navType = nsIWebBrowserPrint.PRINTPREVIEW_GOTO_PAGENUM;
      pageNum = this.mPageTextBox.value;
    } else {
      // We're going to a specific page (aPageNum)
      navType = nsIWebBrowserPrint.PRINTPREVIEW_GOTO_PAGENUM;
      pageNum = aPageNum;
    }

    this.mMessageManager.sendAsyncMessage("Printing:Preview:Navigate", {
      navType,
      pageNum,
    });
  }

  print() {
    PrintUtils.printWindow(this.mPPBrowser.outerWindowID, this.mPPBrowser);
  }

  promptForScaleValue(aValue) {
    var value = Math.round(aValue);
    var promptStr = document.getElementById("print-preview-scale-label").value;
    var renameTitle = document.getElementById("print-preview-prompt-title");
    var result = { value };
    let { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
    var confirmed = Services.prompt.prompt(window, renameTitle, promptStr, result, null, { value });
    if (!confirmed || (!result.value) || (result.value == "")) {
      return -1;
    }
    return result.value;
  }

  setScaleCombobox(aValue) {
    var scaleValues = [0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1, 1.25, 1.5, 1.75, 2];

    aValue = Number(aValue);

    for (var i = 0; i < scaleValues.length; i++) {
      if (aValue == scaleValues[i]) {
        this.mScaleCombobox.selectedIndex = i;
        return;
      }
    }
    this.mScaleCombobox.value = "Custom";
  }

  scale(aValue) {
    var settings = PrintUtils.getPrintSettings();
    if (aValue == "ShrinkToFit") {
      if (!settings.shrinkToFit) {
        settings.shrinkToFit = true;
        this.savePrintSettings(settings, settings.kInitSaveShrinkToFit | settings.kInitSaveScaling);
        PrintUtils.printPreview();
      }
      return;
    }

    if (aValue == "Custom") {
      aValue = this.promptForScaleValue(settings.scaling * 100.0);
      if (aValue >= 10) {
        aValue /= 100.0;
      } else {
        if (this.mScaleCombobox.hasAttribute("lastValidInx")) {
          this.mScaleCombobox.selectedIndex = this.mScaleCombobox.getAttribute("lastValidInx");
        }
        return;
      }
    }

    this.setScaleCombobox(aValue);
    this.mScaleCombobox.setAttribute("lastValidInx", this.mScaleCombobox.selectedIndex);

    if (settings.scaling != aValue || settings.shrinkToFit) {
      settings.shrinkToFit = false;
      settings.scaling = aValue;
      this.savePrintSettings(settings, settings.kInitSaveShrinkToFit | settings.kInitSaveScaling);
      PrintUtils.printPreview();
    }
  }

  orient(aOrientation) {
    const kIPrintSettings = Ci.nsIPrintSettings;
    var orientValue = (aOrientation == "portrait") ? kIPrintSettings.kPortraitOrientation :
      kIPrintSettings.kLandscapeOrientation;
    var settings = PrintUtils.getPrintSettings();
    if (settings.orientation != orientValue) {
      settings.orientation = orientValue;
      this.savePrintSettings(settings, settings.kInitSaveOrientation);
      PrintUtils.printPreview();
    }
  }

  simplify() {
    PrintUtils.setSimplifiedMode(this.mSimplifyPageCheckbox.checked);
    PrintUtils.printPreview();
  }

  enableSimplifyPage() {
    this.mSimplifyPageNotAllowed = false;
    this.mSimplifyPageCheckbox.disabled = false;
    document.l10n.setAttributes(this.mSimplifyPageCheckbox, "printpreview-simplify-page-checkbox-enabled");
  }

  disableSimplifyPage() {
    this.mSimplifyPageNotAllowed = true;
    this.mSimplifyPageCheckbox.disabled = true;
    document.l10n.setAttributes(this.mSimplifyPageCheckbox, "printpreview-simplify-page-checkbox");
  }

  updateToolbar() {
    var settings = PrintUtils.getPrintSettings();

    var isPortrait = settings.orientation == Ci.nsIPrintSettings.kPortraitOrientation;

    this.mPortaitButton.checked = isPortrait;
    this.mLandscapeButton.checked = !isPortrait;

    if (settings.shrinkToFit) {
      this.mScaleCombobox.value = "ShrinkToFit";
    } else {
      this.setScaleCombobox(settings.scaling);
    }

    this.mPageTextBox.value = 1;
  }

  savePrintSettings(settings, flags) {
    var PSSVC = Cc["@mozilla.org/gfx/printsettings-service;1"]
      .getService(Ci.nsIPrintSettingsService);
    PSSVC.savePrintSettingsToPrefs(settings, true, flags);
  }

  receiveMessage(message) {
    if (message.name == "Printing:Preview:UpdatePageCount") {
      let numPages = message.data.numPages;
      this.mTotalPages.value = numPages;
      this.mPageTextBox.max = numPages;
    }
  }
}, { extends: "toolbar" });
