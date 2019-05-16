/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [
  "BlocklistClients",
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

const PREF_BLOCKLIST_PINNING_ENABLED         = "services.blocklist.pinning.enabled";
const PREF_BLOCKLIST_PINNING_BUCKET          = "services.blocklist.pinning.bucket";
const PREF_BLOCKLIST_PINNING_COLLECTION      = "services.blocklist.pinning.collection";
const PREF_BLOCKLIST_PINNING_CHECKED_SECONDS = "services.blocklist.pinning.checked";
const PREF_BLOCKLIST_PINNING_SIGNER          = "services.blocklist.pinning.signer";

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
  async function({ data: { current, created, updated, deleted } }) {
    const certList = Cc["@mozilla.org/security/certstorage;1"]
                       .getService(Ci.nsICertStorage);
    let items = [];

    // See if we have prior revocation data (this can happen when we can't open
    // the database and we have to re-create it (see bug 1546361)).
    let hasPriorRevocationData = await new Promise((resolve) => {
      certList.hasPriorData(Ci.nsICertStorage.DATA_TYPE_REVOCATION, (rv, hasPriorData) => {
        if (rv == Cr.NS_OK) {
          resolve(hasPriorData);
        } else {
          // If calling hasPriorData failed, assume we need to reload
          // everything (even though it's unlikely doing so will succeed).
          resolve(false);
        }
      });
    });

    // If we don't have prior data, make it so we re-load everything.
    if (!hasPriorRevocationData) {
      deleted = [];
      updated = [];
      created = current;
    }

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

var OneCRLBlocklistClient;
var PinningBlocklistClient;
var RemoteSecuritySettingsClient;

function initialize(options = {}) {
  const { verifySignature = true } = options;

  OneCRLBlocklistClient = RemoteSettings(Services.prefs.getCharPref(PREF_SECURITY_SETTINGS_ONECRL_COLLECTION), {
    bucketNamePref: PREF_SECURITY_SETTINGS_ONECRL_BUCKET,
    lastCheckTimePref: PREF_SECURITY_SETTINGS_ONECRL_CHECKED,
    signerName: Services.prefs.getCharPref(PREF_SECURITY_SETTINGS_ONECRL_SIGNER),
  });
  OneCRLBlocklistClient.verifySignature = verifySignature;
  OneCRLBlocklistClient.on("sync", updateCertBlocklist);

  PinningBlocklistClient = RemoteSettings(Services.prefs.getCharPref(PREF_BLOCKLIST_PINNING_COLLECTION), {
    bucketNamePref: PREF_BLOCKLIST_PINNING_BUCKET,
    lastCheckTimePref: PREF_BLOCKLIST_PINNING_CHECKED_SECONDS,
    signerName: Services.prefs.getCharPref(PREF_BLOCKLIST_PINNING_SIGNER),
  });
  PinningBlocklistClient.verifySignature = verifySignature;
  PinningBlocklistClient.on("sync", updatePinningList);

  if (AppConstants.MOZ_NEW_CERT_STORAGE) {
    const { RemoteSecuritySettings } = ChromeUtils.import("resource://gre/modules/psm/RemoteSecuritySettings.jsm");

    // In Bug 1526018 this will move into its own service, as it's not quite like
    // the others.
    RemoteSecuritySettingsClient = new RemoteSecuritySettings();
    RemoteSecuritySettingsClient.verifySignature = verifySignature;

    return {
      OneCRLBlocklistClient,
      PinningBlocklistClient,
      RemoteSecuritySettingsClient,
    };
  }

  return {
    OneCRLBlocklistClient,
    PinningBlocklistClient,
  };
}

let BlocklistClients = {initialize};

