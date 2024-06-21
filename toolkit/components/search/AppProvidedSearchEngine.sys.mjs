/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint no-shadow: error, mozilla/no-aArgs: error */

import {
  SearchEngine,
  EngineURL,
} from "resource://gre/modules/SearchEngine.sys.mjs";

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
  SearchUtils: "resource://gre/modules/SearchUtils.sys.mjs",
});

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "idleService",
  "@mozilla.org/widget/useridleservice;1",
  "nsIUserIdleService"
);

// After the user has been idle for 30s, we'll update icons if we need to.
const ICON_UPDATE_ON_IDLE_DELAY = 30;

/**
 * Handles loading application provided search engine icons from remote settings.
 */
class IconHandler {
  /**
   * The remote settings client for the search engine icons.
   *
   * @type {?RemoteSettingsClient}
   */
  #iconCollection = null;

  /**
   * The list of icon records from the remote settings collection.
   *
   * @type {?object[]}
   */
  #iconList = null;

  /**
   * A flag that indicates if we have queued an idle observer to update icons.
   *
   * @type {boolean}
   */
  #queuedIdle = false;

  /**
   * A map of pending updates that need to be applied to the engines. This is
   * keyed via record id, so that if multiple updates are queued for the same
   * record, then we will only update the engine once.
   *
   * @type {Map<string, object>}
   */
  #pendingUpdatesMap = new Map();

  constructor() {
    this.#iconCollection = lazy.RemoteSettings("search-config-icons");
    this.#iconCollection.on("sync", this._onIconListUpdated.bind(this));
  }

  /**
   * Returns the icon for the record that matches the engine identifier
   * and the preferred width.
   *
   * @param {string} engineIdentifier
   *   The identifier of the engine to match against.
   * @param {number} preferredWidth
   *   The preferred with of the icon.
   * @returns {string}
   *   An object URL that can be used to reference the contents of the specified
   *   source object.
   */
  async getIcon(engineIdentifier, preferredWidth) {
    if (!this.#iconList) {
      await this.#getIconList();
    }

    let iconRecords = this.#iconList.filter(r =>
      this._identifierMatches(engineIdentifier, r.engineIdentifiers)
    );

    if (!iconRecords.length) {
      console.warn("No icon found for", engineIdentifier);
      return null;
    }

    // Default to the first record, in the event we don't have any records
    // that match the width.
    let iconRecord = iconRecords[0];
    for (let record of iconRecords) {
      // TODO: Bug 1655070. We should be using the closest size, but for now use
      // an exact match.
      if (record.imageSize == preferredWidth) {
        iconRecord = record;
        break;
      }
    }

    let iconData;
    try {
      iconData = await this.#iconCollection.attachments.get(iconRecord);
    } catch (ex) {
      console.error(ex);
    }
    if (!iconData) {
      console.warn("Unable to find the icon for", engineIdentifier);
      // Queue an update in case we haven't downloaded it yet.
      this.#pendingUpdatesMap.set(iconRecord.id, iconRecord);
      this.#maybeQueueIdle();
      return null;
    }

    if (iconData.record.last_modified != iconRecord.last_modified) {
      // The icon we have stored is out of date, queue an update so that we'll
      // download the new icon.
      this.#pendingUpdatesMap.set(iconRecord.id, iconRecord);
      this.#maybeQueueIdle();
    }
    return URL.createObjectURL(
      new Blob([iconData.buffer], { type: iconRecord.attachment.mimetype })
    );
  }

  QueryInterface = ChromeUtils.generateQI(["nsIObserver"]);

  /**
   * Called when there is an update queued and the user has been observed to be
   * idle for ICON_UPDATE_ON_IDLE_DELAY seconds.
   *
   * This will always download new icons (added or updated), even if there is
   * no current engine that matches the identifiers. This is to ensure that we
   * have pre-populated the cache if the engine is added later for this user.
   *
   * We do not handle deletes, as remote settings will handle the cleanup of
   * removed records. We also do not expect the case where an icon is removed
   * for an active engine.
   *
   * @param {nsISupports} subject
   *   The subject of the observer.
   * @param {string} topic
   *   The topic of the observer.
   */
  async observe(subject, topic) {
    if (topic != "idle") {
      return;
    }

    this.#queuedIdle = false;
    lazy.idleService.removeIdleObserver(this, ICON_UPDATE_ON_IDLE_DELAY);

    // Update the icon list, in case engines will call getIcon() again.
    await this.#getIconList();

    let appProvidedEngines = await Services.search.getAppProvidedEngines();
    for (let record of this.#pendingUpdatesMap.values()) {
      let iconData;
      try {
        iconData = await this.#iconCollection.attachments.download(record);
      } catch (ex) {
        console.error("Could not download new icon", ex);
        continue;
      }

      for (let engine of appProvidedEngines) {
        await engine.maybeUpdateIconURL(
          record.engineIdentifiers,
          URL.createObjectURL(
            new Blob([iconData.buffer], {
              type: record.attachment.mimetype,
            })
          )
        );
      }
    }

    this.#pendingUpdatesMap.clear();
  }

  /**
   * Checks if the identifier matches any of the engine identifiers.
   *
   * @param {string} identifier
   *   The identifier of the engine.
   * @param {string[]} engineIdentifiers
   *   The list of engine identifiers to match against. This can include
   *   wildcards at the end of strings.
   * @returns {boolean}
   *   Returns true if the identifier matches any of the engine identifiers.
   */
  _identifierMatches(identifier, engineIdentifiers) {
    return engineIdentifiers.some(i => {
      if (i.endsWith("*")) {
        return identifier.startsWith(i.slice(0, -1));
      }
      return identifier == i;
    });
  }

  /**
   * Obtains the icon list from the remote settings collection.
   */
  async #getIconList() {
    try {
      this.#iconList = await this.#iconCollection.get();
    } catch (ex) {
      console.error(ex);
      this.#iconList = [];
    }
    if (!this.#iconList.length) {
      console.error("Failed to obtain search engine icon list records");
    }
  }

  /**
   * Called via a callback when remote settings updates the icon list. This
   * stores potential updates and queues an idle observer to apply them.
   *
   * @param {object} payload
   *   The payload from the remote settings collection.
   * @param {object} payload.data
   *   The payload data from the remote settings collection.
   * @param {object[]} payload.data.created
   *    The list of created records.
   * @param {object[]} payload.data.updated
   *    The list of updated records.
   */
  async _onIconListUpdated({ data: { created, updated } }) {
    created.forEach(record => {
      this.#pendingUpdatesMap.set(record.id, record);
    });
    for (let record of updated) {
      if (record.new) {
        this.#pendingUpdatesMap.set(record.new.id, record.new);
      }
    }
    this.#maybeQueueIdle();
  }

  /**
   * Queues an idle observer if there are pending updates.
   */
  #maybeQueueIdle() {
    if (this.#pendingUpdatesMap && !this.#queuedIdle) {
      this.#queuedIdle = true;
      lazy.idleService.addIdleObserver(this, ICON_UPDATE_ON_IDLE_DELAY);
    }
  }
}

/**
 * AppProvidedSearchEngine represents a search engine defined by the
 * search configuration.
 */
export class AppProvidedSearchEngine extends SearchEngine {
  static URL_TYPE_MAP = new Map([
    ["search", lazy.SearchUtils.URL_TYPE.SEARCH],
    ["suggestions", lazy.SearchUtils.URL_TYPE.SUGGEST_JSON],
    ["trending", lazy.SearchUtils.URL_TYPE.TRENDING_JSON],
  ]);
  static iconHandler = new IconHandler();

  /**
   * A promise for the blob URL of the icon. We save the promise to avoid
   * reentrancy issues.
   *
   * @type {?Promise<string>}
   */
  #blobURLPromise = null;

  /**
   * The identifier from the configuration.
   *
   * @type {?string}
   */
  #configurationId = null;

  /**
   * Whether or not this is a general purpose search engine.
   *
   * @type {boolean}
   */
  #isGeneralPurposeSearchEngine = false;

  /**
   * @param {object} options
   *   The options for this search engine.
   * @param {object} options.config
   *   The engine config from Remote Settings.
   * @param {object} [options.settings]
   *   The saved settings for the user.
   */
  constructor({ config, settings }) {
    // TODO Bug 1875912 - Remove the webextension.id and webextension.locale when
    // we're ready to remove old search-config and use search-config-v2 for all
    // clients. The id in appProvidedSearchEngine should be changed to
    // engine.identifier.
    let extensionId = config.webExtension.id;
    let id = config.webExtension.id + config.webExtension.locale;

    super({
      loadPath: "[app]" + extensionId,
      isAppProvided: true,
      id,
    });

    this._extensionID = extensionId;
    this._locale = config.webExtension.locale;

    this.#configurationId = config.identifier;
    this.#init(config);

    this._loadSettings(settings);
  }

  /**
   * Used to clean up the engine when it is removed. This will revoke the blob
   * URL for the icon.
   */
  async cleanup() {
    if (this.#blobURLPromise) {
      URL.revokeObjectURL(await this.#blobURLPromise);
      this.#blobURLPromise = null;
    }
  }

  /**
   * Update this engine based on new config, used during
   * config upgrades.

   * @param {object} options
   *   The options object.
   *
   * @param {object} options.configuration
   *   The search engine configuration for application provided engines.
   */
  update({ configuration } = {}) {
    this._urls = [];
    this.#init(configuration);
    lazy.SearchUtils.notifyAction(this, lazy.SearchUtils.MODIFIED_TYPE.CHANGED);
  }

  /**
   * This will update the application provided search engine if there is no
   * name change.
   *
   * @param {object} options
   *   The options object.
   * @param {object} [options.configuration]
   *   The search engine configuration for application provided engines.
   * @param {string} [options.locale]
   *   The locale to use for getting details of the search engine.
   * @returns {boolean}
   *   Returns true if the engine was updated, false otherwise.
   */
  async updateIfNoNameChange({ configuration, locale }) {
    if (this.name != configuration.name.trim()) {
      return false;
    }

    this.update({ locale, configuration });
    return true;
  }

  /**
   * Whether or not this engine is provided by the application, e.g. it is
   * in the list of configured search engines. Overrides the definition in
   * `SearchEngine`.
   *
   * @returns {boolean}
   */
  get isAppProvided() {
    return true;
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
   *   Only returns true for application provided engines.
   */
  get inMemory() {
    return true;
  }

  /**
   * Whether or not this engine is a "general" search engine, e.g. is it for
   * generally searching the web, or does it have a specific purpose like
   * shopping.
   *
   * @returns {boolean}
   */
  get isGeneralPurposeEngine() {
    return this.#isGeneralPurposeSearchEngine;
  }

  /**
   * Returns the icon URL for the search engine closest to the preferred width.
   *
   * @param {number} preferredWidth
   *   The preferred width of the image.
   * @returns {Promise<string>}
   *   A promise that resolves to the URL of the icon.
   */
  async getIconURL(preferredWidth) {
    if (this.#blobURLPromise) {
      return this.#blobURLPromise;
    }
    this.#blobURLPromise = AppProvidedSearchEngine.iconHandler.getIcon(
      this.#configurationId,
      preferredWidth
    );
    return this.#blobURLPromise;
  }

  /**
   * This will update the icon URL for the search engine if the engine
   * identifier matches the given engine identifiers.
   *
   * @param {string[]} engineIdentifiers
   *   The engine identifiers to check against.
   * @param {string} blobURL
   *   The new icon URL for the search engine.
   */
  async maybeUpdateIconURL(engineIdentifiers, blobURL) {
    // TODO: Bug 1875912. Once newSearchConfigEnabled has been enabled, we will
    // be able to use `this.id` instead of `this.#configurationId`. At that
    // point, `IconHandler._identifierMatches` can be made into a private
    // function, as this if statement can be handled within `IconHandler.observe`.
    if (
      !AppProvidedSearchEngine.iconHandler._identifierMatches(
        this.#configurationId,
        engineIdentifiers
      )
    ) {
      return;
    }
    if (this.#blobURLPromise) {
      URL.revokeObjectURL(await this.#blobURLPromise);
      this.#blobURLPromise = null;
    }
    this.#blobURLPromise = Promise.resolve(blobURL);
    lazy.SearchUtils.notifyAction(
      this,
      lazy.SearchUtils.MODIFIED_TYPE.ICON_CHANGED
    );
  }

  /**
   * Creates a JavaScript object that represents this engine.
   *
   * @returns {object}
   *   An object suitable for serialization as JSON.
   */
  toJSON() {
    // For applicaiton provided engines we don't want to store all their data in
    // the settings file so just store the relevant metadata.
    return {
      id: this.id,
      _name: this.name,
      _isAppProvided: true,
      _metaData: this._metaData,
    };
  }

  /**
   * Initializes the engine.
   *
   * @param {object} [engineConfig]
   *   The search engine configuration for application provided engines.
   */
  #init(engineConfig) {
    this._orderHint = engineConfig.orderHint;
    this._telemetryId = engineConfig.identifier;
    this.#isGeneralPurposeSearchEngine =
      engineConfig.classification == "general";

    if (engineConfig.charset) {
      this._queryCharset = engineConfig.charset;
    }

    if (engineConfig.telemetrySuffix) {
      this._telemetryId += `-${engineConfig.telemetrySuffix}`;
    }

    if (engineConfig.clickUrl) {
      this.clickUrl = engineConfig.clickUrl;
    }

    this._name = engineConfig.name.trim();
    this._definedAliases =
      engineConfig.aliases?.map(alias => `@${alias}`) ?? [];

    for (const [type, urlData] of Object.entries(engineConfig.urls)) {
      this.#setUrl(type, urlData, engineConfig.partnerCode);
    }
  }

  /**
   * This sets the urls for the search engine based on the supplied parameters.
   *
   * @param {string} type
   *   The type of url. This could be a url for search, suggestions, or trending.
   * @param {object} urlData
   *   The url data contains the template/base url and url params.
   * @param {string} partnerCode
   *   The partner code associated with the search engine.
   */
  #setUrl(type, urlData, partnerCode) {
    let urlType = AppProvidedSearchEngine.URL_TYPE_MAP.get(type);

    if (!urlType) {
      console.warn("unexpected engine url type.", type);
      return;
    }

    let engineURL = new EngineURL(
      urlType,
      urlData.method || "GET",
      urlData.base
    );

    if (urlData.params) {
      for (const param of urlData.params) {
        switch (true) {
          case "value" in param:
            engineURL.addParam(
              param.name,
              param.value == "{partnerCode}" ? partnerCode : param.value
            );
            break;
          case "experimentConfig" in param:
            engineURL._addMozParam({
              name: param.name,
              pref: param.experimentConfig,
              condition: "pref",
            });
            break;
          case "searchAccessPoint" in param:
            for (const [key, value] of Object.entries(
              param.searchAccessPoint
            )) {
              engineURL.addParam(
                param.name,
                value,
                key == "addressbar" ? "keyword" : key
              );
            }
            break;
        }
      }
    }

    if (
      !("searchTermParamName" in urlData) &&
      !urlData.base.includes("{searchTerms}") &&
      !urlType.includes("trending")
    ) {
      throw new Error("Search terms missing from engine URL.");
    }

    if ("searchTermParamName" in urlData) {
      // The search term parameter is always added last, which will add it to the
      // end of the URL. This is because in the past we have seen users trying to
      // modify their searches by altering the end of the URL.
      engineURL.addParam(urlData.searchTermParamName, "{searchTerms}");
    }

    this._urls.push(engineURL);
  }
}
