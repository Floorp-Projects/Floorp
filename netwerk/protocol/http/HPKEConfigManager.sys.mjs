/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

let knownConfigs = new Map();

export class HPKEConfigManager {
  static async get(aURL, aOptions = {}) {
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
    return this.fetchAndStore(aURL, aOptions);
  }

  static async fetchAndStore(aURL, aOptions = {}) {
    let fetchDate = Date.now();
    let resp = await fetch(aURL, { signal: aOptions.abortSignal });
    if (!resp?.ok) {
      throw new Error(
        `Fetching HPKE config from ${aURL} failed with error ${resp.status}`
      );
    }
    let config = await resp.blob().then(b => b.arrayBuffer());
    knownConfigs.set(aURL, { config, fetchDate });
    return config;
  }
}
