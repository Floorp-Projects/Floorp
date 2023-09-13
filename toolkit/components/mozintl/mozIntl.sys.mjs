/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const mozIntlHelper = Cc["@mozilla.org/mozintlhelper;1"].getService(
  Ci.mozIMozIntlHelper
);
const osPrefs = Cc["@mozilla.org/intl/ospreferences;1"].getService(
  Ci.mozIOSPreferences
);

/**
 * RegExp used to parse variant subtags from a BCP47 language tag.
 * For example: ca-valencia
 */
const variantSubtagsMatch = /(?:-(?:[a-z0-9]{5,8}|[0-9][a-z0-9]{3}))+$/;

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
  defineGetter(obj, prop, function () {
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
  Object.defineProperty(obj, prop, { get, enumerable: true });
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
    case "year":
      date.setMonth(0);
    // falls through
    case "month":
      date.setDate(1);
    // falls through
    case "day":
      date.setHours(0);
    // falls through
    case "hour":
      date.setMinutes(0);
    // falls through
    case "minute":
      date.setSeconds(0);
    // falls through
    case "second":
      date.setMilliseconds(0);
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
    case absDiff.years > 0 && absDiff.months > threshold.month:
      return "year";
    case absDiff.months > 0 && absDiff.weeks > threshold.week:
      return "month";
    case absDiff.weeks > 0 && absDiff.days > threshold.day:
      return "week";
    case absDiff.days > 0 && absDiff.hours > threshold.hour:
      return "day";
    case absDiff.hours > 0 && absDiff.minutes > threshold.minute:
      return "hour";
    case absDiff.minutes > 0 && absDiff.seconds > threshold.second:
      return "minute";
    default:
      return "second";
  }
}

/**
 * Used by RelativeTimeFormat.
 *
 * Thresholds to use for calculating the best unit for relative time fromatting.
 */
const threshold = {
  month: 2, // at least 2 months before using year.
  week: 3, // at least 3 weeks before using month.
  day: 6, // at least 6 days before using week.
  hour: 6, // at least 6 hours before using day.
  minute: 59, // at least 59 minutes before using hour.
  second: 59, // at least 59 seconds before using minute.
};

/**
 * Notice: If you're updating this list, you should also
 *         update the list in
 *         languageNames.ftl and regionNames.ftl.
 */
const availableLocaleDisplayNames = {
  region: new Set([
    "ad",
    "ae",
    "af",
    "ag",
    "ai",
    "al",
    "am",
    "ao",
    "aq",
    "ar",
    "as",
    "at",
    "au",
    "aw",
    "az",
    "ba",
    "bb",
    "bd",
    "be",
    "bf",
    "bg",
    "bh",
    "bi",
    "bj",
    "bl",
    "bm",
    "bn",
    "bo",
    "bq",
    "br",
    "bs",
    "bt",
    "bv",
    "bw",
    "by",
    "bz",
    "ca",
    "cc",
    "cd",
    "cf",
    "cg",
    "ch",
    "ci",
    "ck",
    "cl",
    "cm",
    "cn",
    "co",
    "cp",
    "cr",
    "cu",
    "cv",
    "cw",
    "cx",
    "cy",
    "cz",
    "de",
    "dg",
    "dj",
    "dk",
    "dm",
    "do",
    "dz",
    "ec",
    "ee",
    "eg",
    "eh",
    "er",
    "es",
    "et",
    "fi",
    "fj",
    "fk",
    "fm",
    "fo",
    "fr",
    "ga",
    "gb",
    "gd",
    "ge",
    "gf",
    "gg",
    "gh",
    "gi",
    "gl",
    "gm",
    "gn",
    "gp",
    "gq",
    "gr",
    "gs",
    "gt",
    "gu",
    "gw",
    "gy",
    "hk",
    "hm",
    "hn",
    "hr",
    "ht",
    "hu",
    "id",
    "ie",
    "il",
    "im",
    "in",
    "io",
    "iq",
    "ir",
    "is",
    "it",
    "je",
    "jm",
    "jo",
    "jp",
    "ke",
    "kg",
    "kh",
    "ki",
    "km",
    "kn",
    "kp",
    "kr",
    "kw",
    "ky",
    "kz",
    "la",
    "lb",
    "lc",
    "li",
    "lk",
    "lr",
    "ls",
    "lt",
    "lu",
    "lv",
    "ly",
    "ma",
    "mc",
    "md",
    "me",
    "mf",
    "mg",
    "mh",
    "mk",
    "ml",
    "mm",
    "mn",
    "mo",
    "mp",
    "mq",
    "mr",
    "ms",
    "mt",
    "mu",
    "mv",
    "mw",
    "mx",
    "my",
    "mz",
    "na",
    "nc",
    "ne",
    "nf",
    "ng",
    "ni",
    "nl",
    "no",
    "np",
    "nr",
    "nu",
    "nz",
    "om",
    "pa",
    "pe",
    "pf",
    "pg",
    "ph",
    "pk",
    "pl",
    "pm",
    "pn",
    "pr",
    "pt",
    "pw",
    "py",
    "qa",
    "qm",
    "qs",
    "qu",
    "qw",
    "qx",
    "qz",
    "re",
    "ro",
    "rs",
    "ru",
    "rw",
    "sa",
    "sb",
    "sc",
    "sd",
    "se",
    "sg",
    "sh",
    "si",
    "sk",
    "sl",
    "sm",
    "sn",
    "so",
    "sr",
    "ss",
    "st",
    "sv",
    "sx",
    "sy",
    "sz",
    "tc",
    "td",
    "tf",
    "tg",
    "th",
    "tj",
    "tk",
    "tl",
    "tm",
    "tn",
    "to",
    "tr",
    "tt",
    "tv",
    "tw",
    "tz",
    "ua",
    "ug",
    "us",
    "uy",
    "uz",
    "va",
    "vc",
    "ve",
    "vg",
    "vi",
    "vn",
    "vu",
    "wf",
    "ws",
    "xa",
    "xb",
    "xc",
    "xd",
    "xe",
    "xg",
    "xh",
    "xj",
    "xk",
    "xl",
    "xm",
    "xp",
    "xq",
    "xr",
    "xs",
    "xt",
    "xu",
    "xv",
    "xw",
    "ye",
    "yt",
    "za",
    "zm",
    "zw",
  ]),
  language: new Set([
    "aa",
    "ab",
    "ach",
    "ae",
    "af",
    "ak",
    "am",
    "an",
    "ar",
    "as",
    "ast",
    "av",
    "ay",
    "az",
    "ba",
    "be",
    "bg",
    "bh",
    "bi",
    "bm",
    "bn",
    "bo",
    "br",
    "bs",
    "ca",
    "cak",
    "ce",
    "ch",
    "co",
    "cr",
    "crh",
    "cs",
    "csb",
    "cu",
    "cv",
    "cy",
    "da",
    "de",
    "dsb",
    "dv",
    "dz",
    "ee",
    "el",
    "en",
    "eo",
    "es",
    "et",
    "eu",
    "fa",
    "ff",
    "fi",
    "fj",
    "fo",
    "fr",
    "fur",
    "fy",
    "ga",
    "gd",
    "gl",
    "gn",
    "gu",
    "gv",
    "ha",
    "haw",
    "he",
    "hi",
    "hil",
    "ho",
    "hr",
    "hsb",
    "ht",
    "hu",
    "hy",
    "hz",
    "ia",
    "id",
    "ie",
    "ig",
    "ii",
    "ik",
    "io",
    "is",
    "it",
    "iu",
    "ja",
    "jv",
    "ka",
    "kab",
    "kg",
    "ki",
    "kj",
    "kk",
    "kl",
    "km",
    "kn",
    "ko",
    "kok",
    "kr",
    "ks",
    "ku",
    "kv",
    "kw",
    "ky",
    "la",
    "lb",
    "lg",
    "li",
    "lij",
    "ln",
    "lo",
    "lt",
    "ltg",
    "lu",
    "lv",
    "mai",
    "meh",
    "mg",
    "mh",
    "mi",
    "mix",
    "mk",
    "ml",
    "mn",
    "mr",
    "ms",
    "mt",
    "my",
    "na",
    "nb",
    "nd",
    "ne",
    "ng",
    "nl",
    "nn",
    "no",
    "nr",
    "nso",
    "nv",
    "ny",
    "oc",
    "oj",
    "om",
    "or",
    "os",
    "pa",
    "pi",
    "pl",
    "ps",
    "pt",
    "qu",
    "rm",
    "rn",
    "ro",
    "ru",
    "rw",
    "sa",
    "sat",
    "sc",
    "sco",
    "sd",
    "se",
    "sg",
    "si",
    "sk",
    "sl",
    "sm",
    "sn",
    "so",
    "son",
    "sq",
    "sr",
    "ss",
    "st",
    "su",
    "sv",
    "sw",
    "szl",
    "ta",
    "te",
    "tg",
    "th",
    "ti",
    "tig",
    "tk",
    "tl",
    "tlh",
    "tn",
    "to",
    "tr",
    "trs",
    "ts",
    "tt",
    "tw",
    "ty",
    "ug",
    "uk",
    "ur",
    "uz",
    "ve",
    "vi",
    "vo",
    "wa",
    "wen",
    "wo",
    "xh",
    "yi",
    "yo",
    "za",
    "zam",
    "zh",
    "zu",
  ]),
};

/**
 * Notice: If you're updating these names, you should also update the data
 *         in python/mozbuild/mozbuild/action/langpack_localeNames.json.
 */
const nativeLocaleNames = new Map(
  Object.entries({
    ach: "Acholi",
    af: "Afrikaans",
    an: "Aragonés",
    ar: "العربية",
    ast: "Asturianu",
    az: "Azərbaycanca",
    be: "Беларуская",
    bg: "Български",
    bn: "বাংলা",
    bo: "བོད་སྐད",
    br: "Brezhoneg",
    brx: "बड़ो",
    bs: "Bosanski",
    ca: "Català",
    "ca-valencia": "Català (Valencià)",
    cak: "Kaqchikel",
    cs: "Čeština",
    cy: "Cymraeg",
    da: "Dansk",
    de: "Deutsch",
    dsb: "Dolnoserbšćina",
    el: "Ελληνικά",
    "en-CA": "English (CA)",
    "en-GB": "English (GB)",
    "en-US": "English (US)",
    eo: "Esperanto",
    "es-AR": "Español (AR)",
    "es-CL": "Español (CL)",
    "es-ES": "Español (ES)",
    "es-MX": "Español (MX)",
    et: "Eesti",
    eu: "Euskara",
    fa: "فارسی",
    ff: "Pulaar",
    fi: "Suomi",
    fr: "Français",
    fur: "Furlan",
    "fy-NL": "Frysk",
    "ga-IE": "Gaeilge",
    gd: "Gàidhlig",
    gl: "Galego",
    gn: "Guarani",
    "gu-IN": "ગુજરાતી",
    he: "עברית",
    "hi-IN": "हिन्दी",
    hr: "Hrvatski",
    hsb: "Hornjoserbšćina",
    hu: "Magyar",
    "hy-AM": "հայերեն",
    ia: "Interlingua",
    id: "Indonesia",
    is: "Islenska",
    it: "Italiano",
    ja: "日本語",
    "ja-JP-mac": "日本語",
    ka: "ქართული",
    kab: "Taqbaylit",
    kk: "қазақ тілі",
    km: "ខ្មែរ",
    kn: "ಕನ್ನಡ",
    ko: "한국어",
    lij: "Ligure",
    lo: "ລາວ",
    lt: "Lietuvių",
    ltg: "Latgalīšu",
    lv: "Latviešu",
    mk: "македонски",
    ml: "മലയാളം",
    mr: "मराठी",
    ms: "Melayu",
    my: "မြန်မာ",
    "nb-NO": "Norsk Bokmål",
    "ne-NP": "नेपाली",
    nl: "Nederlands",
    "nn-NO": "Nynorsk",
    oc: "Occitan",
    or: "ଓଡ଼ିଆ",
    "pa-IN": "ਪੰਜਾਬੀ",
    pl: "Polski",
    "pt-BR": "Português (BR)",
    "pt-PT": "Português (PT)",
    rm: "Rumantsch",
    ro: "Română",
    ru: "Русский",
    sat: "ᱥᱟᱱᱛᱟᱲᱤ",
    sc: "Sardu",
    sco: "Scots",
    si: "සිංහල",
    sk: "Slovenčina",
    sl: "Slovenščina",
    son: "Soŋay",
    sq: "Shqip",
    sr: "Cрпски",
    "sv-SE": "Svenska",
    szl: "Ślōnsko",
    ta: "தமிழ்",
    te: "తెలుగు",
    tg: "Тоҷикӣ",
    th: "ไทย",
    tl: "Tagalog",
    tr: "Türkçe",
    trs: "Triqui",
    uk: "Українська",
    ur: "اردو",
    uz: "O‘zbek",
    vi: "Tiếng Việt",
    wo: "Wolof",
    xh: "IsiXhosa",
    "zh-CN": "简体中文",
    "zh-TW": "正體中文",
  })
);

class MozRelativeTimeFormat extends Intl.RelativeTimeFormat {
  constructor(locales, options = {}, ...args) {
    // If someone is asking for MozRelativeTimeFormat, it's likely they'll want
    // to use `formatBestUnit` which works better with `auto`
    if (options.numeric === undefined) {
      options.numeric = "auto";
    }
    super(locales, options, ...args);
  }

  formatBestUnit(date, { now = new Date() } = {}) {
    const diff = {
      _: {},
      ms: date.getTime() - now.getTime(),
      years: date.getFullYear() - now.getFullYear(),
    };

    defineCachedGetter(diff, "months", function () {
      return this.years * 12 + date.getMonth() - now.getMonth();
    });
    defineCachedGetter(diff, "weeks", function () {
      return Math.trunc(this.days / 7);
    });
    defineCachedGetter(diff, "days", function () {
      return Math.trunc((startOf(date, "day") - startOf(now, "day")) / day);
    });
    defineCachedGetter(diff, "hours", function () {
      return Math.trunc((startOf(date, "hour") - startOf(now, "hour")) / hour);
    });
    defineCachedGetter(diff, "minutes", function () {
      return Math.trunc(
        (startOf(date, "minute") - startOf(now, "minute")) / minute
      );
    });
    defineCachedGetter(diff, "seconds", function () {
      return Math.trunc(
        (startOf(date, "second") - startOf(now, "second")) / second
      );
    });

    const absDiff = {
      _: {},
    };

    for (let prop of Object.keys(diff)) {
      defineGetter(absDiff, prop, function () {
        return Math.abs(diff[prop]);
      });
    }
    const unit = bestFit(absDiff);

    switch (unit) {
      case "year":
        return this.format(diff.years, unit);
      case "month":
        return this.format(diff.months, unit);
      case "week":
        return this.format(diff.weeks, unit);
      case "day":
        return this.format(diff.days, unit);
      case "hour":
        return this.format(diff.hours, unit);
      case "minute":
        return this.format(diff.minutes, unit);
      default:
        if (unit !== "second") {
          throw new TypeError(`Unsupported unit "${unit}"`);
        }
        return this.format(diff.seconds, unit);
    }
  }
}

export class MozIntl {
  Collator = Intl.Collator;
  ListFormat = Intl.ListFormat;
  Locale = Intl.Locale;
  NumberFormat = Intl.NumberFormat;
  PluralRules = Intl.PluralRules;
  RelativeTimeFormat = MozRelativeTimeFormat;

  constructor() {
    this._cache = {};
    Services.obs.addObserver(this, "intl:app-locales-changed", true);
  }

  observe() {
    // Clear cache when things change.
    this._cache = {};
  }

  getCalendarInfo(locales, ...args) {
    if (!this._cache.hasOwnProperty("getCalendarInfo")) {
      mozIntlHelper.addGetCalendarInfo(this._cache);
    }

    return this._cache.getCalendarInfo(locales, ...args);
  }

  getDisplayNamesDeprecated(locales, options = {}) {
    // Helper for IntlUtils.webidl, will be removed once Intl.DisplayNames is
    // available in non-privileged code.

    let { type, style, calendar, keys = [] } = options;

    let dn = new this.DisplayNames(locales, { type, style, calendar });
    let {
      locale: resolvedLocale,
      type: resolvedType,
      style: resolvedStyle,
      calendar: resolvedCalendar,
    } = dn.resolvedOptions();
    let values = keys.map(key => dn.of(key));

    return {
      locale: resolvedLocale,
      type: resolvedType,
      style: resolvedStyle,
      calendar: resolvedCalendar,
      values,
    };
  }

  getAvailableLocaleDisplayNames(type) {
    if (availableLocaleDisplayNames.hasOwnProperty(type)) {
      return Array.from(availableLocaleDisplayNames[type]);
    }

    return new Error("Unimplemented!");
  }

  getLanguageDisplayNames(locales, langCodes) {
    if (locales !== undefined) {
      throw new Error("First argument support not implemented yet");
    }

    if (!this._cache.hasOwnProperty("languageLocalization")) {
      const loc = new Localization(["toolkit/intl/languageNames.ftl"], true);
      this._cache.languageLocalization = loc;
    }

    const loc = this._cache.languageLocalization;

    return langCodes.map(langCode => {
      if (typeof langCode !== "string") {
        throw new TypeError("All language codes must be strings.");
      }
      let lcLangCode = langCode.toLowerCase();
      if (availableLocaleDisplayNames.language.has(lcLangCode)) {
        const value = loc.formatValueSync(`language-name-${lcLangCode}`);
        if (value !== null) {
          return value;
        }
      }
      return lcLangCode;
    });
  }

  getRegionDisplayNames(locales, regionCodes) {
    if (locales !== undefined) {
      throw new Error("First argument support not implemented yet");
    }

    if (!this._cache.hasOwnProperty("regionLocalization")) {
      const loc = new Localization(["toolkit/intl/regionNames.ftl"], true);
      this._cache.regionLocalization = loc;
    }

    const loc = this._cache.regionLocalization;

    return regionCodes.map(regionCode => {
      if (typeof regionCode !== "string") {
        throw new TypeError("All region codes must be strings.");
      }
      let lcRegionCode = regionCode.toLowerCase();
      if (availableLocaleDisplayNames.region.has(lcRegionCode)) {
        let regionID;
        // Allow changing names over time for specific regions
        switch (lcRegionCode) {
          case "bq":
            regionID = "region-name-bq-2018";
            break;
          case "cv":
            regionID = "region-name-cv-2020";
            break;
          case "cz":
            regionID = "region-name-cz-2019";
            break;
          case "mk":
            regionID = "region-name-mk-2019";
            break;
          case "sz":
            regionID = "region-name-sz-2019";
            break;
          default:
            regionID = `region-name-${lcRegionCode}`;
        }

        const value = loc.formatValueSync(regionID);
        if (value !== null) {
          return value;
        }
      }
      return regionCode.toUpperCase();
    });
  }

  getLocaleDisplayNames(locales, localeCodes, options = {}) {
    const { preferNative = false } = options;

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

      if (preferNative && nativeLocaleNames.has(localeCode)) {
        return nativeLocaleNames.get(localeCode);
      }

      let locale;
      try {
        locale = new Intl.Locale(localeCode.replaceAll("_", "-"));
      } catch {
        return localeCode;
      }

      const {
        language: languageSubtag,
        script: scriptSubtag,
        region: regionSubtag,
      } = locale;

      const variantSubtags = locale.baseName.match(variantSubtagsMatch);

      const displayName = [
        this.getLanguageDisplayNames(locales, [languageSubtag])[0],
      ];

      if (scriptSubtag) {
        displayName.push(scriptSubtag);
      }

      if (regionSubtag) {
        displayName.push(
          this.getRegionDisplayNames(locales, [regionSubtag])[0]
        );
      }

      if (variantSubtags) {
        displayName.push(...variantSubtags[0].substr(1).split("-")); // Collapse multiple variants.
      }

      let modifiers;
      if (displayName.length === 1) {
        return displayName[0];
      } else if (displayName.length > 2) {
        modifiers = displayName.slice(1).join(localeSeparator);
      } else {
        modifiers = displayName[1];
      }
      return localePattern
        .replace("{0}", displayName[0])
        .replace("{1}", modifiers);
    });
  }

  getScriptDirection(locale) {
    // This is a crude implementation until Bug 1693576 lands.
    // See justification in toolkit/components/mozintl/mozIMozIntl.idl
    const { language } = new Intl.Locale(locale);
    if (
      language == "ar" ||
      language == "ckb" ||
      language == "fa" ||
      language == "he" ||
      language == "ur"
    ) {
      return "rtl";
    }
    return "ltr";
  }

  get DateTimeFormat() {
    if (!this._cache.hasOwnProperty("DateTimeFormat")) {
      mozIntlHelper.addDateTimeFormatConstructor(this._cache);

      const DateTimeFormat = this._cache.DateTimeFormat;

      class MozDateTimeFormat extends DateTimeFormat {
        constructor(locales, options, ...args) {
          let resolvedLocales = DateTimeFormat.supportedLocalesOf(locales);
          if (options) {
            if (options.dateStyle || options.timeStyle) {
              options.pattern = osPrefs.getDateTimePattern(
                getDateTimePatternStyle(options.dateStyle),
                getDateTimePatternStyle(options.timeStyle),
                resolvedLocales[0]
              );
            } else {
              // make sure that user doesn't pass a pattern explicitly
              options.pattern = undefined;
            }
          }
          super(resolvedLocales, options, ...args);
        }
      }
      this._cache.MozDateTimeFormat = MozDateTimeFormat;
    }

    return this._cache.MozDateTimeFormat;
  }

  get DisplayNames() {
    if (!this._cache.hasOwnProperty("DisplayNames")) {
      mozIntlHelper.addDisplayNamesConstructor(this._cache);
    }

    return this._cache.DisplayNames;
  }
}

MozIntl.prototype.classID = Components.ID(
  "{35ec195a-e8d0-4300-83af-c8a2cc84b4a3}"
);
MozIntl.prototype.QueryInterface = ChromeUtils.generateQI([
  "mozIMozIntl",
  "nsIObserver",
  "nsISupportsWeakReference",
]);
