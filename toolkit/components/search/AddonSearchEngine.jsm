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
 * AddonSearchEngine represents a search engine defined by an add-on.
 */
class AddonSearchEngine extends lazy.SearchEngine {
  /**
   * Creates a AddonSearchEngine.
   *
   * @param {object} options
   * @param {object} [options.details]
   *   An object that simulates the manifest object from a WebExtension.
   * @param {object} [options.json]
   *   An object that represents the saved JSON settings for the engine.
   */
  constructor({ isAppProvided, details, json } = {}) {
    super({
      loadPath:
        "[other]addEngineWithDetails:" +
        (details?.extensionID ?? json.extensionID ?? json._extensionID ?? null),
      isAppProvided,
    });

    if (details) {
      if (!details.extensionID) {
        throw Components.Exception(
          "Empty extensionID passed to _createAndAddEngine!",
          Cr.NS_ERROR_INVALID_ARG
        );
      }

      this._initFromManifest(
        details.extensionID,
        details.extensionBaseURI,
        details.manifest,
        details.locale,
        details.config
      );
    } else {
      this._initWithJSON(json);
    }
  }
}

var EXPORTED_SYMBOLS = ["AddonSearchEngine"];
