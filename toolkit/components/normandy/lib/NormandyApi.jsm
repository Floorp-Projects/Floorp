/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
const {LogManager} = ChromeUtils.import("resource://normandy/lib/LogManager.jsm");

ChromeUtils.defineModuleGetter(
  this, "CanonicalJSON", "resource://gre/modules/CanonicalJSON.jsm");

XPCOMUtils.defineLazyGlobalGetters(this, ["fetch", "URL"]); /* globals fetch, URL */

var EXPORTED_SYMBOLS = ["NormandyApi"];

const log = LogManager.getLogger("normandy-api");
const prefs = Services.prefs.getBranch("app.normandy.");

let indexPromise = null;

var NormandyApi = {
  InvalidSignatureError: class InvalidSignatureError extends Error {},

  clearIndexCache() {
    indexPromise = null;
  },

  apiCall(method, endpoint, data = {}) {
    const url = new URL(endpoint);
    method = method.toLowerCase();

    let body = undefined;
    if (data) {
      if (method === "get") {
        for (const key of Object.keys(data)) {
          url.searchParams.set(key, data[key]);
        }
      } else if (method === "post") {
        body = JSON.stringify(data);
      }
    }

    const headers = {"Accept": "application/json"};
    return fetch(url.href, {method, body, headers});
  },

  get(endpoint, data) {
    return this.apiCall("get", endpoint, data);
  },

  post(endpoint, data) {
    return this.apiCall("post", endpoint, data);
  },

  absolutify(url) {
    if (url.startsWith("http")) {
      return url;
    }
    const apiBase = prefs.getCharPref("api_url");
    const server = new URL(apiBase).origin;
    if (url.startsWith("/")) {
      return server + url;
    }
    throw new Error("Can't use relative urls");
  },

  async getApiUrl(name) {
    if (!indexPromise) {
      const apiBase = new URL(prefs.getCharPref("api_url"));
      if (!apiBase.pathname.endsWith("/")) {
        apiBase.pathname += "/";
      }
      indexPromise = this.get(apiBase.toString()).then(res => res.json());
    }
    const index = await indexPromise;
    if (!(name in index)) {
      throw new Error(`API endpoint with name "${name}" not found.`);
    }
    const url = index[name];
    return this.absolutify(url);
  },

  async fetchSignedObjects(type, filters) {
    const signedObjectsUrl = await this.getApiUrl(`${type}-signed`);
    const objectsResponse = await this.get(signedObjectsUrl, filters);
    const rawText = await objectsResponse.text();
    const objectsWithSigs = JSON.parse(rawText);

    return Promise.all(
      objectsWithSigs.map(async (item) => {
        // Check that the rawtext (the object and the signature)
        // includes the CanonicalJSON version of the object. This isn't
        // strictly needed, but it is a great benefit for debugging
        // signature problems.
        const object = item[type];
        const serialized = CanonicalJSON.stringify(object);
        if (!rawText.includes(serialized)) {
          log.debug(rawText, serialized);
          throw new NormandyApi.InvalidSignatureError(
            `Canonical ${type} serialization does not match!`);
        }
        // Verify content signature using cryptography (will throw if fails).
        await this.verifyObjectSignature(serialized, item.signature, type);
        return object;
      })
    );
  },

  /**
   * Verify content signature, by serializing the specified `object` as
   * canonical JSON, and using the Normandy signer verifier to check that
   * it matches the signature specified in `signaturePayload`.
   *
   * @param {object|String} data The object (or string) to be checked
   * @param {object} signaturePayload The signature information
   * @param {String} signaturePayload.x5u The certificate chain URL
   * @param {String} signaturePayload.signature base64 signature bytes
   * @param {String} type The object type (eg. `"recipe"`, `"action"`)
   *
   * @throws {NormandyApi.InvalidSignatureError} if signature is invalid.
   */
  async verifyObjectSignature(data, signaturePayload, type) {
    const { signature, x5u } = signaturePayload;
    const certChainResponse = await this.get(this.absolutify(x5u));
    const certChain = await certChainResponse.text();
    const builtSignature = `p384ecdsa=${signature}`;

    const serialized = typeof data == "string" ? data : CanonicalJSON.stringify(data);

    const verifier = Cc["@mozilla.org/security/contentsignatureverifier;1"]
      .createInstance(Ci.nsIContentSignatureVerifier);

    let valid;
    try {
      valid = await verifier.asyncVerifyContentSignature(
        serialized,
        builtSignature,
        certChain,
        "normandy.content-signature.mozilla.org"
      );
    } catch (err) {
      throw new NormandyApi.InvalidSignatureError(`${type} signature validation failed: ${err}`);
    }

    if (!valid) {
      throw new NormandyApi.InvalidSignatureError(`${type} signature is not valid`);
    }
  },

  /**
   * Fetch metadata about this client determined by the server.
   * @return {object} Metadata specified by the server
   */
  async classifyClient() {
    const classifyClientUrl = await this.getApiUrl("classify-client");
    const response = await this.get(classifyClientUrl);
    const clientData = await response.json();
    clientData.request_time = new Date(clientData.request_time);
    return clientData;
  },

  /**
   * Fetch an array of available actions from the server.
   * @param filters
   * @param filters.enabled {boolean} If true, only returns enabled
   * recipes. Default true.
   * @resolves {Array}
   */
  async fetchRecipes(filters = {enabled: true}) {
    return this.fetchSignedObjects("recipe", filters);
  },

  /**
   * Fetch an array of available actions from the server.
   * @resolves {Array}
   */
  async fetchActions(filters = {}) {
    return this.fetchSignedObjects("action", filters);
  },

  /**
   * Fetch details for an extension from the server.
   * @param extensionId {integer} The ID of the extension to look up
   * @resolves {Object}
   */
  async fetchExtensionDetails(extensionId) {
    const baseUrl = await this.getApiUrl("extension-list");
    const extensionDetailsUrl = `${baseUrl}${extensionId}/`;
    const response = await this.get(extensionDetailsUrl);
    return response.json();
  },

  async fetchImplementation(action) {
    const implementationUrl = new URL(this.absolutify(action.implementation_url));

    // fetch implementation
    const response = await fetch(implementationUrl);
    if (!response.ok) {
      throw new Error(
        `Failed to fetch action implementation for ${action.name}: ${response.status}`
      );
    }
    const responseText = await response.text();

    // Try to verify integrity of the implementation text.  If the
    // integrity value doesn't match the content or uses an unknown
    // algorithm, fail.

    // Get the last non-empty portion of the url path, and split it
    // into two to get the aglorithm and hash.
    const parts = implementationUrl.pathname.split("/");
    const lastNonEmpty = parts.filter(p => p !== "").slice(-1)[0];
    const [algorithm, ...hashParts] = lastNonEmpty.split("-");
    const expectedHash = hashParts.join("-");

    if (algorithm !== "sha384") {
      throw new Error(
        `Failed to fetch action implemenation for ${action.name}: ` +
        `Unexpected integrity algorithm, expected "sha384", got ${algorithm}`
      );
    }

    // verify integrity hash
    const hasher = Cc["@mozilla.org/security/hash;1"].createInstance(Ci.nsICryptoHash);
    hasher.init(hasher.SHA384);
    const dataToHash = new TextEncoder().encode(responseText);
    hasher.update(dataToHash, dataToHash.length);
    const useBase64 = true;
    const hash = hasher.finish(useBase64).replace(/\+/g, "-").replace(/\//g, "_");
    if (hash !== expectedHash) {
      throw new Error(
        `Failed to fetch action implementation for ${action.name}: ` +
        `Integrity hash does not match content. Expected ${expectedHash} got ${hash}.`
      );
    }

    return responseText;
  },
};
