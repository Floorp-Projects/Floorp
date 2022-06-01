/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const EXPORTED_SYMBOLS = ["RemoteSecuritySettings"];

const { RemoteSettings } = ChromeUtils.import(
  "resource://services-settings/remote-settings.js"
);

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { X509 } = ChromeUtils.import("resource://gre/modules/psm/X509.jsm");

const SECURITY_STATE_BUCKET = "security-state";
const SECURITY_STATE_SIGNER = "onecrl.content-signature.mozilla.org";

const INTERMEDIATES_DL_PER_POLL_PREF =
  "security.remote_settings.intermediates.downloads_per_poll";
const INTERMEDIATES_DL_PARALLEL_REQUESTS =
  "security.remote_settings.intermediates.parallel_downloads";
const INTERMEDIATES_ENABLED_PREF =
  "security.remote_settings.intermediates.enabled";
const LOGLEVEL_PREF = "browser.policies.loglevel";

const CRLITE_FILTERS_ENABLED_PREF =
  "security.remote_settings.crlite_filters.enabled";

const lazy = {};

XPCOMUtils.defineLazyGlobalGetters(lazy, ["fetch"]);

XPCOMUtils.defineLazyGetter(lazy, "gTextDecoder", () => new TextDecoder());

XPCOMUtils.defineLazyGetter(lazy, "log", () => {
  let { ConsoleAPI } = ChromeUtils.import("resource://gre/modules/Console.jsm");
  return new ConsoleAPI({
    prefix: "RemoteSecuritySettings.jsm",
    // tip: set maxLogLevel to "debug" and use log.debug() to create detailed
    // messages during development. See LOG_LEVELS in Console.jsm for details.
    maxLogLevel: "error",
    maxLogLevelPref: LOGLEVEL_PREF,
  });
});

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

class CRLiteCoverage {
  constructor(b64LogID, minTimestamp, maxTimestamp) {
    this.b64LogID = b64LogID;
    this.minTimestamp = minTimestamp;
    this.maxTimestamp = maxTimestamp;
  }
}
CRLiteCoverage.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsICRLiteCoverage",
]);

class CertInfo {
  constructor(cert, subject) {
    this.cert = cert;
    this.subject = subject;
    this.trust = Ci.nsICertStorage.TRUST_INHERIT;
  }
}
CertInfo.prototype.QueryInterface = ChromeUtils.generateQI(["nsICertInfo"]);

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
  ["nsIIssuerAndSerialRevocationState"]
);

class SubjectAndPubKeyRevocationState extends RevocationState {
  constructor(subject, pubKey, state) {
    super(state);
    this.subject = subject;
    this.pubKey = pubKey;
  }
}
SubjectAndPubKeyRevocationState.prototype.QueryInterface = ChromeUtils.generateQI(
  ["nsISubjectAndPubKeyRevocationState"]
);

function setRevocations(certStorage, revocations) {
  return new Promise(resolve =>
    certStorage.setRevocations(revocations, resolve)
  );
}

/**
 * Helper function that returns a promise that will resolve with whether or not
 * the nsICertStorage implementation has prior data of the given type.
 *
 * @param {Integer} dataType a Ci.nsICertStorage.DATA_TYPE_* constant
 *                           indicating the type of data

 * @return {Promise} a promise that will resolve with true if the data type is
 *                   present
 */
function hasPriorData(dataType) {
  let certStorage = Cc["@mozilla.org/security/certstorage;1"].getService(
    Ci.nsICertStorage
  );
  return new Promise(resolve => {
    certStorage.hasPriorData(dataType, (rv, hasPriorData) => {
      if (rv == Cr.NS_OK) {
        resolve(hasPriorData);
      } else {
        // If calling hasPriorData failed, assume we need to reload everything
        // (even though it's unlikely doing so will succeed).
        resolve(false);
      }
    });
  });
}

/**
 * Revoke the appropriate certificates based on the records from the blocklist.
 *
 * @param {Object} data   Current records in the local db.
 */
const updateCertBlocklist = async function({
  data: { current, created, updated, deleted },
}) {
  let items = [];

  // See if we have prior revocation data (this can happen when we can't open
  // the database and we have to re-create it (see bug 1546361)).
  let hasPriorRevocationData = await hasPriorData(
    Ci.nsICertStorage.DATA_TYPE_REVOCATION
  );

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
    const certList = Cc["@mozilla.org/security/certstorage;1"].getService(
      Ci.nsICertStorage
    );
    await setRevocations(certList, items);
  } catch (e) {
    Cu.reportError(e);
  }
};

var RemoteSecuritySettings = {
  /**
   * Initialize the clients (cheap instantiation) and setup their sync event.
   * This static method is called from BrowserGlue.jsm soon after startup.
   *
   * @returns {Object} intantiated clients for security remote settings.
   */
  init() {
    const OneCRLBlocklistClient = RemoteSettings("onecrl", {
      bucketName: SECURITY_STATE_BUCKET,
      signerName: SECURITY_STATE_SIGNER,
    });
    OneCRLBlocklistClient.on("sync", updateCertBlocklist);

    let IntermediatePreloadsClient = new IntermediatePreloads();
    let CRLiteFiltersClient = new CRLiteFilters();

    this.OneCRLBlocklistClient = OneCRLBlocklistClient;
    this.IntermediatePreloadsClient = IntermediatePreloadsClient;
    this.CRLiteFiltersClient = CRLiteFiltersClient;

    return {
      OneCRLBlocklistClient,
      IntermediatePreloadsClient,
      CRLiteFiltersClient,
    };
  },
};

class IntermediatePreloads {
  constructor() {
    this.client = RemoteSettings("intermediates", {
      bucketName: SECURITY_STATE_BUCKET,
      signerName: SECURITY_STATE_SIGNER,
      localFields: ["cert_import_complete"],
    });

    this.client.on("sync", this.onSync.bind(this));
    Services.obs.addObserver(
      this.onObservePollEnd.bind(this),
      "remote-settings:changes-poll-end"
    );

    lazy.log.debug("Intermediate Preloading: constructor");
  }

  async updatePreloadedIntermediates() {
    if (!Services.prefs.getBoolPref(INTERMEDIATES_ENABLED_PREF, true)) {
      lazy.log.debug("Intermediate Preloading is disabled");
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
    let hasPriorCertData = await hasPriorData(
      Ci.nsICertStorage.DATA_TYPE_CERTIFICATE
    );
    // If we don't have prior data, make it so we re-load everything.
    if (!hasPriorCertData) {
      let current;
      try {
        current = await this.client.db.list();
      } catch (err) {
        lazy.log.warn(
          `Unable to list intermediate preloading collection: ${err}`
        );
        return;
      }
      const toReset = current.filter(record => record.cert_import_complete);
      try {
        await this.client.db.importChanges(
          undefined, // do not touch metadata.
          undefined, // do not touch collection timestamp.
          toReset.map(r => ({ ...r, cert_import_complete: false }))
        );
      } catch (err) {
        lazy.log.warn(
          `Unable to update intermediate preloading collection: ${err}`
        );
        return;
      }
    }
    let current;
    try {
      current = await this.client.db.list();
    } catch (err) {
      lazy.log.warn(
        `Unable to list intermediate preloading collection: ${err}`
      );
      return;
    }
    const waiting = current.filter(record => !record.cert_import_complete);

    lazy.log.debug(
      `There are ${waiting.length} intermediates awaiting download.`
    );
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
    const certStorage = Cc["@mozilla.org/security/certstorage;1"].getService(
      Ci.nsICertStorage
    );
    let result = await new Promise(resolve => {
      certStorage.addCerts(certInfos, resolve);
    }).catch(err => err);
    if (result != Cr.NS_OK) {
      Cu.reportError(`certStorage.addCerts failed: ${result}`);
      return;
    }
    try {
      await this.client.db.importChanges(
        undefined, // do not touch metadata.
        undefined, // do not touch collection timestamp.
        recordsToUpdate.map(r => ({ ...r, cert_import_complete: true }))
      );
    } catch (err) {
      lazy.log.warn(
        `Unable to update intermediate preloading collection: ${err}`
      );
      return;
    }

    Services.obs.notifyObservers(
      null,
      "remote-security-settings:intermediates-updated",
      "success"
    );
  }

  async onObservePollEnd(subject, topic, data) {
    lazy.log.debug(`onObservePollEnd ${subject} ${topic}`);

    try {
      await this.updatePreloadedIntermediates();
    } catch (err) {
      lazy.log.warn(`Unable to update intermediate preloads: ${err}`);
    }
  }

  // This method returns a promise to RemoteSettingsClient.maybeSync method.
  async onSync({ data: { current, created, updated, deleted } }) {
    if (!Services.prefs.getBoolPref(INTERMEDIATES_ENABLED_PREF, true)) {
      lazy.log.debug("Intermediate Preloading is disabled");
      return;
    }

    lazy.log.debug(`Removing ${deleted.length} Intermediate certificates`);
    await this.removeCerts(deleted);
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
    let result = { record, cert: null, subject: null };

    let dataAsString = null;
    try {
      let buffer = await this.client.attachments.downloadAsBytes(record, {
        retries: 0,
      });
      dataAsString = lazy.gTextDecoder.decode(new Uint8Array(buffer));
    } catch (err) {
      if (err.name == "BadContentError") {
        lazy.log.debug(`Bad attachment content.`);
      } else {
        Cu.reportError(`Failed to download attachment: ${err}`);
      }
      return result;
    }

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
// earlier) if its timestamp is farther in the past than the other.
function compareFilters(filterA, filterB) {
  return filterA.effectiveTimestamp - filterB.effectiveTimestamp;
}

class CRLiteFilters {
  constructor() {
    this.client = RemoteSettings("cert-revocations", {
      bucketName: SECURITY_STATE_BUCKET,
      signerName: SECURITY_STATE_SIGNER,
      localFields: ["loaded_into_cert_storage"],
    });

    Services.obs.addObserver(
      this.onObservePollEnd.bind(this),
      "remote-settings:changes-poll-end"
    );
  }

  async onObservePollEnd(subject, topic, data) {
    if (!Services.prefs.getBoolPref(CRLITE_FILTERS_ENABLED_PREF, true)) {
      lazy.log.debug("CRLite filter downloading is disabled");
      Services.obs.notifyObservers(
        null,
        "remote-security-settings:crlite-filters-downloaded",
        "disabled"
      );
      return;
    }

    let hasPriorFilter = await hasPriorData(
      Ci.nsICertStorage.DATA_TYPE_CRLITE_FILTER_FULL
    );
    if (!hasPriorFilter) {
      let current = await this.client.db.list();
      let toReset = current.filter(
        record => !record.incremental && record.loaded_into_cert_storage
      );
      await this.client.db.importChanges(
        undefined, // do not touch metadata.
        undefined, // do not touch collection timestamp.
        toReset.map(r => ({ ...r, loaded_into_cert_storage: false }))
      );
    }
    let hasPriorStash = await hasPriorData(
      Ci.nsICertStorage.DATA_TYPE_CRLITE_FILTER_INCREMENTAL
    );
    if (!hasPriorStash) {
      let current = await this.client.db.list();
      let toReset = current.filter(
        record => record.incremental && record.loaded_into_cert_storage
      );
      await this.client.db.importChanges(
        undefined, // do not touch metadata.
        undefined, // do not touch collection timestamp.
        toReset.map(r => ({ ...r, loaded_into_cert_storage: false }))
      );
    }

    let current = await this.client.db.list();
    let fullFilters = current.filter(filter => !filter.incremental);
    if (fullFilters.length < 1) {
      lazy.log.debug("no full CRLite filters to download?");
      Services.obs.notifyObservers(
        null,
        "remote-security-settings:crlite-filters-downloaded",
        "unavailable"
      );
      return;
    }
    fullFilters.sort(compareFilters);
    lazy.log.debug("fullFilters:", fullFilters);
    let fullFilter = fullFilters.pop(); // the most recent filter sorts last
    let incrementalFilters = current.filter(
      filter =>
        // Return incremental filters that are more recent than (i.e. sort later than) the full
        // filter.
        filter.incremental && compareFilters(filter, fullFilter) > 0
    );
    incrementalFilters.sort(compareFilters);
    // Map of id to filter where that filter's parent has the given id.
    let parentIdMap = {};
    for (let filter of incrementalFilters) {
      if (filter.parent in parentIdMap) {
        lazy.log.debug(`filter with parent id ${filter.parent} already seen?`);
      } else {
        parentIdMap[filter.parent] = filter;
      }
    }
    let filtersToDownload = [];
    let nextFilter = fullFilter;
    while (nextFilter) {
      filtersToDownload.push(nextFilter);
      nextFilter = parentIdMap[nextFilter.id];
    }
    const certList = Cc["@mozilla.org/security/certstorage;1"].getService(
      Ci.nsICertStorage
    );
    filtersToDownload = filtersToDownload.filter(
      filter => !filter.loaded_into_cert_storage
    );
    lazy.log.debug("filtersToDownload:", filtersToDownload);
    let filtersDownloaded = [];
    for (let filter of filtersToDownload) {
      try {
        // If we've already downloaded this, the backend should just grab it from its cache.
        let localURI = await this.client.attachments.downloadToDisk(filter);
        let buffer = await (await lazy.fetch(localURI)).arrayBuffer();
        let bytes = new Uint8Array(buffer);
        lazy.log.debug(
          `Downloaded ${filter.details.name}: ${bytes.length} bytes`
        );
        filter.bytes = bytes;
        filtersDownloaded.push(filter);
      } catch (e) {
        lazy.log.debug(e);
        Cu.reportError("failed to download CRLite filter", e);
      }
    }
    let fullFiltersDownloaded = filtersDownloaded.filter(
      filter => !filter.incremental
    );
    if (fullFiltersDownloaded.length > 0) {
      if (fullFiltersDownloaded.length > 1) {
        lazy.log.warn("trying to install more than one full CRLite filter?");
      }
      let filter = fullFiltersDownloaded[0];

      let coverage = [];
      if (filter.coverage) {
        for (let entry of filter.coverage) {
          coverage.push(
            new CRLiteCoverage(
              entry.logID,
              entry.minTimestamp,
              entry.maxTimestamp
            )
          );
        }
      }
      let enrollment = filter.enrolledIssuers ? filter.enrolledIssuers : [];

      await new Promise(resolve => {
        certList.setFullCRLiteFilter(filter.bytes, enrollment, coverage, rv => {
          lazy.log.debug(`setFullCRLiteFilter: ${rv}`);
          resolve();
        });
      });
    }
    let stashes = filtersDownloaded.filter(filter => filter.incremental);
    let totalLength = stashes.reduce(
      (sum, filter) => sum + filter.bytes.length,
      0
    );
    let concatenatedStashes = new Uint8Array(totalLength);
    let offset = 0;
    for (let filter of stashes) {
      concatenatedStashes.set(filter.bytes, offset);
      offset += filter.bytes.length;
    }
    if (concatenatedStashes.length > 0) {
      lazy.log.debug(
        `adding concatenated incremental updates of total length ${concatenatedStashes.length}`
      );
      await new Promise(resolve => {
        certList.addCRLiteStash(concatenatedStashes, rv => {
          lazy.log.debug(`addCRLiteStash: ${rv}`);
          resolve();
        });
      });
    }

    for (let filter of filtersDownloaded) {
      delete filter.bytes;
    }

    await this.client.db.importChanges(
      undefined, // do not touch metadata.
      undefined, // do not touch collection timestamp.
      filtersDownloaded.map(r => ({ ...r, loaded_into_cert_storage: true }))
    );

    Services.obs.notifyObservers(
      null,
      "remote-security-settings:crlite-filters-downloaded",
      `finished;${filtersDownloaded
        .map(filter => filter.details.name)
        .join(",")}`
    );
  }
}
