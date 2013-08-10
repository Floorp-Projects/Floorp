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

XPCOMUtils.defineLazyModuleGetter(this, "AddonRepository",
                                  "resource://gre/modules/AddonRepository.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DeferredSave",
                                  "resource://gre/modules/DeferredSave.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/Promise.jsm");

["LOG", "WARN", "ERROR"].forEach(function(aName) {
  Object.defineProperty(this, aName, {
    get: function logFuncGetter () {
      Cu.import("resource://gre/modules/AddonLogging.jsm");

      LogManager.getLogger("addons.xpi-utils", this);
      return this[aName];
    },
    configurable: true
  });
}, this);


const KEY_PROFILEDIR                  = "ProfD";
const FILE_DATABASE                   = "extensions.sqlite";
const FILE_JSON_DB                    = "extensions.json";
const FILE_OLD_DATABASE               = "extensions.rdf";
const FILE_XPI_ADDONS_LIST            = "extensions.ini";

// The value for this is in Makefile.in
#expand const DB_SCHEMA                       = __MOZ_EXTENSIONS_DB_SCHEMA__;

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

const FIELDS_ADDON = "internal_id, id, syncGUID, location, version, type, " +
                     "internalName, updateURL, updateKey, optionsURL, " +
                     "optionsType, aboutURL, iconURL, icon64URL, " +
                     "defaultLocale, visible, active, userDisabled, " +
                     "appDisabled, pendingUninstall, descriptor, " +
                     "installDate, updateDate, applyBackgroundUpdates, bootstrap, " +
                     "skinnable, size, sourceURI, releaseNotesURI, softDisabled, " +
                     "isForeignInstall, hasBinaryComponents, strictCompatibility";


// Properties that exist in the install manifest
const PROP_METADATA      = ["id", "version", "type", "internalName", "updateURL",
                            "updateKey", "optionsURL", "optionsType", "aboutURL",
                            "iconURL", "icon64URL"];
const PROP_LOCALE_SINGLE = ["name", "description", "creator", "homepageURL"];
const PROP_LOCALE_MULTI  = ["developers", "translators", "contributors"];
const PROP_TARGETAPP     = ["id", "minVersion", "maxVersion"];

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
      WARN("Async map function failed", e);
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
  ERROR("SQL error " + aError + ": " + aErrorString);
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

  // XXX Can we redesign pendingUpgrade?
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

DBAddonInternal.prototype = {
  applyCompatibilityUpdate: function DBA_applyCompatibilityUpdate(aUpdate, aSyncCompatibility) {
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
  },

  get inDatabase() {
    return true;
  },

  toJSON: function() {
    return copyProperties(this, PROP_JSON_FIELDS);
  }
}

DBAddonInternal.prototype.__proto__ = AddonInternal.prototype;

this.XPIDatabase = {
  // true if the database connection has been opened
  initialized: false,
  // The database file
  jsonFile: FileUtils.getFile(KEY_PROFILEDIR, [FILE_JSON_DB], true),
  // Migration data loaded from an old version of the database.
  migrateData: null,
  // Active add-on directories loaded from extensions.ini and prefs at startup.
  activeBundles: null,
  // Special handling for when the database is locked at first load
  lockedDatabase: false,

  // XXX may be able to refactor this away
  get dbfileExists() {
    delete this.dbfileExists;
    return this.dbfileExists = this.jsonFile.exists();
  },
  set dbfileExists(aValue) {
    delete this.dbfileExists;
    return this.dbfileExists = aValue;
  },

  /**
   * Mark the current stored data dirty, and schedule a flush to disk
   */
  saveChanges: function() {
    if (!this.initialized) {
      throw new Error("Attempt to use XPI database when it is not initialized");
    }

    // handle the "in memory only" case
    if (this.lockedDatabase) {
      return;
    }

    let promise = this._deferredSave.saveChanges();
    if (!this._schemaVersionSet) {
      this._schemaVersionSet = true;
      promise.then(
        count => {
          // Update the XPIDB schema version preference the first time we successfully
          // save the database.
          LOG("XPI Database saved, setting schema version preference to " + DB_SCHEMA);
          Services.prefs.setIntPref(PREF_DB_SCHEMA, DB_SCHEMA);
        },
        error => {
          // Need to try setting the schema version again later
          this._schemaVersionSet = false;
          WARN("Failed to save XPI database", error);
        });
    }
  },

  flush: function() {
    // handle the "in memory only" case
    if (this.lockedDatabase) {
      let done = Promise.defer();
      done.resolve(0);
      return done.promise;
    }

    return this._deferredSave.flush();
  },

  get _deferredSave() {
    delete this._deferredSave;
    return this._deferredSave =
      new DeferredSave(this.jsonFile.path, () => JSON.stringify(this),
                       ASYNC_SAVE_DELAY_MS);
  },

  /**
   * Converts the current internal state of the XPI addon database to JSON
   */
  toJSON: function() {
    let addons = [];
    for (let [key, addon] of this.addonDB) {
      addons.push(addon);
    }
    let toSave = {
      schemaVersion: DB_SCHEMA,
      addons: addons
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
  loadSqliteData: function XPIDB_loadSqliteData() {
    let connection = null;
    let dbfile = FileUtils.getFile(KEY_PROFILEDIR, [FILE_DATABASE], true);
    if (!dbfile.exists()) {
      return false;
    }
    // Attempt to open the database
    try {
      connection = Services.storage.openUnsharedDatabase(dbfile);
    }
    catch (e) {
      // exists but SQLITE can't open it
      WARN("Failed to open sqlite database " + dbfile.path + " for upgrade", e);
      this.migrateData = null;
      return true;
    }
    LOG("Migrating data from sqlite");
    this.migrateData = this.getMigrateDataFromDatabase(connection);
    connection.close();
    return true;
  },

  /**
   * Opens and reads the database file, upgrading from old
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
  openConnection: function XPIDB_openConnection(aRebuildOnError, aForceOpen) {
    // XXX TELEMETRY report opens with aRebuildOnError true (which implies delayed open)
    // vs. aRebuildOnError false (DB loaded during startup)
    delete this.addonDB;
    this.migrateData = null;
    let fstream = null;
    let data = "";
    try {
      LOG("Opening XPI database " + this.jsonFile.path);
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
        // dump("Loaded JSON:\n" + data + "\n");
        let inputAddons = JSON.parse(data);
        // Now do some sanity checks on our JSON db
        if (!("schemaVersion" in inputAddons) || !("addons" in inputAddons)) {
          // Content of JSON file is bad, need to rebuild from scratch
          ERROR("bad JSON file contents");
          this.rebuildDatabase(aRebuildOnError);
        }
        if (inputAddons.schemaVersion != DB_SCHEMA) {
          // Handle mismatched JSON schema version. For now, we assume
          // compatibility for JSON data, though we throw away any fields we
          // don't know about
          // XXX preserve unknown fields during save/restore
          LOG("JSON schema mismatch: expected " + DB_SCHEMA +
              ", actual " + inputAddons.schemaVersion);
        }
        // If we got here, we probably have good data
        // Make AddonInternal instances from the loaded data and save them
        let addonDB = new Map();
        inputAddons.addons.forEach(function(loadedAddon) {
          let newAddon = new DBAddonInternal(loadedAddon);
          addonDB.set(newAddon._key, newAddon);
        });
        this.addonDB = addonDB;
        LOG("Successfully read XPI database");
        this.initialized = true;
      }
      catch(e) {
        // If we catch and log a SyntaxError from the JSON
        // parser, the xpcshell test harness fails the test for us: bug 870828
        if (e.name == "SyntaxError") {
          ERROR("Syntax error parsing saved XPI JSON data");
        }
        else {
          ERROR("Failed to load XPI JSON data from profile", e);
        }
        this.rebuildDatabase(aRebuildOnError);
      }
      finally {
        if (cstream)
          cstream.close();
      }
    }
    catch (e) {
      if (e.result == Cr.NS_ERROR_FILE_NOT_FOUND) {
        // XXX re-implement logic to decide whether to upgrade database
        // by checking the DB_SCHEMA_VERSION preference.
        // Fall back to attempting database upgrades
        WARN("Extensions database not found; attempting to upgrade");
        // See if there is SQLITE to migrate from
        if (!this.loadSqliteData()) {
          // Nope, try RDF
          this.migrateData = this.getMigrateDataFromRDF();
        }

        this.rebuildDatabase(aRebuildOnError);
      }
      else {
        WARN("Extensions database " + this.jsonFile.path +
            " exists but is not readable; rebuilding in memory", e);
        // XXX open question - if we can overwrite at save time, should we, or should we
        // leave the locked database in case we can recover from it next time we start up?
        this.lockedDatabase = true;
        // XXX TELEMETRY report when this happens?
        this.rebuildDatabase(aRebuildOnError);
      }
    }
    finally {
      if (fstream)
        fstream.close();
    }

    return;

    // XXX what about aForceOpen? Appears to handle the case of "don't open DB file if there aren't any extensions"?
    if (!aForceOpen && !this.dbfileExists) {
      this.connection = null;
      return;
    }
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

    // If there is no migration data then load the list of add-on directories
    // that were active during the last run
    if (!this.migrateData)
      this.activeBundles = this.getActiveBundles();

    if (aRebuildOnError) {
      WARN("Rebuilding add-ons database from installed extensions.");
      try {
        let state = XPIProvider.getInstallLocationStates();
        XPIProvider.processFileChanges(state, {}, false);
      }
      catch (e) {
        ERROR("Failed to rebuild XPI database from installed extensions", e);
      }
      // Make to update the active add-ons and add-ons list on shutdown
      Services.prefs.setBoolPref(PREF_PENDING_OPERATIONS, true);
    }
  },

  /**
   * Lazy getter for the addons database
   */
  get addonDB() {
    this.openConnection(true);
    return this.addonDB;
  },

  /**
   * Gets the list of file descriptors of active extension directories or XPI
   * files from the add-ons list. This must be loaded from disk since the
   * directory service gives no easy way to get both directly. This list doesn't
   * include themes as preferences already say which theme is currently active
   *
   * @return an array of persisitent descriptors for the directories
   */
  getActiveBundles: function XPIDB_getActiveBundles() {
    let bundles = [];

    let addonsList = FileUtils.getFile(KEY_PROFILEDIR, [FILE_XPI_ADDONS_LIST],
                                       true);

    if (!addonsList.exists())
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
      WARN("Failed to parse extensions.ini", e);
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

    LOG("Migrating data from " + FILE_OLD_DATABASE);
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
      WARN("Error reading " + FILE_OLD_DATABASE, e);
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
        ERROR("Unable to read anything useful from the database");
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
      ERROR("Error migrating data", e);
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
   */
  shutdown: function XPIDB_shutdown(aCallback) {
    LOG("shutdown");
    if (this.initialized) {
      // If we are running with an in-memory database then force a new
      // extensions.ini to be written to disk on the next startup
      if (this.lockedDatabase)
        Services.prefs.setBoolPref(PREF_PENDING_OPERATIONS, true);

      this.initialized = false;
      let result = null;

      // Make sure any pending writes of the DB are complete, and we
      // finish cleaning up, and then call back
      this.flush()
        .then(null, error => {
          ERROR("Flush of XPI database failed", error);
          Services.prefs.setBoolPref(PREF_PENDING_OPERATIONS, true);
          result = error;
          return 0;
        })
        .then(count => {
          // Clear out the cached addons data loaded from JSON and recreate
          // the getter to allow database re-loads during testing.
          delete this.addonDB;
          Object.defineProperty(this, "addonDB", {
            get: function addonsGetter() {
              this.openConnection(true);
              return this.addonDB;
            },
            configurable: true
          });
          // same for the deferred save
          delete this._deferredSave;
          Object.defineProperty(this, "_deferredSave", {
            set: function deferredSaveGetter() {
              delete this._deferredSave;
              return this._deferredSave =
                new DeferredSave(this.jsonFile.path, this.formJSON.bind(this),
                                 ASYNC_SAVE_DELAY_MS);
            },
            configurable: true
          });
          // re-enable the schema version setter
          delete this._schemaVersionSet;

          if (aCallback)
            aCallback(result);
        });
    }
    else {
      if (aCallback)
        aCallback(null);
    }
  },

  /**
   * Return a list of all install locations known about by the database. This
   * is often a a subset of the total install locations when not all have
   * installed add-ons, occasionally a superset when an install location no
   * longer exists.
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
   * List all addons that match the filter function
   * @param  aFilter
   *         Function that takes an addon instance and returns
   *         true if that addon should be included in the selected array
   * @return an array of DBAddonInternals
   */
  _listAddons: function XPIDB_listAddons(aFilter) {
    if (!this.addonDB)
      return [];

    let addonList = [];
    for (let [key, addon] of this.addonDB) {
      if (aFilter(addon)) {
        addonList.push(addon);
      }
    }

    return addonList;
  },

  /**
   * Find the first addon that matches the filter function
   * @param  aFilter
   *         Function that takes an addon instance and returns
   *         true if that addon should be selected
   * @return The first DBAddonInternal for which the filter returns true
   */
  _findAddon: function XPIDB_findAddon(aFilter) {
    if (!this.addonDB)
      return null;

    for (let [key, addon] of this.addonDB) {
      if (aFilter(addon)) {
        return addon;
      }
    }

    return null;
  },

  /**
   * Synchronously reads all the add-ons in a particular install location.
   *
   * @param  aLocation
   *         The name of the install location
   * @return an array of DBAddonInternals
   */
  getAddonsInLocation: function XPIDB_getAddonsInLocation(aLocation) {
    return this._listAddons(function inLocation(aAddon) {return (aAddon.location == aLocation);});
  },

  /**
   * Asynchronously gets an add-on with a particular ID in a particular
   * install location.
   * XXX IRVING sync for now
   *
   * @param  aId
   *         The ID of the add-on to retrieve
   * @param  aLocation
   *         The name of the install location
   * @param  aCallback
   *         A callback to pass the DBAddonInternal to
   */
  getAddonInLocation: function XPIDB_getAddonInLocation(aId, aLocation, aCallback) {
    getRepositoryAddon(this.addonDB.get(aLocation + ":" + aId), aCallback);
  },

  /**
   * Asynchronously gets the add-on with an ID that is visible.
   * XXX IRVING sync
   *
   * @param  aId
   *         The ID of the add-on to retrieve
   * @param  aCallback
   *         A callback to pass the DBAddonInternal to
   */
  getVisibleAddonForID: function XPIDB_getVisibleAddonForID(aId, aCallback) {
    let addon = this._findAddon(function visibleID(aAddon) {return ((aAddon.id == aId) && aAddon.visible)});
    getRepositoryAddon(addon, aCallback);
  },

  /**
   * Asynchronously gets the visible add-ons, optionally restricting by type.
   * XXX IRVING sync
   *
   * @param  aTypes
   *         An array of types to include or null to include all types
   * @param  aCallback
   *         A callback to pass the array of DBAddonInternals to
   */
  getVisibleAddons: function XPIDB_getVisibleAddons(aTypes, aCallback) {
    let addons = this._listAddons(function visibleType(aAddon) {
      return (aAddon.visible && (!aTypes || (aTypes.length == 0) || (aTypes.indexOf(aAddon.type) > -1)))
    });
    asyncMap(addons, getRepositoryAddon, aCallback);
  },

  /**
   * Synchronously gets all add-ons of a particular type.
   *
   * @param  aType
   *         The type of add-on to retrieve
   * @return an array of DBAddonInternals
   */
  getAddonsByType: function XPIDB_getAddonsByType(aType) {
    return this._listAddons(function byType(aAddon) { return aAddon.type == aType; });
  },

  /**
   * Synchronously gets an add-on with a particular internalName.
   *
   * @param  aInternalName
   *         The internalName of the add-on to retrieve
   * @return a DBAddonInternal
   */
  getVisibleAddonForInternalName: function XPIDB_getVisibleAddonForInternalName(aInternalName) {
    return this._findAddon(function visibleInternalName(aAddon) {
      return (aAddon.visible && (aAddon.internalName == aInternalName));
    });
  },

  /**
   * Asynchronously gets all add-ons with pending operations.
   * XXX IRVING sync
   *
   * @param  aTypes
   *         The types of add-ons to retrieve or null to get all types
   * @param  aCallback
   *         A callback to pass the array of DBAddonInternal to
   */
  getVisibleAddonsWithPendingOperations:
    function XPIDB_getVisibleAddonsWithPendingOperations(aTypes, aCallback) {

    let addons = this._listAddons(function visibleType(aAddon) {
      return (aAddon.visible &&
        (aAddon.pendingUninstall ||
         // Logic here is tricky. If we're active but either
         // disabled flag is set, we're pending disable; if we're not
         // active and neither disabled flag is set, we're pending enable
         (aAddon.active == (aAddon.userDisabled || aAddon.appDisabled))) &&
        (!aTypes || (aTypes.length == 0) || (aTypes.indexOf(aAddon.type) > -1)))
    });
    asyncMap(addons, getRepositoryAddon, aCallback);
  },

  /**
   * Asynchronously get an add-on by its Sync GUID.
   * XXX IRVING sync
   *
   * @param  aGUID
   *         Sync GUID of add-on to fetch
   * @param  aCallback
   *         A callback to pass the DBAddonInternal record to. Receives null
   *         if no add-on with that GUID is found.
   *
   */
  getAddonBySyncGUID: function XPIDB_getAddonBySyncGUID(aGUID, aCallback) {
    let addon = this._findAddon(function bySyncGUID(aAddon) { return aAddon.syncGUID == aGUID; });
    getRepositoryAddon(addon, aCallback);
  },

  /**
   * Synchronously gets all add-ons in the database.
   *
   * @return  an array of DBAddonInternals
   */
  getAddons: function XPIDB_getAddons() {
    return this._listAddons(function(aAddon) {return true;});
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
    // If there is no DB yet then forcibly create one
    // XXX IRVING I don't think this will work as expected because the addonDB
    // getter will kick in. Might not matter because of the way the new DB
    // creates itself.
    if (!this.addonDB)
      this.openConnection(false, true);

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
   * @param  callback
   *         A callback to pass the DBAddonInternal to
   */
  makeAddonVisible: function XPIDB_makeAddonVisible(aAddon) {
    LOG("Make addon " + aAddon._key + " visible");
    for (let [key, otherAddon] of this.addonDB) {
      if ((otherAddon.id == aAddon.id) && (otherAddon._key != aAddon._key)) {
        LOG("Hide addon " + otherAddon._key);
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
    let otherAddon = this._findAddon(excludeSyncGUID);
    if (otherAddon) {
      throw new Error("Addon sync GUID conflict for addon " + aAddon._key +
          ": " + otherAddon._key + " already has GUID " + aGUID);
    }
    aAddon.syncGUID = aGUID;
    this.saveChanges();
  },

  /**
   * Synchronously sets the file descriptor for an add-on.
   * XXX IRVING could replace this with setAddonProperties
   *
   * @param  aAddon
   *         The DBAddonInternal being updated
   * @param  aDescriptor
   *         File path of the installed addon
   */
  setAddonDescriptor: function XPIDB_setAddonDescriptor(aAddon, aDescriptor) {
    aAddon.descriptor = aDescriptor;
    this.saveChanges();
  },

  /**
   * Synchronously updates an add-on's active flag in the database.
   *
   * @param  aAddon
   *         The DBAddonInternal to update
   */
  updateAddonActive: function XPIDB_updateAddonActive(aAddon, aActive) {
    LOG("Updating active state for add-on " + aAddon.id + " to " + aActive);

    aAddon.active = aActive;
    this.saveChanges();
  },

  /**
   * Synchronously calculates and updates all the active flags in the database.
   */
  updateActiveAddons: function XPIDB_updateActiveAddons() {
    // XXX IRVING this may get called during XPI-utils shutdown
    // XXX need to make sure PREF_PENDING_OPERATIONS handling is clean
    LOG("Updating add-on states");
    for (let [key, addon] of this.addonDB) {
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
   */
  writeAddonsList: function XPIDB_writeAddonsList() {
    Services.appinfo.invalidateCachesOnRestart();

    let addonsList = FileUtils.getFile(KEY_PROFILEDIR, [FILE_XPI_ADDONS_LIST],
                                       true);
    let enabledAddons = [];
    let text = "[ExtensionDirs]\r\n";
    let count = 0;
    let fullCount = 0;

    let activeAddons = this._listAddons(function active(aAddon) {
      return aAddon.active && !aAddon.bootstrap && (aAddon.type != "theme");
    });

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
      themes = this._listAddons(function isTheme(aAddon){ return aAddon.type == "theme"; });
    }
    else {
      let activeTheme = this._findAddon(function isSelected(aAddon) {
        return ((aAddon.type == "theme") && (aAddon.internalName == XPIProvider.selectedSkin));
      });
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
      LOG("Writing add-ons list");

      let addonsListTmp = FileUtils.getFile(KEY_PROFILEDIR, [FILE_XPI_ADDONS_LIST + ".tmp"],
                                            true);
      var fos = FileUtils.openFileOutputStream(addonsListTmp);
      fos.write(text, text.length);
      fos.close();
      addonsListTmp.moveTo(addonsListTmp.parent, FILE_XPI_ADDONS_LIST);

      Services.prefs.setCharPref(PREF_EM_ENABLED_ADDONS, enabledAddons.join(","));
    }
    else {
      if (addonsList.exists()) {
        LOG("Deleting add-ons list");
        addonsList.remove(false);
      }

      Services.prefs.clearUserPref(PREF_EM_ENABLED_ADDONS);
    }
  }
};
