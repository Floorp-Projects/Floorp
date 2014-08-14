/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MERGED_COMPARTMENT

"use strict";

this.EXPORTED_SYMBOLS = ["HealthReporter"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

const MILLISECONDS_PER_DAY = 24 * 60 * 60 * 1000;

Cu.import("resource://gre/modules/Metrics.jsm");
Cu.import("resource://services-common/async.js");

Cu.import("resource://services-common/bagheeraclient.js");
#endif

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/TelemetryStopwatch.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "UpdateChannel",
                                  "resource://gre/modules/UpdateChannel.jsm");

// Oldest year to allow in date preferences. This module was implemented in
// 2012 and no dates older than that should be encountered.
const OLDEST_ALLOWED_YEAR = 2012;

const DAYS_IN_PAYLOAD = 180;

const DEFAULT_DATABASE_NAME = "healthreport.sqlite";

const TELEMETRY_INIT = "HEALTHREPORT_INIT_MS";
const TELEMETRY_INIT_FIRSTRUN = "HEALTHREPORT_INIT_FIRSTRUN_MS";
const TELEMETRY_DB_OPEN = "HEALTHREPORT_DB_OPEN_MS";
const TELEMETRY_DB_OPEN_FIRSTRUN = "HEALTHREPORT_DB_OPEN_FIRSTRUN_MS";
const TELEMETRY_GENERATE_PAYLOAD = "HEALTHREPORT_GENERATE_JSON_PAYLOAD_MS";
const TELEMETRY_JSON_PAYLOAD_SERIALIZE = "HEALTHREPORT_JSON_PAYLOAD_SERIALIZE_MS";
const TELEMETRY_PAYLOAD_SIZE_UNCOMPRESSED = "HEALTHREPORT_PAYLOAD_UNCOMPRESSED_BYTES";
const TELEMETRY_PAYLOAD_SIZE_COMPRESSED = "HEALTHREPORT_PAYLOAD_COMPRESSED_BYTES";
const TELEMETRY_UPLOAD = "HEALTHREPORT_UPLOAD_MS";
const TELEMETRY_COLLECT_CONSTANT = "HEALTHREPORT_COLLECT_CONSTANT_DATA_MS";
const TELEMETRY_COLLECT_DAILY = "HEALTHREPORT_COLLECT_DAILY_MS";
const TELEMETRY_SHUTDOWN = "HEALTHREPORT_SHUTDOWN_MS";
const TELEMETRY_COLLECT_CHECKPOINT = "HEALTHREPORT_POST_COLLECT_CHECKPOINT_MS";


/**
 * Helper type to assist with management of Health Reporter state.
 *
 * Instances are not meant to be created outside of a HealthReporter instance.
 *
 * There are two types of IDs associated with clients.
 *
 * Since the beginning of FHR, there has existed a per-upload ID: a UUID is
 * generated at upload time and associated with the state before upload starts.
 * That same upload includes a request to delete all other upload IDs known by
 * the client.
 *
 * Per-upload IDs had the unintended side-effect of creating "orphaned"
 * records/upload IDs on the server. So, a stable client identifer has been
 * introduced. This client identifier is generated when it's missing and sent
 * as part of every upload.
 *
 * There is a high chance we may remove upload IDs in the future.
 */
function HealthReporterState(reporter) {
  this._reporter = reporter;

  let profD = OS.Constants.Path.profileDir;

  if (!profD || !profD.length) {
    throw new Error("Could not obtain profile directory. OS.File not " +
                    "initialized properly?");
  }

  this._log = reporter._log;

  this._stateDir = OS.Path.join(profD, "healthreport");

  // To facilitate testing.
  let leaf = reporter._stateLeaf || "state.json";

  this._filename = OS.Path.join(this._stateDir, leaf);
  this._log.debug("Storing state in " + this._filename);
  this._s = null;
}

HealthReporterState.prototype = Object.freeze({
  /**
   * Persistent string identifier associated with this client.
   */
  get clientID() {
    return this._s.clientID;
  },

  /**
   * The version associated with the client ID.
   */
  get clientIDVersion() {
    return this._s.clientIDVersion;
  },

  get lastPingDate() {
    return new Date(this._s.lastPingTime);
  },

  get lastSubmitID() {
    return this._s.remoteIDs[0];
  },

  get remoteIDs() {
    return this._s.remoteIDs;
  },

  get _lastPayloadPath() {
    return OS.Path.join(this._stateDir, "lastpayload.json");
  },

  init: function () {
    return Task.spawn(function init() {
      OS.File.makeDir(this._stateDir);

      let resetObjectState = function () {
        this._s = {
          // The payload version. This is bumped whenever there is a
          // backwards-incompatible change.
          v: 1,
          // The persistent client identifier.
          clientID: CommonUtils.generateUUID(),
          // Denotes the mechanism used to generate the client identifier.
          // 1: Random UUID.
          clientIDVersion: 1,
          // Upload IDs that might be on the server.
          remoteIDs: [],
          // When we last performed an uploaded.
          lastPingTime: 0,
          // Tracks whether we removed an outdated payload.
          removedOutdatedLastpayload: false,
        };
      }.bind(this);

      try {
        this._s = yield CommonUtils.readJSON(this._filename);
      } catch (ex if ex instanceof OS.File.Error &&
               ex.becauseNoSuchFile) {
        this._log.warn("Saved state file does not exist.");
        resetObjectState();
      } catch (ex) {
        this._log.error("Exception when reading state from disk: " +
                        CommonUtils.exceptionStr(ex));
        resetObjectState();

        // Don't save in case it goes away on next run.
      }

      if (typeof(this._s) != "object") {
        this._log.warn("Read state is not an object. Resetting state.");
        resetObjectState();
        yield this.save();
      }

      if (this._s.v != 1) {
        this._log.warn("Unknown version in state file: " + this._s.v);
        resetObjectState();
        // We explicitly don't save here in the hopes an application re-upgrade
        // comes along and fixes us.
      }

      let regen = false;
      if (!this._s.clientID) {
        this._log.warn("No client ID stored. Generating random ID.");
        regen = true;
      }

      if (typeof(this._s.clientID) != "string") {
        this._log.warn("Client ID is not a string. Regenerating.");
        regen = true;
      }

      if (regen) {
        this._s.clientID = CommonUtils.generateUUID();
        this._s.clientIDVersion = 1;
        yield this.save();
      }

      // Always look for preferences. This ensures that downgrades followed
      // by reupgrades don't result in excessive data loss.
      for (let promise of this._migratePrefs()) {
        yield promise;
      }
    }.bind(this));
  },

  save: function () {
    this._log.info("Writing state file: " + this._filename);
    return CommonUtils.writeJSON(this._s, this._filename);
  },

  addRemoteID: function (id) {
    this._log.warn("Recording new remote ID: " + id);
    this._s.remoteIDs.push(id);
    return this.save();
  },

  removeRemoteID: function (id) {
    return this.removeRemoteIDs(id ? [id] : []);
  },

  removeRemoteIDs: function (ids) {
    if (!ids || !ids.length) {
      this._log.warn("No IDs passed for removal.");
      return Promise.resolve();
    }

    this._log.warn("Removing documents from remote ID list: " + ids);
    let filtered = this._s.remoteIDs.filter((x) => ids.indexOf(x) === -1);

    if (filtered.length == this._s.remoteIDs.length) {
      return Promise.resolve();
    }

    this._s.remoteIDs = filtered;
    return this.save();
  },

  setLastPingDate: function (date) {
    this._s.lastPingTime = date.getTime();

    return this.save();
  },

  updateLastPingAndRemoveRemoteID: function (date, id) {
    return this.updateLastPingAndRemoveRemoteIDs(date, id ? [id] : []);
  },

  updateLastPingAndRemoveRemoteIDs: function (date, ids) {
    if (!ids) {
      return this.setLastPingDate(date);
    }

    this._log.info("Recording last ping time and deleted remote document.");
    this._s.lastPingTime = date.getTime();
    return this.removeRemoteIDs(ids);
  },

  /**
   * Reset the client ID to something else.
   *
   * This fails if remote IDs are stored because changing the client ID
   * while there is remote data will create orphaned records.
   */
  resetClientID: function () {
    if (this.remoteIDs.length) {
      throw new Error("Cannot reset client ID while remote IDs are stored.");
    }

    this._log.warn("Resetting client ID.");
    this._s.clientID = CommonUtils.generateUUID();
    this._s.clientIDVersion = 1;

    return this.save();
  },

  _migratePrefs: function () {
    let prefs = this._reporter._prefs;

    let lastID = prefs.get("lastSubmitID", null);
    let lastPingDate = CommonUtils.getDatePref(prefs, "lastPingTime",
                                               0, this._log, OLDEST_ALLOWED_YEAR);

    // If we have state from prefs, migrate and save it to a file then clear
    // out old prefs.
    if (lastID || (lastPingDate && lastPingDate.getTime() > 0)) {
      this._log.warn("Migrating saved state from preferences.");

      if (lastID) {
        this._log.info("Migrating last saved ID: " + lastID);
        this._s.remoteIDs.push(lastID);
      }

      let ourLast = this.lastPingDate;

      if (lastPingDate && lastPingDate.getTime() > ourLast.getTime()) {
        this._log.info("Migrating last ping time: " + lastPingDate);
        this._s.lastPingTime = lastPingDate.getTime();
      }

      yield this.save();
      prefs.reset(["lastSubmitID", "lastPingTime"]);
    } else {
      this._log.warn("No prefs data found.");
    }
  },
});

/**
 * This is the abstract base class of `HealthReporter`. It exists so that
 * we can sanely divide work on platforms where control of Firefox Health
 * Report is outside of Gecko (e.g., Android).
 */
function AbstractHealthReporter(branch, policy, sessionRecorder) {
  if (!branch.endsWith(".")) {
    throw new Error("Branch must end with a period (.): " + branch);
  }

  if (!policy) {
    throw new Error("Must provide policy to HealthReporter constructor.");
  }

  this._log = Log.repository.getLogger("Services.HealthReport.HealthReporter");
  this._log.info("Initializing health reporter instance against " + branch);

  this._branch = branch;
  this._prefs = new Preferences(branch);

  this._policy = policy;
  this.sessionRecorder = sessionRecorder;

  this._dbName = this._prefs.get("dbName") || DEFAULT_DATABASE_NAME;

  this._storage = null;
  this._storageInProgress = false;
  this._providerManager = null;
  this._providerManagerInProgress = false;
  this._initializeStarted = false;
  this._initialized = false;
  this._initializeHadError = false;
  this._initializedDeferred = Promise.defer();
  this._shutdownRequested = false;
  this._shutdownInitiated = false;
  this._shutdownComplete = false;
  this._deferredShutdown = Promise.defer();
  this._promiseShutdown = this._deferredShutdown.promise;

  this._errors = [];

  this._lastDailyDate = null;

  // Yes, this will probably run concurrently with remaining constructor work.
  let hasFirstRun = this._prefs.get("service.firstRun", false);
  this._initHistogram = hasFirstRun ? TELEMETRY_INIT : TELEMETRY_INIT_FIRSTRUN;
  this._dbOpenHistogram = hasFirstRun ? TELEMETRY_DB_OPEN : TELEMETRY_DB_OPEN_FIRSTRUN;
}

AbstractHealthReporter.prototype = Object.freeze({
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  /**
   * Whether the service is fully initialized and running.
   *
   * If this is false, it is not safe to call most functions.
   */
  get initialized() {
    return this._initialized;
  },

  /**
   * Initialize the instance.
   *
   * This must be called once after object construction or the instance is
   * useless.
   */
  init: function () {
    if (this._initializeStarted) {
      throw new Error("We have already started initialization.");
    }

    this._initializeStarted = true;

    return Task.spawn(function*() {
      TelemetryStopwatch.start(this._initHistogram, this);

      try {
        yield this._state.init();

        if (!this._state._s.removedOutdatedLastpayload) {
          yield this._deleteOldLastPayload();
          this._state._s.removedOutdatedLastpayload = true;
          // Normally we should save this to a file but it directly conflicts with
          // the "application re-upgrade" decision in HealthReporterState::init()
          // which specifically does not save the state to a file.
        }
      } catch (ex) {
        this._log.error("Error deleting last payload: " +
                        CommonUtils.exceptionStr(ex));
      }

      // As soon as we have could have storage, we need to register cleanup or
      // else bad things happen on shutdown.
      Services.obs.addObserver(this, "quit-application", false);

      // The database needs to be shut down by the end of shutdown
      // phase profileBeforeChange.
      Metrics.Storage.shutdown.addBlocker("FHR: Flushing storage shutdown",
        () => {
          // Workaround bug 1017706
          // Apparently, in some cases, quit-application is not triggered
          // (or is triggered after profile-before-change), so we need to
          // make sure that `_initiateShutdown()` is triggered at least
          // once.
          this._initiateShutdown();
          return this._promiseShutdown;
        },
        () => ({
            shutdownInitiated: this._shutdownInitiated,
            initialized: this._initialized,
            shutdownRequested: this._shutdownRequested,
            initializeHadError: this._initializeHadError,
            providerManagerInProgress: this._providerManagerInProgress,
            storageInProgress: this._storageInProgress,
            hasProviderManager: !!this._providerManager,
            hasStorage: !!this._storage,
            shutdownComplete: this._shutdownComplete
          }));

      try {
        this._storageInProgress = true;
        TelemetryStopwatch.start(this._dbOpenHistogram, this);
        let storage = yield Metrics.Storage(this._dbName);
        TelemetryStopwatch.finish(this._dbOpenHistogram, this);
        yield this._onStorageCreated();

        delete this._dbOpenHistogram;
        this._log.info("Storage initialized.");
        this._storage = storage;
        this._storageInProgress = false;

        if (this._shutdownRequested) {
          this._initiateShutdown();
          return null;
        }

        yield this._initializeProviderManager();
        yield this._onProviderManagerInitialized();
        this._initializedDeferred.resolve();
        return this.onInit();
      } catch (ex) {
        yield this._onInitError(ex);
        this._initializedDeferred.reject(ex);
      }
    }.bind(this));
  },

  //----------------------------------------------------
  // SERVICE CONTROL FUNCTIONS
  //
  // You shouldn't need to call any of these externally.
  //----------------------------------------------------

  _onInitError: function (error) {
    TelemetryStopwatch.cancel(this._initHistogram, this);
    TelemetryStopwatch.cancel(this._dbOpenHistogram, this);
    delete this._initHistogram;
    delete this._dbOpenHistogram;

    this._recordError("Error during initialization", error);
    this._initializeHadError = true;
    this._initiateShutdown();
    return Promise.reject(error);

    // FUTURE consider poisoning prototype's functions so calls fail with a
    // useful error message.
  },


  /**
   * Removes the outdated lastpaylaod.json and lastpayload.json.tmp files
   * @see Bug #867902
   * @return a promise for when all the files have been deleted
   */
  _deleteOldLastPayload: function () {
    let paths = [this._state._lastPayloadPath, this._state._lastPayloadPath + ".tmp"];
    return Task.spawn(function removeAllFiles () {
      for (let path of paths) {
        try {
          OS.File.remove(path);
        } catch (ex) {
          if (!ex.becauseNoSuchFile) {
            this._log.error("Exception when removing outdated payload files: " +
                            CommonUtils.exceptionStr(ex));
          }
        }
      }
    }.bind(this));
  },

  _initializeProviderManager: Task.async(function* _initializeProviderManager() {
    if (this._collector) {
      throw new Error("Provider manager has already been initialized.");
    }

    this._log.info("Initializing provider manager.");
    this._providerManager = new Metrics.ProviderManager(this._storage);
    this._providerManager.onProviderError = this._recordError.bind(this);
    this._providerManager.onProviderInit = this._initProvider.bind(this);
    this._providerManagerInProgress = true;

    let catString = this._prefs.get("service.providerCategories") || "";
    if (catString.length) {
      for (let category of catString.split(",")) {
        yield this._providerManager.registerProvidersFromCategoryManager(category);
      }
    }
  }),

  _onProviderManagerInitialized: function () {
    TelemetryStopwatch.finish(this._initHistogram, this);
    delete this._initHistogram;
    this._log.debug("Provider manager initialized.");
    this._providerManagerInProgress = false;

    if (this._shutdownRequested) {
      this._initiateShutdown();
      return;
    }

    this._log.info("HealthReporter started.");
    this._initialized = true;
    Services.obs.addObserver(this, "idle-daily", false);

    // If upload is not enabled, ensure daily collection works. If upload
    // is enabled, this will be performed as part of upload.
    //
    // This is important because it ensures about:healthreport contains
    // longitudinal data even if upload is disabled. Having about:healthreport
    // provide useful info even if upload is disabled was a core launch
    // requirement.
    //
    // We do not catch changes to the backing pref. So, if the session lasts
    // many days, we may fail to collect. However, most sessions are short and
    // this code will likely be refactored as part of splitting up policy to
    // serve Android. So, meh.
    if (!this._policy.healthReportUploadEnabled) {
      this._log.info("Upload not enabled. Scheduling daily collection.");
      // Since the timer manager is a singleton and there could be multiple
      // HealthReporter instances, we need to encode a unique identifier in
      // the timer ID.
      try {
        let timerName = this._branch.replace(".", "-", "g") + "lastDailyCollection";
        let tm = Cc["@mozilla.org/updates/timer-manager;1"]
                   .getService(Ci.nsIUpdateTimerManager);
        tm.registerTimer(timerName, this.collectMeasurements.bind(this),
                         24 * 60 * 60);
      } catch (ex) {
        this._log.error("Error registering collection timer: " +
                        CommonUtils.exceptionStr(ex));
      }
    }

    // Clean up caches and reduce memory usage.
    this._storage.compact();
  },

  // nsIObserver to handle shutdown.
  observe: function (subject, topic, data) {
    switch (topic) {
      case "quit-application":
        Services.obs.removeObserver(this, "quit-application");
        this._initiateShutdown();
        break;

      case "idle-daily":
        this._performDailyMaintenance();
        break;
    }
  },

  _initiateShutdown: function () {
    // Ensure we only begin the main shutdown sequence once.
    if (this._shutdownInitiated) {
      this._log.warn("Shutdown has already been initiated. No-op.");
      return;
    }

    this._log.info("Request to shut down.");

    this._initialized = false;
    this._shutdownRequested = true;

    if (this._initializeHadError) {
      this._log.warn("Initialization had error. Shutting down immediately.");
    } else {
      if (this._providerManagerInProgress) {
        this._log.warn("Provider manager is in progress of initializing. " +
                       "Waiting to finish.");
        return;
      }

      // If storage is in the process of initializing, we need to wait for it
      // to finish before continuing. The initialization process will call us
      // again once storage has initialized.
      if (this._storageInProgress) {
        this._log.warn("Storage is in progress of initializing. Waiting to finish.");
        return;
      }
    }

    this._log.warn("Initiating main shutdown procedure.");

    // Everything from here must only be performed once or else race conditions
    // could occur.

    TelemetryStopwatch.start(TELEMETRY_SHUTDOWN, this);
    this._shutdownInitiated = true;

    // We may not have registered the observer yet. If not, this will
    // throw.
    try {
      Services.obs.removeObserver(this, "idle-daily");
    } catch (ex) { }

    Task.spawn(function*() {
      try {
        if (this._providerManager) {
          this._log.info("Shutting down provider manager.");
          for (let provider of this._providerManager.providers) {
            try {
              yield provider.shutdown();
            } catch (ex) {
              this._log.warn("Error when shutting down provider: " +
                             CommonUtils.exceptionStr(ex));
            }
          }
          this._log.info("Provider manager shut down.");
          this._providerManager = null;
          this._onProviderManagerShutdown();
        }
        if (this._storage) {
          this._log.info("Shutting down storage.");
          try {
            yield this._storage.close();
            yield this._onStorageClose();
          } catch (error) {
            this._log.warn("Error when closing storage: " +
                           CommonUtils.exceptionStr(error));
          }
          this._storage = null;
        }

        this._log.warn("Shutdown complete.");
        this._shutdownComplete = true;
      } finally {
        this._deferredShutdown.resolve();
        TelemetryStopwatch.finish(TELEMETRY_SHUTDOWN, this);
      }
    }.bind(this));
  },

  onInit: function() {
    return this._initializedDeferred.promise;
  },

  _onStorageCreated: function() {
    // Do nothing.
    // This method provides a hook point for the test suite.
  },

  _onStorageClose: function() {
    // Do nothing.
    // This method provides a hook point for the test suite.
  },

  _onProviderManagerShutdown: function() {
    // Do nothing.
    // This method provides a hook point for the test suite.
  },

  /**
   * Convenience method to shut down the instance.
   *
   * This should *not* be called outside of tests.
   */
  _shutdown: function () {
    this._initiateShutdown();
    return this._promiseShutdown;
  },

  _performDailyMaintenance: function () {
    this._log.info("Request to perform daily maintenance.");

    if (!this._initialized) {
      return;
    }

    let now = new Date();
    let cutoff = new Date(now.getTime() - MILLISECONDS_PER_DAY * (DAYS_IN_PAYLOAD - 1));

    // The operation is enqueued and put in a transaction by the storage module.
    this._storage.pruneDataBefore(cutoff);
  },

  //--------------------
  // Provider Management
  //--------------------

  /**
   * Obtain a provider from its name.
   *
   * This will only return providers that are currently initialized. If
   * a provider is lazy initialized (like pull-only providers) this
   * will likely not return anything.
   */
  getProvider: function (name) {
    if (!this._providerManager) {
      return null;
    }

    return this._providerManager.getProvider(name);
  },

  _initProvider: function (provider) {
    provider.healthReporter = this;
  },

  /**
   * Record an exception for reporting in the payload.
   *
   * A side effect is the exception is logged.
   *
   * Note that callers need to be extra sensitive about ensuring personal
   * or otherwise private details do not leak into this. All of the user data
   * on the stack in FHR code should be limited to data we were collecting with
   * the intent to submit. So, it is covered under the user's consent to use
   * the feature.
   *
   * @param message
   *        (string) Human readable message describing error.
   * @param ex
   *        (Error) The error that should be captured.
   */
  _recordError: function (message, ex) {
    let recordMessage = message;
    let logMessage = message;

    if (ex) {
      recordMessage += ": " + CommonUtils.exceptionStr(ex);
      logMessage += ": " + CommonUtils.exceptionStr(ex);
    }

    // Scrub out potentially identifying information from strings that could
    // make the payload.
    let appData = Services.dirsvc.get("UAppData", Ci.nsIFile);
    let profile = Services.dirsvc.get("ProfD", Ci.nsIFile);

    let appDataURI = Services.io.newFileURI(appData);
    let profileURI = Services.io.newFileURI(profile);

    // Order of operation is important here. We do the URI before the path version
    // because the path may be a subset of the URI. We also have to check for the case
    // where UAppData is underneath the profile directory (or vice-versa) so we
    // don't substitute incomplete strings.

    function replace(uri, path, thing) {
      // Try is because .spec can throw on invalid URI.
      try {
        recordMessage = recordMessage.replace(uri.spec, '<' + thing + 'URI>', 'g');
      } catch (ex) { }

      recordMessage = recordMessage.replace(path, '<' + thing + 'Path>', 'g');
    }

    if (appData.path.contains(profile.path)) {
      replace(appDataURI, appData.path, 'AppData');
      replace(profileURI, profile.path, 'Profile');
    } else {
      replace(profileURI, profile.path, 'Profile');
      replace(appDataURI, appData.path, 'AppData');
    }

    this._log.warn(logMessage);
    this._errors.push(recordMessage);
  },

  /**
   * Collect all measurements for all registered providers.
   */
  collectMeasurements: function () {
    if (!this._initialized) {
      return Promise.reject(new Error("Not initialized."));
    }

    return Task.spawn(function doCollection() {
      yield this._providerManager.ensurePullOnlyProvidersRegistered();

      try {
        TelemetryStopwatch.start(TELEMETRY_COLLECT_CONSTANT, this);
        yield this._providerManager.collectConstantData();
        TelemetryStopwatch.finish(TELEMETRY_COLLECT_CONSTANT, this);
      } catch (ex) {
        TelemetryStopwatch.cancel(TELEMETRY_COLLECT_CONSTANT, this);
        this._log.warn("Error collecting constant data: " +
                       CommonUtils.exceptionStr(ex));
      }

      // Daily data is collected if it hasn't yet been collected this
      // application session or if it has been more than a day since the
      // last collection. This means that providers could see many calls to
      // collectDailyData per calendar day. However, this collection API
      // makes no guarantees about limits. The alternative would involve
      // recording state. The simpler implementation prevails for now.
      if (!this._lastDailyDate ||
          Date.now() - this._lastDailyDate > MILLISECONDS_PER_DAY) {

        try {
          TelemetryStopwatch.start(TELEMETRY_COLLECT_DAILY, this);
          this._lastDailyDate = new Date();
          yield this._providerManager.collectDailyData();
          TelemetryStopwatch.finish(TELEMETRY_COLLECT_DAILY, this);
        } catch (ex) {
          TelemetryStopwatch.cancel(TELEMETRY_COLLECT_DAILY, this);
          this._log.warn("Error collecting daily data from providers: " +
                         CommonUtils.exceptionStr(ex));
        }
      }

      yield this._providerManager.ensurePullOnlyProvidersUnregistered();

      // Flush gathered data to disk. This will incur an fsync. But, if
      // there is ever a time we want to persist data to disk, it's
      // after a massive collection.
      try {
        TelemetryStopwatch.start(TELEMETRY_COLLECT_CHECKPOINT, this);
        yield this._storage.checkpoint();
        TelemetryStopwatch.finish(TELEMETRY_COLLECT_CHECKPOINT, this);
      } catch (ex) {
        TelemetryStopwatch.cancel(TELEMETRY_COLLECT_CHECKPOINT, this);
        throw ex;
      }

      throw new Task.Result();
    }.bind(this));
  },

  /**
   * Helper function to perform data collection and obtain the JSON payload.
   *
   * If you are looking for an up-to-date snapshot of FHR data that pulls in
   * new data since the last upload, this is how you should obtain it.
   *
   * @param asObject
   *        (bool) Whether to resolve an object or JSON-encoded string of that
   *        object (the default).
   *
   * @return Promise<Object | string>
   */
  collectAndObtainJSONPayload: function (asObject=false) {
    if (!this._initialized) {
      return Promise.reject(new Error("Not initialized."));
    }

    return Task.spawn(function collectAndObtain() {
      yield this._storage.setAutoCheckpoint(0);
      yield this._providerManager.ensurePullOnlyProvidersRegistered();

      let payload;
      let error;

      try {
        yield this.collectMeasurements();
        payload = yield this.getJSONPayload(asObject);
      } catch (ex) {
        error = ex;
        this._collectException("Error collecting and/or retrieving JSON payload",
                               ex);
      } finally {
        yield this._providerManager.ensurePullOnlyProvidersUnregistered();
        yield this._storage.setAutoCheckpoint(1);

        if (error) {
          throw error;
        }
      }

      // We hold off throwing to ensure that behavior between finally
      // and generators and throwing is sane.
      throw new Task.Result(payload);
    }.bind(this));
  },


  /**
   * Obtain the JSON payload for currently-collected data.
   *
   * The payload only contains data that has been recorded to FHR. Some
   * providers may have newer data available. If you want to ensure you
   * have all available data, call `collectAndObtainJSONPayload`
   * instead.
   *
   * @param asObject
   *        (bool) Whether to return an object or JSON encoding of that
   *        object (the default).
   *
   * @return Promise<string|object>
   */
  getJSONPayload: function (asObject=false) {
    TelemetryStopwatch.start(TELEMETRY_GENERATE_PAYLOAD, this);
    let deferred = Promise.defer();

    Task.spawn(this._getJSONPayload.bind(this, this._now(), asObject)).then(
      function onResult(result) {
        TelemetryStopwatch.finish(TELEMETRY_GENERATE_PAYLOAD, this);
        deferred.resolve(result);
      }.bind(this),
      function onError(error) {
        TelemetryStopwatch.cancel(TELEMETRY_GENERATE_PAYLOAD, this);
        deferred.reject(error);
      }.bind(this)
    );

    return deferred.promise;
  },

  _getJSONPayload: function (now, asObject=false) {
    let pingDateString = this._formatDate(now);
    this._log.info("Producing JSON payload for " + pingDateString);

    // May not be present if we are generating as a result of init error.
    if (this._providerManager) {
      yield this._providerManager.ensurePullOnlyProvidersRegistered();
    }

    let o = {
      version: 2,
      clientID: this._state.clientID,
      clientIDVersion: this._state.clientIDVersion,
      thisPingDate: pingDateString,
      geckoAppInfo: this.obtainAppInfo(this._log),
      data: {last: {}, days: {}},
    };

    let outputDataDays = o.data.days;

    // Guard here in case we don't track this (e.g., on Android).
    let lastPingDate = this.lastPingDate;
    if (lastPingDate && lastPingDate.getTime() > 0) {
      o.lastPingDate = this._formatDate(lastPingDate);
    }

    // We can still generate a payload even if we're not initialized.
    // This is to facilitate error upload on init failure.
    if (this._initialized) {
      for (let provider of this._providerManager.providers) {
        let providerName = provider.name;

        let providerEntry = {
          measurements: {},
        };

        // Measurement name to recorded version.
        let lastVersions = {};
        // Day string to mapping of measurement name to recorded version.
        let dayVersions = {};

        for (let [measurementKey, measurement] of provider.measurements) {
          let name = providerName + "." + measurement.name;
          let version = measurement.version;

          let serializer;
          try {
            // The measurement is responsible for returning a serializer which
            // is aware of the measurement version.
            serializer = measurement.serializer(measurement.SERIALIZE_JSON);
          } catch (ex) {
            this._recordError("Error obtaining serializer for measurement: " +
                              name, ex);
            continue;
          }

          let data;
          try {
            data = yield measurement.getValues();
          } catch (ex) {
            this._recordError("Error obtaining data for measurement: " + name,
                              ex);
            continue;
          }

          if (data.singular.size) {
            try {
              let serialized = serializer.singular(data.singular);
              if (serialized) {
                // Only replace the existing data if there is no data or if our
                // version is newer than the old one.
                if (!(name in o.data.last) || version > lastVersions[name]) {
                  o.data.last[name] = serialized;
                  lastVersions[name] = version;
                }
              }
            } catch (ex) {
              this._recordError("Error serializing singular data: " + name,
                                ex);
              continue;
            }
          }

          let dataDays = data.days;
          for (let i = 0; i < DAYS_IN_PAYLOAD; i++) {
            let date = new Date(now.getTime() - i * MILLISECONDS_PER_DAY);
            if (!dataDays.hasDay(date)) {
              continue;
            }
            let dateFormatted = this._formatDate(date);

            try {
              let serialized = serializer.daily(dataDays.getDay(date));
              if (!serialized) {
                continue;
              }

              if (!(dateFormatted in outputDataDays)) {
                outputDataDays[dateFormatted] = {};
              }

              // This needs to be separate because dayVersions is provider
              // specific and gets blown away in a loop while outputDataDays
              // is persistent.
              if (!(dateFormatted in dayVersions)) {
                dayVersions[dateFormatted] = {};
              }

              if (!(name in outputDataDays[dateFormatted]) ||
                  version > dayVersions[dateFormatted][name]) {
                outputDataDays[dateFormatted][name] = serialized;
                dayVersions[dateFormatted][name] = version;
              }
            } catch (ex) {
              this._recordError("Error populating data for day: " + name, ex);
              continue;
            }
          }
        }
      }
    } else {
      o.notInitialized = 1;
      this._log.warn("Not initialized. Sending report with only error info.");
    }

    if (this._errors.length) {
      o.errors = this._errors.slice(0, 20);
    }

    if (this._initialized) {
      this._storage.compact();
    }

    if (!asObject) {
      TelemetryStopwatch.start(TELEMETRY_JSON_PAYLOAD_SERIALIZE, this);
      o = JSON.stringify(o);
      TelemetryStopwatch.finish(TELEMETRY_JSON_PAYLOAD_SERIALIZE, this);
    }

    if (this._providerManager) {
      yield this._providerManager.ensurePullOnlyProvidersUnregistered();
    }

    throw new Task.Result(o);
  },

  _now: function _now() {
    return new Date();
  },

  // These are stolen from AppInfoProvider.
  appInfoVersion: 1,
  appInfoFields: {
    // From nsIXULAppInfo.
    vendor: "vendor",
    name: "name",
    id: "ID",
    version: "version",
    appBuildID: "appBuildID",
    platformVersion: "platformVersion",
    platformBuildID: "platformBuildID",

    // From nsIXULRuntime.
    os: "OS",
    xpcomabi: "XPCOMABI",
  },

  /**
   * Statically return a bundle of app info data, a subset of that produced by
   * AppInfoProvider._populateConstants. This allows us to more usefully handle
   * payloads that, due to error, contain no data.
   *
   * Returns a very sparse object if Services.appinfo is unavailable.
   */
  obtainAppInfo: function () {
    let out = {"_v": this.appInfoVersion};
    try {
      let ai = Services.appinfo;
      for (let [k, v] in Iterator(this.appInfoFields)) {
        out[k] = ai[v];
      }
    } catch (ex) {
      this._log.warn("Could not obtain Services.appinfo: " +
                     CommonUtils.exceptionStr(ex));
    }

    try {
      out["updateChannel"] = UpdateChannel.get();
    } catch (ex) {
      this._log.warn("Could not obtain update channel: " +
                     CommonUtils.exceptionStr(ex));
    }

    return out;
  },
});

/**
 * HealthReporter and its abstract superclass coordinate collection and
 * submission of health report metrics.
 *
 * This is the main type for Firefox Health Report on desktop. It glues all the
 * lower-level components (such as collection and submission) together.
 *
 * An instance of this type is created as an XPCOM service. See
 * DataReportingService.js and
 * DataReporting.manifest/HealthReportComponents.manifest.
 *
 * It is theoretically possible to have multiple instances of this running
 * in the application. For example, this type may one day handle submission
 * of telemetry data as well. However, there is some moderate coupling between
 * this type and *the* Firefox Health Report (e.g., the policy). This could
 * be abstracted if needed.
 *
 * Note that `AbstractHealthReporter` exists to allow for Firefox Health Report
 * to be more easily implemented on platforms where a separate controlling
 * layer is responsible for payload upload and deletion.
 *
 * IMPLEMENTATION NOTES
 * ====================
 *
 * These notes apply to the combination of `HealthReporter` and
 * `AbstractHealthReporter`.
 *
 * Initialization and shutdown are somewhat complicated and worth explaining
 * in extra detail.
 *
 * The complexity is driven by the requirements of SQLite connection management.
 * Once you have a SQLite connection, it isn't enough to just let the
 * application shut down. If there is an open connection or if there are
 * outstanding SQL statements come XPCOM shutdown time, Storage will assert.
 * On debug builds you will crash. On release builds you will get a shutdown
 * hang. This must be avoided!
 *
 * During initialization, the second we create a SQLite connection (via
 * Metrics.Storage) we register observers for application shutdown. The
 * "quit-application" notification initiates our shutdown procedure. The
 * subsequent "profile-do-change" notification ensures it has completed.
 *
 * The handler for "profile-do-change" may result in event loop spinning. This
 * is because of race conditions between our shutdown code and application
 * shutdown.
 *
 * All of our shutdown routines are async. There is the potential that these
 * async functions will not complete before XPCOM shutdown. If they don't
 * finish in time, we could get assertions in Storage. Our solution is to
 * initiate storage early in the shutdown cycle ("quit-application").
 * Hopefully all the async operations have completed by the time we reach
 * "profile-do-change." If so, great. If not, we spin the event loop until
 * they have completed, avoiding potential race conditions.
 *
 * @param branch
 *        (string) The preferences branch to use for state storage. The value
 *        must end with a period (.).
 *
 * @param policy
 *        (HealthReportPolicy) Policy driving execution of HealthReporter.
 */
this.HealthReporter = function (branch, policy, sessionRecorder, stateLeaf=null) {
  this._stateLeaf = stateLeaf;
  this._uploadInProgress = false;

  AbstractHealthReporter.call(this, branch, policy, sessionRecorder);

  if (!this.serverURI) {
    throw new Error("No server URI defined. Did you forget to define the pref?");
  }

  if (!this.serverNamespace) {
    throw new Error("No server namespace defined. Did you forget a pref?");
  }

  this._state = new HealthReporterState(this);
}

this.HealthReporter.prototype = Object.freeze({
  __proto__: AbstractHealthReporter.prototype,

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  get lastSubmitID() {
    return this._state.lastSubmitID;
  },

  /**
   * When we last successfully submitted data to the server.
   *
   * This is sent as part of the upload. This is redundant with similar data
   * in the policy because we like the modules to be loosely coupled and the
   * similar data in the policy is only used for forensic purposes.
   */
  get lastPingDate() {
    return this._state.lastPingDate;
  },

  /**
   * The base URI of the document server to which to submit data.
   *
   * This is typically a Bagheera server instance. It is the URI up to but not
   * including the version prefix. e.g. https://data.metrics.mozilla.com/
   */
  get serverURI() {
    return this._prefs.get("documentServerURI", null);
  },

  set serverURI(value) {
    if (!value) {
      throw new Error("serverURI must have a value.");
    }

    if (typeof(value) != "string") {
      throw new Error("serverURI must be a string: " + value);
    }

    this._prefs.set("documentServerURI", value);
  },

  /**
   * The namespace on the document server to which we will be submitting data.
   */
  get serverNamespace() {
    return this._prefs.get("documentServerNamespace", "metrics");
  },

  set serverNamespace(value) {
    if (!value) {
      throw new Error("serverNamespace must have a value.");
    }

    if (typeof(value) != "string") {
      throw new Error("serverNamespace must be a string: " + value);
    }

    this._prefs.set("documentServerNamespace", value);
  },

  /**
   * Whether this instance will upload data to a server.
   */
  get willUploadData() {
    return this._policy.dataSubmissionPolicyAccepted &&
           this._policy.healthReportUploadEnabled;
  },

  /**
   * Whether remote data is currently stored.
   *
   * @return bool
   */
  haveRemoteData: function () {
    return !!this._state.lastSubmitID;
  },

  /**
   * Called to initiate a data upload.
   *
   * The passed argument is a `DataSubmissionRequest` from policy.jsm.
   */
  requestDataUpload: function (request) {
    if (!this._initialized) {
      return Promise.reject(new Error("Not initialized."));
    }

    return Task.spawn(function doUpload() {
      yield this._providerManager.ensurePullOnlyProvidersRegistered();
      try {
        yield this.collectMeasurements();
        try {
          yield this._uploadData(request);
        } catch (ex) {
          this._onSubmitDataRequestFailure(ex);
        }
      } finally {
        yield this._providerManager.ensurePullOnlyProvidersUnregistered();
      }
    }.bind(this));
  },

  /**
   * Request that server data be deleted.
   *
   * If deletion is scheduled to occur immediately, a promise will be returned
   * that will be fulfilled when the deletion attempt finishes. Otherwise,
   * callers should poll haveRemoteData() to determine when remote data is
   * deleted.
   */
  requestDeleteRemoteData: function (reason) {
    if (!this.haveRemoteData()) {
      return;
    }

    return this._policy.deleteRemoteData(reason);
  },

  /**
   * Override default handler to incur an upload describing the error.
   */
  _onInitError: function (error) {
    // Need to capture this before we call the parent else it's always
    // set.
    let inShutdown = this._shutdownRequested;

    let result;
    try {
      result = AbstractHealthReporter.prototype._onInitError.call(this, error);
    } catch (ex) {
      this._log.error("Error when calling _onInitError: " +
                      CommonUtils.exceptionStr(ex));
    }

    // This bypasses a lot of the checks in policy, such as respect for
    // backoff. We should arguably not do this. However, reporting
    // startup errors is important. And, they should not occur with much
    // frequency in the wild. So, it shouldn't be too big of a deal.
    if (!inShutdown &&
        this._policy.ensureNotifyResponse(new Date()) &&
        this._policy.healthReportUploadEnabled) {
      // We don't care about what happens to this request. It's best
      // effort.
      let request = {
        onNoDataAvailable: function () {},
        onSubmissionSuccess: function () {},
        onSubmissionFailureSoft: function () {},
        onSubmissionFailureHard: function () {},
        onUploadInProgress: function () {},
      };

      this._uploadData(request);
    }

    return result;
  },

  _onBagheeraResult: function (request, isDelete, date, result) {
    this._log.debug("Received Bagheera result.");

    return Task.spawn(function onBagheeraResult() {
      let hrProvider = this.getProvider("org.mozilla.healthreport");

      if (!result.transportSuccess) {
        // The built-in provider may not be initialized if this instance failed
        // to initialize fully.
        if (hrProvider && !isDelete) {
          try {
            hrProvider.recordEvent("uploadTransportFailure", date);
          } catch (ex) {
            this._log.error("Error recording upload transport failure: " +
                            CommonUtils.exceptionStr(ex));
          }
        }

        request.onSubmissionFailureSoft("Network transport error.");
        throw new Task.Result(false);
      }

      if (!result.serverSuccess) {
        if (hrProvider && !isDelete) {
          try {
            hrProvider.recordEvent("uploadServerFailure", date);
          } catch (ex) {
            this._log.error("Error recording server failure: " +
                            CommonUtils.exceptionStr(ex));
          }
        }

        request.onSubmissionFailureHard("Server failure.");
        throw new Task.Result(false);
      }

      if (hrProvider && !isDelete) {
        try {
          hrProvider.recordEvent("uploadSuccess", date);
        } catch (ex) {
          this._log.error("Error recording upload success: " +
                          CommonUtils.exceptionStr(ex));
        }
      }

      if (isDelete) {
        this._log.warn("Marking delete as successful.");
        yield this._state.removeRemoteIDs([result.id]);
      } else {
        this._log.warn("Marking upload as successful.");
        yield this._state.updateLastPingAndRemoveRemoteIDs(date, result.deleteIDs);
      }

      request.onSubmissionSuccess(this._now());

      throw new Task.Result(true);
    }.bind(this));
  },

  _onSubmitDataRequestFailure: function (error) {
    this._log.error("Error processing request to submit data: " +
                    CommonUtils.exceptionStr(error));
  },

  _formatDate: function (date) {
    // Why, oh, why doesn't JS have a strftime() equivalent?
    return date.toISOString().substr(0, 10);
  },

  _uploadData: function (request) {
    // Under ideal circumstances, clients should never race to this
    // function. However, server logs have observed behavior where
    // racing to this function could be a cause. So, this lock was
    // instituted.
    if (this._uploadInProgress) {
      this._log.warn("Upload requested but upload already in progress.");
      let provider = this.getProvider("org.mozilla.healthreport");
      let promise = provider.recordEvent("uploadAlreadyInProgress");
      request.onUploadInProgress("Upload already in progress.");
      return promise;
    }

    let id = CommonUtils.generateUUID();

    this._log.info("Uploading data to server: " + this.serverURI + " " +
                   this.serverNamespace + ":" + id);
    let client = new BagheeraClient(this.serverURI);
    let now = this._now();

    return Task.spawn(function doUpload() {
      try {
        // The test for upload locking monkeypatches getJSONPayload.
        // If the next two lines change, be sure to verify the test is
        // accurate!
        this._uploadInProgress = true;
        let payload = yield this.getJSONPayload();

        let histogram = Services.telemetry.getHistogramById(TELEMETRY_PAYLOAD_SIZE_UNCOMPRESSED);
        histogram.add(payload.length);

        let lastID = this.lastSubmitID;
        yield this._state.addRemoteID(id);

        let hrProvider = this.getProvider("org.mozilla.healthreport");
        if (hrProvider) {
          let event = lastID ? "continuationUploadAttempt"
                             : "firstDocumentUploadAttempt";
          try {
            hrProvider.recordEvent(event, now);
          } catch (ex) {
            this._log.error("Error when recording upload attempt: " +
                            CommonUtils.exceptionStr(ex));
          }
        }

        TelemetryStopwatch.start(TELEMETRY_UPLOAD, this);
        let result;
        try {
          let options = {
            deleteIDs: this._state.remoteIDs.filter((x) => { return x != id; }),
            telemetryCompressed: TELEMETRY_PAYLOAD_SIZE_COMPRESSED,
          };
          result = yield client.uploadJSON(this.serverNamespace, id, payload,
                                           options);
          TelemetryStopwatch.finish(TELEMETRY_UPLOAD, this);
        } catch (ex) {
          TelemetryStopwatch.cancel(TELEMETRY_UPLOAD, this);
          if (hrProvider) {
            try {
              hrProvider.recordEvent("uploadClientFailure", now);
            } catch (ex) {
              this._log.error("Error when recording client failure: " +
                              CommonUtils.exceptionStr(ex));
            }
          }
          throw ex;
        }

        yield this._onBagheeraResult(request, false, now, result);
      } finally {
        this._uploadInProgress = false;
      }
    }.bind(this));
  },

  /**
   * Request deletion of remote data.
   *
   * @param request
   *        (DataSubmissionRequest) Tracks progress of this request.
   */
  deleteRemoteData: function (request) {
    if (!this._state.lastSubmitID) {
      this._log.info("Received request to delete remote data but no data stored.");
      request.onNoDataAvailable();
      return;
    }

    this._log.warn("Deleting remote data.");
    let client = new BagheeraClient(this.serverURI);

    return Task.spawn(function* doDelete() {
      try {
        let result = yield client.deleteDocument(this.serverNamespace,
                                                 this.lastSubmitID);
        yield this._onBagheeraResult(request, true, this._now(), result);
      } catch (ex) {
        this._log.error("Error processing request to delete data: " +
                        CommonUtils.exceptionStr(error));
      } finally {
        // If we don't have any remote documents left, nuke the ID.
        // This is done for privacy reasons. Why preserve the ID if we
        // don't need to?
        if (!this.haveRemoteData()) {
          yield this._state.resetClientID();
        }
      }
    }.bind(this));
  },
});

