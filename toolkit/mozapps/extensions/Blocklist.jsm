/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* eslint "valid-jsdoc": [2, {requireReturn: false}] */

var EXPORTED_SYMBOLS = ["Blocklist"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "AddonManager",
  "resource://gre/modules/AddonManager.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "AddonManagerPrivate",
  "resource://gre/modules/AddonManager.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "RemoteSettings",
  "resource://services-settings/remote-settings.js"
);
ChromeUtils.defineModuleGetter(
  this,
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
const PREF_BLOCKLIST_SUPPRESSUI = "extensions.blocklist.suppressUI";
const PREF_BLOCKLIST_USE_MLBF = "extensions.blocklist.useMLBF";
const PREF_BLOCKLIST_USE_MLBF_STASHES = "extensions.blocklist.useMLBF.stashes";
const PREF_EM_LOGGING_ENABLED = "extensions.logging.enabled";
const URI_BLOCKLIST_DIALOG =
  "chrome://mozapps/content/extensions/blocklist.xhtml";
const DEFAULT_SEVERITY = 3;
const DEFAULT_LEVEL = 2;
const MAX_BLOCK_LEVEL = 3;
const SEVERITY_OUTDATED = 0;
const VULNERABILITYSTATUS_NONE = 0;
const VULNERABILITYSTATUS_UPDATE_AVAILABLE = 1;
const VULNERABILITYSTATUS_NO_UPDATE = 2;

// Remote Settings blocklist constants
const PREF_BLOCKLIST_BUCKET = "services.blocklist.bucket";
const PREF_BLOCKLIST_GFX_COLLECTION = "services.blocklist.gfx.collection";
const PREF_BLOCKLIST_GFX_CHECKED_SECONDS = "services.blocklist.gfx.checked";
const PREF_BLOCKLIST_GFX_SIGNER = "services.blocklist.gfx.signer";
const PREF_BLOCKLIST_PLUGINS_COLLECTION =
  "services.blocklist.plugins.collection";
const PREF_BLOCKLIST_PLUGINS_CHECKED_SECONDS =
  "services.blocklist.plugins.checked";
const PREF_BLOCKLIST_PLUGINS_SIGNER = "services.blocklist.plugins.signer";
// Blocklist v2 - legacy JSON format.
const PREF_BLOCKLIST_ADDONS_COLLECTION = "services.blocklist.addons.collection";
const PREF_BLOCKLIST_ADDONS_CHECKED_SECONDS =
  "services.blocklist.addons.checked";
const PREF_BLOCKLIST_ADDONS_SIGNER = "services.blocklist.addons.signer";
// Blocklist v3 - MLBF format.
const PREF_BLOCKLIST_ADDONS3_COLLECTION =
  "services.blocklist.addons-mlbf.collection";
const PREF_BLOCKLIST_ADDONS3_CHECKED_SECONDS =
  "services.blocklist.addons-mlbf.checked";
const PREF_BLOCKLIST_ADDONS3_SIGNER = "services.blocklist.addons-mlbf.signer";

const BlocklistTelemetry = {
  /**
   * Record the RemoteSettings Blocklist lastModified server time into the
   * "blocklist.lastModified_rs keyed scalar (or "Missing Date" when unable
   * to retrieve a valid timestamp).
   *
   * @param {string} blocklistType
   *        The blocklist type that has been updated (one of "addons" or "plugins",
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

    if (lastModified > 0) {
      // convert from timestamp in ms into UTC datetime string, so it is going
      // to be record in the same format previously used by blocklist.lastModified_xml.
      lastModified = new Date(lastModified).toUTCString();
      Services.telemetry.scalarSet(
        `blocklist.lastModified_rs_${blocklistType}`,
        lastModified
      );
    } else {
      Services.telemetry.scalarSet(
        `blocklist.lastModified_rs_${blocklistType}`,
        "Missing Date"
      );
    }
  },
};

this.BlocklistTelemetry = BlocklistTelemetry;

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
      if (!os.includes(gAppOS)) {
        return false;
      }
    }

    if (item.xpcomabi) {
      let xpcomabi = item.xpcomabi.split(",");
      if (!xpcomabi.includes(gApp.XPCOMABI)) {
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
        tA.guid == gAppID &&
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
   * each versionRange property has valid severity and vulnerabilityStatus properties,
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
      if (!vr.hasOwnProperty("vulnerabilityStatus")) {
        vr.vulnerabilityStatus = VULNERABILITYSTATUS_NONE;
      }

      if (!Array.isArray(vr.targetApplication)) {
        vr.targetApplication = [];
      }
      if (!vr.targetApplication.length) {
        vr.targetApplication.push({ minVersion: null, maxVersion: null });
      }
      vr.targetApplication.forEach(tA => {
        if (!tA.guid) {
          tA.guid = gAppID;
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
async function targetAppFilter(entry, environment = {}) {
  // If the entry has a JEXL filter expression, it should prevail.
  // The legacy target app mechanism will be kept in place for old entries.
  // See https://bugzilla.mozilla.org/show_bug.cgi?id=1463377
  const { filter_expression } = entry;
  if (filter_expression) {
    return jexlFilterFunc(entry, environment);
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
    const matchesRange = Services.vc.compare(gApp.version, maxVersion) <= 0;
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
        guid == gAppID &&
        Services.vc.compare(gApp.version, maxVersion) <= 0
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
 *
 * Note: we assign to the global to allow tests to reach the object directly.
 */
this.GfxBlocklistRS = {
  _ensureInitialized() {
    if (this._initialized || !gBlocklistEnabled) {
      return;
    }
    this._initialized = true;
    this._client = RemoteSettings(
      Services.prefs.getCharPref(PREF_BLOCKLIST_GFX_COLLECTION),
      {
        bucketNamePref: PREF_BLOCKLIST_BUCKET,
        lastCheckTimePref: PREF_BLOCKLIST_GFX_CHECKED_SECONDS,
        signerName: Services.prefs.getCharPref(PREF_BLOCKLIST_GFX_SIGNER),
        filterFunc: targetAppFilter,
      }
    );
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
 * The plugins blocklist implementation. The JSON objects for plugin blocks look
 * something like:
 *
 *  {
 *    "blockID":"p906",
 *    "details": {
 *      "bug":"https://bugzilla.mozilla.org/show_bug.cgi?id=1159917",
 *      "who":"Which users it affects",
 *      "why":"Why it's being blocklisted",
 *      "name":"Java Plugin 7 update 45 to 78 (click-to-play), Windows",
 *      "created":"2015-05-19T09:02:45Z"
 *    },
 *    "enabled":true,
 *    "infoURL":"https://java.com/",
 *    "matchName":"Java\\(TM\\) Platform SE 7 U(4[5-9]|(5|6)\\d|7[0-8])(\\s[^\\d\\._U]|$)",
 *    "versionRange":[
 *      {
 *        "severity":0,
 *        "targetApplication":[
 *          {
 *            "guid":"{ec8030f7-c20a-464f-9b0e-13a3a9e97384}",
 *            "maxVersion":"57.0.*",
 *            "minVersion":"0"
 *          }
 *        ],
 *        "vulnerabilityStatus":1
 *      }
 *    ],
 *    "matchFilename":"npjp2\\.dll",
 *    "id":"f254e5bc-12c7-7954-fe6b-8f1fdab0ae88",
 *    "last_modified":1519390914542,
 *  }
 *
 * Note: we assign to the global to allow tests to reach the object directly.
 */
this.PluginBlocklistRS = {
  _matchProps: {
    matchDescription: "description",
    matchFilename: "filename",
    matchName: "name",
  },

  async _ensureEntries() {
    await this.ensureInitialized();
    if (!this._entries && gBlocklistEnabled) {
      await this._updateEntries();

      // Dispatch to mainthread because consumers may try to construct nsIPluginHost
      // again based on this notification, while we were called from nsIPluginHost
      // anyway, leading to re-entrancy.
      Services.tm.dispatchToMainThread(function() {
        Services.obs.notifyObservers(null, "plugin-blocklist-loaded");
      });
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
      for (let k of Object.keys(this._matchProps)) {
        if (entry[k]) {
          try {
            entry.matches[this._matchProps[k]] = new RegExp(entry[k], "m");
          } catch (ex) {
            /* Ignore invalid regexes */
          }
        }
      }
      Utils.ensureVersionRangeIsSane(entry);
    });

    BlocklistTelemetry.recordRSBlocklistLastModified("plugins", this._client);
  },

  async _filterItem(entry) {
    if (!(await targetAppFilter(entry))) {
      return null;
    }
    if (!Utils.matchesOSABI(entry)) {
      return null;
    }
    if (!entry.matchFilename && !entry.matchName && !entry.matchDescription) {
      let blockID = entry.blockID || entry.id;
      Cu.reportError(new Error(`Nothing to filter plugin item ${blockID}`));
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
    this._client = RemoteSettings(
      Services.prefs.getCharPref(PREF_BLOCKLIST_PLUGINS_COLLECTION),
      {
        bucketNamePref: PREF_BLOCKLIST_BUCKET,
        lastCheckTimePref: PREF_BLOCKLIST_PLUGINS_CHECKED_SECONDS,
        signerName: Services.prefs.getCharPref(PREF_BLOCKLIST_PLUGINS_SIGNER),
        filterFunc: this._filterItem,
      }
    );
    this._onUpdate = this._onUpdate.bind(this);
    this._client.on("sync", this._onUpdate);
  },

  shutdown() {
    if (this._client) {
      this._client.off("sync", this._onUpdate);
    }
  },

  async _onUpdate() {
    let oldEntries = this._entries || [];
    this.ensureInitialized();
    await this._updateEntries();
    const pluginHost = Cc["@mozilla.org/plugin/host;1"].getService(
      Ci.nsIPluginHost
    );
    const plugins = pluginHost.getPluginTags();

    let blockedItems = [];

    for (let plugin of plugins) {
      let oldState = this._getState(plugin, oldEntries);
      let state = this._getState(plugin, this._entries);
      LOG(
        "Blocklist state for " +
          plugin.name +
          " changed from " +
          oldState +
          " to " +
          state
      );
      // We don't want to re-warn about items
      if (state == oldState) {
        continue;
      }

      if (oldState == Ci.nsIBlocklistService.STATE_BLOCKED) {
        if (state == Ci.nsIBlocklistService.STATE_SOFTBLOCKED) {
          plugin.enabledState = Ci.nsIPluginTag.STATE_DISABLED;
        }
      } else if (
        !plugin.disabled &&
        state != Ci.nsIBlocklistService.STATE_NOT_BLOCKED
      ) {
        if (
          state != Ci.nsIBlocklistService.STATE_OUTDATED &&
          state != Ci.nsIBlocklistService.STATE_VULNERABLE_UPDATE_AVAILABLE &&
          state != Ci.nsIBlocklistService.STATE_VULNERABLE_NO_UPDATE
        ) {
          blockedItems.push({
            name: plugin.name,
            version: plugin.version,
            icon: "chrome://mozapps/skin/plugins/pluginGeneric.svg",
            disable: false,
            blocked: state == Ci.nsIBlocklistService.STATE_BLOCKED,
            item: plugin,
            url: await this.getURL(plugin),
          });
        }
      }
    }

    if (blockedItems.length) {
      this._showBlockedPluginsPrompt(blockedItems);
    } else {
      this._notifyUpdate();
    }
  },

  _showBlockedPluginsPrompt(blockedPlugins) {
    if ("@mozilla.org/addons/blocklist-prompt;1" in Cc) {
      try {
        let blockedPrompter = Cc[
          "@mozilla.org/addons/blocklist-prompt;1"
        ].getService().wrappedJSObject;
        blockedPrompter.prompt(blockedPlugins);
      } catch (e) {
        LOG(e);
      }
      this._notifyUpdate();
      return;
    }

    let args = {
      restart: false,
      list: blockedPlugins,
    };
    // This lets the dialog get the raw js object
    args.wrappedJSObject = args;

    /*
      Some tests run without UI, so the async code listens to a message
      that can be sent programatically
    */
    let applyBlocklistChanges = async () => {
      Services.obs.removeObserver(
        applyBlocklistChanges,
        "addon-blocklist-closed"
      );

      for (let blockedData of blockedPlugins) {
        if (!blockedData.disable) {
          continue;
        }

        // This will disable all the plugins immediately.
        if (blockedData.item instanceof Ci.nsIPluginTag) {
          blockedData.item.enabledState = Ci.nsIPluginTag.STATE_DISABLED;
        }
      }

      if (!args.restart) {
        this._notifyUpdate();
        return;
      }

      // We need to ensure the new blocklist state is written to disk before restarting.
      // We'll notify about the blocklist update, then wait for nsIPluginHost
      // to finish processing it, then restart the browser.
      let pluginUpdatesFinishedPromise = new Promise(resolve => {
        Services.obs.addObserver(function updatesFinished() {
          Services.obs.removeObserver(
            updatesFinished,
            "plugin-blocklist-updates-finished"
          );
          resolve();
        }, "plugin-blocklist-updates-finished");
      });
      this._notifyUpdate();
      await pluginUpdatesFinishedPromise;

      // Notify all windows that an application quit has been requested.
      var cancelQuit = Cc["@mozilla.org/supports-PRBool;1"].createInstance(
        Ci.nsISupportsPRBool
      );
      Services.obs.notifyObservers(cancelQuit, "quit-application-requested");

      // Something aborted the quit process.
      if (cancelQuit.data) {
        return;
      }

      Services.startup.quit(
        Ci.nsIAppStartup.eRestart | Ci.nsIAppStartup.eAttemptQuit
      );
    };

    Services.obs.addObserver(applyBlocklistChanges, "addon-blocklist-closed");

    if (Services.prefs.getBoolPref(PREF_BLOCKLIST_SUPPRESSUI, false)) {
      applyBlocklistChanges();
      return;
    }

    function blocklistUnloadHandler(event) {
      if (event.target.location == URI_BLOCKLIST_DIALOG) {
        applyBlocklistChanges();
        blocklistWindow.removeEventListener("unload", blocklistUnloadHandler);
      }
    }

    let blocklistWindow = Services.ww.openWindow(
      null,
      URI_BLOCKLIST_DIALOG,
      "",
      "chrome,centerscreen,dialog,titlebar",
      args
    );
    if (blocklistWindow) {
      blocklistWindow.addEventListener("unload", blocklistUnloadHandler);
    }
  },

  _notifyUpdate() {
    Services.obs.notifyObservers(null, "plugin-blocklist-updated");
  },

  async getURL(plugin) {
    await this._ensureEntries();
    let r = this._getEntry(plugin, this._entries);
    if (!r) {
      return null;
    }
    let blockEntry = r.entry;
    let blockID = blockEntry.blockID || blockEntry.id;
    return blockEntry.infoURL || Utils._createBlocklistURL(blockID);
  },

  async getState(plugin, appVersion, toolkitVersion) {
    if (AppConstants.platform == "android") {
      return Ci.nsIBlocklistService.STATE_NOT_BLOCKED;
    }
    await this._ensureEntries();
    return this._getState(plugin, this._entries, appVersion, toolkitVersion);
  },

  /**
   * Private helper to get the blocklist entry for a plugin given a set of
   * blocklist entries and versions.
   *
   * @param {nsIPluginTag} plugin
   *        The nsIPluginTag to get the blocklist state for.
   * @param {object[]} pluginEntries
   *        The plugin blocklist entries to compare against.
   * @param {string?} appVersion
   *        The application version to compare to, will use the current
   *        version if null.
   * @param {string?} toolkitVersion
   *        The toolkit version to compare to, will use the current version if
   *        null.
   * @returns {object?}
   *        {entry: blocklistEntry, version: blocklistEntryVersion},
   *        or null if there is no matching entry.
   */
  _getEntry(plugin, pluginEntries, appVersion, toolkitVersion) {
    if (!gBlocklistEnabled) {
      return null;
    }

    // Not all applications implement nsIXULAppInfo (e.g. xpcshell doesn't).
    if (!appVersion && !gApp.version) {
      return Ci.nsIBlocklistService.STATE_NOT_BLOCKED;
    }

    if (!appVersion) {
      appVersion = gApp.version;
    }
    if (!toolkitVersion) {
      toolkitVersion = gApp.platformVersion;
    }

    const pluginProperties = {
      description: plugin.description,
      filename: plugin.filename,
      name: plugin.name,
      version: plugin.version,
    };
    if (!pluginEntries) {
      Cu.reportError(
        new Error("There are no plugin entries. This should never happen.")
      );
    }
    for (let blockEntry of pluginEntries) {
      var matchFailed = false;
      for (var name in blockEntry.matches) {
        let pluginProperty = pluginProperties[name];
        if (
          typeof pluginProperty != "string" ||
          !blockEntry.matches[name].test(pluginProperty)
        ) {
          matchFailed = true;
          break;
        }
      }

      if (matchFailed) {
        continue;
      }

      for (let versionRange of blockEntry.versionRange) {
        if (
          Utils.versionsMatch(
            versionRange,
            pluginProperties.version,
            appVersion,
            toolkitVersion
          )
        ) {
          return { entry: blockEntry, version: versionRange };
        }
      }
    }

    return null;
  },

  /**
   * Private version of getState that allows the caller to pass in
   * the plugin blocklist entries.
   *
   * @param {nsIPluginTag} plugin
   *        The nsIPluginTag to get the blocklist state for.
   * @param {object[]} pluginEntries
   *        The plugin blocklist entries to compare against.
   * @param {string?} appVersion
   *        The application version to compare to, will use the current
   *        version if null.
   * @param {string?} toolkitVersion
   *        The toolkit version to compare to, will use the current version if
   *        null.
   * @returns {integer}
   *        The blocklist state for the item, one of the STATE constants as
   *        defined in nsIBlocklistService.
   */
  _getState(plugin, pluginEntries, appVersion, toolkitVersion) {
    let r = this._getEntry(plugin, pluginEntries, appVersion, toolkitVersion);
    if (!r) {
      return Ci.nsIBlocklistService.STATE_NOT_BLOCKED;
    }

    let { version: versionRange } = r;

    if (versionRange.severity >= gBlocklistLevel) {
      return Ci.nsIBlocklistService.STATE_BLOCKED;
    }
    if (versionRange.severity == SEVERITY_OUTDATED) {
      let vulnerabilityStatus = versionRange.vulnerabilityStatus;
      if (vulnerabilityStatus == VULNERABILITYSTATUS_UPDATE_AVAILABLE) {
        return Ci.nsIBlocklistService.STATE_VULNERABLE_UPDATE_AVAILABLE;
      }
      if (vulnerabilityStatus == VULNERABILITYSTATUS_NO_UPDATE) {
        return Ci.nsIBlocklistService.STATE_VULNERABLE_NO_UPDATE;
      }
      return Ci.nsIBlocklistService.STATE_OUTDATED;
    }
    return Ci.nsIBlocklistService.STATE_SOFTBLOCKED;
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
 *
 * Note: we assign to the global to allow tests to reach the object directly.
 */
this.ExtensionBlocklistRS = {
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

  async _filterItem(entry) {
    if (!(await targetAppFilter(entry))) {
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
    this._client = RemoteSettings(
      Services.prefs.getCharPref(PREF_BLOCKLIST_ADDONS_COLLECTION),
      {
        bucketNamePref: PREF_BLOCKLIST_BUCKET,
        lastCheckTimePref: PREF_BLOCKLIST_ADDONS_CHECKED_SECONDS,
        signerName: Services.prefs.getCharPref(PREF_BLOCKLIST_ADDONS_SIGNER),
        filterFunc: this._filterItem,
      }
    );
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

    const types = ["extension", "theme", "locale", "dictionary", "service"];
    let addons = await AddonManager.getAddonsByTypes(types);
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
        addon.softDisabled = false;
      }

      // If an add-on has dropped from hard to soft blocked just mark it as
      // soft disabled and don't warn about it.
      if (
        state == Ci.nsIBlocklistService.STATE_SOFTBLOCKED &&
        oldState == Ci.nsIBlocklistService.STATE_BLOCKED
      ) {
        addon.softDisabled = true;
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
          addon.softDisabled = true;
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

    AddonManagerPrivate.updateAddonAppDisabledStates();
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
    if (!appVersion && !gApp.version) {
      return null;
    }

    if (!appVersion) {
      appVersion = gApp.version;
    }
    if (!toolkitVersion) {
      toolkitVersion = gApp.platformVersion;
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
 * To update the blocklist, a replacement MLBF is published:
 *
 * {
 *   "generation_time": 1585692000000,
 *   "attachment": { ... RemoteSettings attachment ... }
 *   "attachment_type": "bloomfilter-full",
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
 * The use of stashes is currently optional, and toggled via the
 * extensions.blocklist.useMLBF.stashes preference (true = use stashes).
 *
 * Note: we assign to the global to allow tests to reach the object directly.
 */
this.ExtensionBlocklistMLBF = {
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
    } = await this._client.attachments.download(record, {
      attachmentId: this.RS_ATTACHMENT_ID,
      useCache: true,
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
      let mlbfRecord;
      if (this.stashesEnabled) {
        mlbfRecord = mlbfRecords.find(
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
          .map(({ stash }) => ({
            blocked: new Set(stash.blocked),
            unblocked: new Set(stash.unblocked),
          }));
      } else {
        mlbfRecord = mlbfRecords.find(
          r =>
            r.attachment_type == "bloomfilter-full" ||
            r.attachment_type == "bloomfilter-base"
        );
        this._stashes = null;
      }

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
        }
        return this._updatePromise;
      });
    this._updatePromise = updatePromise;
    return updatePromise;
  },

  ensureInitialized() {
    if (!gBlocklistEnabled || this._initialized) {
      return;
    }
    this._initialized = true;
    this._client = RemoteSettings(
      Services.prefs.getCharPref(PREF_BLOCKLIST_ADDONS3_COLLECTION),
      {
        bucketNamePref: PREF_BLOCKLIST_BUCKET,
        lastCheckTimePref: PREF_BLOCKLIST_ADDONS3_CHECKED_SECONDS,
        signerName: Services.prefs.getCharPref(PREF_BLOCKLIST_ADDONS3_SIGNER),
      }
    );
    this._onUpdate = this._onUpdate.bind(this);
    this._client.on("sync", this._onUpdate);
    this.stashesEnabled = Services.prefs.getBoolPref(
      PREF_BLOCKLIST_USE_MLBF_STASHES,
      false
    );
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

    // Check add-ons from XPIProvider.
    const types = ["extension", "theme", "locale", "dictionary"];
    let addons = await AddonManager.getAddonsByTypes(types);
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
        addon.softDisabled = false;
      }
    }

    AddonManagerPrivate.updateAddonAppDisabledStates();
  },

  async getState(addon) {
    let state = await this.getEntry(addon);
    return state ? state.state : Ci.nsIBlocklistService.STATE_NOT_BLOCKED;
  },

  async getEntry(addon) {
    if (!this._mlbfData) {
      this.ensureInitialized();
      await this._updateMLBF(false);
    }

    let blockKey = addon.id + ":" + addon.version;

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
      // ... in other words, this shouldn't happen in practice.
      return null;
    }
    let { cascadeFilter, generationTime } = this._mlbfData;
    if (!cascadeFilter.has(blockKey)) {
      // Add-on not blocked or unknown.
      return null;
    }
    // Add-on blocked, or unknown add-on inadvertently labeled as blocked.

    if (signedDate.getTime() > generationTime) {
      // The bloom filter only reports 100% accurate results for known add-ons.
      // Since the add-on was unknown when the bloom filter was generated, the
      // block decision is incorrect and should be treated as unblocked.
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

// From appinfo in Services.jsm. It is not possible to use the one in
// Services.jsm since it will not successfully QueryInterface nsIXULAppInfo in
// xpcshell tests due to other code calling Services.appinfo before the
// nsIXULAppInfo is created by the tests.
XPCOMUtils.defineLazyGetter(this, "gApp", function() {
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

XPCOMUtils.defineLazyGetter(this, "gAppID", function() {
  return gApp.ID;
});
XPCOMUtils.defineLazyGetter(this, "gAppOS", function() {
  return gApp.OS;
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

    // If the stub blocklist service deferred any queries because we
    // weren't loaded yet, execute them now.
    for (let entry of Services.blocklist.pluginQueries.splice(0)) {
      entry.resolve(
        this.getPluginBlocklistState(
          entry.plugin,
          entry.appVersion,
          entry.toolkitVersion
        )
      );
    }
  },
  isLoaded: true,

  shutdown() {
    GfxBlocklistRS.shutdown();
    PluginBlocklistRS.shutdown();
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
            if (oldImpl._initialized) {
              oldImpl.shutdown();
              this.ExtensionBlocklist.undoShutdown();
              this.ExtensionBlocklist._onUpdate();
            } // else neither has been initialized yet. Wait for it to happen.
            break;
          case PREF_BLOCKLIST_USE_MLBF_STASHES:
            ExtensionBlocklistMLBF.stashesEnabled = Services.prefs.getBoolPref(
              PREF_BLOCKLIST_USE_MLBF_STASHES,
              false
            );
            if (
              ExtensionBlocklistMLBF._initialized &&
              !ExtensionBlocklistMLBF._didShutdown
            ) {
              ExtensionBlocklistMLBF._onUpdate();
            }
            break;
        }
        break;
    }
  },

  loadBlocklistAsync() {
    // Need to ensure we notify gfx of new stuff.
    GfxBlocklistRS.checkForEntries();
    this.ExtensionBlocklist.ensureInitialized();
    PluginBlocklistRS.ensureInitialized();
  },

  getPluginBlocklistState(plugin, appVersion, toolkitVersion) {
    return PluginBlocklistRS.getState(plugin, appVersion, toolkitVersion);
  },

  getPluginBlockURL(plugin) {
    return PluginBlocklistRS.getURL(plugin);
  },

  getAddonBlocklistState(addon, appVersion, toolkitVersion) {
    // NOTE: appVersion/toolkitVersion are only used by ExtensionBlocklistRS.
    return this.ExtensionBlocklist.getState(addon, appVersion, toolkitVersion);
  },

  getAddonBlocklistEntry(addon, appVersion, toolkitVersion) {
    // NOTE: appVersion/toolkitVersion are only used by ExtensionBlocklistRS.
    return this.ExtensionBlocklist.getEntry(addon, appVersion, toolkitVersion);
  },

  _chooseExtensionBlocklistImplementationFromPref() {
    if (Services.prefs.getBoolPref(PREF_BLOCKLIST_USE_MLBF, false)) {
      this.ExtensionBlocklist = ExtensionBlocklistMLBF;
    } else {
      this.ExtensionBlocklist = ExtensionBlocklistRS;
    }
  },

  _blocklistUpdated() {
    this.ExtensionBlocklist._onUpdate();
    PluginBlocklistRS._onUpdate();
  },
};

Blocklist._init();
