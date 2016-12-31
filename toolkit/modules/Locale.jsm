/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["Locale"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");

const PREF_MATCH_OS_LOCALE            = "intl.locale.matchOS";
const PREF_SELECTED_LOCALE            = "general.useragent.locale";

this.Locale = {
  /**
   * Gets the currently selected locale for display.
   * @return  the selected locale or "en-US" if none is selected
   */
  getLocale() {
    if (Preferences.get(PREF_MATCH_OS_LOCALE, false))
      return Services.locale.getLocaleComponentForUserAgent();
    try {
      let locale = Preferences.get(PREF_SELECTED_LOCALE, null, Ci.nsIPrefLocalizedString);
      if (locale)
        return locale;
    } catch (e) {}
    return Preferences.get(PREF_SELECTED_LOCALE, "en-US");
  },

  /**
   * Selects the closest matching locale from a list of locales.
   *
   * @param  aLocales
   *         An array of locales
   * @return the best match for the currently selected locale
   */
  findClosestLocale(aLocales) {
    let appLocale = this.getLocale();

    // Holds the best matching localized resource
    var bestmatch = null;
    // The number of locale parts it matched with
    var bestmatchcount = 0;
    // The number of locale parts in the match
    var bestpartcount = 0;

    var matchLocales = [appLocale.toLowerCase()];
    /* If the current locale is English then it will find a match if there is
       a valid match for en-US so no point searching that locale too. */
    if (matchLocales[0].substring(0, 3) != "en-")
      matchLocales.push("en-us");

    for (let locale of matchLocales) {
      var lparts = locale.split("-");
      for (let localized of aLocales) {
        for (let found of localized.locales) {
          found = found.toLowerCase();
          // Exact match is returned immediately
          if (locale == found)
            return localized;

          var fparts = found.split("-");
          /* If we have found a possible match and this one isn't any longer
             then we dont need to check further. */
          if (bestmatch && fparts.length < bestmatchcount)
            continue;

          // Count the number of parts that match
          var maxmatchcount = Math.min(fparts.length, lparts.length);
          var matchcount = 0;
          while (matchcount < maxmatchcount &&
                 fparts[matchcount] == lparts[matchcount])
            matchcount++;

          /* If we matched more than the last best match or matched the same and
             this locale is less specific than the last best match. */
          if (matchcount > bestmatchcount ||
              (matchcount == bestmatchcount && fparts.length < bestpartcount)) {
            bestmatch = localized;
            bestmatchcount = matchcount;
            bestpartcount = fparts.length;
          }
        }
      }
      // If we found a valid match for this locale return it
      if (bestmatch)
        return bestmatch;
    }
    return null;
  },
};
