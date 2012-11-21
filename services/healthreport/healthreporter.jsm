/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["HealthReporter"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://services-common/bagheeraclient.js");
Cu.import("resource://services-common/log4moz.js");
Cu.import("resource://services-common/observers.js");
Cu.import("resource://services-common/preferences.js");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://gre/modules/commonjs/promise/core.js");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/services/healthreport/policy.jsm");
Cu.import("resource://gre/modules/services/metrics/collector.jsm");


// Oldest year to allow in date preferences. This module was implemented in
// 2012 and no dates older than that should be encountered.
const OLDEST_ALLOWED_YEAR = 2012;


/**
 * Coordinates collection and submission of metrics.
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
 * @param branch
 *        (string) The preferences branch to use for state storage. The value
 *        must end with a period (.).
 */
this.HealthReporter = function HealthReporter(branch) {
  if (!branch.endsWith(".")) {
    throw new Error("Branch argument must end with a period (.): " + branch);
  }

  this._log = Log4Moz.repository.getLogger("Services.HealthReport.HealthReporter");

  this._prefs = new Preferences(branch);

  let policyBranch = new Preferences(branch + "policy.");
  this._policy = new HealthReportPolicy(policyBranch, this);
  this._collector = new MetricsCollector();

  if (!this.serverURI) {
    throw new Error("No server URI defined. Did you forget to define the pref?");
  }

  if (!this.serverNamespace) {
    throw new Error("No server namespace defined. Did you forget a pref?");
  }
}

HealthReporter.prototype = {
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
   * Whether remote data is currently stored.
   *
   * @return bool
   */
  haveRemoteData: function haveRemoteData() {
    return !!this.lastSubmitID;
  },

  /**
   * Perform post-construction initialization and start background activity.
   *
   * If this isn't called, no data upload will occur.
   *
   * This returns a promise that will be fulfilled when all initialization
   * activity is completed. It is not safe for this instance to perform
   * additional actions until this promise has been resolved.
   */
  start: function start() {
    let onExists = function onExists() {
      this._policy.startPolling();
      this._log.info("HealthReporter started.");

      return Promise.resolve();
    }.bind(this);

    return this._ensureDirectoryExists(this._stateDir)
               .then(onExists);
  },

  /**
   * Stop background functionality.
   */
  stop: function stop() {
    this._policy.stopPolling();
  },

  /**
   * Register a `MetricsProvider` with this instance.
   *
   * This needs to be called or no data will be collected. See also
   * registerProvidersFromCategoryManager`.
   *
   * @param provider
   *        (MetricsProvider) The provider to register for collection.
   */
  registerProvider: function registerProvider(provider) {
    return this._collector.registerProvider(provider);
  },

  /**
   * Registers providers from a category manager category.
   *
   * This examines the specified category entries and registers found
   * providers.
   *
   * Category entries are essentially JS modules and the name of the symbol
   * within that module that is a `MetricsProvider` instance.
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
   *   let reporter = new HealthReporter("healthreport.");
   *   reporter.registerProvidersFromCategoryManager("healthreport-js-provider");
   *
   * @param category
   *        (string) Name of category to query and load from.
   */
  registerProvidersFromCategoryManager:
    function registerProvidersFromCategoryManager(category) {

    let cm = Cc["@mozilla.org/categorymanager;1"]
               .getService(Ci.nsICategoryManager);

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
        this.registerProvider(provider);
      } catch (ex) {
        this._log.warn("Error registering provider from category manager: " +
                       entry + "; " + CommonUtils.exceptionStr(ex));
        continue;
      }
    }
  },

  /**
   * Collect all measurements for all registered providers.
   */
  collectMeasurements: function collectMeasurements() {
    return this._collector.collectConstantMeasurements();
  },

  /**
   * Record the user's rejection of the data submission policy.
   *
   * This should be what everything uses to disable data submission.
   *
   * @param reason
   *        (string) Why data submission is being disabled.
   */
  recordPolicyRejection: function recordPolicyRejection(reason) {
    this._policy.recordUserRejection(reason);
  },

  /**
   * Record the user's acceptance of the data submission policy.
   *
   * This should be what everything uses to enable data submission.
   *
   * @param reason
   *        (string) Why data submission is being enabled.
   */
  recordPolicyAcceptance: function recordPolicyAcceptance(reason) {
    this._policy.recordUserAcceptance(reason);
  },

  /**
   * Whether the data submission policy has been accepted.
   *
   * If this is true, health data will be submitted unless one of the kill
   * switches is active.
   */
  get dataSubmissionPolicyAccepted() {
    return this._policy.dataSubmissionPolicyAccepted;
  },

  /**
   * Whether this health reporter will upload data to a server.
   */
  get willUploadData() {
    return this._policy.dataSubmissionPolicyAccepted &&
           this._policy.dataUploadEnabled;
  },

  /**
   * Request that server data be deleted.
   *
   * If deletion is scheduled to occur immediately, a promise will be returned
   * that will be fulfilled when the deletion attempt finishes. Otherwise,
   * callers should poll haveRemoteData() to determine when remote data is
   * deleted.
   */
  requestDeleteRemoteData: function requestDeleteRemoteData(reason) {
    if (!this.lastSubmitID) {
      return;
    }

    return this._policy.deleteRemoteData(reason);
  },

  getJSONPayload: function getJSONPayload() {
    let o = {
      version: 1,
      thisPingDate: this._formatDate(this._now()),
      providers: {},
    };

    let lastPingDate = this.lastPingDate;
    if (lastPingDate.getTime() > 0) {
      o.lastPingDate = this._formatDate(lastPingDate);
    }

    for (let [name, provider] of this._collector.collectionResults) {
      o.providers[name] = provider;
    }

    return JSON.stringify(o);
  },

  _onBagheeraResult: function _onBagheeraResult(request, isDelete, result) {
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

  _onSubmitDataRequestFailure: function _onSubmitDataRequestFailure(error) {
    this._log.error("Error processing request to submit data: " +
                    CommonUtils.exceptionStr(error));
  },

  _formatDate: function _formatDate(date) {
    // Why, oh, why doesn't JS have a strftime() equivalent?
    return date.toISOString().substr(0, 10);
  },


  _uploadData: function _uploadData(request) {
    let id = CommonUtils.generateUUID();

    this._log.info("Uploading data to server: " + this.serverURI + " " +
                   this.serverNamespace + ":" + id);
    let client = new BagheeraClient(this.serverURI);

    let payload = this.getJSONPayload();

    return this._saveLastPayload(payload)
               .then(client.uploadJSON.bind(client,
                                            this.serverNamespace,
                                            id,
                                            payload,
                                            this.lastSubmitID))
               .then(this._onBagheeraResult.bind(this, request, false));
  },

  _deleteRemoteData: function _deleteRemoteData(request) {
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

  _ensureDirectoryExists: function _ensureDirectoryExists(path) {
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

  _saveLastPayload: function _saveLastPayload(payload) {
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
  getLastPayload: function getLoadPayload() {
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

  //-----------------------------
  // HealthReportPolicy listeners
  //-----------------------------

  onRequestDataUpload: function onRequestDataSubmission(request) {
    this.collectMeasurements()
        .then(this._uploadData.bind(this, request),
              this._onSubmitDataRequestFailure.bind(this));
  },

  onNotifyDataPolicy: function onNotifyDataPolicy(request) {
    // This isn't very loosely coupled. We may want to have this call
    // registered listeners instead.
    Observers.notify("healthreport:notify-data-policy:request", request);
  },

  onRequestRemoteDelete: function onRequestRemoteDelete(request) {
    this._deleteRemoteData(request);
  },

  //------------------------------------
  // End of HealthReportPolicy listeners
  //------------------------------------
};

Object.freeze(HealthReporter.prototype);

