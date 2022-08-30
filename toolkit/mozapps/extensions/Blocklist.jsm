/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* eslint "valid-jsdoc": [2, {requireReturn: false}] */

var EXPORTED_SYMBOLS = ["Blocklist", "BlocklistPrivate"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const lazy = {};
ChromeUtils.defineModuleGetter(
  lazy,
  "AddonManager",
  "resource://gre/modules/AddonManager.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "AddonManagerPrivate",
  "resource://gre/modules/AddonManager.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "RemoteSettings",
  "resource://services-settings/remote-settings.js"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "jexlFilterFunc",
  "resource://services-settings/remote-settings.js"
);

const CascadeFilter = Components.Constructor(
  "@mozilla.org/cascade-filter;1",
  "nsICascadeFilter",
  "setFilterData"
);

// The whole ID should be surrounded by literal ().
// IDs may contain alphanumerics, _, -, {}, @ and a literal '.'
// They may also contain backslashes (needed to escape the {} and dot)
// We filter out backslash escape sequences (like `\w`) separately
// (see kEscapeSequences).
const kIdSubRegex =
  "\\([" +
  "\\\\" + // note: just a backslash, but between regex and string it needs escaping.
  "\\w .{}@-]+\\)";

// prettier-ignore
// Find regular expressions of the form:
// /^((id1)|(id2)|(id3)|...|(idN))$/
// The outer set of parens enclosing the entire list of IDs is optional.
const kIsMultipleIds = new RegExp(
  // Start with literal sequence /^(
  //  (the `(` is optional)
  "^/\\^\\(?" +
    // Then at least one ID in parens ().
    kIdSubRegex +
    // Followed by any number of IDs in () separated by pipes.
    // Note: using a non-capturing group because we don't care about the value.
    "(?:\\|" + kIdSubRegex + ")*" +
  // Finally, we need to end with literal sequence )$/
  //  (the leading `)` is optional like at the start)
  "\\)?\\$/$"
);

// Check for a backslash followed by anything other than a literal . or curlies
const kEscapeSequences = /\\[^.{}]/;

// Used to remove the following 3 things:
// leading literal /^(
//    plus an optional (
// any backslash
// trailing literal )$/
//    plus an optional ) before the )$/
const kRegExpRemovalRegExp = /^\/\^\(\(?|\\|\)\)?\$\/$/g;

// In order to block add-ons, their type should be part of this list. There
// may be additional requirements such as requiring the add-on to be signed.
// See the uses of kXPIAddonTypes before introducing new addon types or
// providers that differ from the existing types.
XPCOMUtils.defineLazyGetter(lazy, "kXPIAddonTypes", () => {
  // In practice, this result is equivalent to ALL_XPI_TYPES in XPIProvider.jsm.
  // "plugin" (from GMPProvider.sys.mjs) is intentionally omitted, as we decided to
  // not support blocklisting of GMP plugins in bug 1086668.
  return lazy.AddonManagerPrivate.getAddonTypesByProvider("XPIProvider");
});

// For a given input string matcher, produce either a string to compare with,
// a regular expression, or a set of strings to compare with.
function processMatcher(str) {
  if (!str.startsWith("/")) {
    return str;
  }
  // Process regexes matching multiple IDs into a set.
  if (kIsMultipleIds.test(str) && !kEscapeSequences.test(str)) {
    // Remove the regexp gunk at the start and end of the string, as well
    // as all backslashes, and split by )|( to leave the list of IDs.
    return new Set(str.replace(kRegExpRemovalRegExp, "").split(")|("));
  }
  let lastSlash = str.lastIndexOf("/");
  let pattern = str.slice(1, lastSlash);
  let flags = str.slice(lastSlash + 1);
  return new RegExp(pattern, flags);
}

// Returns true if the addonProps object passes the constraints set by matches.
// (For every non-null property in matches, the same key must exist in
// addonProps and its value must match)
function doesAddonEntryMatch(matches, addonProps) {
  for (let [key, value] of Object.entries(matches)) {
    if (value === null || value === undefined) {
      continue;
    }
    if (addonProps[key]) {
      // If this property matches (member of the set, matches regex, or
      // an exact string match), continue to look at the other properties of
      // the `matches` object.
      // If not, return false immediately.
      if (value.has && value.has(addonProps[key])) {
        continue;
      }
      if (value.test && value.test(addonProps[key])) {
        continue;
      }
      if (typeof value == "string" && value === addonProps[key]) {
        continue;
      }
    }
    // If we get here, the property doesn't match, so this entry doesn't match.
    return false;
  }
  // If we get here, all the properties must have matched.
  return true;
}

const TOOLKIT_ID = "toolkit@mozilla.org";
const PREF_BLOCKLIST_ITEM_URL = "extensions.blocklist.itemURL";
const PREF_BLOCKLIST_ADDONITEM_URL = "extensions.blocklist.addonItemURL";
const PREF_BLOCKLIST_ENABLED = "extensions.blocklist.enabled";
const PREF_BLOCKLIST_LEVEL = "extensions.blocklist.level";
const PREF_BLOCKLIST_USE_MLBF = "extensions.blocklist.useMLBF";
const PREF_EM_LOGGING_ENABLED = "extensions.logging.enabled";
const DEFAULT_SEVERITY = 3;
const DEFAULT_LEVEL = 2;
const MAX_BLOCK_LEVEL = 3;

const BLOCKLIST_BUCKET = "blocklists";

const BlocklistTelemetry = {
  init() {
    // Used by BlocklistTelemetry.recordAddonBlockChangeTelemetry.
    Services.telemetry.setEventRecordingEnabled("blocklist", true);
  },

  /**
   * Record the RemoteSettings Blocklist lastModified server time into the
   * "blocklist.lastModified_rs keyed scalar (or "Missing Date" when unable
   * to retrieve a valid timestamp).
   *
   * @param {string} blocklistType
   *        The blocklist type that has been updated ("addons" or "addons_mlbf");
   *        the "gfx" blocklist is not covered by this telemetry).
   * @param {RemoteSettingsClient} remoteSettingsClient
   *        The RemoteSettings client to retrieve the lastModified timestamp from.
   */
  async recordRSBlocklistLastModified(blocklistType, remoteSettingsClient) {
    // In some tests overrides ensureInitialized and remoteSettingsClient
    // can be undefined, and in that case we don't want to record any
    // telemetry scalar.
    if (!remoteSettingsClient) {
      return;
    }

    let lastModified = await remoteSettingsClient.getLastModified();
    BlocklistTelemetry.recordTimeScalar(
      "lastModified_rs_" + blocklistType,
      lastModified
    );
  },

  /**
   * Record a timestamp in telemetry as a UTC string or "Missing Date" if the
   * input is not a valid timestamp.
   *
   * @param {string} telemetryKey
   *        The part of after "blocklist.", as defined in Scalars.yaml.
   * @param {number} time
   *        A timestamp to record. If invalid, "Missing Date" will be recorded.
   */
  recordTimeScalar(telemetryKey, time) {
    if (time > 0) {
      // convert from timestamp in ms into UTC datetime string, so it is going
      // to be record in the same format previously used by blocklist.lastModified_xml.
      let dateString = new Date(time).toUTCString();
      Services.telemetry.scalarSet("blocklist." + telemetryKey, dateString);
    } else {
      Services.telemetry.scalarSet("blocklist." + telemetryKey, "Missing Date");
    }
  },

  /**
   * Record whether an add-on is blocked and the parameters that guided the
   * decision to block or unblock the add-on.
   *
   * @param {AddonWrapper|object} addon
   *        The blocked or unblocked add-on. Not necessarily installed.
   *        Could be an object with the id, version and blocklistState
   *        properties when the AddonWrapper is not available (e.g. during
   *        update checks).
   * @param {string} reason
   *        The reason for recording the event,
   *        "addon_install", "addon_update", "addon_update_check",
   *        "addon_db_modified", "blocklist_update".
   */
  recordAddonBlockChangeTelemetry(addon, reason) {
    // Reduce the timer resolution for anonymity.
    let hoursSinceInstall = -1;
    if (reason === "blocklist_update" || reason === "addon_db_modified") {
      hoursSinceInstall = Math.round(
        (Date.now() - addon.installDate.getTime()) / 3600000
      );
    }

    const value = addon.id;
    const extra = {
      blocklistState: `${addon.blocklistState}`,
      addon_version: addon.version,
      signed_date: `${addon.signedDate?.getTime() || 0}`,
      hours_since: `${hoursSinceInstall}`,

      ...ExtensionBlocklistMLBF.getBlocklistMetadataForTelemetry(),
    };

    Services.telemetry.recordEvent(
      "blocklist",
      "addonBlockChange",
      reason,
      value,
      extra
    );
  },
};

const Utils = {
  /**
   * Checks whether this entry is valid for the current OS and ABI.
   * If the entry has an "os" property then the current OS must appear in
   * its comma separated list for it to be valid. Similarly for the
   * xpcomabi property.
   *
   * @param {Object} item
   *        The blocklist item.
   * @returns {bool}
   *        Whether the entry matches the current OS.
   */
  matchesOSABI(item) {
    if (item.os) {
      let os = item.os.split(",");
      if (!os.includes(lazy.gAppOS)) {
        return false;
      }
    }

    if (item.xpcomabi) {
      let xpcomabi = item.xpcomabi.split(",");
      if (!xpcomabi.includes(lazy.gApp.XPCOMABI)) {
        return false;
      }
    }
    return true;
  },

  /**
   * Checks if a version is higher than or equal to the minVersion (if provided)
   * and lower than or equal to the maxVersion (if provided).
   * @param {string} version
   *        The version to test.
   * @param {string?} minVersion
   *        The minimum version. If null it is assumed that version is always
   *        larger.
   * @param {string?} maxVersion
   *        The maximum version. If null it is assumed that version is always
   *        smaller.
   * @returns {boolean}
   *        Whether the item matches the range.
   */
  versionInRange(version, minVersion, maxVersion) {
    if (minVersion && Services.vc.compare(version, minVersion) < 0) {
      return false;
    }
    if (maxVersion && Services.vc.compare(version, maxVersion) > 0) {
      return false;
    }
    return true;
  },

  /**
   * Tests if this versionRange matches the item specified, and has a matching
   * targetApplication id and version.
   * @param {Object} versionRange
   *        The versionRange to check against
   * @param {string} itemVersion
   *        The version of the actual addon/plugin to test for.
   * @param {string} appVersion
   *        The version of the application to test for.
   * @param {string} toolkitVersion
   *        The version of toolkit to check for.
   * @returns {boolean}
   *        True if this version range covers the item and app/toolkit version given.
   */
  versionsMatch(versionRange, itemVersion, appVersion, toolkitVersion) {
    // Some platforms have no version for plugins, these don't match if there
    // was a min/maxVersion provided
    if (!itemVersion && (versionRange.minVersion || versionRange.maxVersion)) {
      return false;
    }

    // Check if the item version matches
    if (
      !this.versionInRange(
        itemVersion,
        versionRange.minVersion,
        versionRange.maxVersion
      )
    ) {
      return false;
    }

    // Check if the application or toolkit version matches
    for (let tA of versionRange.targetApplication) {
      if (
        tA.guid == lazy.gAppID &&
        this.versionInRange(appVersion, tA.minVersion, tA.maxVersion)
      ) {
        return true;
      }
      if (
        tA.guid == TOOLKIT_ID &&
        this.versionInRange(toolkitVersion, tA.minVersion, tA.maxVersion)
      ) {
        return true;
      }
    }
    return false;
  },

  /**
   * Given a blocklist JS object entry, ensure it has a versionRange property, where
   * each versionRange property has a valid severity property
   * and at least 1 valid targetApplication.
   * If it didn't have a valid targetApplication array before and/or it was empty,
   * fill it with an entry with null min/maxVersion properties, which will match
   * every version.
   *
   * If there *are* targetApplications, if any of them don't have a guid property,
   * assign them the current app's guid.
   *
   * @param {Object} entry
   *                 blocklist entry object.
   */
  ensureVersionRangeIsSane(entry) {
    if (!entry.versionRange.length) {
      entry.versionRange.push({});
    }
    for (let vr of entry.versionRange) {
      if (!vr.hasOwnProperty("severity")) {
        vr.severity = DEFAULT_SEVERITY;
      }
      if (!Array.isArray(vr.targetApplication)) {
        vr.targetApplication = [];
      }
      if (!vr.targetApplication.length) {
        vr.targetApplication.push({ minVersion: null, maxVersion: null });
      }
      vr.targetApplication.forEach(tA => {
        if (!tA.guid) {
          tA.guid = lazy.gAppID;
        }
      });
    }
  },

  /**
   * Create a blocklist URL for the given blockID
   * @param {String} id the blockID to use
   * @returns {String} the blocklist URL.
   */
  _createBlocklistURL(id) {
    let url = Services.urlFormatter.formatURLPref(PREF_BLOCKLIST_ITEM_URL);
    return url.replace(/%blockID%/g, id);
  },
};

/**
 * This custom filter function is used to limit the entries returned
 * by `RemoteSettings("...").get()` depending on the target app information
 * defined on entries.
 *
 * Note that this is async because `jexlFilterFunc` is async.
 *
 * @param {Object} entry a Remote Settings record
 * @param {Object} environment the JEXL environment object.
 * @returns {Object} The entry if it matches, `null` otherwise.
 */
async function targetAppFilter(entry, environment) {
  // If the entry has a JEXL filter expression, it should prevail.
  // The legacy target app mechanism will be kept in place for old entries.
  // See https://bugzilla.mozilla.org/show_bug.cgi?id=1463377
  const { filter_expression } = entry;
  if (filter_expression) {
    return lazy.jexlFilterFunc(entry, environment);
  }

  // Keep entries without target information.
  if (!("versionRange" in entry)) {
    return entry;
  }

  const { versionRange } = entry;

  // Everywhere in this method, we avoid checking the minVersion, because
  // we want to retain items whose minVersion is higher than the current
  // app version, so that we have the items around for app updates.

  // Gfx blocklist has a specific versionRange object, which is not a list.
  if (!Array.isArray(versionRange)) {
    const { maxVersion = "*" } = versionRange;
    const matchesRange =
      Services.vc.compare(lazy.gApp.version, maxVersion) <= 0;
    return matchesRange ? entry : null;
  }

  // Iterate the targeted applications, at least one of them must match.
  // If no target application, keep the entry.
  if (!versionRange.length) {
    return entry;
  }
  for (const vr of versionRange) {
    const { targetApplication = [] } = vr;
    if (!targetApplication.length) {
      return entry;
    }
    for (const ta of targetApplication) {
      const { guid } = ta;
      if (!guid) {
        return entry;
      }
      const { maxVersion = "*" } = ta;
      if (
        guid == lazy.gAppID &&
        Services.vc.compare(lazy.gApp.version, maxVersion) <= 0
      ) {
        return entry;
      }
      if (
        guid == "toolkit@mozilla.org" &&
        Services.vc.compare(Services.appinfo.platformVersion, maxVersion) <= 0
      ) {
        return entry;
      }
    }
  }
  // Skip this entry.
  return null;
}

/**
 * The Graphics blocklist implementation. The JSON objects for graphics blocks look
 * something like:
 *
 * {
 *  "blockID": "g35",
 *  "os": "WINNT 6.1",
 *  "vendor": "0xabcd",
 *  "devices": [
 *    "0x2783",
 *    "0x1234",
 *  ],
 *  "feature": " DIRECT2D ",
 *  "featureStatus": " BLOCKED_DRIVER_VERSION ",
 *  "driverVersion": " 8.52.322.2202 ",
 *  "driverVersionComparator": " LESS_THAN ",
 *  "versionRange": {"minVersion": "5.0", "maxVersion: "25.0"},
 * }
 *
 * The RemoteSetttings client takes care of filtering out versions that don't apply.
 * The code here stores entries in memory and sends them to the gfx component in
 * serialized text form, using ',', '\t' and '\n' as separators.
 */
const GfxBlocklistRS = {
  _ensureInitialized() {
    if (this._initialized || !gBlocklistEnabled) {
      return;
    }
    this._initialized = true;
    this._client = lazy.RemoteSettings("gfx", {
      bucketName: BLOCKLIST_BUCKET,
      filterFunc: targetAppFilter,
    });
    this.checkForEntries = this.checkForEntries.bind(this);
    this._client.on("sync", this.checkForEntries);
  },

  shutdown() {
    if (this._client) {
      this._client.off("sync", this.checkForEntries);
    }
  },

  sync() {
    this._ensureInitialized();
    return this._client.sync();
  },

  async checkForEntries() {
    this._ensureInitialized();
    if (!gBlocklistEnabled) {
      return []; // return value expected by tests.
    }
    let entries = await this._client.get().catch(ex => Cu.reportError(ex));
    // Handle error silently. This can happen if our request to fetch data is aborted,
    // e.g. by application shutdown.
    if (!entries) {
      return [];
    }
    // Trim helper (spaces, tabs, no-break spaces..)
    const trim = s =>
      (s || "").replace(/(^[\s\uFEFF\xA0]+)|([\s\uFEFF\xA0]+$)/g, "");

    entries = entries.map(entry => {
      let props = [
        "blockID",
        "driverVersion",
        "driverVersionMax",
        "driverVersionComparator",
        "feature",
        "featureStatus",
        "os",
        "vendor",
        "devices",
      ];
      let rv = {};
      for (let p of props) {
        let val = entry[p];
        // Ignore falsy values or empty arrays.
        if (!val || (Array.isArray(val) && !val.length)) {
          continue;
        }
        if (typeof val == "string") {
          val = trim(val);
        } else if (p == "devices") {
          let invalidDevices = [];
          let validDevices = [];
          // We serialize the array of devices as a comma-separated string, so
          // we need to ensure that none of the entries contain commas, also in
          // the future.
          val.forEach(v =>
            v.includes(",") ? invalidDevices.push(v) : validDevices.push(v)
          );
          for (let dev of invalidDevices) {
            const e = new Error(
              `Block ${entry.blockID} contains unsupported device: ${dev}`
            );
            Cu.reportError(e);
          }
          if (!validDevices) {
            continue;
          }
          val = validDevices;
        }
        rv[p] = val;
      }
      if (entry.versionRange) {
        rv.versionRange = {
          minVersion: trim(entry.versionRange.minVersion) || "0",
          maxVersion: trim(entry.versionRange.maxVersion) || "*",
        };
      }
      return rv;
    });
    if (entries.length) {
      let sortedProps = [
        "blockID",
        "devices",
        "driverVersion",
        "driverVersionComparator",
        "driverVersionMax",
        "feature",
        "featureStatus",
        "hardware",
        "manufacturer",
        "model",
        "os",
        "osversion",
        "product",
        "vendor",
        "versionRange",
      ];
      // Notify `GfxInfoBase`, by passing a string serialization.
      let payload = [];
      for (let gfxEntry of entries) {
        let entryLines = [];
        for (let key of sortedProps) {
          if (gfxEntry[key]) {
            let value = gfxEntry[key];
            if (Array.isArray(value)) {
              value = value.join(",");
            } else if (value.maxVersion) {
              // Both minVersion and maxVersion are always set on each entry.
              value = value.minVersion + "," + value.maxVersion;
            }
            entryLines.push(key + ":" + value);
          }
        }
        payload.push(entryLines.join("\t"));
      }
      Services.obs.notifyObservers(
        null,
        "blocklist-data-gfxItems",
        payload.join("\n")
      );
    }
    // The return value is only used by tests.
    return entries;
  },
};

/**
 * The extensions blocklist implementation. The JSON objects for extension
 * blocks look something like:
 *
 * {
 *   "guid": "someguid@addons.mozilla.org",
 *   "prefs": ["i.am.a.pref.that.needs.resetting"],
 *   "schema": 1480349193877,
 *   "blockID": "i12345",
 *   "details": {
 *     "bug": "https://bugzilla.mozilla.org/show_bug.cgi?id=1234567",
 *     "who": "All Firefox users who have this add-on installed. If you wish to continue using this add-on, you can enable it in the Add-ons Manager.",
 *     "why": "This add-on is in violation of the <a href=\"https://developer.mozilla.org/en-US/Add-ons/Add-on_guidelines\">Add-on Guidelines</a>, using multiple add-on IDs and potentially doing other unwanted activities.",
 *     "name": "Some pretty name",
 *     "created": "2019-05-06T19:52:20Z"
 *   },
 *   "enabled": true,
 *   "versionRange": [
 *     {
 *       "severity": 1,
 *       "maxVersion": "*",
 *       "minVersion": "0",
 *       "targetApplication": []
 *     }
 *   ],
 *   "id": "<unique guid>",
 *   "last_modified": 1480349215672,
 * }
 *
 * This is a legacy format, and implements deprecated operations (bug 1620580).
 * ExtensionBlocklistMLBF supersedes this implementation.
 */
const ExtensionBlocklistRS = {
  async _ensureEntries() {
    this.ensureInitialized();
    if (!this._entries && gBlocklistEnabled) {
      await this._updateEntries();
    }
  },

  async _updateEntries() {
    if (!gBlocklistEnabled) {
      this._entries = [];
      return;
    }
    this._entries = await this._client.get().catch(ex => Cu.reportError(ex));
    // Handle error silently. This can happen if our request to fetch data is aborted,
    // e.g. by application shutdown.
    if (!this._entries) {
      this._entries = [];
      return;
    }
    this._entries.forEach(entry => {
      entry.matches = {};
      if (entry.guid) {
        entry.matches.id = processMatcher(entry.guid);
      }
      for (let key of EXTENSION_BLOCK_FILTERS) {
        if (key == "id" || !entry[key]) {
          continue;
        }
        entry.matches[key] = processMatcher(entry[key]);
      }
      Utils.ensureVersionRangeIsSane(entry);
    });

    BlocklistTelemetry.recordRSBlocklistLastModified("addons", this._client);
  },

  async _filterItem(entry, environment) {
    if (!(await targetAppFilter(entry, environment))) {
      return null;
    }
    if (!Utils.matchesOSABI(entry)) {
      return null;
    }
    // Need something to filter on - at least a guid or name (either could be a regex):
    if (!entry.guid && !entry.name) {
      let blockID = entry.blockID || entry.id;
      Cu.reportError(new Error(`Nothing to filter add-on item ${blockID} on`));
      return null;
    }
    return entry;
  },

  sync() {
    this.ensureInitialized();
    return this._client.sync();
  },

  ensureInitialized() {
    if (!gBlocklistEnabled || this._initialized) {
      return;
    }
    this._initialized = true;
    this._client = lazy.RemoteSettings("addons", {
      bucketName: BLOCKLIST_BUCKET,
      filterFunc: this._filterItem,
    });
    this._onUpdate = this._onUpdate.bind(this);
    this._client.on("sync", this._onUpdate);
  },

  shutdown() {
    if (this._client) {
      this._client.off("sync", this._onUpdate);
      this._didShutdown = true;
    }
  },

  // Called when the blocklist implementation is changed via a pref.
  undoShutdown() {
    if (this._didShutdown) {
      this._client.on("sync", this._onUpdate);
      this._didShutdown = false;
    }
  },

  async _onUpdate() {
    let oldEntries = this._entries || [];
    await this.ensureInitialized();
    await this._updateEntries();

    let addons = await lazy.AddonManager.getAddonsByTypes(lazy.kXPIAddonTypes);
    for (let addon of addons) {
      let oldState = addon.blocklistState;
      if (addon.updateBlocklistState) {
        await addon.updateBlocklistState(false);
      } else if (oldEntries) {
        let oldEntry = this._getEntry(addon, oldEntries);
        oldState = oldEntry
          ? oldEntry.state
          : Ci.nsIBlocklistService.STATE_NOT_BLOCKED;
      } else {
        oldState = Ci.nsIBlocklistService.STATE_NOT_BLOCKED;
      }
      let state = addon.blocklistState;

      LOG(
        "Blocklist state for " +
          addon.id +
          " changed from " +
          oldState +
          " to " +
          state
      );

      // We don't want to re-warn about add-ons
      if (state == oldState) {
        continue;
      }

      // Ensure that softDisabled is false if the add-on is not soft blocked
      if (state != Ci.nsIBlocklistService.STATE_SOFTBLOCKED) {
        await addon.setSoftDisabled(false);
      }

      // If an add-on has dropped from hard to soft blocked just mark it as
      // soft disabled and don't warn about it.
      if (
        state == Ci.nsIBlocklistService.STATE_SOFTBLOCKED &&
        oldState == Ci.nsIBlocklistService.STATE_BLOCKED
      ) {
        await addon.setSoftDisabled(true);
      }

      if (
        state == Ci.nsIBlocklistService.STATE_BLOCKED ||
        state == Ci.nsIBlocklistService.STATE_SOFTBLOCKED
      ) {
        // Mark it as softblocked if necessary. Note that we avoid setting
        // softDisabled at the same time as userDisabled to make it clear
        // which was the original cause of the add-on becoming disabled in a
        // way that the user can change.
        if (
          state == Ci.nsIBlocklistService.STATE_SOFTBLOCKED &&
          !addon.userDisabled
        ) {
          await addon.setSoftDisabled(true);
        }
        // It's a block. We must reset certain preferences.
        let entry = this._getEntry(addon, this._entries);
        if (entry.prefs && entry.prefs.length) {
          for (let pref of entry.prefs) {
            Services.prefs.clearUserPref(pref);
          }
        }
      }
    }

    lazy.AddonManagerPrivate.updateAddonAppDisabledStates();
  },

  async getState(addon, appVersion, toolkitVersion) {
    let entry = await this.getEntry(addon, appVersion, toolkitVersion);
    return entry ? entry.state : Ci.nsIBlocklistService.STATE_NOT_BLOCKED;
  },

  async getEntry(addon, appVersion, toolkitVersion) {
    await this._ensureEntries();
    return this._getEntry(addon, this._entries, appVersion, toolkitVersion);
  },

  _getEntry(addon, addonEntries, appVersion, toolkitVersion) {
    if (!gBlocklistEnabled || !addon) {
      return null;
    }

    // Not all applications implement nsIXULAppInfo (e.g. xpcshell doesn't).
    if (!appVersion && !lazy.gApp.version) {
      return null;
    }

    if (!appVersion) {
      appVersion = lazy.gApp.version;
    }
    if (!toolkitVersion) {
      toolkitVersion = lazy.gApp.platformVersion;
    }

    let addonProps = {};
    for (let key of EXTENSION_BLOCK_FILTERS) {
      addonProps[key] = addon[key];
    }
    if (addonProps.creator) {
      addonProps.creator = addonProps.creator.name;
    }

    for (let entry of addonEntries) {
      // First check if it matches our properties. If not, just skip to the next item.
      if (!doesAddonEntryMatch(entry.matches, addonProps)) {
        continue;
      }
      // If those match, check the app or toolkit version works:
      for (let versionRange of entry.versionRange) {
        if (
          Utils.versionsMatch(
            versionRange,
            addon.version,
            appVersion,
            toolkitVersion
          )
        ) {
          let blockID = entry.blockID || entry.id;
          return {
            state:
              versionRange.severity >= gBlocklistLevel
                ? Ci.nsIBlocklistService.STATE_BLOCKED
                : Ci.nsIBlocklistService.STATE_SOFTBLOCKED,
            url: Utils._createBlocklistURL(blockID),
            prefs: entry.prefs || [],
          };
        }
      }
    }
    return null;
  },
};

/**
 * The extensions blocklist implementation, the third version.
 *
 * The current blocklist is represented by a multi-level bloom filter (MLBF)
 * (aka "Cascade Bloom Filter") that works like a set, i.e. supports a has()
 * operation, except it is probabilistic. The MLBF is 100% accurate for known
 * entries and unreliable for unknown entries.  When the backend generates the
 * MLBF, all known add-ons are recorded, including their block state. Unknown
 * add-ons are identified by their signature date being newer than the MLBF's
 * generation time, and they are considered to not be blocked.
 *
 * Legacy blocklists used to distinguish between "soft block" and "hard block",
 * but the current blocklist only supports one type of block ("hard block").
 * After checking the blocklist states, any previous "soft blocked" addons will
 * either be (hard) blocked or unblocked based on the blocklist.
 *
 * The MLBF is attached to a RemoteSettings record, as follows:
 *
 * {
 *   "generation_time": 1585692000000,
 *   "attachment": { ... RemoteSettings attachment ... }
 *   "attachment_type": "bloomfilter-base",
 * }
 *
 * The collection can also contain stashes:
 *
 * {
 *   "stash_time": 1585692000001,
 *   "stash": {
 *     "blocked": [ "addonid:1.0", ... ],
 *     "unblocked": [ "addonid:1.0", ... ]
 *  }
 *
 * Stashes can be used to update the blocklist without forcing the whole MLBF
 * to be downloaded again. These stashes are applied on top of the base MLBF.
 */
const ExtensionBlocklistMLBF = {
  RS_ATTACHMENT_ID: "addons-mlbf.bin",

  async _fetchMLBF(record) {
    // |record| may be unset. In that case, the MLBF dump is used instead
    // (provided that the client has been built with it included).
    let hash = record?.attachment.hash;
    if (this._mlbfData && hash && this._mlbfData.cascadeHash === hash) {
      // MLBF not changed, save the efforts of downloading the data again.

      // Although the MLBF has not changed, the time in the record has. This
      // means that the MLBF is known to provide accurate results for add-ons
      // that were signed after the previously known date (but before the newly
      // given date). To ensure that add-ons in this time range are also blocked
      // as expected, update the cached generationTime.
      if (record.generation_time > this._mlbfData.generationTime) {
        this._mlbfData.generationTime = record.generation_time;
      }
      return this._mlbfData;
    }
    const {
      buffer,
      record: actualRecord,
      _source: rsAttachmentSource,
    } = await this._client.attachments.download(record, {
      attachmentId: this.RS_ATTACHMENT_ID,
      fallbackToCache: true,
      fallbackToDump: true,
    });
    return {
      cascadeHash: actualRecord.attachment.hash,
      cascadeFilter: new CascadeFilter(new Uint8Array(buffer)),
      // Note: generation_time is semantically distinct from last_modified.
      // generation_time is compared with the signing date of the add-on, so it
      // should be in sync with the signing service's clock.
      // In contrast, last_modified does not have such strong requirements.
      generationTime: actualRecord.generation_time,
      // Used for telemetry.
      rsAttachmentSource,
    };
  },

  async _updateMLBF(forceUpdate = false) {
    // The update process consists of fetching the collection, followed by
    // potentially multiple network requests. As long as the collection has not
    // been changed, repeated update requests can be coalesced. But when the
    // collection has been updated, all pending update requests should await the
    // new update request instead of the previous one.
    if (!forceUpdate && this._updatePromise) {
      return this._updatePromise;
    }
    const isUpdateReplaced = () => this._updatePromise != updatePromise;
    const updatePromise = (async () => {
      if (!gBlocklistEnabled) {
        this._mlbfData = null;
        this._stashes = null;
        return;
      }
      let records = await this._client.get();
      if (isUpdateReplaced()) {
        return;
      }

      let mlbfRecords = records
        .filter(r => r.attachment)
        // Newest attachments first.
        .sort((a, b) => b.generation_time - a.generation_time);
      const mlbfRecord = mlbfRecords.find(
        r => r.attachment_type == "bloomfilter-base"
      );
      this._stashes = records
        .filter(({ stash }) => {
          return (
            // Exclude non-stashes, e.g. MLBF attachments.
            stash &&
            // Sanity check for type.
            Array.isArray(stash.blocked) &&
            Array.isArray(stash.unblocked)
          );
        })
        // Sort by stash time - newest first.
        .sort((a, b) => b.stash_time - a.stash_time)
        .map(({ stash, stash_time }) => ({
          blocked: new Set(stash.blocked),
          unblocked: new Set(stash.unblocked),
          stash_time,
        }));

      let mlbf = await this._fetchMLBF(mlbfRecord);
      // When a MLBF dump is packaged with the browser, mlbf will always be
      // non-null at this point.
      if (isUpdateReplaced()) {
        return;
      }
      this._mlbfData = mlbf;
    })()
      .catch(e => {
        Cu.reportError(e);
      })
      .then(() => {
        if (!isUpdateReplaced()) {
          this._updatePromise = null;
          this._recordPostUpdateTelemetry();
        }
        return this._updatePromise;
      });
    this._updatePromise = updatePromise;
    return updatePromise;
  },

  // Update the telemetry of the blocklist. This is always called, even if
  // the update request failed (e.g. due to network errors or data corruption).
  _recordPostUpdateTelemetry() {
    BlocklistTelemetry.recordRSBlocklistLastModified(
      "addons_mlbf",
      this._client
    );
    Services.telemetry.scalarSet(
      "blocklist.mlbf_source",
      this._mlbfData?.rsAttachmentSource || "unknown"
    );
    BlocklistTelemetry.recordTimeScalar(
      "mlbf_generation_time",
      this._mlbfData?.generationTime
    );
    // stashes has conveniently already been sorted by stash_time, newest first.
    let stashes = this._stashes || [];
    BlocklistTelemetry.recordTimeScalar(
      "mlbf_stash_time_oldest",
      stashes[stashes.length - 1]?.stash_time
    );
    BlocklistTelemetry.recordTimeScalar(
      "mlbf_stash_time_newest",
      stashes[0]?.stash_time
    );
  },

  // Used by BlocklistTelemetry.recordAddonBlockChangeTelemetry.
  getBlocklistMetadataForTelemetry() {
    // Blocklist telemetry can only be reported when a blocklist decision
    // has been made. That implies that the blocklist has been loaded, so
    // ExtensionBlocklistMLBF should have been initialized.
    // (except when the blocklist is disabled, or blocklist v2 is used)
    const generationTime = this._mlbfData?.generationTime ?? 0;

    // Keys to include in the blocklist.addonBlockChange telemetry event.
    return {
      mlbf_last_time:
        // stashes are sorted, newest first. Stashes are newer than the MLBF.
        `${this._stashes?.[0]?.stash_time ?? generationTime}`,
      mlbf_generation: `${generationTime}`,
      mlbf_source: this._mlbfData?.rsAttachmentSource ?? "unknown",
    };
  },

  ensureInitialized() {
    if (!gBlocklistEnabled || this._initialized) {
      return;
    }
    this._initialized = true;
    this._client = lazy.RemoteSettings("addons-bloomfilters", {
      bucketName: BLOCKLIST_BUCKET,
    });
    this._onUpdate = this._onUpdate.bind(this);
    this._client.on("sync", this._onUpdate);
  },

  shutdown() {
    if (this._client) {
      this._client.off("sync", this._onUpdate);
      this._didShutdown = true;
    }
  },

  // Called when the blocklist implementation is changed via a pref.
  undoShutdown() {
    if (this._didShutdown) {
      this._client.on("sync", this._onUpdate);
      this._didShutdown = false;
    }
  },

  async _onUpdate() {
    this.ensureInitialized();
    await this._updateMLBF(true);

    let addons = await lazy.AddonManager.getAddonsByTypes(lazy.kXPIAddonTypes);
    for (let addon of addons) {
      let oldState = addon.blocklistState;
      await addon.updateBlocklistState(false);
      let state = addon.blocklistState;

      LOG(
        "Blocklist state for " +
          addon.id +
          " changed from " +
          oldState +
          " to " +
          state
      );

      // We don't want to re-warn about add-ons
      if (state == oldState) {
        continue;
      }

      // Ensure that softDisabled is false if the add-on is not soft blocked
      // (by a previous implementation of the blocklist).
      if (state != Ci.nsIBlocklistService.STATE_SOFTBLOCKED) {
        await addon.setSoftDisabled(false);
      }

      BlocklistTelemetry.recordAddonBlockChangeTelemetry(
        addon,
        "blocklist_update"
      );
    }

    lazy.AddonManagerPrivate.updateAddonAppDisabledStates();
  },

  async getState(addon) {
    let state = await this.getEntry(addon);
    return state ? state.state : Ci.nsIBlocklistService.STATE_NOT_BLOCKED;
  },

  async getEntry(addon) {
    if (!this._stashes) {
      this.ensureInitialized();
      await this._updateMLBF(false);
    } else if (this._updatePromise) {
      // _stashes has been initialized, but the initialization of _mlbfData is
      // still pending.
      await this._updatePromise;
    }

    let blockKey = addon.id + ":" + addon.version;

    // _stashes will be unset if !gBlocklistEnabled.
    if (this._stashes) {
      // Stashes are ordered by newest first.
      for (let stash of this._stashes) {
        // blocked and unblocked do not have overlapping entries.
        if (stash.blocked.has(blockKey)) {
          return this._createBlockEntry(addon);
        }
        if (stash.unblocked.has(blockKey)) {
          return null;
        }
      }
    }

    // signedDate is a Date if the add-on is signed, null if not signed,
    // undefined if it's an addon update descriptor instead of an addon wrapper.
    let { signedDate } = addon;
    if (!signedDate) {
      // The MLBF does not apply to unsigned add-ons.
      return null;
    }

    if (!this._mlbfData) {
      // This could happen in theory in any of the following cases:
      // - the blocklist is disabled.
      // - The RemoteSettings backend served a malformed MLBF.
      // - The RemoteSettings backend is unreachable, and this client was built
      //   without including a dump of the MLBF.
      //
      // ... in other words, this is unlikely to happen in practice.
      return null;
    }
    let { cascadeFilter, generationTime } = this._mlbfData;
    if (!cascadeFilter.has(blockKey)) {
      // Add-on not blocked or unknown.
      return null;
    }
    // Add-on blocked, or unknown add-on inadvertently labeled as blocked.

    let { signedState } = addon;
    if (
      signedState !== lazy.AddonManager.SIGNEDSTATE_PRELIMINARY &&
      signedState !== lazy.AddonManager.SIGNEDSTATE_SIGNED
    ) {
      // The block decision can only be relied upon for known add-ons, i.e.
      // signed via AMO. Anything else is unknown and ignored:
      //
      // - SIGNEDSTATE_SYSTEM and SIGNEDSTATE_PRIVILEGED are signed
      //   independently of AMO.
      //
      // - SIGNEDSTATE_NOT_REQUIRED already has an early return above due to
      //   signedDate being unset for these kinds of add-ons.
      //
      // - SIGNEDSTATE_BROKEN, SIGNEDSTATE_UNKNOWN and SIGNEDSTATE_MISSING
      //   means that the signature cannot be relied upon. It is equivalent to
      //   removing the signature from the XPI file, which already causes them
      //   to be disabled on release builds (where MOZ_REQUIRE_SIGNING=true).
      return null;
    }

    if (signedDate.getTime() > generationTime) {
      // The bloom filter only reports 100% accurate results for known add-ons.
      // Since the add-on was unknown when the bloom filter was generated, the
      // block decision is incorrect and should be treated as unblocked.
      return null;
    }

    if (AppConstants.NIGHTLY_BUILD && addon.type === "locale") {
      // Only Mozilla can create langpacks with a valid signature.
      // Langpacks for Release, Beta and ESR are submitted to AMO.
      // DevEd does not support external langpacks (bug 1563923), only builtins.
      //   (and built-in addons are not subjected to the blocklist).
      // Langpacks for Nightly are not known to AMO, so the MLBF cannot be used.
      return null;
    }

    return this._createBlockEntry(addon);
  },

  _createBlockEntry(addon) {
    return {
      state: Ci.nsIBlocklistService.STATE_BLOCKED,
      url: this.createBlocklistURL(addon.id, addon.version),
    };
  },

  createBlocklistURL(id, version) {
    let url = Services.urlFormatter.formatURLPref(PREF_BLOCKLIST_ADDONITEM_URL);
    return url.replace(/%addonID%/g, id).replace(/%addonVersion%/g, version);
  },
};

const EXTENSION_BLOCK_FILTERS = [
  "id",
  "name",
  "creator",
  "homepageURL",
  "updateURL",
];

var gLoggingEnabled = null;
var gBlocklistEnabled = true;
var gBlocklistLevel = DEFAULT_LEVEL;

/**
 * @class nsIBlocklistPrompt
 *
 * nsIBlocklistPrompt is used, if available, by the default implementation of
 * nsIBlocklistService to display a confirmation UI to the user before blocking
 * extensions/plugins.
 */
/**
 * @method prompt
 *
 * Prompt the user about newly blocked addons. The prompt is then resposible
 * for soft-blocking any addons that need to be afterwards
 *
 * @param {object[]} aAddons
 *         An array of addons and plugins that are blocked. These are javascript
 *         objects with properties:
 *          name    - the plugin or extension name,
 *          version - the version of the extension or plugin,
 *          icon    - the plugin or extension icon,
 *          disable - can be used by the nsIBlocklistPrompt to allows users to decide
 *                    whether a soft-blocked add-on should be disabled,
 *          blocked - true if the item is hard-blocked, false otherwise,
 *          item    - the nsIPluginTag or Addon object
 */

// It is not possible to use the one in Services since it will not successfully
// QueryInterface nsIXULAppInfo in xpcshell tests due to other code calling
// Services.appinfo before the nsIXULAppInfo is created by the tests.
XPCOMUtils.defineLazyGetter(lazy, "gApp", function() {
  // eslint-disable-next-line mozilla/use-services
  let appinfo = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime);
  try {
    appinfo.QueryInterface(Ci.nsIXULAppInfo);
  } catch (ex) {
    // Not all applications implement nsIXULAppInfo (e.g. xpcshell doesn't).
    if (
      !(ex instanceof Components.Exception) ||
      ex.result != Cr.NS_NOINTERFACE
    ) {
      throw ex;
    }
  }
  return appinfo;
});

XPCOMUtils.defineLazyGetter(lazy, "gAppID", function() {
  return lazy.gApp.ID;
});
XPCOMUtils.defineLazyGetter(lazy, "gAppOS", function() {
  return lazy.gApp.OS;
});

/**
 * Logs a string to the error console.
 * @param {string} string
 *        The string to write to the error console..
 */
function LOG(string) {
  if (gLoggingEnabled) {
    dump("*** " + string + "\n");
    Services.console.logStringMessage(string);
  }
}

let Blocklist = {
  _init() {
    Services.obs.addObserver(this, "xpcom-shutdown");
    gLoggingEnabled = Services.prefs.getBoolPref(
      PREF_EM_LOGGING_ENABLED,
      false
    );
    gBlocklistEnabled = Services.prefs.getBoolPref(
      PREF_BLOCKLIST_ENABLED,
      true
    );
    gBlocklistLevel = Math.min(
      Services.prefs.getIntPref(PREF_BLOCKLIST_LEVEL, DEFAULT_LEVEL),
      MAX_BLOCK_LEVEL
    );
    this._chooseExtensionBlocklistImplementationFromPref();
    Services.prefs.addObserver("extensions.blocklist.", this);
    Services.prefs.addObserver(PREF_EM_LOGGING_ENABLED, this);
    BlocklistTelemetry.init();
  },
  isLoaded: true,

  shutdown() {
    GfxBlocklistRS.shutdown();
    this.ExtensionBlocklist.shutdown();

    Services.obs.removeObserver(this, "xpcom-shutdown");
    Services.prefs.removeObserver("extensions.blocklist.", this);
    Services.prefs.removeObserver(PREF_EM_LOGGING_ENABLED, this);
  },

  observe(subject, topic, prefName) {
    switch (topic) {
      case "xpcom-shutdown":
        this.shutdown();
        break;
      case "nsPref:changed":
        switch (prefName) {
          case PREF_EM_LOGGING_ENABLED:
            gLoggingEnabled = Services.prefs.getBoolPref(
              PREF_EM_LOGGING_ENABLED,
              false
            );
            break;
          case PREF_BLOCKLIST_ENABLED:
            gBlocklistEnabled = Services.prefs.getBoolPref(
              PREF_BLOCKLIST_ENABLED,
              true
            );
            this._blocklistUpdated();
            break;
          case PREF_BLOCKLIST_LEVEL:
            gBlocklistLevel = Math.min(
              Services.prefs.getIntPref(PREF_BLOCKLIST_LEVEL, DEFAULT_LEVEL),
              MAX_BLOCK_LEVEL
            );
            this._blocklistUpdated();
            break;
          case PREF_BLOCKLIST_USE_MLBF:
            let oldImpl = this.ExtensionBlocklist;
            this._chooseExtensionBlocklistImplementationFromPref();
            // The implementation may be unchanged when the pref is ignored.
            if (oldImpl != this.ExtensionBlocklist && oldImpl._initialized) {
              oldImpl.shutdown();
              this.ExtensionBlocklist.undoShutdown();
              this.ExtensionBlocklist._onUpdate();
            } // else neither has been initialized yet. Wait for it to happen.
            break;
        }
        break;
    }
  },

  loadBlocklistAsync() {
    // Need to ensure we notify gfx of new stuff.
    // Geckoview calls this for each new tab (bug 1730026), so ensure we only
    // check for entries when first initialized.
    if (!GfxBlocklistRS._initialized) {
      GfxBlocklistRS.checkForEntries();
    }
    this.ExtensionBlocklist.ensureInitialized();
  },

  getAddonBlocklistState(addon, appVersion, toolkitVersion) {
    // NOTE: appVersion/toolkitVersion are only used by ExtensionBlocklistRS.
    return this.ExtensionBlocklist.getState(addon, appVersion, toolkitVersion);
  },

  getAddonBlocklistEntry(addon, appVersion, toolkitVersion) {
    // NOTE: appVersion/toolkitVersion are only used by ExtensionBlocklistRS.
    return this.ExtensionBlocklist.getEntry(addon, appVersion, toolkitVersion);
  },

  recordAddonBlockChangeTelemetry(addon, reason) {
    BlocklistTelemetry.recordAddonBlockChangeTelemetry(addon, reason);
  },

  // TODO bug 1649906, bug 1639050: Remove blocklist v2.
  // Allow blocklist for Android and unit tests only.
  allowDeprecatedBlocklistV2: AppConstants.platform === "android",

  _chooseExtensionBlocklistImplementationFromPref() {
    if (
      this.allowDeprecatedBlocklistV2 &&
      !Services.prefs.getBoolPref(PREF_BLOCKLIST_USE_MLBF, false)
    ) {
      this.ExtensionBlocklist = ExtensionBlocklistRS;
      Services.telemetry.scalarSet("blocklist.mlbf_enabled", false);
    } else {
      this.ExtensionBlocklist = ExtensionBlocklistMLBF;
      Services.telemetry.scalarSet("blocklist.mlbf_enabled", true);
    }
  },

  _blocklistUpdated() {
    this.ExtensionBlocklist._onUpdate();
  },
};

Blocklist._init();

// Allow tests to reach implementation objects.
const BlocklistPrivate = {
  BlocklistTelemetry,
  ExtensionBlocklistMLBF,
  ExtensionBlocklistRS,
  GfxBlocklistRS,
};
