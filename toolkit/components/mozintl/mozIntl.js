/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

const Cc = Components.classes;
const Ci = Components.interfaces;

const mozIntlHelper =
  Cc["@mozilla.org/mozintlhelper;1"].getService(Ci.mozIMozIntlHelper);
const osPrefs =
  Cc["@mozilla.org/intl/ospreferences;1"].getService(Ci.mozIOSPreferences);

/**
 * This helper function retrives currently used app locales, allowing
 * all mozIntl APIs to use the current regional prefs locales unless
 * called with explicitly listed locales.
 */
function getLocales(locales) {
  if (!locales) {
    return Services.locale.getRegionalPrefsLocales();
  }
  return locales;
}

function getDateTimePatternStyle(option) {
  switch (option) {
    case "full":
      return osPrefs.dateTimeFormatStyleFull;
    case "long":
      return osPrefs.dateTimeFormatStyleLong;
    case "medium":
      return osPrefs.dateTimeFormatStyleMedium;
    case "short":
      return osPrefs.dateTimeFormatStyleShort;
    default:
      return osPrefs.dateTimeFormatStyleNone;
  }
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

  get DateTimeFormat() {
    if (!this._cache.hasOwnProperty("DateTimeFormat")) {
      mozIntlHelper.addDateTimeFormatConstructor(this._cache);
    }

    let DateTimeFormat = this._cache.DateTimeFormat;

    class MozDateTimeFormat extends this._cache.DateTimeFormat {
      constructor(locales, options, ...args) {
        let resolvedLocales = DateTimeFormat.supportedLocalesOf(getLocales(locales));
        if (options) {
          if (options.dateStyle || options.timeStyle) {
            options.pattern = osPrefs.getDateTimePattern(
              getDateTimePatternStyle(options.dateStyle),
              getDateTimePatternStyle(options.timeStyle),
              resolvedLocales[0]);
          } else {
            // make sure that user doesn't pass a pattern explicitly
            options.pattern = undefined;
          }
        }
        super(resolvedLocales, options, ...args);
      }
    }
    return MozDateTimeFormat;
  }

  get NumberFormat() {
    class MozNumberFormat extends Intl.NumberFormat {
      constructor(locales, options, ...args) {
        super(getLocales(locales), options, ...args);
      }
    }
    return MozNumberFormat;
  }

  get Collator() {
    class MozCollator extends Intl.Collator {
      constructor(locales, options, ...args) {
        super(getLocales(locales), options, ...args);
      }
    }
    return MozCollator;
  }

  get PluralRules() {
    class MozPluralRules extends Intl.PluralRules {
      constructor(locales, options, ...args) {
        super(getLocales(locales), options, ...args);
      }
    }
    return MozPluralRules;
  }
}

MozIntl.prototype.classID = Components.ID("{35ec195a-e8d0-4300-83af-c8a2cc84b4a3}");
MozIntl.prototype.QueryInterface = XPCOMUtils.generateQI([Ci.mozIMozIntl, Ci.nsISupports]);

var components = [MozIntl];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
