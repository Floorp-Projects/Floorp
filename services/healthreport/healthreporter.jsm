/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["HealthReporter"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://services-common/async.js");
Cu.import("resource://services-common/bagheeraclient.js");
Cu.import("resource://services-common/log4moz.js");
Cu.import("resource://services-common/preferences.js");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://gre/modules/commonjs/promise/core.js");
Cu.import("resource://gre/modules/Metrics.jsm");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");


// Oldest year to allow in date preferences. This module was implemented in
// 2012 and no dates older than that should be encountered.
const OLDEST_ALLOWED_YEAR = 2012;

const DAYS_IN_PAYLOAD = 180;
const MILLISECONDS_PER_DAY = 24 * 60 * 60 * 1000;

const DEFAULT_DATABASE_NAME = "healthreport.sqlite";


/**
 * Coordinates collection and submission of health report metrics.
 *
 * This is the main type for Firefox Health Report. It glues all the
 * lower-level components (such as collection and submission) together.
 *
 * An instance of this type is created as an XPCOM service. See
 * HealthReportService.js and HealthReportComponents.manifest.
 *
 * It is theoretically possible to have multiple instances of this running
 * in the application. For example, this type may one day handle submission
 * of telemetry data as well. However, there is some moderate coupling between
 * this type and *the* Firefox Health Report (e.g. the policy). This could
 * be abstracted if needed.
 *
 * IMPLEMENTATION NOTES
 * ====================
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
function HealthReporter(branch, policy) {
  if (!branch.endsWith(".")) {
    throw new Error("Branch must end with a period (.): " + branch);
  }

  if (!policy) {
    throw new Error("Must provide policy to HealthReporter constructor.");
  }

  this._log = Log4Moz.repository.getLogger("Services.HealthReport.HealthReporter");
  this._log.info("Initializing health reporter instance against " + branch);

  this._prefs = new Preferences(branch);

  if (!this.serverURI) {
    throw new Error("No server URI defined. Did you forget to define the pref?");
  }

  if (!this.serverNamespace) {
    throw new Error("No server namespace defined. Did you forget a pref?");
  }

  this._policy = policy;

  this._dbName = this._prefs.get("dbName") || DEFAULT_DATABASE_NAME;

  this._storage = null;
  this._storageInProgress = false;
  this._collector = null;
  this._collectorInProgress = false;
  this._initialized = false;
  this._initializeHadError = false;
  this._initializedDeferred = Promise.defer();
  this._shutdownRequested = false;
  this._shutdownInitiated = false;
  this._shutdownComplete = false;
  this._shutdownCompleteCallback = null;

  this._ensureDirectoryExists(this._stateDir)
      .then(this._onStateDirCreated.bind(this),
            this._onInitError.bind(this));

}

HealthReporter.prototype = Object.freeze({
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
   * When we last successfully submitted data to the server.
   *
   * This is sent as part of the upload. This is redundant with similar data
   * in the policy because we like the modules to be loosely coupled and the
   * similar data in the policy is only used for forensic purposes.
   */
  get lastPingDate() {
    return CommonUtils.getDatePref(this._prefs, "lastPingTime", 0, this._log,
                                   OLDEST_ALLOWED_YEAR);
  },

  set lastPingDate(value) {
    CommonUtils.setDatePref(this._prefs, "lastPingTime", value,
                            OLDEST_ALLOWED_YEAR);
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
   * The document ID for data to be submitted to the server.
   *
   * This should be a UUID.
   *
   * We generate a new UUID when we upload data to the server. When we get a
   * successful response for that upload, we record that UUID in this value.
   * On the subsequent upload, this ID will be deleted from the server.
   */
  get lastSubmitID() {
    return this._prefs.get("lastSubmitID", null) || null;
  },

  set lastSubmitID(value) {
    this._prefs.set("lastSubmitID", value || "");
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
    return !!this.lastSubmitID;
  },

  //----------------------------------------------------
  // SERVICE CONTROL FUNCTIONS
  //
  // You shouldn't need to call any of these externally.
  //----------------------------------------------------

  _onInitError: function (error) {
    this._log.error("Error during initialization: " +
                    CommonUtils.exceptionStr(error));
    this._initializeHadError = true;
    this._initiateShutdown();
    this._initializedDeferred.reject(error);

    // FUTURE consider poisoning prototype's functions so calls fail with a
    // useful error message.
  },

  _onStateDirCreated: function () {
    // As soon as we have could storage, we need to register cleanup or
    // else bad things happen on shutdown.
    Services.obs.addObserver(this, "quit-application", false);
    Services.obs.addObserver(this, "profile-before-change", false);

    this._storageInProgress = true;
    Metrics.Storage(this._dbName).then(this._onStorageCreated.bind(this),
                                       this._onInitError.bind(this));
  },

  // Called when storage has been opened.
  _onStorageCreated: function (storage) {
    this._log.info("Storage initialized.");
    this._storage = storage;
    this._storageInProgress = false;

    if (this._shutdownRequested) {
      this._initiateShutdown();
      return;
    }

    Task.spawn(this._initializeCollector.bind(this))
        .then(this._onCollectorInitialized.bind(this),
              this._onInitError.bind(this));
  },

  _initializeCollector: function () {
    if (this._collector) {
      throw new Error("Collector has already been initialized.");
    }

    this._log.info("Initializing collector.");
    this._collector = new Metrics.Collector(this._storage);
    this._collectorInProgress = true;

    let catString = this._prefs.get("service.providerCategories") || "";
    if (catString.length) {
      for (let category of catString.split(",")) {
        yield this.registerProvidersFromCategoryManager(category);
      }
    }
  },

  _onCollectorInitialized: function () {
    this._log.debug("Collector initialized.");
    this._collectorInProgress = false;

    if (this._shutdownRequested) {
      this._initiateShutdown();
      return;
    }

    this._log.info("HealthReporter started.");
    this._initialized = true;
    Services.obs.addObserver(this, "idle-daily", false);
    this._initializedDeferred.resolve(this);
  },

  // nsIObserver to handle shutdown.
  observe: function (subject, topic, data) {
    switch (topic) {
      case "quit-application":
        Services.obs.removeObserver(this, "quit-application");
        this._initiateShutdown();
        break;

      case "profile-before-change":
        Services.obs.removeObserver(this, "profile-before-change");
        this._waitForShutdown();
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

    if (this._collectorInProgress) {
      this._log.warn("Collector is in progress of initializing. Waiting to finish.");
      return;
    }

    // If storage is in the process of initializing, we need to wait for it
    // to finish before continuing. The initialization process will call us
    // again once storage has initialized.
    if (this._storageInProgress) {
      this._log.warn("Storage is in progress of initializing. Waiting to finish.");
      return;
    }

    this._log.warn("Initiating main shutdown procedure.");

    // Everything from here must only be performed once or else race conditions
    // could occur.
    this._shutdownInitiated = true;

    // We may not have registered the observer yet. If not, this will
    // throw.
    try {
      Services.obs.removeObserver(this, "idle-daily");
    } catch (ex) { }

    // If we have collectors, we need to shut down providers.
    if (this._collector) {
      let onShutdown = this._onCollectorShutdown.bind(this);
      Task.spawn(this._shutdownCollector.bind(this))
          .then(onShutdown, onShutdown);
      return;
    }

    this._log.warn("Don't have collector. Proceeding to storage shutdown.");
    this._shutdownStorage();
  },

  _shutdownCollector: function () {
    this._log.info("Shutting down collector.");
    for (let provider of this._collector.providers) {
      try {
        yield provider.shutdown();
      } catch (ex) {
        this._log.warn("Error when shutting down provider: " +
                       CommonUtils.exceptionStr(ex));
      }
    }
  },

  _onCollectorShutdown: function () {
    this._log.info("Collector shut down.");
    this._collector = null;
    this._shutdownStorage();
  },

  _shutdownStorage: function () {
    if (!this._storage) {
      this._onShutdownComplete();
    }

    this._log.info("Shutting down storage.");
    let onClose = this._onStorageClose.bind(this);
    this._storage.close().then(onClose, onClose);
  },

  _onStorageClose: function (error) {
    this._log.info("Storage has been closed.");

    if (error) {
      this._log.warn("Error when closing storage: " +
                     CommonUtils.exceptionStr(error));
    }

    this._storage = null;
    this._onShutdownComplete();
  },

  _onShutdownComplete: function () {
    this._log.warn("Shutdown complete.");
    this._shutdownComplete = true;

    if (this._shutdownCompleteCallback) {
      this._shutdownCompleteCallback();
    }
  },

  _waitForShutdown: function () {
    if (this._shutdownComplete) {
      return;
    }

    this._shutdownCompleteCallback = Async.makeSpinningCallback();
    this._shutdownCompleteCallback.wait();
    this._shutdownCompleteCallback = null;
  },

  /**
   * Convenience method to shut down the instance.
   *
   * This should *not* be called outside of tests.
   */
  _shutdown: function () {
    this._initiateShutdown();
    this._waitForShutdown();
  },

  /**
   * Return a promise that is resolved once the service has been initialized.
   */
  onInit: function () {
    if (this._initializeHadError) {
      throw new Error("Service failed to initialize.");
    }

    if (this._initialized) {
      return Promise.resolve(this);
    }

    return this._initializedDeferred.promise;
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
   * Register a `Metrics.Provider` with this instance.
   *
   * This needs to be called or no data will be collected. See also
   * registerProvidersFromCategoryManager`.
   *
   * @param provider
   *        (Metrics.Provider) The provider to register for collection.
   */
  registerProvider: function (provider) {
    return this._collector.registerProvider(provider);
  },

  /**
   * Registers providers from a category manager category.
   *
   * This examines the specified category entries and registers found
   * providers.
   *
   * Category entries are essentially JS modules and the name of the symbol
   * within that module that is a `Metrics.Provider` instance.
   *
   * The category entry name is the name of the JS type for the provider. The
   * value is the resource:// URI to import which makes this type available.
   *
   * Example entry:
   *
   *   FooProvider resource://gre/modules/foo.jsm
   *
   * One can register entries in the application's .manifest file. e.g.
   *
   *   category healthreport-js-provider FooProvider resource://gre/modules/foo.jsm
   *
   * Then to load them:
   *
   *   let reporter = getHealthReporter("healthreport.");
   *   reporter.registerProvidersFromCategoryManager("healthreport-js-provider");
   *
   * @param category
   *        (string) Name of category to query and load from.
   */
  registerProvidersFromCategoryManager: function (category) {
    this._log.info("Registering providers from category: " + category);
    let cm = Cc["@mozilla.org/categorymanager;1"]
               .getService(Ci.nsICategoryManager);

    let promises = [];
    let enumerator = cm.enumerateCategory(category);
    while (enumerator.hasMoreElements()) {
      let entry = enumerator.getNext()
                            .QueryInterface(Ci.nsISupportsCString)
                            .toString();

      let uri = cm.getCategoryEntry(category, entry);
      this._log.info("Attempting to load provider from category manager: " +
                     entry + " from " + uri);

      try {
        let ns = {};
        Cu.import(uri, ns);

        let provider = new ns[entry]();
        promises.push(this.registerProvider(provider));
      } catch (ex) {
        this._log.warn("Error registering provider from category manager: " +
                       entry + "; " + CommonUtils.exceptionStr(ex));
        continue;
      }
    }

    return Task.spawn(function wait() {
      for (let promise of promises) {
        yield promise;
      }
    });
  },

  /**
   * Collect all measurements for all registered providers.
   */
  collectMeasurements: function () {
    return this._collector.collectConstantData();
  },

  /**
   * Called to initiate a data upload.
   *
   * The passed argument is a `DataSubmissionRequest` from policy.jsm.
   */
  requestDataUpload: function (request) {
    this.collectMeasurements()
        .then(this._uploadData.bind(this, request),
              this._onSubmitDataRequestFailure.bind(this));
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
    if (!this.lastSubmitID) {
      return;
    }

    return this._policy.deleteRemoteData(reason);
  },

  getJSONPayload: function () {
    return Task.spawn(this._getJSONPayload.bind(this, this._now()));
  },

  _getJSONPayload: function (now) {
    let pingDateString = this._formatDate(now);
    this._log.info("Producing JSON payload for " + pingDateString);

    let o = {
      version: 1,
      thisPingDate: pingDateString,
      data: {last: {}, days: {}},
    };

    let outputDataDays = o.data.days;

    // We need to be careful that data in errors does not leak potentially
    // private information.
    // FUTURE ask Privacy if we can put exception stacks in here.
    let errors = [];

    let lastPingDate = this.lastPingDate;
    if (lastPingDate.getTime() > 0) {
      o.lastPingDate = this._formatDate(lastPingDate);
    }

    for (let provider of this._collector.providers) {
      let providerName = provider.name;

      let providerEntry = {
        measurements: {},
      };

      for (let [measurementKey, measurement] of provider.measurements) {
        let name = providerName + "." + measurement.name + "." + measurement.version;

        let serializer;
        try {
          serializer = measurement.serializer(measurement.SERIALIZE_JSON);
        } catch (ex) {
          this._log.warn("Error obtaining serializer for measurement: " + name +
                         ": " + CommonUtils.exceptionStr(ex));
          errors.push("Could not obtain serializer: " + name);
          continue;
        }

        let data;
        try {
          data = yield this._storage.getMeasurementValues(measurement.id);
        } catch (ex) {
          this._log.warn("Error obtaining data for measurement: " +
                         name + ": " + CommonUtils.exceptionStr(ex));
          errors.push("Could not obtain data: " + name);
          continue;
        }

        if (data.singular.size) {
          try {
            o.data.last[name] = serializer.singular(data.singular);
          } catch (ex) {
            this._log.warn("Error serializing data: " + CommonUtils.exceptionStr(ex));
            errors.push("Error serializing singular: " + name);
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

            outputDataDays[dateFormatted][name] = serialized;
          } catch (ex) {
            this._log.warn("Error populating data for day: " +
                           CommonUtils.exceptionStr(ex));
            errors.push("Could not serialize day: " + name +
                        " ( " + dateFormatted + ")");
            continue;
          }
        }
      }
    }

    if (errors.length) {
      o.errors = errors;
    }

    throw new Task.Result(JSON.stringify(o));
  },

  _onBagheeraResult: function (request, isDelete, result) {
    this._log.debug("Received Bagheera result.");

    let promise = Promise.resolve(null);

    if (!result.transportSuccess) {
      request.onSubmissionFailureSoft("Network transport error.");
      return promise;
    }

    if (!result.serverSuccess) {
      request.onSubmissionFailureHard("Server failure.");
      return promise;
    }

    let now = this._now();

    if (isDelete) {
      this.lastSubmitID = null;
    } else {
      this.lastSubmitID = result.id;
      this.lastPingDate = now;
    }

    request.onSubmissionSuccess(now);

    return promise;
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
    let id = CommonUtils.generateUUID();

    this._log.info("Uploading data to server: " + this.serverURI + " " +
                   this.serverNamespace + ":" + id);
    let client = new BagheeraClient(this.serverURI);

    return Task.spawn(function doUpload() {
      let payload = yield this.getJSONPayload();
      yield this._saveLastPayload(payload);
      let result = yield client.uploadJSON(this.serverNamespace, id, payload,
                                           this.lastSubmitID);
      yield this._onBagheeraResult(request, false, result);
    }.bind(this));
  },

  deleteRemoteData: function (request) {
    if (!this.lastSubmitID) {
      this._log.info("Received request to delete remote data but no data stored.");
      request.onNoDataAvailable();
      return;
    }

    this._log.warn("Deleting remote data.");
    let client = new BagheeraClient(this.serverURI);

    return client.deleteDocument(this.serverNamespace, this.lastSubmitID)
                 .then(this._onBagheeraResult.bind(this, request, true),
                       this._onSubmitDataRequestFailure.bind(this));

  },

  get _stateDir() {
    let profD = OS.Constants.Path.profileDir;

    // Work around bug 810543 until OS.File is more resilient.
    if (!profD || !profD.length) {
      throw new Error("Could not obtain profile directory. OS.File not " +
                      "initialized properly?");
    }

    return OS.Path.join(profD, "healthreport");
  },

  _ensureDirectoryExists: function (path) {
    let deferred = Promise.defer();

    OS.File.makeDir(path).then(
      function onResult() {
        deferred.resolve(true);
      },
      function onError(error) {
        if (error.becauseExists) {
          deferred.resolve(true);
          return;
        }

        deferred.reject(error);
      }
    );

    return deferred.promise;
  },

  get _lastPayloadPath() {
    return OS.Path.join(this._stateDir, "lastpayload.json");
  },

  _saveLastPayload: function (payload) {
    let path = this._lastPayloadPath;
    let pathTmp = path + ".tmp";

    let encoder = new TextEncoder();
    let buffer = encoder.encode(payload);

    return OS.File.writeAtomic(path, buffer, {tmpPath: pathTmp});
  },

  /**
   * Obtain the last uploaded payload.
   *
   * The promise is resolved to a JSON-decoded object on success. The promise
   * is rejected if the last uploaded payload could not be found or there was
   * an error reading or parsing it.
   *
   * @return Promise<object>
   */
  getLastPayload: function () {
    let path = this._lastPayloadPath;

    return OS.File.read(path).then(
      function onData(buffer) {
        let decoder = new TextDecoder();
        let json = JSON.parse(decoder.decode(buffer));

        return Promise.resolve(json);
      },
      function onError(error) {
        return Promise.reject(error);
      }
    );
  },

  _now: function _now() {
    return new Date();
  },

});

