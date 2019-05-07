// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["RFPHelper"];

const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

const kPrefResistFingerprinting = "privacy.resistFingerprinting";
const kPrefSpoofEnglish = "privacy.spoof_english";
const kTopicHttpOnModifyRequest = "http-on-modify-request";

const kPrefLetterboxing = "privacy.resistFingerprinting.letterboxing";
const kPrefLetterboxingDimensions =
  "privacy.resistFingerprinting.letterboxing.dimensions";
const kPrefLetterboxingTesting =
  "privacy.resistFingerprinting.letterboxing.testing";
const kTopicDOMWindowOpened = "domwindowopened";
const kEventLetterboxingSizeUpdate = "Letterboxing:ContentSizeUpdated";

var logConsole;
function log(msg) {
  if (!logConsole) {
    logConsole = console.createInstance({
      prefix: "RFPHelper.jsm",
      maxLogLevelPref: "privacy.resistFingerprinting.jsmloglevel",
    });
  }

  logConsole.log(msg);
}

class _RFPHelper {
  // ============================================================================
  // Shared Setup
  // ============================================================================
  constructor() {
    this._initialized = false;
  }

  init() {
    if (this._initialized) {
      return;
    }
    this._initialized = true;

    // Add unconditional observers
    Services.prefs.addObserver(kPrefResistFingerprinting, this);
    Services.prefs.addObserver(kPrefLetterboxing, this);
    XPCOMUtils.defineLazyPreferenceGetter(this, "_letterboxingDimensions",
      kPrefLetterboxingDimensions, "", null, this._parseLetterboxingDimensions);
    XPCOMUtils.defineLazyPreferenceGetter(this, "_isLetterboxingTesting",
      kPrefLetterboxingTesting, false);

    // Add RFP and Letterboxing observers if prefs are enabled
    this._handleResistFingerprintingChanged();
    this._handleLetterboxingPrefChanged();
  }

  uninit() {
    if (!this._initialized) {
      return;
    }
    this._initialized = false;

    // Remove unconditional observers
    Services.prefs.removeObserver(kPrefResistFingerprinting, this);
    Services.prefs.removeObserver(kPrefLetterboxing, this);
    // Remove the RFP observers, swallowing exceptions if they weren't present
    this._removeRFPObservers();
  }

  observe(subject, topic, data) {
    switch (topic) {
      case "nsPref:changed":
        this._handlePrefChanged(data);
        break;
      case kTopicHttpOnModifyRequest:
        this._handleHttpOnModifyRequest(subject, data);
        break;
      case kTopicDOMWindowOpened:
        // We attach to the newly created window by adding tabsProgressListener
        // and event listener on it. We listen for new tabs being added or
        // the change of the content principal and apply margins accordingly.
        this._handleDOMWindowOpened(subject);
        break;
      default:
        break;
    }
  }

  handleEvent(aMessage) {
    switch (aMessage.type) {
      case "TabOpen":
      {
        let tab = aMessage.target;
        this._addOrClearContentMargin(tab.linkedBrowser);
        break;
      }
      default:
        break;
    }
  }

  receiveMessage(aMessage) {
    switch (aMessage.name) {
      case kEventLetterboxingSizeUpdate:
        let win = aMessage.target.ownerGlobal;
        this._updateMarginsForTabsInWindow(win);
        break;
      default:
        break;
    }
  }

  _handlePrefChanged(data) {
    switch (data) {
      case kPrefResistFingerprinting:
        this._handleResistFingerprintingChanged();
        break;
      case kPrefSpoofEnglish:
        this._handleSpoofEnglishChanged();
        break;
      case kPrefLetterboxing:
        this._handleLetterboxingPrefChanged();
        break;
      default:
        break;
    }
  }

  // ============================================================================
  // Language Prompt
  // ============================================================================
  _addRFPObservers() {
    Services.prefs.addObserver(kPrefSpoofEnglish, this);
    if (this._shouldPromptForLanguagePref()) {
      Services.obs.addObserver(this, kTopicHttpOnModifyRequest);
    }
  }

  _removeRFPObservers() {
    try {
      Services.pref.removeObserver(kPrefSpoofEnglish, this);
    } catch (e) {
      // do nothing
    }
    try {
      Services.obs.removeObserver(this, kTopicHttpOnModifyRequest);
    } catch (e) {
      // do nothing
    }
  }

  _handleResistFingerprintingChanged() {
    if (Services.prefs.getBoolPref(kPrefResistFingerprinting)) {
      this._addRFPObservers();
    } else {
      this._removeRFPObservers();
    }
  }

  _handleSpoofEnglishChanged() {
    switch (Services.prefs.getIntPref(kPrefSpoofEnglish)) {
      case 0: // will prompt
        // This should only happen when turning privacy.resistFingerprinting off.
        // Works like disabling accept-language spoofing.
      case 1: // don't spoof
        if (Services.prefs.prefHasUserValue("javascript.use_us_english_locale")) {
          Services.prefs.clearUserPref("javascript.use_us_english_locale");
        }
        // We don't reset intl.accept_languages. Instead, setting
        // privacy.spoof_english to 1 allows user to change preferred language
        // settings through Preferences UI.
        break;
      case 2: // spoof
        Services.prefs.setCharPref("intl.accept_languages", "en-US, en");
        Services.prefs.setBoolPref("javascript.use_us_english_locale", true);
        break;
      default:
        break;
    }
  }

  _shouldPromptForLanguagePref() {
    return (Services.locale.appLocaleAsLangTag.substr(0, 2) !== "en")
      && (Services.prefs.getIntPref(kPrefSpoofEnglish) === 0);
  }

  _handleHttpOnModifyRequest(subject, data) {
    // If we are loading an HTTP page from content, show the
    // "request English language web pages?" prompt.
    let httpChannel;
    try {
      httpChannel = subject.QueryInterface(Ci.nsIHttpChannel);
    } catch (e) {
      return;
    }

    if (!httpChannel) {
      return;
    }

    let notificationCallbacks = httpChannel.notificationCallbacks;
    if (!notificationCallbacks) {
      return;
    }

    let loadContext = notificationCallbacks.getInterface(Ci.nsILoadContext);
    if (!loadContext || !loadContext.isContent) {
      return;
    }

    if (!subject.URI.schemeIs("http") && !subject.URI.schemeIs("https")) {
      return;
    }
    // The above QI did not throw, the scheme is http[s], and we know the
    // load context is content, so we must have a true HTTP request from content.
    // Stop the observer and display the prompt if another window has
    // not already done so.
    Services.obs.removeObserver(this, kTopicHttpOnModifyRequest);

    if (!this._shouldPromptForLanguagePref()) {
      return;
    }

    this._promptForLanguagePreference();

    // The Accept-Language header for this request was set when the
    // channel was created. Reset it to match the value that will be
    // used for future requests.
    let val = this._getCurrentAcceptLanguageValue(subject.URI);
    if (val) {
      httpChannel.setRequestHeader("Accept-Language", val, false);
    }
  }

  _promptForLanguagePreference() {
    // Display two buttons, both with string titles.
    let flags = Services.prompt.STD_YES_NO_BUTTONS;
    let brandBundle = Services.strings.createBundle(
      "chrome://branding/locale/brand.properties");
    let brandShortName = brandBundle.GetStringFromName("brandShortName");
    let navigatorBundle = Services.strings.createBundle(
      "chrome://browser/locale/browser.properties");
    let message = navigatorBundle.formatStringFromName(
      "privacy.spoof_english", [brandShortName], 1);
    let response = Services.prompt.confirmEx(
      null, "", message, flags, null, null, null, null, {value: false});

    // Update preferences to reflect their response and to prevent the prompt
    // from being displayed again.
    Services.prefs.setIntPref(kPrefSpoofEnglish, (response == 0) ? 2 : 1);
  }

  _getCurrentAcceptLanguageValue(uri) {
    let channel = Services.io.newChannelFromURI(
        uri,
        null, // aLoadingNode
        Services.scriptSecurityManager.getSystemPrincipal(),
        null, // aTriggeringPrincipal
        Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
        Ci.nsIContentPolicy.TYPE_OTHER);
    let httpChannel;
    try {
      httpChannel = channel.QueryInterface(Ci.nsIHttpChannel);
    } catch (e) {
      return null;
    }
    return httpChannel.getRequestHeader("Accept-Language");
  }

  // ==============================================================================
  // Letterboxing
  // ============================================================================
  /**
   * We use the TabsProgressListener to catch the change of the content
   * principal. We would clear the margins around the content viewport if
   * it is the system principal.
   */
  onLocationChange(aBrowser) {
    this._addOrClearContentMargin(aBrowser);
  }

  _handleLetterboxingPrefChanged() {
    if (Services.prefs.getBoolPref(kPrefLetterboxing, false)) {
      Services.ww.registerNotification(this);
      this._attachAllWindows();
    } else {
      this._detachAllWindows();
      Services.ww.unregisterNotification(this);
    }
  }

  // The function to parse the dimension set from the pref value. The pref value
  // should be formated as 'width1xheight1, width2xheight2, ...'. For
  // example, '100x100, 200x200, 400x200 ...'.
  _parseLetterboxingDimensions(aPrefValue) {
    if (!aPrefValue || !aPrefValue.match(/^(?:\d+x\d+,\s*)*(?:\d+x\d+)$/)) {
      if (aPrefValue) {
        Cu.reportError(`Invalid pref value for ${kPrefLetterboxingDimensions}: ${aPrefValue}`);
      }
      return [];
    }

    return aPrefValue.split(",").map(item => {
      let sizes = item.split("x").map(size => parseInt(size, 10));

      return {
        width: sizes[0],
        height: sizes[1],
      };
    });
  }

  _addOrClearContentMargin(aBrowser) {
    let tab = aBrowser.getTabBrowser()
                      .getTabForBrowser(aBrowser);

    // We won't do anything for lazy browsers.
    if (!aBrowser.isConnected) {
      return;
    }

    // We should apply no margin around an empty tab or a tab with system
    // principal.
    if (tab.isEmpty || aBrowser.contentPrincipal.isSystemPrincipal) {
      this._clearContentViewMargin(aBrowser);
    } else {
      this._roundContentView(aBrowser);
    }
  }

  /**
   * Given a width or height, returns the appropriate margin to apply.
   */
  steppedRange(aDimension) {
    let stepping;
    if (aDimension <= 50) {
      return 0;
    } else if (aDimension <= 500) {
      stepping = 50;
    } else if (aDimension <= 1600) {
      stepping = 100;
    } else {
      stepping = 200;
    }

    return (aDimension % stepping) / 2;
  }

  /**
   * The function will round the given browser by adding margins around the
   * content viewport.
   */
  async _roundContentView(aBrowser) {
    let logId = Math.random();
    log("_roundContentView[" + logId + "]");
    let win = aBrowser.ownerGlobal;
    let browserContainer = aBrowser.getTabBrowser()
                                   .getBrowserContainer(aBrowser);

    let {contentWidth, contentHeight, containerWidth, containerHeight} =
      await win.promiseDocumentFlushed(() => {
        let contentWidth = aBrowser.clientWidth;
        let contentHeight = aBrowser.clientHeight;
        let containerWidth = browserContainer.clientWidth;
        let containerHeight = browserContainer.clientHeight;

        return {
          contentWidth,
          contentHeight,
          containerWidth,
          containerHeight,
        };
      });

    log("_roundContentView[" + logId + "] contentWidth=" + contentWidth + " contentHeight=" + contentHeight +
      " containerWidth=" + containerWidth + " containerHeight=" + containerHeight + " ");

    let calcMargins = (aWidth, aHeight) => {
      let result;
      log("_roundContentView[" + logId + "] calcMargins(" + aWidth + ", " + aHeight + ")");
      // If the set is empty, we will round the content with the default
      // stepping size.
      if (!this._letterboxingDimensions.length) {
        result = {
          width: this.steppedRange(aWidth),
          height: this.steppedRange(aHeight),
        };
        log("_roundContentView[" + logId + "] calcMargins(" + aWidth + ", " + aHeight + ") = " + result.width + " x " + result.height);
        return result;
      }

      let matchingArea = aWidth * aHeight;
      let minWaste = Number.MAX_SAFE_INTEGER;
      let targetDimensions = undefined;

      // Find the desired dimensions which waste the least content area.
      for (let dim of this._letterboxingDimensions) {
        // We don't need to consider the dimensions which cannot fit into the
        // real content size.
        if (dim.width > aWidth || dim.height > aHeight) {
          continue;
        }

        let waste = matchingArea - dim.width * dim.height;

        if (waste >= 0 && waste < minWaste) {
          targetDimensions = dim;
          minWaste = waste;
        }
      }

      // If we cannot find any dimensions match to the real content window, this
      // means the content area is smaller the smallest size in the set. In this
      // case, we won't apply any margins.
      if (!targetDimensions) {
        result = {
          width: 0,
          height: 0,
        };
      } else {
        result = {
          width: (aWidth - targetDimensions.width) / 2,
          height: (aHeight - targetDimensions.height) / 2,
        };
      }

      log("_roundContentView[" + logId + "] calcMargins(" + aWidth + ", " + aHeight + ") = " + result.width + " x " + result.height);
      return result;
    };

    // Calculating the margins around the browser element in order to round the
    // content viewport. We will use a 200x100 stepping if the dimension set
    // is not given.
    let margins = calcMargins(containerWidth, containerHeight);

    // If the size of the content is already quantized, we do nothing.
    if (aBrowser.style.margin == `${margins.height}px ${margins.width}px`) {
      log("_roundContentView[" + logId + "] is_rounded == true");
      if (this._isLetterboxingTesting) {
        log("_roundContentView[" + logId + "] is_rounded == true test:letterboxing:update-margin-finish");
        Services.obs.notifyObservers(null, "test:letterboxing:update-margin-finish");
      }
      return;
    }

    win.requestAnimationFrame(() => {
      log("_roundContentView[" + logId + "] setting margins to " + margins.width + " x " + margins.height);
      // One cannot (easily) control the color of a margin unfortunately.
      // An initial attempt to use a border instead of a margin resulted
      // in offset event dispatching; so for now we use a colorless margin.
      aBrowser.style.margin = `${margins.height}px ${margins.width}px`;
    });
  }

  _clearContentViewMargin(aBrowser) {
    aBrowser.ownerGlobal.requestAnimationFrame(() => {
      aBrowser.style.margin = "";
    });
  }

  _updateMarginsForTabsInWindow(aWindow) {
    let tabBrowser = aWindow.gBrowser;

    for (let tab of tabBrowser.tabs) {
      let browser = tab.linkedBrowser;
      this._addOrClearContentMargin(browser);
    }
  }

  _attachWindow(aWindow) {
    aWindow.gBrowser
           .addTabsProgressListener(this);
    aWindow.addEventListener("TabOpen", this);
    aWindow.messageManager
           .addMessageListener(kEventLetterboxingSizeUpdate, this);

    // Rounding the content viewport.
    this._updateMarginsForTabsInWindow(aWindow);
  }

  _attachAllWindows() {
    let windowList = Services.wm.getEnumerator("navigator:browser");

    while (windowList.hasMoreElements()) {
      let win = windowList.getNext();

      if (win.closed || !win.gBrowser) {
        continue;
      }

      this._attachWindow(win);
    }
  }

  _detachWindow(aWindow) {
    let tabBrowser = aWindow.gBrowser;
    tabBrowser.removeTabsProgressListener(this);
    aWindow.removeEventListener("TabOpen", this);
    aWindow.messageManager
           .removeMessageListener(kEventLetterboxingSizeUpdate, this);

    // Clear all margins and tooltip for all browsers.
    for (let tab of tabBrowser.tabs) {
      let browser = tab.linkedBrowser;
      this._clearContentViewMargin(browser);
    }
  }

  _detachAllWindows() {
    let windowList = Services.wm.getEnumerator("navigator:browser");

    while (windowList.hasMoreElements()) {
      let win = windowList.getNext();

      if (win.closed || !win.gBrowser) {
        continue;
      }

      this._detachWindow(win);
    }
  }

  _handleDOMWindowOpened(aSubject) {
    let win = aSubject.QueryInterface(Ci.nsIDOMWindow);
    let self = this;

    win.addEventListener("load", () => {
      // We attach to the new window when it has been loaded if the new loaded
      // window is a browsing window.
      if (win.document
             .documentElement
             .getAttribute("windowtype") !== "navigator:browser") {
        return;
      }
      self._attachWindow(win);
    }, {once: true});
  }
}

let RFPHelper = new _RFPHelper();
