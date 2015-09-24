/* vim: sw=2 ts=2 sts=2 expandtab filetype=javascript
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [ "DownloadUtils" ];

/**
 * This module provides the DownloadUtils object which contains useful methods
 * for downloads such as displaying file sizes, transfer times, and download
 * locations.
 *
 * List of methods:
 *
 * [string status, double newLast]
 * getDownloadStatus(int aCurrBytes, [optional] int aMaxBytes,
 *                   [optional] double aSpeed, [optional] double aLastSec)
 *
 * string progress
 * getTransferTotal(int aCurrBytes, [optional] int aMaxBytes)
 *
 * [string timeLeft, double newLast]
 * getTimeLeft(double aSeconds, [optional] double aLastSec)
 *
 * [string dateCompact, string dateComplete]
 * getReadableDates(Date aDate, [optional] Date aNow)
 *
 * [string displayHost, string fullHost]
 * getURIHost(string aURIString)
 *
 * [string convertedBytes, string units]
 * convertByteUnits(int aBytes)
 *
 * [int time, string units, int subTime, string subUnits]
 * convertTimeUnits(double aSecs)
 */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PluralForm",
                                  "resource://gre/modules/PluralForm.jsm");

this.__defineGetter__("gDecimalSymbol", function() {
    delete this.gDecimalSymbol;
      return this.gDecimalSymbol = Number(5.4).toLocaleString().match(/\D/);
});

var localeNumberFormatCache = new Map();
function getLocaleNumberFormat(fractionDigits) {
  // Backward compatibility: don't use localized digits
  let locale = Intl.NumberFormat().resolvedOptions().locale +
               "-u-nu-latn";
  let key = locale + "_" + fractionDigits;
  if (!localeNumberFormatCache.has(key)) {
    localeNumberFormatCache.set(key,
      Intl.NumberFormat(locale,
                        { maximumFractionDigits: fractionDigits,
                          minimumFractionDigits: fractionDigits }));
  }
  return localeNumberFormatCache.get(key);
}

const kDownloadProperties =
  "chrome://mozapps/locale/downloads/downloads.properties";

var gStr = {
  statusFormat: "statusFormat3",
  statusFormatInfiniteRate: "statusFormatInfiniteRate",
  statusFormatNoRate: "statusFormatNoRate",
  transferSameUnits: "transferSameUnits2",
  transferDiffUnits: "transferDiffUnits2",
  transferNoTotal: "transferNoTotal2",
  timePair: "timePair2",
  timeLeftSingle: "timeLeftSingle2",
  timeLeftDouble: "timeLeftDouble2",
  timeFewSeconds: "timeFewSeconds",
  timeUnknown: "timeUnknown",
  monthDate: "monthDate2",
  yesterday: "yesterday",
  doneScheme: "doneScheme2",
  doneFileScheme: "doneFileScheme",
  units: ["bytes", "kilobyte", "megabyte", "gigabyte"],
  // Update timeSize in convertTimeUnits if changing the length of this array
  timeUnits: ["seconds", "minutes", "hours", "days"],
  infiniteRate: "infiniteRate",
};

// This lazily initializes the string bundle upon first use.
this.__defineGetter__("gBundle", function() {
  delete this.gBundle;
  return this.gBundle = Cc["@mozilla.org/intl/stringbundle;1"].
                        getService(Ci.nsIStringBundleService).
                        createBundle(kDownloadProperties);
});

// Keep track of at most this many second/lastSec pairs so that multiple calls
// to getTimeLeft produce the same time left
const kCachedLastMaxSize = 10;
var gCachedLast = [];

this.DownloadUtils = {
  /**
   * Generate a full status string for a download given its current progress,
   * total size, speed, last time remaining
   *
   * @param aCurrBytes
   *        Number of bytes transferred so far
   * @param [optional] aMaxBytes
   *        Total number of bytes or -1 for unknown
   * @param [optional] aSpeed
   *        Current transfer rate in bytes/sec or -1 for unknown
   * @param [optional] aLastSec
   *        Last time remaining in seconds or Infinity for unknown
   * @return A pair: [download status text, new value of "last seconds"]
   */
  getDownloadStatus: function DU_getDownloadStatus(aCurrBytes, aMaxBytes,
                                                   aSpeed, aLastSec)
  {
    let [transfer, timeLeft, newLast, normalizedSpeed]
      = this._deriveTransferRate(aCurrBytes, aMaxBytes, aSpeed, aLastSec);

    let [rate, unit] = DownloadUtils.convertByteUnits(normalizedSpeed);

    let status;
    if (rate === "Infinity") {
      // Infinity download speed doesn't make sense. Show a localized phrase instead.
      let params = [transfer, gBundle.GetStringFromName(gStr.infiniteRate), timeLeft];
      status = gBundle.formatStringFromName(gStr.statusFormatInfiniteRate, params,
                                            params.length);
    }
    else {
      let params = [transfer, rate, unit, timeLeft];
      status = gBundle.formatStringFromName(gStr.statusFormat, params,
                                            params.length);
    }
    return [status, newLast];
  },

  /**
   * Generate a status string for a download given its current progress,
   * total size, speed, last time remaining. The status string contains the
   * time remaining, as well as the total bytes downloaded. Unlike
   * getDownloadStatus, it does not include the rate of download.
   *
   * @param aCurrBytes
   *        Number of bytes transferred so far
   * @param [optional] aMaxBytes
   *        Total number of bytes or -1 for unknown
   * @param [optional] aSpeed
   *        Current transfer rate in bytes/sec or -1 for unknown
   * @param [optional] aLastSec
   *        Last time remaining in seconds or Infinity for unknown
   * @return A pair: [download status text, new value of "last seconds"]
   */
  getDownloadStatusNoRate:
  function DU_getDownloadStatusNoRate(aCurrBytes, aMaxBytes, aSpeed,
                                      aLastSec)
  {
    let [transfer, timeLeft, newLast]
      = this._deriveTransferRate(aCurrBytes, aMaxBytes, aSpeed, aLastSec);

    let params = [transfer, timeLeft];
    let status = gBundle.formatStringFromName(gStr.statusFormatNoRate, params,
                                              params.length);
    return [status, newLast];
  },

  /**
   * Helper function that returns a transfer string, a time remaining string,
   * and a new value of "last seconds".
   * @param aCurrBytes
   *        Number of bytes transferred so far
   * @param [optional] aMaxBytes
   *        Total number of bytes or -1 for unknown
   * @param [optional] aSpeed
   *        Current transfer rate in bytes/sec or -1 for unknown
   * @param [optional] aLastSec
   *        Last time remaining in seconds or Infinity for unknown
   * @return A triple: [amount transferred string, time remaining string,
   *                    new value of "last seconds"]
   */
  _deriveTransferRate: function DU__deriveTransferRate(aCurrBytes,
                                                       aMaxBytes, aSpeed,
                                                       aLastSec)
  {
    if (aMaxBytes == null)
      aMaxBytes = -1;
    if (aSpeed == null)
      aSpeed = -1;
    if (aLastSec == null)
      aLastSec = Infinity;

    // Calculate the time remaining if we have valid values
    let seconds = (aSpeed > 0) && (aMaxBytes > 0) ?
      (aMaxBytes - aCurrBytes) / aSpeed : -1;

    let transfer = DownloadUtils.getTransferTotal(aCurrBytes, aMaxBytes);
    let [timeLeft, newLast] = DownloadUtils.getTimeLeft(seconds, aLastSec);
    return [transfer, timeLeft, newLast, aSpeed];
  },

  /**
   * Generate the transfer progress string to show the current and total byte
   * size. Byte units will be as large as possible and the same units for
   * current and max will be suppressed for the former.
   *
   * @param aCurrBytes
   *        Number of bytes transferred so far
   * @param [optional] aMaxBytes
   *        Total number of bytes or -1 for unknown
   * @return The transfer progress text
   */
  getTransferTotal: function DU_getTransferTotal(aCurrBytes, aMaxBytes)
  {
    if (aMaxBytes == null)
      aMaxBytes = -1;

    let [progress, progressUnits] = DownloadUtils.convertByteUnits(aCurrBytes);
    let [total, totalUnits] = DownloadUtils.convertByteUnits(aMaxBytes);

    // Figure out which byte progress string to display
    let name, values;
    if (aMaxBytes < 0) {
      name = gStr.transferNoTotal;
      values = [
        progress,
        progressUnits,
      ];
    } else if (progressUnits == totalUnits) {
      name = gStr.transferSameUnits;
      values = [
        progress,
        total,
        totalUnits,
      ];
    } else {
      name = gStr.transferDiffUnits;
      values = [
        progress,
        progressUnits,
        total,
        totalUnits,
      ];
    }

    return gBundle.formatStringFromName(name, values, values.length);
  },

  /**
   * Generate a "time left" string given an estimate on the time left and the
   * last time. The extra time is used to give a better estimate on the time to
   * show. Both the time values are doubles instead of integers to help get
   * sub-second accuracy for current and future estimates.
   *
   * @param aSeconds
   *        Current estimate on number of seconds left for the download
   * @param [optional] aLastSec
   *        Last time remaining in seconds or Infinity for unknown
   * @return A pair: [time left text, new value of "last seconds"]
   */
  getTimeLeft: function DU_getTimeLeft(aSeconds, aLastSec)
  {
    if (aLastSec == null)
      aLastSec = Infinity;

    if (aSeconds < 0)
      return [gBundle.GetStringFromName(gStr.timeUnknown), aLastSec];

    // Try to find a cached lastSec for the given second
    aLastSec = gCachedLast.reduce((aResult, aItem) =>
      aItem[0] == aSeconds ? aItem[1] : aResult, aLastSec);

    // Add the current second/lastSec pair unless we have too many
    gCachedLast.push([aSeconds, aLastSec]);
    if (gCachedLast.length > kCachedLastMaxSize)
      gCachedLast.shift();

    // Apply smoothing only if the new time isn't a huge change -- e.g., if the
    // new time is more than half the previous time; this is useful for
    // downloads that start/resume slowly
    if (aSeconds > aLastSec / 2) {
      // Apply hysteresis to favor downward over upward swings
      // 30% of down and 10% of up (exponential smoothing)
      let diff = aSeconds - aLastSec;
      aSeconds = aLastSec + (diff < 0 ? .3 : .1) * diff;

      // If the new time is similar, reuse something close to the last seconds,
      // but subtract a little to provide forward progress
      let diffPct = diff / aLastSec * 100;
      if (Math.abs(diff) < 5 || Math.abs(diffPct) < 5)
        aSeconds = aLastSec - (diff < 0 ? .4 : .2);
    }

    // Decide what text to show for the time
    let timeLeft;
    if (aSeconds < 4) {
      // Be friendly in the last few seconds
      timeLeft = gBundle.GetStringFromName(gStr.timeFewSeconds);
    } else {
      // Convert the seconds into its two largest units to display
      let [time1, unit1, time2, unit2] =
        DownloadUtils.convertTimeUnits(aSeconds);

      let pair1 =
        gBundle.formatStringFromName(gStr.timePair, [time1, unit1], 2);
      let pair2 =
        gBundle.formatStringFromName(gStr.timePair, [time2, unit2], 2);

      // Only show minutes for under 1 hour unless there's a few minutes left;
      // or the second pair is 0.
      if ((aSeconds < 3600 && time1 >= 4) || time2 == 0) {
        timeLeft = gBundle.formatStringFromName(gStr.timeLeftSingle,
                                                [pair1], 1);
      } else {
        // We've got 2 pairs of times to display
        timeLeft = gBundle.formatStringFromName(gStr.timeLeftDouble,
                                                [pair1, pair2], 2);
      }
    }

    return [timeLeft, aSeconds];
  },

  /**
   * Converts a Date object to two readable formats, one compact, one complete.
   * The compact format is relative to the current date, and is not an accurate
   * representation. For example, only the time is displayed for today. The
   * complete format always includes both the date and the time, excluding the
   * seconds, and is often shown when hovering the cursor over the compact
   * representation.
   *
   * @param aDate
   *        Date object representing the date and time to format. It is assumed
   *        that this value represents a past date.
   * @param [optional] aNow
   *        Date object representing the current date and time. The real date
   *        and time of invocation is used if this parameter is omitted.
   * @return A pair: [compact text, complete text]
   */
  getReadableDates: function DU_getReadableDates(aDate, aNow)
  {
    if (!aNow) {
      aNow = new Date();
    }

    let dts = Cc["@mozilla.org/intl/scriptabledateformat;1"]
              .getService(Ci.nsIScriptableDateFormat);

    // Figure out when today begins
    let today = new Date(aNow.getFullYear(), aNow.getMonth(), aNow.getDate());

    // Figure out if the time is from today, yesterday, this week, etc.
    let dateTimeCompact;
    if (aDate >= today) {
      // After today started, show the time
      dateTimeCompact = dts.FormatTime("",
                                       dts.timeFormatNoSeconds,
                                       aDate.getHours(),
                                       aDate.getMinutes(),
                                       0);
    } else if (today - aDate < (24 * 60 * 60 * 1000)) {
      // After yesterday started, show yesterday
      dateTimeCompact = gBundle.GetStringFromName(gStr.yesterday);
    } else if (today - aDate < (6 * 24 * 60 * 60 * 1000)) {
      // After last week started, show day of week
      dateTimeCompact = aDate.toLocaleFormat("%A");
    } else {
      // Show month/day
      let month = aDate.toLocaleFormat("%B");
      // Remove leading 0 by converting the date string to a number
      let date = Number(aDate.toLocaleFormat("%d"));
      dateTimeCompact = gBundle.formatStringFromName(gStr.monthDate, [month, date], 2);
    }

    let dateTimeFull = dts.FormatDateTime("",
                                          dts.dateFormatLong,
                                          dts.timeFormatNoSeconds,
                                          aDate.getFullYear(),
                                          aDate.getMonth() + 1,
                                          aDate.getDate(),
                                          aDate.getHours(),
                                          aDate.getMinutes(),
                                          0);

    return [dateTimeCompact, dateTimeFull];
  },

  /**
   * Get the appropriate display host string for a URI string depending on if
   * the URI has an eTLD + 1, is an IP address, a local file, or other protocol
   *
   * @param aURIString
   *        The URI string to try getting an eTLD + 1, etc.
   * @return A pair: [display host for the URI string, full host name]
   */
  getURIHost: function DU_getURIHost(aURIString)
  {
    let ioService = Cc["@mozilla.org/network/io-service;1"].
                    getService(Ci.nsIIOService);
    let eTLDService = Cc["@mozilla.org/network/effective-tld-service;1"].
                      getService(Ci.nsIEffectiveTLDService);
    let idnService = Cc["@mozilla.org/network/idn-service;1"].
                     getService(Ci.nsIIDNService);

    // Get a URI that knows about its components
    let uri = ioService.newURI(aURIString, null, null);

    // Get the inner-most uri for schemes like jar:
    if (uri instanceof Ci.nsINestedURI)
      uri = uri.innermostURI;

    let fullHost;
    try {
      // Get the full host name; some special URIs fail (data: jar:)
      fullHost = uri.host;
    } catch (e) {
      fullHost = "";
    }

    let displayHost;
    try {
      // This might fail if it's an IP address or doesn't have more than 1 part
      let baseDomain = eTLDService.getBaseDomain(uri);

      // Convert base domain for display; ignore the isAscii out param
      displayHost = idnService.convertToDisplayIDN(baseDomain, {});
    } catch (e) {
      // Default to the host name
      displayHost = fullHost;
    }

    // Check if we need to show something else for the host
    if (uri.scheme == "file") {
      // Display special text for file protocol
      displayHost = gBundle.GetStringFromName(gStr.doneFileScheme);
      fullHost = displayHost;
    } else if (displayHost.length == 0) {
      // Got nothing; show the scheme (data: about: moz-icon:)
      displayHost =
        gBundle.formatStringFromName(gStr.doneScheme, [uri.scheme], 1);
      fullHost = displayHost;
    } else if (uri.port != -1) {
      // Tack on the port if it's not the default port
      let port = ":" + uri.port;
      displayHost += port;
      fullHost += port;
    }

    return [displayHost, fullHost];
  },

  /**
   * Converts a number of bytes to the appropriate unit that results in an
   * internationalized number that needs fewer than 4 digits.
   *
   * @param aBytes
   *        Number of bytes to convert
   * @return A pair: [new value with 3 sig. figs., its unit]
   */
  convertByteUnits: function DU_convertByteUnits(aBytes)
  {
    let unitIndex = 0;

    // Convert to next unit if it needs 4 digits (after rounding), but only if
    // we know the name of the next unit
    while ((aBytes >= 999.5) && (unitIndex < gStr.units.length - 1)) {
      aBytes /= 1024;
      unitIndex++;
    }

    // Get rid of insignificant bits by truncating to 1 or 0 decimal points
    // 0 -> 0; 1.2 -> 1.2; 12.3 -> 12.3; 123.4 -> 123; 234.5 -> 235
    // added in bug 462064: (unitIndex != 0) makes sure that no decimal digit for bytes appears when aBytes < 100
    let fractionDigits = (aBytes > 0) && (aBytes < 100) && (unitIndex != 0) ? 1 : 0;

    // Don't try to format Infinity values using NumberFormat.
    if (aBytes === Infinity) {
      aBytes = "Infinity";
    } else {
      if (Intl) {
        aBytes = getLocaleNumberFormat(fractionDigits)
                   .format(aBytes);
      } else if (gDecimalSymbol != ".") {
        // FIXME: Fall back to the old hack, will be fixed in bug 1200494.
        aBytes = aBytes.toFixed(fractionDigits).replace(".", gDecimalSymbol);
      }
    }

    return [aBytes, gBundle.GetStringFromName(gStr.units[unitIndex])];
  },

  /**
   * Converts a number of seconds to the two largest units. Time values are
   * whole numbers, and units have the correct plural/singular form.
   *
   * @param aSecs
   *        Seconds to convert into the appropriate 2 units
   * @return 4-item array [first value, its unit, second value, its unit]
   */
  convertTimeUnits: function DU_convertTimeUnits(aSecs)
  {
    // These are the maximum values for seconds, minutes, hours corresponding
    // with gStr.timeUnits without the last item
    let timeSize = [60, 60, 24];

    let time = aSecs;
    let scale = 1;
    let unitIndex = 0;

    // Keep converting to the next unit while we have units left and the
    // current one isn't the largest unit possible
    while ((unitIndex < timeSize.length) && (time >= timeSize[unitIndex])) {
      time /= timeSize[unitIndex];
      scale *= timeSize[unitIndex];
      unitIndex++;
    }

    let value = convertTimeUnitsValue(time);
    let units = convertTimeUnitsUnits(value, unitIndex);

    let extra = aSecs - value * scale;
    let nextIndex = unitIndex - 1;

    // Convert the extra time to the next largest unit
    for (let index = 0; index < nextIndex; index++)
      extra /= timeSize[index];

    let value2 = convertTimeUnitsValue(extra);
    let units2 = convertTimeUnitsUnits(value2, nextIndex);

    return [value, units, value2, units2];
  },
};

/**
 * Private helper for convertTimeUnits that gets the display value of a time
 *
 * @param aTime
 *        Time value for display
 * @return An integer value for the time rounded down
 */
function convertTimeUnitsValue(aTime)
{
  return Math.floor(aTime);
}

/**
 * Private helper for convertTimeUnits that gets the display units of a time
 *
 * @param aTime
 *        Time value for display
 * @param aIndex
 *        Index into gStr.timeUnits for the appropriate unit
 * @return The appropriate plural form of the unit for the time
 */
function convertTimeUnitsUnits(aTime, aIndex)
{
  // Negative index would be an invalid unit, so just give empty
  if (aIndex < 0)
    return "";

  return PluralForm.get(aTime, gBundle.GetStringFromName(gStr.timeUnits[aIndex]));
}

/**
 * Private helper function to log errors to the error console and command line
 *
 * @param aMsg
 *        Error message to log or an array of strings to concat
 */
function log(aMsg)
{
  let msg = "DownloadUtils.jsm: " + (aMsg.join ? aMsg.join("") : aMsg);
  Cc["@mozilla.org/consoleservice;1"].getService(Ci.nsIConsoleService).
    logStringMessage(msg);
  dump(msg + "\n");
}
