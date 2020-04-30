/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["Downloader"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "RemoteSettingsWorker",
  "resource://services-settings/RemoteSettingsWorker.jsm"
);
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
  static get DownloadError() {
    return DownloadError;
  }
  static get BadContentError() {
    return BadContentError;
  }

  constructor(...folders) {
    this.folders = ["settings", ...folders];
    this._cdnURL = null;
  }

  /**
   * @returns {Object} An object with async "get", "set" and "delete" methods.
   *                   The keys are strings, the values may be any object that
   *                   can be stored in IndexedDB (including Blob).
   */
  get cacheImpl() {
    throw new Error("This Downloader does not support caching");
  }

  /**
   * Download attachment and return the result together with the record.
   * If the requested record cannot be downloaded and fallbacks are enabled, the
   * returned attachment may have a different record than the input record.
   *
   * @param {Object} record A Remote Settings entry with attachment.
   *                        If omitted, the attachmentId option must be set.
   * @param {Object} options Some download options.
   * @param {Number} options.retries Number of times download should be retried (default: `3`)
   * @param {Number} options.checkHash Check content integrity (default: `true`)
   * @param {string} options.attachmentId The attachment identifier to use for
   *                                      caching and accessing the attachment.
   *                                      (default: record.id)
   * @param {Boolean} options.useCache Whether to use a cache to read and store
   *                                   the attachment. (default: false)
   * @param {Boolean} options.fallbackToCache Return the cached attachment when the
   *                                          input record cannot be fetched.
   *                                          (default: false)
   * @param {Boolean} options.fallbackToDump Use the remote settings dump as a
   *                                         potential source of the attachment.
   *                                         (default: false)
   * @throws {Downloader.DownloadError} if the file could not be fetched.
   * @throws {Downloader.BadContentError} if the downloaded content integrity is not valid.
   * @returns {Object} An object with two properties:
   *   buffer: ArrayBuffer with the file content.
   *   record: Record associated with the bytes.
   *   _source: identifies the source of the result. Used for testing.
   */
  async download(record, options) {
    let {
      retries,
      checkHash,
      attachmentId = record?.id,
      useCache = false,
      fallbackToCache = false,
      fallbackToDump = false,
    } = options || {};

    if (!useCache) {
      // For backwards compatibility.
      // WARNING: Its return type is different from what's documented.
      // See downloadToDisk's documentation.
      return this.downloadToDisk(record, options);
    }

    if (!this.cacheImpl) {
      throw new Error("useCache is true but there is no cacheImpl!");
    }

    if (!attachmentId) {
      // Check for pre-condition. This should not happen, but it is explicitly
      // checked to avoid mixing up attachments, which could be dangerous.
      throw new Error("download() was called without attachmentId or recordID");
    }

    let buffer, cachedRecord;
    if (useCache) {
      try {
        let cached = await this.cacheImpl.get(attachmentId);
        if (cached) {
          cachedRecord = cached.record;
          buffer = await cached.blob.arrayBuffer();
        }
      } catch (e) {
        Cu.reportError(e);
      }
    }

    if (buffer && record) {
      const { size, hash } = cachedRecord.attachment;
      if (
        record.attachment.size === size &&
        record.attachment.hash === hash &&
        (await RemoteSettingsWorker.checkContentHash(buffer, size, hash))
      ) {
        // Best case: File already downloaded and still up to date.
        return { buffer, record: cachedRecord, _source: "cache_match" };
      }
    }

    let errorIfAllFails;

    // No cached attachment available. Check if an attachment is available in
    // the dump that is packaged with the client.
    let dumpInfo;
    if (fallbackToDump && record) {
      try {
        dumpInfo = await this._readAttachmentDump(attachmentId);
        // Check if there is a match. If there is no match, we will fall through
        // to the next case (downloading from the network). We may still use the
        // dump at the end of the function as the ultimate fallback.
        if (record.attachment.hash === dumpInfo.record.attachment.hash) {
          return {
            buffer: await dumpInfo.readBuffer(),
            record: dumpInfo.record,
            _source: "dump_match",
          };
        }
      } catch (e) {
        fallbackToDump = false;
        errorIfAllFails = e;
      }
    }

    // There is no local version that matches the requested record.
    // Try to download the attachment specified in record.
    if (record && record.attachment) {
      try {
        const newBuffer = await this.downloadAsBytes(record, {
          retries,
          checkHash,
        });
        const blob = new Blob([newBuffer]);
        if (useCache) {
          // Caching is optional, don't wait for the cache before returning.
          this.cacheImpl
            .set(attachmentId, { record, blob })
            .catch(e => Cu.reportError(e));
        }
        return { buffer: newBuffer, record, _source: "remote_match" };
      } catch (e) {
        // No network, corrupted content, etc.
        errorIfAllFails = e;
      }
    }

    // Unable to find an attachment that matches the record. Consider falling
    // back to local versions, even if their attachment hash do not match the
    // one from the requested record.

    if (buffer && fallbackToCache) {
      const { size, hash } = cachedRecord.attachment;
      if (await RemoteSettingsWorker.checkContentHash(buffer, size, hash)) {
        return { buffer, record: cachedRecord, _source: "cache_fallback" };
      }
    }

    // Unable to find a valid attachment, fall back to the packaged dump.
    if (fallbackToDump) {
      try {
        dumpInfo = dumpInfo || (await this._readAttachmentDump(attachmentId));
        return {
          buffer: await dumpInfo.readBuffer(),
          record: dumpInfo.record,
          _source: "dump_fallback",
        };
      } catch (e) {
        errorIfAllFails = e;
      }
    }

    if (errorIfAllFails) {
      throw errorIfAllFails;
    }

    throw new Downloader.DownloadError(attachmentId);
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
  async downloadToDisk(record, options = {}) {
    const { retries = 3 } = options;
    const {
      attachment: { filename, size, hash },
    } = record;
    const localFilePath = OS.Path.join(
      OS.Constants.Path.localProfileDir,
      ...this.folders,
      filename
    );
    const localFileUrl = `file://${[
      ...OS.Path.split(OS.Constants.Path.localProfileDir).components,
      ...this.folders,
      filename,
    ].join("/")}`;

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
        // Download and write on disk.
        const buffer = await this.downloadAsBytes(record, {
          checkHash: false, // Hash will be checked on file.
          retries: 0, // Already in a retry loop.
        });
        await OS.File.writeAtomic(localFilePath, buffer, {
          tmpPath: `${localFilePath}.tmp`,
        });
      } catch (e) {
        if (retried >= retries) {
          throw e;
        }
      }
      retried++;
    }
  }

  /**
   * Download the record attachment and return its content as bytes.
   *
   * @param {Object} record A Remote Settings entry with attachment.
   * @param {Object} options Some download options.
   * @param {Number} options.retries Number of times download should be retried (default: `3`)
   * @param {Number} options.checkHash Check content integrity (default: `true`)
   * @throws {Downloader.DownloadError} if the file could not be fetched.
   * @throws {Downloader.BadContentError} if the downloaded content integrity is not valid.
   * @returns {ArrayBuffer} the file content.
   */
  async downloadAsBytes(record, options = {}) {
    const {
      attachment: { location, hash, size },
    } = record;

    const remoteFileUrl = (await this._baseAttachmentsURL()) + location;

    const { retries = 3, checkHash = true } = options;
    let retried = 0;
    while (true) {
      try {
        const buffer = await this._fetchAttachment(remoteFileUrl);
        if (!checkHash) {
          return buffer;
        }
        if (await RemoteSettingsWorker.checkContentHash(buffer, size, hash)) {
          return buffer;
        }
        // Content is corrupted.
        throw new Downloader.BadContentError(location);
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
    const {
      attachment: { filename },
    } = record;
    const path = OS.Path.join(
      OS.Constants.Path.localProfileDir,
      ...this.folders,
      filename
    );
    await OS.File.remove(path, { ignoreAbsent: true });
    await this._rmDirs();
  }

  async deleteCached(attachmentId) {
    return this.cacheImpl.delete(attachmentId);
  }

  async _baseAttachmentsURL() {
    if (!this._cdnURL) {
      const server = Services.prefs.getCharPref("services.settings.server");
      const serverInfo = await (await fetch(`${server}/`)).json();
      // Server capabilities expose attachments configuration.
      const {
        capabilities: {
          attachments: { base_url },
        },
      } = serverInfo;
      // Make sure the URL always has a trailing slash.
      this._cdnURL = base_url + (base_url.endsWith("/") ? "" : "/");
    }
    return this._cdnURL;
  }

  async _fetchAttachment(url) {
    const headers = new Headers();
    headers.set("Accept-Encoding", "gzip");
    const resp = await fetch(url, { headers });
    if (!resp.ok) {
      throw new Downloader.DownloadError(url, resp);
    }
    return resp.arrayBuffer();
  }

  async _readAttachmentDump(attachmentId) {
    async function fetchResource(resourceUrl) {
      try {
        return await fetch(resourceUrl);
      } catch (e) {
        throw new Downloader.DownloadError(resourceUrl);
      }
    }
    const resourceUrlPrefix =
      Downloader._RESOURCE_BASE_URL + "/" + this.folders.join("/") + "/";
    const recordUrl = `${resourceUrlPrefix}${attachmentId}.meta.json`;
    const attachmentUrl = `${resourceUrlPrefix}${attachmentId}`;
    const record = await (await fetchResource(recordUrl)).json();
    return {
      record,
      async readBuffer() {
        return (await fetchResource(attachmentUrl)).arrayBuffer();
      },
    };
  }

  // Separate variable to allow tests to override this.
  static _RESOURCE_BASE_URL = "resource://app/defaults";

  async _makeDirs() {
    const dirPath = OS.Path.join(
      OS.Constants.Path.localProfileDir,
      ...this.folders
    );
    await OS.File.makeDir(dirPath, { from: OS.Constants.Path.localProfileDir });
  }

  async _rmDirs() {
    for (let i = this.folders.length; i > 0; i--) {
      const dirPath = OS.Path.join(
        OS.Constants.Path.localProfileDir,
        ...this.folders.slice(0, i)
      );
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
