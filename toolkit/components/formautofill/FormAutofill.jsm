/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["FormAutofill"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  Region: "resource://gre/modules/Region.jsm",
});

const ADDRESSES_FIRST_TIME_USE_PREF = "extensions.formautofill.firstTimeUse";
const AUTOFILL_ADDRESSES_AVAILABLE_PREF =
  "extensions.formautofill.addresses.supported";
// This pref should be refactored after the migration of the old bool pref
const AUTOFILL_CREDITCARDS_AVAILABLE_PREF =
  "extensions.formautofill.creditCards.supported";
const BROWSER_SEARCH_REGION_PREF = "browser.search.region";
const CREDITCARDS_USED_STATUS_PREF = "extensions.formautofill.creditCards.used";
const CREDITCARDS_AUTOFILL_SUPPORTED_COUNTRIES_PREF =
  "extensions.formautofill.creditCards.supportedCountries";
const ENABLED_AUTOFILL_ADDRESSES_PREF =
  "extensions.formautofill.addresses.enabled";
const ENABLED_AUTOFILL_ADDRESSES_CAPTURE_PREF =
  "extensions.formautofill.addresses.capture.enabled";
const ENABLED_AUTOFILL_ADDRESSES_SUPPORTED_COUNTRIES_PREF =
  "extensions.formautofill.addresses.supportedCountries";
const ENABLED_AUTOFILL_CREDITCARDS_PREF =
  "extensions.formautofill.creditCards.enabled";
const ENABLED_AUTOFILL_CREDITCARDS_REAUTH_PREF =
  "extensions.formautofill.reauth.enabled";
const AUTOFILL_CREDITCARDS_HIDE_UI_PREF =
  "extensions.formautofill.creditCards.hideui";
const FORM_AUTOFILL_SUPPORT_RTL_PREF = "extensions.formautofill.supportRTL";

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "logLevel",
  "extensions.formautofill.loglevel",
  "Warn"
);

// A logging helper for debug logging to avoid creating Console objects
// or triggering expensive JS -> C++ calls when debug logging is not
// enabled.
//
// Console objects, even natively-implemented ones, can consume a lot of
// memory, and since this code may run in every content process, that
// memory can add up quickly. And, even when debug-level messages are
// being ignored, console.debug() calls can be expensive.
//
// This helper avoids both of those problems by never touching the
// console object unless debug logging is enabled.
function debug() {
  if (logLevel.toLowerCase() == "debug") {
    this.log.debug(...arguments);
  }
}

var FormAutofill = {
  ENABLED_AUTOFILL_ADDRESSES_PREF,
  ENABLED_AUTOFILL_ADDRESSES_CAPTURE_PREF,
  ENABLED_AUTOFILL_CREDITCARDS_PREF,
  ENABLED_AUTOFILL_CREDITCARDS_REAUTH_PREF,
  ADDRESSES_FIRST_TIME_USE_PREF,
  CREDITCARDS_USED_STATUS_PREF,

  get DEFAULT_REGION() {
    return Region.home || "US";
  },
  /**
   * Determines if an autofill feature should be enabled based on the "available"
   * and "supportedCountries" parameters.
   *
   * @param {string} available Available can be one of the following: "on", "detect", "off".
   * "on" forces the particular Form Autofill feature on, while "detect" utilizes the supported countries
   * to see if the feature should be available.
   * @param {string[]} supportedCountries
   * @returns {boolean} `true` if autofill feature is supported in the current browser search region
   */
  _isSupportedRegion(available, supportedCountries) {
    if (available == "on") {
      return true;
    } else if (available == "detect") {
      if (!FormAutofill.supportRTL && Services.locale.isAppLocaleRTL) {
        return false;
      }
      let region = Services.prefs.getCharPref(BROWSER_SEARCH_REGION_PREF, "");

      return supportedCountries.includes(region);
    }
    return false;
  },
  isAutofillAddressesAvailableInCountry(country) {
    return FormAutofill._addressAutofillSupportedCountries.includes(country);
  },
  get isAutofillEnabled() {
    return this.isAutofillAddressesEnabled || this.isAutofillCreditCardsEnabled;
  },
  /**
   * Determines if the credit card autofill feature is available to use in the browser.
   * If the feature is not available, then there are no user facing ways to enable it.
   * @returns {boolean} `true` if credit card autofill is available
   */
  get isAutofillCreditCardsAvailable() {
    return this._isSupportedRegion(
      FormAutofill._isAutofillCreditCardsAvailable,
      FormAutofill._creditCardAutofillSupportedCountries
    );
  },
  /**
   * Determines if the address autofill feature is available to use in the browser.
   * If the feature is not available, then there are no user facing ways to enable it.
   * @returns {boolean} `true` if address autofill is available
   */
  get isAutofillAddressesAvailable() {
    return this._isSupportedRegion(
      FormAutofill._isAutofillAddressesAvailable,
      FormAutofill._addressAutofillSupportedCountries
    );
  },
  /**
   * Determines if the user has enabled or disabled credit card autofill.
   * @returns {boolean} `true` if credit card autofill is enabled
   */
  get isAutofillCreditCardsEnabled() {
    return (
      this.isAutofillCreditCardsAvailable &&
      FormAutofill._isAutofillCreditCardsEnabled
    );
  },
  /**
   * Determines if the user has enabled or disabled address autofill.
   * @returns {boolean} `true` if address autofill is enabled
   */
  get isAutofillAddressesEnabled() {
    return (
      this.isAutofillAddressesAvailable &&
      FormAutofill._isAutofillAddressesEnabled
    );
  },

  defineLazyLogGetter(scope, logPrefix) {
    scope.debug = debug;

    XPCOMUtils.defineLazyGetter(scope, "log", () => {
      let ConsoleAPI = ChromeUtils.import(
        "resource://gre/modules/Console.jsm",
        {}
      ).ConsoleAPI;
      return new ConsoleAPI({
        maxLogLevelPref: "extensions.formautofill.loglevel",
        prefix: logPrefix,
      });
    });
  },
};

XPCOMUtils.defineLazyPreferenceGetter(
  FormAutofill,
  "_isAutofillAddressesAvailable",
  AUTOFILL_ADDRESSES_AVAILABLE_PREF
);
XPCOMUtils.defineLazyPreferenceGetter(
  FormAutofill,
  "_isAutofillAddressesEnabled",
  ENABLED_AUTOFILL_ADDRESSES_PREF
);
XPCOMUtils.defineLazyPreferenceGetter(
  FormAutofill,
  "isAutofillAddressesCaptureEnabled",
  ENABLED_AUTOFILL_ADDRESSES_CAPTURE_PREF
);
XPCOMUtils.defineLazyPreferenceGetter(
  FormAutofill,
  "_isAutofillCreditCardsAvailable",
  AUTOFILL_CREDITCARDS_AVAILABLE_PREF
);
XPCOMUtils.defineLazyPreferenceGetter(
  FormAutofill,
  "_isAutofillCreditCardsEnabled",
  ENABLED_AUTOFILL_CREDITCARDS_PREF
);
XPCOMUtils.defineLazyPreferenceGetter(
  FormAutofill,
  "isAutofillCreditCardsHideUI",
  AUTOFILL_CREDITCARDS_HIDE_UI_PREF
);
XPCOMUtils.defineLazyPreferenceGetter(
  FormAutofill,
  "isAutofillAddressesFirstTimeUse",
  ADDRESSES_FIRST_TIME_USE_PREF
);
XPCOMUtils.defineLazyPreferenceGetter(
  FormAutofill,
  "AutofillCreditCardsUsedStatus",
  CREDITCARDS_USED_STATUS_PREF
);
XPCOMUtils.defineLazyPreferenceGetter(
  FormAutofill,
  "_addressAutofillSupportedCountries",
  ENABLED_AUTOFILL_ADDRESSES_SUPPORTED_COUNTRIES_PREF,
  null,
  val => val.split(",")
);
XPCOMUtils.defineLazyPreferenceGetter(
  FormAutofill,
  "_creditCardAutofillSupportedCountries",
  CREDITCARDS_AUTOFILL_SUPPORTED_COUNTRIES_PREF,
  null,
  null,
  val => val.split(",")
);
XPCOMUtils.defineLazyPreferenceGetter(
  FormAutofill,
  "supportRTL",
  FORM_AUTOFILL_SUPPORT_RTL_PREF
);

// XXX: This should be invalidated on intl:app-locales-changed.
XPCOMUtils.defineLazyGetter(FormAutofill, "countries", () => {
  let availableRegionCodes = Services.intl.getAvailableLocaleDisplayNames(
    "region"
  );
  let displayNames = Services.intl.getRegionDisplayNames(
    undefined,
    availableRegionCodes
  );
  let result = new Map();
  for (let i = 0; i < availableRegionCodes.length; i++) {
    result.set(availableRegionCodes[i].toUpperCase(), displayNames[i]);
  }
  return result;
});
