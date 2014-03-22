/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/AddonManager.jsm");

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

Cu.import("resource://gre/modules/Log.jsm");
const LOGGER_ID = "addons.xpi-utils";

// Create a new logger for use by the Addons XPI Provider Utils
// (Requires AddonManager.jsm)
let logger = Log.repository.getLogger(LOGGER_ID);

const KEY_PROFILEDIR                  = "ProfD";
const FILE_DATABASE                   = "extensions.sqlite";
const FILE_JSON_DB                    = "extensions.json";
const FILE_OLD_DATABASE               = "extensions.rdf";
const FILE_XPI_ADDONS_LIST            = "extensions.ini";

// The value for this is in Makefile.in
#expand const DB_SCHEMA                       = __MOZ_EXTENSIONS_DB_SCHEMA__;

// The last version of DB_SCHEMA implemented in SQLITE
const LAST_SQLITE_DB_SCHEMA           = 14;
const PREF_DB_SCHEMA                  = "extensions.databaseSchema";
const PREF_PENDING_OPERATIONS         = "extensions.pendingOperations";
const PREF_EM_ENABLED_ADDONS          = "extensions.enabledAddons";
const PREF_EM_DSS_ENABLED             = "extensions.dss.enabled";


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
                          "targetPlatforms"];

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

  try {
    this._sourceBundle = this._installLocation.getLocationForID(this.id);
  }
  catch (e) {
    // An exception will be thrown if the add-on appears in the database but
    // not on disk. In general this should only happen during startup as
    // this change is being detected.
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
  for (let [, addon] of addonDB) {
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
  let addonList = [];
  for (let [, addon] of addonDB) {
    if (aFilter(addon)) {
      addonList.push(addon);
    }
  }

  return addonList;
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

    if (!this._deferredSave) {
      this._deferredSave = new DeferredSave(this.jsonFile.path,
                                            () => JSON.stringify(this),
                                            ASYNC_SAVE_DELAY_MS);
    }

    let promise = this._deferredSave.saveChanges();
    if (!this._schemaVersionSet) {
      this._schemaVersionSet = true;
      promise.then(
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
          logger.warn("Failed to save XPI database", error);
          // this._deferredSave.lastError has the most recent error so we don't
          // need this any more
          this._loadError = null;
        });
    }
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
        let (str = {}) {
          let read = 0;
          do {
            read = cstream.readString(0xffffffff, str); // read as much as we can and put it in str.value
            data += str.value;
          } while (read != 0);
        }
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

    if (XPIProvider.installStates && XPIProvider.installStates.length == 0) {
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
        XPIProvider.processFileChanges(XPIProvider.installStates, {}, false);
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
   * Return a list of all install locations known about by the database. This
   * is often a a subset of the total install locations when not all have
   * installed add-ons, occasionally a superset when an install location no
   * longer exists. Only called from XPIProvider.processFileChanges, when
   * the database should already be loaded.
   *
   * @return  a Set of names of install locations
   */
  getInstallLocations: function XPIDB_getInstallLocations() {
    let locations = new Set();
    if (!this.addonDB)
      return locations;

    for (let [, addon] of this.addonDB) {
      locations.add(addon.location);
    }
    return locations;
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
          logger.error("getAddonList failed", e);
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
          logger.error("getAddon failed", e);
          makeSafe(aCallback)(null);
        });
  },

  /**
   * Synchronously reads all the add-ons in a particular install location.
   * Always called with the addon database already loaded.
   *
   * @param  aLocation
   *         The name of the install location
   * @return an array of DBAddonInternals
   */
  getAddonsInLocation: function XPIDB_getAddonsInLocation(aLocation) {
    return _filterDB(this.addonDB, aAddon => (aAddon.location == aLocation));
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
                    // Logic here is tricky. If we're active but either
                    // disabled flag is set, we're pending disable; if we're not
                    // active and neither disabled flag is set, we're pending enable
                    (aAddon.active == (aAddon.userDisabled || aAddon.appDisabled))) &&
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
    aNewAddon.active = (aNewAddon.visible && !aNewAddon.userDisabled &&
                        !aNewAddon.appDisabled && !aNewAddon.pendingUninstall);

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
      let newActive = (addon.visible && !addon.userDisabled &&
                      !addon.softDisabled && !addon.appDisabled &&
                      !addon.pendingUninstall);
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
