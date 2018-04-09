/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// These are injected from XPIProvider.jsm
/* globals ADDON_SIGNING, SIGNED_TYPES, BOOTSTRAP_REASONS, DB_SCHEMA,
          AddonInternal, XPIProvider, XPIStates, syncLoadManifestFromFile,
          isUsableAddon, recordAddonTelemetry,
          flushChromeCaches, descriptorToPath, DEFAULT_SKIN */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  AddonManagerPrivate: "resource://gre/modules/AddonManager.jsm",
  AddonRepository: "resource://gre/modules/addons/AddonRepository.jsm",
  DeferredTask: "resource://gre/modules/DeferredTask.jsm",
  FileUtils: "resource://gre/modules/FileUtils.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

XPCOMUtils.defineLazyServiceGetter(this, "Blocklist",
                                   "@mozilla.org/extensions/blocklist;1",
                                   Ci.nsIBlocklistService);

ChromeUtils.import("resource://gre/modules/Log.jsm");
const LOGGER_ID = "addons.xpi-utils";

const nsIFile = Components.Constructor("@mozilla.org/file/local;1", "nsIFile",
                                       "initWithPath");

// Create a new logger for use by the Addons XPI Provider Utils
// (Requires AddonManager.jsm)
var logger = Log.repository.getLogger(LOGGER_ID);

const KEY_PROFILEDIR                  = "ProfD";
const FILE_JSON_DB                    = "extensions.json";

// The last version of DB_SCHEMA implemented in SQLITE
const LAST_SQLITE_DB_SCHEMA           = 14;
const PREF_DB_SCHEMA                  = "extensions.databaseSchema";
const PREF_PENDING_OPERATIONS         = "extensions.pendingOperations";
const PREF_EM_AUTO_DISABLED_SCOPES    = "extensions.autoDisableScopes";

const KEY_APP_SYSTEM_ADDONS           = "app-system-addons";
const KEY_APP_SYSTEM_DEFAULTS         = "app-system-defaults";
const KEY_APP_GLOBAL                  = "app-global";
const KEY_APP_TEMPORARY               = "app-temporary";

// Properties to save in JSON file
const PROP_JSON_FIELDS = ["id", "syncGUID", "location", "version", "type",
                          "internalName", "updateURL", "optionsURL",
                          "optionsType", "optionsBrowserStyle", "aboutURL",
                          "defaultLocale", "visible", "active", "userDisabled",
                          "appDisabled", "pendingUninstall", "installDate",
                          "updateDate", "applyBackgroundUpdates", "bootstrap", "path",
                          "skinnable", "size", "sourceURI", "releaseNotesURI",
                          "softDisabled", "foreignInstall",
                          "strictCompatibility", "locales", "targetApplications",
                          "targetPlatforms", "signedState",
                          "seen", "dependencies", "hasEmbeddedWebExtension",
                          "userPermissions", "icons", "iconURL", "icon64URL",
                          "blocklistState", "blocklistURL", "startupData"];

// Time to wait before async save of XPI JSON database, in milliseconds
const ASYNC_SAVE_DELAY_MS = 20;

/**
 * Asynchronously fill in the _repositoryAddon field for one addon
 */
function getRepositoryAddon(aAddon, aCallback) {
  if (!aAddon) {
    aCallback(aAddon);
    return;
  }
  AddonRepository.getCachedAddonByID(aAddon.id, repoAddon => {
    aAddon._repositoryAddon = repoAddon;
    aCallback(aAddon);
  });
}

/**
 * Wrap an API-supplied function in an exception handler to make it safe to call
 */
function makeSafe(aCallback) {
  return function(...aArgs) {
    try {
      aCallback(...aArgs);
    } catch (ex) {
      logger.warn("XPI Database callback failed", ex);
    }
  };
}

/**
 * A helper method to asynchronously call a function on an array of objects.
 * Returns a promise that resolves with the results for each function call in
 * the same order as the aObjects array.
 * WARNING: not currently error-safe; if the async function does not call its
 * callback for any of the array elements, asyncMap will never resolve.
 *
 * @param  aObjects
 *         The array of objects to process asynchronously
 * @param  aMethod
 *         Function with signature function(object, function(f_of_object))
 */
function asyncMap(aObjects, aMethod) {
  let methodCalls = aObjects.map(obj => {
    return new Promise(resolve => {
      try {
        aMethod(obj, resolve);
      } catch (e) {
        logger.error("Async map function failed", e);
        resolve(undefined);
      }
    });
  });

  return Promise.all(methodCalls);
}

/**
 * Copies properties from one object to another. If no target object is passed
 * a new object will be created and returned.
 *
 * @param  aObject
 *         An object to copy from
 * @param  aProperties
 *         An array of properties to be copied
 * @param  aTarget
 *         An optional target object to copy the properties to
 * @return the object that the properties were copied onto
 */
function copyProperties(aObject, aProperties, aTarget) {
  if (!aTarget)
    aTarget = {};
  aProperties.forEach(function(aProp) {
    if (aProp in aObject)
      aTarget[aProp] = aObject[aProp];
  });
  return aTarget;
}

/**
 * The DBAddonInternal is a special AddonInternal that has been retrieved from
 * the database. The constructor will initialize the DBAddonInternal with a set
 * of fields, which could come from either the JSON store or as an
 * XPIProvider.AddonInternal created from an addon's manifest
 * @constructor
 * @param aLoaded
 *        Addon data fields loaded from JSON or the addon manifest.
 */
function DBAddonInternal(aLoaded) {
  AddonInternal.call(this);

  if (aLoaded.descriptor) {
    if (!aLoaded.path) {
      aLoaded.path = descriptorToPath(aLoaded.descriptor);
    }
    delete aLoaded.descriptor;
  }

  copyProperties(aLoaded, PROP_JSON_FIELDS, this);

  if (!this.dependencies)
    this.dependencies = [];
  Object.freeze(this.dependencies);

  if (aLoaded._installLocation) {
    this._installLocation = aLoaded._installLocation;
    this.location = aLoaded._installLocation.name;
  } else if (aLoaded.location) {
    this._installLocation = XPIProvider.installLocationsByName[this.location];
  }

  this._key = this.location + ":" + this.id;

  if (!aLoaded._sourceBundle) {
    throw new Error("Expected passed argument to contain a path");
  }

  this._sourceBundle = aLoaded._sourceBundle;
}

DBAddonInternal.prototype = Object.create(AddonInternal.prototype);
Object.assign(DBAddonInternal.prototype, {
  applyCompatibilityUpdate(aUpdate, aSyncCompatibility) {
    let wasCompatible = this.isCompatible;

    this.targetApplications.forEach(function(aTargetApp) {
      aUpdate.targetApplications.forEach(function(aUpdateTarget) {
        if (aTargetApp.id == aUpdateTarget.id && (aSyncCompatibility ||
            Services.vc.compare(aTargetApp.maxVersion, aUpdateTarget.maxVersion) < 0)) {
          aTargetApp.minVersion = aUpdateTarget.minVersion;
          aTargetApp.maxVersion = aUpdateTarget.maxVersion;
          XPIDatabase.saveChanges();
        }
      });
    });

    if (wasCompatible != this.isCompatible)
      XPIProvider.updateAddonDisabledState(this);
  },

  toJSON() {
    let jsonData = copyProperties(this, PROP_JSON_FIELDS);

    // Experiments are serialized as disabled so they aren't run on the next
    // startup.
    if (this.type == "experiment") {
      jsonData.userDisabled = true;
      jsonData.active = false;
    }

    return jsonData;
  },

  get inDatabase() {
    return true;
  }
});

/**
 * Internal interface: find an addon from an already loaded addonDB
 */
function _findAddon(addonDB, aFilter) {
  for (let addon of addonDB.values()) {
    if (aFilter(addon)) {
      return addon;
    }
  }
  return null;
}

/**
 * Internal interface to get a filtered list of addons from a loaded addonDB
 */
function _filterDB(addonDB, aFilter) {
  return Array.from(addonDB.values()).filter(aFilter);
}

this.XPIDatabase = {
  // true if the database connection has been opened
  initialized: false,
  // The database file
  jsonFile: FileUtils.getFile(KEY_PROFILEDIR, [FILE_JSON_DB], true),
  // Migration data loaded from an old version of the database.
  migrateData: null,
  // Active add-on directories loaded from extensions.ini and prefs at startup.
  activeBundles: null,

  _saveTask: null,

  // Saved error object if we fail to read an existing database
  _loadError: null,

  // Saved error object if we fail to save the database
  _saveError: null,

  // Error reported by our most recent attempt to read or write the database, if any
  get lastError() {
    if (this._loadError)
      return this._loadError;
    if (this._saveError)
      return this._saveError;
    return null;
  },

  async _saveNow() {
    try {
      let json = JSON.stringify(this);
      let path = this.jsonFile.path;
      await OS.File.writeAtomic(path, json, {tmpPath: `${path}.tmp`});

      if (!this._schemaVersionSet) {
        // Update the XPIDB schema version preference the first time we
        // successfully save the database.
        logger.debug("XPI Database saved, setting schema version preference to " + DB_SCHEMA);
        Services.prefs.setIntPref(PREF_DB_SCHEMA, DB_SCHEMA);
        this._schemaVersionSet = true;

        // Reading the DB worked once, so we don't need the load error
        this._loadError = null;
      }
    } catch (error) {
      logger.warn("Failed to save XPI database", error);
      this._saveError = error;
      throw error;
    }
  },

  /**
   * Mark the current stored data dirty, and schedule a flush to disk
   */
  saveChanges() {
    if (!this.initialized) {
      throw new Error("Attempt to use XPI database when it is not initialized");
    }

    if (XPIProvider._closing) {
      // use an Error here so we get a stack trace.
      let err = new Error("XPI database modified after shutdown began");
      logger.warn(err);
      AddonManagerPrivate.recordSimpleMeasure("XPIDB_late_stack", Log.stackTrace(err));
    }

    if (!this._saveTask) {
      this._saveTask = new DeferredTask(() => this._saveNow(),
                                        ASYNC_SAVE_DELAY_MS);
    }

    this._saveTask.arm();
  },

  async finalize() {
    // handle the "in memory only" and "saveChanges never called" cases
    if (!this._saveTask) {
      return;
    }

    await this._saveTask.finalize();
  },

  /**
   * Converts the current internal state of the XPI addon database to
   * a JSON.stringify()-ready structure
   */
  toJSON() {
    if (!this.addonDB) {
      // We never loaded the database?
      throw new Error("Attempt to save database without loading it first");
    }

    let toSave = {
      schemaVersion: DB_SCHEMA,
      addons: Array.from(this.addonDB.values())
                   .filter(addon => addon.location != KEY_APP_TEMPORARY),
    };
    return toSave;
  },

  /**
   * Synchronously opens and reads the database file, upgrading from old
   * databases or making a new DB if needed.
   *
   * The possibilities, in order of priority, are:
   * 1) Perfectly good, up to date database
   * 2) Out of date JSON database needs to be upgraded => upgrade
   * 3) JSON database exists but is mangled somehow => build new JSON
   * 4) no JSON DB, but a usable SQLITE db we can upgrade from => upgrade
   * 5) useless SQLITE DB => build new JSON
   * 6) usable RDF DB => upgrade
   * 7) useless RDF DB => build new JSON
   * 8) Nothing at all => build new JSON
   * @param  aRebuildOnError
   *         A boolean indicating whether add-on information should be loaded
   *         from the install locations if the database needs to be rebuilt.
   *         (if false, caller is XPIProvider.checkForChanges() which will rebuild)
   */
  syncLoadDB(aRebuildOnError) {
    this.migrateData = null;
    let fstream = null;
    let data = "";
    try {
      let readTimer = AddonManagerPrivate.simpleTimer("XPIDB_syncRead_MS");
      logger.debug("Opening XPI database " + this.jsonFile.path);
      fstream = Cc["@mozilla.org/network/file-input-stream;1"].
              createInstance(Ci.nsIFileInputStream);
      fstream.init(this.jsonFile, -1, 0, 0);
      let cstream = null;
      try {
        cstream = Cc["@mozilla.org/intl/converter-input-stream;1"].
                createInstance(Ci.nsIConverterInputStream);
        cstream.init(fstream, "UTF-8", 0, 0);

        let str = {};
        let read = 0;
        do {
          read = cstream.readString(0xffffffff, str); // read as much as we can and put it in str.value
          data += str.value;
        } while (read != 0);

        readTimer.done();
        this.parseDB(data, aRebuildOnError);
      } catch (e) {
        logger.error("Failed to load XPI JSON data from profile", e);
        let rebuildTimer = AddonManagerPrivate.simpleTimer("XPIDB_rebuildReadFailed_MS");
        this.rebuildDatabase(aRebuildOnError);
        rebuildTimer.done();
      } finally {
        if (cstream)
          cstream.close();
      }
    } catch (e) {
      if (e.result === Cr.NS_ERROR_FILE_NOT_FOUND) {
        this.upgradeDB(aRebuildOnError);
      } else {
        this.rebuildUnreadableDB(e, aRebuildOnError);
      }
    } finally {
      if (fstream)
        fstream.close();
    }
    // If an async load was also in progress, record in telemetry.
    if (this._dbPromise) {
      AddonManagerPrivate.recordSimpleMeasure("XPIDB_overlapped_load", 1);
    }
    this._dbPromise = Promise.resolve(this.addonDB);
    Services.obs.notifyObservers(this.addonDB, "xpi-database-loaded");
  },

  /**
   * Parse loaded data, reconstructing the database if the loaded data is not valid
   * @param aRebuildOnError
   *        If true, synchronously reconstruct the database from installed add-ons
   */
  parseDB(aData, aRebuildOnError) {
    let parseTimer = AddonManagerPrivate.simpleTimer("XPIDB_parseDB_MS");
    try {
      // dump("Loaded JSON:\n" + aData + "\n");
      let inputAddons = JSON.parse(aData);
      // Now do some sanity checks on our JSON db
      if (!("schemaVersion" in inputAddons) || !("addons" in inputAddons)) {
        parseTimer.done();
        // Content of JSON file is bad, need to rebuild from scratch
        logger.error("bad JSON file contents");
        AddonManagerPrivate.recordSimpleMeasure("XPIDB_startupError", "badJSON");
        let rebuildTimer = AddonManagerPrivate.simpleTimer("XPIDB_rebuildBadJSON_MS");
        this.rebuildDatabase(aRebuildOnError);
        rebuildTimer.done();
        return;
      }
      if (inputAddons.schemaVersion != DB_SCHEMA) {
        // Handle mismatched JSON schema version. For now, we assume
        // compatibility for JSON data, though we throw away any fields we
        // don't know about (bug 902956)
        AddonManagerPrivate.recordSimpleMeasure("XPIDB_startupError",
                                                "schemaMismatch-" + inputAddons.schemaVersion);
        logger.debug("JSON schema mismatch: expected " + DB_SCHEMA +
            ", actual " + inputAddons.schemaVersion);
        // When we rev the schema of the JSON database, we need to make sure we
        // force the DB to save so that the DB_SCHEMA value in the JSON file and
        // the preference are updated.
      }
      // If we got here, we probably have good data
      // Make AddonInternal instances from the loaded data and save them
      let addonDB = new Map();
      for (let loadedAddon of inputAddons.addons) {
        try {
          if (!loadedAddon.path) {
            loadedAddon.path = descriptorToPath(loadedAddon.descriptor);
          }
          loadedAddon._sourceBundle = new nsIFile(loadedAddon.path);
        } catch (e) {
          // We can fail here when the path is invalid, usually from the
          // wrong OS
          logger.warn("Could not find source bundle for add-on " + loadedAddon.id, e);
        }

        let newAddon = new DBAddonInternal(loadedAddon);
        addonDB.set(newAddon._key, newAddon);
      }
      parseTimer.done();
      this.addonDB = addonDB;
      logger.debug("Successfully read XPI database");
      this.initialized = true;
    } catch (e) {
      // If we catch and log a SyntaxError from the JSON
      // parser, the xpcshell test harness fails the test for us: bug 870828
      parseTimer.done();
      if (e.name == "SyntaxError") {
        logger.error("Syntax error parsing saved XPI JSON data");
        AddonManagerPrivate.recordSimpleMeasure("XPIDB_startupError", "syntax");
      } else {
        logger.error("Failed to load XPI JSON data from profile", e);
        AddonManagerPrivate.recordSimpleMeasure("XPIDB_startupError", "other");
      }
      let rebuildTimer = AddonManagerPrivate.simpleTimer("XPIDB_rebuildReadFailed_MS");
      this.rebuildDatabase(aRebuildOnError);
      rebuildTimer.done();
    }
  },

  /**
   * Upgrade database from earlier (sqlite or RDF) version if available
   */
  upgradeDB(aRebuildOnError) {
    let upgradeTimer = AddonManagerPrivate.simpleTimer("XPIDB_upgradeDB_MS");

    let schemaVersion = Services.prefs.getIntPref(PREF_DB_SCHEMA, 0);
    if (schemaVersion > LAST_SQLITE_DB_SCHEMA) {
      // we've upgraded before but the JSON file is gone, fall through
      // and rebuild from scratch
      AddonManagerPrivate.recordSimpleMeasure("XPIDB_startupError", "dbMissing");
    }

    this.rebuildDatabase(aRebuildOnError);
    upgradeTimer.done();
  },

  /**
   * Reconstruct when the DB file exists but is unreadable
   * (for example because read permission is denied)
   */
  rebuildUnreadableDB(aError, aRebuildOnError) {
    let rebuildTimer = AddonManagerPrivate.simpleTimer("XPIDB_rebuildUnreadableDB_MS");
    logger.warn("Extensions database " + this.jsonFile.path +
        " exists but is not readable; rebuilding", aError);
    // Remember the error message until we try and write at least once, so
    // we know at shutdown time that there was a problem
    this._loadError = aError;
    AddonManagerPrivate.recordSimpleMeasure("XPIDB_startupError", "unreadable");
    this.rebuildDatabase(aRebuildOnError);
    rebuildTimer.done();
  },

  /**
   * Open and read the XPI database asynchronously, upgrading if
   * necessary. If any DB load operation fails, we need to
   * synchronously rebuild the DB from the installed extensions.
   *
   * @return Promise<Map> resolves to the Map of loaded JSON data stored
   *         in this.addonDB; never rejects.
   */
  asyncLoadDB() {
    // Already started (and possibly finished) loading
    if (this._dbPromise) {
      return this._dbPromise;
    }

    logger.debug("Starting async load of XPI database " + this.jsonFile.path);
    AddonManagerPrivate.recordSimpleMeasure("XPIDB_async_load", XPIProvider.runPhase);
    let readOptions = {
      outExecutionDuration: 0
    };
    this._dbPromise = OS.File.read(this.jsonFile.path, null, readOptions).then(
      byteArray => {
        logger.debug("Async JSON file read took " + readOptions.outExecutionDuration + " MS");
        AddonManagerPrivate.recordSimpleMeasure("XPIDB_asyncRead_MS",
          readOptions.outExecutionDuration);

        if (this.addonDB) {
          logger.debug("Synchronous load completed while waiting for async load");
          return this.addonDB;
        }
        logger.debug("Finished async read of XPI database, parsing...");
        let decodeTimer = AddonManagerPrivate.simpleTimer("XPIDB_decode_MS");
        let decoder = new TextDecoder();
        let data = decoder.decode(byteArray);
        decodeTimer.done();
        this.parseDB(data, true);
        return this.addonDB;
      })
    .catch(
      error => {
        if (this.addonDB) {
          logger.debug("Synchronous load completed while waiting for async load");
          return this.addonDB;
        }
        if (error.becauseNoSuchFile) {
          this.upgradeDB(true);
        } else {
          // it's there but unreadable
          this.rebuildUnreadableDB(error, true);
        }
        return this.addonDB;
      });

    this._dbPromise.then(() => {
      Services.obs.notifyObservers(this.addonDB, "xpi-database-loaded");
    });

    return this._dbPromise;
  },

  /**
   * Rebuild the database from addon install directories. If this.migrateData
   * is available, uses migrated information for settings on the addons found
   * during rebuild
   * @param aRebuildOnError
   *         A boolean indicating whether add-on information should be loaded
   *         from the install locations if the database needs to be rebuilt.
   *         (if false, caller is XPIProvider.checkForChanges() which will rebuild)
   */
  rebuildDatabase(aRebuildOnError) {
    this.addonDB = new Map();
    this.initialized = true;

    if (XPIStates.size == 0) {
      // No extensions installed, so we're done
      logger.debug("Rebuilding XPI database with no extensions");
      return;
    }

    // If there is no migration data then load the list of add-on directories
    // that were active during the last run
    if (!this.migrateData) {
      this.activeBundles = Array.from(XPIStates.initialEnabledAddons(),
                                      addon => addon.path);
      if (!this.activeBundles.length)
        this.activeBundles = null;
    }


    if (aRebuildOnError) {
      logger.warn("Rebuilding add-ons database from installed extensions.");
      try {
        XPIDatabaseReconcile.processFileChanges({}, false);
      } catch (e) {
        logger.error("Failed to rebuild XPI database from installed extensions", e);
      }
      // Make sure to update the active add-ons and add-ons list on shutdown
      Services.prefs.setBoolPref(PREF_PENDING_OPERATIONS, true);
    }
  },

  /**
   * Shuts down the database connection and releases all cached objects.
   * Return: Promise{integer} resolves / rejects with the result of the DB
   *                          flush after the database is flushed and
   *                          all cleanup is done
   */
  async shutdown() {
    logger.debug("shutdown");
    if (this.initialized) {
      // If our last database I/O had an error, try one last time to save.
      if (this.lastError)
        this.saveChanges();

      this.initialized = false;

      // If we're shutting down while still loading, finish loading
      // before everything else!
      if (this._dbPromise) {
        await this._dbPromise;
      }

      // Await any pending DB writes and finish cleaning up.
      await this.finalize();

      if (this._saveError) {
        // If our last attempt to read or write the DB failed, force a new
        // extensions.ini to be written to disk on the next startup
        Services.prefs.setBoolPref(PREF_PENDING_OPERATIONS, true);
      }

      // Clear out the cached addons data loaded from JSON
      delete this.addonDB;
      delete this._dbPromise;
      // same for the deferred save
      delete this._saveTask;
      // re-enable the schema version setter
      delete this._schemaVersionSet;
    }
  },

  /**
   * Asynchronously list all addons that match the filter function
   * @param  aFilter
   *         Function that takes an addon instance and returns
   *         true if that addon should be included in the selected array
   * @param  aCallback
   *         Optional and will be called with an array of addons matching
   *         aFilter or an empty array if none match.
   * @return a Promise that resolves to the list of add-ons matching aFilter or
   *         an empty array if none match
   */
  async getAddonList(aFilter, aCallback) {
    try {
      let addonDB = await this.asyncLoadDB();
      let addonList = _filterDB(addonDB, aFilter);
      let addons = await asyncMap(addonList, getRepositoryAddon);
      if (aCallback) {
        makeSafe(aCallback)(addons);
      }
      return addons;
    } catch (error) {
      logger.error("getAddonList failed", error);
      if (aCallback) {
        makeSafe(aCallback)([]);
      }
      return [];
    }
  },

  /**
   * (Possibly asynchronously) get the first addon that matches the filter function
   * @param  aFilter
   *         Function that takes an addon instance and returns
   *         true if that addon should be selected
   * @param  aCallback
   *         Called back with the addon, or null if no matching addon is found
   */
  getAddon(aFilter, aCallback) {
    return this.asyncLoadDB().then(
      addonDB => {
        getRepositoryAddon(_findAddon(addonDB, aFilter), makeSafe(aCallback));
      })
    .catch(
        error => {
          logger.error("getAddon failed", error);
          makeSafe(aCallback)(null);
        });
  },

  /**
   * Asynchronously gets an add-on with a particular ID in a particular
   * install location.
   *
   * @param  aId
   *         The ID of the add-on to retrieve
   * @param  aLocation
   *         The name of the install location
   * @param  aCallback
   *         A callback to pass the DBAddonInternal to
   */
  getAddonInLocation(aId, aLocation, aCallback) {
    this.asyncLoadDB().then(
        addonDB => getRepositoryAddon(addonDB.get(aLocation + ":" + aId),
                                      makeSafe(aCallback)));
  },

  /**
   * Asynchronously get all the add-ons in a particular install location.
   *
   * @param  aLocation
   *         The name of the install location
   * @param  aCallback
   *         A callback to pass the array of DBAddonInternals to
   */
  getAddonsInLocation(aLocation, aCallback) {
    this.getAddonList(aAddon => aAddon._installLocation.name == aLocation, aCallback);
  },

  /**
   * Asynchronously gets the add-on with the specified ID that is visible.
   *
   * @param  aId
   *         The ID of the add-on to retrieve
   * @param  aCallback
   *         A callback to pass the DBAddonInternal to
   */
  getVisibleAddonForID(aId, aCallback) {
    this.getAddon(aAddon => ((aAddon.id == aId) && aAddon.visible),
                  aCallback);
  },

  /**
   * Asynchronously gets the visible add-ons, optionally restricting by type.
   *
   * @param  aTypes
   *         An array of types to include or null to include all types
   * @param  aCallback
   *         A callback to pass the array of DBAddonInternals to
   */
  getVisibleAddons(aTypes, aCallback) {
    this.getAddonList(aAddon => (aAddon.visible &&
                                 (!aTypes || (aTypes.length == 0) ||
                                  (aTypes.indexOf(aAddon.type) > -1))),
                      aCallback);
  },

  /**
   * Synchronously gets all add-ons of a particular type(s).
   *
   * @param  aType, aType2, ...
   *         The type(s) of add-on to retrieve
   * @return an array of DBAddonInternals
   */
  getAddonsByType(...aTypes) {
    if (!this.addonDB) {
      // jank-tastic! Must synchronously load DB if the theme switches from
      // an XPI theme to a lightweight theme before the DB has loaded,
      // because we're called from sync XPIProvider.addonChanged
      logger.warn(`Synchronous load of XPI database due to ` +
                  `getAddonsByType([${aTypes.join(", ")}]) ` +
                  `Stack: ${Error().stack}`);
      AddonManagerPrivate.recordSimpleMeasure("XPIDB_lateOpen_byType", XPIProvider.runPhase);
      this.syncLoadDB(true);
    }

    return _filterDB(this.addonDB, aAddon => aTypes.includes(aAddon.type));
  },

  /**
   * Synchronously gets an add-on with a particular internalName.
   *
   * @param  aInternalName
   *         The internalName of the add-on to retrieve
   * @return a DBAddonInternal
   */
  getVisibleAddonForInternalName(aInternalName) {
    if (!this.addonDB) {
      // This may be called when the DB hasn't otherwise been loaded
      logger.warn(`Synchronous load of XPI database due to ` +
                  `getVisibleAddonForInternalName. Stack: ${Error().stack}`);
      AddonManagerPrivate.recordSimpleMeasure("XPIDB_lateOpen_forInternalName",
          XPIProvider.runPhase);
      this.syncLoadDB(true);
    }

    return _findAddon(this.addonDB,
                      aAddon => aAddon.visible &&
                                (aAddon.internalName == aInternalName));
  },

  /**
   * Asynchronously gets all add-ons with pending operations.
   *
   * @param  aTypes
   *         The types of add-ons to retrieve or null to get all types
   * @param  aCallback
   *         A callback to pass the array of DBAddonInternal to
   */
  getVisibleAddonsWithPendingOperations(aTypes, aCallback) {
    this.getAddonList(
        aAddon => (aAddon.visible &&
                   aAddon.pendingUninstall &&
                   (!aTypes || (aTypes.length == 0) || (aTypes.indexOf(aAddon.type) > -1))),
        aCallback);
  },

  /**
   * Asynchronously get an add-on by its Sync GUID.
   *
   * @param  aGUID
   *         Sync GUID of add-on to fetch
   * @param  aCallback
   *         A callback to pass the DBAddonInternal record to. Receives null
   *         if no add-on with that GUID is found.
   *
   */
  getAddonBySyncGUID(aGUID, aCallback) {
    this.getAddon(aAddon => aAddon.syncGUID == aGUID,
                  aCallback);
  },

  /**
   * Synchronously gets all add-ons in the database.
   * This is only called from the preference observer for the default
   * compatibility version preference, so we can return an empty list if
   * we haven't loaded the database yet.
   *
   * @return  an array of DBAddonInternals
   */
  getAddons() {
    if (!this.addonDB) {
      return [];
    }
    return _filterDB(this.addonDB, aAddon => true);
  },

  /**
   * Synchronously adds an AddonInternal's metadata to the database.
   *
   * @param  aAddon
   *         AddonInternal to add
   * @param  aPath
   *         The file path of the add-on
   * @return The DBAddonInternal that was added to the database
   */
  addAddonMetadata(aAddon, aPath) {
    if (!this.addonDB) {
      AddonManagerPrivate.recordSimpleMeasure("XPIDB_lateOpen_addMetadata",
          XPIProvider.runPhase);
      this.syncLoadDB(false);
    }

    let newAddon = new DBAddonInternal(aAddon);
    newAddon.path = aPath;
    this.addonDB.set(newAddon._key, newAddon);
    if (newAddon.visible) {
      this.makeAddonVisible(newAddon);
    }

    this.saveChanges();
    return newAddon;
  },

  /**
   * Synchronously updates an add-on's metadata in the database. Currently just
   * removes and recreates.
   *
   * @param  aOldAddon
   *         The DBAddonInternal to be replaced
   * @param  aNewAddon
   *         The new AddonInternal to add
   * @param  aPath
   *         The file path of the add-on
   * @return The DBAddonInternal that was added to the database
   */
  updateAddonMetadata(aOldAddon, aNewAddon, aPath) {
    this.removeAddonMetadata(aOldAddon);
    aNewAddon.syncGUID = aOldAddon.syncGUID;
    aNewAddon.installDate = aOldAddon.installDate;
    aNewAddon.applyBackgroundUpdates = aOldAddon.applyBackgroundUpdates;
    aNewAddon.foreignInstall = aOldAddon.foreignInstall;
    aNewAddon.seen = aOldAddon.seen;
    aNewAddon.active = (aNewAddon.visible && !aNewAddon.disabled && !aNewAddon.pendingUninstall);

    // addAddonMetadata does a saveChanges()
    return this.addAddonMetadata(aNewAddon, aPath);
  },

  /**
   * Synchronously removes an add-on from the database.
   *
   * @param  aAddon
   *         The DBAddonInternal being removed
   */
  removeAddonMetadata(aAddon) {
    this.addonDB.delete(aAddon._key);
    this.saveChanges();
  },

  updateXPIStates(addon) {
    let xpiState = XPIStates.getAddon(addon.location, addon.id);
    if (xpiState) {
      xpiState.syncWithDB(addon);
      XPIStates.save();
    }
  },

  /**
   * Synchronously marks a DBAddonInternal as visible marking all other
   * instances with the same ID as not visible.
   *
   * @param  aAddon
   *         The DBAddonInternal to make visible
   */
  makeAddonVisible(aAddon) {
    logger.debug("Make addon " + aAddon._key + " visible");
    for (let [, otherAddon] of this.addonDB) {
      if ((otherAddon.id == aAddon.id) && (otherAddon._key != aAddon._key)) {
        logger.debug("Hide addon " + otherAddon._key);
        otherAddon.visible = false;
        otherAddon.active = false;

        this.updateXPIStates(otherAddon);
      }
    }
    aAddon.visible = true;
    this.updateXPIStates(aAddon);
    this.saveChanges();
  },

  /**
   * Synchronously marks a given add-on ID visible in a given location,
   * instances with the same ID as not visible.
   *
   * @param  aAddon
   *         The DBAddonInternal to make visible
   */
  makeAddonLocationVisible(aId, aLocation) {
    logger.debug(`Make addon ${aId} visible in location ${aLocation}`);
    let result;
    for (let [, addon] of this.addonDB) {
      if (addon.id != aId) {
        continue;
      }
      if (addon.location == aLocation) {
        logger.debug("Reveal addon " + addon._key);
        addon.visible = true;
        addon.active = true;
        this.updateXPIStates(addon);
        result = addon;
      } else {
        logger.debug("Hide addon " + addon._key);
        addon.visible = false;
        addon.active = false;
        this.updateXPIStates(addon);
      }
    }
    this.saveChanges();
    return result;
  },

  /**
   * Synchronously sets properties for an add-on.
   *
   * @param  aAddon
   *         The DBAddonInternal being updated
   * @param  aProperties
   *         A dictionary of properties to set
   */
  setAddonProperties(aAddon, aProperties) {
    for (let key in aProperties) {
      aAddon[key] = aProperties[key];
    }
    this.saveChanges();
  },

  /**
   * Synchronously sets the Sync GUID for an add-on.
   * Only called when the database is already loaded.
   *
   * @param  aAddon
   *         The DBAddonInternal being updated
   * @param  aGUID
   *         GUID string to set the value to
   * @throws if another addon already has the specified GUID
   */
  setAddonSyncGUID(aAddon, aGUID) {
    // Need to make sure no other addon has this GUID
    function excludeSyncGUID(otherAddon) {
      return (otherAddon._key != aAddon._key) && (otherAddon.syncGUID == aGUID);
    }
    let otherAddon = _findAddon(this.addonDB, excludeSyncGUID);
    if (otherAddon) {
      throw new Error("Addon sync GUID conflict for addon " + aAddon._key +
          ": " + otherAddon._key + " already has GUID " + aGUID);
    }
    aAddon.syncGUID = aGUID;
    this.saveChanges();
  },

  /**
   * Synchronously updates an add-on's active flag in the database.
   *
   * @param  aAddon
   *         The DBAddonInternal to update
   */
  updateAddonActive(aAddon, aActive) {
    logger.debug("Updating active state for add-on " + aAddon.id + " to " + aActive);

    aAddon.active = aActive;
    this.saveChanges();
  },

  /**
   * Synchronously calculates and updates all the active flags in the database.
   */
  updateActiveAddons() {
    if (!this.addonDB) {
      logger.warn("updateActiveAddons called when DB isn't loaded");
      // force the DB to load
      AddonManagerPrivate.recordSimpleMeasure("XPIDB_lateOpen_updateActive",
          XPIProvider.runPhase);
      this.syncLoadDB(true);
    }
    logger.debug("Updating add-on states");
    for (let [, addon] of this.addonDB) {
      let newActive = (addon.visible && !addon.disabled && !addon.pendingUninstall);
      if (newActive != addon.active) {
        addon.active = newActive;
        this.saveChanges();
      }
    }
  },
};

this.XPIDatabaseReconcile = {
  /**
   * Returns a map of ID -> add-on. When the same add-on ID exists in multiple
   * install locations the highest priority location is chosen.
   */
  flattenByID(addonMap, hideLocation) {
    let map = new Map();

    for (let installLocation of XPIProvider.installLocations) {
      if (installLocation.name == hideLocation)
        continue;

      let locationMap = addonMap.get(installLocation.name);
      if (!locationMap)
        continue;

      for (let [id, addon] of locationMap) {
        if (!map.has(id))
          map.set(id, addon);
      }
    }

    return map;
  },

  /**
   * Finds the visible add-ons from the map.
   */
  getVisibleAddons(addonMap) {
    let map = new Map();

    for (let addons of addonMap.values()) {
      for (let [id, addon] of addons) {
        if (!addon.visible)
          continue;

        if (map.has(id)) {
          logger.warn("Previous database listed more than one visible add-on with id " + id);
          continue;
        }

        map.set(id, addon);
      }
    }

    return map;
  },

  /**
   * Called to add the metadata for an add-on in one of the install locations
   * to the database. This can be called in three different cases. Either an
   * add-on has been dropped into the location from outside of Firefox, or
   * an add-on has been installed through the application, or the database
   * has been upgraded or become corrupt and add-on data has to be reloaded
   * into it.
   *
   * @param  aInstallLocation
   *         The install location containing the add-on
   * @param  aId
   *         The ID of the add-on
   * @param  aAddonState
   *         The new state of the add-on
   * @param  aNewAddon
   *         The manifest for the new add-on if it has already been loaded
   * @param  aOldAppVersion
   *         The version of the application last run with this profile or null
   *         if it is a new profile or the version is unknown
   * @param  aOldPlatformVersion
   *         The version of the platform last run with this profile or null
   *         if it is a new profile or the version is unknown
   * @return a boolean indicating if flushing caches is required to complete
   *         changing this add-on
   */
  addMetadata(aInstallLocation, aId, aAddonState, aNewAddon, aOldAppVersion,
              aOldPlatformVersion) {
    logger.debug("New add-on " + aId + " installed in " + aInstallLocation.name);

    // If we had staged data for this add-on or we aren't recovering from a
    // corrupt database and we don't have migration data for this add-on then
    // this must be a new install.
    let isNewInstall = !!aNewAddon || !XPIDatabase.activeBundles;

    // If it's a new install and we haven't yet loaded the manifest then it
    // must be something dropped directly into the install location
    let isDetectedInstall = isNewInstall && !aNewAddon;

    // Load the manifest if necessary and sanity check the add-on ID
    try {
      if (!aNewAddon) {
        // Load the manifest from the add-on.
        let file = new nsIFile(aAddonState.path);
        aNewAddon = syncLoadManifestFromFile(file, aInstallLocation);
      }
      // The add-on in the manifest should match the add-on ID.
      if (aNewAddon.id != aId) {
        throw new Error("Invalid addon ID: expected addon ID " + aId +
                        ", found " + aNewAddon.id + " in manifest");
      }
    } catch (e) {
      logger.warn("addMetadata: Add-on " + aId + " is invalid", e);

      // Remove the invalid add-on from the install location if the install
      // location isn't locked
      if (aInstallLocation.isLinkedAddon(aId))
        logger.warn("Not uninstalling invalid item because it is a proxy file");
      else if (aInstallLocation.locked)
        logger.warn("Could not uninstall invalid item from locked install location");
      else
        aInstallLocation.uninstallAddon(aId);
      return null;
    }

    // Update the AddonInternal properties.
    aNewAddon.installDate = aAddonState.mtime;
    aNewAddon.updateDate = aAddonState.mtime;

    // Assume that add-ons in the system add-ons install location aren't
    // foreign and should default to enabled.
    aNewAddon.foreignInstall = isDetectedInstall &&
                               aInstallLocation.name != KEY_APP_SYSTEM_ADDONS &&
                               aInstallLocation.name != KEY_APP_SYSTEM_DEFAULTS;

    // appDisabled depends on whether the add-on is a foreignInstall so update
    aNewAddon.appDisabled = !isUsableAddon(aNewAddon);

    // The default theme is never a foreign install
    if (aNewAddon.type == "theme" && aNewAddon.internalName == DEFAULT_SKIN)
      aNewAddon.foreignInstall = false;

    if (isDetectedInstall && aNewAddon.foreignInstall) {
      // If the add-on is a foreign install and is in a scope where add-ons
      // that were dropped in should default to disabled then disable it
      let disablingScopes = Services.prefs.getIntPref(PREF_EM_AUTO_DISABLED_SCOPES, 0);
      if (aInstallLocation.scope & disablingScopes) {
        logger.warn("Disabling foreign installed add-on " + aNewAddon.id + " in "
            + aInstallLocation.name);
        aNewAddon.userDisabled = true;
        aNewAddon.seen = false;
      }
    }

    return XPIDatabase.addAddonMetadata(aNewAddon, aAddonState.path);
  },

  /**
   * Called when an add-on has been removed.
   *
   * @param  aOldAddon
   *         The AddonInternal as it appeared the last time the application
   *         ran
   * @return a boolean indicating if flushing caches is required to complete
   *         changing this add-on
   */
  removeMetadata(aOldAddon) {
    // This add-on has disappeared
    logger.debug("Add-on " + aOldAddon.id + " removed from " + aOldAddon.location);
    XPIDatabase.removeAddonMetadata(aOldAddon);
  },

  /**
   * Updates an add-on's metadata and determines. This is called when either the
   * add-on's install directory path or last modified time has changed.
   *
   * @param  aInstallLocation
   *         The install location containing the add-on
   * @param  aOldAddon
   *         The AddonInternal as it appeared the last time the application
   *         ran
   * @param  aAddonState
   *         The new state of the add-on
   * @param  aNewAddon
   *         The manifest for the new add-on if it has already been loaded
   * @return a boolean indicating if flushing caches is required to complete
   *         changing this add-on
   */
  updateMetadata(aInstallLocation, aOldAddon, aAddonState, aNewAddon) {
    logger.debug("Add-on " + aOldAddon.id + " modified in " + aInstallLocation.name);

    try {
      // If there isn't an updated install manifest for this add-on then load it.
      if (!aNewAddon) {
        let file = new nsIFile(aAddonState.path);
        aNewAddon = syncLoadManifestFromFile(file, aInstallLocation, aOldAddon);
      }

      // The ID in the manifest that was loaded must match the ID of the old
      // add-on.
      if (aNewAddon.id != aOldAddon.id)
        throw new Error("Incorrect id in install manifest for existing add-on " + aOldAddon.id);
    } catch (e) {
      logger.warn("updateMetadata: Add-on " + aOldAddon.id + " is invalid", e);
      XPIDatabase.removeAddonMetadata(aOldAddon);
      XPIStates.removeAddon(aOldAddon.location, aOldAddon.id);
      if (!aInstallLocation.locked)
        aInstallLocation.uninstallAddon(aOldAddon.id);
      else
        logger.warn("Could not uninstall invalid item from locked install location");

      return null;
    }

    // Set the additional properties on the new AddonInternal
    aNewAddon.updateDate = aAddonState.mtime;

    // Update the database
    return XPIDatabase.updateAddonMetadata(aOldAddon, aNewAddon, aAddonState.path);
  },

  /**
   * Updates an add-on's path for when the add-on has moved in the
   * filesystem but hasn't changed in any other way.
   *
   * @param  aInstallLocation
   *         The install location containing the add-on
   * @param  aOldAddon
   *         The AddonInternal as it appeared the last time the application
   *         ran
   * @param  aAddonState
   *         The new state of the add-on
   * @return a boolean indicating if flushing caches is required to complete
   *         changing this add-on
   */
  updatePath(aInstallLocation, aOldAddon, aAddonState) {
    logger.debug("Add-on " + aOldAddon.id + " moved to " + aAddonState.path);
    aOldAddon.path = aAddonState.path;
    aOldAddon._sourceBundle = new nsIFile(aAddonState.path);

    return aOldAddon;
  },

  /**
   * Called when no change has been detected for an add-on's metadata but the
   * application has changed so compatibility may have changed.
   *
   * @param  aInstallLocation
   *         The install location containing the add-on
   * @param  aOldAddon
   *         The AddonInternal as it appeared the last time the application
   *         ran
   * @param  aAddonState
   *         The new state of the add-on
   * @param  aReloadMetadata
   *         A boolean which indicates whether metadata should be reloaded from
   *         the addon manifests. Default to false.
   * @return the new addon.
   */
  updateCompatibility(aInstallLocation, aOldAddon, aAddonState, aReloadMetadata) {
    logger.debug("Updating compatibility for add-on " + aOldAddon.id + " in " + aInstallLocation.name);

    let checkSigning = aOldAddon.signedState === undefined && ADDON_SIGNING &&
                       SIGNED_TYPES.has(aOldAddon.type);

    let manifest = null;
    if (checkSigning || aReloadMetadata) {
      try {
        let file = new nsIFile(aAddonState.path);
        manifest = syncLoadManifestFromFile(file, aInstallLocation);
      } catch (err) {
        // If we can no longer read the manifest, it is no longer compatible.
        aOldAddon.brokenManifest = true;
        aOldAddon.appDisabled = true;
        return aOldAddon;
      }
    }

    // If updating from a version of the app that didn't support signedState
    // then update that property now
    if (checkSigning) {
      aOldAddon.signedState = manifest.signedState;
    }

    // May be updating from a version of the app that didn't support all the
    // properties of the currently-installed add-ons.
    if (aReloadMetadata) {
      // Avoid re-reading these properties from manifest,
      // use existing addon instead.
      // TODO - consider re-scanning for targetApplications.
      let remove = ["syncGUID", "foreignInstall", "visible", "active",
                    "userDisabled", "applyBackgroundUpdates", "sourceURI",
                    "releaseNotesURI", "targetApplications"];

      let props = PROP_JSON_FIELDS.filter(a => !remove.includes(a));
      copyProperties(manifest, props, aOldAddon);
    }

    aOldAddon.appDisabled = !isUsableAddon(aOldAddon);

    return aOldAddon;
  },

  /**
   * Compares the add-ons that are currently installed to those that were
   * known to be installed when the application last ran and applies any
   * changes found to the database. Also sends "startupcache-invalidate" signal to
   * observerservice if it detects that data may have changed.
   * Always called after XPIProviderUtils.js and extensions.json have been loaded.
   *
   * @param  aManifests
   *         A dictionary of cached AddonInstalls for add-ons that have been
   *         installed
   * @param  aUpdateCompatibility
   *         true to update add-ons appDisabled property when the application
   *         version has changed
   * @param  aOldAppVersion
   *         The version of the application last run with this profile or null
   *         if it is a new profile or the version is unknown
   * @param  aOldPlatformVersion
   *         The version of the platform last run with this profile or null
   *         if it is a new profile or the version is unknown
   * @param  aSchemaChange
   *         The schema has changed and all add-on manifests should be re-read.
   * @return a boolean indicating if a change requiring flushing the caches was
   *         detected
   */
  processFileChanges(aManifests, aUpdateCompatibility, aOldAppVersion, aOldPlatformVersion,
                     aSchemaChange) {
    let loadedManifest = (aInstallLocation, aId) => {
      if (!(aInstallLocation.name in aManifests))
        return null;
      if (!(aId in aManifests[aInstallLocation.name]))
        return null;
      return aManifests[aInstallLocation.name][aId];
    };

    // Add-ons loaded from the database can have an uninitialized _sourceBundle
    // if the path was invalid. Swallow that error and say they don't exist.
    let exists = (aAddon) => {
      try {
        return aAddon._sourceBundle.exists();
      } catch (e) {
        if (e.result == Cr.NS_ERROR_NOT_INITIALIZED)
          return false;
        throw e;
      }
    };

    // Get the previous add-ons from the database and put them into maps by location
    let previousAddons = new Map();
    for (let a of XPIDatabase.getAddons()) {
      let locationAddonMap = previousAddons.get(a.location);
      if (!locationAddonMap) {
        locationAddonMap = new Map();
        previousAddons.set(a.location, locationAddonMap);
      }
      locationAddonMap.set(a.id, a);
    }

    // Keep track of add-ons whose blocklist status may have changed. We'll check this
    // after everything else.
    let addonsToCheckAgainstBlocklist = [];

    // Build the list of current add-ons into similar maps. When add-ons are still
    // present we re-use the add-on objects from the database and update their
    // details directly
    let currentAddons = new Map();
    for (let installLocation of XPIProvider.installLocations) {
      let locationAddonMap = new Map();
      currentAddons.set(installLocation.name, locationAddonMap);

      // Get all the on-disk XPI states for this location, and keep track of which
      // ones we see in the database.
      let states = XPIStates.getLocation(installLocation.name);

      // Iterate through the add-ons installed the last time the application
      // ran
      let dbAddons = previousAddons.get(installLocation.name);
      if (dbAddons) {
        for (let [id, oldAddon] of dbAddons) {
          // Check if the add-on is still installed
          let xpiState = states && states.get(id);
          if (xpiState) {
            // Here the add-on was present in the database and on disk
            recordAddonTelemetry(oldAddon);

            // Check if the add-on has been changed outside the XPI provider
            if (oldAddon.updateDate != xpiState.mtime) {
              // Did time change in the wrong direction?
              if (xpiState.mtime < oldAddon.updateDate) {
                XPIProvider.setTelemetry(oldAddon.id, "olderFile", {
                  mtime: xpiState.mtime,
                  oldtime: oldAddon.updateDate
                });
              }
            }

            let oldPath = oldAddon.path || descriptorToPath(oldAddon.descriptor);

            // The add-on has changed if the modification time has changed, if
            // we have an updated manifest for it, or if the schema version has
            // changed.
            //
            // Also reload the metadata for add-ons in the application directory
            // when the application version has changed.
            let newAddon = loadedManifest(installLocation, id);
            if (newAddon || oldAddon.updateDate != xpiState.mtime ||
                (aUpdateCompatibility && (installLocation.name == KEY_APP_GLOBAL ||
                                          installLocation.name == KEY_APP_SYSTEM_DEFAULTS))) {
              newAddon = this.updateMetadata(installLocation, oldAddon, xpiState, newAddon);
            } else if (oldPath != xpiState.path) {
              newAddon = this.updatePath(installLocation, oldAddon, xpiState);
            } else if (aUpdateCompatibility || aSchemaChange) {
              // Check compatility when the application version and/or schema
              // version has changed. A schema change also reloads metadata from
              // the manifests.
              newAddon = this.updateCompatibility(installLocation, oldAddon, xpiState,
                                                  aSchemaChange);
              // We need to do a blocklist check later, but the add-on may have changed by then.
              // Avoid storing the current copy and just get one when we need one instead.
              addonsToCheckAgainstBlocklist.push(newAddon.id);
            } else {
              // No change
              newAddon = oldAddon;
            }

            if (newAddon)
              locationAddonMap.set(newAddon.id, newAddon);
          } else {
            // The add-on is in the DB, but not in xpiState (and thus not on disk).
            this.removeMetadata(oldAddon);
          }
        }
      }

      // Any add-on in our current location that we haven't seen needs to
      // be added to the database.
      // Get the migration data for this install location so we can include that as
      // we add, in case this is a database upgrade or rebuild.
      let locMigrateData = {};
      if (XPIDatabase.migrateData && installLocation.name in XPIDatabase.migrateData)
        locMigrateData = XPIDatabase.migrateData[installLocation.name];

      if (states) {
        for (let [id, xpiState] of states) {
          if (locationAddonMap.has(id))
            continue;
          let migrateData = id in locMigrateData ? locMigrateData[id] : null;
          let newAddon = loadedManifest(installLocation, id);
          let addon = this.addMetadata(installLocation, id, xpiState, newAddon,
                                       aOldAppVersion, aOldPlatformVersion, migrateData);
          if (addon)
            locationAddonMap.set(addon.id, addon);
        }
      }
    }

    // previousAddons may contain locations where the database contains add-ons
    // but the browser is no longer configured to use that location. The metadata
    // for those add-ons must be removed from the database.
    for (let [locationName, addons] of previousAddons) {
      if (!currentAddons.has(locationName)) {
        for (let oldAddon of addons.values())
          this.removeMetadata(oldAddon);
      }
    }

    // Validate the updated system add-ons
    let systemAddonLocation = XPIProvider.installLocationsByName[KEY_APP_SYSTEM_ADDONS];
    let addons = currentAddons.get(KEY_APP_SYSTEM_ADDONS) || new Map();

    let hideLocation;

    if (!systemAddonLocation.isValid(addons)) {
      // Hide the system add-on updates if any are invalid.
      logger.info("One or more updated system add-ons invalid, falling back to defaults.");
      hideLocation = KEY_APP_SYSTEM_ADDONS;
    }

    let previousVisible = this.getVisibleAddons(previousAddons);
    let currentVisible = this.flattenByID(currentAddons, hideLocation);

    // Pass over the new set of visible add-ons, record any changes that occurred
    // during startup and call bootstrap install/uninstall scripts as necessary
    for (let [id, currentAddon] of currentVisible) {
      let previousAddon = previousVisible.get(id);

      // Note if any visible add-on is not in the application install location
      if (currentAddon._installLocation.name != KEY_APP_GLOBAL)
        XPIProvider.allAppGlobal = false;

      let isActive = !currentAddon.disabled && !currentAddon.pendingUninstall;
      let wasActive = previousAddon ? previousAddon.active : currentAddon.active;

      if (!previousAddon) {
        // If we had a manifest for this add-on it was a staged install and
        // so wasn't something recovered from a corrupt database
        let wasStaged = !!loadedManifest(currentAddon._installLocation, id);

        // We might be recovering from a corrupt database, if so use the
        // list of known active add-ons to update the new add-on
        if (!wasStaged && XPIDatabase.activeBundles) {
          // For themes we know which is active by the current skin setting
          if (currentAddon.type == "theme")
            isActive = currentAddon.internalName == DEFAULT_SKIN;
          else
            isActive = XPIDatabase.activeBundles.includes(currentAddon.path);

          if (currentAddon.type == "webextension-theme")
            currentAddon.userDisabled = !isActive;

          // If the add-on wasn't active and it isn't already disabled in some way
          // then it was probably either softDisabled or userDisabled
          if (!isActive && !currentAddon.disabled) {
            // If the add-on is softblocked then assume it is softDisabled
            if (currentAddon.blocklistState == Blocklist.STATE_SOFTBLOCKED)
              currentAddon.softDisabled = true;
            else
              currentAddon.userDisabled = true;
          }
        } else {
          // This is a new install
          if (currentAddon.foreignInstall)
            AddonManagerPrivate.addStartupChange(AddonManager.STARTUP_CHANGE_INSTALLED, id);

          if (currentAddon.bootstrap) {
            AddonManagerPrivate.addStartupChange(AddonManager.STARTUP_CHANGE_INSTALLED, id);
            // Visible bootstrapped add-ons need to have their install method called
            XPIProvider.callBootstrapMethod(currentAddon, currentAddon._sourceBundle,
                                            "install", BOOTSTRAP_REASONS.ADDON_INSTALL);
            if (!isActive)
              XPIProvider.unloadBootstrapScope(currentAddon.id);
          }
        }
      } else {
        if (previousAddon !== currentAddon) {
          // This is an add-on that has changed, either the metadata was reloaded
          // or the version in a different location has become visible
          AddonManagerPrivate.addStartupChange(AddonManager.STARTUP_CHANGE_CHANGED, id);

          let installReason = Services.vc.compare(previousAddon.version, currentAddon.version) < 0 ?
                              BOOTSTRAP_REASONS.ADDON_UPGRADE :
                              BOOTSTRAP_REASONS.ADDON_DOWNGRADE;

          // If the previous add-on was in a different path, bootstrapped
          // and still exists then call its uninstall method.
          if (previousAddon.bootstrap && previousAddon._installLocation &&
              exists(previousAddon) &&
              currentAddon._sourceBundle.path != previousAddon._sourceBundle.path) {

            XPIProvider.callBootstrapMethod(previousAddon, previousAddon._sourceBundle,
                                            "uninstall", installReason,
                                            { newVersion: currentAddon.version });
            XPIProvider.unloadBootstrapScope(previousAddon.id);
          }

          // Make sure to flush the cache when an old add-on has gone away
          flushChromeCaches();

          if (currentAddon.bootstrap) {
            // Visible bootstrapped add-ons need to have their install method called
            let file = currentAddon._sourceBundle.clone();
            XPIProvider.callBootstrapMethod(currentAddon, file,
                                            "install", installReason,
                                            { oldVersion: previousAddon.version });
            if (currentAddon.disabled)
              XPIProvider.unloadBootstrapScope(currentAddon.id);
          }
        }

        if (isActive != wasActive) {
          let change = isActive ? AddonManager.STARTUP_CHANGE_ENABLED
                                : AddonManager.STARTUP_CHANGE_DISABLED;
          AddonManagerPrivate.addStartupChange(change, id);
        }
      }

      XPIDatabase.makeAddonVisible(currentAddon);
      currentAddon.active = isActive;
    }

    // Pass over the set of previously visible add-ons that have now gone away
    // and record the change.
    for (let [id, previousAddon] of previousVisible) {
      if (currentVisible.has(id))
        continue;

      // This add-on vanished

      // If the previous add-on was bootstrapped and still exists then call its
      // uninstall method.
      if (previousAddon.bootstrap && exists(previousAddon)) {
        XPIProvider.callBootstrapMethod(previousAddon, previousAddon._sourceBundle,
                                        "uninstall", BOOTSTRAP_REASONS.ADDON_UNINSTALL);
        XPIProvider.unloadBootstrapScope(previousAddon.id);
      }
      AddonManagerPrivate.addStartupChange(AddonManager.STARTUP_CHANGE_UNINSTALLED, id);
      XPIStates.removeAddon(previousAddon.location, id);

      // Make sure to flush the cache when an old add-on has gone away
      flushChromeCaches();
    }

    // Make sure add-ons from hidden locations are marked invisible and inactive
    let locationAddonMap = currentAddons.get(hideLocation);
    if (locationAddonMap) {
      for (let addon of locationAddonMap.values()) {
        addon.visible = false;
        addon.active = false;
      }
    }

    // Finally update XPIStates to match everything
    for (let [locationName, locationAddonMap] of currentAddons) {
      for (let [id, addon] of locationAddonMap) {
        let xpiState = XPIStates.getAddon(locationName, id);
        xpiState.syncWithDB(addon);
      }
    }
    XPIStates.save();

    // Clear out any cached migration data.
    XPIDatabase.migrateData = null;
    XPIDatabase.saveChanges();

    // Do some blocklist checks. These will happen after we've just saved everything,
    // because they're async and depend on the blocklist loading. When we're done, save
    // the data if any of the add-ons' blocklist state has changed.
    AddonManager.shutdown.addBlocker(
      "Update add-on blocklist state into add-on DB",
      (async () => {
        // Avoid querying the AddonManager immediately to give startup a chance
        // to complete.
        await Promise.resolve();
        let addons = await AddonManager.getAddonsByIDs(addonsToCheckAgainstBlocklist);
        await Promise.all(addons.map(addon => {
          if (addon) {
            return addon.updateBlocklistState({updateDatabase: false});
          }
          return null;
        }));
        XPIDatabase.saveChanges();
      })().catch(Cu.reportError)
    );

    return true;
  },
};
