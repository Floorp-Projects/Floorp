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
    this.appendChild(MozXULElement.parseXULToFragment(`
      <button id="print-preview-print" label="&print.label;" accesskey="&print.accesskey;" oncommand="this.parentNode.print();" icon="print"/>
      <button id="print-preview-pageSetup" label="&pageSetup.label;" accesskey="&pageSetup.accesskey;" oncommand="this.parentNode.doPageSetup();"/>
      <vbox align="center" pack="center">
        <label value="&page.label;" accesskey="&page.accesskey;" control="print-preview-pageNumber"/>
      </vbox>
      <toolbarbutton id="print-preview-navigateHome" class="print-preview-navigate-button tabbable" oncommand="parentNode.navigate(0, 0, 'home');" tooltiptext="&homearrow.tooltip;"/>
      <toolbarbutton id="print-preview-navigatePrevious" class="print-preview-navigate-button tabbable" oncommand="parentNode.navigate(-1, 0, 0);" tooltiptext="&previousarrow.tooltip;"/>
      <hbox align="center" pack="center">
        <textbox id="print-preview-pageNumber" value="1" min="1" type="number" hidespinbuttons="true" onchange="navigate(0, this.valueNumber, 0);"/>
        <label value="&of.label;"/>
        <label id="print-preview-totalPages" value="1"/>
      </hbox>
      <toolbarbutton id="print-preview-navigateNext" class="print-preview-navigate-button tabbable" oncommand="parentNode.navigate(1, 0, 0);" tooltiptext="&nextarrow.tooltip;"/>
      <toolbarbutton id="print-preview-navigateEnd" class="print-preview-navigate-button tabbable" oncommand="parentNode.navigate(0, 0, 'end');" tooltiptext="&endarrow.tooltip;"/>
      <toolbarseparator class="toolbarseparator-primary"/>
      <vbox align="center" pack="center">
        <label id="print-preview-scale-label" value="&scale.label;" accesskey="&scale.accesskey;" control="print-preview-scale"/>
      </vbox>
      <hbox align="center" pack="center">
        <menulist id="print-preview-scale" crop="none" oncommand="parentNode.parentNode.scale(this.selectedItem.value);">
          <menupopup>
            <menuitem value="0.3" label="&p30.label;"/>
            <menuitem value="0.4" label="&p40.label;"/>
            <menuitem value="0.5" label="&p50.label;"/>
            <menuitem value="0.6" label="&p60.label;"/>
            <menuitem value="0.7" label="&p70.label;"/>
            <menuitem value="0.8" label="&p80.label;"/>
            <menuitem value="0.9" label="&p90.label;"/>
            <menuitem value="1" label="&p100.label;"/>
            <menuitem value="1.25" label="&p125.label;"/>
            <menuitem value="1.5" label="&p150.label;"/>
            <menuitem value="1.75" label="&p175.label;"/>
            <menuitem value="2" label="&p200.label;"/>
            <menuseparator/>
            <menuitem flex="1" value="ShrinkToFit" label="&ShrinkToFit.label;"/>
            <menuitem value="Custom" label="&Custom.label;"/>
          </menupopup>
        </menulist>
      </hbox>
      <toolbarseparator class="toolbarseparator-primary"/>
      <hbox align="center" pack="center">
        <toolbarbutton id="print-preview-portrait-button" label="&portrait.label;" checked="true" accesskey="&portrait.accesskey;" type="radio" group="orient" class="tabbable" oncommand="parentNode.parentNode.orient('portrait');"/>
        <toolbarbutton id="print-preview-landscape-button" label="&landscape.label;" accesskey="&landscape.accesskey;" type="radio" group="orient" class="tabbable" oncommand="parentNode.parentNode.orient('landscape');"/>
      </hbox>
      <toolbarseparator class="toolbarseparator-primary"/>
      <checkbox id="print-preview-simplify" label="&simplifyPage.label;" checked="false" disabled="true" accesskey="&simplifyPage.accesskey;" tooltiptext-disabled="&simplifyPage.disabled.tooltip;" tooltiptext-enabled="&simplifyPage.enabled.tooltip;" oncommand="this.parentNode.simplify();"/>
      <toolbarseparator class="toolbarseparator-primary"/>
      <button label="&close.label;" accesskey="&close.accesskey;" oncommand="PrintUtils.exitPrintPreview();" icon="close"/>
      <data id="print-preview-prompt-title" value="&customPrompt.title;"/>
    `, `
    <!DOCTYPE bindings [
      <!ENTITY % printPreviewDTD SYSTEM "chrome://global/locale/printPreview.dtd" >
      %printPreviewDTD;
    ]>`));

    this.mPrintButton = document.getElementById("print-preview-print");

    this.mPageSetupButton = document.getElementById("print-preview-pageSetup");

    this.mNavigateHomeButton = document.getElementById("print-preview-navigateHome");

    this.mNavigatePreviousButton = document.getElementById("print-preview-navigatePrevious");

    this.mPageTextBox = document.getElementById("print-preview-pageNumber");

    this.mNavigateNextButton = document.getElementById("print-preview-navigateNext");

    this.mNavigateEndButton = document.getElementById("print-preview-navigateEnd");

    this.mTotalPages = document.getElementById("print-preview-totalPages");

    this.mScaleCombobox = document.getElementById("print-preview-scale-label");

    this.mPortaitButton = document.getElementById("print-preview-portrait-button");

    this.mLandscapeButton = document.getElementById("print-preview-landscape-button");

    this.mSimplifyPageCheckbox = document.getElementById("print-preview-simplify");

    this.mSimplifyPageNotAllowed = this.mSimplifyPageCheckbox.disabled;

    this.mSimplifyPageToolbarSeparator = this.mSimplifyPageCheckbox.nextSibling;

    this.mPrintPreviewObs = "";

    this.mWebProgress = "";

    this.mPPBrowser = null;

    this.mMessageManager = null;
  }

  initialize(aPPBrowser) {
    let { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm", {});
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
    let { AppConstants } = ChromeUtils.import("resource://gre/modules/AppConstants.jsm", {});
    let useCompatCharacters = AppConstants.isPlatformAndVersionAtMost("win", "6.1");
    let leftEnd = useCompatCharacters ? "\u23EA" : "\u23EE";
    let rightEnd = useCompatCharacters ? "\u23E9" : "\u23ED";
    this.mNavigateHomeButton.label = ltr ? leftEnd : rightEnd;
    this.mNavigatePreviousButton.label = ltr ? "\u25C2" : "\u25B8";
    this.mNavigateNextButton.label = ltr ? "\u25B8" : "\u25C2";
    this.mNavigateEndButton.label = ltr ? rightEnd : leftEnd;
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
      this.mPageTextBox.valueNumber += aDirection;
      navType = nsIWebBrowserPrint.PRINTPREVIEW_GOTO_PAGENUM;
      pageNum = this.mPageTextBox.value; // TODO: back to valueNumber?
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
    let { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm", {});
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
    this.mSimplifyPageCheckbox.setAttribute("tooltiptext",
      this.mSimplifyPageCheckbox.getAttribute("tooltiptext-enabled"));
  }

  disableSimplifyPage() {
    this.mSimplifyPageNotAllowed = true;
    this.mSimplifyPageCheckbox.disabled = true;
    this.mSimplifyPageCheckbox.setAttribute("tooltiptext",
      this.mSimplifyPageCheckbox.getAttribute("tooltiptext-disabled"));
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
