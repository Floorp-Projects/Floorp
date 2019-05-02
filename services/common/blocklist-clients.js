/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [
  "initialize",
];

const { AppConstants } = ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

ChromeUtils.defineModuleGetter(this, "RemoteSettings", "resource://services-settings/remote-settings.js");
ChromeUtils.defineModuleGetter(this, "jexlFilterFunc", "resource://services-settings/remote-settings.js");

const PREF_SECURITY_SETTINGS_ONECRL_BUCKET     = "services.settings.security.onecrl.bucket";
const PREF_SECURITY_SETTINGS_ONECRL_COLLECTION = "services.settings.security.onecrl.collection";
const PREF_SECURITY_SETTINGS_ONECRL_SIGNER     = "services.settings.security.onecrl.signer";
const PREF_SECURITY_SETTINGS_ONECRL_CHECKED    = "services.settings.security.onecrl.checked";

const PREF_BLOCKLIST_BUCKET                  = "services.blocklist.bucket";
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

class RevocationState {
  constructor(state) {
    this.state = state;
  }
}

class IssuerAndSerialRevocationState extends RevocationState {
  constructor(issuer, serial, state) {
    super(state);
    this.issuer = issuer;
    this.serial = serial;
  }
}
IssuerAndSerialRevocationState.prototype.QueryInterface =
  ChromeUtils.generateQI([Ci.nsIIssuerAndSerialRevocationState]);

class SubjectAndPubKeyRevocationState extends RevocationState {
  constructor(subject, pubKey, state) {
    super(state);
    this.subject = subject;
    this.pubKey = pubKey;
  }
}
SubjectAndPubKeyRevocationState.prototype.QueryInterface =
  ChromeUtils.generateQI([Ci.nsISubjectAndPubKeyRevocationState]);

function setRevocations(certStorage, revocations) {
  return new Promise((resolve) =>
    certStorage.setRevocations(revocations, resolve)
  );
}

/**
 * Revoke the appropriate certificates based on the records from the blocklist.
 *
 * @param {Object} data   Current records in the local db.
 */
const updateCertBlocklist = AppConstants.MOZ_NEW_CERT_STORAGE ?
  async function({ data: { created, updated, deleted } }) {
    const certList = Cc["@mozilla.org/security/certstorage;1"]
                       .getService(Ci.nsICertStorage);
    let items = [];

    for (let item of deleted) {
      if (item.issuerName && item.serialNumber) {
        items.push(new IssuerAndSerialRevocationState(item.issuerName,
          item.serialNumber, Ci.nsICertStorage.STATE_UNSET));
      } else if (item.subject && item.pubKeyHash) {
        items.push(new SubjectAndPubKeyRevocationState(item.subject,
          item.pubKeyHash, Ci.nsICertStorage.STATE_UNSET));
      }
    }

    const toAdd = created.concat(updated.map(u => u.new));

    for (let item of toAdd) {
      if (item.issuerName && item.serialNumber) {
        items.push(new IssuerAndSerialRevocationState(item.issuerName,
          item.serialNumber, Ci.nsICertStorage.STATE_ENFORCE));
      } else if (item.subject && item.pubKeyHash) {
        items.push(new SubjectAndPubKeyRevocationState(item.subject,
          item.pubKeyHash, Ci.nsICertStorage.STATE_ENFORCE));
      }
    }

    try {
      await setRevocations(certList, items);
    } catch (e) {
      Cu.reportError(e);
    }
  } : async function({ data: { current: records } }) {
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
  };

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
var RemoteSecuritySettingsClient;

function initialize() {
  OneCRLBlocklistClient = RemoteSettings(Services.prefs.getCharPref(PREF_SECURITY_SETTINGS_ONECRL_COLLECTION), {
    bucketNamePref: PREF_SECURITY_SETTINGS_ONECRL_BUCKET,
    lastCheckTimePref: PREF_SECURITY_SETTINGS_ONECRL_CHECKED,
    signerName: Services.prefs.getCharPref(PREF_SECURITY_SETTINGS_ONECRL_SIGNER),
  });
  OneCRLBlocklistClient.on("sync", updateCertBlocklist);

  AddonBlocklistClient = RemoteSettings(Services.prefs.getCharPref(PREF_BLOCKLIST_ADDONS_COLLECTION), {
    bucketNamePref: PREF_BLOCKLIST_BUCKET,
    lastCheckTimePref: PREF_BLOCKLIST_ADDONS_CHECKED_SECONDS,
    signerName: Services.prefs.getCharPref(PREF_BLOCKLIST_ADDONS_SIGNER),
    filterFunc: targetAppFilter,
  });

  PluginBlocklistClient = RemoteSettings(Services.prefs.getCharPref(PREF_BLOCKLIST_PLUGINS_COLLECTION), {
    bucketNamePref: PREF_BLOCKLIST_BUCKET,
    lastCheckTimePref: PREF_BLOCKLIST_PLUGINS_CHECKED_SECONDS,
    signerName: Services.prefs.getCharPref(PREF_BLOCKLIST_PLUGINS_SIGNER),
    filterFunc: targetAppFilter,
  });

  GfxBlocklistClient = RemoteSettings(Services.prefs.getCharPref(PREF_BLOCKLIST_GFX_COLLECTION), {
    bucketNamePref: PREF_BLOCKLIST_BUCKET,
    lastCheckTimePref: PREF_BLOCKLIST_GFX_CHECKED_SECONDS,
    signerName: Services.prefs.getCharPref(PREF_BLOCKLIST_GFX_SIGNER),
    filterFunc: targetAppFilter,
  });

  PinningBlocklistClient = RemoteSettings(Services.prefs.getCharPref(PREF_BLOCKLIST_PINNING_COLLECTION), {
    bucketNamePref: PREF_BLOCKLIST_PINNING_BUCKET,
    lastCheckTimePref: PREF_BLOCKLIST_PINNING_CHECKED_SECONDS,
    signerName: Services.prefs.getCharPref(PREF_BLOCKLIST_PINNING_SIGNER),
  });
  PinningBlocklistClient.on("sync", updatePinningList);

  if (AppConstants.MOZ_NEW_CERT_STORAGE) {
    const { RemoteSecuritySettings } = ChromeUtils.import("resource://gre/modules/psm/RemoteSecuritySettings.jsm");

    // In Bug 1526018 this will move into its own service, as it's not quite like
    // the others.
    RemoteSecuritySettingsClient = new RemoteSecuritySettings();

    return {
      OneCRLBlocklistClient,
      AddonBlocklistClient,
      PluginBlocklistClient,
      GfxBlocklistClient,
      PinningBlocklistClient,
      RemoteSecuritySettingsClient,
    };
  }

  return {
    OneCRLBlocklistClient,
    AddonBlocklistClient,
    PluginBlocklistClient,
    GfxBlocklistClient,
    PinningBlocklistClient,
  };
}
