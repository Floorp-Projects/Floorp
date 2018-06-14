/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [
  "initialize",
];

ChromeUtils.import("resource://gre/modules/Services.jsm");
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm", {});

ChromeUtils.defineModuleGetter(this, "RemoteSettings", "resource://services-settings/remote-settings.js");
ChromeUtils.defineModuleGetter(this, "jexlFilterFunc", "resource://services-settings/remote-settings.js");

const PREF_BLOCKLIST_BUCKET                  = "services.blocklist.bucket";
const PREF_BLOCKLIST_ONECRL_COLLECTION       = "services.blocklist.onecrl.collection";
const PREF_BLOCKLIST_ONECRL_CHECKED_SECONDS  = "services.blocklist.onecrl.checked";
const PREF_BLOCKLIST_ONECRL_SIGNER           = "services.blocklist.onecrl.signer";
const PREF_BLOCKLIST_ADDONS_COLLECTION       = "services.blocklist.addons.collection";
const PREF_BLOCKLIST_ADDONS_CHECKED_SECONDS  = "services.blocklist.addons.checked";
const PREF_BLOCKLIST_ADDONS_SIGNER           = "services.blocklist.addons.signer";
const PREF_BLOCKLIST_PLUGINS_COLLECTION      = "services.blocklist.plugins.collection";
const PREF_BLOCKLIST_PLUGINS_CHECKED_SECONDS = "services.blocklist.plugins.checked";
const PREF_BLOCKLIST_PLUGINS_SIGNER          = "services.blocklist.plugins.signer";
const PREF_BLOCKLIST_PINNING_ENABLED         = "services.blocklist.pinning.enabled";
const PREF_BLOCKLIST_PINNING_BUCKET          = "services.blocklist.pinning.bucket";
const PREF_BLOCKLIST_PINNING_COLLECTION      = "services.blocklist.pinning.collection";
const PREF_BLOCKLIST_PINNING_CHECKED_SECONDS = "services.blocklist.pinning.checked";
const PREF_BLOCKLIST_PINNING_SIGNER          = "services.blocklist.pinning.signer";
const PREF_BLOCKLIST_GFX_COLLECTION          = "services.blocklist.gfx.collection";
const PREF_BLOCKLIST_GFX_CHECKED_SECONDS     = "services.blocklist.gfx.checked";
const PREF_BLOCKLIST_GFX_SIGNER              = "services.blocklist.gfx.signer";

/**
 * Revoke the appropriate certificates based on the records from the blocklist.
 *
 * @param {Object} data   Current records in the local db.
 */
async function updateCertBlocklist({ data: { current: records } }) {
  const certList = Cc["@mozilla.org/security/certblocklist;1"]
                     .getService(Ci.nsICertBlocklist);
  for (let item of records) {
    try {
      if (item.issuerName && item.serialNumber) {
        certList.revokeCertByIssuerAndSerial(item.issuerName,
                                            item.serialNumber);
      } else if (item.subject && item.pubKeyHash) {
        certList.revokeCertBySubjectAndPubKey(item.subject,
                                              item.pubKeyHash);
      }
    } catch (e) {
      // prevent errors relating to individual blocklist entries from
      // causing sync to fail. We will accumulate telemetry on these failures in
      // bug 1254099.
      Cu.reportError(e);
    }
  }
  certList.saveEntries();
}

/**
 * Modify the appropriate security pins based on records from the remote
 * collection.
 *
 * @param {Object} data   Current records in the local db.
 */
async function updatePinningList({ data: { current: records } }) {
  if (!Services.prefs.getBoolPref(PREF_BLOCKLIST_PINNING_ENABLED)) {
    return;
  }

  const siteSecurityService = Cc["@mozilla.org/ssservice;1"]
      .getService(Ci.nsISiteSecurityService);

  // clear the current preload list
  siteSecurityService.clearPreloads();

  // write each KeyPin entry to the preload list
  for (let item of records) {
    try {
      const {pinType, pins = [], versions} = item;
      if (versions.includes(Services.appinfo.version)) {
        if (pinType == "KeyPin" && pins.length) {
          siteSecurityService.setKeyPins(item.hostName,
              item.includeSubdomains,
              item.expires,
              pins.length,
              pins, true);
        }
        if (pinType == "STSPin") {
          siteSecurityService.setHSTSPreload(item.hostName,
                                             item.includeSubdomains,
                                             item.expires);
        }
      }
    } catch (e) {
      // prevent errors relating to individual preload entries from causing
      // sync to fail. We will accumulate telemetry for such failures in bug
      // 1254099.
    }
  }
}

/**
 * Write list of records into JSON file, and notify nsBlocklistService.
 *
 * @param {Object} client   RemoteSettingsClient instance
 * @param {Object} data      Current records in the local db.
 */
async function updateJSONBlocklist(client, { data: { current: records } }) {
  // Write JSON dump for synchronous load at startup.
  const path = OS.Path.join(OS.Constants.Path.profileDir, client.filename);
  const blocklistFolder = OS.Path.dirname(path);

  await OS.File.makeDir(blocklistFolder, {from: OS.Constants.Path.profileDir});

  const serialized = JSON.stringify({data: records}, null, 2);
  try {
    await OS.File.writeAtomic(path, serialized, {tmpPath: path + ".tmp"});
    // Notify change to `nsBlocklistService`
    const eventData = {filename: client.filename};
    Services.cpmm.sendAsyncMessage("Blocklist:reload-from-disk", eventData);
  } catch (e) {
    Cu.reportError(e);
  }
}


/**
 * This custom filter function is used to limit the entries returned
 * by `RemoteSettings("...").get()` depending on the target app information
 * defined on entries.
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
    const matchesRange = (Services.vc.compare(appVersion, maxVersion) <= 0);
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
      if (guid == appID &&
          Services.vc.compare(appVersion, maxVersion) <= 0) {
        return entry;
      }
      if (guid == "toolkit@mozilla.org" &&
          Services.vc.compare(toolkitVersion, maxVersion) <= 0) {
        return entry;
      }
    }
  }
  // Skip this entry.
  return null;
}

var AddonBlocklistClient;
var GfxBlocklistClient;
var OneCRLBlocklistClient;
var PinningBlocklistClient;
var PluginBlocklistClient;

function initialize() {
  OneCRLBlocklistClient = RemoteSettings(Services.prefs.getCharPref(PREF_BLOCKLIST_ONECRL_COLLECTION), {
    bucketName: Services.prefs.getCharPref(PREF_BLOCKLIST_BUCKET),
    lastCheckTimePref: PREF_BLOCKLIST_ONECRL_CHECKED_SECONDS,
    signerName: Services.prefs.getCharPref(PREF_BLOCKLIST_ONECRL_SIGNER),
  });
  OneCRLBlocklistClient.on("sync", updateCertBlocklist);

  AddonBlocklistClient = RemoteSettings(Services.prefs.getCharPref(PREF_BLOCKLIST_ADDONS_COLLECTION), {
    bucketName: Services.prefs.getCharPref(PREF_BLOCKLIST_BUCKET),
    lastCheckTimePref: PREF_BLOCKLIST_ADDONS_CHECKED_SECONDS,
    signerName: Services.prefs.getCharPref(PREF_BLOCKLIST_ADDONS_SIGNER),
    filterFunc: targetAppFilter,
  });
  AddonBlocklistClient.on("sync", updateJSONBlocklist.bind(null, AddonBlocklistClient));

  PluginBlocklistClient = RemoteSettings(Services.prefs.getCharPref(PREF_BLOCKLIST_PLUGINS_COLLECTION), {
    bucketName: Services.prefs.getCharPref(PREF_BLOCKLIST_BUCKET),
    lastCheckTimePref: PREF_BLOCKLIST_PLUGINS_CHECKED_SECONDS,
    signerName: Services.prefs.getCharPref(PREF_BLOCKLIST_PLUGINS_SIGNER),
    filterFunc: targetAppFilter,
  });
  PluginBlocklistClient.on("sync", updateJSONBlocklist.bind(null, PluginBlocklistClient));

  GfxBlocklistClient = RemoteSettings(Services.prefs.getCharPref(PREF_BLOCKLIST_GFX_COLLECTION), {
    bucketName: Services.prefs.getCharPref(PREF_BLOCKLIST_BUCKET),
    lastCheckTimePref: PREF_BLOCKLIST_GFX_CHECKED_SECONDS,
    signerName: Services.prefs.getCharPref(PREF_BLOCKLIST_GFX_SIGNER),
    filterFunc: targetAppFilter,
  });
  GfxBlocklistClient.on("sync", updateJSONBlocklist.bind(null, GfxBlocklistClient));

  PinningBlocklistClient = RemoteSettings(Services.prefs.getCharPref(PREF_BLOCKLIST_PINNING_COLLECTION), {
    bucketName: Services.prefs.getCharPref(PREF_BLOCKLIST_PINNING_BUCKET),
    lastCheckTimePref: PREF_BLOCKLIST_PINNING_CHECKED_SECONDS,
    signerName: Services.prefs.getCharPref(PREF_BLOCKLIST_PINNING_SIGNER),
  });
  PinningBlocklistClient.on("sync", updatePinningList);

  return {
    OneCRLBlocklistClient,
    AddonBlocklistClient,
    PluginBlocklistClient,
    GfxBlocklistClient,
    PinningBlocklistClient
  };
}
