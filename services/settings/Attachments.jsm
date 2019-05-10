/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = [
  "Downloader",
];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.defineModuleGetter(this, "RemoteSettingsWorker",
                               "resource://services-settings/RemoteSettingsWorker.jsm");
ChromeUtils.defineModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyGlobalGetters(this, ["fetch"]);


class DownloadError extends Error {
  constructor(url, resp) {
    super(`Could not download ${url}`);
    this.name = "DownloadError";
    this.resp = resp;
  }
}

class BadContentError extends Error {
  constructor(path) {
    super(`${path} content does not match server hash`);
    this.name = "BadContentError";
  }
}

class Downloader {
  static get DownloadError() { return DownloadError; }
  static get BadContentError() { return BadContentError; }

  constructor(...folders) {
    this.folders = ["settings", ...folders];
    this._cdnURL = null;
  }

  /**
   * Download the record attachment into the local profile directory
   * and return a file:// URL that points to the local path.
   *
   * No-op if the file was already downloaded and not corrupted.
   *
   * @param {Object} record A Remote Settings entry with attachment.
   * @param {Object} options Some download options.
   * @param {Number} options.retries Number of times download should be retried (default: `3`)
   * @throws {Downloader.DownloadError} if the file could not be fetched.
   * @throws {Downloader.BadContentError} if the downloaded file integrity is not valid.
   * @returns {String} the absolute file path to the downloaded attachment.
   */
  async download(record, options = {}) {
    const { retries = 3 } = options;
    const { attachment: { location, filename, hash, size } } = record;
    const localFilePath = OS.Path.join(OS.Constants.Path.localProfileDir, ...this.folders, filename);
    const localFileUrl = `file://${[
      ...OS.Path.split(OS.Constants.Path.localProfileDir).components,
      ...this.folders,
      filename,
    ].join("/")}`;
    const remoteFileUrl = (await this._baseAttachmentsURL()) + location;

    await this._makeDirs();

    let retried = 0;
    while (true) {
      if (await RemoteSettingsWorker.checkFileHash(localFileUrl, size, hash)) {
        return localFileUrl;
      }
      // File does not exist or is corrupted.
      if (retried > retries) {
        throw new Downloader.BadContentError(localFilePath);
      }
      try {
        await this._fetchAttachment(remoteFileUrl, localFilePath);
      } catch (e) {
        if (retried >= retries) {
          throw e;
        }
      }
      retried++;
    }
  }

  /**
   * Delete the record attachment downloaded locally.
   * No-op if the related file does not exist.
   *
   * @param record A Remote Settings entry with attachment.
   */
  async delete(record) {
    const { attachment: { filename } } = record;
    const path = OS.Path.join(OS.Constants.Path.localProfileDir, ...this.folders, filename);
    await OS.File.remove(path, { ignoreAbsent: true });
    await this._rmDirs();
  }

  async _baseAttachmentsURL() {
    if (!this._cdnURL) {
      const server = Services.prefs.getCharPref("services.settings.server");
      const serverInfo = await (await fetch(`${server}/`)).json();
      // Server capabilities expose attachments configuration.
      const { capabilities: { attachments: { base_url } } } = serverInfo;
      // Make sure the URL always has a trailing slash.
      this._cdnURL = base_url + (base_url.endsWith("/") ? "" : "/");
    }
    return this._cdnURL;
  }

  async _fetchAttachment(url, destination) {
    const headers = new Headers();
    headers.set("Accept-Encoding", "gzip");
    const resp = await fetch(url, { headers });
    if (!resp.ok) {
      throw new Downloader.DownloadError(url, resp);
    }
    const buffer = await resp.arrayBuffer();
    await OS.File.writeAtomic(destination, buffer, { tmpPath: `${destination}.tmp` });
  }

  async _makeDirs() {
    const dirPath = OS.Path.join(OS.Constants.Path.localProfileDir, ...this.folders);
    await OS.File.makeDir(dirPath, { from: OS.Constants.Path.localProfileDir });
  }

  async _rmDirs() {
    for (let i = this.folders.length; i > 0; i--) {
      const dirPath = OS.Path.join(OS.Constants.Path.localProfileDir, ...this.folders.slice(0, i));
      try {
        await OS.File.removeEmptyDir(dirPath, { ignoreAbsent: true });
      } catch (e) {
        // This could fail if there's something in
        // the folder we're not permitted to remove.
        break;
      }
    }
  }
}
