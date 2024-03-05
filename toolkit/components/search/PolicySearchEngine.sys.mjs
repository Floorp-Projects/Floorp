/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint no-shadow: error, mozilla/no-aArgs: error */

import { SearchEngine } from "resource://gre/modules/SearchEngine.sys.mjs";

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
   * @param {object} options.details
   *   An object that matches the `SearchEngines` policy schema.
   * @param {object} [options.settings]
   *   The saved settings for the user.
   *
   * @see browser/components/enterprisepolicies/schemas/policies-schema.json
   */
  constructor(options = {}) {
    let id = "policy-" + options.details.Name;

    super({
      loadPath: "[policy]",
      id,
    });

    // Map the data to look like a WebExtension manifest, as that is what
    // _initWithDetails requires.
    let details = {
      description: options.details.Description,
      iconURL: options.details.IconURL ? options.details.IconURL.href : null,
      name: options.details.Name,
      // If the encoding is not specified or is falsy, we will fall back to
      // the default encoding.
      encoding: options.details.Encoding,
      search_url: encodeURI(options.details.URLTemplate),
      keyword: options.details.Alias,
      search_url_post_params:
        options.details.Method == "POST" ? options.details.PostData : undefined,
      suggest_url: options.details.SuggestURLTemplate,
    };
    this._initWithDetails(details);

    this._loadSettings(options.settings);
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
