/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Java Script module that helps consumers store data directly
 * to cloud storage provider download folders.
 *
 * Takes cloud storage providers metadata as JSON input on Mac, Linux and Windows.
 *
 * Handles scan, prompt response save and exposes preferred storage provider.
 */

"use strict";

var EXPORTED_SYMBOLS = ["CloudStorage"];
ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGlobalGetters(this, ["fetch"]);

ChromeUtils.defineModuleGetter(this, "Downloads",
                               "resource://gre/modules/Downloads.jsm");
ChromeUtils.defineModuleGetter(this, "FileUtils",
                               "resource://gre/modules/FileUtils.jsm");
ChromeUtils.defineModuleGetter(this, "OS",
                               "resource://gre/modules/osfile.jsm");

const CLOUD_SERVICES_PREF = "cloud.services.";
const CLOUD_PROVIDERS_URI = "resource://cloudstorage/providers.json";

/**
 * Provider metadata JSON is loaded from resource://cloudstorage/providers.json
 * Sample providers.json format
 *
 * {
 *   "Dropbox": {
 *     "displayName": "Dropbox",
 *     "relativeDownloadPath": ["homeDir", "Dropbox"],
 *     "relativeDiscoveryPath":  {
 *       "linux": ["homeDir", ".dropbox", "info.json"],
 *       "macosx": ["homeDir", ".dropbox", "info.json"],
 *       "win": ["LocalAppData", "Dropbox", "info.json"]
 *     },
 *     "typeSpecificData": {
 *       "default": "Downloads",
 *       "screenshot": "Screenshots"
 *     }
 *  }
 *
 * Providers JSON is flat list of providers metdata with property as key in format @Provider
 *
 * @Provider - Unique cloud provider key, possible values: "Dropbox", "GDrive"
 *
 * @displayName - cloud storage name displayed in the prompt.
 *
 * @relativeDownloadPath - download path on user desktop for a cloud storage provider.
 * By default downloadPath is a concatenation of home dir and name of dropbox folder.
 * Example value: ["homeDir", "Dropbox"]
 *
 * @relativeDiscoveryPath - Lists discoveryPath by platform. Provider is not supported on a platform
 * if its value doesn't exist in relativeDiscoveryPath. relativeDiscoveryPath by platform is stored
 * as an array ofsubdirectories, which when concatenated, forms discovery path.
 * During scan discoveryPath is checked for the existence of cloud storage provider on user desktop.
 *
 * @typeSpecificData - provides folder name for a cloud storage depending
 * on type of data downloaded. Default folder is 'Downloads'. Other options are
 * 'screenshot' depending on provider support.
 */

/**
 *
 * Internal cloud services prefs
 *
 * cloud.services.api.enabled - set to true to initialize and use Cloud Storage module
 *
 * cloud.services.storage.key - set to string with preferred provider key
 *
 * cloud.services.lastPrompt - set to time when last prompt was shown
 *
 * cloud.services.interval.prompt - set to time interval in days after which prompt should be shown
 *
 * cloud.services.rejected.key - set to string with comma separated provider keys rejected
 * by user when prompted to opt-in
 *
 * browser.download.folderList - set to int and indicates the location users wish to save downloaded files to.
 *     0 - The desktop is the default download location.
 *     1 - The system's downloads folder is the default download location.
 *     2 - The default download location is elsewhere as specified in
 *         browser.download.dir.
 *     3 - The default download location is elsewhere as specified by
 *         cloud storage API getDownloadFolder
 *
 * browser.download.dir - local file handle
 *   A local folder user may have selected for downloaded files to be
 *   saved. This folder is enabled when folderList equals 2.
 */

/**
 * The external API exported by this module.
 */

var CloudStorage = {
  /**
    * Init method to initialize providers metadata
    */
  async init() {
    let isInitialized = null;
    try {
      // Invoke internal method asynchronously to read and
      // parse providers metadata from JSON
      isInitialized = await CloudStorageInternal.initProviders();
    } catch (err) {
      Cu.reportError(err);
    }
    return isInitialized;
  },

  /**
   * Returns information to allow the consumer to decide whether showing
   * a doorhanger prompt is appropriate. If a preferred provider is set
   * on desktop, user is not prompted again and method returns null.
   *
   * @return {Promise} which resolves to an object with property name
   * as 'key' and 'value'.
   * 'key' property is provider key such as 'Dropbox', 'GDrive'.
   * 'value' property contains metadata for respective provider.
   * Resolves null if it's not appropriate to prompt.
   */
  promisePromptInfo() {
    return CloudStorageInternal.promisePromptInfo();
  },

  /**
   * Save user response from doorhanger prompt.
   * If user confirms and checks 'always remember', update prefs
   * cloud.services.storage.key and browser.download.folderList to pick
   * download location from cloud storage API
   * If user denies, save provider as rejected in cloud.services.rejected.key
   *
   * @param key
   *        cloud storage provider key from provider metadata
   * @param remember
   *        bool value indicating whether user has asked to always remember
   *        the settings
   * @param selected
   *        bool value by default set to false indicating if user has selected
   *        to save downloaded file with cloud provider
   */
  savePromptResponse(key, remember, selected = false) {
    Services.prefs.setIntPref(CLOUD_SERVICES_PREF + "lastprompt",
                              Math.floor(Date.now() / 1000));
    if (remember) {
      if (selected) {
        CloudStorageInternal.setCloudStoragePref(key);
      } else {
        // Store provider as rejected by setting cloud.services.rejected.key
        // and not use in re-prompt
        CloudStorageInternal.handleRejected(key);
      }
    }
  },

  /**
   * Retrieve download folder of an opted-in storage provider
   * by type specific data
   * @param typeSpecificData
   *        type of data downloaded, options are 'default', 'screenshot'
   * @return {Promise} which resolves to full path to provider download folder
   */
  getDownloadFolder(typeSpecificData) {
    return CloudStorageInternal.getDownloadFolder(typeSpecificData);
  },

  /**
   * Get key of provider opted-in by user to store downloaded files
   *
   * @return {String}
   * Storage provider key from provider metadata. Return empty string
   * if user has not selected a preferred provider.
   */
  getPreferredProvider() {
    return CloudStorageInternal.preferredProviderKey;
  },

  /**
   * Get metadata of provider opted-in by user to store downloaded files.
   * Return preferred provider metadata without scanning by doing simple lookup
   * inside storage providers metadata using preferred provider key
   *
   * @return {Object}
   * Object with preferred provider metadata. Return null
   * if user has not selected a preferred provider.
   */
  getPreferredProviderMetaData() {
    return CloudStorageInternal.getPreferredProviderMetaData();
  },

  /**
   * Get display name of a provider actively in use to store downloaded files
   *
   * @return {String}
   * String with provider display name. Returns null if a provider
   * is not in use.
   */
  getProviderIfInUse() {
    return CloudStorageInternal.getProviderIfInUse();
  },

  /**
   * Get providers found on user desktop. Used for unit tests
   *
   * @return {Promise}
   * @resolves
   * Map object with entries key set to storage provider key and values set to
   * storage provider metadata
   */
  getStorageProviders() {
    return CloudStorageInternal.getStorageProviders();
  },
};

/**
 * The internal API for the CloudStorage module.
 */

var CloudStorageInternal = {
  /**
   * promiseInit saves returned init method promise and is
   * used to wait for initialization to complete.
   */
  promiseInit: null,

  /**
   * Internal property having storage providers data
   */
  providersMetaData: null,

  async _downloadJSON(uri) {
    let json = null;
    try {
      let response = await fetch(uri);
      if (response.ok) {
        json = await response.json();
      }
    } catch (e) {
      Cu.reportError("Fetching " + uri + " results in error: " + e);
    }
    return json;
  },

  /**
   * Reset 'browser.download.folderList' cloud storage value '3' back
   * to '2' or '1' depending on custom path or system default Downloads path
   * in pref 'browser.download.dir'.
   */
  async resetFolderListPref() {
    let folderListValue = Services.prefs.getIntPref("browser.download.folderList", 0);
    if (folderListValue !== 3) {
      return;
    }

    let downloadDirPath = null;
    try {
      let file = Services.prefs.getComplexValue("browser.download.dir",
                                                Ci.nsIFile);
      downloadDirPath = file.path;
    } catch (e) {}

    if (!downloadDirPath ||
        (downloadDirPath === await Downloads.getSystemDownloadsDirectory())) {
      // if downloadDirPath is the Downloads folder path or unspecified
      folderListValue = 1;
    } else if (downloadDirPath === Services.dirsvc.get("Desk", Ci.nsIFile).path) {
      // if downloadDirPath is the Desktop path
      folderListValue = 0;
    } else {
      // otherwise
      folderListValue = 2;
    }
    Services.prefs.setIntPref("browser.download.folderList", folderListValue);
  },

  /**
   * Loads storage providers metadata asynchronously from providers.json.
   *
   * @returns {Promise} with resolved boolean value true if providers
   * metadata is successfully initialized
   */
  async initProviders() {
    // Cloud Storage API should continue initialization and load providers metadata
    // only if a consumer add-on using API sets pref 'cloud.services.api.enabled' to true
    // If API is not enabled, check and reset cloud storage value in folderList pref.
    if (!this.isAPIEnabled) {
      this.resetFolderListPref().catch((err) => {
        Cu.reportError("CloudStorage: Failed to reset folderList pref " + err);
      });
      return false;
    }

    let response = await this._downloadJSON(CLOUD_PROVIDERS_URI);
    this.providersMetaData = await this._parseProvidersJSON(response);

    let providersCount = Object.keys(this.providersMetaData).length;
    if (providersCount > 0) {
      // Array of boolean results for each provider handled for custom downloadpath
      let handledProviders = await this.initDownloadPathIfProvidersExist();
      if (handledProviders.length === providersCount) {
        return true;
      }
    }
    return false;
  },

  /**
   * Load parsed metadata inside providers object
   */
  _parseProvidersJSON(providers) {
    if (!providers) {
      return {};
    }

    // Use relativeDiscoveryPath to filter providers object by platform.
    // DownloadPath and discoveryPath are stored as
    // array of subdirectories inside providers.json
    // Update providers object discoveryPath and downloadPath
    // property values by concatenating subdirectories and forming platform
    // specific directory path

    Object.getOwnPropertyNames(providers).forEach(key => {
      if (providers[key].relativeDiscoveryPath.hasOwnProperty(AppConstants.platform)) {
        providers[key].discoveryPath =
          this._concatPath(providers[key].relativeDiscoveryPath[AppConstants.platform]);
        providers[key].downloadPath =
          this._concatPath(providers[key].relativeDownloadPath);
      } else {
        // delete key not supported on AppConstants.platform
        delete providers[key];
      }
    });
    return providers;
  },

  /**
   * Concatenate subdir value inside array to form
   * platform specific directory path
   *
   * @param arrDirs
   *        String Array containing sub directories name
   * @returns Path of type String
   */
  _concatPath(arrDirs) {
    let dirPath = "";
    for (let subDir of arrDirs) {
      switch (subDir) {
        case "homeDir":
          subDir = OS.Constants.Path.homeDir ? OS.Constants.Path.homeDir : "";
          break;
        case "LocalAppData":
          if (OS.Constants.Win) {
            let nsIFileLocal = Services.dirsvc.get("LocalAppData", Ci.nsIFile);
            subDir = nsIFileLocal && nsIFileLocal.path ? nsIFileLocal.path : "";
          } else {
            subDir = "";
          }
          break;
      }
      dirPath = OS.Path.join(dirPath, subDir);
    }
    return dirPath;
  },

  /**
   * Check for custom download paths and override providers metadata
   * downloadPath property
   *
   * For dropbox open config file ~/.dropbox/info.json
   * and override downloadPath with path found
   * See https://www.dropbox.com/en/help/desktop-web/find-folder-paths
   *
   * For all other providers we are using downloadpath from providers.json
   *
   * @returns {Promise} with array boolean values for respective provider. Value is true if a
   * provider exist on user desktop and its downloadPath is updated. Promise returns with
   * resolved array value when all providers in metadata are handled.
   */
  initDownloadPathIfProvidersExist() {
    let providerKeys = Object.keys(this.providersMetaData);
    let promises = providerKeys.map(key => {
      return key === "Dropbox" ?
             this._initDropbox(key) :
             Promise.resolve(false);
    });
    return Promise.all(promises);
  },

  /**
   * Read Dropbox info.json and override providers metadata
   * downloadPath property
   *
   * @return {Promise}
   * @resolves
   * false if dropbox provider is not found. Returns true if dropbox service exist
   * on user desktop and downloadPath in providermetadata is updated with
   * value read from config file info.json
   */
  async _initDropbox(key) {
    // Check if Dropbox provider exist on desktop before continuing
    if (!await this._checkIfAssetExists(this.providersMetaData[key].discoveryPath)) {
      return false;
    }

    // Check in cloud.services.rejected.key if Dropbox is previously rejected before continuing
    let rejectedKeys = this.cloudStorageRejectedKeys.split(",");
    if (rejectedKeys.includes(key)) {
      return false;
    }

    let file = null;
    try {
      file = new FileUtils.File(this.providersMetaData[key].discoveryPath);
    } catch (ex) {
      return false;
    }

    let data = await this._downloadJSON(Services.io.newFileURI(file).spec);

    if (!data) {
      return false;
    }

    let path = data && data.personal && data.personal.path;
    if (!path) {
      return false;
    }
    let isUsable = await this._isUsableDirectory(path);
    if (isUsable) {
      this.providersMetaData.Dropbox.downloadPath = path;
    }
    return isUsable;
  },

  /**
   * Determines if a given directory is valid and can be used to download files
   *
   * @param full absolute path to the directory
   *
   * @return {Promise} which resolves true if we can use the directory, false otherwise.
   */
  async _isUsableDirectory(path) {
    let isUsable = false;
    try {
      let info = await OS.File.stat(path);
      isUsable = info.isDir;
    } catch (e) {
      // Directory doesn't exist, so isUsable will still be false
    }
    return isUsable;
  },

  /**
   * Retrieve download folder of preferred provider by type specific data
   *
   * @param dataType
   *        type of data downloaded, options are 'default', 'screenshot'
   *        default value is 'default'
   * @return {Promise} which resolves to full path to download folder
   * Resolves null if a valid download folder is not found.
   */
  async getDownloadFolder(dataType = "default") {
    // Wait for cloudstorage to initialize if providers metadata is not available
    if (!this.providersMetaData) {
      let isInitialized = await this.promiseInit;
      if (!isInitialized && !this.providersMetaData) {
        Cu.reportError("CloudStorage: Failed to initialize and retrieve download folder ");
        return null;
      }
    }

    let key = this.preferredProviderKey;
    if (!key || !this.providersMetaData.hasOwnProperty(key)) {
      return null;
    }

    let provider = this.providersMetaData[key];
    if (!provider.typeSpecificData[dataType]) {
      return null;
    }

    let downloadDirPath = OS.Path.join(provider.downloadPath,
                                       provider.typeSpecificData[dataType]);
    if (!(await this._isUsableDirectory(downloadDirPath))) {
      return null;
    }
    return downloadDirPath;
  },

  /**
   * Return scanned provider info used by consumer inside doorhanger prompt.
   * @return {Promise}
   * which resolves to an object with property 'key' as found provider and
   * property 'value' as provider metadata.
   * Resolves null if no provider info is returned.
   */
  async promisePromptInfo() {
    // Check if user has not previously opted-in for preferred provider download folder
    // and if time elapsed since last prompt shown has exceeded maximum allowed interval
    // in pref cloud.services.interval.prompt before continuing to scan for providers
    if (!this.preferredProviderKey && this.shouldPrompt()) {
      return this.scan();
    }
    return Promise.resolve(null);
  },

  /**
   * Check if its time to prompt by reading lastprompt service pref.
   * Return true if pref doesn't exist or last prompt time is
   * more than prompt interval
   */
  shouldPrompt() {
    let lastPrompt = this.lastPromptTime;
    let now = Math.floor(Date.now() / 1000);
    let interval = now - lastPrompt;

    // Convert prompt interval to seconds
    let maxAllow = this.promptInterval * 24 * 60 * 60;
    return interval >= maxAllow;
  },

  /**
   * Scans for local storage providers available on user desktop
   *
   * providers list is read in order as specified in providers.json.
   * If a user has multiple cloud storage providers on desktop, return the first
   * provider after filtering the rejected keys
   *
   * @return {Promise}
   * which resolves to an object providerInfo with found provider key and value
   * as provider metadata. Resolves null if no valid provider found
   */
  async scan() {
    let providers = await this.getStorageProviders();
    if (!providers.size) {
      // No storage services installed on user desktop
      return null;
    }

    // Filter the rejected providers in cloud.services.rejected.key
    // from the providers map object
    let rejectedKeys = this.cloudStorageRejectedKeys.split(",");
    for (let rejectedKey of rejectedKeys) {
      providers.delete(rejectedKey);
    }

    // Pick first storage provider from providers
    let provider = providers.entries().next().value;
    if (provider) {
      return {key: provider[0], value: provider[1]};
    }
    return null;
  },

  /**
   * Checks if the asset with input path exist on
   * file system
   * @return {Promise}
   * @resolves
   * boolean value of file existence check
   */
  _checkIfAssetExists(path) {
    return OS.File.exists(path).catch(err => {
      Cu.reportError(`Couldn't check existance of ${path}`, err);
      return false;
    });
  },

  /**
   * get access to all local storage providers available on user desktop
   *
   * @return {Promise}
   * @resolves
   * Map object with entries key set to storage provider key and values set to
   * storage provider metadata
   */
  async getStorageProviders() {
    let providers = Object.entries(this.providersMetaData || {});

    // Array of promises with boolean value exist for respective storage.
    let promises = providers.map(([, provider]) => this._checkIfAssetExists(provider.discoveryPath));
    let results = await Promise.all(promises);

    // Filter providers array to remove provider with discoveryPath asset exist resolved value false
    providers = providers.filter((_, idx) => results[idx]);
    return new Map(providers);
  },

  /**
   * Save the rejected provider in cloud.services.rejected.key. Pref
   * stores rejected keys value as comma separated string.
   *
   * @param key
   *        Provider key to be saved in cloud.services.rejected.key pref
   */
  handleRejected(key) {
    let rejected = this.cloudStorageRejectedKeys;

    if (!rejected) {
      Services.prefs.setCharPref(CLOUD_SERVICES_PREF + "rejected.key", key);
    } else {
      // Pref exists with previous rejected keys, append
      // key at the end and update pref
      let keys = rejected.split(",");
      if (key) {
        keys.push(key);
      }
      Services.prefs.setCharPref(CLOUD_SERVICES_PREF + "rejected.key", keys.join(","));
    }
  },

  /**
   *
   * Sets pref cloud.services.storage.key. It updates download browser.download.folderList
   * value to 3 indicating download location is stored elsewhere, as specified by
   * cloud storage API getDownloadFolder
   *
   * @param key
   *        cloud storage provider key from provider metadata
   */
  setCloudStoragePref(key) {
    Services.prefs.setCharPref(CLOUD_SERVICES_PREF + "storage.key", key);
    Services.prefs.setIntPref("browser.download.folderList", 3);
  },

  /**
   * get access to preferred provider metadata by using preferred provider key
   *
   * @return {Object}
   * Object with preferred provider metadata. Returns null if preferred provider is not set
   */
  getPreferredProviderMetaData() {
    // Use preferred provider key to retrieve metadata from ProvidersMetaData
    return this.providersMetaData.hasOwnProperty(this.preferredProviderKey) ?
      this.providersMetaData[this.preferredProviderKey] : null;
  },

  /**
   * Get provider display name if cloud storage API is used by an add-on
   * and user has set preferred provider and a valid download directory
   * path exists on user desktop.
   *
   * @return {String}
   * String with preferred provider display name. Returns null if provider is not in use.
   */
  async getProviderIfInUse() {
    // Check if consumer add-on is present and user has set preferred provider key
    // and a valid download path exist on user desktop
    if (this.isAPIEnabled && this.preferredProviderKey && await this.getDownloadFolder()) {
      let provider = this.getPreferredProviderMetaData();
      return provider.displayName || null;
    }
    return null;
  },
};

/**
 * Provider key retrieved from service pref cloud.services.storage.key
 */
XPCOMUtils.defineLazyPreferenceGetter(CloudStorageInternal, "preferredProviderKey",
  CLOUD_SERVICES_PREF + "storage.key", "");

/**
 * Provider keys rejected by user for default download
 */
XPCOMUtils.defineLazyPreferenceGetter(CloudStorageInternal, "cloudStorageRejectedKeys",
  CLOUD_SERVICES_PREF + "rejected.key", "");

/**
 * Lastprompt time in seconds, by default set to 0
 */
XPCOMUtils.defineLazyPreferenceGetter(CloudStorageInternal, "lastPromptTime",
  CLOUD_SERVICES_PREF + "lastprompt", 0 /* 0 second */);

/**
 * show prompt interval in days, by default set to 0
 */
XPCOMUtils.defineLazyPreferenceGetter(CloudStorageInternal, "promptInterval",
  CLOUD_SERVICES_PREF + "interval.prompt", 0 /* 0 days */);

/**
 * generic pref that shows if cloud storage API is in use, by default set to false.
 * Re-run CloudStorage init evertytime pref is set.
 */
XPCOMUtils.defineLazyPreferenceGetter(CloudStorageInternal, "isAPIEnabled",
  CLOUD_SERVICES_PREF + "api.enabled", false, () => CloudStorage.init());

CloudStorageInternal.promiseInit = CloudStorage.init();
