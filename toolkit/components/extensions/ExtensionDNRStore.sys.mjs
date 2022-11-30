/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { ExtensionParent } = ChromeUtils.import(
  "resource://gre/modules/ExtensionParent.jsm"
);

const { ExtensionUtils } = ChromeUtils.import(
  "resource://gre/modules/ExtensionUtils.jsm"
);

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  Schemas: "resource://gre/modules/Schemas.jsm",
  PromiseUtils: "resource://gre/modules/PromiseUtils.jsm",
});

ChromeUtils.defineESModuleGetters(lazy, {
  ExtensionDNR: "resource://gre/modules/ExtensionDNR.sys.mjs",
});

const { DefaultMap, ExtensionError } = ExtensionUtils;
const { StartupCache } = ExtensionParent;

// DNR Rules store subdirectory/file names and file extensions.
//
// NOTE: each extension's stored rules are stored in a per-extension file
// and stored rules filename is derived from the extension uuid assigned
// at install time.
//
// TODO(Bug 1803365): consider introducing a startupCache file.
const RULES_STORE_DIRNAME = "extension-dnr";
const RULES_STORE_FILEEXT = ".json.lz4";

// Class defining the format of the data stored into the per-extension files
// managed by RulesetsStore.
//
// StoreData instances are saved in the profile extension-dir subdirectory as
// lz4-compressed JSON files, only the ruleset_id is stored on disk for the
// enabled static rulesets (while the actual rules would need to be loaded back
// from the related rules JSON files part of the extension assets).
class StoreData {
  // NOTE: Update schema version upgrade handling code in `RulesetsStore.#readData`
  // along with bumps to the schema version here.
  static VERSION = 1;

  /**
   * @param {object} params
   * @param {number} [params.schemaVersion=StoreData.VERSION]
   *        file schema version
   * @param {string} [params.extVersion]
   *        extension version
   * @param {Map<string, { rules }>} [params.staticRulesets=new Map()]
   *        map of the array of loaded ruleset rules by ruleset_id, or object keeping track
   *        rulesets explicitly disabled or disabled due to load or JSON parsing errors.
   */
  constructor({ schemaVersion, extVersion, staticRulesets } = {}) {
    this.schemaVersion = schemaVersion || this.constructor.VERSION;
    this.extVersion = extVersion ?? null;
    this.staticRulesets =
      staticRulesets instanceof Map ? staticRulesets : new Map();
  }

  get isEmpty() {
    return !this.staticRulesets.size;
  }

  // This method is used to convert the data in the format stored on disk
  // as a JSON file.
  toJSON() {
    const data = {
      schemaVersion: this.schemaVersion,
      extVersion: this.extVersion,
      // Only store the array of the enabled ruleset_id in the set of data
      // persisted in a JSON form.
      staticRulesets: this.staticRulesets
        ? Array.from(this.staticRulesets.entries(), ([id, _ruleset]) => id)
        : undefined,
    };
    return data;
  }
}

class Queue {
  #tasks = [];
  #runningTask = null;
  #closed = false;

  get hasPendingTasks() {
    return !!this.#runningTask || !!this.#tasks.length;
  }

  get isClosed() {
    return this.#closed;
  }

  async close() {
    if (this.#closed) {
      return;
    }
    const drainedQueuePromise = this.queueTask(() => {});
    this.#closed = true;
    return drainedQueuePromise;
  }

  queueTask(callback) {
    if (this.#closed) {
      throw new Error("Unexpected queueTask call on closed queue");
    }
    const deferred = lazy.PromiseUtils.defer();
    this.#tasks.push({ callback, deferred });
    // Run the queued task right away if there isn't one already running.
    if (!this.#runningTask) {
      this.#runNextTask();
    }
    return deferred.promise;
  }

  async #runNextTask() {
    if (!this.#tasks.length) {
      this.#runningTask = null;
      return;
    }

    this.#runningTask = this.#tasks.shift();
    const { callback, deferred } = this.#runningTask;
    try {
      let result = callback();
      if (result instanceof Promise) {
        result = await result;
      }
      deferred.resolve(result);
    } catch (err) {
      deferred.reject(err);
    }

    this.#runNextTask();
  }
}

/**
 * Class managing the rulesets persisted across browser sessions.
 *
 * The data gets stored in two per-extension files:
 *
 * - `ProfD/extension-dnr/EXT_UUID.json.lz4` is a lz4-compressed JSON file that is expected to include
 *   the ruleset ids for the enabled static rulesets and the dynamic rules.
 *
 * All browser data stored is expected to be persisted across browser updates, but the enabled static ruleset
 * ids are expected to be reset and reinitialized from the extension manifest.json properties when the
 * add-on is being updated (either downgraded or upgraded).
 *
 * In case of unexpected data schema downgrades (which may be hit if the user explicit pass --allow-downgrade
 * while using an older browser version than the one used when the data has been stored), the entire stored
 * data is reset and re-initialized from scratch based on the manifest.json file.
 */
class RulesetsStore {
  constructor() {
    // Map<extensionUUID, StoreData>
    this._data = new Map();
    // Map<extensionUUID, Promise<StoreData>>
    this._dataPromises = new Map();
    // Map<extensionUUID, Promise<void>>
    this._savePromises = new Map();
    // Map<extensionUUID, Queue>
    this._dataUpdateQueues = new DefaultMap(() => new Queue());
    // Map<extensionUUID, { close: Function }>
    this._shutdownHandlers = new Map();
    // Promise to await on to ensure the store parent directory exist
    // (the parent directory is shared by all extensions and so we only need one).
    this._ensureStoreDirectoryPromise = null;
  }

  /**
   * Remove store file for the given extension UUId from disk (used to remove all
   * data on addon uninstall).
   *
   * @param {string} extensionUUID
   * @returns {Promise<void>}
   */
  async clearOnUninstall(extensionUUID) {
    const storeFile = this.#getStoreFilePath(extensionUUID);

    // TODO(Bug 1803363): consider collect telemetry on DNR store file removal errors.
    // TODO: consider catch and report unexpected errors
    await IOUtils.remove(storeFile, { ignoreAbsent: true });
  }

  /**
   * Update DNR RulesetManager rules to match the current DNR rules enabled in the DNRStore.
   *
   * @param {Extension} extension
   * @param {object}    [params]
   * @param {boolean}   [params.isInstallOrUpdate=false]
   * @param {boolean}   [params.updateStaticRulesets=true]
   *
   * @returns {Promise<void>}
   */
  async updateRulesetManager(
    extension,
    { isInstallOrUpdate = false, updateStaticRulesets = true } = {}
  ) {
    if (!updateStaticRulesets && !isInstallOrUpdate) {
      return;
    }

    let staticRulesetsMap = await this.getEnabledStaticRulesets(extension);

    // Convert into array and ensure order match the order of the rulesets in
    // the extension manifest.
    const enabledStaticRules = [];
    // Order the static rulesets by index of rule_resources in manifest.json.
    const orderedRulesets = Array.from(staticRulesetsMap.entries()).sort(
      ([_idA, rsA], [_idB, rsB]) => rsA.idx - rsB.idx
    );
    for (const [rulesetId, ruleset] of orderedRulesets) {
      enabledStaticRules.push({ id: rulesetId, rules: ruleset.rules });
    }
    const ruleManager = lazy.ExtensionDNR.getRuleManager(extension);
    ruleManager.setEnabledStaticRulesets(enabledStaticRules);
  }

  /**
   * Load (or initialize) the store file data for the given extension and
   * return a Map of the enabled static rulesets and their related rules.
   *
   * - if the extension manifest doesn't have any static rulesets declared in the
   *   manifest, returns null
   *
   * - if the extension version from the stored data doesn't match the current
   *   extension versions, the static rules are being reloaded from the manifest.
   *
   * @param {Extension} extension
   * @param {boolean}   [isInstallOrUpdate=false]
   *                    A flag to determine if the enabled static rulesets persisted
   *                    should be reset and reinitialized from the manifest.json file.
   *
   * @returns {Promise<Map<ruleset_id, object>|void>}
   */
  async getEnabledStaticRulesets(extension, isInstallOrUpdate = false) {
    let data = await this.#getDataPromise(extension, isInstallOrUpdate);
    return data?.staticRulesets;
  }

  async getAvailableStaticRuleCount(extension) {
    const { GUARANTEED_MINIMUM_STATIC_RULES } = lazy.ExtensionDNR.limits;

    const ruleResources =
      extension.manifest.declarative_net_request?.rule_resources;
    // TODO: return maximum rules count when no static rules is listed in the manifest?
    if (!Array.isArray(ruleResources)) {
      return GUARANTEED_MINIMUM_STATIC_RULES;
    }

    const enabledRulesets = await this.getEnabledStaticRulesets(extension);
    const enabledRulesCount = Array.from(enabledRulesets.values()).reduce(
      (acc, ruleset) => acc + ruleset.rules.length,
      0
    );

    return GUARANTEED_MINIMUM_STATIC_RULES - enabledRulesCount;
  }

  /**
   * Update the enabled rulesets, queue changes to prevent races between calls
   * that may be triggered while an update is still in process.
   *
   * @param {Extension}     extension
   * @param {object}        params
   * @param {Array<string>} [params.disableRulesetIds=[]]
   * @param {Array<string>} [params.enableRulesetIds=[]]
   */
  async updateEnabledStaticRulesets(
    extension,
    { disableRulesetIds, enableRulesetIds }
  ) {
    return this._dataUpdateQueues.get(extension.uuid).queueTask(() => {
      return this.#updateEnabledStaticRulesets(extension, {
        disableRulesetIds,
        enableRulesetIds,
      });
    });
  }

  /**
   * Return the store file path for the given the extension's uuid.
   *
   * @param {string} extensionUUID
   * @returns {{ storeFile: string}}
   *          An object including the full paths to the storeFile for the extension.
   */
  getFilePaths(extensionUUID) {
    return {
      storeFile: this.#getStoreFilePath(extensionUUID),
    };
  }

  /**
   * Save the data for the given extension on disk.
   *
   * @param {Extension} extension
   */
  async save(extension) {
    const { uuid, id } = extension;
    let savePromise = this._savePromises.get(uuid);

    if (!savePromise) {
      savePromise = this.#saveNow(uuid);
      this._savePromises.set(uuid, savePromise);
      IOUtils.profileBeforeChange.addBlocker(
        `Flush WebExtension DNR RulesetsStore: ${id}`,
        savePromise
      );
    }

    return savePromise;
  }

  /**
   * Register an onClose shutdown handler to cleanup the data from memory when
   * the extension is shutting down.
   *
   * @param {Extension} extension
   * @returns {void}
   */
  unloadOnShutdown(extension) {
    const extensionUUID = extension.uuid;
    let shutdownHandler = this._shutdownHandlers.get(extensionUUID);
    if (!shutdownHandler) {
      shutdownHandler = {
        close: async () => {
          // Wait for the update tasks to have been executed, then unload the
          // data.
          const dataUpdateQueue = this._dataUpdateQueues.has(extensionUUID)
            ? this._dataUpdateQueues.get(extensionUUID)
            : undefined;
          if (dataUpdateQueue) {
            try {
              await dataUpdateQueue.close();
            } catch (err) {
              // Unexpected error on closing the update queue.
              Cu.reportError(err);
            }
            this._dataUpdateQueues.delete(extensionUUID);
          }
          this.unloadData(extensionUUID);
        },
      };
      this._shutdownHandlers.set(extensionUUID, shutdownHandler);
    }
    extension.callOnClose(shutdownHandler);
  }

  /**
   * Unload data for the given extension UUID from memory (e.g. when the extension is disabled or uninstalled),
   * waits for a pending save promise to be settled if any.
   *
   * @param {string} extensionUUID
   * @returns {Promise<void>}
   */
  async unloadData(extensionUUID) {
    const savePromise = this._savePromises.get(extensionUUID);
    if (savePromise) {
      await savePromise;
      this._savePromises.delete(extensionUUID);
    }

    this._dataPromises.delete(extensionUUID);
    this._data.delete(extensionUUID);
  }

  /**
   * Return a branch new StoreData instance given an extension.
   *
   * @param {Extension} extension
   * @returns {StoreData}
   */
  #getDefaults(extension) {
    return new StoreData({
      extVersion: extension.version,
    });
  }

  /**
   * Return the path to the store file given the extension's uuid.
   *
   * @param {string} extensionUUID
   * @returns {string} Full path to the store file for the extension.
   */
  #getStoreFilePath(extensionUUID) {
    return PathUtils.join(
      Services.dirsvc.get("ProfD", Ci.nsIFile).path,
      RULES_STORE_DIRNAME,
      `${extensionUUID}${RULES_STORE_FILEEXT}`
    );
  }

  #ensureStoreDirectory(extensionUUID) {
    // Currently all extensions share the same directory, so we can re-use this promise across all
    // `#ensureStoreDirectory` calls.
    if (this._ensureStoreDirectoryPromise === null) {
      const file = this.#getStoreFilePath(extensionUUID);
      this._ensureStoreDirectoryPromise = IOUtils.makeDirectory(
        PathUtils.parent(file),
        {
          ignoreExisting: true,
          createAncestors: true,
        }
      );
    }
    return this._ensureStoreDirectoryPromise;
  }

  async #getDataPromise(extension, isInstallOrUpdate = false) {
    if (isInstallOrUpdate) {
      if (this._savePromises.has(extension.uuid)) {
        Cu.reportError(
          `Unexpected pending save task while reading DNR data after an install/update of extension "${extension.id}"`
        );
      }
      // await pending saving data to be saved and unloaded.
      await this.unloadData(extension.uuid);
    }
    let dataPromise = this._dataPromises.get(extension.uuid);
    if (!dataPromise) {
      dataPromise = this.#readData(extension, isInstallOrUpdate);
      this._dataPromises.set(extension.uuid, dataPromise);
    }
    return dataPromise;
  }

  /**
   * Reads the store file for the given extensions and all rules
   * for the enabled static ruleset ids listed in the store file.
   *
   * @param {Extension} extension
   * @param {Array<string>} [enabledRulesetIds]
   *        An optional array of enabled ruleset ids to be loaded
   *        (used to load a specific group of static rulesets,
   *        either when the list of static rules needs to be recreated based
   *        on the enabled rulesets, or when the extension is
   *        changing the enabled rulesets using the `updateEnabledRulesets`
   *        API method).
   * @returns {Promise<Map<ruleset_id, object>> | void}
   */
  async #getManifestStaticRulesets(
    extension,
    {
      enabledRulesetIds = null,
      availableStaticRuleCount = lazy.ExtensionDNR.limits
        .GUARANTEED_MINIMUM_STATIC_RULES,
      isUpdateEnabledRulesets = false,
    } = {}
  ) {
    const ruleResources =
      extension.manifest.declarative_net_request?.rule_resources;
    if (!Array.isArray(ruleResources)) {
      return null;
    }

    // Map<ruleset_id, { enabled, rules }>}
    const rulesets = new Map();

    const {
      MAX_NUMBER_OF_ENABLED_STATIC_RULESETS,
      // Warnings on MAX_NUMBER_OF_STATIC_RULESETS are already
      // reported (see ExtensionDNR.validateManifestEntry, called
      // from the DNR API onManifestEntry callback).
    } = lazy.ExtensionDNR.limits;

    for (let [idx, { id, enabled, path }] of ruleResources.entries()) {
      // Retrieve the file path from the normalized path.
      path = Services.io.newURI(path).filePath;

      // If passed enabledRulesetIds is used to determine if the enabled
      // rules in the manifest should be overridden from the list of
      // enabled static rulesets stored on disk.
      if (Array.isArray(enabledRulesetIds)) {
        enabled = enabledRulesetIds.includes(id);
      }

      // Duplicated ruleset ids are validated as part of the JSONSchema validation,
      // here we log a warning to signal that we are ignoring it if when the validation
      // error isn't strict (e.g. for non temporarily installed, which shouldn't normally
      // hit in the long run because we can also validate it before signing the extension).
      if (rulesets.has(id)) {
        Cu.reportError(
          `Disabled static ruleset with duplicated ruleset_id "${id}"`
        );
        continue;
      }

      if (enabled && rulesets.size >= MAX_NUMBER_OF_ENABLED_STATIC_RULESETS) {
        // This is technically reported from the manifest validation, as a warning
        // on extension installed non temporarily, and so checked and logged here
        // in case we are hitting it while loading the enabled rulesets.
        Cu.reportError(
          `Ignoring enabled static ruleset exceeding the MAX_NUMBER_OF_ENABLED_STATIC_RULESETS limit (${MAX_NUMBER_OF_ENABLED_STATIC_RULESETS}): ruleset_id "${id}" (extension: "${extension.id}")`
        );
        continue;
      }

      const rawRules =
        enabled &&
        (await extension.readJSON(path).catch(err => {
          Cu.reportError(err);
          enabled = false;
          extension.packagingError(
            `Reading declarative_net_request static rules file ${path}: ${err.message}`
          );
        }));

      // Skip rulesets that are not enabled or can't be enabled (e.g. if we got error on loading or
      // parsing the rules JSON file).
      if (!enabled) {
        continue;
      }

      if (!Array.isArray(rawRules)) {
        extension.packagingError(
          `Reading declarative_net_request static rules file ${path}: rules file must contain an Array of rules`
        );
        continue;
      }

      // Normalize rules read from JSON.
      const validationContext = {
        url: extension.baseURI.spec,
        principal: extension.principal,
        logError: error => {
          extension.packagingWarning(error);
        },
        preprocessors: {},
        manifestVersion: extension.manifestVersion,
      };

      // TODO(Bug 1803369): consider to only report the errors and warnings about invalid static rules for
      // temporarily installed extensions (chrome only shows them for unpacked extensions).
      // TODO(Bug 1803369): consider to also include the rule id if one was available.
      const getInvalidRuleMessage = (ruleIndex, msg) =>
        `Invalid rule at index ${ruleIndex} from static ruleset_id "${id}", ${msg}`;

      const ruleValidator = new lazy.ExtensionDNR.RuleValidator([]);

      for (const [rawIndex, rawRule] of rawRules.entries()) {
        try {
          const normalizedRule = lazy.Schemas.normalize(
            rawRule,
            "declarativeNetRequest.Rule",
            validationContext
          );
          if (normalizedRule.value) {
            ruleValidator.addRules([normalizedRule.value]);
          } else {
            extension.packagingWarning(
              getInvalidRuleMessage(
                rawIndex,
                normalizedRule.error ?? "Unexpected undefined rule"
              )
            );
          }
        } catch (err) {
          Cu.reportError(err);
          extension.packagingWarning(
            getInvalidRuleMessage(rawIndex, "An unexpected error occurred")
          );
        }
      }

      // TODO(Bug 1803369): consider including an index in the invalid rules warnings.
      if (ruleValidator.getFailures().length) {
        extension.packagingError(
          `Reading declarative_net_request static rules file ${path}: ${ruleValidator
            .getFailures()
            .map(f => f.message)
            .join(", ")}`
        );
      }

      const validatedRules = ruleValidator.getValidatedRules();

      // NOTE: this is currently only accounting for valid rules because
      // only the valid rules will be actually be loaded. Reconsider if
      // we should instead also account for the rules that have been
      // ignored as invalid.
      if (availableStaticRuleCount - validatedRules.length < 0) {
        if (isUpdateEnabledRulesets) {
          throw new ExtensionError(
            "updateEnabledRulesets request is exceeding the available static rule count"
          );
        }

        // TODO(Bug 1803363): consider collect telemetry.
        Cu.reportError(
          `Ignoring static ruleset exceeding the available static rule count: ruleset_id "${id}" (extension: "${extension.id}")`
        );
        // TODO: currently ignoring the current ruleset but would load the one that follows if it
        // fits in the available rule count when loading the rule on extension startup,
        // should it stop loading additional rules instead?
        continue;
      }
      availableStaticRuleCount -= validatedRules.length;

      rulesets.set(id, { idx, rules: validatedRules });
    }

    return rulesets;
  }

  /**
   * Read the stored data for the given extension, either from:
   * - memory, if already loaded
   * - store file (if available and not detected as a data schema downgrade)
   * - manifest file and packaged ruleset JSON files (if there was no valid stored data found)
   *
   * @param {Extension} extension
   * @param {boolean}   [isInstallOrUpdate=false]
   *                    A flag to determine if the enabled static rulesets persisted
   *                    should be reset and reinitialized from the manifest.json file.
   *
   * @returns {Promise<StoreData>}
   */
  async #readData(extension, isInstallOrUpdate = false) {
    // Return early if the data was already loaded in memory.
    if (!isInstallOrUpdate && this._dataPromises.has(extension.uuid)) {
      return this._dataPromises.get(extension.uuid);
    }

    // Try to load the data stored in the json file.
    let result = await this.#readStoreData(extension, isInstallOrUpdate);

    // Reset the stored data if a data schema version downgrade has been
    // detected (this should only be hit on downgrades if the user have
    // also explicitly passed --allow-downgrade CLI option).
    if (result && result.version > StoreData.VERSION) {
      Cu.reportError(
        `Unsupport DNR store schema version downgrade: resetting stored data for ${extension.id}`
      );
      result = null;
    }

    // Use defaults and extension manifest if no data stored was found
    // (or it got reset due to an unsupported profile downgrade being detected).
    if (!result) {
      // We don't have any data stored, load the static rules from the manifest.
      result = this.#getDefaults(extension);
      // Initialize the staticRules data from the manifest.
      result.staticRulesets = await this.#getManifestStaticRulesets(extension);
    }

    // TODO: handle DNR store schema changes here when the StoreData.VERSION is being bumped.
    // if (result && result.version < StoreData.VERSION) {
    //   result = this.upgradeStoreDataSchema(result);
    // }

    this._data.set(extension.uuid, result);

    return result;
  }

  /**
   * Reads the store file for the given extensions and all rules
   * for the enabled static ruleset ids listed in the store file.
   *
   * @param {Extension} extension
   * @param {boolean}   [isInstallOrUpdate=false]
   *                    A flag to determine if the enabled static rulesets persisted
   *                    should be reset and reinitialized from the manifest.json file.
   *
   * @returns {Promise<StoreData|void>}
   */
  async #readStoreData(extension, isInstallOrUpdate) {
    // TODO(Bug 1803363): record into Glean telemetry DNR RulesetsStore store load time.
    try {
      let file = this.#getStoreFilePath(extension.uuid);
      let data = await IOUtils.readJSON(file, { decompress: true });
      // Ignore the stored enabled ruleset ids if the current extension version
      // mismatches the version the store data was generated from or the data is
      // being read as part of install or updating the extension.
      if (isInstallOrUpdate || data.extVersion !== extension.version) {
        data.staticRulesets = null;
        data.extVersion = extension.version;
      }
      // In the JSON stored data we only store the enabled rulestore_id and
      // the actual rules have to be loaded.
      data.staticRulesets = await this.#getManifestStaticRulesets(
        extension,
        // Only load the rules from rulesets that are enabled in the stored DNR data,
        // if the array (eventually empty) of the enabled static rules isn't in the
        // stored data, then load all the ones enabled in the manifest.
        {
          enabledRulesetIds: Array.isArray(data.staticRulesets)
            ? data.staticRulesets
            : null,
        }
      );
      return new StoreData(data);
    } catch (e) {
      if (!DOMException.isInstance(e) || e.name !== "NotFoundError") {
        Cu.reportError(e);
      }
      // TODO(Bug 1803363) record store read errors in telemetry scalar.
    }
    return null;
  }

  /**
   * Save the data for the given extension on disk.
   *
   * @param {string} extensionUUID
   * @returns {Promise<void>}
   */
  async #saveNow(extensionUUID) {
    try {
      if (!this._dataPromises.has(extensionUUID)) {
        throw new Error(
          `Unexpected uninitialized DNR store on saving data for extension uuid "${extensionUUID}"`
        );
      }
      const storeFile = this.#getStoreFilePath(extensionUUID);

      const data = this._data.get(extensionUUID);
      if (data.isEmpty) {
        await IOUtils.remove(storeFile, { ignoreAbsent: true });
        return;
      }
      await this.#ensureStoreDirectory(extensionUUID);
      await IOUtils.writeJSON(storeFile, data, {
        tmpPath: `${storeFile}.tmp`,
        compress: true,
      });
      // TODO(Bug 1803363): report jsonData lengths into a telemetry scalar.
      // TODO(Bug 1803363): report jsonData time to write into a telemetry scalar.
    } catch (err) {
      Cu.reportError(err);
      throw err;
    } finally {
      this._savePromises.delete(extensionUUID);
    }
  }

  /**
   * Internal implementation for updating the enabled rulesets and enforcing
   * static rulesets and rules count limits.
   *
   * @param {Extension}     extension
   * @param {object}        params
   * @param {Array<string>} [params.disableRulesetIds=[]]
   * @param {Array<string>} [params.enableRulesetIds=[]]
   */
  async #updateEnabledStaticRulesets(
    extension,
    { disableRulesetIds, enableRulesetIds }
  ) {
    const ruleResources =
      extension.manifest.declarative_net_request?.rule_resources;
    if (!Array.isArray(ruleResources)) {
      return;
    }

    const enabledRulesets = await this.getEnabledStaticRulesets(extension);
    const updatedEnabledRulesets = new Map();
    let disableIds = new Set(disableRulesetIds);
    let enableIds = new Set(enableRulesetIds);

    // valiate the ruleset ids for existence (which will also reject calls
    // including the reserved _session and _dynamic, because static rulesets
    // id are validated as part of the manifest validation and they are not
    // allowed to start with '_').
    const existingIds = new Set(ruleResources.map(rs => rs.id));
    const errorOnInvalidRulesetIds = rsIdSet => {
      for (const rsId of rsIdSet) {
        if (!existingIds.has(rsId)) {
          throw new ExtensionError(`Invalid ruleset id: "${rsId}"`);
        }
      }
    };
    errorOnInvalidRulesetIds(disableIds);
    errorOnInvalidRulesetIds(enableIds);

    // Copy into the updatedEnabledRulesets Map any ruleset that is not
    // requested to be disabled or is enabled back in the same request.
    for (const [rulesetId, ruleset] of enabledRulesets) {
      if (!disableIds.has(rulesetId) || enableIds.has(rulesetId)) {
        updatedEnabledRulesets.set(rulesetId, ruleset);
        enableIds.delete(rulesetId);
      }
    }

    const {
      MAX_NUMBER_OF_ENABLED_STATIC_RULESETS,
      GUARANTEED_MINIMUM_STATIC_RULES,
    } = lazy.ExtensionDNR.limits;

    const maxNewRulesetsCount =
      MAX_NUMBER_OF_ENABLED_STATIC_RULESETS - updatedEnabledRulesets.size;

    if (enableIds.size > maxNewRulesetsCount) {
      // Log an error for the developer.
      throw new ExtensionError(
        `updatedEnabledRulesets request is exceeding MAX_NUMBER_OF_ENABLED_STATIC_RULESETS`
      );
    }

    const availableStaticRuleCount =
      GUARANTEED_MINIMUM_STATIC_RULES -
      Array.from(updatedEnabledRulesets.values()).reduce(
        (acc, ruleset) => acc + ruleset.rules.length,
        0
      );

    const newRulesets = await this.#getManifestStaticRulesets(extension, {
      enabledRulesetIds: Array.from(enableIds),
      availableStaticRuleCount,
      isUpdateEnabledRulesets: true,
    });

    for (const [rulesetId, ruleset] of newRulesets.entries()) {
      updatedEnabledRulesets.set(rulesetId, ruleset);
    }

    this._data.get(extension.uuid).staticRulesets = updatedEnabledRulesets;
    await this.save(extension);
    await this.updateRulesetManager(extension);
  }
}

const store = new RulesetsStore();

/**
 * Load and add the DNR stored rules to the RuleManager instance for the given
 * extension.
 *
 * @param {Extension} extension
 * @returns {Promise<void>}
 */

async function initExtension(extension) {
  // - on new installs the stored rules should be recreated from scratch
  //   (and any stale previously stored data to be ignored)
  // - on upgrades/downgrades:
  //   - the dynamic rules are expected to be preserved
  //   - the static rules are expected to be refreshed from the new
  //     manifest data (also the enabled rulesets are expected to be
  //     reset to the state described in the manifest)
  const { startupReason } = extension;
  // TODO(Bug 1803369): consider also setting to true if the extension is installed temporarily.
  let isInstallOrUpdate = false;

  switch (startupReason) {
    case "ADDON_INSTALL":
    case "ADDON_UPGRADE":
    case "ADDON_DOWNGRADE": {
      // Reset the stored static rules on addon updates.
      await StartupCache.delete(extension, "dnr", "hasEnabledStaticRules");
      // Also pass it to getEnabledStaticRulesets to re-initialize the stored
      // static ruleset ids from the manifest.json data.
      isInstallOrUpdate = true;
    }
  }
  const hasEnabledStaticRules = await StartupCache.get(
    extension,
    ["dnr", "hasEnabledStaticRules"],
    async () => {
      const staticRulesets = await store.getEnabledStaticRulesets(
        extension,
        isInstallOrUpdate
      );

      return staticRulesets?.size;
    }
  );

  if (hasEnabledStaticRules) {
    await store.updateRulesetManager(extension, { isInstallOrUpdate });
  }

  if (extension.hasShutdown) {
    await store.unloadData(extension.uuid);
    return;
  }

  store.unloadOnShutdown(extension);
}

const requireTestOnlyCallers = () => {
  if (!Services.env.exists("XPCSHELL_TEST_PROFILE_DIR")) {
    throw new Error("This should only be called from XPCShell tests");
  }
};

export const ExtensionDNRStore = {
  async clearOnUninstall(extensionUUID) {
    return store.clearOnUninstall(extensionUUID);
  },
  initExtension,
  async updateEnabledStaticRulesets(extension, updateRulesetOptions) {
    await store.updateEnabledStaticRulesets(extension, updateRulesetOptions);
  },
  // Test-only helpers
  _getStoreForTesting() {
    requireTestOnlyCallers();
    return store;
  },
};
