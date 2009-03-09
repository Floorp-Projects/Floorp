/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Download Manager Utility Code.
 *
 * The Initial Developer of the Original Code is
 * Edward Lee <edward.lee@engineering.uiuc.edu>.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

var EXPORTED_SYMBOLS = [ "DownloadUtils" ];

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
 * [string displayHost, string fullHost]
 * getURIHost(string aURIString)
 *
 * [double convertedBytes, string units]
 * convertByteUnits(int aBytes)
 *
 * [int time, string units, int subTime, string subUnits]
 * convertTimeUnits(double aSecs)
 */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
Cu.import("resource://gre/modules/PluralForm.jsm");

const kDownloadProperties =
  "chrome://mozapps/locale/downloads/downloads.properties";

// These strings will be converted to the corresponding ones from the string
// bundle on use
let kStrings = {
  statusFormat: "statusFormat2",
  transferSameUnits: "transferSameUnits",
  transferDiffUnits: "transferDiffUnits",
  transferNoTotal: "transferNoTotal",
  timePair: "timePair",
  timeLeftSingle: "timeLeftSingle",
  timeLeftDouble: "timeLeftDouble",
  timeFewSeconds: "timeFewSeconds",
  timeUnknown: "timeUnknown",
  doneScheme: "doneScheme",
  doneFileScheme: "doneFileScheme",
  units: ["bytes", "kilobyte", "megabyte", "gigabyte"],
  // Update timeSize in convertTimeUnits if changing the length of this array
  timeUnits: ["seconds", "minutes", "hours", "days"],
};

// This object will lazily load the strings defined in kStrings
let gStr = {
  /**
   * Initialize lazy string getters
   */
  _init: function()
  {
    // Make each "name" a lazy-loading string that knows how to load itself. We
    // need to locally scope name and value to keep them around for the getter.
    for (let [name, value] in Iterator(kStrings))
      let ([n, v] = [name, value])
        gStr.__defineGetter__(n, function() gStr._getStr(n, v));
  },

  /**
   * Convert strings to those in the string bundle. This lazily loads the
   * string bundle *once* only when used the first time.
   */
  get _getStr()
  {
    // Delete the getter to be overwritten
    delete gStr._getStr;

    // Lazily load the bundle into the closure on first call to _getStr
    let getStr = Cc["@mozilla.org/intl/stringbundle;1"].
                 getService(Ci.nsIStringBundleService).
                 createBundle(kDownloadProperties).
                 GetStringFromName;

    // _getStr is a function that sets string "name" to stringbundle's "value"
    return gStr._getStr = function(name, value) {
      // Delete the getter to be overwritten
      delete gStr[name];

      try {
        // "name" is a string or array of the stringbundle-loaded "value"
        return gStr[name] = typeof value == "string" ?
                            getStr(value) :
                            value.map(getStr);
      } catch (e) {
        log(["Couldn't get string '", name, "' from property '", value, "'"]);
        // Don't return anything (undefined), and because we deleted ourselves,
        // future accesses will also be undefined
      }
    };
  },
};
// Initialize the lazy string getters!
gStr._init();

// Keep track of at most this many second/lastSec pairs so that multiple calls
// to getTimeLeft produce the same time left
const kCachedLastMaxSize = 10;
let gCachedLast = [];

let DownloadUtils = {
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
    if (aMaxBytes == null)
      aMaxBytes = -1;
    if (aSpeed == null)
      aSpeed = -1;
    if (aLastSec == null)
      aLastSec = Infinity;

    // Calculate the time remaining if we have valid values
    let seconds = (aSpeed > 0) && (aMaxBytes > 0) ?
      (aMaxBytes - aCurrBytes) / aSpeed : -1;

    // Update the bytes transferred and bytes total
    let status;
    let (transfer = DownloadUtils.getTransferTotal(aCurrBytes, aMaxBytes)) {
      // Insert 1 is the download progress
      status = replaceInsert(gStr.statusFormat, 1, transfer);
    }

    // Update the download rate
    let ([rate, unit] = DownloadUtils.convertByteUnits(aSpeed)) {
      // Insert 2 is the download rate
      status = replaceInsert(status, 2, rate);
      // Insert 3 is the |unit|/sec
      status = replaceInsert(status, 3, unit);
    }

    // Update time remaining
    let ([timeLeft, newLast] = DownloadUtils.getTimeLeft(seconds, aLastSec)) {
      // Insert 4 is the time remaining
      status = replaceInsert(status, 4, timeLeft);

      return [status, newLast];
    }
  },

  /**
   * Generate the transfer progress string to show the current and total byte
   * size. Byte units will be as large as possible and the same units for
   * current and max will be supressed for the former.
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
    let transfer;
    if (total < 0)
      transfer = gStr.transferNoTotal;
    else if (progressUnits == totalUnits)
      transfer = gStr.transferSameUnits;
    else
      transfer = gStr.transferDiffUnits;

    transfer = replaceInsert(transfer, 1, progress);
    transfer = replaceInsert(transfer, 2, progressUnits);
    transfer = replaceInsert(transfer, 3, total);
    transfer = replaceInsert(transfer, 4, totalUnits);

    return transfer;
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
      return [gStr.timeUnknown, aLastSec];

    // Try to find a cached lastSec for the given second
    aLastSec = gCachedLast.reduce(function(aResult, aItem)
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
      let (diff = aSeconds - aLastSec) {
        aSeconds = aLastSec + (diff < 0 ? .3 : .1) * diff;
      }

      // If the new time is similar, reuse something close to the last seconds,
      // but subtract a little to provide forward progress
      let diff = aSeconds - aLastSec;
      let diffPct = diff / aLastSec * 100;
      if (Math.abs(diff) < 5 || Math.abs(diffPct) < 5)
        aSeconds = aLastSec - (diff < 0 ? .4 : .2);
    }

    // Decide what text to show for the time
    let timeLeft;
    if (aSeconds < 4) {
      // Be friendly in the last few seconds
      timeLeft = gStr.timeFewSeconds;
    } else {
      // Convert the seconds into its two largest units to display
      let [time1, unit1, time2, unit2] =
        DownloadUtils.convertTimeUnits(aSeconds);

      let pair1 = replaceInsert(gStr.timePair, 1, time1);
      pair1 = replaceInsert(pair1, 2, unit1);
      let pair2 = replaceInsert(gStr.timePair, 1, time2);
      pair2 = replaceInsert(pair2, 2, unit2);

      // Only show minutes for under 1 hour or the second pair is 0
      if (aSeconds < 3600 || time2 == 0) {
        timeLeft = replaceInsert(gStr.timeLeftSingle, 1, pair1);
      } else {
        // We've got 2 pairs of times to display
        timeLeft = replaceInsert(gStr.timeLeftDouble, 1, pair1);
        timeLeft = replaceInsert(timeLeft, 2, pair2);
      }
    }

    return [timeLeft, aSeconds];
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
      displayHost = gStr.doneFileScheme;
      fullHost = displayHost;
    } else if (displayHost.length == 0) {
      // Got nothing; show the scheme (data: about: moz-icon:)
      displayHost = replaceInsert(gStr.doneScheme, 1, uri.scheme);
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
   * Converts a number of bytes to the appropriate unit that results in a
   * number that needs fewer than 4 digits
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
    aBytes = aBytes.toFixed((aBytes > 0) && (aBytes < 100) ? 1 : 0);

    return [aBytes, gStr.units[unitIndex]];
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

  return PluralForm.get(aTime, gStr.timeUnits[aIndex]);
}

/**
 * Private helper function to replace a placeholder string with a real string
 *
 * @param aText
 *        Source text containing placeholder (e.g., #1)
 * @param aIndex
 *        Index number of placeholder to replace
 * @param aValue
 *        New string to put in place of placeholder
 * @return The string with placeholder replaced with the new string
 */
function replaceInsert(aText, aIndex, aValue)
{
  return aText.replace("#" + aIndex, aValue);
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
