/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

let knownConfigs = new Map();

export class OHTTPConfigManager {
  static async get(aURL, aOptions = {}) {
    // If we're in a child, forward to the parent.
    let { remoteType } = Services.appinfo;
    if (remoteType) {
      if (remoteType != "privilegedabout") {
        // The remoteTypes definition in the actor definition will enforce
        // that calling getActor fails, this is a more readable error:
        throw new Error(
          "OHTTPConfigManager cannot be used outside of the privilegedabout process."
        );
      }
      let actor = ChromeUtils.domProcessChild.getActor("OHTTPConfigManager");
      return actor.sendQuery("getconfig", { url: aURL, options: aOptions });
    }
    try {
      let config = await this.#getInternal(aURL, aOptions);
      return new Uint8Array(config);
    } catch (ex) {
      console.error(ex);
      return null;
    }
  }

  static async #getInternal(aURL, aOptions = {}) {
    let { maxAge = -1 } = aOptions;
    let knownConfig = knownConfigs.get(aURL);
    if (
      knownConfig &&
      (maxAge < 0 || Date.now() - knownConfig.fetchDate < maxAge)
    ) {
      return knownConfig.config;
    }
    return this.#fetchAndStore(aURL, aOptions);
  }

  static async #fetchAndStore(aURL, aOptions = {}) {
    let fetchDate = Date.now();
    let resp = await fetch(aURL);
    if (!resp?.ok) {
      throw new Error(
        `Fetching OHTTP config from ${aURL} failed with error ${resp.status}`
      );
    }
    let config = await resp.blob().then(b => b.arrayBuffer());
    knownConfigs.set(aURL, { config, fetchDate });
    return config;
  }
}

export class OHTTPConfigManagerParent extends JSProcessActorParent {
  receiveMessage(msg) {
    if (msg.name == "getconfig") {
      return OHTTPConfigManager.get(msg.data.url, msg.data.options);
    }
    return null;
  }
}
