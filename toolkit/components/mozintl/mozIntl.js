/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

const mozIntlHelper =
  Cc["@mozilla.org/mozintlhelper;1"].getService(Ci.mozIMozIntlHelper);
const osPrefs =
  Cc["@mozilla.org/intl/ospreferences;1"].getService(Ci.mozIOSPreferences);

/**
 * RegExp used to parse a BCP47 language tag (ex: en-US, sr-Cyrl-RU etc.)
 */
const languageTagMatch = /^([a-z]{2,3}|[a-z]{4}|[a-z]{5,8})(?:[-_]([a-z]{4}))?(?:[-_]([A-Z]{2}|[0-9]{3}))?((?:[-_](?:[a-z0-9]{5,8}|[0-9][a-z0-9]{3}))*)(?:[-_][a-wy-z0-9](?:[-_][a-z0-9]{2,8})+)*(?:[-_]x(?:[-_][a-z0-9]{1,8})+)?$/i;

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

/**
 * Number of milliseconds in other time units.
 *
 * This is used by relative time format best unit
 * calculations.
 */
const second = 1e3;
const minute = 6e4;
const hour = 36e5;
const day = 864e5;

/**
 * Use by RelativeTimeFormat.
 *
 * Allows for defining a cached getter to perform
 * calculations only once.
 *
 * @param {Object} obj - Object to place the getter on.
 * @param {String} prop - Name of the property.
 * @param {Function} get - Function that will be used as a getter.
 */
function defineCachedGetter(obj, prop, get) {
  defineGetter(obj, prop, function() {
    if (!this._[prop]) {
      this._[prop] = get.call(this);
    }
    return this._[prop];
  });
}

/**
 * Used by RelativeTimeFormat.
 *
 * Defines a getter on an object
 *
 * @param {Object} obj - Object to place the getter on.
 * @param {String} prop - Name of the property.
 * @param {Function} get - Function that will be used as a getter.
 */
function defineGetter(obj, prop, get) {
  Object.defineProperty(obj, prop, {get});
}

/**
 * Used by RelativeTimeFormat.
 *
 * Allows for calculation of the beginning of
 * a period for discrete distances.
 *
 * @param {Date} date - Date of which we're looking to find a start of.
 * @param {String} unit - Period to calculate the start of.
 *
 * @returns {Date}
 */
function startOf(date, unit) {
  date = new Date(date.getTime());
  switch (unit) {
    case "year": date.setMonth(0);
    // falls through
    case "month": date.setDate(1);
    // falls through
    case "day": date.setHours(0);
    // falls through
    case "hour": date.setMinutes(0);
    // falls through
    case "minute": date.setSeconds(0);
    // falls through
    case "second": date.setMilliseconds(0);
  }
  return date;
}

/**
 * Used by RelativeTimeFormat.
 *
 * Calculates the best fit unit to use for an absolute diff distance based
 * on thresholds.
 *
 * @param {Object} absDiff - Object with absolute diff per unit calculated.
 *
 * @returns {String}
 */
function bestFit(absDiff) {
  switch (true) {
    case absDiff.years > 0 && absDiff.months > threshold.month: return "year";
    case absDiff.months > 0 && absDiff.days > threshold.day: return "month";
    // case absDiff.months > 0 && absDiff.weeks > threshold.week: return "month";
    // case absDiff.weeks > 0 && absDiff.days > threshold.day: return "week";
    case absDiff.days > 0 && absDiff.hours > threshold.hour: return "day";
    case absDiff.hours > 0 && absDiff.minutes > threshold.minute: return "hour";
    case absDiff.minutes > 0 && absDiff.seconds > threshold.second: return "minute";
    default: return "second";
  }
}

/**
 * Used by RelativeTimeFormat.
 *
 * Thresholds to use for calculating the best unit for relative time fromatting.
 */
const threshold = {
  month: 2, // at least 2 months before using year.
  // week: 4, // at least 4 weeks before using month.
  day: 6, // at least 6 days before using month.
  hour: 6, // at least 6 hours before using day.
  minute: 59, // at least 59 minutes before using hour.
  second: 59 // at least 59 seconds before using minute.
};

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

  getLanguageDisplayNames(locales, langCodes) {
    if (locales !== undefined) {
      throw new Error("First argument support not implemented yet");
    }
    const languageBundle = Services.strings.createBundle(
          "chrome://global/locale/languageNames.properties");

    return langCodes.map(langCode => {
      if (typeof langCode !== "string") {
        throw new TypeError("All language codes must be strings.");
      }
      try {
        return languageBundle.GetStringFromName(langCode.toLowerCase());
      } catch (e) {
        return langCode.toLowerCase(); // Fall back to raw language subtag.
      }
    });
  }

  getRegionDisplayNames(locales, regionCodes) {
    if (locales !== undefined) {
      throw new Error("First argument support not implemented yet");
    }
    const regionBundle = Services.strings.createBundle(
          "chrome://global/locale/regionNames.properties");

    return regionCodes.map(regionCode => {
      if (typeof regionCode !== "string") {
        throw new TypeError("All region codes must be strings.");
      }
      try {
        return regionBundle.GetStringFromName(regionCode.toLowerCase());
      } catch (e) {
        return regionCode.toUpperCase(); // Fall back to raw region subtag.
      }
    });
  }

  getLocaleDisplayNames(locales, localeCodes) {
    if (locales !== undefined) {
      throw new Error("First argument support not implemented yet");
    }
    // Patterns hardcoded from CLDR 33 english.
    // We can later look into fetching them from CLDR directly.
    const localePattern = "{0} ({1})";
    const localeSeparator = ", ";

    return localeCodes.map(localeCode => {
      if (typeof localeCode !== "string") {
        throw new TypeError("All locale codes must be strings.");
      }
      // Get the display name for this dictionary.
      // XXX: To be replaced with Intl.Locale once it lands - bug 1433303.
      const match = localeCode.match(languageTagMatch);

      if (match === null) {
        return localeCode;
      }

      const [
        /* languageTag */,
        languageSubtag,
        scriptSubtag,
        regionSubtag,
        variantSubtags
      ] = match;

      const displayName = [
        this.getLanguageDisplayNames(locales, [languageSubtag])[0]
      ];

      if (scriptSubtag) {
        displayName.push(scriptSubtag);
      }

      if (regionSubtag) {
        displayName.push(this.getRegionDisplayNames(locales, [regionSubtag])[0]);
      }

      if (variantSubtags) {
        displayName.push(...variantSubtags.substr(1).split(/[-_]/)); // Collapse multiple variants.
      }

      let modifiers;
      if (displayName.length === 1) {
        return displayName[0];
      } else if (displayName.length > 2) {
        modifiers = displayName.slice(1).join(localeSeparator);
      } else {
        modifiers = displayName[1];
      }
      return localePattern.replace("{0}", displayName[0]).replace("{1}", modifiers);
    });
  }

  get DateTimeFormat() {
    if (!this._cache.hasOwnProperty("DateTimeFormat")) {
      mozIntlHelper.addDateTimeFormatConstructor(this._cache);
    }

    let DateTimeFormat = this._cache.DateTimeFormat;

    class MozDateTimeFormat extends DateTimeFormat {
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

  get RelativeTimeFormat() {
    if (!this._cache.hasOwnProperty("RelativeTimeFormat")) {
      mozIntlHelper.addRelativeTimeFormatConstructor(this._cache);
    }

    const RelativeTimeFormat = this._cache.RelativeTimeFormat;

    class MozRelativeTimeFormat extends RelativeTimeFormat {
      constructor(locales, options = {}, ...args) {

        // If someone is asking for MozRelativeTimeFormat, it's likely they'll want
        // to use `formatBestUnit` which works better with `auto`
        if (options.numeric === undefined) {
          options.numeric = "auto";
        }
        super(getLocales(locales), options, ...args);
      }

      formatBestUnit(date, {now = new Date()} = {}) {
        const diff = {
          _: {},
          ms: date.getTime() - now.getTime(),
          years: date.getFullYear() - now.getFullYear()
        };

        defineCachedGetter(diff, "months", function() {
          return this.years * 12 + date.getMonth() - now.getMonth();
        });
        defineCachedGetter(diff, "days", function() {
          return Math.trunc((startOf(date, "day") - startOf(now, "day")) / day);
        });
        defineCachedGetter(diff, "hours", function() {
          return Math.trunc((startOf(date, "hour") - startOf(now, "hour")) / hour);
        });
        defineCachedGetter(diff, "minutes", function() {
          return Math.trunc((startOf(date, "minute") - startOf(now, "minute")) / minute);
        });
        defineCachedGetter(diff, "seconds", function() {
          return Math.trunc((startOf(date, "second") - startOf(now, "second")) / second);
        });

        const absDiff = {
          _: {}
        };

        defineGetter(absDiff, "years", function() {
          return Math.abs(diff.years);
        });
        defineGetter(absDiff, "months", function() {
          return Math.abs(diff.months);
        });
        defineGetter(absDiff, "days", function() {
          return Math.abs(diff.days);
        });
        defineGetter(absDiff, "hours", function() {
          return Math.abs(diff.hours);
        });
        defineGetter(absDiff, "minutes", function() {
          return Math.abs(diff.minutes);
        });
        defineGetter(absDiff, "seconds", function() {
          return Math.abs(diff.seconds);
        });

        const unit = bestFit(absDiff);

        switch (unit) {
          case "year": return this.format(diff.years, unit);
          case "month": return this.format(diff.months, unit);
          case "day": return this.format(diff.days, unit);
          case "hour": return this.format(diff.hours, unit);
          case "minute": return this.format(diff.minutes, unit);
          default:
            if (unit !== "second") {
              throw new TypeError(`Unsupported unit "${unit}"`);
            }
            return this.format(diff.seconds, unit);
        }
      }
    }
    return MozRelativeTimeFormat;
  }
}

MozIntl.prototype.classID = Components.ID("{35ec195a-e8d0-4300-83af-c8a2cc84b4a3}");
MozIntl.prototype.QueryInterface = ChromeUtils.generateQI([Ci.mozIMozIntl]);

var components = [MozIntl];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
