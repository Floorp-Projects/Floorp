/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://normandy/lib/LogManager.jsm");

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
    const apiBase = prefs.getCharPref("api_url");
    const server = new URL(apiBase).origin;
    if (url.startsWith("http")) {
      return url;
    } else if (url.startsWith("/")) {
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

    const verifiedObjects = [];

    for (const objectWithSig of objectsWithSigs) {
      const {signature, x5u} = objectWithSig.signature;
      const object = objectWithSig[type];

      const serialized = CanonicalJSON.stringify(object);
      // Check that the rawtext (the object and the signature)
      // includes the CanonicalJSON version of the object. This isn't
      // strictly needed, but it is a great benefit for debugging
      // signature problems.
      if (!rawText.includes(serialized)) {
        log.debug(rawText, serialized);
        throw new NormandyApi.InvalidSignatureError(
          `Canonical ${type} serialization does not match!`);
      }

      const certChainResponse = await this.get(this.absolutify(x5u));
      const certChain = await certChainResponse.text();
      const builtSignature = `p384ecdsa=${signature}`;

      const verifier = Cc["@mozilla.org/security/contentsignatureverifier;1"]
        .createInstance(Ci.nsIContentSignatureVerifier);

      let valid;
      try {
        valid = verifier.verifyContentSignature(
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

      verifiedObjects.push(object);
    }

    return verifiedObjects;
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
