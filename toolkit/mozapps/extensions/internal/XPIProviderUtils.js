/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cr = Components.results;
var Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/AddonManager.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AddonRepository",
                                  "resource://gre/modules/addons/AddonRepository.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DeferredSave",
                                  "resource://gre/modules/DeferredSave.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "Blocklist",
                                   "@mozilla.org/extensions/blocklist;1",
                                   Ci.nsIBlocklistService);

Cu.import("resource://gre/modules/Log.jsm");
const LOGGER_ID = "addons.xpi-utils";

// Create a new logger for use by the Addons XPI Provider Utils
// (Requires AddonManager.jsm)
var logger = Log.repository.getLogger(LOGGER_ID);

const KEY_PROFILEDIR                  = "ProfD";
const FILE_DATABASE                   = "extensions.sqlite";
const FILE_JSON_DB                    = "extensions.json";
const FILE_OLD_DATABASE               = "extensions.rdf";
const FILE_XPI_ADDONS_LIST            = "extensions.ini";

// The value for this is in Makefile.in
#expand const DB_SCHEMA               = __MOZ_EXTENSIONS_DB_SCHEMA__;

// The last version of DB_SCHEMA implemented in SQLITE
const LAST_SQLITE_DB_SCHEMA           = 14;
const PREF_DB_SCHEMA                  = "extensions.databaseSchema";
const PREF_PENDING_OPERATIONS         = "extensions.pendingOperations";
const PREF_EM_ENABLED_ADDONS          = "extensions.enabledAddons";
const PREF_EM_DSS_ENABLED             = "extensions.dss.enabled";
const PREF_EM_AUTO_DISABLED_SCOPES    = "extensions.autoDisableScopes";

const KEY_APP_PROFILE                 = "app-profile";
const KEY_APP_SYSTEM_ADDONS           = "app-system-addons";
const KEY_APP_SYSTEM_DEFAULTS         = "app-system-defaults";
const KEY_APP_GLOBAL                  = "app-global";

// Properties that only exist in the database
const DB_METADATA        = ["syncGUID",
                            "installDate",
                            "updateDate",
                            "size",
                            "sourceURI",
                            "releaseNotesURI",
                            "applyBackgroundUpdates"];
const DB_BOOL_METADATA   = ["visible", "active", "userDisabled", "appDisabled",
                            "pendingUninstall", "bootstrap", "skinnable",
                            "softDisabled", "isForeignInstall",
                            "hasBinaryComponents", "strictCompatibility"];

// Properties to save in JSON file
const PROP_JSON_FIELDS = ["id", "syncGUID", "location", "version", "type",
                          "internalName", "updateURL", "updateKey", "optionsURL",
                          "optionsType", "aboutURL", "iconURL", "icon64URL",
                          "defaultLocale", "visible", "active", "userDisabled",
                          "appDisabled", "pendingUninstall", "descriptor", "installDate",
                          "updateDate", "applyBackgroundUpdates", "bootstrap",
                          "skinnable", "size", "sourceURI", "releaseNotesURI",
                          "softDisabled", "foreignInstall", "hasBinaryComponents",
                          "strictCompatibility", "locales", "targetApplications",
                          "targetPlatforms", "multiprocessCompatible", "signedState"];

// Properties that should be migrated where possible from an old database. These
// shouldn't include properties that can be read directly from install.rdf files
// or calculated
const DB_MIGRATE_METADATA= ["installDate", "userDisabled", "softDisabled",
                            "sourceURI", "applyBackgroundUpdates",
                            "releaseNotesURI", "foreignInstall", "syncGUID"];

// Time to wait before async save of XPI JSON database, in milliseconds
const ASYNC_SAVE_DELAY_MS = 20;

const PREFIX_ITEM_URI                 = "urn:mozilla:item:";
const RDFURI_ITEM_ROOT                = "urn:mozilla:item:root"
const PREFIX_NS_EM                    = "http://www.mozilla.org/2004/em-rdf#";

XPCOMUtils.defineLazyServiceGetter(this, "gRDF", "@mozilla.org/rdf/rdf-service;1",
                                   Ci.nsIRDFService);

function EM_R(aProperty) {
  return gRDF.GetResource(PREFIX_NS_EM + aProperty);
}

/**
 * Converts an RDF literal, resource or integer into a string.
 *
 * @param  aLiteral
 *         The RDF object to convert
 * @return a string if the object could be converted or null
 */
function getRDFValue(aLiteral) {
  if (aLiteral instanceof Ci.nsIRDFLiteral)
    return aLiteral.Value;
  if (aLiteral instanceof Ci.nsIRDFResource)
    return aLiteral.Value;
  if (aLiteral instanceof Ci.nsIRDFInt)
    return aLiteral.Value;
  return null;
}

/**
 * Gets an RDF property as a string
 *
 * @param  aDs
 *         The RDF datasource to read the property from
 * @param  aResource
 *         The RDF resource to read the property from
 * @param  aProperty
 *         The property to read
 * @return a string if the property existed or null
 */
function getRDFProperty(aDs, aResource, aProperty) {
  return getRDFValue(aDs.GetTarget(aResource, EM_R(aProperty), true));
}

/**
 * Asynchronously fill in the _repositoryAddon field for one addon
 */
function getRepositoryAddon(aAddon, aCallback) {
  if (!aAddon) {
    aCallback(aAddon);
    return;
  }
  function completeAddon(aRepositoryAddon) {
    aAddon._repositoryAddon = aRepositoryAddon;
    aAddon.compatibilityOverrides = aRepositoryAddon ?
                                      aRepositoryAddon.compatibilityOverrides :
                                      null;
    aCallback(aAddon);
  }
  AddonRepository.getCachedAddonByID(aAddon.id, completeAddon);
}

/**
 * Wrap an API-supplied function in an exception handler to make it safe to call
 */
function makeSafe(aCallback) {
  return function(...aArgs) {
    try {
      aCallback(...aArgs);
    }
    catch(ex) {
      logger.warn("XPI Database callback failed", ex);
    }
  }
}

/**
 * A helper method to asynchronously call a function on an array
 * of objects, calling a callback when function(x) has been gathered
 * for every element of the array.
 * WARNING: not currently error-safe; if the async function does not call
 * our internal callback for any of the array elements, asyncMap will not
 * call the callback parameter.
 *
 * @param  aObjects
 *         The array of objects to process asynchronously
 * @param  aMethod
 *         Function with signature function(object, function aCallback(f_of_object))
 * @param  aCallback
 *         Function with signature f([aMethod(object)]), called when all values
 *         are available
 */
function asyncMap(aObjects, aMethod, aCallback) {
  var resultsPending = aObjects.length;
  var results = []
  if (resultsPending == 0) {
    aCallback(results);
    return;
  }

  function asyncMap_gotValue(aIndex, aValue) {
    results[aIndex] = aValue;
    if (--resultsPending == 0) {
      aCallback(results);
    }
  }

  aObjects.map(function asyncMap_each(aObject, aIndex, aArray) {
    try {
      aMethod(aObject, function asyncMap_callback(aResult) {
        asyncMap_gotValue(aIndex, aResult);
      });
    }
    catch (e) {
      logger.warn("Async map function failed", e);
      asyncMap_gotValue(aIndex, undefined);
    }
  });
}

/**
 * A generator to synchronously return result rows from an mozIStorageStatement.
 *
 * @param  aStatement
 *         The statement to execute
 */
function resultRows(aStatement) {
  try {
    while (stepStatement(aStatement))
      yield aStatement.row;
  }
  finally {
    aStatement.reset();
  }
}

/**
 * A helper function to log an SQL error.
 *
 * @param  aError
 *         The storage error code associated with the error
 * @param  aErrorString
 *         An error message
 */
function logSQLError(aError, aErrorString) {
  logger.error("SQL error " + aError + ": " + aErrorString);
}

/**
 * A helper function to log any errors that occur during async statements.
 *
 * @param  aError
 *         A mozIStorageError to log
 */
function asyncErrorLogger(aError) {
  logSQLError(aError.result, aError.message);
}

/**
 * A helper function to step a statement synchronously and log any error that
 * occurs.
 *
 * @param  aStatement
 *         A mozIStorageStatement to execute
 */
function stepStatement(aStatement) {
  try {
    return aStatement.executeStep();
  }
  catch (e) {
    logSQLError(XPIDatabase.connection.lastError,
                XPIDatabase.connection.lastErrorString);
    throw e;
  }
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
    aTarget[aProp] = aObject[aProp];
  });
  return aTarget;
}

/**
 * Copies properties from a mozIStorageRow to an object. If no target object is
 * passed a new object will be created and returned.
 *
 * @param  aRow
 *         A mozIStorageRow to copy from
 * @param  aProperties
 *         An array of properties to be copied
 * @param  aTarget
 *         An optional target object to copy the properties to
 * @return the object that the properties were copied onto
 */
function copyRowProperties(aRow, aProperties, aTarget) {
  if (!aTarget)
    aTarget = {};
  aProperties.forEach(function(aProp) {
    aTarget[aProp] = aRow.getResultByName(aProp);
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
  copyProperties(aLoaded, PROP_JSON_FIELDS, this);

  if (aLoaded._installLocation) {
    this._installLocation = aLoaded._installLocation;
    this.location = aLoaded._installLocation._name;
  }
  else if (aLoaded.location) {
    this._installLocation = XPIProvider.installLocationsByName[this.location];
  }

  this._key = this.location + ":" + this.id;

  if (aLoaded._sourceBundle) {
    this._sourceBundle = aLoaded._sourceBundle;
  }
  else if (aLoaded.descriptor) {
    this._sourceBundle = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    this._sourceBundle.persistentDescriptor = aLoaded.descriptor;
  }
  else {
    throw new Error("Expected passed argument to contain a descriptor");
  }

  XPCOMUtils.defineLazyGetter(this, "pendingUpgrade",
    function DBA_pendingUpgradeGetter() {
      for (let install of XPIProvider.installs) {
        if (install.state == AddonManager.STATE_INSTALLED &&
            !(install.addon.inDatabase) &&
            install.addon.id == this.id &&
            install.installLocation == this._installLocation) {
          delete this.pendingUpgrade;
          return this.pendingUpgrade = install.addon;
        }
      };
      return null;
    });
}

function DBAddonInternalPrototype()
{
  this.applyCompatibilityUpdate =
    function(aUpdate, aSyncCompatibility) {
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
      if (aUpdate.multiprocessCompatible !== undefined &&
          aUpdate.multiprocessCompatible != this.multiprocessCompatible) {
        this.multiprocessCompatible = aUpdate.multiprocessCompatible;
        XPIDatabase.saveChanges();
      }

      if (wasCompatible != this.isCompatible)
        XPIProvider.updateAddonDisabledState(this);
    };

  this.toJSON =
    function() {
      return copyProperties(this, PROP_JSON_FIELDS);
    };

  Object.defineProperty(this, "inDatabase",
                        { get: function() { return true; },
                          enumerable: true,
                          configurable: true });
}
DBAddonInternalPrototype.prototype = AddonInternal.prototype;

DBAddonInternal.prototype = new DBAddonInternalPrototype();

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
  return [for (addon of addonDB.values()) if (aFilter(addon)) addon];
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

  // Saved error object if we fail to read an existing database
  _loadError: null,

  // Error reported by our most recent attempt to read or write the database, if any
  get lastError() {
    if (this._loadError)
      return this._loadError;
    if (this._deferredSave)
      return this._deferredSave.lastError;
    return null;
  },

  /**
   * Mark the current stored data dirty, and schedule a flush to disk
   */
  saveChanges: function() {
    if (!this.initialized) {
      throw new Error("Attempt to use XPI database when it is not initialized");
    }

    if (XPIProvider._closing) {
      // use an Error here so we get a stack trace.
      let err = new Error("XPI database modified after shutdown began");
      logger.warn(err);
      AddonManagerPrivate.recordSimpleMeasure("XPIDB_late_stack", Log.stackTrace(err));
    }

    if (!this._deferredSave) {
      this._deferredSave = new DeferredSave(this.jsonFile.path,
                                            () => JSON.stringify(this),
                                            ASYNC_SAVE_DELAY_MS);
    }

    let promise = this._deferredSave.saveChanges();
    if (!this._schemaVersionSet) {
      this._schemaVersionSet = true;
      promise = promise.then(
        count => {
          // Update the XPIDB schema version preference the first time we successfully
          // save the database.
          logger.debug("XPI Database saved, setting schema version preference to " + DB_SCHEMA);
          Services.prefs.setIntPref(PREF_DB_SCHEMA, DB_SCHEMA);
          // Reading the DB worked once, so we don't need the load error
          this._loadError = null;
        },
        error => {
          // Need to try setting the schema version again later
          this._schemaVersionSet = false;
          // this._deferredSave.lastError has the most recent error so we don't
          // need this any more
          this._loadError = null;

          throw error;
        });
    }

    promise.catch(error => {
      logger.warn("Failed to save XPI database", error);
    });
  },

  flush: function() {
    // handle the "in memory only" and "saveChanges never called" cases
    if (!this._deferredSave) {
      return Promise.resolve(0);
    }

    return this._deferredSave.flush();
  },

  /**
   * Converts the current internal state of the XPI addon database to
   * a JSON.stringify()-ready structure
   */
  toJSON: function() {
    if (!this.addonDB) {
      // We never loaded the database?
      throw new Error("Attempt to save database without loading it first");
    }

    let toSave = {
      schemaVersion: DB_SCHEMA,
      addons: [...this.addonDB.values()]
    };
    return toSave;
  },

  /**
   * Pull upgrade information from an existing SQLITE database
   *
   * @return false if there is no SQLITE database
   *         true and sets this.migrateData to null if the SQLITE DB exists
   *              but does not contain useful information
   *         true and sets this.migrateData to
   *              {location: {id1:{addon1}, id2:{addon2}}, location2:{...}, ...}
   *              if there is useful information
   */
  getMigrateDataFromSQLITE: function XPIDB_getMigrateDataFromSQLITE() {
    let connection = null;
    let dbfile = FileUtils.getFile(KEY_PROFILEDIR, [FILE_DATABASE], true);
    // Attempt to open the database
    try {
      connection = Services.storage.openUnsharedDatabase(dbfile);
    }
    catch (e) {
      logger.warn("Failed to open sqlite database " + dbfile.path + " for upgrade", e);
      return null;
    }
    logger.debug("Migrating data from sqlite");
    let migrateData = this.getMigrateDataFromDatabase(connection);
    connection.close();
    return migrateData;
  },

  /**
   * Synchronously opens and reads the database file, upgrading from old
   * databases or making a new DB if needed.
   *
   * The possibilities, in order of priority, are:
   * 1) Perfectly good, up to date database
   * 2) Out of date JSON database needs to be upgraded => upgrade
   * 3) JSON database exists but is mangled somehow => build new JSON
   * 4) no JSON DB, but a useable SQLITE db we can upgrade from => upgrade
   * 5) useless SQLITE DB => build new JSON
   * 6) useable RDF DB => upgrade
   * 7) useless RDF DB => build new JSON
   * 8) Nothing at all => build new JSON
   * @param  aRebuildOnError
   *         A boolean indicating whether add-on information should be loaded
   *         from the install locations if the database needs to be rebuilt.
   *         (if false, caller is XPIProvider.checkForChanges() which will rebuild)
   */
  syncLoadDB: function XPIDB_syncLoadDB(aRebuildOnError) {
    this.migrateData = null;
    let fstream = null;
    let data = "";
    try {
      let readTimer = AddonManagerPrivate.simpleTimer("XPIDB_syncRead_MS");
      logger.debug("Opening XPI database " + this.jsonFile.path);
      fstream = Components.classes["@mozilla.org/network/file-input-stream;1"].
              createInstance(Components.interfaces.nsIFileInputStream);
      fstream.init(this.jsonFile, -1, 0, 0);
      let cstream = null;
      try {
        cstream = Components.classes["@mozilla.org/intl/converter-input-stream;1"].
                createInstance(Components.interfaces.nsIConverterInputStream);
        cstream.init(fstream, "UTF-8", 0, 0);

        let str = {};
        let read = 0;
        do {
          read = cstream.readString(0xffffffff, str); // read as much as we can and put it in str.value
          data += str.value;
        } while (read != 0);

        readTimer.done();
        this.parseDB(data, aRebuildOnError);
      }
      catch(e) {
        logger.error("Failed to load XPI JSON data from profile", e);
        let rebuildTimer = AddonManagerPrivate.simpleTimer("XPIDB_rebuildReadFailed_MS");
        this.rebuildDatabase(aRebuildOnError);
        rebuildTimer.done();
      }
      finally {
        if (cstream)
          cstream.close();
      }
    }
    catch (e) {
      if (e.result === Cr.NS_ERROR_FILE_NOT_FOUND) {
        this.upgradeDB(aRebuildOnError);
      }
      else {
        this.rebuildUnreadableDB(e, aRebuildOnError);
      }
    }
    finally {
      if (fstream)
        fstream.close();
    }
    // If an async load was also in progress, resolve that promise with our DB;
    // otherwise create a resolved promise
    if (this._dbPromise) {
      AddonManagerPrivate.recordSimpleMeasure("XPIDB_overlapped_load", 1);
      this._dbPromise.resolve(this.addonDB);
    }
    else
      this._dbPromise = Promise.resolve(this.addonDB);
  },

  /**
   * Parse loaded data, reconstructing the database if the loaded data is not valid
   * @param aRebuildOnError
   *        If true, synchronously reconstruct the database from installed add-ons
   */
  parseDB: function(aData, aRebuildOnError) {
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
        let newAddon = new DBAddonInternal(loadedAddon);
        addonDB.set(newAddon._key, newAddon);
      };
      parseTimer.done();
      this.addonDB = addonDB;
      logger.debug("Successfully read XPI database");
      this.initialized = true;
    }
    catch(e) {
      // If we catch and log a SyntaxError from the JSON
      // parser, the xpcshell test harness fails the test for us: bug 870828
      parseTimer.done();
      if (e.name == "SyntaxError") {
        logger.error("Syntax error parsing saved XPI JSON data");
        AddonManagerPrivate.recordSimpleMeasure("XPIDB_startupError", "syntax");
      }
      else {
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
  upgradeDB: function(aRebuildOnError) {
    let upgradeTimer = AddonManagerPrivate.simpleTimer("XPIDB_upgradeDB_MS");
    try {
      let schemaVersion = Services.prefs.getIntPref(PREF_DB_SCHEMA);
      if (schemaVersion <= LAST_SQLITE_DB_SCHEMA) {
        // we should have an older SQLITE database
        logger.debug("Attempting to upgrade from SQLITE database");
        this.migrateData = this.getMigrateDataFromSQLITE();
      }
      else {
        // we've upgraded before but the JSON file is gone, fall through
        // and rebuild from scratch
        AddonManagerPrivate.recordSimpleMeasure("XPIDB_startupError", "dbMissing");
      }
    }
    catch(e) {
      // No schema version pref means either a really old upgrade (RDF) or
      // a new profile
      this.migrateData = this.getMigrateDataFromRDF();
    }

    this.rebuildDatabase(aRebuildOnError);
    upgradeTimer.done();
  },

  /**
   * Reconstruct when the DB file exists but is unreadable
   * (for example because read permission is denied)
   */
  rebuildUnreadableDB: function(aError, aRebuildOnError) {
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
  asyncLoadDB: function XPIDB_asyncLoadDB() {
    // Already started (and possibly finished) loading
    if (this._dbPromise) {
      return this._dbPromise;
    }

    logger.debug("Starting async load of XPI database " + this.jsonFile.path);
    AddonManagerPrivate.recordSimpleMeasure("XPIDB_async_load", XPIProvider.runPhase);
    let readOptions = {
      outExecutionDuration: 0
    };
    return this._dbPromise = OS.File.read(this.jsonFile.path, null, readOptions).then(
      byteArray => {
        logger.debug("Async JSON file read took " + readOptions.outExecutionDuration + " MS");
        AddonManagerPrivate.recordSimpleMeasure("XPIDB_asyncRead_MS",
          readOptions.outExecutionDuration);
        if (this._addonDB) {
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
    .then(null,
      error => {
        if (this._addonDB) {
          logger.debug("Synchronous load completed while waiting for async load");
          return this.addonDB;
        }
        if (error.becauseNoSuchFile) {
          this.upgradeDB(true);
        }
        else {
          // it's there but unreadable
          this.rebuildUnreadableDB(error, true);
        }
        return this.addonDB;
      });
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
  rebuildDatabase: function XIPDB_rebuildDatabase(aRebuildOnError) {
    this.addonDB = new Map();
    this.initialized = true;

    if (XPIStates.size == 0) {
      // No extensions installed, so we're done
      logger.debug("Rebuilding XPI database with no extensions");
      return;
    }

    // If there is no migration data then load the list of add-on directories
    // that were active during the last run
    if (!this.migrateData)
      this.activeBundles = this.getActiveBundles();

    if (aRebuildOnError) {
      logger.warn("Rebuilding add-ons database from installed extensions.");
      try {
        XPIDatabaseReconcile.processFileChanges({}, false);
      }
      catch (e) {
        logger.error("Failed to rebuild XPI database from installed extensions", e);
      }
      // Make sure to update the active add-ons and add-ons list on shutdown
      Services.prefs.setBoolPref(PREF_PENDING_OPERATIONS, true);
    }
  },

  /**
   * Gets the list of file descriptors of active extension directories or XPI
   * files from the add-ons list. This must be loaded from disk since the
   * directory service gives no easy way to get both directly. This list doesn't
   * include themes as preferences already say which theme is currently active
   *
   * @return an array of persistent descriptors for the directories
   */
  getActiveBundles: function XPIDB_getActiveBundles() {
    let bundles = [];

    // non-bootstrapped extensions
    let addonsList = FileUtils.getFile(KEY_PROFILEDIR, [FILE_XPI_ADDONS_LIST],
                                       true);

    if (!addonsList.exists())
      // XXX Irving believes this is broken in the case where there is no
      // extensions.ini but there are bootstrap extensions (e.g. Android)
      return null;

    try {
      let iniFactory = Cc["@mozilla.org/xpcom/ini-parser-factory;1"]
                         .getService(Ci.nsIINIParserFactory);
      let parser = iniFactory.createINIParser(addonsList);
      let keys = parser.getKeys("ExtensionDirs");

      while (keys.hasMore())
        bundles.push(parser.getString("ExtensionDirs", keys.getNext()));
    }
    catch (e) {
      logger.warn("Failed to parse extensions.ini", e);
      return null;
    }

    // Also include the list of active bootstrapped extensions
    for (let id in XPIProvider.bootstrappedAddons)
      bundles.push(XPIProvider.bootstrappedAddons[id].descriptor);

    return bundles;
  },

  /**
   * Retrieves migration data from the old extensions.rdf database.
   *
   * @return an object holding information about what add-ons were previously
   *         userDisabled and any updated compatibility information
   */
  getMigrateDataFromRDF: function XPIDB_getMigrateDataFromRDF(aDbWasMissing) {

    // Migrate data from extensions.rdf
    let rdffile = FileUtils.getFile(KEY_PROFILEDIR, [FILE_OLD_DATABASE], true);
    if (!rdffile.exists())
      return null;

    logger.debug("Migrating data from " + FILE_OLD_DATABASE);
    let migrateData = {};

    try {
      let ds = gRDF.GetDataSourceBlocking(Services.io.newFileURI(rdffile).spec);
      let root = Cc["@mozilla.org/rdf/container;1"].
                 createInstance(Ci.nsIRDFContainer);
      root.Init(ds, gRDF.GetResource(RDFURI_ITEM_ROOT));
      let elements = root.GetElements();

      while (elements.hasMoreElements()) {
        let source = elements.getNext().QueryInterface(Ci.nsIRDFResource);

        let location = getRDFProperty(ds, source, "installLocation");
        if (location) {
          if (!(location in migrateData))
            migrateData[location] = {};
          let id = source.ValueUTF8.substring(PREFIX_ITEM_URI.length);
          migrateData[location][id] = {
            version: getRDFProperty(ds, source, "version"),
            userDisabled: false,
            targetApplications: []
          }

          let disabled = getRDFProperty(ds, source, "userDisabled");
          if (disabled == "true" || disabled == "needs-disable")
            migrateData[location][id].userDisabled = true;

          let targetApps = ds.GetTargets(source, EM_R("targetApplication"),
                                         true);
          while (targetApps.hasMoreElements()) {
            let targetApp = targetApps.getNext()
                                      .QueryInterface(Ci.nsIRDFResource);
            let appInfo = {
              id: getRDFProperty(ds, targetApp, "id")
            };

            let minVersion = getRDFProperty(ds, targetApp, "updatedMinVersion");
            if (minVersion) {
              appInfo.minVersion = minVersion;
              appInfo.maxVersion = getRDFProperty(ds, targetApp, "updatedMaxVersion");
            }
            else {
              appInfo.minVersion = getRDFProperty(ds, targetApp, "minVersion");
              appInfo.maxVersion = getRDFProperty(ds, targetApp, "maxVersion");
            }
            migrateData[location][id].targetApplications.push(appInfo);
          }
        }
      }
    }
    catch (e) {
      logger.warn("Error reading " + FILE_OLD_DATABASE, e);
      migrateData = null;
    }

    return migrateData;
  },

  /**
   * Retrieves migration data from a database that has an older or newer schema.
   *
   * @return an object holding information about what add-ons were previously
   *         userDisabled and any updated compatibility information
   */
  getMigrateDataFromDatabase: function XPIDB_getMigrateDataFromDatabase(aConnection) {
    let migrateData = {};

    // Attempt to migrate data from a different (even future!) version of the
    // database
    try {
      var stmt = aConnection.createStatement("PRAGMA table_info(addon)");

      const REQUIRED = ["internal_id", "id", "location", "userDisabled",
                        "installDate", "version"];

      let reqCount = 0;
      let props = [];
      for (let row in resultRows(stmt)) {
        if (REQUIRED.indexOf(row.name) != -1) {
          reqCount++;
          props.push(row.name);
        }
        else if (DB_METADATA.indexOf(row.name) != -1) {
          props.push(row.name);
        }
        else if (DB_BOOL_METADATA.indexOf(row.name) != -1) {
          props.push(row.name);
        }
      }

      if (reqCount < REQUIRED.length) {
        logger.error("Unable to read anything useful from the database");
        return null;
      }
      stmt.finalize();

      stmt = aConnection.createStatement("SELECT " + props.join(",") + " FROM addon");
      for (let row in resultRows(stmt)) {
        if (!(row.location in migrateData))
          migrateData[row.location] = {};
        let addonData = {
          targetApplications: []
        }
        migrateData[row.location][row.id] = addonData;

        props.forEach(function(aProp) {
          if (aProp == "isForeignInstall")
            addonData.foreignInstall = (row[aProp] == 1);
          if (DB_BOOL_METADATA.indexOf(aProp) != -1)
            addonData[aProp] = row[aProp] == 1;
          else
            addonData[aProp] = row[aProp];
        })
      }

      var taStmt = aConnection.createStatement("SELECT id, minVersion, " +
                                                   "maxVersion FROM " +
                                                   "targetApplication WHERE " +
                                                   "addon_internal_id=:internal_id");

      for (let location in migrateData) {
        for (let id in migrateData[location]) {
          taStmt.params.internal_id = migrateData[location][id].internal_id;
          delete migrateData[location][id].internal_id;
          for (let row in resultRows(taStmt)) {
            migrateData[location][id].targetApplications.push({
              id: row.id,
              minVersion: row.minVersion,
              maxVersion: row.maxVersion
            });
          }
        }
      }
    }
    catch (e) {
      // An error here means the schema is too different to read
      logger.error("Error migrating data", e);
      return null;
    }
    finally {
      if (taStmt)
        taStmt.finalize();
      if (stmt)
        stmt.finalize();
    }

    return migrateData;
  },

  /**
   * Shuts down the database connection and releases all cached objects.
   * Return: Promise{integer} resolves / rejects with the result of the DB
   *                          flush after the database is flushed and
   *                          all cleanup is done
   */
  shutdown: function XPIDB_shutdown() {
    logger.debug("shutdown");
    if (this.initialized) {
      // If our last database I/O had an error, try one last time to save.
      if (this.lastError)
        this.saveChanges();

      this.initialized = false;

      if (this._deferredSave) {
        AddonManagerPrivate.recordSimpleMeasure(
            "XPIDB_saves_total", this._deferredSave.totalSaves);
        AddonManagerPrivate.recordSimpleMeasure(
            "XPIDB_saves_overlapped", this._deferredSave.overlappedSaves);
        AddonManagerPrivate.recordSimpleMeasure(
            "XPIDB_saves_late", this._deferredSave.dirty ? 1 : 0);
      }

      // Return a promise that any pending writes of the DB are complete and we
      // are finished cleaning up
      let flushPromise = this.flush();
      flushPromise.then(null, error => {
          logger.error("Flush of XPI database failed", error);
          AddonManagerPrivate.recordSimpleMeasure("XPIDB_shutdownFlush_failed", 1);
          // If our last attempt to read or write the DB failed, force a new
          // extensions.ini to be written to disk on the next startup
          Services.prefs.setBoolPref(PREF_PENDING_OPERATIONS, true);
        })
        .then(count => {
          // Clear out the cached addons data loaded from JSON
          delete this.addonDB;
          delete this._dbPromise;
          // same for the deferred save
          delete this._deferredSave;
          // re-enable the schema version setter
          delete this._schemaVersionSet;
        });
      return flushPromise;
    }
    return Promise.resolve(0);
  },

  /**
   * Asynchronously list all addons that match the filter function
   * @param  aFilter
   *         Function that takes an addon instance and returns
   *         true if that addon should be included in the selected array
   * @param  aCallback
   *         Called back with an array of addons matching aFilter
   *         or an empty array if none match
   */
  getAddonList: function(aFilter, aCallback) {
    this.asyncLoadDB().then(
      addonDB => {
        let addonList = _filterDB(addonDB, aFilter);
        asyncMap(addonList, getRepositoryAddon, makeSafe(aCallback));
      })
    .then(null,
        error => {
          logger.error("getAddonList failed", error);
          makeSafe(aCallback)([]);
        });
  },

  /**
   * (Possibly asynchronously) get the first addon that matches the filter function
   * @param  aFilter
   *         Function that takes an addon instance and returns
   *         true if that addon should be selected
   * @param  aCallback
   *         Called back with the addon, or null if no matching addon is found
   */
  getAddon: function(aFilter, aCallback) {
    return this.asyncLoadDB().then(
      addonDB => {
        getRepositoryAddon(_findAddon(addonDB, aFilter), makeSafe(aCallback));
      })
    .then(null,
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
  getAddonInLocation: function XPIDB_getAddonInLocation(aId, aLocation, aCallback) {
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
  getAddonsInLocation: function XPIDB_getAddonsInLocation(aLocation, aCallback) {
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
  getVisibleAddonForID: function XPIDB_getVisibleAddonForID(aId, aCallback) {
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
  getVisibleAddons: function XPIDB_getVisibleAddons(aTypes, aCallback) {
    this.getAddonList(aAddon => (aAddon.visible &&
                                 (!aTypes || (aTypes.length == 0) ||
                                  (aTypes.indexOf(aAddon.type) > -1))),
                      aCallback);
  },

  /**
   * Synchronously gets all add-ons of a particular type.
   *
   * @param  aType
   *         The type of add-on to retrieve
   * @return an array of DBAddonInternals
   */
  getAddonsByType: function XPIDB_getAddonsByType(aType) {
    if (!this.addonDB) {
      // jank-tastic! Must synchronously load DB if the theme switches from
      // an XPI theme to a lightweight theme before the DB has loaded,
      // because we're called from sync XPIProvider.addonChanged
      logger.warn("Synchronous load of XPI database due to getAddonsByType(" + aType + ")");
      AddonManagerPrivate.recordSimpleMeasure("XPIDB_lateOpen_byType", XPIProvider.runPhase);
      this.syncLoadDB(true);
    }
    return _filterDB(this.addonDB, aAddon => (aAddon.type == aType));
  },

  /**
   * Synchronously gets an add-on with a particular internalName.
   *
   * @param  aInternalName
   *         The internalName of the add-on to retrieve
   * @return a DBAddonInternal
   */
  getVisibleAddonForInternalName: function XPIDB_getVisibleAddonForInternalName(aInternalName) {
    if (!this.addonDB) {
      // This may be called when the DB hasn't otherwise been loaded
      logger.warn("Synchronous load of XPI database due to getVisibleAddonForInternalName");
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
  getVisibleAddonsWithPendingOperations:
    function XPIDB_getVisibleAddonsWithPendingOperations(aTypes, aCallback) {

    this.getAddonList(
        aAddon => (aAddon.visible &&
                   (aAddon.pendingUninstall ||
                    // Logic here is tricky. If we're active but disabled,
                    // we're pending disable; !active && !disabled, we're pending enable
                    (aAddon.active == aAddon.disabled)) &&
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
  getAddonBySyncGUID: function XPIDB_getAddonBySyncGUID(aGUID, aCallback) {
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
  getAddons: function XPIDB_getAddons() {
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
   * @param  aDescriptor
   *         The file descriptor of the add-on
   * @return The DBAddonInternal that was added to the database
   */
  addAddonMetadata: function XPIDB_addAddonMetadata(aAddon, aDescriptor) {
    if (!this.addonDB) {
      AddonManagerPrivate.recordSimpleMeasure("XPIDB_lateOpen_addMetadata",
          XPIProvider.runPhase);
      this.syncLoadDB(false);
    }

    let newAddon = new DBAddonInternal(aAddon);
    newAddon.descriptor = aDescriptor;
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
   * @param  aDescriptor
   *         The file descriptor of the add-on
   * @return The DBAddonInternal that was added to the database
   */
  updateAddonMetadata: function XPIDB_updateAddonMetadata(aOldAddon, aNewAddon,
                                                          aDescriptor) {
    this.removeAddonMetadata(aOldAddon);
    aNewAddon.syncGUID = aOldAddon.syncGUID;
    aNewAddon.installDate = aOldAddon.installDate;
    aNewAddon.applyBackgroundUpdates = aOldAddon.applyBackgroundUpdates;
    aNewAddon.foreignInstall = aOldAddon.foreignInstall;
    aNewAddon.active = (aNewAddon.visible && !aNewAddon.disabled && !aNewAddon.pendingUninstall);

    // addAddonMetadata does a saveChanges()
    return this.addAddonMetadata(aNewAddon, aDescriptor);
  },

  /**
   * Synchronously removes an add-on from the database.
   *
   * @param  aAddon
   *         The DBAddonInternal being removed
   */
  removeAddonMetadata: function XPIDB_removeAddonMetadata(aAddon) {
    this.addonDB.delete(aAddon._key);
    this.saveChanges();
  },

  /**
   * Synchronously marks a DBAddonInternal as visible marking all other
   * instances with the same ID as not visible.
   *
   * @param  aAddon
   *         The DBAddonInternal to make visible
   */
  makeAddonVisible: function XPIDB_makeAddonVisible(aAddon) {
    logger.debug("Make addon " + aAddon._key + " visible");
    for (let [, otherAddon] of this.addonDB) {
      if ((otherAddon.id == aAddon.id) && (otherAddon._key != aAddon._key)) {
        logger.debug("Hide addon " + otherAddon._key);
        otherAddon.visible = false;
        otherAddon.active = false;
      }
    }
    aAddon.visible = true;
    this.saveChanges();
  },

  /**
   * Synchronously sets properties for an add-on.
   *
   * @param  aAddon
   *         The DBAddonInternal being updated
   * @param  aProperties
   *         A dictionary of properties to set
   */
  setAddonProperties: function XPIDB_setAddonProperties(aAddon, aProperties) {
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
  setAddonSyncGUID: function XPIDB_setAddonSyncGUID(aAddon, aGUID) {
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
  updateAddonActive: function XPIDB_updateAddonActive(aAddon, aActive) {
    logger.debug("Updating active state for add-on " + aAddon.id + " to " + aActive);

    aAddon.active = aActive;
    this.saveChanges();
  },

  /**
   * Synchronously calculates and updates all the active flags in the database.
   */
  updateActiveAddons: function XPIDB_updateActiveAddons() {
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

  /**
   * Writes out the XPI add-ons list for the platform to read.
   * @return true if the file was successfully updated, false otherwise
   */
  writeAddonsList: function XPIDB_writeAddonsList() {
    if (!this.addonDB) {
      // force the DB to load
      AddonManagerPrivate.recordSimpleMeasure("XPIDB_lateOpen_writeList",
          XPIProvider.runPhase);
      this.syncLoadDB(true);
    }
    Services.appinfo.invalidateCachesOnRestart();

    let addonsList = FileUtils.getFile(KEY_PROFILEDIR, [FILE_XPI_ADDONS_LIST],
                                       true);
    let enabledAddons = [];
    let text = "[ExtensionDirs]\r\n";
    let count = 0;
    let fullCount = 0;

    let activeAddons = _filterDB(
      this.addonDB,
      aAddon => aAddon.active && !aAddon.bootstrap && (aAddon.type != "theme"));

    for (let row of activeAddons) {
      text += "Extension" + (count++) + "=" + row.descriptor + "\r\n";
      enabledAddons.push(encodeURIComponent(row.id) + ":" +
                         encodeURIComponent(row.version));
    }
    fullCount += count;

    // The selected skin may come from an inactive theme (the default theme
    // when a lightweight theme is applied for example)
    text += "\r\n[ThemeDirs]\r\n";

    let dssEnabled = false;
    try {
      dssEnabled = Services.prefs.getBoolPref(PREF_EM_DSS_ENABLED);
    } catch (e) {}

    let themes = [];
    if (dssEnabled) {
      themes = _filterDB(this.addonDB, aAddon => aAddon.type == "theme");
    }
    else {
      let activeTheme = _findAddon(
        this.addonDB,
        aAddon => (aAddon.type == "theme") &&
                  (aAddon.internalName == XPIProvider.selectedSkin));
      if (activeTheme) {
        themes.push(activeTheme);
      }
    }

    if (themes.length > 0) {
      count = 0;
      for (let row of themes) {
        text += "Extension" + (count++) + "=" + row.descriptor + "\r\n";
        enabledAddons.push(encodeURIComponent(row.id) + ":" +
                           encodeURIComponent(row.version));
      }
      fullCount += count;
    }

    text += "\r\n[MultiprocessIncompatibleExtensions]\r\n";

    count = 0;
    for (let row of activeAddons) {
      if (!row.multiprocessCompatible) {
        text += "Extension" + (count++) + "=" + row.id + "\r\n";
      }
    }

    if (fullCount > 0) {
      logger.debug("Writing add-ons list");

      try {
        let addonsListTmp = FileUtils.getFile(KEY_PROFILEDIR, [FILE_XPI_ADDONS_LIST + ".tmp"],
                                              true);
        var fos = FileUtils.openFileOutputStream(addonsListTmp);
        fos.write(text, text.length);
        fos.close();
        addonsListTmp.moveTo(addonsListTmp.parent, FILE_XPI_ADDONS_LIST);

        Services.prefs.setCharPref(PREF_EM_ENABLED_ADDONS, enabledAddons.join(","));
      }
      catch (e) {
        logger.error("Failed to write add-ons list to profile directory", e);
        return false;
      }
    }
    else {
      if (addonsList.exists()) {
        logger.debug("Deleting add-ons list");
        try {
          addonsList.remove(false);
        }
        catch (e) {
          logger.error("Failed to remove " + addonsList.path, e);
          return false;
        }
      }

      Services.prefs.clearUserPref(PREF_EM_ENABLED_ADDONS);
    }
    return true;
  }
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

    for (let [location, addons] of addonMap) {
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
   * @param  aMigrateData
   *         If during startup the database had to be upgraded this will
   *         contain data that used to be held about this add-on
   * @return a boolean indicating if flushing caches is required to complete
   *         changing this add-on
   */
  addMetadata(aInstallLocation, aId, aAddonState, aNewAddon, aOldAppVersion,
              aOldPlatformVersion, aMigrateData) {
    logger.debug("New add-on " + aId + " installed in " + aInstallLocation.name);

    // If we had staged data for this add-on or we aren't recovering from a
    // corrupt database and we don't have migration data for this add-on then
    // this must be a new install.
    let isNewInstall = (!!aNewAddon) || (!XPIDatabase.activeBundles && !aMigrateData);

    // If it's a new install and we haven't yet loaded the manifest then it
    // must be something dropped directly into the install location
    let isDetectedInstall = isNewInstall && !aNewAddon;

    // Load the manifest if necessary and sanity check the add-on ID
    try {
      if (!aNewAddon) {
        // Load the manifest from the add-on.
        let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
        file.persistentDescriptor = aAddonState.descriptor;
        aNewAddon = syncLoadManifestFromFile(file, aInstallLocation);
      }
      // The add-on in the manifest should match the add-on ID.
      if (aNewAddon.id != aId) {
        throw new Error("Invalid addon ID: expected addon ID " + aId +
                        ", found " + aNewAddon.id + " in manifest");
      }
    }
    catch (e) {
      logger.warn("addMetadata: Add-on " + aId + " is invalid", e);

      // Remove the invalid add-on from the install location if the install
      // location isn't locked, no restart will be necessary
      if (!aInstallLocation.locked)
        aInstallLocation.uninstallAddon(aId);
      else
        logger.warn("Could not uninstall invalid item from locked install location");
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

    if (aMigrateData) {
      // If there is migration data then apply it.
      logger.debug("Migrating data from old database");

      DB_MIGRATE_METADATA.forEach(function(aProp) {
        // A theme's disabled state is determined by the selected theme
        // preference which is read in loadManifestFromRDF
        if (aProp == "userDisabled" && aNewAddon.type == "theme")
          return;

        if (aProp in aMigrateData)
          aNewAddon[aProp] = aMigrateData[aProp];
      });

      // Force all non-profile add-ons to be foreignInstalls since they can't
      // have been installed through the API
      aNewAddon.foreignInstall |= aInstallLocation.name != KEY_APP_PROFILE;

      // Some properties should only be migrated if the add-on hasn't changed.
      // The version property isn't a perfect check for this but covers the
      // vast majority of cases.
      if (aMigrateData.version == aNewAddon.version) {
        logger.debug("Migrating compatibility info");
        if ("targetApplications" in aMigrateData)
          aNewAddon.applyCompatibilityUpdate(aMigrateData, true);
      }

      // Since the DB schema has changed make sure softDisabled is correct
      applyBlocklistChanges(aNewAddon, aNewAddon, aOldAppVersion,
                            aOldPlatformVersion);
    }

    // The default theme is never a foreign install
    if (aNewAddon.type == "theme" && aNewAddon.internalName == XPIProvider.defaultSkin)
      aNewAddon.foreignInstall = false;

    if (isDetectedInstall && aNewAddon.foreignInstall) {
      // If the add-on is a foreign install and is in a scope where add-ons
      // that were dropped in should default to disabled then disable it
      let disablingScopes = Preferences.get(PREF_EM_AUTO_DISABLED_SCOPES, 0);
      if (aInstallLocation.scope & disablingScopes) {
        logger.warn("Disabling foreign installed add-on " + aNewAddon.id + " in "
            + aInstallLocation.name);
        aNewAddon.userDisabled = true;
      }
    }

    return XPIDatabase.addAddonMetadata(aNewAddon, aAddonState.descriptor);
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
   * Updates an add-on's metadata and determines if a restart of the
   * application is necessary. This is called when either the add-on's
   * install directory path or last modified time has changed.
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
        let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
        file.persistentDescriptor = aAddonState.descriptor;
        aNewAddon = syncLoadManifestFromFile(file, aInstallLocation);
        applyBlocklistChanges(aOldAddon, aNewAddon);

        // Carry over any pendingUninstall state to add-ons modified directly
        // in the profile. This is important when the attempt to remove the
        // add-on in processPendingFileChanges failed and caused an mtime
        // change to the add-ons files.
        aNewAddon.pendingUninstall = aOldAddon.pendingUninstall;
      }

      // The ID in the manifest that was loaded must match the ID of the old
      // add-on.
      if (aNewAddon.id != aOldAddon.id)
        throw new Error("Incorrect id in install manifest for existing add-on " + aOldAddon.id);
    }
    catch (e) {
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
    return XPIDatabase.updateAddonMetadata(aOldAddon, aNewAddon, aAddonState.descriptor);
  },

  /**
   * Updates an add-on's descriptor for when the add-on has moved in the
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
  updateDescriptor(aInstallLocation, aOldAddon, aAddonState) {
    logger.debug("Add-on " + aOldAddon.id + " moved to " + aAddonState.descriptor);
    aOldAddon.descriptor = aAddonState.descriptor;
    aOldAddon._sourceBundle.persistentDescriptor = aAddonState.descriptor;

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
   * @param  aOldAppVersion
   *         The version of the application last run with this profile or null
   *         if it is a new profile or the version is unknown
   * @param  aOldPlatformVersion
   *         The version of the platform last run with this profile or null
   *         if it is a new profile or the version is unknown
   * @return a boolean indicating if flushing caches is required to complete
   *         changing this add-on
   */
  updateCompatibility(aInstallLocation, aOldAddon, aAddonState, aOldAppVersion, aOldPlatformVersion) {
    logger.debug("Updating compatibility for add-on " + aOldAddon.id + " in " + aInstallLocation.name);

    // If updating from a version of the app that didn't support signedState
    // then fetch that property now
    if (aOldAddon.signedState === undefined && ADDON_SIGNING &&
        SIGNED_TYPES.has(aOldAddon.type)) {
      let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
      file.persistentDescriptor = aAddonState.descriptor;
      let manifest = syncLoadManifestFromFile(file, aInstallLocation);
      aOldAddon.signedState = manifest.signedState;
    }
    // This updates the addon's JSON cached data in place
    applyBlocklistChanges(aOldAddon, aOldAddon, aOldAppVersion,
                          aOldPlatformVersion);
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
   * @return a boolean indicating if a change requiring flushing the caches was
   *         detected
   */
  processFileChanges(aManifests, aUpdateCompatibility, aOldAppVersion, aOldPlatformVersion) {
    let loadedManifest = (aInstallLocation, aId) => {
      if (!(aInstallLocation.name in aManifests))
        return null;
      if (!(aId in aManifests[aInstallLocation.name]))
        return null;
      return aManifests[aInstallLocation.name][aId];
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
                  name: XPIProvider._mostRecentlyModifiedFile[id],
                  mtime: xpiState.mtime,
                  oldtime: oldAddon.updateDate
                });
              } else {
                XPIProvider.setTelemetry(oldAddon.id, "modifiedFile",
                                         XPIProvider._mostRecentlyModifiedFile[id]);
              }
            }

            // The add-on has changed if the modification time has changed, or
            // we have an updated manifest for it. Also reload the metadata for
            // add-ons in the application directory when the application version
            // has changed
            let newAddon = loadedManifest(installLocation, id);
            if (newAddon || oldAddon.updateDate != xpiState.mtime ||
                (aUpdateCompatibility && (installLocation.name == KEY_APP_GLOBAL ||
                                          installLocation.name == KEY_APP_SYSTEM_DEFAULTS))) {
              newAddon = this.updateMetadata(installLocation, oldAddon, xpiState, newAddon);
            }
            else if (oldAddon.descriptor != xpiState.descriptor) {
              newAddon = this.updateDescriptor(installLocation, oldAddon, xpiState);
            }
            else if (aUpdateCompatibility) {
              newAddon = this.updateCompatibility(installLocation, oldAddon, xpiState,
                                                  aOldAppVersion, aOldPlatformVersion);
            }
            else {
              // No change
              newAddon = oldAddon;
            }

            if (newAddon)
              locationAddonMap.set(newAddon.id, newAddon);
          }
          else {
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
        for (let [id, oldAddon] of addons)
          this.removeMetadata(oldAddon);
      }
    }

    // Validate the updated system add-ons
    let systemAddonLocation = XPIProvider.installLocationsByName[KEY_APP_SYSTEM_ADDONS];
    let addons = currentAddons.get(KEY_APP_SYSTEM_ADDONS) || new Map();

    let hideLocation;
    if (systemAddonLocation.isActive() && systemAddonLocation.isValid(addons)) {
      // Hide the system add-on defaults
      logger.info("Hiding the default system add-ons.");
      hideLocation = KEY_APP_SYSTEM_DEFAULTS;
    }
    else {
      // Hide the system add-on updates
      logger.info("Hiding the updated system add-ons.");
      hideLocation = KEY_APP_SYSTEM_ADDONS;
    }

    let previousVisible = this.getVisibleAddons(previousAddons);
    let currentVisible = this.flattenByID(currentAddons, hideLocation);
    let sawActiveTheme = false;
    XPIProvider.bootstrappedAddons = {};

    // Pass over the new set of visible add-ons, record any changes that occured
    // during startup and call bootstrap install/uninstall scripts as necessary
    for (let [id, currentAddon] of currentVisible) {
      let previousAddon = previousVisible.get(id);

      // Note if any visible add-on is not in the application install location
      if (currentAddon._installLocation.name != KEY_APP_GLOBAL)
        XPIProvider.allAppGlobal = false;

      let isActive = !currentAddon.disabled;
      let wasActive = previousAddon ? previousAddon.active : currentAddon.active

      if (!previousAddon) {
        // If we had a manifest for this add-on it was a staged install and
        // so wasn't something recovered from a corrupt database
        let wasStaged = !!loadedManifest(currentAddon._installLocation, id);

        // We might be recovering from a corrupt database, if so use the
        // list of known active add-ons to update the new add-on
        if (!wasStaged && XPIDatabase.activeBundles) {
          // For themes we know which is active by the current skin setting
          if (currentAddon.type == "theme")
            isActive = currentAddon.internalName == XPIProvider.currentSkin;
          else
            isActive = XPIDatabase.activeBundles.indexOf(currentAddon.descriptor) != -1;

          // If the add-on wasn't active and it isn't already disabled in some way
          // then it was probably either softDisabled or userDisabled
          if (!isActive && !currentAddon.disabled) {
            // If the add-on is softblocked then assume it is softDisabled
            if (currentAddon.blocklistState == Blocklist.STATE_SOFTBLOCKED)
              currentAddon.softDisabled = true;
            else
              currentAddon.userDisabled = true;
          }
        }
        else {
          // This is a new install
          if (currentAddon.foreignInstall)
            AddonManagerPrivate.addStartupChange(AddonManager.STARTUP_CHANGE_INSTALLED, id);

          if (currentAddon.bootstrap) {
            // Visible bootstrapped add-ons need to have their install method called
            XPIProvider.callBootstrapMethod(currentAddon, currentAddon._sourceBundle,
                                            "install", BOOTSTRAP_REASONS.ADDON_INSTALL);
            if (!isActive)
              XPIProvider.unloadBootstrapScope(currentAddon.id);
          }
        }
      }
      else {
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
              previousAddon._sourceBundle.exists() &&
              currentAddon._sourceBundle.path != previousAddon._sourceBundle.path) {

            XPIProvider.callBootstrapMethod(previousAddon, previousAddon._sourceBundle,
                                            "uninstall", installReason,
                                            { newVersion: currentAddon.version });
            XPIProvider.unloadBootstrapScope(previousAddon.id);
          }

          // Make sure to flush the cache when an old add-on has gone away
          flushStartupCache();

          if (currentAddon.bootstrap) {
            // Visible bootstrapped add-ons need to have their install method called
            let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
            file.persistentDescriptor = currentAddon._sourceBundle.persistentDescriptor;
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

      // Make sure the bootstrap information is up to date for this ID
      if (currentAddon.bootstrap && currentAddon.active) {
        XPIProvider.bootstrappedAddons[id] = {
          version: currentAddon.version,
          type: currentAddon.type,
          descriptor: currentAddon._sourceBundle.persistentDescriptor,
          multiprocessCompatible: currentAddon.multiprocessCompatible,
          runInSafeMode: canRunInSafeMode(currentAddon),
        };
      }

      if (currentAddon.active && currentAddon.internalName == XPIProvider.selectedSkin)
        sawActiveTheme = true;
    }

    // Pass over the set of previously visible add-ons that have now gone away
    // and record the change.
    for (let [id, previousAddon] of previousVisible) {
      if (currentVisible.has(id))
        continue;

      // This add-on vanished

      // If the previous add-on was bootstrapped and still exists then call its
      // uninstall method.
      if (previousAddon.bootstrap && previousAddon._sourceBundle.exists()) {
        XPIProvider.callBootstrapMethod(previousAddon, previousAddon._sourceBundle,
                                        "uninstall", BOOTSTRAP_REASONS.ADDON_UNINSTALL);
        XPIProvider.unloadBootstrapScope(previousAddon.id);
      }
      AddonManagerPrivate.addStartupChange(AddonManager.STARTUP_CHANGE_UNINSTALLED, id);

      // Make sure to flush the cache when an old add-on has gone away
      flushStartupCache();
    }

    // Make sure add-ons from hidden locations are marked invisible and inactive
    let locationAddonMap = currentAddons.get(hideLocation);
    if (locationAddonMap) {
      for (let addon of locationAddonMap.values()) {
        addon.visible = false;
        addon.active = false;
      }
    }

    // If a custom theme is selected and it wasn't seen in the new list of
    // active add-ons then enable the default theme
    if (XPIProvider.selectedSkin != XPIProvider.defaultSkin && !sawActiveTheme) {
      logger.info("Didn't see selected skin " + XPIProvider.selectedSkin);
      XPIProvider.enableDefaultTheme();
    }

    // Finally update XPIStates to match everything
    for (let [locationName, locationAddonMap] of currentAddons) {
      for (let [id, addon] of locationAddonMap) {
        let xpiState = XPIStates.getAddon(locationName, id);
        xpiState.syncWithDB(addon);
      }
    }
    XPIStates.save();

    XPIProvider.persistBootstrappedAddons();

    // Clear out any cached migration data.
    XPIDatabase.migrateData = null;
    XPIDatabase.saveChanges();

    return true;
  },
}
