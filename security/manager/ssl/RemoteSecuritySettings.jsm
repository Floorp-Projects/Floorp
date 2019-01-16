/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {RemoteSettings} = ChromeUtils.import("resource://services-settings/remote-settings.js", {});

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyGlobalGetters(this, ["fetch"]);

XPCOMUtils.defineLazyGetter(this, "gTextDecoder", () => new TextDecoder());

XPCOMUtils.defineLazyGetter(this, "baseAttachmentsURL", async () => {
  const server = Services.prefs.getCharPref("services.settings.server");
  const serverInfo = await (await fetch(`${server}/`)).json();
  const {capabilities: {attachments: {base_url}}} = serverInfo;
  return base_url;
});

const INTERMEDIATES_ENABLED_PREF          = "security.remote_settings.intermediates.enabled";
const INTERMEDIATES_COLLECTION_PREF      = "security.remote_settings.intermediates.collection";
const INTERMEDIATES_BUCKET_PREF          = "security.remote_settings.intermediates.bucket";
const INTERMEDIATES_SIGNER_PREF          = "security.remote_settings.intermediates.signer";
const INTERMEDIATES_CHECKED_SECONDS_PREF = "security.remote_settings.intermediates.checked";

let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(Ci.nsIX509CertDB);

function getHash(aStr) {
  // return the two-digit hexadecimal code for a byte
  let hasher = Cc["@mozilla.org/security/hash;1"].createInstance(Ci.nsICryptoHash);
  hasher.init(Ci.nsICryptoHash.SHA256);
  let stringStream = Cc["@mozilla.org/io/string-input-stream;1"].createInstance(Ci.nsIStringInputStream);
  stringStream.data = aStr;
  hasher.updateFromStream(stringStream, -1);

  // convert the binary hash data to a hex string.
  return hasher.finish(true);
}

// Remove all colons from a string
function stripColons(hexString) {
  return hexString.replace(/:/g, "");
}

this.RemoteSecuritySettings = class RemoteSecuritySettings {
    constructor() {
        this.onSync = this.onSync.bind(this);
        this.client = RemoteSettings(Services.prefs.getCharPref(INTERMEDIATES_COLLECTION_PREF), {
          bucketNamePref: INTERMEDIATES_BUCKET_PREF,
          lastCheckTimePref: INTERMEDIATES_CHECKED_SECONDS_PREF,
          signerName: Services.prefs.getCharPref(INTERMEDIATES_SIGNER_PREF),
          localFields: ["cert_import_complete"],
        });
        this.client.on("sync", this.onSync);
    }
    async onSync(event) {
        const {
          data: {deleted, current},
        } = event;

        if (!Services.prefs.getBoolPref(INTERMEDIATES_ENABLED_PREF, true)) {
          return;
        }

        const col = await this.client.openCollection();

        this.removeCerts(deleted);

        // Bug 1429800: once the CertStateService has the correct interface, also
        // store the whitelist status and crlite enrollment status

        // Download attachments that are awaiting download, up to a max.
        const maxDownloadsPerRun = 100;

        // Bug 1519256: Move this to a separate method that's on a separate timer
        // with a higher frequency (so we can attempt to download outstanding
        // certs more than once daily)

        let waiting = current.filter(record => !record.cert_import_complete);
        await Promise.all(waiting.slice(0, maxDownloadsPerRun)
          .map(record => this.maybeDownloadAttachment(record, col))
        );

        // Bug 1519273 - Log telemetry after a sync
    }

    /**
     * Downloads the attachment data of the given record. Does not retry,
     * leaving that to the caller.
     * @param  {AttachmentRecord} record The data to obtain
     * @return {Promise}          resolves to a Uint8Array on success
     */
    async _downloadAttachmentBytes(record) {
      const {attachment: {location}} = record;
      const remoteFilePath = (await baseAttachmentsURL) + location;
      const headers = new Headers();
      headers.set("Accept-Encoding", "gzip");

      return fetch(remoteFilePath, {headers})
      .then(resp => {
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
     * failure.) Errors are reported via Cu.reportError; If you need to know
     * success/failure, check record.cert_import_complete.
     * @param  {AttachmentRecord} record defines which data to obtain
     * @param  {KintoCollection}  col The kinto collection to update
     * @return {Promise}          a Promise representing the transaction
     */
    async maybeDownloadAttachment(record, col) {
      const {attachment: {hash, size}} = record;

      return this._downloadAttachmentBytes(record)
      .then(function(attachmentData) {
        if (!attachmentData || attachmentData.length == 0) {
          // Bug 1519273 - Log telemetry for these rejections
          return Promise.reject();
        }

        // check the length
        if (attachmentData.length !== size) {
          return Promise.reject();
        }

        // check the hash
        let dataAsString = gTextDecoder.decode(attachmentData);
        let calculatedHash = getHash(dataAsString);
        if (calculatedHash !== hash) {
          return Promise.reject();
        }

        // split off the header and footer, base64 decode, construct the cert
        // from the resulting DER data.
        let b64data = dataAsString.split("-----")[2].replace(/\s/g, "");
        let certDer = atob(b64data);

        // We can assume that roots obtained from remote-settings are part of
        // the root program. If they aren't, they won't be used for path-
        // building or have trust anyway, so just add it to the DB.
        certdb.addCert(certDer, ",,");

        record.cert_import_complete = true;
        return col.update(record);
      })
      .catch(() => {
        // Don't abort the outer Promise.all because of an error. Errors were
        // sent using Cu.reportError()
      });
    }

    async maybeSync(expectedTimestamp, options) {
      return this.client.maybeSync(expectedTimestamp, options);
    }

    // Note that removing certificates from the DB will likely not have an
    // effect until restart.
    async removeCerts(records) {
      let recordsToRemove = records;

      for (let cert of certdb.getCerts().getEnumerator()) {
        let certHash = stripColons(cert.sha256Fingerprint);
        for (let i = 0; i < recordsToRemove.length; i++) {
          let record = recordsToRemove[i];
          if (record.pubKeyHash == certHash) {
            certdb.deleteCertificate(cert);
            recordsToRemove.splice(i, 1);
            break;
          }
        }
      }

      if (recordsToRemove.length > 0) {
        Cu.reportError(`Failed to remove ${recordsToRemove.length} intermediate certificates`);
      }
    }
};

const EXPORTED_SYMBOLS = ["RemoteSecuritySettings"];
