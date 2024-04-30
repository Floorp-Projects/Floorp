/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  clearTimeout: "resource://gre/modules/Timer.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "console", () => {
  return console.createInstance({
    maxLogLevelPref: "browser.ml.logLevel",
    prefix: "ML",
  });
});

const ALLOWED_HUBS = [
  "chrome://*",
  "resource://*",
  "http://localhost",
  "https://localhost",
  "https://model-hub.mozilla.org",
];

const ALLOWED_HEADERS_KEYS = ["Content-Type", "ETag", "status"];
const DEFAULT_URL_TEMPLATE = "{model}/resolve/{revision}";

/**
 * Checks if a given URL string corresponds to an allowed hub.
 *
 * This function validates a URL against a list of allowed hubs, ensuring that it:
 * - Is well-formed according to the URL standard.
 * - Does not include a username or password.
 * - Matches the allowed scheme and hostname.
 *
 * @param {string} urlString The URL string to validate.
 * @returns {boolean} True if the URL is allowed; false otherwise.
 */
function allowedHub(urlString) {
  try {
    const url = new URL(urlString);
    // Check for username or password in the URL
    if (url.username !== "" || url.password !== "") {
      return false; // Reject URLs with username or password
    }
    const scheme = url.protocol;
    const host = url.hostname;
    const fullPrefix = `${scheme}//${host}`;

    return ALLOWED_HUBS.some(allowedHub => {
      const [allowedScheme, allowedHost] = allowedHub.split("://");
      if (allowedHost === "*") {
        return `${allowedScheme}:` === scheme;
      }
      const allowedPrefix = `${allowedScheme}://${allowedHost}`;
      return fullPrefix === allowedPrefix;
    });
  } catch (error) {
    lazy.console.error("Error parsing URL:", error);
    return false;
  }
}

const NO_ETAG = "NO_ETAG";

/**
 * Class for managing a cache stored in IndexedDB.
 */
export class IndexedDBCache {
  /**
   * Reference to the IndexedDB database.
   *
   * @type {IDBDatabase|null}
   */
  db = null;

  /**
   * Version of the database. Null if not set.
   *
   * @type {number|null}
   */
  dbVersion = null;

  /**
   * Total size of the files stored in the cache.
   *
   * @type {number}
   */
  totalSize = 0;

  /**
   * Name of the database used by IndexedDB.
   *
   * @type {string}
   */
  dbName;

  /**
   * Name of the object store for storing files.
   *
   * @type {string}
   */
  fileStoreName;

  /**
   * Name of the object store for storing headers.
   *
   * @type {string}
   */
  headersStoreName;
  /**
   * Maximum size of the cache in bytes. Defaults to 1GB.
   *
   * @type {number}
   */
  #maxSize = 1_073_741_824; // 1GB in bytes

  /**
   * Private constructor to prevent direct instantiation.
   * Use IndexedDBCache.init to create an instance.
   *
   * @param {string} dbName - The name of the database file.
   * @param {number} version - The version number of the database.
   */
  constructor(dbName = "modelFiles", version = 1) {
    this.dbName = dbName;
    this.dbVersion = version;
    this.fileStoreName = "files";
    this.headersStoreName = "headers";
  }

  /**
   * Static method to create and initialize an instance of IndexedDBCache.
   *
   * @param {string} [dbName="modelFiles"] - The name of the database.
   * @param {number} [version=1] - The version number of the database.
   * @returns {Promise<IndexedDBCache>} An initialized instance of IndexedDBCache.
   */
  static async init(dbName = "modelFiles", version = 1) {
    const cacheInstance = new IndexedDBCache(dbName, version);
    cacheInstance.db = await cacheInstance.#openDB();
    const storedSize = await cacheInstance.#getData(
      cacheInstance.headersStoreName,
      "totalSize"
    );
    cacheInstance.totalSize = storedSize ? storedSize.size : 0;
    return cacheInstance;
  }

  /**
   * Called to close the DB connection and dispose the instance
   *
   */
  async dispose() {
    if (this.db) {
      this.db.close();
      this.db = null;
    }
  }

  /**
   * Opens or creates the IndexedDB database.
   *
   * @returns {Promise<IDBDatabase>}
   */
  async #openDB() {
    return new Promise((resolve, reject) => {
      const request = indexedDB.open(this.dbName, this.dbVersion);
      request.onerror = event => reject(event.target.error);
      request.onsuccess = event => resolve(event.target.result);
      request.onupgradeneeded = event => {
        const db = event.target.result;
        if (!db.objectStoreNames.contains(this.fileStoreName)) {
          db.createObjectStore(this.fileStoreName, { keyPath: "id" });
        }
        if (!db.objectStoreNames.contains(this.headersStoreName)) {
          db.createObjectStore(this.headersStoreName, { keyPath: "id" });
        }
      };
    });
  }

  /**
   * Generic method to get the data from a specified object store.
   *
   * @param {string} storeName - The name of the object store.
   * @param {string} key - The key within the object store to retrieve the data from.
   * @returns {Promise<any>}
   */
  async #getData(storeName, key) {
    return new Promise((resolve, reject) => {
      const transaction = this.db.transaction([storeName], "readonly");
      const store = transaction.objectStore(storeName);
      const request = store.get(key);
      request.onerror = event => reject(event.target.error);
      request.onsuccess = event => resolve(event.target.result);
    });
  }

  // Used in tests
  async _testGetData(storeName, key) {
    return this.#getData(storeName, key);
  }

  /**
   * Generic method to update data in a specified object store.
   *
   * @param {string} storeName - The name of the object store.
   * @param {object} data - The data to store.
   * @returns {Promise<void>}
   */
  async #updateData(storeName, data) {
    return new Promise((resolve, reject) => {
      const transaction = this.db.transaction([storeName], "readwrite");
      const store = transaction.objectStore(storeName);
      const request = store.put(data);
      request.onerror = event => reject(event.target.error);
      request.onsuccess = () => resolve();
    });
  }

  /**
   * Deletes a specific cache entry.
   *
   * @param {string} storeName - The name of the object store.
   * @param {string} key - The key of the entry to delete.
   * @returns {Promise<void>}
   */
  async #deleteData(storeName, key) {
    return new Promise((resolve, reject) => {
      const transaction = this.db.transaction([storeName], "readwrite");
      const store = transaction.objectStore(storeName);
      const request = store.delete(key);
      request.onerror = event => reject(event.target.error);
      request.onsuccess = () => resolve();
    });
  }

  /**
   * Checks if a specified model file exists in storage.
   *
   * @param {string} model - The model name (organization/name)
   * @param {string} revision - The model revision.
   * @param {string} file - The file name.
   * @returns {Promise<boolean>} A promise that resolves with `true` if the key exists, otherwise `false`.
   */
  async fileExists(model, revision, file) {
    const storeName = this.fileStoreName;
    const cacheKey = `${model}/${revision}/${file}`;
    return new Promise((resolve, reject) => {
      const transaction = this.db.transaction([storeName], "readonly");
      const store = transaction.objectStore(storeName);
      const request = store.getKey(cacheKey);
      request.onerror = event => reject(event.target.error);
      request.onsuccess = event => resolve(event.target.result !== undefined);
    });
  }

  /**
   * Retrieves the headers for a specific cache entry.
   *
   * @param {string} model - The model name (organization/name)
   * @param {string} revision - The model revision.
   * @param {string} file - The file name.
   * @returns {Promise<object|null>} The headers or null if not found.
   */
  async getHeaders(model, revision, file) {
    const headersKey = `${model}/${revision}`;
    const cacheKey = `${model}/${revision}/${file}`;
    const headers = await this.#getData(this.headersStoreName, headersKey);
    if (headers && headers.files[cacheKey]) {
      return headers.files[cacheKey];
    }
    return null; // Return null if no headers is found
  }

  /**
   * Retrieves the file for a specific cache entry.
   *
   * @param {string} model - The model name (organization/name).
   * @param {string} revision - The model version.
   * @param {string} file - The file name.
   * @returns {Promise<[ArrayBuffer, object]|null>} The file ArrayBuffer and its headers or null if not found.
   */
  async getFile(model, revision, file) {
    const cacheKey = `${model}/${revision}/${file}`;
    const stored = await this.#getData(this.fileStoreName, cacheKey);
    if (stored) {
      const headers = await this.getHeaders(model, revision, file);
      return [stored.data, headers];
    }
    return null; // Return null if no file is found
  }

  /**
   * Adds or updates a cache entry.
   *
   * @param {string} model - The model name (organization/name).
   * @param {string} revision - The model version.
   * @param {string} file - The file name.
   * @param {ArrayBuffer} arrayBuffer - The data to cache.
   * @param {object} [headers] - The headers for the file.
   * @returns {Promise<void>}
   */
  async put(model, revision, file, arrayBuffer, headers = {}) {
    const cacheKey = `${model}/${revision}/${file}`;
    const newSize = this.totalSize + arrayBuffer.byteLength;
    if (newSize > this.#maxSize) {
      throw new Error("Exceeding total cache size limit of 1GB");
    }

    const headersKey = `${model}/${revision}`;
    const data = { id: cacheKey, data: arrayBuffer };

    // Store the file data
    await this.#updateData(this.fileStoreName, data);

    // Update headers store - whith defaults for ETag and Content-Type
    headers = headers || {};
    headers["Content-Type"] =
      headers["Content-Type"] ?? "application/octet-stream";
    headers.ETag = headers.ETag ?? NO_ETAG;

    // filter out any keys that are not allowed
    headers = Object.keys(headers)
      .filter(key => ALLOWED_HEADERS_KEYS.includes(key))
      .reduce((obj, key) => {
        obj[key] = headers[key];
        return obj;
      }, {});

    const headersStore = (await this.#getData(
      this.headersStoreName,
      headersKey
    )) || {
      id: headersKey,
      files: {},
    };
    headersStore.files[cacheKey] = headers;
    await this.#updateData(this.headersStoreName, headersStore);

    // Update size
    await this.#updateTotalSize(arrayBuffer.byteLength);
  }

  /**
   * Updates the total size of the cache.
   *
   * @param {number} sizeToAdd - The size to add to the total.
   * @returns {Promise<void>}
   */
  async #updateTotalSize(sizeToAdd) {
    this.totalSize += sizeToAdd;
    await this.#updateData(this.headersStoreName, {
      id: "totalSize",
      size: this.totalSize,
    });
  }
  /**
   * Deletes all data related to a specific model.
   *
   * @param {string} model - The model name (organization/name).
   * @param {string} revision - The model version.
   * @returns {Promise<void>}
   */
  async deleteModel(model, revision) {
    const headersKey = `${model}/${revision}`;
    const headers = await this.#getData(this.headersStoreName, headersKey);
    if (headers) {
      for (const fileKey in headers.files) {
        await this.#deleteData(this.fileStoreName, fileKey);
      }
      await this.#deleteData(this.headersStoreName, headersKey); // Remove headers entry after files are deleted
    }
  }

  /**
   * Lists all models stored in the cache.
   *
   * @returns {Promise<Array<string>>} An array of model identifiers.
   */
  async listModels() {
    const models = [];
    return new Promise((resolve, reject) => {
      const transaction = this.db.transaction(
        [this.headersStoreName],
        "readonly"
      );
      const store = transaction.objectStore(this.headersStoreName);
      const request = store.openCursor();
      request.onerror = event => reject(event.target.error);
      request.onsuccess = event => {
        const cursor = event.target.result;
        if (cursor) {
          models.push(cursor.value.id); // Assuming id is the organization/modelName
          cursor.continue();
        } else {
          resolve(models);
        }
      };
    });
  }
}

export class ModelHub {
  constructor({ rootUrl, urlTemplate = DEFAULT_URL_TEMPLATE }) {
    if (!allowedHub(rootUrl)) {
      throw new Error(`Invalid model hub root url: ${rootUrl}`);
    }
    this.rootUrl = rootUrl;
    this.cache = null;

    // Ensures the URL template is well-formed and does not contain any invalid characters.
    const pattern = /^(?:\{\w+\}|\w+)(?:\/(?:\{\w+\}|\w+))*$/;
    //               ^                                         $   Start and end of string
    //                (?:\{\w+\}|\w+)                            Match a {placeholder} or alphanumeric characters
    //                                 (?:\/(?:\$\{\w+\}|\w+))*    Zero or more groups of a forward slash followed by a ${placeholder} or alphanumeric characters
    if (!pattern.test(urlTemplate)) {
      throw new Error(`Invalid URL template: ${urlTemplate}`);
    }
    this.urlTemplate = urlTemplate;
  }

  async #initCache() {
    if (this.cache) {
      return;
    }
    this.cache = await IndexedDBCache.init();
  }

  /**
   * This method takes a model URL and parses it to extract the
   * model name, optional model version, and file path.
   *
   * The expected URL format are :
   *
   * `/organization/model/revision/filePath`
   * `https://hub/organization/model/revision/filePath`
   *
   * @param {string} url - The full URL to the model, including protocol and domain - or the relative path.
   * @returns {object} An object containing the parsed components of the URL. The
   *                   object has properties `model`, and `file`,
   *                   and optionally `revision` if the URL includes a version.
   * @throws {Error} Throws an error if the URL does not start with `this.rootUrl` or
   *                 if the URL format does not match the expected structure.
   *
   * @example
   * // For a URL
   * parseModelUrl("https://example.com/org1/model1/v1/file/path");
   * // returns { model: "org1/model1", revision: "v1", file: "file/path" }
   *
   * @example
   * // For a relative URL
   * parseModelUrl("/org1/model1/revision/file/path");
   * // returns { model: "org1/model1", revision: "v1", file: "file/path" }
   */
  parseUrl(url) {
    let parts;
    if (url.startsWith("/")) {
      // relative URL
      parts = url.slice(1).split("/");
    } else {
      // absolute URL
      if (!url.startsWith(this.rootUrl)) {
        throw new Error(`Invalid domain for model URL: ${url}`);
      }
      const urlObject = new URL(url);
      const rootUrlObject = new URL(this.rootUrl);

      // Remove the root URL's pathname from the full URL's pathname
      const relativePath = urlObject.pathname.substring(
        rootUrlObject.pathname.length
      );

      parts = relativePath.slice(1).split("/");
    }

    if (parts.length < 3) {
      throw new Error(`Invalid model URL: ${url}`);
    }

    const file = parts.slice(3).join("/");
    if (file == null || !file.length) {
      throw new Error(`Invalid model URL: ${url}`);
    }

    return {
      model: `${parts[0]}/${parts[1]}`,
      revision: parts[2],
      file,
    };
  }

  /** Creates the file URL from the organization, model, and version.
   *
   * @param {string} model
   * @param {string} revision
   * @param {string} file
   * @returns {string} The full URL
   */
  #fileUrl(model, revision, file) {
    const baseUrl = new URL(this.rootUrl);
    if (!baseUrl.pathname.endsWith("/")) {
      baseUrl.pathname += "/";
    }
    // Replace placeholders in the URL template with the provided data.
    // If some keys are missing in the data object, the placeholder is left as is.
    // If the placeholder is not found in the data object, it is left as is.
    const data = {
      model,
      revision,
    };
    let path = this.urlTemplate.replace(
      /\{(\w+)\}/g,
      (match, key) => data[key] || match
    );
    path = `${path}/${file}`;

    const fullPath = `${baseUrl.pathname}${
      path.startsWith("/") ? path.slice(1) : path
    }`;

    const urlObject = new URL(fullPath, baseUrl.origin);
    urlObject.searchParams.append("download", "true");
    return urlObject.toString();
  }

  /** Checks the model and revision inputs.
   *
   * @param { string } model
   * @param { string } revision
   * @param { string } file
   * @returns { Error } The error instance(can be null)
   */
  #checkInput(model, revision, file) {
    // Matches a string with the format 'organization/model' where:
    // - 'organization' consists only of letters, digits, and hyphens, cannot start or end with a hyphen,
    //   and cannot contain consecutive hyphens.
    // - 'model' can contain letters, digits, hyphens, underscores, or periods.
    //
    // Pattern breakdown:
    //   ^                                     Start of string
    //    (?!-)                                Negative lookahead for 'organization' not starting with hyphen
    //         (?!.*--)                        Negative lookahead for 'organization' not containing consecutive hyphens
    //                 [A-Za-z0-9-]+           'organization' part: Alphanumeric characters or hyphens
    //                            (?<!-)       Negative lookbehind for 'organization' not ending with a hyphen
    //                                  \/     Literal '/' character separating 'organization' and 'model'
    //                                    [A-Za-z0-9-_.]+    'model' part: Alphanumeric characters, hyphens, underscores, or periods
    //                                                  $    End of string
    const modelRegex = /^(?!-)(?!.*--)[A-Za-z0-9-]+(?<!-)\/[A-Za-z0-9-_.]+$/;

    // Matches strings consisting of alphanumeric characters, hyphens, or periods.
    //
    //                    ^               $   Start and end of string
    //                     [A-Za-z0-9-.]+     Alphanum characters, hyphens, or periods, one or more times
    const versionRegex = /^[A-Za-z0-9-.]+$/;

    // Matches filenames with subdirectories, starting with alphanumeric or underscore,
    // and optionally ending with a dot followed by a 2-4 letter extension.
    //
    //                 ^                                    $   Start and end of string
    //                  (?:\/)?                                  Optional leading slash (for absolute paths or root directory)
    //                        (?!\/)                             Negative lookahead for not starting with a slash
    //                              [A-Za-z0-9-_]+               First directory or filename
    //                                           (?:            Begin non-capturing group for additional directories or file
    //                                              \/              Directory separator
    //                                                [A-Za-z0-9-_]+ Directory or file name
    //                                                             )* Zero or more times
    //                                                                 (?:[.][A-Za-z]{2,4})?   Optional non-capturing group for file extension
    const fileRegex =
      /^(?:\/)?(?!\/)[A-Za-z0-9-_]+(?:\/[A-Za-z0-9-_]+)*(?:[.][A-Za-z]{2,4})?$/;

    if (!modelRegex.test(model)) {
      return new Error("Invalid model name.");
    }

    if (
      !versionRegex.test(revision) ||
      revision.includes(" ") ||
      /[\^$]/.test(revision)
    ) {
      return new Error("Invalid version identifier.");
    }

    if (!fileRegex.test(file)) {
      return new Error("Invalid file name");
    }

    return null;
  }

  /**
   * Returns the ETag value given an URL
   *
   * @param {string} url
   * @param {number} timeout in ms. Default is 1000
   * @returns {Promise<string>} ETag (can be null)
   */
  async getETag(url, timeout = 1000) {
    const controller = new AbortController();
    const id = lazy.setTimeout(() => controller.abort(), timeout);

    try {
      const headResponse = await fetch(url, {
        method: "HEAD",
        signal: controller.signal,
      });
      const currentEtag = headResponse.headers.get("ETag");
      return currentEtag;
    } catch (error) {
      lazy.console.warn("An error occurred when calling HEAD:", error);
      return null;
    } finally {
      lazy.clearTimeout(id);
    }
  }

  /**
   * Given an organization, model, and version, fetch a model file in the hub as a Response.
   *
   * @param {object} config
   * @param {string} config.model
   * @param {string} config.revision
   * @param {string} config.file
   * @returns {Promise<Response>} The file content
   */
  async getModelFileAsResponse({ model, revision, file }) {
    const [blob, headers] = await this.getModelFileAsBlob({
      model,
      revision,
      file,
    });

    return new Response(blob, { headers });
  }

  /**
   * Given an organization, model, and version, fetch a model file in the hub as an ArrayBuffer.
   *
   * @param {object} config
   * @param {string} config.model
   * @param {string} config.revision
   * @param {string} config.file
   * @returns {Promise<[ArrayBuffer, headers]>} The file content
   */
  async getModelFileAsArrayBuffer({ model, revision, file }) {
    const [blob, headers] = await this.getModelFileAsBlob({
      model,
      revision,
      file,
    });
    return [await blob.arrayBuffer(), headers];
  }

  /**
   * Given an organization, model, and version, fetch a model file in the hub as blob.
   *
   * @param {object} config
   * @param {string} config.model
   * @param {string} config.revision
   * @param {string} config.file
   * @returns {Promise<[Blob, object]>} The file content
   */
  async getModelFileAsBlob({ model, revision, file }) {
    // Make sure inputs are clean. We don't sanitize them but throw an exception
    let checkError = this.#checkInput(model, revision, file);
    if (checkError) {
      throw checkError;
    }
    const url = this.#fileUrl(model, revision, file);
    lazy.console.debug(`Getting model file from ${url}`);

    await this.#initCache();

    let useCached;

    // If the revision is `main` we want to check the ETag in the hub
    if (revision === "main") {
      // this can be null if no ETag was found or there were a network error
      const hubETag = await this.getETag(url);

      // Storage ETag lookup
      const cachedHeaders = await this.cache.getHeaders(model, revision, file);
      const cachedEtag = cachedHeaders ? cachedHeaders.ETag : null;

      // If we have something in store, and the hub ETag is null or it matches the cached ETag, return the cached response
      useCached =
        cachedEtag !== null && (hubETag === null || cachedEtag === hubETag);
    } else {
      // If we are dealing with a pinned revision, we ignore the ETag, to spare HEAD hits on every call
      useCached = await this.cache.fileExists(model, revision, file);
    }

    if (useCached) {
      lazy.console.debug(`Cache Hit for ${url}`);
      return await this.cache.getFile(model, revision, file);
    }

    lazy.console.debug(`Fetching ${url}`);
    try {
      const response = await fetch(url);
      if (response.ok) {
        const clone = response.clone();
        const headers = {
          // We don't store the boundary or the charset, just the content type,
          // so we drop what's after the semicolon.
          "Content-Type": response.headers.get("Content-Type").split(";")[0],
          ETag: response.headers.get("ETag"),
        };

        await this.cache.put(
          model,
          revision,
          file,
          await clone.blob(),
          headers
        );
        return [await response.blob(), headers];
      }
    } catch (error) {
      lazy.console.error(`Failed to fetch ${url}:`, error);
    }

    throw new Error(`Failed to fetch the model file: ${url}`);
  }
}
