// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

this.EXPORTED_SYMBOLS = ["LanguagePrompt"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const kPrefResistFingerprinting = "privacy.resistFingerprinting";
const kPrefSpoofEnglish = "privacy.spoof_english";
const kTopicHttpOnModifyRequest = "http-on-modify-request";

class _LanguagePrompt {
  constructor() {
    this._initialized = false;
  }

  init() {
    if (this._initialized) {
      return;
    }
    this._initialized = true;

    Services.prefs.addObserver(kPrefResistFingerprinting, this);
    this._handleResistFingerprintingChanged();
  }

  uninit() {
    if (!this._initialized) {
      return;
    }
    this._initialized = false;

    Services.prefs.removeObserver(kPrefResistFingerprinting, this);
    this._removeObservers();
  }

  observe(subject, topic, data) {
    switch (topic) {
      case "nsPref:changed":
        this._handlePrefChanged(data);
        break;
      case kTopicHttpOnModifyRequest:
        this._handleHttpOnModifyRequest(subject, data);
        break;
      default:
        break;
    }
  }

  _removeObservers() {
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

  _shouldPromptForLanguagePref() {
    return (Services.locale.getAppLocaleAsLangTag().substr(0, 2) !== "en")
      && (Services.prefs.getIntPref(kPrefSpoofEnglish) === 0);
  }

  _handlePrefChanged(data) {
    switch (data) {
      case kPrefResistFingerprinting:
        this._handleResistFingerprintingChanged();
        break;
      case kPrefSpoofEnglish:
        this._handleSpoofEnglishChanged();
        break;
      default:
        break;
    }
  }

  _handleResistFingerprintingChanged() {
    if (Services.prefs.getBoolPref(kPrefResistFingerprinting)) {
      Services.prefs.addObserver(kPrefSpoofEnglish, this);
      if (this._shouldPromptForLanguagePref()) {
        Services.obs.addObserver(this, kTopicHttpOnModifyRequest);
      }
    } else {
      this._removeObservers();
      Services.prefs.setIntPref(kPrefSpoofEnglish, 0);
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
    let channel = Services.io.newChannelFromURI2(
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
}

let LanguagePrompt = new _LanguagePrompt();
