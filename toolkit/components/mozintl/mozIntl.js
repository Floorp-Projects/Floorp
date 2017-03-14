/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const Cc = Components.classes;
const Ci = Components.interfaces;

const mozIntlHelper =
  Cc["@mozilla.org/mozintlhelper;1"].getService(Ci.mozIMozIntlHelper);
const localeSvc =
  Cc["@mozilla.org/intl/localeservice;1"].getService(Ci.mozILocaleService);

/**
 * This helper function retrives currently used app locales, allowing
 * all mozIntl APIs to use the current app locales unless called with
 * explicitly listed locales.
 */
function getLocales(locales) {
  if (!locales) {
    return localeSvc.getAppLocalesAsBCP47();
  }
  return locales;
}

class MozIntl {
  constructor() {
    this._cache = {};
  }

  getCalendarInfo(locales, ...args) {
    if (!this._cache.hasOwnProperty("getCalendarInfo")) {
      mozIntlHelper.addGetCalendarInfo(this._cache);
    }

    return this._cache.getCalendarInfo(getLocales(locales), ...args);
  }

  getDisplayNames(locales, ...args) {
    if (!this._cache.hasOwnProperty("getDisplayNames")) {
      mozIntlHelper.addGetDisplayNames(this._cache);
    }

    return this._cache.getDisplayNames(getLocales(locales), ...args);
  }

  getLocaleInfo(locales, ...args) {
    if (!this._cache.hasOwnProperty("getLocaleInfo")) {
      mozIntlHelper.addGetLocaleInfo(this._cache);
    }

    return this._cache.getLocaleInfo(getLocales(locales), ...args);
  }

  createPluralRules(locales, ...args) {
    if (!this._cache.hasOwnProperty("PluralRules")) {
      mozIntlHelper.addPluralRulesConstructor(this._cache);
    }

    return new this._cache.PluralRules(getLocales(locales), ...args);
  }
}

MozIntl.prototype.classID = Components.ID("{35ec195a-e8d0-4300-83af-c8a2cc84b4a3}");
MozIntl.prototype.QueryInterface = XPCOMUtils.generateQI([Ci.mozIMozIntl, Ci.nsISupports]);

var components = [MozIntl];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
