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
XPCOMUtils.defineLazyGlobalGetters(this, ["XMLHttpRequest"]);

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
  "CertUtils",
  "resource://gre/modules/CertUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "FileUtils",
  "resource://gre/modules/FileUtils.jsm"
);
ChromeUtils.defineModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");
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
ChromeUtils.defineModuleGetter(
  this,
  "ServiceRequest",
  "resource://gre/modules/ServiceRequest.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "UpdateUtils",
  "resource://gre/modules/UpdateUtils.jsm"
);

/**
#    The blocklist XML file looks something like this:
#
#    <blocklist xmlns="http://www.mozilla.org/2006/addons-blocklist">
#      <emItems>
#        <emItem id="item_1@domain" blockID="i1">
#          <prefs>
#            <pref>accessibility.accesskeycausesactivation</pref>
#            <pref>accessibility.blockautorefresh</pref>
#          </prefs>
#          <versionRange minVersion="1.0" maxVersion="2.0.*">
#            <targetApplication id="{ec8030f7-c20a-464f-9b0e-13a3a9e97384}">
#              <versionRange minVersion="1.5" maxVersion="1.5.*"/>
#              <versionRange minVersion="1.7" maxVersion="1.7.*"/>
#            </targetApplication>
#            <targetApplication id="toolkit@mozilla.org">
#              <versionRange minVersion="1.9" maxVersion="1.9.*"/>
#            </targetApplication>
#          </versionRange>
#          <versionRange minVersion="3.0" maxVersion="3.0.*">
#            <targetApplication id="{ec8030f7-c20a-464f-9b0e-13a3a9e97384}">
#              <versionRange minVersion="1.5" maxVersion="1.5.*"/>
#            </targetApplication>
#            <targetApplication id="toolkit@mozilla.org">
#              <versionRange minVersion="1.9" maxVersion="1.9.*"/>
#            </targetApplication>
#          </versionRange>
#        </emItem>
#        <emItem id="item_2@domain" blockID="i2">
#          <versionRange minVersion="3.1" maxVersion="4.*"/>
#        </emItem>
#        <emItem id="item_3@domain">
#          <versionRange>
#            <targetApplication id="{ec8030f7-c20a-464f-9b0e-13a3a9e97384}">
#              <versionRange minVersion="1.5" maxVersion="1.5.*"/>
#            </targetApplication>
#          </versionRange>
#        </emItem>
#        <emItem id="item_4@domain" blockID="i3">
#          <versionRange>
#            <targetApplication>
#              <versionRange minVersion="1.5" maxVersion="1.5.*"/>
#            </targetApplication>
#          </versionRange>
#        <emItem id="/@badperson\.com$/"/>
#      </emItems>
#      <pluginItems>
#        <pluginItem blockID="i4">
#          <!-- All match tags must match a plugin to blocklist a plugin -->
#          <match name="name" exp="some plugin"/>
#          <match name="description" exp="1[.]2[.]3"/>
#        </pluginItem>
#      </pluginItems>
#      <gfxItems>
#        <gfxItem ... />
#      </gfxItems>
#    </blocklist>
   */

const TOOLKIT_ID = "toolkit@mozilla.org";
const KEY_PROFILEDIR = "ProfD";
const KEY_APPDIR = "XCurProcD";
const FILE_BLOCKLIST = "blocklist.xml";
const PREF_BLOCKLIST_LASTUPDATETIME =
  "app.update.lastUpdateTime.blocklist-background-update-timer";
const PREF_BLOCKLIST_URL = "extensions.blocklist.url";
const PREF_BLOCKLIST_ITEM_URL = "extensions.blocklist.itemURL";
const PREF_BLOCKLIST_ENABLED = "extensions.blocklist.enabled";
const PREF_BLOCKLIST_LAST_MODIFIED = "extensions.blocklist.lastModified";
const PREF_BLOCKLIST_LEVEL = "extensions.blocklist.level";
const PREF_BLOCKLIST_PINGCOUNTTOTAL = "extensions.blocklist.pingCountTotal";
const PREF_BLOCKLIST_PINGCOUNTVERSION = "extensions.blocklist.pingCountVersion";
const PREF_BLOCKLIST_SUPPRESSUI = "extensions.blocklist.suppressUI";
const PREF_APP_DISTRIBUTION = "distribution.id";
const PREF_APP_DISTRIBUTION_VERSION = "distribution.version";
const PREF_EM_LOGGING_ENABLED = "extensions.logging.enabled";
const XMLURI_BLOCKLIST = "http://www.mozilla.org/2006/addons-blocklist";
const XMLURI_PARSE_ERROR =
  "http://www.mozilla.org/newlayout/xml/parsererror.xml";
const URI_BLOCKLIST_DIALOG =
  "chrome://mozapps/content/extensions/blocklist.xul";
const DEFAULT_SEVERITY = 3;
const DEFAULT_LEVEL = 2;
const MAX_BLOCK_LEVEL = 3;
const SEVERITY_OUTDATED = 0;
const VULNERABILITYSTATUS_NONE = 0;
const VULNERABILITYSTATUS_UPDATE_AVAILABLE = 1;
const VULNERABILITYSTATUS_NO_UPDATE = 2;

// Kinto blocklist constants
const PREF_BLOCKLIST_BUCKET = "services.blocklist.bucket";
const PREF_BLOCKLIST_GFX_COLLECTION = "services.blocklist.gfx.collection";
const PREF_BLOCKLIST_GFX_CHECKED_SECONDS = "services.blocklist.gfx.checked";
const PREF_BLOCKLIST_GFX_SIGNER = "services.blocklist.gfx.signer";
const PREF_BLOCKLIST_PLUGINS_COLLECTION =
  "services.blocklist.plugins.collection";
const PREF_BLOCKLIST_PLUGINS_CHECKED_SECONDS =
  "services.blocklist.plugins.checked";
const PREF_BLOCKLIST_PLUGINS_SIGNER = "services.blocklist.plugins.signer";
const PREF_BLOCKLIST_ADDONS_COLLECTION = "services.blocklist.addons.collection";
const PREF_BLOCKLIST_ADDONS_CHECKED_SECONDS =
  "services.blocklist.addons.checked";
const PREF_BLOCKLIST_ADDONS_SIGNER = "services.blocklist.addons.signer";

const BlocklistTelemetry = {
  /**
   * Record the XML Blocklist lastModified server time into the
   * "blocklist.lastModified_xml scalar.
   *
   * @param {XMLDocument} xmlDoc
   *        The blocklist XML file to retrieve the lastupdate attribute
   *        and record it into the "blocklist.lastModified_xml" scalar.
   *        The scalar value is a datetime string in UTC format.
   */
  recordXMLBlocklistLastModified(xmlDoc) {
    const lastUpdate =
      xmlDoc && xmlDoc.documentElement.getAttribute("lastupdate");
    if (lastUpdate) {
      Services.telemetry.scalarSet(
        "blocklist.lastModified_xml",
        // Date(...).toUTCString will return "Invalid Date" if the
        // timestamp isn't valid (e.g. parseInt returns NaN).
        new Date(parseInt(lastUpdate, 10)).toUTCString()
      );
    } else {
      Services.telemetry.scalarSet(
        "blocklist.lastModified_xml",
        "Missing Date"
      );
    }
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
async function targetAppFilter(entry, environment) {
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

  const { appID, version: appVersion, toolkitVersion } = environment;
  const { versionRange } = entry;

  // Everywhere in this method, we avoid checking the minVersion, because
  // we want to retain items whose minVersion is higher than the current
  // app version, so that we have the items around for app updates.

  // Gfx blocklist has a specific versionRange object, which is not a list.
  if (!Array.isArray(versionRange)) {
    const { maxVersion = "*" } = versionRange;
    const matchesRange = Services.vc.compare(appVersion, maxVersion) <= 0;
    return matchesRange ? entry : null;
  }

  // Iterate the targeted applications, at least one of them must match.
  // If no target application, keep the entry.
  if (versionRange.length == 0) {
    return entry;
  }
  for (const vr of versionRange) {
    const { targetApplication = [] } = vr;
    if (targetApplication.length == 0) {
      return entry;
    }
    for (const ta of targetApplication) {
      const { guid } = ta;
      if (!guid) {
        return entry;
      }
      const { maxVersion = "*" } = ta;
      if (guid == appID && Services.vc.compare(appVersion, maxVersion) <= 0) {
        return entry;
      }
      if (
        guid == "toolkit@mozilla.org" &&
        Services.vc.compare(toolkitVersion, maxVersion) <= 0
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
    let entries = await this._client.get();
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
  },

  async _filterItem(entry) {
    if (
      !(await targetAppFilter(entry, { appID: gAppID, version: gApp.version }))
    ) {
      return null;
    }
    if (!Utils.matchesOSABI(entry)) {
      return null;
    }
    if (!entry.matchFilename && !entry.matchName && !entry.matchDescription) {
      Cu.reportError(
        new Error("Nothing to filter plugin item " + entry.blockID + " on")
      );
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
    if (!blockEntry.blockID) {
      return null;
    }

    return blockEntry.infoURL || Utils._createBlocklistURL(blockEntry.blockID);
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
      function getCriteria(str) {
        if (!str.startsWith("/")) {
          return str;
        }
        let lastSlash = str.lastIndexOf("/");
        let pattern = str.slice(1, lastSlash);
        let flags = str.slice(lastSlash + 1);
        return new RegExp(pattern, flags);
      }
      entry.matches = {};
      if (entry.guid) {
        entry.matches.id = getCriteria(entry.guid);
      }
      for (let key of EXTENSION_BLOCK_FILTERS) {
        if (key == "id" || !entry[key]) {
          continue;
        }
        entry.matches[key] = getCriteria(entry[key]);
      }
      Utils.ensureVersionRangeIsSane(entry);
    });
  },

  async _filterItem(entry) {
    if (
      !(await targetAppFilter(entry, { appID: gAppID, version: gApp.version }))
    ) {
      return null;
    }
    if (!Utils.matchesOSABI(entry)) {
      return null;
    }
    // Need something to filter on - at least a guid or name (either could be a regex):
    if (!entry.guid && !entry.name) {
      Cu.reportError(
        new Error("Nothing to filter add-on item " + entry.blockID + " on")
      );
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

    let propMatches = ([k, v]) => {
      return (
        !v ||
        addonProps[k] == v ||
        (v instanceof RegExp && v.test(addonProps[k]))
      );
    };
    for (let entry of addonEntries) {
      // First check if it matches our properties. If not, just skip to the next item.
      if (!Object.entries(entry.matches).every(propMatches)) {
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
          return {
            state:
              versionRange.severity >= gBlocklistLevel
                ? Ci.nsIBlocklistService.STATE_BLOCKED
                : Ci.nsIBlocklistService.STATE_SOFTBLOCKED,
            url: entry.blockID && Utils._createBlocklistURL(entry.blockID),
            prefs: entry.prefs || [],
          };
        }
      }
    }
    return null;
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

class BlocklistError extends Error {}

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
XPCOMUtils.defineLazyGetter(this, "gAppVersion", function() {
  return gApp.version;
});
XPCOMUtils.defineLazyGetter(this, "gAppOS", function() {
  return gApp.OS;
});

XPCOMUtils.defineLazyGetter(this, "gABI", function() {
  let abi = null;
  try {
    abi = gApp.XPCOMABI;
  } catch (e) {
    LOG("BlockList Global gABI: XPCOM ABI unknown.");
  }
  return abi;
});

XPCOMUtils.defineLazyGetter(this, "gOSVersion", function() {
  let osVersion;
  try {
    osVersion =
      Services.sysinfo.getProperty("name") +
      " " +
      Services.sysinfo.getProperty("version");
  } catch (e) {
    LOG("BlockList Global gOSVersion: OS Version unknown.");
  }

  if (osVersion) {
    try {
      osVersion +=
        " (" + Services.sysinfo.getProperty("secondaryLibrary") + ")";
    } catch (e) {
      // Not all platforms have a secondary widget library, so an error is nothing to worry about.
    }
    osVersion = encodeURIComponent(osVersion);
  }
  return osVersion;
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

// Restarts the application checking in with observers first
function restartApp() {
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
}

/**
 * Checks whether this blocklist element is valid for the current OS and ABI.
 * If the element has an "os" attribute then the current OS must appear in
 * its comma separated list for the element to be valid. Similarly for the
 * xpcomabi attribute.
 *
 * @param {Element} blocklistElement
 *        The blocklist element from an XML blocklist.
 * @returns {bool}
 *        Whether the entry matches the current OS.
 */
function matchesOSABI(blocklistElement) {
  let os = blocklistElement.getAttribute("os");
  if (os) {
    let choices = os.split(",");
    if (choices.length > 0 && !choices.includes(gApp.OS)) {
      return false;
    }
  }

  let xpcomabi = blocklistElement.getAttribute("xpcomabi");
  if (xpcomabi) {
    let choices = xpcomabi.split(",");
    if (choices.length > 0 && !choices.includes(gApp.XPCOMABI)) {
      return false;
    }
  }

  return true;
}

/* Get the distribution pref values, from defaults only */
function getDistributionPrefValue(aPrefName) {
  return Services.prefs
    .getDefaultBranch(null)
    .getCharPref(aPrefName, "default");
}

let gLoadingWasTriggered = false;

/**
 * Manages the Blocklist. The Blocklist is a representation of the contents of
 * blocklist.xml and allows us to remotely disable / re-enable blocklisted
 * items managed by the Extension Manager with an item's appDisabled property.
 * It also blocklists plugins with data from blocklist.xml.
 */
var BlocklistXML = {
  _init() {
    if (gLoadingWasTriggered) {
      this.loadBlocklistAsync();
    }
  },

  STATE_NOT_BLOCKED: Ci.nsIBlocklistService.STATE_NOT_BLOCKED,
  STATE_SOFTBLOCKED: Ci.nsIBlocklistService.STATE_SOFTBLOCKED,
  STATE_BLOCKED: Ci.nsIBlocklistService.STATE_BLOCKED,
  STATE_OUTDATED: Ci.nsIBlocklistService.STATE_OUTDATED,
  STATE_VULNERABLE_UPDATE_AVAILABLE:
    Ci.nsIBlocklistService.STATE_VULNERABLE_UPDATE_AVAILABLE,
  STATE_VULNERABLE_NO_UPDATE: Ci.nsIBlocklistService.STATE_VULNERABLE_NO_UPDATE,

  /**
   * Extension ID -> array of Version Ranges
   * Each value in the version range array is a JS Object that has the
   * following properties:
   *   "minVersion"  The minimum version in a version range (default = 0)
   *   "maxVersion"  The maximum version in a version range (default = *)
   *   "targetApps"  Application ID -> array of Version Ranges
   *                 (default = current application ID)
   *                 Each value in the version range array is a JS Object that
   *                 has the following properties:
   *                   "minVersion"  The minimum version in a version range
   *                                 (default = 0)
   *                   "maxVersion"  The maximum version in a version range
   *                                 (default = *)
   */
  _addonEntries: null,
  _gfxEntries: null,
  _pluginEntries: null,

  get profileBlocklistPath() {
    let path = OS.Path.join(OS.Constants.Path.profileDir, FILE_BLOCKLIST);
    Object.defineProperty(this, "profileBlocklistPath", { value: path });
    return path;
  },

  shutdown() {},

  _onBlocklistEnabledToggle() {
    // This is a bit messy. Especially in tests, but in principle also by users toggling
    // this preference repeatedly, plugin loads could race with each other if we don't
    // enforce that they are applied sequentially.
    // So we only update once the previous `_blocklistUpdated` call finishes running.
    let lastUpdate = this._lastUpdate || undefined;
    let newUpdate = (this._lastUpdate = (async () => {
      await lastUpdate;
      this._clear();
      await this.loadBlocklistAsync();
      await this._blocklistUpdated(null, null);
      if (newUpdate == this._lastUpdate) {
        delete this._lastUpdate;
      }
    })().catch(Cu.reportError));
  },

  /**
   * Determine the blocklist state of an add-on
   * @param {Addon} addon
   *        The addon item to be checked.
   * @param {string?} appVersion
   *        The version of the application we are checking in the blocklist.
   *        If this parameter is null, the version of the running application
   *        is used.
   * @param {string?} toolkitVersion
   *        The version of the toolkit we are checking in the blocklist.
   *        If this parameter is null, the version of the running toolkit
   *        is used.
   * @returns {integer} The STATE constant.
   */
  async getAddonBlocklistState(addon, appVersion, toolkitVersion) {
    await this.loadBlocklistAsync();
    return this._getAddonBlocklistState(
      addon,
      this._addonEntries,
      appVersion,
      toolkitVersion
    );
  },

  /**
   * Returns a matching blocklist entry for the given add-on, if one
   * exists.
   *
   * @param {Addon} addon
   *        The add-on object of the item to get the blocklist state for.
   * @param {object[]} addonEntries
   *        The add-on blocklist entries to compare against.
   * @param {string?} appVersion
   *        The application version to compare to, will use the current
   *        version if null.
   * @param {string?} toolkitVersion
   *        The toolkit version to compare to, will use the current version if
   *        null.
   * @returns {object?}
   *          A blocklist entry for this item, with `state` and `url`
   *          properties indicating the block state and URL, if there is
   *          a matching blocklist entry, or null otherwise.
   */
  _getAddonBlocklistEntry(addon, addonEntries, appVersion, toolkitVersion) {
    if (!gBlocklistEnabled) {
      return null;
    }

    // Not all applications implement nsIXULAppInfo (e.g. xpcshell doesn't).
    if (!appVersion && !gAppVersion) {
      return null;
    }

    if (!appVersion) {
      appVersion = gAppVersion;
    }
    if (!toolkitVersion) {
      toolkitVersion = gApp.platformVersion;
    }

    var blItem = this._findMatchingAddonEntry(addonEntries, addon);
    if (!blItem) {
      return null;
    }

    for (let currentblItem of blItem.versions) {
      if (
        currentblItem.includesItem(addon.version, appVersion, toolkitVersion)
      ) {
        return {
          state:
            currentblItem.severity >= gBlocklistLevel
              ? Ci.nsIBlocklistService.STATE_BLOCKED
              : Ci.nsIBlocklistService.STATE_SOFTBLOCKED,
          url: blItem.blockID && this._createBlocklistURL(blItem.blockID),
        };
      }
    }
    return null;
  },

  /**
   * Returns a promise that resolves to the blocklist entry.
   * The blocklist entry is an object with `state` and `url`
   * properties, if a blocklist entry for the add-on exists, or null
   * otherwise.

   * @param {Addon} addon
   *          The addon object to match.
   * @param {string?} appVersion
   *        The version of the application we are checking in the blocklist.
   *        If this parameter is null, the version of the running application
   *        is used.
   * @param {string?} toolkitVersion
   *        The version of the toolkit we are checking in the blocklist.
   *        If this parameter is null, the version of the running toolkit
   *        is used.
   * @returns {Promise<object?>}
   *        The blocklist entry for the add-on, if one exists, or null
   *        otherwise.
   */
  async getAddonBlocklistEntry(addon, appVersion, toolkitVersion) {
    await this.loadBlocklistAsync();
    return this._getAddonBlocklistEntry(
      addon,
      this._addonEntries,
      appVersion,
      toolkitVersion
    );
  },

  /**
   * Private version of getAddonBlocklistState that allows the caller to pass in
   * the add-on blocklist entries to compare against.
   *
   * @param {Addon} addon
   *        The add-on object of the item to get the blocklist state for.
   * @param {object[]} addonEntries
   *        The add-on blocklist entries to compare against.
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
  _getAddonBlocklistState(addon, addonEntries, appVersion, toolkitVersion) {
    let entry = this._getAddonBlocklistEntry(
      addon,
      addonEntries,
      appVersion,
      toolkitVersion
    );
    if (entry) {
      return entry.state;
    }
    return Ci.nsIBlocklistService.STATE_NOT_BLOCKED;
  },

  /**
   * Returns the set of prefs of the add-on stored in the blocklist file
   * (probably to revert them on disabling).
   * @param {Addon} addon
   *        The add-on whose to-be-reset prefs are to be found.
   * @returns {string[]}
   *        An array of preference names.
   */
  _getAddonPrefs(addon) {
    let entry = this._findMatchingAddonEntry(this._addonEntries, addon);
    return entry.prefs.slice(0);
  },

  _findMatchingAddonEntry(aAddonEntries, aAddon) {
    if (!aAddon) {
      return null;
    }
    // Returns true if the params object passes the constraints set by entry.
    // (For every non-null property in entry, the same key must exist in
    // params and value must be the same)
    function checkEntry(entry, params) {
      for (let [key, value] of Object.entries(entry)) {
        if (value === null || value === undefined) {
          continue;
        }
        if (params[key]) {
          if (value instanceof RegExp) {
            if (!value.test(params[key])) {
              return false;
            }
          } else if (value !== params[key]) {
            return false;
          }
        } else {
          return false;
        }
      }
      return true;
    }

    let params = {};
    for (let filter of EXTENSION_BLOCK_FILTERS) {
      params[filter] = aAddon[filter];
    }
    if (params.creator) {
      params.creator = params.creator.name;
    }
    for (let entry of aAddonEntries) {
      if (checkEntry(entry.attributes, params)) {
        return entry;
      }
    }
    return null;
  },

  _createBlocklistURL(id) {
    let url = Services.urlFormatter.formatURLPref(PREF_BLOCKLIST_ITEM_URL);
    return url.replace(/%blockID%/g, id);
  },

  notify(aTimer) {
    if (!gBlocklistEnabled) {
      return;
    }

    try {
      var dsURI = Services.prefs.getCharPref(PREF_BLOCKLIST_URL);
    } catch (e) {
      LOG(
        "Blocklist::notify: The " +
          PREF_BLOCKLIST_URL +
          " preference" +
          " is missing!"
      );
      return;
    }

    var pingCountVersion = Services.prefs.getIntPref(
      PREF_BLOCKLIST_PINGCOUNTVERSION,
      0
    );
    var pingCountTotal = Services.prefs.getIntPref(
      PREF_BLOCKLIST_PINGCOUNTTOTAL,
      1
    );
    var daysSinceLastPing = 0;
    if (pingCountVersion == 0) {
      daysSinceLastPing = "new";
    } else {
      // Seconds in one day is used because nsIUpdateTimerManager stores the
      // last update time in seconds.
      let secondsInDay = 60 * 60 * 24;
      let lastUpdateTime = Services.prefs.getIntPref(
        PREF_BLOCKLIST_LASTUPDATETIME,
        0
      );
      if (lastUpdateTime == 0) {
        daysSinceLastPing = "invalid";
      } else {
        let now = Math.round(Date.now() / 1000);
        daysSinceLastPing = Math.floor((now - lastUpdateTime) / secondsInDay);
      }

      if (daysSinceLastPing == 0 || daysSinceLastPing == "invalid") {
        pingCountVersion = pingCountTotal = "invalid";
      }
    }

    if (pingCountVersion < 1) {
      pingCountVersion = 1;
    }
    if (pingCountTotal < 1) {
      pingCountTotal = 1;
    }

    let replacements = {
      APP_ID: gAppID,
      PRODUCT: gApp.name,
      BUILD_ID: gApp.appBuildID,
      BUILD_TARGET: gApp.OS + "_" + gABI,
      OS_VERSION: gOSVersion,
      LOCALE: Services.locale.requestedLocale,
      CHANNEL: UpdateUtils.UpdateChannel,
      PLATFORM_VERSION: gApp.platformVersion,
      DISTRIBUTION: getDistributionPrefValue(PREF_APP_DISTRIBUTION),
      DISTRIBUTION_VERSION: getDistributionPrefValue(
        PREF_APP_DISTRIBUTION_VERSION
      ),
      PING_COUNT: pingCountVersion,
      TOTAL_PING_COUNT: pingCountTotal,
      DAYS_SINCE_LAST_PING: daysSinceLastPing,
    };
    dsURI = dsURI.replace(/%([A-Z_]+)%/g, function(fullMatch, name) {
      // Not all applications implement nsIXULAppInfo (e.g. xpcshell doesn't).
      if (gAppVersion && (name == "APP_VERSION" || name == "VERSION")) {
        return gAppVersion;
      }
      // Some items, like DAYS_SINCE_LAST_PING, can be undefined, so we can't just
      // `return replacements[name] || fullMatch` or something like that.
      if (!replacements.hasOwnProperty(name)) {
        return fullMatch;
      }
      return replacements[name];
    });
    dsURI = dsURI.replace(/\+/g, "%2B");

    // Under normal operations it will take around 5,883,516 years before the
    // preferences used to store pingCountVersion and pingCountTotal will rollover
    // so this code doesn't bother trying to do the "right thing" here.
    if (pingCountVersion != "invalid") {
      pingCountVersion++;
      if (pingCountVersion > 2147483647) {
        // Rollover to -1 if the value is greater than what is support by an
        // integer preference. The -1 indicates that the counter has been reset.
        pingCountVersion = -1;
      }
      Services.prefs.setIntPref(
        PREF_BLOCKLIST_PINGCOUNTVERSION,
        pingCountVersion
      );
    }

    if (pingCountTotal != "invalid") {
      pingCountTotal++;
      if (pingCountTotal > 2147483647) {
        // Rollover to 1 if the value is greater than what is support by an
        // integer preference.
        pingCountTotal = -1;
      }
      Services.prefs.setIntPref(PREF_BLOCKLIST_PINGCOUNTTOTAL, pingCountTotal);
    }

    // Verify that the URI is valid
    try {
      var uri = Services.io.newURI(dsURI);
    } catch (e) {
      LOG(
        "Blocklist::notify: There was an error creating the blocklist URI\r\n" +
          "for: " +
          dsURI +
          ", error: " +
          e
      );
      return;
    }

    LOG("Blocklist::notify: Requesting " + uri.spec);
    let request = new ServiceRequest({ mozAnon: true });
    request.open("GET", uri.spec, true);
    request.channel.notificationCallbacks = new CertUtils.BadCertHandler();
    request.overrideMimeType("text/xml");

    // The server will return a `304 Not Modified` response if the blocklist was
    // not changed since last check.
    const lastModified = Services.prefs.getCharPref(
      PREF_BLOCKLIST_LAST_MODIFIED,
      ""
    );
    if (lastModified) {
      request.setRequestHeader("If-Modified-Since", lastModified);
    } else {
      request.setRequestHeader("Cache-Control", "no-cache");
    }

    request.addEventListener("error", event => this.onXMLError(event));
    request.addEventListener("load", event => this.onXMLLoad(event));
    request.send(null);
  },

  async onXMLLoad(aEvent) {
    let request = aEvent.target;
    try {
      CertUtils.checkCert(request.channel);
    } catch (e) {
      LOG("Blocklist::onXMLLoad: " + e);
      return;
    }

    let { status } = request;
    if (status == 304) {
      LOG("Blocklist::onXMLLoad: up to date.");
      return;
    }

    if (status != 200 && status != 0) {
      LOG(
        "Blocklist::onXMLLoad: there was an error during load, got status: " +
          status
      );
      return;
    }

    let { responseXML } = request;
    if (
      !responseXML ||
      responseXML.documentElement.namespaceURI == XMLURI_PARSE_ERROR
    ) {
      LOG(
        "Blocklist::onXMLLoad: there was an error during load, we got invalid XML"
      );
      return;
    }

    // Save current blocklist timestamp to pref.
    const lastModified = request.getResponseHeader("Last-Modified") || "";
    Services.prefs.setCharPref(PREF_BLOCKLIST_LAST_MODIFIED, lastModified);

    if (!this.isLoaded) {
      await this.loadBlocklistAsync();
    }

    var oldAddonEntries = this._addonEntries;
    var oldPluginEntries = this._pluginEntries;

    await this._loadBlocklistFromXML(responseXML);
    // We don't inform the users when the graphics blocklist changed at runtime.
    // However addons and plugins blocking status is refreshed.
    this._blocklistUpdated(oldAddonEntries, oldPluginEntries);

    try {
      let path = this.profileBlocklistPath;
      await OS.File.writeAtomic(path, request.responseText, {
        tmpPath: path + ".tmp",
      });
    } catch (e) {
      LOG("Blocklist::onXMLLoad: " + e);
    }
  },

  onXMLError(aEvent) {
    try {
      var request = aEvent.target;
      // the following may throw (e.g. a local file or timeout)
      var status = request.status;
    } catch (e) {
      request = aEvent.target.channel.QueryInterface(Ci.nsIRequest);
      status = request.status;
    }
    var statusText = "XMLHttpRequest channel unavailable";
    // When status is 0 we don't have a valid channel.
    if (status != 0) {
      try {
        statusText = request.statusText;
      } catch (e) {}
    }
    LOG(
      "Blocklist:onError: There was an error loading the blocklist file\r\n" +
        statusText
    );
  },

  /**
   * Whether or not we've finished loading the blocklist.
   */
  get isLoaded() {
    return (
      this._addonEntries != null &&
      this._gfxEntries != null &&
      this._pluginEntries != null
    );
  },

  /* Used for testing */
  _clear() {
    this._addonEntries = null;
    this._gfxEntries = null;
    this._pluginEntries = null;
    delete this._loadPromise;
  },

  /**
   * Trigger loading the blocklist content asynchronously.
   */
  async loadBlocklistAsync() {
    if (this.isLoaded) {
      return;
    }
    if (!this._loadPromise) {
      this._loadPromise = this._loadBlocklistAsyncInternal();
    }
    await this._loadPromise;
  },

  async _loadBlocklistAsyncInternal() {
    if (this.isLoaded) {
      return;
    }

    if (!gBlocklistEnabled) {
      LOG("Blocklist::loadBlocklist: blocklist is disabled");
      return;
    }

    let xmlDoc;
    try {
      // Get the path inside the try...catch because there's no profileDir in e.g. xpcshell tests.
      let profFile = FileUtils.getFile(KEY_PROFILEDIR, [FILE_BLOCKLIST]);
      xmlDoc = await this._loadFile(profFile);
    } catch (e) {
      LOG("Blocklist::loadBlocklistAsync: Failed to load XML file " + e);
    }

    // If there's no blocklist.xml in the profile, read it from the
    // application.  Even if blocklist.xml exists in the profile, if this
    // is the first run of a new application version, read the application
    // version anyway and check if it is newer.
    if (!xmlDoc || AddonManagerPrivate.browserUpdated) {
      var appFile = FileUtils.getFile(KEY_APPDIR, [FILE_BLOCKLIST]);
      let appDoc;
      try {
        appDoc = await this._loadFile(appFile);
      } catch (e) {
        LOG("Blocklist::loadBlocklistAsync: Failed to load XML file " + e);
      }

      if (xmlDoc && appDoc) {
        // If we have a new blocklist.xml in the application directory,
        // delete the older one from the profile so we'll fall back to
        // the app version on subsequent startups.
        let clearProfile = false;
        if (!xmlDoc.documentElement.hasAttribute("lastupdate")) {
          clearProfile = true;
        } else {
          let profileTS = parseInt(
            xmlDoc.documentElement.getAttribute("lastupdate"),
            10
          );
          let appTS = parseInt(
            appDoc.documentElement.getAttribute("lastupdate"),
            10
          );
          if (appTS > profileTS) {
            clearProfile = true;
          }
        }

        if (clearProfile) {
          await OS.File.remove(this.profileBlocklistPath);
          xmlDoc = null;
        }
      }

      if (!xmlDoc) {
        // If we get here, either there was not a blocklist.xml in the
        // profile, or it was deleted for being too old.  Fall back to
        // the version from the application instead.
        xmlDoc = appDoc;
      }
    }

    if (xmlDoc) {
      await new Promise(resolve => {
        ChromeUtils.idleDispatch(async () => {
          if (!this.isLoaded) {
            await this._loadBlocklistFromXML(xmlDoc);
          }
          resolve();
        });
      });
      return;
    }

    LOG("Blocklist::loadBlocklistAsync: no XML File found");
    // Neither file is present, so we just add empty lists, to avoid JS errors fetching
    // blocklist information otherwise.
    this._addonEntries = [];
    this._gfxEntries = [];
    this._pluginEntries = [];
  },

  // Loads the given file as xml, resolving with an XMLDocument.
  // Rejects if the file cannot be loaded or if it cannot be parsed as xml.
  _loadFile(file) {
    return new Promise((resolve, reject) => {
      let request = new XMLHttpRequest();
      request.open("GET", Services.io.newFileURI(file).spec, true);
      request.overrideMimeType("text/xml");
      request.addEventListener("error", reject);
      request.addEventListener("load", function() {
        let { status } = request;
        if (status != 200 && status != 0) {
          LOG(
            "_loadFile: there was an error during load, got status: " + status
          );
          reject(new BlocklistError("Couldn't load blocklist file"));
          return;
        }
        let doc = request.responseXML;
        if (doc.documentElement.namespaceURI != XMLURI_BLOCKLIST) {
          LOG(
            "_loadFile: aborting due to incorrect XML Namespace.\n" +
              `Expected: ${XMLURI_BLOCKLIST}\n` +
              `Received: ${doc.documentElement.namespaceURI}`
          );
          reject(
            new BlocklistError("Local blocklist file has the wrong namespace!")
          );
          return;
        }
        resolve(doc);
      });
      request.send(null);
    });
  },

  async _loadBlocklistFromXML(doc) {
    this._addonEntries = [];
    this._gfxEntries = [];
    this._pluginEntries = [];
    try {
      var children = doc.documentElement.children;
      for (let element of children) {
        switch (element.localName) {
          case "emItems":
            this._addonEntries = await this._processItemNodes(
              element.children,
              "emItem",
              this._handleEmItemNode
            );
            break;
          case "pluginItems":
            this._pluginEntries = await this._processItemNodes(
              element.children,
              "pluginItem",
              this._handlePluginItemNode
            );
            break;
          case "gfxItems":
            // Parse as simple list of objects.
            this._gfxEntries = await this._processItemNodes(
              element.children,
              "gfxBlacklistEntry",
              this._handleGfxBlacklistNode
            );
            break;
          default:
            LOG(
              "Blocklist::_loadBlocklistFromXML: ignored entries " +
                element.localName
            );
        }
      }
      if (this._gfxEntries.length > 0) {
        this._notifyObserversBlocklistGFX();
      }
    } catch (e) {
      LOG(
        "Blocklist::_loadBlocklistFromXML: Error constructing blocklist " + e
      );
    }

    // Record the last modified timestamp right before notify the observers.
    BlocklistTelemetry.recordXMLBlocklistLastModified(doc);

    // Dispatch to mainthread because consumers may try to construct nsIPluginHost
    // again based on this notification, while we were called from nsIPluginHost
    // anyway, leading to re-entrancy.
    Services.tm.dispatchToMainThread(function() {
      Services.obs.notifyObservers(null, "blocklist-loaded");
    });
  },

  async _processItemNodes(items, itemName, handler) {
    var result = [];
    let deadline = await new Promise(ChromeUtils.idleDispatch);
    for (let item of items) {
      if (item.localName == itemName) {
        handler(item, result);
      }
      if (!deadline || deadline.didTimeout || deadline.timeRemaining() < 1) {
        deadline = await new Promise(ChromeUtils.idleDispatch);
      }
    }
    return result;
  },

  _handleEmItemNode(blocklistElement, result) {
    if (!matchesOSABI(blocklistElement)) {
      return;
    }

    let blockEntry = {
      versions: [],
      prefs: [],
      blockID: null,
      attributes: {},
      // Atleast one of EXTENSION_BLOCK_FILTERS must get added to attributes
    };

    for (let filter of EXTENSION_BLOCK_FILTERS) {
      let attr = blocklistElement.getAttribute(filter);
      if (attr) {
        // Any filter starting with '/' is interpreted as a regex. So if an attribute
        // starts with a '/' it must be checked via a regex.
        if (attr.startsWith("/")) {
          let lastSlash = attr.lastIndexOf("/");
          let pattern = attr.slice(1, lastSlash);
          let flags = attr.slice(lastSlash + 1);
          blockEntry.attributes[filter] = new RegExp(pattern, flags);
        } else {
          blockEntry.attributes[filter] = attr;
        }
      }
    }

    var children = blocklistElement.children;

    for (let childElement of children) {
      let localName = childElement.localName;
      if (localName == "prefs" && childElement.hasChildNodes) {
        let prefElements = childElement.children;
        for (let prefElement of prefElements) {
          if (prefElement.localName == "pref") {
            blockEntry.prefs.push(prefElement.textContent);
          }
        }
      } else if (localName == "versionRange") {
        blockEntry.versions.push(new BlocklistItemData(childElement));
      }
    }
    // if only the extension ID is specified block all versions of the
    // extension for the current application.
    if (blockEntry.versions.length == 0) {
      blockEntry.versions.push(new BlocklistItemData(null));
    }

    blockEntry.blockID = blocklistElement.getAttribute("blockID");

    result.push(blockEntry);
  },

  _handlePluginItemNode(blocklistElement, result) {
    if (!matchesOSABI(blocklistElement)) {
      return;
    }

    let children = blocklistElement.children;
    var blockEntry = {
      matches: {},
      versions: [],
      blockID: null,
      infoURL: null,
    };
    var hasMatch = false;
    for (let childElement of children) {
      switch (childElement.localName) {
        case "match":
          var name = childElement.getAttribute("name");
          var exp = childElement.getAttribute("exp");
          try {
            blockEntry.matches[name] = new RegExp(exp, "m");
            hasMatch = true;
          } catch (e) {
            // Ignore invalid regular expressions
          }
          break;
        case "versionRange":
          blockEntry.versions.push(new BlocklistItemData(childElement));
          break;
        case "infoURL":
          blockEntry.infoURL = childElement.textContent;
          break;
      }
    }
    // Plugin entries require *something* to match to an actual plugin
    if (!hasMatch) {
      return;
    }
    // Add a default versionRange if there wasn't one specified
    if (blockEntry.versions.length == 0) {
      blockEntry.versions.push(new BlocklistItemData(null));
    }

    blockEntry.blockID = blocklistElement.getAttribute("blockID");

    result.push(blockEntry);
  },

  // <gfxBlacklistEntry blockID="g60">
  //   <os>WINNT 6.0</os>
  //   <osversion>14</osversion> currently only used for Android
  //   <versionRange minVersion="42.0" maxVersion="13.0b2"/>
  //   <vendor>0x8086</vendor>
  //   <devices>
  //     <device>0x2582</device>
  //     <device>0x2782</device>
  //   </devices>
  //   <feature> DIRECT3D_10_LAYERS </feature>
  //   <featureStatus> BLOCKED_DRIVER_VERSION </featureStatus>
  //   <driverVersion> 8.52.322.2202 </driverVersion>
  //   <driverVersionMax> 8.52.322.2202 </driverVersionMax>
  //   <driverVersionComparator> LESS_THAN_OR_EQUAL </driverVersionComparator>
  //   <model>foo</model>
  //   <product>foo</product>
  //   <manufacturer>foo</manufacturer>
  //   <hardware>foo</hardware>
  // </gfxBlacklistEntry>
  _handleGfxBlacklistNode(blocklistElement, result) {
    const blockEntry = {};

    // The blockID attribute is always present in the actual data produced on server
    // (see https://github.com/mozilla/addons-server/blob/2016.05.05/src/olympia/blocklist/templates/blocklist/blocklist.xml#L74)
    // But it is sometimes missing in test fixtures.
    if (blocklistElement.hasAttribute("blockID")) {
      blockEntry.blockID = blocklistElement.getAttribute("blockID");
    }

    for (let matchElement of blocklistElement.children) {
      let value;
      if (matchElement.localName == "devices") {
        value = [];
        for (let childElement of matchElement.children) {
          const childValue = (childElement.textContent || "").trim();
          // Make sure no empty value is added.
          if (childValue) {
            if (/,/.test(childValue)) {
              // Devices can't contain comma.
              // (c.f serialization in _notifyObserversBlocklistGFX)
              const e = new Error(`Unsupported device name ${childValue}`);
              Cu.reportError(e);
            } else {
              value.push(childValue);
            }
          }
        }
      } else if (matchElement.localName == "versionRange") {
        value = {
          minVersion:
            (matchElement.getAttribute("minVersion") || "").trim() || "0",
          maxVersion:
            (matchElement.getAttribute("maxVersion") || "").trim() || "*",
        };
      } else {
        value = (matchElement.textContent || "").trim();
      }
      if (value) {
        blockEntry[matchElement.localName] = value;
      }
    }
    result.push(blockEntry);
  },

  /* See nsIBlocklistService */
  async getPluginBlocklistState(plugin, appVersion, toolkitVersion) {
    if (AppConstants.platform == "android") {
      return Ci.nsIBlocklistService.STATE_NOT_BLOCKED;
    }
    await this.loadBlocklistAsync();
    return this._getPluginBlocklistState(
      plugin,
      this._pluginEntries,
      appVersion,
      toolkitVersion
    );
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
  _getPluginBlocklistEntry(plugin, pluginEntries, appVersion, toolkitVersion) {
    if (!gBlocklistEnabled) {
      return null;
    }

    // Not all applications implement nsIXULAppInfo (e.g. xpcshell doesn't).
    if (!appVersion && !gAppVersion) {
      return Ci.nsIBlocklistService.STATE_NOT_BLOCKED;
    }

    if (!appVersion) {
      appVersion = gAppVersion;
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
    for (var blockEntry of pluginEntries) {
      var matchFailed = false;
      for (var name in blockEntry.matches) {
        let pluginProperty = pluginProperties[name];
        if (
          typeof pluginProperty !== "string" ||
          !blockEntry.matches[name].test(pluginProperty)
        ) {
          matchFailed = true;
          break;
        }
      }

      if (matchFailed) {
        continue;
      }

      for (let blockEntryVersion of blockEntry.versions) {
        if (
          blockEntryVersion.includesItem(
            pluginProperties.version,
            appVersion,
            toolkitVersion
          )
        ) {
          return { entry: blockEntry, version: blockEntryVersion };
        }
      }
    }

    return null;
  },

  /**
   * Private version of getPluginBlocklistState that allows the caller to pass in
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
  _getPluginBlocklistState(plugin, pluginEntries, appVersion, toolkitVersion) {
    let r = this._getPluginBlocklistEntry(
      plugin,
      pluginEntries,
      appVersion,
      toolkitVersion
    );
    if (!r) {
      return Ci.nsIBlocklistService.STATE_NOT_BLOCKED;
    }

    let { version: blockEntryVersion } = r;

    if (blockEntryVersion.severity >= gBlocklistLevel) {
      return Ci.nsIBlocklistService.STATE_BLOCKED;
    }
    if (blockEntryVersion.severity == SEVERITY_OUTDATED) {
      let vulnerabilityStatus = blockEntryVersion.vulnerabilityStatus;
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

  async getPluginBlockURL(plugin) {
    await this.loadBlocklistAsync();

    let r = this._getPluginBlocklistEntry(plugin, this._pluginEntries);
    if (!r) {
      return null;
    }
    let blockEntry = r.entry;
    if (!blockEntry.blockID) {
      return null;
    }

    return blockEntry.infoURL || this._createBlocklistURL(blockEntry.blockID);
  },

  _notifyObserversBlocklistGFX() {
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
    // This way we avoid spreading XML structure logics there.
    let payload = [];
    for (let gfxEntry of this._gfxEntries) {
      let entryLines = [];
      for (let key of sortedProps) {
        if (gfxEntry[key]) {
          let value = gfxEntry[key];
          if (Array.isArray(value)) {
            value = value.join(",");
          } else if (value.maxVersion) {
            // When XML is parsed, both minVersion and maxVersion are set.
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
  },

  _notifyObserversBlocklistUpdated() {
    Services.obs.notifyObservers(this, "addon-blocklist-updated");
    Services.obs.notifyObservers(this, "plugin-blocklist-updated");
  },

  async _blocklistUpdated(oldAddonEntries, oldPluginEntries) {
    var addonList = [];

    // A helper function that reverts the prefs passed to default values.
    function resetPrefs(prefs) {
      for (let pref of prefs) {
        Services.prefs.clearUserPref(pref);
      }
    }
    const types = ["extension", "theme", "locale", "dictionary", "service"];
    let addons = await AddonManager.getAddonsByTypes(types);
    for (let addon of addons) {
      let oldState = addon.blocklistState;
      if (addon.updateBlocklistState) {
        await addon.updateBlocklistState(false);
      } else if (oldAddonEntries) {
        oldState = this._getAddonBlocklistState(addon, oldAddonEntries);
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

      if (state === Ci.nsIBlocklistService.STATE_BLOCKED) {
        // It's a hard block. We must reset certain preferences.
        let prefs = this._getAddonPrefs(addon);
        resetPrefs(prefs);
      }

      // Ensure that softDisabled is false if the add-on is not soft blocked
      if (state != Ci.nsIBlocklistService.STATE_SOFTBLOCKED) {
        addon.softDisabled = false;
      }

      // Don't warn about add-ons becoming unblocked.
      if (state == Ci.nsIBlocklistService.STATE_NOT_BLOCKED) {
        continue;
      }

      // If an add-on has dropped from hard to soft blocked just mark it as
      // soft disabled and don't warn about it.
      if (
        state == Ci.nsIBlocklistService.STATE_SOFTBLOCKED &&
        oldState == Ci.nsIBlocklistService.STATE_BLOCKED
      ) {
        addon.softDisabled = true;
        continue;
      }

      // If the add-on is already disabled for some reason then don't warn
      // about it
      if (!addon.isActive) {
        // But mark it as softblocked if necessary. Note that we avoid setting
        // softDisabled at the same time as userDisabled to make it clear
        // which was the original cause of the add-on becoming disabled in a
        // way that the user can change.
        if (
          state == Ci.nsIBlocklistService.STATE_SOFTBLOCKED &&
          !addon.userDisabled
        ) {
          addon.softDisabled = true;
        }
        continue;
      }

      let entry = this._getAddonBlocklistEntry(addon, this._addonEntries);
      addonList.push({
        name: addon.name,
        version: addon.version,
        icon: addon.iconURL,
        disable: false,
        blocked: state == Ci.nsIBlocklistService.STATE_BLOCKED,
        item: addon,
        url: entry && entry.url,
      });
    }

    AddonManagerPrivate.updateAddonAppDisabledStates();

    var phs = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
    var plugins = phs.getPluginTags();

    for (let plugin of plugins) {
      let oldState = -1;
      if (oldPluginEntries) {
        oldState = this._getPluginBlocklistState(plugin, oldPluginEntries);
      }
      let state = this._getPluginBlocklistState(plugin, this._pluginEntries);
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
          addonList.push({
            name: plugin.name,
            version: plugin.version,
            icon: "chrome://global/skin/plugins/pluginGeneric.svg",
            disable: false,
            blocked: state == Ci.nsIBlocklistService.STATE_BLOCKED,
            item: plugin,
            url: await this.getPluginBlockURL(plugin),
          });
        }
      }
    }

    if (addonList.length == 0) {
      this._notifyObserversBlocklistUpdated();
      return;
    }

    if ("@mozilla.org/addons/blocklist-prompt;1" in Cc) {
      try {
        let blockedPrompter = Cc[
          "@mozilla.org/addons/blocklist-prompt;1"
        ].getService().wrappedJSObject;
        blockedPrompter.prompt(addonList);
      } catch (e) {
        LOG(e);
      }
      this._notifyObserversBlocklistUpdated();
      return;
    }

    var args = {
      restart: false,
      list: addonList,
    };
    // This lets the dialog get the raw js object
    args.wrappedJSObject = args;

    /*
      Some tests run without UI, so the async code listens to a message
      that can be sent programatically
    */
    let applyBlocklistChanges = () => {
      for (let addon of addonList) {
        if (!addon.disable) {
          continue;
        }

        if (addon.item instanceof Ci.nsIPluginTag) {
          addon.item.enabledState = Ci.nsIPluginTag.STATE_DISABLED;
        } else {
          // This add-on is softblocked.
          addon.item.softDisabled = true;
          // We must revert certain prefs.
          let prefs = this._getAddonPrefs(addon.item);
          resetPrefs(prefs);
        }
      }

      if (args.restart) {
        restartApp();
      }

      this._notifyObserversBlocklistUpdated();
      Services.obs.removeObserver(
        applyBlocklistChanges,
        "addon-blocklist-closed"
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
};

/*
 * Helper for constructing a blocklist.
 */
function BlocklistItemData(versionRangeElement) {
  this.targetApps = {};
  let foundTarget = false;
  this.severity = DEFAULT_SEVERITY;
  this.vulnerabilityStatus = VULNERABILITYSTATUS_NONE;
  if (versionRangeElement) {
    let versionRange = this.getBlocklistVersionRange(versionRangeElement);
    this.minVersion = versionRange.minVersion;
    this.maxVersion = versionRange.maxVersion;
    if (versionRangeElement.hasAttribute("severity")) {
      this.severity = versionRangeElement.getAttribute("severity");
    }
    if (versionRangeElement.hasAttribute("vulnerabilitystatus")) {
      this.vulnerabilityStatus = versionRangeElement.getAttribute(
        "vulnerabilitystatus"
      );
    }
    for (let targetAppElement of versionRangeElement.children) {
      if (targetAppElement.localName == "targetApplication") {
        foundTarget = true;
        // default to the current application if id is not provided.
        let appID = targetAppElement.id || gAppID;
        this.targetApps[appID] = this.getBlocklistAppVersions(targetAppElement);
      }
    }
  } else {
    this.minVersion = this.maxVersion = null;
  }

  // Default to all versions of the current application when no targetApplication
  // elements were found
  if (!foundTarget) {
    this.targetApps[gAppID] = [{ minVersion: null, maxVersion: null }];
  }
}

BlocklistItemData.prototype = {
  /**
   * Tests if a version of an item is included in the version range and target
   * application information represented by this BlocklistItemData using the
   * provided application and toolkit versions.
   * @param {string} version
   *        The version of the item being tested.
   * @param {string} appVersion
   *        The application version to test with.
   * @param {string} toolkitVersion
   *        The toolkit version to test with.
   * @returns {boolean}
   *        True if the version range covers the item version and application
   *        or toolkit version.
   */
  includesItem(version, appVersion, toolkitVersion) {
    // Some platforms have no version for plugins, these don't match if there
    // was a min/maxVersion provided
    if (!version && (this.minVersion || this.maxVersion)) {
      return false;
    }

    // Check if the item version matches
    if (!this.matchesRange(version, this.minVersion, this.maxVersion)) {
      return false;
    }

    // Check if the application version matches
    if (this.matchesTargetRange(gAppID, appVersion)) {
      return true;
    }

    // Check if the toolkit version matches
    return this.matchesTargetRange(TOOLKIT_ID, toolkitVersion);
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
  matchesRange(version, minVersion, maxVersion) {
    if (minVersion && Services.vc.compare(version, minVersion) < 0) {
      return false;
    }
    if (maxVersion && Services.vc.compare(version, maxVersion) > 0) {
      return false;
    }
    return true;
  },

  /**
   * Tests if there is a matching range for the given target application id and
   * version.
   * @param {string} appID
   *        The application ID to test for, may be for an application or toolkit
   * @param {string} appVersion
   *        The version of the application to test for.
   * @returns {boolean}
   *        True if this version range covers the application version given.
   */
  matchesTargetRange(appID, appVersion) {
    var blTargetApp = this.targetApps[appID];
    if (!blTargetApp) {
      return false;
    }

    for (let app of blTargetApp) {
      if (this.matchesRange(appVersion, app.minVersion, app.maxVersion)) {
        return true;
      }
    }

    return false;
  },

  /**
   * Retrieves a version range (e.g. minVersion and maxVersion) for a
   * blocklist item's targetApplication element.
   * @param {Element} targetAppElement
   *        A targetApplication blocklist element.
   * @returns {object[]}
   *        An array of JS objects with the following properties:
   *          "minVersion"  The minimum version in a version range (default = null).
   *          "maxVersion"  The maximum version in a version range (default = null).
   */
  getBlocklistAppVersions(targetAppElement) {
    var appVersions = [];

    if (targetAppElement) {
      for (let versionRangeElement of targetAppElement.children) {
        if (versionRangeElement.localName == "versionRange") {
          appVersions.push(this.getBlocklistVersionRange(versionRangeElement));
        }
      }
    }
    // return minVersion = null and maxVersion = null if no specific versionRange
    // elements were found
    if (appVersions.length == 0) {
      appVersions.push({ minVersion: null, maxVersion: null });
    }
    return appVersions;
  },

  /**
   * Retrieves a version range (e.g. minVersion and maxVersion) for a blocklist
   * versionRange element.
   *
   * @param {Element} versionRangeElement
   *        The versionRange blocklist element.
   *
   * @returns {Object}
   *        A JS object with the following properties:
   *          "minVersion"  The minimum version in a version range (default = null).
   *          "maxVersion"  The maximum version in a version range (default = null).
   */
  getBlocklistVersionRange(versionRangeElement) {
    // getAttribute returns null if the attribute is not present.
    let minVersion = versionRangeElement.getAttribute("minVersion");
    let maxVersion = versionRangeElement.getAttribute("maxVersion");

    return { minVersion, maxVersion };
  },
};

let BlocklistRS = {
  _init() {},
  shutdown() {
    GfxBlocklistRS.shutdown();
    PluginBlocklistRS.shutdown();
    ExtensionBlocklistRS.shutdown();
  },
  isLoaded: true,

  notify() {
    // ignore. We might miss a timer notification once if the XML impl. is disabled
    // when the timer fires and subsequently gets enabled. That seems OK.
  },

  forceUpdate() {
    for (let blocklist of [
      GfxBlocklistRS,
      ExtensionBlocklistRS,
      PluginBlocklistRS,
    ]) {
      blocklist.sync().catch(Cu.reportError);
    }
  },

  loadBlocklistAsync() {
    // Need to ensure we notify gfx of new stuff.
    GfxBlocklistRS.checkForEntries();
    ExtensionBlocklistRS.ensureInitialized();
    PluginBlocklistRS.ensureInitialized();
    // Also ensure that if we start the other service after this, we
    // initialize it straight away.
    gLoadingWasTriggered = true;
  },

  getPluginBlocklistState(plugin, appVersion, toolkitVersion) {
    return PluginBlocklistRS.getState(plugin, appVersion, toolkitVersion);
  },

  getPluginBlockURL(plugin) {
    return PluginBlocklistRS.getURL(plugin);
  },

  getAddonBlocklistState(addon, appVersion, toolkitVersion) {
    return ExtensionBlocklistRS.getState(addon, appVersion, toolkitVersion);
  },

  getAddonBlocklistEntry(addon, appVersion, toolkitVersion) {
    return ExtensionBlocklistRS.getEntry(addon, appVersion, toolkitVersion);
  },

  _blocklistUpdated() {
    ExtensionBlocklistRS._onUpdate();
    PluginBlocklistRS._onUpdate();
  },
};

const kSharedAPIs = [
  "notify",
  "loadBlocklistAsync",
  "getPluginBlockURL",
  "getPluginBlocklistState",
  "getAddonBlocklistState",
  "getAddonBlocklistEntry",
  "_blocklistUpdated",
];
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
    Services.prefs.addObserver("extensions.blocklist.", this);
    Services.prefs.addObserver(PREF_EM_LOGGING_ENABLED, this);

    // Define forwarding functions:
    for (let k of kSharedAPIs) {
      this[k] = (...args) => this._impl[k](...args);
    }

    this.onUpdateImplementation();

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
  // the `isLoaded` property needs forwarding too:
  get isLoaded() {
    return this._impl.isLoaded;
  },

  onUpdateImplementation(shouldCheckForUpdates = false) {
    this._impl = this.useXML ? BlocklistXML : BlocklistRS;
    this._impl._init();
    if (shouldCheckForUpdates) {
      if (this.useXML) {
        // In theory, we should be able to use the "last update" pref to figure out
        // when we last fetched updates and avoid fetching it if we switched
        // this on and off within a day. However, the pref is updated by the
        // update timer code, but the request is made in our code - and a request is
        // not made if we're not using the XML blocklist, despite the pref being
        // updated. In other words, the pref being updated is no guarantee that we
        // actually updated the list that day. So just unconditionally update it:
        this._impl.notify();
      } else {
        this._impl.forceUpdate();
      }
    }
  },

  shutdown() {
    this._impl.shutdown();
    Services.obs.removeObserver(this, "xpcom-shutdown");
    Services.prefs.removeObserver("extensions.blocklist.", this);
    Services.prefs.removeObserver(PREF_EM_LOGGING_ENABLED, this);
  },

  observe(subject, topic, prefName) {
    switch (topic) {
      case "xpcom-shutdown":
        this.shutdown();
        break;
      case "profile-after-change":
        // We're only called here on non-Desktop-Firefox, and use this opportunity to try to
        // load the blocklist asynchronously. On desktop Firefox, we load the list from
        // nsBrowserGlue after sessionstore-windows-restored.
        this.loadBlocklistAsync();
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
            // Historically, this only does something if we're using the XML blocklist,
            // so check:
            if (this._impl == BlocklistXML) {
              this._impl._onBlocklistEnabledToggle();
            } else {
              this._impl._blocklistUpdated();
            }
            break;
          case PREF_BLOCKLIST_LEVEL:
            gBlocklistLevel = Math.min(
              Services.prefs.getIntPref(PREF_BLOCKLIST_LEVEL, DEFAULT_LEVEL),
              MAX_BLOCK_LEVEL
            );
            this._blocklistUpdated(null, null);
            break;
        }
        break;
    }
  },
};

XPCOMUtils.defineLazyPreferenceGetter(
  Blocklist,
  "useXML",
  "extensions.blocklist.useXML",
  true,
  () => Blocklist.onUpdateImplementation(true)
);

Blocklist._init();
