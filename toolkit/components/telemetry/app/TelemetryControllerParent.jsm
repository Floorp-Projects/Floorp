/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { AsyncShutdown } = ChromeUtils.import(
  "resource://gre/modules/AsyncShutdown.jsm"
);
const { PromiseUtils } = ChromeUtils.import(
  "resource://gre/modules/PromiseUtils.jsm"
);
const { DeferredTask } = ChromeUtils.import(
  "resource://gre/modules/DeferredTask.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { TelemetryUtils } = ChromeUtils.import(
  "resource://gre/modules/TelemetryUtils.jsm"
);
const { TelemetryControllerBase } = ChromeUtils.import(
  "resource://gre/modules/TelemetryControllerBase.jsm"
);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const Utils = TelemetryUtils;

const PING_FORMAT_VERSION = 4;

// Delay before intializing telemetry (ms)
const TELEMETRY_DELAY =
  Services.prefs.getIntPref("toolkit.telemetry.initDelay", 60) * 1000;
// Delay before initializing telemetry if we're testing (ms)
const TELEMETRY_TEST_DELAY = 1;

// How long to wait (ms) before sending the new profile ping on the first
// run of a new profile.
const NEWPROFILE_PING_DEFAULT_DELAY = 30 * 60 * 1000;

// Ping types.
const PING_TYPE_MAIN = "main";
const PING_TYPE_DELETION_REQUEST = "deletion-request";
const PING_TYPE_UNINSTALL = "uninstall";

// Session ping reasons.
const REASON_GATHER_PAYLOAD = "gather-payload";
const REASON_GATHER_SUBSESSION_PAYLOAD = "gather-subsession-payload";

const lazy = {};

ChromeUtils.defineModuleGetter(
  lazy,
  "jwcrypto",
  "resource://services-crypto/jwcrypto.jsm"
);

XPCOMUtils.defineLazyModuleGetters(lazy, {
  ClientID: "resource://gre/modules/ClientID.jsm",
  CoveragePing: "resource://gre/modules/CoveragePing.jsm",
  TelemetryStorage: "resource://gre/modules/TelemetryStorage.jsm",
  TelemetryEnvironment: "resource://gre/modules/TelemetryEnvironment.jsm",
  TelemetryArchive: "resource://gre/modules/TelemetryArchive.jsm",
  TelemetrySession: "resource://gre/modules/TelemetrySession.jsm",
  TelemetrySend: "resource://gre/modules/TelemetrySend.jsm",
  TelemetryReportingPolicy:
    "resource://gre/modules/TelemetryReportingPolicy.jsm",
  TelemetryModules: "resource://gre/modules/ModulesPing.jsm",
  TelemetryUntrustedModulesPing:
    "resource://gre/modules/UntrustedModulesPing.jsm",
  UpdatePing: "resource://gre/modules/UpdatePing.jsm",
  TelemetryHealthPing: "resource://gre/modules/HealthPing.jsm",
  TelemetryEventPing: "resource://gre/modules/EventPing.jsm",
  TelemetryPrioPing: "resource://gre/modules/PrioPing.jsm",
  UninstallPing: "resource://gre/modules/UninstallPing.jsm",
});

/**
 * This is a policy object used to override behavior for testing.
 */
var Policy = {
  now: () => new Date(),
  generatePingId: () => Utils.generateUUID(),
  getCachedClientID: () => lazy.ClientID.getCachedClientID(),
};

var EXPORTED_SYMBOLS = ["TelemetryController", "Policy"];

var TelemetryController = Object.freeze({
  /**
   * Used only for testing purposes.
   */
  testInitLogging() {
    TelemetryControllerBase.configureLogging();
  },

  /**
   * Used only for testing purposes.
   */
  testReset() {
    return Impl.reset();
  },

  /**
   * Used only for testing purposes.
   */
  testSetup() {
    return Impl.setupTelemetry(true);
  },

  /**
   * Used only for testing purposes.
   */
  testShutdown() {
    return Impl.shutdown();
  },

  /**
   * Used only for testing purposes.
   */
  testPromiseJsProbeRegistration() {
    return Promise.resolve(Impl._probeRegistrationPromise);
  },

  /**
   * Register 'dynamic builtin' probes from the JSON definition files.
   * This is needed to support adding new probes in developer builds
   * without rebuilding the whole codebase.
   *
   * This is not meant to be used outside of local developer builds.
   */
  testRegisterJsProbes() {
    return Impl.registerJsProbes();
  },

  /**
   * Used only for testing purposes.
   */
  testPromiseDeletionRequestPingSubmitted() {
    return Promise.resolve(Impl._deletionRequestPingSubmittedPromise);
  },

  /**
   * Send a notification.
   */
  observe(aSubject, aTopic, aData) {
    return Impl.observe(aSubject, aTopic, aData);
  },

  /**
   * Submit ping payloads to Telemetry. This will assemble a complete ping, adding
   * environment data, client id and some general info.
   * Depending on configuration, the ping will be sent to the server (immediately or later)
   * and archived locally.
   *
   * To identify the different pings and to be able to query them pings have a type.
   * A type is a string identifier that should be unique to the type ping that is being submitted,
   * it should only contain alphanumeric characters and '-' for separation, i.e. satisfy:
   * /^[a-z0-9][a-z0-9-]+[a-z0-9]$/i
   *
   * @param {String} aType The type of the ping.
   * @param {Object} aPayload The actual data payload for the ping.
   * @param {Object} [aOptions] Options object.
   * @param {Boolean} [aOptions.addClientId=false] true if the ping should contain the client
   *                  id, false otherwise.
   * @param {Boolean} [aOptions.addEnvironment=false] true if the ping should contain the
   *                  environment data.
   * @param {Object}  [aOptions.overrideEnvironment=null] set to override the environment data.
   * @param {Boolean} [aOptions.usePingSender=false] if true, send the ping using the PingSender.
   * @param {String} [aOptions.overrideClientId=undefined] if set, override the
   *                 client id to the provided value. Implies aOptions.addClientId=true.
   * @returns {Promise} Test-only - a promise that resolves with the ping id once the ping is stored or sent.
   */
  submitExternalPing(aType, aPayload, aOptions = {}) {
    aOptions.addClientId = aOptions.addClientId || false;
    aOptions.addEnvironment = aOptions.addEnvironment || false;
    aOptions.usePingSender = aOptions.usePingSender || false;

    return Impl.submitExternalPing(aType, aPayload, aOptions);
  },

  /**
   * Get the current session ping data as it would be sent out or stored.
   *
   * @param {bool} aSubsession Whether to get subsession data. Optional, defaults to false.
   * @return {object} The current ping data if Telemetry is enabled, null otherwise.
   */
  getCurrentPingData(aSubsession = false) {
    return Impl.getCurrentPingData(aSubsession);
  },

  /**
   * Save a ping to disk.
   *
   * @param {String} aType The type of the ping.
   * @param {Object} aPayload The actual data payload for the ping.
   * @param {Object} [aOptions] Options object.
   * @param {Boolean} [aOptions.addClientId=false] true if the ping should contain the client
   *                  id, false otherwise.
   * @param {Boolean} [aOptions.addEnvironment=false] true if the ping should contain the
   *                  environment data.
   * @param {Boolean} [aOptions.overwrite=false] true overwrites a ping with the same name,
   *                  if found.
   * @param {Object}  [aOptions.overrideEnvironment=null] set to override the environment data.
   * @param {String} [aOptions.overrideClientId=undefined] if set, override the
   *                 client id to the provided value. Implies aOptions.addClientId=true.
   *
   * @returns {Promise} A promise that resolves with the ping id when the ping is saved to
   *                    disk.
   */
  addPendingPing(aType, aPayload, aOptions = {}) {
    let options = aOptions;
    options.addClientId = aOptions.addClientId || false;
    options.addEnvironment = aOptions.addEnvironment || false;
    options.overwrite = aOptions.overwrite || false;

    return Impl.addPendingPing(aType, aPayload, options);
  },

  /**
   * Check if we have an aborted-session ping from a previous session.
   * If so, submit and then remove it.
   *
   * @return {Promise} Promise that is resolved when the ping is saved.
   */
  checkAbortedSessionPing() {
    return Impl.checkAbortedSessionPing();
  },

  /**
   * Save an aborted-session ping to disk without adding it to the pending pings.
   *
   * @param {Object} aPayload The ping payload data.
   * @return {Promise} Promise that is resolved when the ping is saved.
   */
  saveAbortedSessionPing(aPayload) {
    return Impl.saveAbortedSessionPing(aPayload);
  },

  /**
   * Remove the aborted-session ping if any exists.
   *
   * @return {Promise} Promise that is resolved when the ping was removed.
   */
  removeAbortedSessionPing() {
    return Impl.removeAbortedSessionPing();
  },

  /**
   * Create an uninstall ping and write it to disk, replacing any already present.
   * This is stored independently from other pings, and only read by
   * the Windows uninstaller.
   *
   * WINDOWS ONLY, does nothing and resolves immediately on other platforms.
   *
   * @return {Promise} Resolved when the ping has been saved.
   */
  saveUninstallPing() {
    return Impl.saveUninstallPing();
  },

  /**
   * Allows the sync ping to tell the controller that it is initializing, so
   * should be included in the orderly shutdown process.
   *
   * @param {Function} aFnShutdown The function to call as telemetry shuts down.

   */
  registerSyncPingShutdown(afnShutdown) {
    Impl.registerSyncPingShutdown(afnShutdown);
  },

  /**
   * Allows waiting for TelemetryControllers delayed initialization to complete.
   * The returned promise is guaranteed to resolve before TelemetryController is shutting down.
   * @return {Promise} Resolved when delayed TelemetryController initialization completed.
   */
  promiseInitialized() {
    return Impl.promiseInitialized();
  },
});

var Impl = {
  _initialized: false,
  _initStarted: false, // Whether we started setting up TelemetryController.
  _shuttingDown: false, // Whether the browser is shutting down.
  _shutDown: false, // Whether the browser has shut down.
  _logger: null,
  _prevValues: {},
  // The previous build ID, if this is the first run with a new build.
  // Undefined if this is not the first run, or the previous build ID is unknown.
  _previousBuildID: undefined,
  _clientID: null,
  // A task performing delayed initialization
  _delayedInitTask: null,
  // The deferred promise resolved when the initialization task completes.
  _delayedInitTaskDeferred: null,

  // This is a public barrier Telemetry clients can use to add blockers to the shutdown
  // of TelemetryController.
  // After this barrier, clients can not submit Telemetry pings anymore.
  _shutdownBarrier: new AsyncShutdown.Barrier(
    "TelemetryController: Waiting for clients."
  ),
  // This state is included in the async shutdown annotation for crash pings and reports.
  _shutdownState: "Shutdown not started.",
  // This is a private barrier blocked by pending async ping activity (sending & saving).
  _connectionsBarrier: new AsyncShutdown.Barrier(
    "TelemetryController: Waiting for pending ping activity"
  ),
  // This is true when running in the test infrastructure.
  _testMode: false,
  // The task performing the delayed sending of the "new-profile" ping.
  _delayedNewPingTask: null,
  // The promise used to wait for the JS probe registration (dynamic builtin).
  _probeRegistrationPromise: null,
  // The promise of any outstanding task sending the "deletion-request" ping.
  _deletionRequestPingSubmittedPromise: null,
  // A function to shutdown the sync/fxa ping, or null if that ping has not
  // self-initialized.
  _fnSyncPingShutdown: null,

  get _log() {
    return TelemetryControllerBase.log;
  },

  /**
   * Get the data for the "application" section of the ping.
   */
  _getApplicationSection() {
    // Querying architecture and update channel can throw. Make sure to recover and null
    // those fields.
    let arch = null;
    try {
      arch = Services.sysinfo.get("arch");
    } catch (e) {
      this._log.trace(
        "_getApplicationSection - Unable to get system architecture.",
        e
      );
    }

    let updateChannel = null;
    try {
      updateChannel = Utils.getUpdateChannel();
    } catch (e) {
      this._log.trace(
        "_getApplicationSection - Unable to get update channel.",
        e
      );
    }

    return {
      architecture: arch,
      buildId: Services.appinfo.appBuildID,
      name: Services.appinfo.name,
      version: Services.appinfo.version,
      displayVersion: AppConstants.MOZ_APP_VERSION_DISPLAY,
      vendor: Services.appinfo.vendor,
      platformVersion: Services.appinfo.platformVersion,
      xpcomAbi: Services.appinfo.XPCOMABI,
      channel: updateChannel,
    };
  },

  /**
   * Assemble a complete ping following the common ping format specification.
   *
   * @param {String} aType The type of the ping.
   * @param {Object} aPayload The actual data payload for the ping.
   * @param {Object} aOptions Options object.
   * @param {Boolean} aOptions.addClientId true if the ping should contain the client
   *                  id, false otherwise.
   * @param {Boolean} aOptions.addEnvironment true if the ping should contain the
   *                  environment data.
   * @param {Object}  [aOptions.overrideEnvironment=null] set to override the environment data.
   * @param {String} [aOptions.overrideClientId=undefined] if set, override the
   *                 client id to the provided value. Implies aOptions.addClientId=true.
   * @param {Boolean} [aOptions.useEncryption=false] if true, encrypt data client-side before sending.
   * @param {Object}  [aOptions.publicKey=null] the public key to use if encryption is enabled (JSON Web Key).
   * @param {String}  [aOptions.encryptionKeyId=null] the public key ID to use if encryption is enabled.
   * @param {String}  [aOptions.studyName=null] the study name to use.
   * @param {String}  [aOptions.schemaName=null] the schema name to use if encryption is enabled.
   * @param {String}  [aOptions.schemaNamespace=null] the schema namespace to use if encryption is enabled.
   * @param {String}  [aOptions.schemaVersion=null] the schema version to use if encryption is enabled.
   * @param {Boolean} [aOptions.addPioneerId=false] true if the ping should contain the Pioneer id, false otherwise.
   * @param {Boolean} [aOptions.overridePioneerId=undefined] if set, override the
   *                  pioneer id to the provided value. Only works if aOptions.addPioneerId=true.
   * @returns {Object} An object that contains the assembled ping data.
   */
  assemblePing: function assemblePing(aType, aPayload, aOptions = {}) {
    this._log.trace(
      "assemblePing - Type " + aType + ", aOptions " + JSON.stringify(aOptions)
    );

    // Clone the payload data so we don't race against unexpected changes in subobjects that are
    // still referenced by other code.
    // We can't trust all callers to do this properly on their own.
    let payload = Cu.cloneInto(aPayload, {});

    // Fill the common ping fields.
    let pingData = {
      type: aType,
      id: Policy.generatePingId(),
      creationDate: Policy.now().toISOString(),
      version: PING_FORMAT_VERSION,
      application: this._getApplicationSection(),
      payload,
    };

    if (aOptions.addClientId || aOptions.overrideClientId) {
      pingData.clientId = aOptions.overrideClientId || this._clientID;
    }

    if (aOptions.addEnvironment) {
      pingData.environment =
        aOptions.overrideEnvironment ||
        lazy.TelemetryEnvironment.currentEnvironment;
    }

    return pingData;
  },

  /**
   * Track any pending ping send and save tasks through the promise passed here.
   * This is needed to block shutdown on any outstanding ping activity.
   */
  _trackPendingPingTask(aPromise) {
    this._connectionsBarrier.client.addBlocker(
      "Waiting for ping task",
      aPromise
    );
  },

  /**
   * Internal function to assemble a complete ping, adding environment data, client id
   * and some general info. This waits on the client id to be loaded/generated if it's
   * not yet available. Note that this function is synchronous unless we need to load
   * the client id.
   * Depending on configuration, the ping will be sent to the server (immediately or later)
   * and archived locally.
   *
   * @param {String} aType The type of the ping.
   * @param {Object} aPayload The actual data payload for the ping.
   * @param {Object} [aOptions] Options object.
   * @param {Boolean} [aOptions.addClientId=false] true if the ping should contain the client
   *                  id, false otherwise.
   * @param {Boolean} [aOptions.addEnvironment=false] true if the ping should contain the
   *                  environment data.
   * @param {Object}  [aOptions.overrideEnvironment=null] set to override the environment data.
   * @param {Boolean} [aOptions.usePingSender=false] if true, send the ping using the PingSender.
   * @param {Boolean} [aOptions.useEncryption=false] if true, encrypt data client-side before sending.
   * @param {Object}  [aOptions.publicKey=null] the public key to use if encryption is enabled (JSON Web Key).
   * @param {String}  [aOptions.encryptionKeyId=null] the public key ID to use if encryption is enabled.
   * @param {String}  [aOptions.studyName=null] the study name to use.
   * @param {String}  [aOptions.schemaName=null] the schema name to use if encryption is enabled.
   * @param {String}  [aOptions.schemaNamespace=null] the schema namespace to use if encryption is enabled.
   * @param {String}  [aOptions.schemaVersion=null] the schema version to use if encryption is enabled.
   * @param {Boolean} [aOptions.addPioneerId=false] true if the ping should contain the Pioneer id, false otherwise.
   * @param {Boolean} [aOptions.overridePioneerId=undefined] if set, override the
   *                  pioneer id to the provided value. Only works if aOptions.addPioneerId=true.
   * @param {String} [aOptions.overrideClientId=undefined] if set, override the
   *                 client id to the provided value. Implies aOptions.addClientId=true.
   * @returns {Promise} Test-only - a promise that is resolved with the ping id once the ping is stored or sent.
   */
  async _submitPingLogic(aType, aPayload, aOptions) {
    // Make sure to have a clientId if we need one. This cover the case of submitting
    // a ping early during startup, before Telemetry is initialized, if no client id was
    // cached.
    if (!this._clientID && aOptions.addClientId && !aOptions.overrideClientId) {
      this._log.trace("_submitPingLogic - Waiting on client id");
      Services.telemetry
        .getHistogramById("TELEMETRY_PING_SUBMISSION_WAITING_CLIENTID")
        .add();
      // We can safely call |getClientID| here and during initialization: we would still
      // spawn and return one single loading task.
      this._clientID = await lazy.ClientID.getClientID();
    }

    let pingData = this.assemblePing(aType, aPayload, aOptions);
    this._log.trace("submitExternalPing - ping assembled, id: " + pingData.id);

    if (aOptions.useEncryption === true) {
      try {
        if (!aOptions.publicKey) {
          throw new Error("Public key is required when using encryption.");
        }

        if (
          !(
            aOptions.schemaName &&
            aOptions.schemaNamespace &&
            aOptions.schemaVersion
          )
        ) {
          throw new Error(
            "Schema name, namespace, and version are required when using encryption."
          );
        }

        const payload = {};
        payload.encryptedData = await lazy.jwcrypto.generateJWE(
          aOptions.publicKey,
          new TextEncoder("utf-8").encode(JSON.stringify(aPayload))
        );

        payload.schemaVersion = aOptions.schemaVersion;
        payload.schemaName = aOptions.schemaName;
        payload.schemaNamespace = aOptions.schemaNamespace;

        payload.encryptionKeyId = aOptions.encryptionKeyId;

        if (aOptions.addPioneerId === true) {
          if (aOptions.overridePioneerId) {
            // The caller provided a substitute id, let's use that
            // instead of querying the pref.
            payload.pioneerId = aOptions.overridePioneerId;
          } else {
            // This will throw if there is no pioneer ID set.
            payload.pioneerId = Services.prefs.getStringPref(
              "toolkit.telemetry.pioneerId"
            );
          }
          payload.studyName = aOptions.studyName;
        }

        pingData.payload = payload;
      } catch (e) {
        this._log.error("_submitPingLogic - Unable to encrypt ping", e);
        // Do not attempt to continue
        throw e;
      }
    }

    // Always persist the pings if we are allowed to. We should not yield on any of the
    // following operations to keep this function synchronous for the majority of the calls.
    let archivePromise = lazy.TelemetryArchive.promiseArchivePing(
      pingData
    ).catch(e =>
      this._log.error(
        "submitExternalPing - Failed to archive ping " + pingData.id,
        e
      )
    );
    let p = [archivePromise];

    p.push(
      lazy.TelemetrySend.submitPing(pingData, {
        usePingSender: aOptions.usePingSender,
      })
    );

    return Promise.all(p).then(() => pingData.id);
  },

  /**
   * Submit ping payloads to Telemetry.
   *
   * @param {String} aType The type of the ping.
   * @param {Object} aPayload The actual data payload for the ping.
   * @param {Object} [aOptions] Options object.
   * @param {Boolean} [aOptions.addClientId=false] true if the ping should contain the client
   *                  id, false otherwise.
   * @param {Boolean} [aOptions.addEnvironment=false] true if the ping should contain the
   *                  environment data.
   * @param {Object}  [aOptions.overrideEnvironment=null] set to override the environment data.
   * @param {Boolean} [aOptions.usePingSender=false] if true, send the ping using the PingSender.
   * @param {Boolean} [aOptions.useEncryption=false] if true, encrypt data client-side before sending.
   * @param {Object}  [aOptions.publicKey=null] the public key to use if encryption is enabled (JSON Web Key).
   * @param {String}  [aOptions.encryptionKeyId=null] the public key ID to use if encryption is enabled.
   * @param {String}  [aOptions.studyName=null] the study name to use.
   * @param {String}  [aOptions.schemaName=null] the schema name to use if encryption is enabled.
   * @param {String}  [aOptions.schemaNamespace=null] the schema namespace to use if encryption is enabled.
   * @param {String}  [aOptions.schemaVersion=null] the schema version to use if encryption is enabled.
   * @param {Boolean} [aOptions.addPioneerId=false] true if the ping should contain the Pioneer id, false otherwise.
   * @param {Boolean} [aOptions.overridePioneerId=undefined] if set, override the
   *                  pioneer id to the provided value. Only works if aOptions.addPioneerId=true.
   * @param {String} [aOptions.overrideClientId=undefined] if set, override the
   *                 client id to the provided value. Implies aOptions.addClientId=true.
   * @returns {Promise} Test-only - a promise that is resolved with the ping id once the ping is stored or sent.
   */
  submitExternalPing: function send(aType, aPayload, aOptions) {
    this._log.trace(
      "submitExternalPing - type: " +
        aType +
        ", aOptions: " +
        JSON.stringify(aOptions)
    );

    // Reject pings sent after shutdown.
    if (this._shutDown) {
      const errorMessage =
        "submitExternalPing - Submission is not allowed after shutdown, discarding ping of type: " +
        aType;
      this._log.error(errorMessage);
      return Promise.reject(new Error(errorMessage));
    }

    // Enforce the type string to only contain sane characters.
    const typeUuid = /^[a-z0-9][a-z0-9-]+[a-z0-9]$/i;
    if (!typeUuid.test(aType)) {
      this._log.error("submitExternalPing - invalid ping type: " + aType);
      let histogram = Services.telemetry.getKeyedHistogramById(
        "TELEMETRY_INVALID_PING_TYPE_SUBMITTED"
      );
      histogram.add(aType, 1);
      return Promise.reject(new Error("Invalid type string submitted."));
    }
    // Enforce that the payload is an object.
    if (
      aPayload === null ||
      typeof aPayload !== "object" ||
      Array.isArray(aPayload)
    ) {
      this._log.error(
        "submitExternalPing - invalid payload type: " + typeof aPayload
      );
      let histogram = Services.telemetry.getHistogramById(
        "TELEMETRY_INVALID_PAYLOAD_SUBMITTED"
      );
      histogram.add(1);
      return Promise.reject(new Error("Invalid payload type submitted."));
    }

    let promise = this._submitPingLogic(aType, aPayload, aOptions);
    this._trackPendingPingTask(promise);
    return promise;
  },

  /**
   * Save a ping to disk.
   *
   * @param {String} aType The type of the ping.
   * @param {Object} aPayload The actual data payload for the ping.
   * @param {Object} aOptions Options object.
   * @param {Boolean} aOptions.addClientId true if the ping should contain the client id,
   *                  false otherwise.
   * @param {Boolean} aOptions.addEnvironment true if the ping should contain the
   *                  environment data.
   * @param {Boolean} aOptions.overwrite true overwrites a ping with the same name, if found.
   * @param {Object}  [aOptions.overrideEnvironment=null] set to override the environment data.
   * @param {String} [aOptions.overrideClientId=undefined] if set, override the
   *                 client id to the provided value. Implies aOptions.addClientId=true.
   *
   * @returns {Promise} A promise that resolves with the ping id when the ping is saved to
   *                    disk.
   */
  addPendingPing: function addPendingPing(aType, aPayload, aOptions) {
    this._log.trace(
      "addPendingPing - Type " +
        aType +
        ", aOptions " +
        JSON.stringify(aOptions)
    );

    let pingData = this.assemblePing(aType, aPayload, aOptions);

    let savePromise = lazy.TelemetryStorage.savePendingPing(pingData);
    let archivePromise = lazy.TelemetryArchive.promiseArchivePing(
      pingData
    ).catch(e => {
      this._log.error(
        "addPendingPing - Failed to archive ping " + pingData.id,
        e
      );
    });

    // Wait for both the archiving and ping persistence to complete.
    let promises = [savePromise, archivePromise];
    return Promise.all(promises).then(() => pingData.id);
  },

  /**
   * Check whether we have an aborted-session ping. If so add it to the pending pings and archive it.
   *
   * @return {Promise} Promise that is resolved when the ping is submitted and archived.
   */
  async checkAbortedSessionPing() {
    let ping = await lazy.TelemetryStorage.loadAbortedSessionPing();
    this._log.trace(
      "checkAbortedSessionPing - found aborted-session ping: " + !!ping
    );
    if (!ping) {
      return;
    }

    try {
      // Previous aborted-session might have been with a canary client ID.
      // Don't send it.
      if (ping.clientId != Utils.knownClientID) {
        await lazy.TelemetryStorage.addPendingPing(ping);
        await lazy.TelemetryArchive.promiseArchivePing(ping);
      }
    } catch (e) {
      this._log.error(
        "checkAbortedSessionPing - Unable to add the pending ping",
        e
      );
    } finally {
      await lazy.TelemetryStorage.removeAbortedSessionPing();
    }
  },

  /**
   * Save an aborted-session ping to disk without adding it to the pending pings.
   *
   * @param {Object} aPayload The ping payload data.
   * @return {Promise} Promise that is resolved when the ping is saved.
   */
  saveAbortedSessionPing(aPayload) {
    this._log.trace("saveAbortedSessionPing");
    const options = { addClientId: true, addEnvironment: true };
    const pingData = this.assemblePing(PING_TYPE_MAIN, aPayload, options);
    return lazy.TelemetryStorage.saveAbortedSessionPing(pingData);
  },

  removeAbortedSessionPing() {
    return lazy.TelemetryStorage.removeAbortedSessionPing();
  },

  async saveUninstallPing() {
    if (AppConstants.platform != "win") {
      return undefined;
    }

    this._log.trace("saveUninstallPing");

    let payload = {};
    try {
      payload.otherInstalls = lazy.UninstallPing.getOtherInstallsCount();
      this._log.info(
        "saveUninstallPing - otherInstalls",
        payload.otherInstalls
      );
    } catch (e) {
      this._log.warn("saveUninstallPing - getOtherInstallCount failed", e);
    }
    const options = { addClientId: true, addEnvironment: true };
    const pingData = this.assemblePing(PING_TYPE_UNINSTALL, payload, options);

    return lazy.TelemetryStorage.saveUninstallPing(pingData);
  },

  /**
   * This triggers basic telemetry initialization and schedules a full initialized for later
   * for performance reasons.
   *
   * This delayed initialization means TelemetryController init can be in the following states:
   * 1) setupTelemetry was never called
   * or it was called and
   *   2) _delayedInitTask was scheduled, but didn't run yet.
   *   3) _delayedInitTask is currently running.
   *   4) _delayedInitTask finished running and is nulled out.
   *
   * @return {Promise} Resolved when TelemetryController and TelemetrySession are fully
   *                   initialized. This is only used in tests.
   */
  setupTelemetry: function setupTelemetry(testing) {
    this._initStarted = true;
    this._shuttingDown = false;
    this._shutDown = false;
    this._testMode = testing;

    this._log.trace("setupTelemetry");

    if (this._delayedInitTask) {
      this._log.error("setupTelemetry - init task already running");
      return this._delayedInitTaskDeferred.promise;
    }

    if (this._initialized && !this._testMode) {
      this._log.error("setupTelemetry - already initialized");
      return Promise.resolve();
    }

    // Enable adding scalars in artifact builds and build faster modes.
    // The function is async: we intentionally don't wait for it to complete
    // as we don't want to delay startup.
    this._probeRegistrationPromise = this.registerJsProbes();

    // This will trigger displaying the datachoices infobar.
    lazy.TelemetryReportingPolicy.setup();

    if (!TelemetryControllerBase.enableTelemetryRecording()) {
      this._log.config(
        "setupChromeProcess - Telemetry recording is disabled, skipping Chrome process setup."
      );
      return Promise.resolve();
    }

    this._attachObservers();

    // Perform a lightweight, early initialization for the component, just registering
    // a few observers and initializing the session.
    lazy.TelemetrySession.earlyInit(this._testMode);
    Services.telemetry.earlyInit();

    // Annotate crash reports so that we get pings for startup crashes
    lazy.TelemetrySend.earlyInit();

    // For very short session durations, we may never load the client
    // id from disk.
    // We try to cache it in prefs to avoid this, even though this may
    // lead to some stale client ids.
    this._clientID = lazy.ClientID.getCachedClientID();

    // Init the update ping telemetry as early as possible. This won't have
    // an impact on startup.
    lazy.UpdatePing.earlyInit();

    // Delay full telemetry initialization to give the browser time to
    // run various late initializers. Otherwise our gathered memory
    // footprint and other numbers would be too optimistic.
    this._delayedInitTaskDeferred = PromiseUtils.defer();
    this._delayedInitTask = new DeferredTask(
      async () => {
        try {
          // TODO: This should probably happen after all the delayed init here.
          this._initialized = true;
          await lazy.TelemetryEnvironment.delayedInit();

          // Load the ClientID.
          this._clientID = await lazy.ClientID.getClientID();

          // Fix-up a canary client ID if detected.
          const uploadEnabled = Services.prefs.getBoolPref(
            TelemetryUtils.Preferences.FhrUploadEnabled,
            false
          );
          if (uploadEnabled && this._clientID == Utils.knownClientID) {
            this._log.trace(
              "Upload enabled, but got canary client ID. Resetting."
            );
            await lazy.ClientID.removeClientID();
            this._clientID = await lazy.ClientID.getClientID();
          } else if (!uploadEnabled && this._clientID != Utils.knownClientID) {
            this._log.trace(
              "Upload disabled, but got a valid client ID. Setting canary client ID."
            );
            await lazy.ClientID.setCanaryClientID();
            this._clientID = await lazy.ClientID.getClientID();
          }

          await lazy.TelemetrySend.setup(this._testMode);

          // Perform TelemetrySession delayed init.
          await lazy.TelemetrySession.delayedInit();
          await Services.telemetry.delayedInit();

          if (
            Services.prefs.getBoolPref(
              TelemetryUtils.Preferences.NewProfilePingEnabled,
              false
            ) &&
            !lazy.TelemetrySession.newProfilePingSent
          ) {
            // Kick off the scheduling of the new-profile ping.
            this.scheduleNewProfilePing();
          }

          // Purge the pings archive by removing outdated pings. We don't wait for
          // this task to complete, but TelemetryStorage blocks on it during
          // shutdown.
          lazy.TelemetryStorage.runCleanPingArchiveTask();

          // Now that FHR/healthreporter is gone, make sure to remove FHR's DB from
          // the profile directory. This is a temporary measure that we should drop
          // in the future.
          lazy.TelemetryStorage.removeFHRDatabase();

          // The init sequence is forced to run on shutdown for short sessions and
          // we don't want to start TelemetryModules as the timer registration will fail.
          if (!this._shuttingDown) {
            // Report the modules loaded in the Firefox process.
            lazy.TelemetryModules.start();

            // Send coverage ping.
            await lazy.CoveragePing.startup();

            // Start the untrusted modules ping, which reports events where
            // untrusted modules were loaded into the Firefox process.
            if (AppConstants.platform == "win") {
              lazy.TelemetryUntrustedModulesPing.start();
            }
          }

          lazy.TelemetryEventPing.startup();
          lazy.TelemetryPrioPing.startup();

          if (uploadEnabled) {
            await this.saveUninstallPing().catch(e =>
              this._log.warn("_delayedInitTask - saveUninstallPing failed", e)
            );
          } else {
            await lazy.TelemetryStorage.removeUninstallPings().catch(e =>
              this._log.warn("_delayedInitTask - saveUninstallPing", e)
            );
          }

          this._delayedInitTaskDeferred.resolve();
        } catch (e) {
          this._delayedInitTaskDeferred.reject(e);
        } finally {
          this._delayedInitTask = null;
        }
      },
      this._testMode ? TELEMETRY_TEST_DELAY : TELEMETRY_DELAY,
      this._testMode ? 0 : undefined
    );

    AsyncShutdown.sendTelemetry.addBlocker(
      "TelemetryController: shutting down",
      () => this.shutdown(),
      () => this._getState()
    );

    this._delayedInitTask.arm();
    return this._delayedInitTaskDeferred.promise;
  },

  // Do proper shutdown waiting and cleanup.
  async _cleanupOnShutdown() {
    if (!this._initialized) {
      return;
    }

    let start = TelemetryUtils.monotonicNow();
    let now = () => " " + (TelemetryUtils.monotonicNow() - start);
    this._shutdownStep = "_cleanupOnShutdown begin " + now();

    this._detachObservers();

    // Now do an orderly shutdown.
    try {
      if (this._delayedNewPingTask) {
        this._shutdownStep = "awaiting delayed new ping task" + now();
        await this._delayedNewPingTask.finalize();
      }

      this._shutdownStep = "Update" + now();
      lazy.UpdatePing.shutdown();

      this._shutdownStep = "Event" + now();
      lazy.TelemetryEventPing.shutdown();
      this._shutdownStep = "Prio" + now();
      await lazy.TelemetryPrioPing.shutdown();

      // Shutdown the sync ping if it is initialized - this is likely, but not
      // guaranteed, to submit a "shutdown" sync ping.
      if (this._fnSyncPingShutdown) {
        this._shutdownStep = "Sync" + now();
        this._fnSyncPingShutdown();
      }

      // Stop the datachoices infobar display.
      this._shutdownStep = "Policy" + now();
      lazy.TelemetryReportingPolicy.shutdown();
      this._shutdownStep = "Environment" + now();
      lazy.TelemetryEnvironment.shutdown();

      // Stop any ping sending.
      this._shutdownStep = "TelemetrySend" + now();
      await lazy.TelemetrySend.shutdown();

      // Send latest data.
      this._shutdownStep = "Health ping" + now();
      await lazy.TelemetryHealthPing.shutdown();

      this._shutdownStep = "TelemetrySession" + now();
      await lazy.TelemetrySession.shutdown();
      this._shutdownStep = "Services.telemetry" + now();
      await Services.telemetry.shutdown();

      // First wait for clients processing shutdown.
      this._shutdownStep = "await shutdown barrier" + now();
      await this._shutdownBarrier.wait();

      // ... and wait for any outstanding async ping activity.
      this._shutdownStep = "await connections barrier" + now();
      await this._connectionsBarrier.wait();

      if (AppConstants.platform !== "android") {
        // No PingSender on Android.
        this._shutdownStep = "Flush pingsender batch" + now();
        lazy.TelemetrySend.flushPingSenderBatch();
      }

      // Perform final shutdown operations.
      this._shutdownStep = "await TelemetryStorage" + now();
      await lazy.TelemetryStorage.shutdown();
    } finally {
      // Reset state.
      this._initialized = false;
      this._initStarted = false;
      this._shutDown = true;
    }
  },

  shutdown() {
    this._log.trace("shutdown");

    this._shuttingDown = true;

    // We can be in one the following states here:
    // 1) setupTelemetry was never called
    // or it was called and
    //   2) _delayedInitTask was scheduled, but didn't run yet.
    //   3) _delayedInitTask is running now.
    //   4) _delayedInitTask finished running already.

    // This handles 1).
    if (!this._initStarted) {
      this._shutDown = true;
      return Promise.resolve();
    }

    // This handles 4).
    if (!this._delayedInitTask) {
      // We already ran the delayed initialization.
      return this._cleanupOnShutdown();
    }

    // This handles 2) and 3).
    return this._delayedInitTask
      .finalize()
      .then(() => this._cleanupOnShutdown());
  },

  /**
   * This observer drives telemetry.
   */
  observe(aSubject, aTopic, aData) {
    // The logger might still be not available at this point.
    if (aTopic == "profile-after-change") {
      // If we don't have a logger, we need to make sure |Log.repository.getLogger()| is
      // called before |getLoggerWithMessagePrefix|. Otherwise logging won't work.
      TelemetryControllerBase.configureLogging();
    }

    this._log.trace(`observe - ${aTopic} notified.`);

    switch (aTopic) {
      case "profile-after-change":
        // profile-after-change is only registered for chrome processes.
        return this.setupTelemetry();
      case "nsPref:changed":
        if (aData == TelemetryUtils.Preferences.FhrUploadEnabled) {
          return this._onUploadPrefChange();
        }
    }
    return undefined;
  },

  /**
   * Register the sync ping's shutdown handler.
   */
  registerSyncPingShutdown(fnShutdown) {
    if (this._fnSyncPingShutdown) {
      throw new Error("The sync ping shutdown handler is already registered.");
    }
    this._fnSyncPingShutdown = fnShutdown;
  },

  /**
   * Get an object describing the current state of this module for AsyncShutdown diagnostics.
   */
  _getState() {
    return {
      initialized: this._initialized,
      initStarted: this._initStarted,
      haveDelayedInitTask: !!this._delayedInitTask,
      shutdownBarrier: this._shutdownBarrier.state,
      connectionsBarrier: this._connectionsBarrier.state,
      sendModule: lazy.TelemetrySend.getShutdownState(),
      haveDelayedNewProfileTask: !!this._delayedNewPingTask,
      shutdownStep: this._shutdownStep,
    };
  },

  /**
   * Called whenever the FHR Upload preference changes (e.g. when user disables FHR from
   * the preferences panel), this triggers sending the "deletion-request" ping.
   */
  _onUploadPrefChange() {
    const uploadEnabled = Services.prefs.getBoolPref(
      TelemetryUtils.Preferences.FhrUploadEnabled,
      false
    );
    if (uploadEnabled) {
      this._log.trace(
        "_onUploadPrefChange - upload was enabled again. Resetting client ID"
      );

      // Delete cached client ID immediately, so other usage is forced to refetch it.
      this._clientID = null;

      // Generate a new client ID and make sure this module uses the new version
      let p = (async () => {
        await lazy.ClientID.removeClientID();
        let id = await lazy.ClientID.getClientID();
        this._clientID = id;
        Services.telemetry.scalarSet("telemetry.data_upload_optin", true);

        await this.saveUninstallPing().catch(e =>
          this._log.warn("_onUploadPrefChange - saveUninstallPing failed", e)
        );
      })();

      this._shutdownBarrier.client.addBlocker(
        "TelemetryController: resetting client ID after data upload was enabled",
        p
      );

      return;
    }

    let p = (async () => {
      try {
        // 1. Cancel the current pings.
        // 2. Clear unpersisted pings
        await lazy.TelemetrySend.clearCurrentPings();

        // 3. Remove all pending pings
        await lazy.TelemetryStorage.removeAppDataPings();
        await lazy.TelemetryStorage.runRemovePendingPingsTask();
        await lazy.TelemetryStorage.removeUninstallPings();
      } catch (e) {
        this._log.error(
          "_onUploadPrefChange - error clearing pending pings",
          e
        );
      } finally {
        // 4. Reset session and subsession counter
        lazy.TelemetrySession.resetSubsessionCounter();

        // 5. Collect any additional identifiers we want to send in the
        // deletion request.
        const scalars = Services.telemetry.getSnapshotForScalars(
          "deletion-request",
          /* clear */ true
        );

        // 6. Set ClientID to a known value
        let oldClientId = await lazy.ClientID.getClientID();
        await lazy.ClientID.setCanaryClientID();
        this._clientID = await lazy.ClientID.getClientID();

        // 7. Send the deletion-request ping.
        this._log.trace("_onUploadPrefChange - Sending deletion-request ping.");
        this.submitExternalPing(
          PING_TYPE_DELETION_REQUEST,
          { scalars },
          { overrideClientId: oldClientId }
        );
        this._deletionRequestPingSubmittedPromise = null;
      }
    })();

    this._deletionRequestPingSubmittedPromise = p;
    this._shutdownBarrier.client.addBlocker(
      "TelemetryController: removing pending pings after data upload was disabled",
      p
    );

    Services.obs.notifyObservers(
      null,
      TelemetryUtils.TELEMETRY_UPLOAD_DISABLED_TOPIC
    );
  },

  QueryInterface: ChromeUtils.generateQI(["nsISupportsWeakReference"]),

  _attachObservers() {
    if (TelemetryControllerBase.IS_UNIFIED_TELEMETRY) {
      // Watch the FHR upload setting to trigger "deletion-request" pings.
      Services.prefs.addObserver(
        TelemetryUtils.Preferences.FhrUploadEnabled,
        this,
        true
      );
    }
  },

  /**
   * Remove the preference observer to avoid leaks.
   */
  _detachObservers() {
    if (TelemetryControllerBase.IS_UNIFIED_TELEMETRY) {
      Services.prefs.removeObserver(
        TelemetryUtils.Preferences.FhrUploadEnabled,
        this
      );
    }
  },

  /**
   * Allows waiting for TelemetryControllers delayed initialization to complete.
   * This will complete before TelemetryController is shutting down.
   * @return {Promise} Resolved when delayed TelemetryController initialization completed.
   */
  promiseInitialized() {
    return this._delayedInitTaskDeferred.promise;
  },

  getCurrentPingData(aSubsession) {
    this._log.trace("getCurrentPingData - subsession: " + aSubsession);

    // Telemetry is disabled, don't gather any data.
    if (!Services.telemetry.canRecordBase) {
      return null;
    }

    const reason = aSubsession
      ? REASON_GATHER_SUBSESSION_PAYLOAD
      : REASON_GATHER_PAYLOAD;
    const type = PING_TYPE_MAIN;
    const payload = lazy.TelemetrySession.getPayload(reason);
    const options = { addClientId: true, addEnvironment: true };
    const ping = this.assemblePing(type, payload, options);

    return ping;
  },

  async reset() {
    this._clientID = null;
    this._fnSyncPingShutdown = null;
    this._detachObservers();

    let sessionReset = lazy.TelemetrySession.testReset();

    this._connectionsBarrier = new AsyncShutdown.Barrier(
      "TelemetryController: Waiting for pending ping activity"
    );
    this._shutdownBarrier = new AsyncShutdown.Barrier(
      "TelemetryController: Waiting for clients."
    );

    // We need to kick of the controller setup first for tests that check the
    // cached client id.
    let controllerSetup = this.setupTelemetry(true);

    await sessionReset;
    await lazy.TelemetrySend.reset();
    await lazy.TelemetryStorage.reset();
    await lazy.TelemetryEnvironment.testReset();

    await controllerSetup;
  },

  /**
   * Schedule sending the "new-profile" ping.
   */
  scheduleNewProfilePing() {
    this._log.trace("scheduleNewProfilePing");

    const sendDelay = Services.prefs.getIntPref(
      TelemetryUtils.Preferences.NewProfilePingDelay,
      NEWPROFILE_PING_DEFAULT_DELAY
    );

    this._delayedNewPingTask = new DeferredTask(async () => {
      try {
        await this.sendNewProfilePing();
      } finally {
        this._delayedNewPingTask = null;
      }
    }, sendDelay);

    this._delayedNewPingTask.arm();
  },

  /**
   * Generate and send the new-profile ping
   */
  async sendNewProfilePing() {
    this._log.trace(
      "sendNewProfilePing - shutting down: " + this._shuttingDown
    );

    const scalars = Services.telemetry.getSnapshotForScalars(
      "new-profile",
      /* clear */ true
    );

    // Generate the payload.
    const payload = {
      reason: this._shuttingDown ? "shutdown" : "startup",
      processes: {
        parent: {
          scalars: scalars.parent,
        },
      },
    };

    // Generate and send the "new-profile" ping. This uses the
    // pingsender if we're shutting down.
    let options = {
      addClientId: true,
      addEnvironment: true,
      usePingSender: this._shuttingDown,
    };
    // TODO: we need to be smarter about when to send the ping (and save the
    // state to file). |requestIdleCallback| is currently only accessible
    // through DOM. See bug 1361996.
    await TelemetryController.submitExternalPing(
      "new-profile",
      payload,
      options
    ).then(
      () => lazy.TelemetrySession.markNewProfilePingSent(),
      e =>
        this._log.error(
          "sendNewProfilePing - failed to submit new-profile ping",
          e
        )
    );
  },

  /**
   * Register 'dynamic builtin' probes from the JSON definition files.
   * This is needed to support adding new probes in developer builds
   * without rebuilding the whole codebase.
   *
   * This is not meant to be used outside of local developer builds.
   */
  async registerJsProbes() {
    // We don't support this outside of developer builds.
    if (AppConstants.MOZILLA_OFFICIAL && !this._testMode) {
      return;
    }

    this._log.trace("registerJsProbes - registering builtin JS probes");

    await this.registerScalarProbes();
    await this.registerEventProbes();
  },

  _loadProbeDefinitions(filename) {
    let probeFile = Services.dirsvc.get("GreD", Ci.nsIFile);
    probeFile.append(filename);
    if (!probeFile.exists()) {
      this._log.trace(
        `loadProbeDefinitions - no builtin JS probe file ${filename}`
      );
      return null;
    }

    return IOUtils.readUTF8(probeFile.path);
  },

  async registerScalarProbes() {
    this._log.trace(
      "registerScalarProbes - registering scalar builtin JS probes"
    );

    // Load the scalar probes JSON file.
    const scalarProbeFilename = "ScalarArtifactDefinitions.json";
    let scalarJSProbes = {};
    try {
      let fileContent = await this._loadProbeDefinitions(scalarProbeFilename);
      scalarJSProbes = JSON.parse(fileContent, (property, value) => {
        // Fixup the "kind" property: it's a string, and we need the constant
        // coming from nsITelemetry.
        if (property !== "kind" || typeof value != "string") {
          return value;
        }

        let newValue;
        switch (value) {
          case "nsITelemetry::SCALAR_TYPE_COUNT":
            newValue = Services.telemetry.SCALAR_TYPE_COUNT;
            break;
          case "nsITelemetry::SCALAR_TYPE_BOOLEAN":
            newValue = Services.telemetry.SCALAR_TYPE_BOOLEAN;
            break;
          case "nsITelemetry::SCALAR_TYPE_STRING":
            newValue = Services.telemetry.SCALAR_TYPE_STRING;
            break;
        }
        return newValue;
      });
    } catch (ex) {
      this._log.error(
        `registerScalarProbes - there was an error loading ${scalarProbeFilename}`,
        ex
      );
    }

    // Register the builtin probes.
    for (let category in scalarJSProbes) {
      // Expire the expired scalars
      for (let name in scalarJSProbes[category]) {
        let def = scalarJSProbes[category][name];
        if (
          !def ||
          !def.expires ||
          def.expires == "never" ||
          def.expires == "default"
        ) {
          continue;
        }
        if (
          Services.vc.compare(AppConstants.MOZ_APP_VERSION, def.expires) >= 0
        ) {
          def.expired = true;
        }
      }
      Services.telemetry.registerBuiltinScalars(
        category,
        scalarJSProbes[category]
      );
    }
  },

  async registerEventProbes() {
    this._log.trace(
      "registerEventProbes - registering builtin JS Event probes"
    );

    // Load the event probes JSON file.
    const eventProbeFilename = "EventArtifactDefinitions.json";
    let eventJSProbes = {};
    try {
      let fileContent = await this._loadProbeDefinitions(eventProbeFilename);
      eventJSProbes = JSON.parse(fileContent);
    } catch (ex) {
      this._log.error(
        `registerEventProbes - there was an error loading ${eventProbeFilename}`,
        ex
      );
    }

    // Register the builtin probes.
    for (let category in eventJSProbes) {
      for (let name in eventJSProbes[category]) {
        let def = eventJSProbes[category][name];
        if (
          !def ||
          !def.expires ||
          def.expires == "never" ||
          def.expires == "default"
        ) {
          continue;
        }
        if (
          Services.vc.compare(AppConstants.MOZ_APP_VERSION, def.expires) >= 0
        ) {
          def.expired = true;
        }
      }
      Services.telemetry.registerBuiltinEvents(
        category,
        eventJSProbes[category]
      );
    }
  },
};
