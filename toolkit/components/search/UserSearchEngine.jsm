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
 * UserSearchEngine represents a search engine defined by a user.
 */
class UserSearchEngine extends lazy.SearchEngine {
  /**
   * Creates a UserSearchEngine.
   *
   * @param {object} options
   * @param {object} [options.details]
   * @param {string} [options.details.name]
   *   The search engine name.
   * @param {string} [options.details.url]
   *   The search url for the engine.
   * @param {string} [options.details.keyword]
   *   The keyword for the engine.
   * @param {object} [options.json]
   *   An object that represents the saved JSON settings for the engine.
   */
  constructor(options = {}) {
    super({
      loadPath: "[other]addEngineWithDetails:set-via-user",
      isAppProvided: false,
    });

    if (options.details) {
      this._initFromManifest(
        "",
        "",
        {
          chrome_settings_overrides: {
            search_provider: {
              name: options.details.name,
              search_url: encodeURI(options.details.url),
              keyword: options.details.alias,
            },
          },
        },
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
}

var EXPORTED_SYMBOLS = ["UserSearchEngine"];
