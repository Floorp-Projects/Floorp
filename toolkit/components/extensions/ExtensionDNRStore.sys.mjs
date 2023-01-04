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

/**
 * Internal representation of the enabled static rulesets (used in StoreData
 * and Store methods type signatures).
 *
 * @typedef {object} EnabledStaticRuleset
 * @inner
 * @property {number} idx
 *           Represent the position of the static ruleset in the manifest
 *           `declarative_net_request.rule_resources` array.
 * @property {Array<Rule>} rules
 *           Represent the array of the DNR rules associated with the static
 *           ruleset.
 */

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
   * @param {Map<string, EnabledStaticRuleset>} [params.staticRulesets=new Map()]
   *        map of the enabled static rulesets by ruleset_id, as resolved by
   *        `Store.prototype.#getManifestStaticRulesets`.
   *        NOTE: This map is converted in an array of the ruleset_id strings when the StoreData
   *        instance is being stored on disk (see `toJSON` method) and then converted back to a Map
   *        by `Store.prototype.#getManifestStaticRulesets` when the data is loaded back from disk.
   * @param {Array<Rule>} [params.dynamicRuleset=[]]
   *        array of dynamic rules stored by the extension.
   */
  constructor({
    schemaVersion,
    extVersion,
    staticRulesets,
    dynamicRuleset,
  } = {}) {
    this.schemaVersion = schemaVersion || this.constructor.VERSION;
    this.extVersion = extVersion ?? null;
    this.setStaticRulesets(staticRulesets);
    this.setDynamicRuleset(dynamicRuleset);
  }

  get isEmpty() {
    return !this.staticRulesets.size && !this.dynamicRuleset.length;
  }

  setStaticRulesets(updatedStaticRulesets = new Map()) {
    this.staticRulesets = updatedStaticRulesets;
  }

  setDynamicRuleset(updatedDynamicRuleset = []) {
    this.dynamicRuleset = updatedDynamicRuleset;
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
      dynamicRuleset: this.dynamicRuleset,
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
      const lastTask = this.#tasks[this.#tasks.length - 1];
      return lastTask?.deferred.promise;
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
   * Load (or initialize) the store file data for the given extension and
   * return an Array of the dynamic rules.
   *
   * @param {Extension} extension
   *
   * @returns {Promise<Array<Rule>>}
   *          Resolve to a reference to the dynamic rules array.
   *          NOTE: the caller should never mutate the content of this array,
   *          updates to the dynamic rules should always go through
   *          the `updateDynamicRules` method.
   */
  async getDynamicRules(extension) {
    let data = await this.#getDataPromise(extension);
    return data.dynamicRuleset;
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
   *
   * @returns {Promise<Map<ruleset_id, EnabledStaticRuleset>>}
   *          Resolves to a reference to the static rulesets map.
   *          NOTE: the caller should never mutate the content of this map,
   *          updates to the enabled static rulesets should always go through
   *          the `updateEnabledStaticRulesets` method.
   */
  async getEnabledStaticRulesets(extension) {
    let data = await this.#getDataPromise(extension);
    return data.staticRulesets;
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
   * Initialize the DNR store for the given extension, it does also queue the task to make
   * sure that extension DNR API calls triggered while the initialization may still be
   * in progress will be executed sequentially.
   *
   * @param {Extension}     extension
   *
   * @returns {Promise<void>} A promise resolved when the async initialization has been
   *                          completed.
   */
  async initExtension(extension) {
    const ensureExtensionRunning = () => {
      if (extension.hasShutdown) {
        throw new Error(
          `DNR store initialization abort, extension is already shutting down: ${extension.id}`
        );
      }
    };

    // Make sure we wait for pending save promise to have been
    // completed and old data unloaded (this may be hit if an
    // extension updates or reloads while there are still
    // rules updates being processed and then stored on disk).
    ensureExtensionRunning();
    if (this._savePromises.has(extension.uuid)) {
      Cu.reportError(
        `Unexpected pending save task while reading DNR data after an install/update of extension "${extension.id}"`
      );
      // await pending saving data to be saved and unloaded.
      await this.#unloadData(extension.uuid);
      // Make sure the extension is still running after awaiting on
      // unloadData to be completed.
      ensureExtensionRunning();
    }

    return this._dataUpdateQueues.get(extension.uuid).queueTask(() => {
      return this.#initExtension(extension);
    });
  }

  /**
   * Update the dynamic rules, queue changes to prevent races between calls
   * that may be triggered while an update is still in process.
   *
   * @param {Extension}     extension
   * @param {object}        params
   * @param {Array<string>} [params.removeRuleIds=[]]
   * @param {Array<Rule>} [params.addRules=[]]
   *
   * @returns {Promise<void>} A promise resolved when the dynamic rules async update has
   *                          been completed.
   */
  async updateDynamicRules(extension, { removeRuleIds, addRules }) {
    return this._dataUpdateQueues.get(extension.uuid).queueTask(() => {
      return this.#updateDynamicRules(extension, {
        removeRuleIds,
        addRules,
      });
    });
  }

  /**
   * Update the enabled rulesets, queue changes to prevent races between calls
   * that may be triggered while an update is still in process.
   *
   * @param {Extension}     extension
   * @param {object}        params
   * @param {Array<string>} [params.disableRulesetIds=[]]
   * @param {Array<string>} [params.enableRulesetIds=[]]
   *
   * @returns {Promise<void>} A promise resolved when the enabled static rulesets async
   *                          update has been completed.
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
   * Update DNR RulesetManager rules to match the current DNR rules enabled in the DNRStore.
   *
   * @param {Extension} extension
   * @param {object}    [params]
   * @param {boolean}   [params.updateStaticRulesets=true]
   * @param {boolean}   [params.updateDynamicRuleset=true]
   */
  updateRulesetManager(
    extension,
    { updateStaticRulesets = true, updateDynamicRuleset = true } = {}
  ) {
    if (!updateStaticRulesets && !updateDynamicRuleset) {
      return;
    }

    if (
      !this._dataPromises.has(extension.uuid) ||
      !this._data.has(extension.uuid)
    ) {
      throw new Error(
        `Unexpected call to updateRulesetManager before DNR store was fully initialized for extension "${extension.id}"`
      );
    }
    const data = this._data.get(extension.uuid);
    const ruleManager = lazy.ExtensionDNR.getRuleManager(extension);

    if (updateStaticRulesets) {
      let staticRulesetsMap = data.staticRulesets;
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
      ruleManager.setEnabledStaticRulesets(enabledStaticRules);
    }

    if (updateDynamicRuleset) {
      ruleManager.setDynamicRules(data.dynamicRuleset);
    }
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
    if (extension.hasShutdown) {
      throw new Error(
        `DNR store registering an extension shutdown handler too late, the extension is already shutting down: ${extension.id}`
      );
    }

    const extensionUUID = extension.uuid;
    extension.callOnClose({
      close: async () => this.#unloadData(extensionUUID),
    });
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

  async #getDataPromise(extension) {
    let dataPromise = this._dataPromises.get(extension.uuid);
    if (!dataPromise) {
      if (extension.hasShutdown) {
        throw new Error(
          `DNR store data loading aborted, the extension is already shutting down: ${extension.id}`
        );
      }

      this.unloadOnShutdown(extension);
      dataPromise = this.#readData(extension);
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
   * @returns {Promise<Map<ruleset_id, EnabledStaticRuleset>>}
   *          map of the enabled static rulesets by ruleset_id.
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
    // Map<ruleset_id, EnabledStaticRuleset>}
    const rulesets = new Map();

    const ruleResources =
      extension.manifest.declarative_net_request?.rule_resources;
    if (!Array.isArray(ruleResources)) {
      return rulesets;
    }

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

      // TODO(Bug 1803369): consider to only report the errors and warnings about invalid static rules for
      // temporarily installed extensions (chrome only shows them for unpacked extensions).
      const logRuleValidationError = err => extension.packagingWarning(err);

      const validatedRules = this.#getValidatedRules(extension, id, rawRules, {
        logRuleValidationError,
      });

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
   * Returns an array of validated and normalized Rule instances given an array
   * of raw rules data (e.g. in form of plain objects read from the static rules
   * JSON files or the dynamicRuleset property from the extension DNR store data).
   *
   * @param   {Extension}     extension
   * @param   {string}        rulesetId
   * @param   {Array<object>} rawRules
   * @param   {object}        options
   * @param   {Function}      [options.logRuleValidationError]
   *                          an optional callback to call for logging the
   *                          validation errors, defaults to use Cu.reportError
   *                          (but getManifestStaticRulesets overrides it to use
   *                          extensions.packagingWarning instead).
   *
   * @returns {Array<Rule>}
   */
  #getValidatedRules(
    extension,
    rulesetId,
    rawRules,
    { logRuleValidationError = err => Cu.reportError(err) } = {}
  ) {
    const ruleValidator = new lazy.ExtensionDNR.RuleValidator([]);
    // Normalize rules read from JSON.
    const validationContext = {
      url: extension.baseURI.spec,
      principal: extension.principal,
      logError: logRuleValidationError,
      preprocessors: {},
      manifestVersion: extension.manifestVersion,
    };

    // TODO(Bug 1803369): consider to also include the rule id if one was available.
    const getInvalidRuleMessage = (ruleIndex, msg) =>
      `Invalid rule at index ${ruleIndex} from ruleset "${rulesetId}", ${msg}`;

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
          logRuleValidationError(
            getInvalidRuleMessage(
              rawIndex,
              normalizedRule.error ?? "Unexpected undefined rule"
            )
          );
        }
      } catch (err) {
        logRuleValidationError(
          getInvalidRuleMessage(rawIndex, "An unexpected error occurred")
        );
      }
    }

    // TODO(Bug 1803369): consider including an index in the invalid rules warnings.
    if (ruleValidator.getFailures().length) {
      logRuleValidationError(
        `Invalid rules found in ruleset "${rulesetId}": ${ruleValidator
          .getFailures()
          .map(f => f.message)
          .join(", ")}`
      );
    }

    return ruleValidator.getValidatedRules();
  }

  #hasInstallOrUpdateStartupReason(extension) {
    switch (extension.startupReason) {
      case "ADDON_INSTALL":
      case "ADDON_UPGRADE":
      case "ADDON_DOWNGRADE":
        return true;
    }

    return false;
  }

  /**
   * Load and add the DNR stored rules to the RuleManager instance for the given
   * extension.
   *
   * @param {Extension} extension
   * @returns {Promise<void>}
   */
  async #initExtension(extension) {
    // - on new installs the stored rules should be recreated from scratch
    //   (and any stale previously stored data to be ignored)
    // - on upgrades/downgrades:
    //   - the dynamic rules are expected to be preserved
    //   - the static rules are expected to be refreshed from the new
    //     manifest data (also the enabled rulesets are expected to be
    //     reset to the state described in the manifest)
    //
    // TODO(Bug 1803369): consider also setting to true if the extension is installed temporarily.
    if (this.#hasInstallOrUpdateStartupReason(extension)) {
      // Reset the stored static rules on addon updates.
      await StartupCache.delete(extension, "dnr", "hasEnabledStaticRules");
    }

    const hasEnabledStaticRules = await StartupCache.get(
      extension,
      ["dnr", "hasEnabledStaticRules"],
      async () => {
        const staticRulesets = await this.getEnabledStaticRulesets(extension);

        return staticRulesets.size;
      }
    );
    const hasDynamicRules = await StartupCache.get(
      extension,
      ["dnr", "hasDynamicRules"],
      async () => {
        const dynamicRuleset = await this.getDynamicRules(extension);

        return dynamicRuleset.length;
      }
    );

    if (hasEnabledStaticRules || hasDynamicRules) {
      await this.#getDataPromise(extension);
      this.updateRulesetManager(extension, {
        updateStaticRulesets: hasEnabledStaticRules,
        updateDynamicRuleset: hasDynamicRules,
      });
    }
  }

  /**
   * Read the stored data for the given extension, either from:
   * - store file (if available and not detected as a data schema downgrade)
   * - manifest file and packaged ruleset JSON files (if there was no valid stored data found)
   *
   * This private method is only called from #getDataPromise, which caches the return value
   * in memory.
   *
   * @param {Extension} extension
   *
   * @returns {Promise<StoreData>}
   */
  async #readData(extension) {
    // Try to load the data stored in the json file.
    let result = await this.#readStoreData(extension);

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
      result.setStaticRulesets(
        await this.#getManifestStaticRulesets(extension)
      );
    }

    // TODO: handle DNR store schema changes here when the StoreData.VERSION is being bumped.
    // if (result && result.version < StoreData.VERSION) {
    //   result = this.upgradeStoreDataSchema(result);
    // }

    // The extension has already shutting down and we may already got past
    // the unloadData cleanup (given that there is still a promise in
    // the _dataPromises Map).
    if (extension.hasShutdown && !this._dataPromises.has(extension.uuid)) {
      throw new Error(
        `DNR store data loading aborted, the extension is already shutting down: ${extension.id}`
      );
    }

    this._data.set(extension.uuid, result);

    return result;
  }

  /**
   * Reads the store file for the given extensions and all rules
   * for the enabled static ruleset ids listed in the store file.
   *
   * @param {Extension} extension
   *
   * @returns {Promise<StoreData|void>}
   */
  async #readStoreData(extension) {
    // TODO(Bug 1803363): record into Glean telemetry DNR RulesetsStore store load time.
    let file = this.#getStoreFilePath(extension.uuid);
    let data;
    let isCorrupted = false;
    let storeFileFound = false;
    try {
      data = await IOUtils.readJSON(file, { decompress: true });
      storeFileFound = true;
    } catch (e) {
      if (!(DOMException.isInstance(e) && e.name === "NotFoundError")) {
        Cu.reportError(e);
        isCorrupted = true;
        storeFileFound = true;
      }
      // TODO(Bug 1803363) record store read errors in telemetry scalar.
    }

    // Reset data read from disk if its type isn't the expected one.
    isCorrupted ||=
      !data ||
      !Array.isArray(data.staticRulesets) ||
      // DNR data stored in 109 would not have any dynamicRuleset
      // property and so don't consider the data corrupted if
      // there isn't any dynamicRuleset property at all.
      ("dynamicRuleset" in data && !Array.isArray(data.dynamicRuleset));

    if (isCorrupted && storeFileFound) {
      // Wipe the corrupted data and backup the corrupted file.
      data = null;
      try {
        let uniquePath = await IOUtils.createUniqueFile(
          PathUtils.parent(file),
          PathUtils.filename(file) + ".corrupt",
          0o600
        );
        Cu.reportError(
          `Detected corrupted DNR store data for ${extension.id}, renaming store data file to ${uniquePath}`
        );
        await IOUtils.move(file, uniquePath);
      } catch (err) {
        Cu.reportError(err);
      }
    }

    if (!data) {
      return null;
    }

    const resetStaticRulesets =
      // Reset the static rulesets on install or updating the extension.
      //
      // NOTE: this method is called only once and its return value cached in
      // memory for the entire lifetime of the extension and so we don't need
      // to store any flag to avoid resetting the static rulesets more than
      // once for the same Extension instance.
      this.#hasInstallOrUpdateStartupReason(extension) ||
      // Ignore the stored enabled ruleset ids if the current extension version
      // mismatches the version the store data was generated from.
      data.extVersion !== extension.version;

    if (resetStaticRulesets) {
      data.staticRulesets = undefined;
      data.extVersion = extension.version;
    }

    // If the data is being loaded for a new addon install, make sure to clear
    // any potential stale dynamic rules stored on disk.
    //
    // NOTE: this is expected to only be hit if there was a failure to cleanup
    // state data upon uninstall (e.g. in case the machine shutdowns or
    // Firefox crashes before we got to update the data stored on disk).
    if (extension.startupReason === "ADDON_INSTALL") {
      data.dynamicRuleset = [];
    }

    // In the JSON stored data we only store the enabled rulestore_id and
    // the actual rules have to be loaded.
    data.staticRulesets = await this.#getManifestStaticRulesets(
      extension,
      // Only load the rules from rulesets that are enabled in the stored DNR data,
      // if the array (eventually empty) of the enabled static rules isn't in the
      // stored data, then load all the ones enabled in the manifest.
      { enabledRulesetIds: data.staticRulesets }
    );

    if (data.dynamicRuleset?.length) {
      // Make sure all dynamic rules loaded from disk as validated and normalized
      // (in case they may have been tempered, but also for when we are loading
      // data stored by a different Firefox version from the one that stored the
      // data on disk, e.g. in case validation or normalization logic may have been
      // different in the two Firefox version).
      data.dynamicRuleset = this.#getValidatedRules(
        extension,
        "_dynamic" /* rulesetId */,
        data.dynamicRuleset
      );
    }
    return new StoreData(data);
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
   * Unload data for the given extension UUID from memory (e.g. when the extension is disabled or uninstalled),
   * waits for a pending save promise to be settled if any.
   *
   * NOTE: this method clear the data cached in memory and close the update queue
   * and so it should only be called from the extension shutdown handler and
   * by the initExtension method before pushing into the update queue for the
   * for the extension the initExtension task.
   *
   * @param {string} extensionUUID
   * @returns {Promise<void>}
   */
  async #unloadData(extensionUUID) {
    // Wait for the update tasks to have been executed, then
    // wait for the data to have been saved and finally unload
    // the data cached in memory.
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

    const savePromise = this._savePromises.get(extensionUUID);
    if (savePromise) {
      await savePromise;
      this._savePromises.delete(extensionUUID);
    }

    this._dataPromises.delete(extensionUUID);
    this._data.delete(extensionUUID);
  }

  /**
   * Internal implementation for updating the dynamic ruleset and enforcing
   * dynamic rules count limits.
   *
   * Callers ensure that there is never a concurrent call of #updateDynamicRules
   * for a given extension, so we can safely modify ruleManager.dynamicRules
   * from inside this method, even asynchronously.
   *
   * @param {Extension}     extension
   * @param {object}        params
   * @param {Array<string>} [params.removeRuleIds=[]]
   * @param {Array<Rule>}   [params.addRules=[]]
   */
  async #updateDynamicRules(extension, { removeRuleIds, addRules }) {
    const ruleManager = lazy.ExtensionDNR.getRuleManager(extension);
    const ruleValidator = new lazy.ExtensionDNR.RuleValidator(
      ruleManager.getDynamicRules()
    );
    if (removeRuleIds) {
      ruleValidator.removeRuleIds(removeRuleIds);
    }
    if (addRules) {
      ruleValidator.addRules(addRules);
    }
    let failures = ruleValidator.getFailures();
    if (failures.length) {
      throw new ExtensionError(failures[0].message);
    }
    this._data
      .get(extension.uuid)
      .setDynamicRuleset(ruleValidator.getValidatedRules());
    await this.save(extension);
    // updateRulesetManager calls ruleManager.setDynamicRules using the
    // validated rules assigned above to this._data.
    this.updateRulesetManager(extension, {
      updateDynamicRuleset: true,
      updateStaticRulesets: false,
    });
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

    this._data.get(extension.uuid).setStaticRulesets(updatedEnabledRulesets);
    await this.save(extension);
    this.updateRulesetManager(extension, {
      updateDynamicRuleset: false,
      updateStaticRulesets: true,
    });
  }
}

const store = new RulesetsStore();

const requireTestOnlyCallers = () => {
  if (!Services.env.exists("XPCSHELL_TEST_PROFILE_DIR")) {
    throw new Error("This should only be called from XPCShell tests");
  }
};

export const ExtensionDNRStore = {
  async clearOnUninstall(extensionUUID) {
    return store.clearOnUninstall(extensionUUID);
  },
  async initExtension(extension) {
    await store.initExtension(extension);
  },
  async updateDynamicRules(extension, updateRuleOptions) {
    await store.updateDynamicRules(extension, updateRuleOptions);
  },
  async updateEnabledStaticRulesets(extension, updateRulesetOptions) {
    await store.updateEnabledStaticRulesets(extension, updateRulesetOptions);
  },
  // Test-only helpers
  _getStoreForTesting() {
    requireTestOnlyCallers();
    return store;
  },
};
