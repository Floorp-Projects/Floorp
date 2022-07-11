/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { RemoteSettings } = ChromeUtils.import(
  "resource://services-settings/remote-settings.js"
);
const FileUtils = ChromeUtils.import("resource://gre/modules/FileUtils.jsm")
  .FileUtils;

const EXPORTED_SYMBOLS = ["PublicSuffixList"];

const RECORD_ID = "tld-dafsa";
const SIGNAL = "public-suffix-list-updated";

const PublicSuffixList = {
  CLIENT: RemoteSettings("public-suffix-list"),

  init() {
    // Only initialize once.
    if (this._initialized) {
      return;
    }
    this._initialized = true;

    this.CLIENT.on("sync", this.onUpdate.bind(this));
    /* We have a single record for this collection. Let's see if we already have it locally.
     * Note that on startup, we don't need to synchronize immediately on new profiles.
     */
    this.CLIENT.get({ syncIfEmpty: false, filters: { id: RECORD_ID } })
      .then(async records => {
        if (records.length == 1) {
          // Get the downloaded file URI (most likely to be a no-op here, since file will exist).
          const fileURI = await this.CLIENT.attachments.downloadToDisk(
            records[0]
          );
          // Send a signal so that the C++ code loads the updated list on startup.
          this.notifyUpdate(fileURI);
        }
      })
      .catch(err => console.error(err));
  },

  /**
   * This method returns the path to the file based on the file uri received
   * Example:
   * On windows "file://C:/Users/AppData/main/public-suffix-list/dafsa.bin"
   * will be converted to "C:\\Users\\main\\public-suffix-list\\dafsa.bin
   *
   * On macOS/linux "file:///home/main/public-suffix-list/dafsa.bin"
   * will be converted to "/home/main/public-suffix-list/dafsa.bin"
   */
  getFilePath(fileURI) {
    const uri = Services.io.newURI(fileURI);
    const file = uri.QueryInterface(Ci.nsIFileURL).file;
    return file.path;
  },

  notifyUpdate(fileURI) {
    if (!Services.prefs.getBoolPref("network.psl.onUpdate_notify", false)) {
      // Updating the PSL while Firefox is running could cause principals to
      // have a different base domain before/after the update.
      // See bug 1582647 comment 30
      return;
    }

    const filePath = this.getFilePath(fileURI);
    const nsifile = new FileUtils.File(filePath);
    /* Send a signal to be read by the C++, the method
     * ::Observe(nsISupports* aSubject, const char* aTopic, const char16_t* aData)
     * at netwerk/dns/nsEffectiveTLDService.cpp
     */
    Services.obs.notifyObservers(
      nsifile, // aSubject
      SIGNAL, // aTopic
      filePath // aData
    );
  },

  async onUpdate({ data: { created, updated, deleted } }) {
    // In theory, this will never happen, we will never delete the record.
    if (deleted.length == 1) {
      await this.CLIENT.attachments.deleteFromDisk(deleted[0]);
    }
    // Handle creation and update the same way
    const changed = created.concat(updated.map(u => u.new));
    /* In theory, we should never have more than one record. And if we receive
     * this event, it's because the single record was updated.
     */
    if (changed.length != 1) {
      console.warn("Unsupported sync event for Public Suffix List");
      return;
    }
    // Download the updated file.
    let fileURI;
    try {
      fileURI = await this.CLIENT.attachments.downloadToDisk(changed[0]);
    } catch (err) {
      Cu.reportError(err);
      return;
    }

    // Notify the C++ part to reload it from disk.
    this.notifyUpdate(fileURI);
  },
};
