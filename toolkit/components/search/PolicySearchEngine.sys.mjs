/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint no-shadow: error, mozilla/no-aArgs: error */

import { SearchEngine } from "resource://gre/modules/SearchEngine.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  SearchUtils: "resource://gre/modules/SearchUtils.sys.mjs",
});

/**
 * PolicySearchEngine represents a search engine defined by an enterprise
 * policy.
 */
export class PolicySearchEngine extends SearchEngine {
  /**
   * Creates a PolicySearchEngine.
   *
   * @param {object} options
   *   The options for this search engine.
   * @param {object} [options.details]
   *   An object with the details for this search engine. See
   *   nsISearchService.addPolicyEngine for more details.
   * @param {object} [options.json]
   *   An object that represents the saved JSON settings for the engine.
   */
  constructor(options = {}) {
    let id = "policy-" + (options.details?.name ?? options.json._name);

    super({
      loadPath: "[policy]",
      id,
    });

    if (options.details) {
      this._initWithDetails(options.details);
    } else {
      this._initWithJSON(options.json);
    }
  }

  /**
   * Whether or not this engine is an in-memory only search engine.
   * These engines are typically application provided or policy engines,
   * where they are loaded every time on SearchService initialization
   * using the policy JSON or the extension manifest. Minimal details of the
   * in-memory engines are saved to disk, but they are never loaded
   * from the user's saved settings file.
   *
   * @returns {boolean}
   *   All policy engines are in-memory, so this always returns true.
   */
  get inMemory() {
    return true;
  }

  /**
   * Returns the appropriate identifier to use for telemetry.
   *
   * @returns {string}
   */
  get telemetryId() {
    return `other-${this.name}`;
  }

  /**
   * Updates a search engine that is specified from enterprise policies.
   *
   * @param {object} details
   *   An object that simulates the manifest object from a WebExtension. See
   *   nsISearchService.updatePolicyEngine for more details.
   */
  update(details) {
    this._urls = [];
    this._iconMapObj = null;

    this._initWithDetails(details);

    lazy.SearchUtils.notifyAction(this, lazy.SearchUtils.MODIFIED_TYPE.CHANGED);
  }

  /**
   * Creates a JavaScript object that represents this engine.
   *
   * @returns {object}
   *   An object suitable for serialization as JSON.
   */
  toJSON() {
    // For policy engines, we load them at every startup and we don't want to
    // store all their data in the settings file so just return the relevant
    // metadata.
    let json = super.toJSON();

    // We only want to return a sub-set of fields, as the details for this engine
    // are loaded on each startup from the enterprise policies.
    return {
      id: json.id,
      _name: json._name,
      // Load path is included so that we know this is an enterprise engine.
      _loadPath: json._loadPath,
      _metaData: json._metaData,
    };
  }
}
