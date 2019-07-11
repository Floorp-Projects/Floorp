/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const EXPORTED_SYMBOLS = ["RemoteSecuritySettings"];

const { RemoteSettings } = ChromeUtils.import(
  "resource://services-settings/remote-settings.js"
);

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { X509 } = ChromeUtils.import(
  "resource://gre/modules/psm/X509.jsm",
  null
);

const INTERMEDIATES_BUCKET_PREF =
  "security.remote_settings.intermediates.bucket";
const INTERMEDIATES_CHECKED_SECONDS_PREF =
  "security.remote_settings.intermediates.checked";
const INTERMEDIATES_COLLECTION_PREF =
  "security.remote_settings.intermediates.collection";
const INTERMEDIATES_DL_PER_POLL_PREF =
  "security.remote_settings.intermediates.downloads_per_poll";
const INTERMEDIATES_DL_PARALLEL_REQUESTS =
  "security.remote_settings.intermediates.parallel_downloads";
const INTERMEDIATES_ENABLED_PREF =
  "security.remote_settings.intermediates.enabled";
const INTERMEDIATES_SIGNER_PREF =
  "security.remote_settings.intermediates.signer";
const LOGLEVEL_PREF = "browser.policies.loglevel";

const ONECRL_BUCKET_PREF = "services.settings.security.onecrl.bucket";
const ONECRL_COLLECTION_PREF = "services.settings.security.onecrl.collection";
const ONECRL_SIGNER_PREF = "services.settings.security.onecrl.signer";
const ONECRL_CHECKED_PREF = "services.settings.security.onecrl.checked";

const PINNING_ENABLED_PREF = "services.blocklist.pinning.enabled";
const PINNING_BUCKET_PREF = "services.blocklist.pinning.bucket";
const PINNING_COLLECTION_PREF = "services.blocklist.pinning.collection";
const PINNING_CHECKED_SECONDS_PREF = "services.blocklist.pinning.checked";
const PINNING_SIGNER_PREF = "services.blocklist.pinning.signer";

const CRLITE_FILTERS_BUCKET_PREF =
  "security.remote_settings.crlite_filters.bucket";
const CRLITE_FILTERS_CHECKED_SECONDS_PREF =
  "security.remote_settings.crlite_filters.checked";
const CRLITE_FILTERS_COLLECTION_PREF =
  "security.remote_settings.crlite_filters.collection";
const CRLITE_FILTERS_ENABLED_PREF =
  "security.remote_settings.crlite_filters.enabled";
const CRLITE_FILTERS_SIGNER_PREF =
  "security.remote_settings.crlite_filters.signer";

XPCOMUtils.defineLazyGlobalGetters(this, ["fetch"]);

XPCOMUtils.defineLazyGetter(this, "gTextDecoder", () => new TextDecoder());

XPCOMUtils.defineLazyGetter(this, "baseAttachmentsURL", async () => {
  const server = Services.prefs.getCharPref("services.settings.server");
  const serverInfo = await (await fetch(`${server}/`, {
    credentials: "omit",
  })).json();
  const {
    capabilities: {
      attachments: { base_url },
    },
  } = serverInfo;
  return base_url;
});

XPCOMUtils.defineLazyGetter(this, "log", () => {
  let { ConsoleAPI } = ChromeUtils.import("resource://gre/modules/Console.jsm");
  return new ConsoleAPI({
    prefix: "RemoteSecuritySettings.jsm",
    // tip: set maxLogLevel to "debug" and use log.debug() to create detailed
    // messages during development. See LOG_LEVELS in Console.jsm for details.
    maxLogLevel: "error",
    maxLogLevelPref: LOGLEVEL_PREF,
  });
});

function hexify(data) {
  return Array.from(data, (c, i) =>
    data
      .charCodeAt(i)
      .toString(16)
      .padStart(2, "0")
  ).join("");
}

// Hash a UTF-8 string into a hex string with SHA256
function getHash(str) {
  // return the two-digit hexadecimal code for a byte
  let hasher = Cc["@mozilla.org/security/hash;1"].createInstance(
    Ci.nsICryptoHash
  );
  hasher.init(Ci.nsICryptoHash.SHA256);
  let stringStream = Cc["@mozilla.org/io/string-input-stream;1"].createInstance(
    Ci.nsIStringInputStream
  );
  stringStream.data = str;
  hasher.updateFromStream(stringStream, -1);

  // convert the binary hash data to a hex string.
  return hexify(hasher.finish(false));
}

// Converts a JS string to an array of bytes consisting of the char code at each
// index in the string.
function stringToBytes(s) {
  let b = [];
  for (let i = 0; i < s.length; i++) {
    b.push(s.charCodeAt(i));
  }
  return b;
}

// Converts an array of bytes to a JS string using fromCharCode on each byte.
function bytesToString(bytes) {
  if (bytes.length > 65535) {
    throw new Error("input too long for bytesToString");
  }
  return String.fromCharCode.apply(null, bytes);
}

class CRLiteState {
  constructor(subject, spkiHash, state) {
    this.subject = subject;
    this.spkiHash = spkiHash;
    this.state = state;
  }
}
CRLiteState.prototype.QueryInterface = ChromeUtils.generateQI([
  Ci.nsICRLiteState,
]);

class CertInfo {
  constructor(cert, subject) {
    this.cert = cert;
    this.subject = subject;
    this.trust = Ci.nsICertStorage.TRUST_INHERIT;
  }
}
CertInfo.prototype.QueryInterface = ChromeUtils.generateQI([Ci.nsICertInfo]);

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
IssuerAndSerialRevocationState.prototype.QueryInterface = ChromeUtils.generateQI(
  [Ci.nsIIssuerAndSerialRevocationState]
);

class SubjectAndPubKeyRevocationState extends RevocationState {
  constructor(subject, pubKey, state) {
    super(state);
    this.subject = subject;
    this.pubKey = pubKey;
  }
}
SubjectAndPubKeyRevocationState.prototype.QueryInterface = ChromeUtils.generateQI(
  [Ci.nsISubjectAndPubKeyRevocationState]
);

function setRevocations(certStorage, revocations) {
  return new Promise(resolve =>
    certStorage.setRevocations(revocations, resolve)
  );
}

/**
 * Revoke the appropriate certificates based on the records from the blocklist.
 *
 * @param {Object} data   Current records in the local db.
 */
const updateCertBlocklist = AppConstants.MOZ_NEW_CERT_STORAGE
  ? async function({ data: { current, created, updated, deleted } }) {
      const certList = Cc["@mozilla.org/security/certstorage;1"].getService(
        Ci.nsICertStorage
      );
      let items = [];

      // See if we have prior revocation data (this can happen when we can't open
      // the database and we have to re-create it (see bug 1546361)).
      let hasPriorRevocationData = await new Promise(resolve => {
        certList.hasPriorData(
          Ci.nsICertStorage.DATA_TYPE_REVOCATION,
          (rv, hasPriorData) => {
            if (rv == Cr.NS_OK) {
              resolve(hasPriorData);
            } else {
              // If calling hasPriorData failed, assume we need to reload
              // everything (even though it's unlikely doing so will succeed).
              resolve(false);
            }
          }
        );
      });

      // If we don't have prior data, make it so we re-load everything.
      if (!hasPriorRevocationData) {
        deleted = [];
        updated = [];
        created = current;
      }

      for (let item of deleted) {
        if (item.issuerName && item.serialNumber) {
          items.push(
            new IssuerAndSerialRevocationState(
              item.issuerName,
              item.serialNumber,
              Ci.nsICertStorage.STATE_UNSET
            )
          );
        } else if (item.subject && item.pubKeyHash) {
          items.push(
            new SubjectAndPubKeyRevocationState(
              item.subject,
              item.pubKeyHash,
              Ci.nsICertStorage.STATE_UNSET
            )
          );
        }
      }

      const toAdd = created.concat(updated.map(u => u.new));

      for (let item of toAdd) {
        if (item.issuerName && item.serialNumber) {
          items.push(
            new IssuerAndSerialRevocationState(
              item.issuerName,
              item.serialNumber,
              Ci.nsICertStorage.STATE_ENFORCE
            )
          );
        } else if (item.subject && item.pubKeyHash) {
          items.push(
            new SubjectAndPubKeyRevocationState(
              item.subject,
              item.pubKeyHash,
              Ci.nsICertStorage.STATE_ENFORCE
            )
          );
        }
      }

      try {
        await setRevocations(certList, items);
      } catch (e) {
        Cu.reportError(e);
      }
    }
  : async function({ data: { current: records } }) {
      const certList = Cc["@mozilla.org/security/certblocklist;1"].getService(
        Ci.nsICertBlocklist
      );
      for (let item of records) {
        try {
          if (item.issuerName && item.serialNumber) {
            certList.revokeCertByIssuerAndSerial(
              item.issuerName,
              item.serialNumber
            );
          } else if (item.subject && item.pubKeyHash) {
            certList.revokeCertBySubjectAndPubKey(
              item.subject,
              item.pubKeyHash
            );
          }
        } catch (e) {
          // Prevent errors relating to individual blocklist entries from causing sync to fail.
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
  if (!Services.prefs.getBoolPref(PINNING_ENABLED_PREF)) {
    return;
  }

  const siteSecurityService = Cc["@mozilla.org/ssservice;1"].getService(
    Ci.nsISiteSecurityService
  );

  // clear the current preload list
  siteSecurityService.clearPreloads();

  // write each KeyPin entry to the preload list
  for (let item of records) {
    try {
      const { pinType, pins = [], versions } = item;
      if (versions.includes(Services.appinfo.version)) {
        if (pinType == "KeyPin" && pins.length) {
          siteSecurityService.setKeyPins(
            item.hostName,
            item.includeSubdomains,
            item.expires,
            pins,
            true
          );
        }
        if (pinType == "STSPin") {
          siteSecurityService.setHSTSPreload(
            item.hostName,
            item.includeSubdomains,
            item.expires
          );
        }
      }
    } catch (e) {
      // Prevent errors relating to individual preload entries from causing sync to fail.
      Cu.reportError(e);
    }
  }
}

var RemoteSecuritySettings = {
  /**
   * Initialize the clients (cheap instantiation) and setup their sync event.
   * This static method is called from BrowserGlue.jsm soon after startup.
   *
   * @returns {Object} intantiated clients for security remote settings.
   */
  init() {
    const OneCRLBlocklistClient = RemoteSettings(
      Services.prefs.getCharPref(ONECRL_COLLECTION_PREF),
      {
        bucketNamePref: ONECRL_BUCKET_PREF,
        lastCheckTimePref: ONECRL_CHECKED_PREF,
        signerName: Services.prefs.getCharPref(ONECRL_SIGNER_PREF),
      }
    );
    OneCRLBlocklistClient.on("sync", updateCertBlocklist);

    const PinningBlocklistClient = RemoteSettings(
      Services.prefs.getCharPref(PINNING_COLLECTION_PREF),
      {
        bucketNamePref: PINNING_BUCKET_PREF,
        lastCheckTimePref: PINNING_CHECKED_SECONDS_PREF,
        signerName: Services.prefs.getCharPref(PINNING_SIGNER_PREF),
      }
    );
    PinningBlocklistClient.on("sync", updatePinningList);

    let IntermediatePreloadsClient;
    let CRLiteFiltersClient;
    if (AppConstants.MOZ_NEW_CERT_STORAGE) {
      IntermediatePreloadsClient = new IntermediatePreloads();
      CRLiteFiltersClient = new CRLiteFilters();
    }

    return {
      OneCRLBlocklistClient,
      PinningBlocklistClient,
      IntermediatePreloadsClient,
      CRLiteFiltersClient,
    };
  },
};

class IntermediatePreloads {
  constructor() {
    this.client = RemoteSettings(
      Services.prefs.getCharPref(INTERMEDIATES_COLLECTION_PREF),
      {
        bucketNamePref: INTERMEDIATES_BUCKET_PREF,
        lastCheckTimePref: INTERMEDIATES_CHECKED_SECONDS_PREF,
        signerName: Services.prefs.getCharPref(INTERMEDIATES_SIGNER_PREF),
        localFields: ["cert_import_complete"],
      }
    );

    this.client.on("sync", this.onSync.bind(this));
    Services.obs.addObserver(
      this.onObservePollEnd.bind(this),
      "remote-settings:changes-poll-end"
    );

    log.debug("Intermediate Preloading: constructor");
  }

  async updatePreloadedIntermediates() {
    // Bug 1429800: once the CertStateService has the correct interface, also
    // store the whitelist status and crlite enrollment status

    if (!Services.prefs.getBoolPref(INTERMEDIATES_ENABLED_PREF, true)) {
      log.debug("Intermediate Preloading is disabled");
      Services.obs.notifyObservers(
        null,
        "remote-security-settings:intermediates-updated",
        "disabled"
      );
      return;
    }

    // Download attachments that are awaiting download, up to a max.
    const maxDownloadsPerRun = Services.prefs.getIntPref(
      INTERMEDIATES_DL_PER_POLL_PREF,
      100
    );
    const parallelDownloads = Services.prefs.getIntPref(
      INTERMEDIATES_DL_PARALLEL_REQUESTS,
      8
    );

    // Bug 1519256: Move this to a separate method that's on a separate timer
    // with a higher frequency (so we can attempt to download outstanding
    // certs more than once daily)

    // See if we have prior cert data (this can happen when we can't open the database and we
    // have to re-create it (see bug 1546361)).
    const certStorage = Cc["@mozilla.org/security/certstorage;1"].getService(
      Ci.nsICertStorage
    );
    let hasPriorCertData = await new Promise(resolve => {
      certStorage.hasPriorData(
        Ci.nsICertStorage.DATA_TYPE_CERTIFICATE,
        (rv, hasPriorData) => {
          if (rv == Cr.NS_OK) {
            resolve(hasPriorData);
          } else {
            // If calling hasPriorData failed, assume we need to reload everything (even though
            // it's unlikely doing so will succeed).
            resolve(false);
          }
        }
      );
    });
    const col = await this.client.openCollection();
    // If we don't have prior data, make it so we re-load everything.
    if (!hasPriorCertData) {
      const { data: current } = await col.list({ order: "" }); // no sort needed.
      const toReset = current.filter(record => record.cert_import_complete);
      await col.db.execute(transaction => {
        toReset.forEach(record => {
          transaction.update({ ...record, cert_import_complete: false });
        });
      });
    }
    const { data: current } = await col.list({ order: "" }); // no sort needed.
    const waiting = current.filter(record => !record.cert_import_complete);

    log.debug(`There are ${waiting.length} intermediates awaiting download.`);
    if (waiting.length == 0) {
      // Nothing to do.
      Services.obs.notifyObservers(
        null,
        "remote-security-settings:intermediates-updated",
        "success"
      );
      return;
    }

    let toDownload = waiting.slice(0, maxDownloadsPerRun);
    let recordsCertsAndSubjects = [];
    for (let i = 0; i < toDownload.length; i += parallelDownloads) {
      const chunk = toDownload.slice(i, i + parallelDownloads);
      const downloaded = await Promise.all(
        chunk.map(record => this.maybeDownloadAttachment(record))
      );
      recordsCertsAndSubjects = recordsCertsAndSubjects.concat(downloaded);
    }

    let certInfos = [];
    let recordsToUpdate = [];
    for (let { record, cert, subject } of recordsCertsAndSubjects) {
      if (cert && subject) {
        certInfos.push(new CertInfo(cert, subject));
        recordsToUpdate.push(record);
      }
    }
    let result = await new Promise(resolve => {
      certStorage.addCerts(certInfos, resolve);
    }).catch(err => err);
    if (result != Cr.NS_OK) {
      Cu.reportError(`certStorage.addCerts failed: ${result}`);
      return;
    }
    await col.db.execute(transaction => {
      recordsToUpdate.forEach(record => {
        transaction.update({ ...record, cert_import_complete: true });
      });
    });

    Services.obs.notifyObservers(
      null,
      "remote-security-settings:intermediates-updated",
      "success"
    );
  }

  async onObservePollEnd(subject, topic, data) {
    log.debug(`onObservePollEnd ${subject} ${topic}`);

    try {
      await this.updatePreloadedIntermediates();
    } catch (err) {
      log.warn(`Unable to update intermediate preloads: ${err}`);
    }
  }

  // This method returns a promise to RemoteSettingsClient.maybeSync method.
  async onSync({ data: { current, created, updated, deleted } }) {
    if (!Services.prefs.getBoolPref(INTERMEDIATES_ENABLED_PREF, true)) {
      log.debug("Intermediate Preloading is disabled");
      return;
    }

    log.debug(`Removing ${deleted.length} Intermediate certificates`);
    await this.removeCerts(deleted);
    let certStorage = Cc["@mozilla.org/security/certstorage;1"].getService(
      Ci.nsICertStorage
    );
    let hasPriorCRLiteData = await new Promise(resolve => {
      certStorage.hasPriorData(
        Ci.nsICertStorage.DATA_TYPE_CRLITE,
        (rv, hasPriorData) => {
          if (rv == Cr.NS_OK) {
            resolve(hasPriorData);
          } else {
            resolve(false);
          }
        }
      );
    });
    if (!hasPriorCRLiteData) {
      deleted = [];
      updated = [];
      created = current;
    }
    const toAdd = created.concat(updated.map(u => u.new));
    let entries = [];
    for (let entry of deleted) {
      entries.push(
        new CRLiteState(
          entry.subjectDN,
          entry.pubKeyHash,
          Ci.nsICertStorage.STATE_UNSET
        )
      );
    }
    for (let entry of toAdd) {
      entries.push(
        new CRLiteState(
          entry.subjectDN,
          entry.pubKeyHash,
          entry.crlite_enrolled
            ? Ci.nsICertStorage.STATE_ENFORCE
            : Ci.nsICertStorage.STATE_UNSET
        )
      );
    }
    await new Promise(resolve => certStorage.setCRLiteState(entries, resolve));
  }

  /**
   * Downloads the attachment data of the given record. Does not retry,
   * leaving that to the caller.
   * @param  {AttachmentRecord} record The data to obtain
   * @return {Promise}          resolves to a Uint8Array on success
   */
  async _downloadAttachmentBytes(record) {
    const {
      attachment: { location },
    } = record;
    const remoteFilePath = (await baseAttachmentsURL) + location;
    const headers = new Headers();
    headers.set("Accept-Encoding", "gzip");

    return fetch(remoteFilePath, {
      headers,
      credentials: "omit",
    })
      .then(resp => {
        log.debug(`Download fetch completed: ${resp.ok} ${resp.status}`);
        if (!resp.ok) {
          Cu.reportError(`Failed to fetch ${remoteFilePath}: ${resp.status}`);
          return Promise.reject();
        }
        return resp.arrayBuffer();
      })
      .then(buffer => new Uint8Array(buffer));
  }

  /**
   * Attempts to download the attachment, assuming it's not been processed
   * already. Does not retry, and always resolves (e.g., does not reject upon
   * failure.) Errors are reported via Cu.reportError.
   * @param  {AttachmentRecord} record defines which data to obtain
   * @return {Promise}          a Promise that will resolve to an object with the properties
   *                            record, cert, and subject. record is the original record.
   *                            cert is the base64-encoded bytes of the downloaded certificate (if
   *                            downloading was successful), and null otherwise.
   *                            subject is the base64-encoded bytes of the subject distinguished
   *                            name of the same.
   */
  async maybeDownloadAttachment(record) {
    const {
      attachment: { hash, size },
    } = record;
    let result = { record, cert: null, subject: null };

    let attachmentData;
    try {
      attachmentData = await this._downloadAttachmentBytes(record);
    } catch (err) {
      Cu.reportError(`Failed to download attachment: ${err}`);
      return result;
    }

    if (!attachmentData || attachmentData.length == 0) {
      // Bug 1519273 - Log telemetry for these rejections
      log.debug(`Empty attachment. Hash=${hash}`);
      return result;
    }

    // check the length
    if (attachmentData.length !== size) {
      log.debug(
        `Unexpected attachment length. Hash=${hash} Lengths ${
          attachmentData.length
        } != ${size}`
      );
      return result;
    }

    // check the hash
    let dataAsString = gTextDecoder.decode(attachmentData);
    let calculatedHash = getHash(dataAsString);
    if (calculatedHash !== hash) {
      log.warn(
        `Invalid hash. CalculatedHash=${calculatedHash}, Hash=${hash}, data=${dataAsString}`
      );
      return result;
    }
    log.debug(`downloaded cert with hash=${hash}, size=${size}`);

    let certBase64;
    let subjectBase64;
    try {
      // split off the header and footer
      certBase64 = dataAsString.split("-----")[2].replace(/\s/g, "");
      // get an array of bytes so we can use X509.jsm
      let certBytes = stringToBytes(atob(certBase64));
      let cert = new X509.Certificate();
      cert.parse(certBytes);
      // get the DER-encoded subject and get a base64-encoded string from it
      // TODO(bug 1542028): add getters for _der and _bytes
      subjectBase64 = btoa(
        bytesToString(cert.tbsCertificate.subject._der._bytes)
      );
    } catch (err) {
      Cu.reportError(`Failed to decode cert: ${err}`);
      return result;
    }
    result.cert = certBase64;
    result.subject = subjectBase64;
    return result;
  }

  async maybeSync(expectedTimestamp, options) {
    return this.client.maybeSync(expectedTimestamp, options);
  }

  async removeCerts(recordsToRemove) {
    let certStorage = Cc["@mozilla.org/security/certstorage;1"].getService(
      Ci.nsICertStorage
    );
    let hashes = recordsToRemove.map(record => record.derHash);
    let result = await new Promise(resolve => {
      certStorage.removeCertsByHashes(hashes, resolve);
    }).catch(err => err);
    if (result != Cr.NS_OK) {
      Cu.reportError(`Failed to remove some intermediate certificates`);
    }
  }
}

// Helper function to compare filters. One filter is "less than" another filter (i.e. it sorts
// earlier) if its date is older than the other. Non-incremental filters sort earlier than
// incremental filters of the same date.
function compareFilters(filterA, filterB) {
  let timeA = new Date(filterA.details.name.replace(/-(full|diff)$/, ""));
  let timeB = new Date(filterB.details.name.replace(/-(full|diff)$/, ""));
  // If timeA is older (i.e. it is less than) timeB, it sorts earlier, so return a value less than
  // 0.
  if (timeA < timeB) {
    return -1;
  }
  if (timeA == timeB) {
    let incrementalA = filterA.details.name.includes("-diff");
    let incrementalB = filterB.details.name.includes("-diff");
    if (incrementalA == incrementalB) {
      return 0;
    }
    // If filterA is non-incremental, it sorts earlier, so return a value less than 0.
    if (!incrementalA) {
      return -1;
    }
  }
  // Otherwise, timeB is less recent or they have the same time but filterB is non-incremental while
  // filterA is incremental, so B sorts earlier, so return a value greater than 1.
  return 1;
}

class CRLiteFilters {
  constructor() {
    this.client = RemoteSettings(
      Services.prefs.getCharPref(CRLITE_FILTERS_COLLECTION_PREF),
      {
        bucketNamePref: CRLITE_FILTERS_BUCKET_PREF,
        lastCheckTimePref: CRLITE_FILTERS_CHECKED_SECONDS_PREF,
        signerName: Services.prefs.getCharPref(CRLITE_FILTERS_SIGNER_PREF),
      }
    );

    Services.obs.addObserver(
      this.onObservePollEnd.bind(this),
      "remote-settings:changes-poll-end"
    );
  }

  async onObservePollEnd(subject, topic, data) {
    if (!Services.prefs.getBoolPref(CRLITE_FILTERS_ENABLED_PREF, true)) {
      log.debug("CRLite filter downloading is disabled");
      Services.obs.notifyObservers(
        null,
        "remote-security-settings:crlite-filters-downloaded",
        "disabled"
      );
      return;
    }
    let col = await this.client.openCollection();
    let { data: current } = await col.list();
    let fullFilters = current.filter(filter => !filter.incremental);
    if (fullFilters.length < 1) {
      log.debug("no full CRLite filters to download?");
      Services.obs.notifyObservers(
        null,
        "remote-security-settings:crlite-filters-downloaded",
        "unavailable"
      );
      return;
    }
    fullFilters.sort(compareFilters);
    log.debug(fullFilters);
    let fullFilter = fullFilters.pop(); // the most recent filter sorts last
    let incrementalFilters = current.filter(
      filter =>
        // Return incremental filters that are more recent than (i.e. sort later than) the full
        // filter.
        filter.incremental && compareFilters(filter, fullFilter) > 0
    );
    incrementalFilters.sort(compareFilters);
    let filtersDownloaded = [];
    for (let filter of [fullFilter].concat(incrementalFilters)) {
      try {
        // If we've already downloaded this, the backend should just grab it from its cache.
        let localURI = await this.client.attachments.download(filter);
        let buffer = await (await fetch(localURI)).arrayBuffer();
        let bytes = new Uint8Array(buffer);
        log.debug(`Downloaded ${filter.details.name}: ${bytes.length} bytes`);
        // In a future bug, this code will pass the downloaded filter on to nsICertStorage.
        filtersDownloaded.push(filter.details.name);
      } catch (e) {
        Cu.reportError("failed to download CRLite filter", e);
      }
    }
    Services.obs.notifyObservers(
      null,
      "remote-security-settings:crlite-filters-downloaded",
      `finished;${filtersDownloaded.join(",")}`
    );
  }
}
