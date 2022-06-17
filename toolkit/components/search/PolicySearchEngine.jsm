/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint no-shadow: error, mozilla/no-aArgs: error */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  SearchEngine: "resource://gre/modules/SearchEngine.jsm",
  SearchUtils: "resource://gre/modules/SearchUtils.jsm",
});

/**
 * PolicySearchEngine represents a search engine defined by an enterprise
 * policy.
 */
class PolicySearchEngine extends lazy.SearchEngine {
  /**
   * Creates a PolicySearchEngine.
   *
   * @param {object} options
   * @param {object} [options.details]
   *   An object with the details for this search engine. See
   *   nsISearchService.addPolicyEngine for more details.
   * @param {object} [options.json]
   *   An object that represents the saved JSON settings for the engine.
   */
  constructor(options = {}) {
    super({
      loadPath: "[other]addEngineWithDetails:set-via-policy",
      isAppProvided: false,
    });

    if (options.details) {
      this._initFromManifest(
        "",
        "",
        options.details,
        lazy.SearchUtils.DEFAULT_TAG
      );
    } else {
      this._initWithJSON(options.json);
    }
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
    this._updateFromManifest("", "", details, lazy.SearchUtils.DEFAULT_TAG);
  }
}

var EXPORTED_SYMBOLS = ["PolicySearchEngine"];
