/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This module sends the Telemetry Ecosystem pings periodically:
 * https://firefox-source-docs.mozilla.org/toolkit/components/telemetry/telemetry/data/ecosystem-ping.html
 */

"use strict";

var EXPORTED_SYMBOLS = ["EcosystemTelemetry"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm", this);

XPCOMUtils.defineLazyModuleGetters(this, {
  Weave: "resource://services-sync/main.js",
  ONLOGIN_NOTIFICATION: "resource://gre/modules/FxAccountsCommon.js",
  ONLOGOUT_NOTIFICATION: "resource://gre/modules/FxAccountsCommon.js",
  TelemetryController: "resource://gre/modules/TelemetryController.jsm",
  TelemetryUtils: "resource://gre/modules/TelemetryUtils.jsm",
  TelemetryEnvironment: "resource://gre/modules/TelemetryEnvironment.jsm",
  Log: "resource://gre/modules/Log.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

XPCOMUtils.defineLazyServiceGetters(this, {
  Telemetry: ["@mozilla.org/base/telemetry;1", "nsITelemetry"],
});

const LOGGER_NAME = "Toolkit.Telemetry";
const LOGGER_PREFIX = "EcosystemTelemetry::";

var Policy = {
  sendPing: (type, payload, options) =>
    TelemetryController.submitExternalPing(type, payload, options),
  monotonicNow: () => TelemetryUtils.monotonicNow(),
  fxaUid: () => {
    try {
      return Weave.Service.identity.hashedUID();
    } catch (ex) {
      return null;
    }
  },
  isClientConfigured: () =>
    Weave.Status.checkSetup() !== Weave.CLIENT_NOT_CONFIGURED,
};

var EcosystemTelemetry = {
  Reason: Object.freeze({
    PERIODIC: "periodic", // Send the ping in regular intervals
    SHUTDOWN: "shutdown", // Send the ping on shutdown
    LOGIN: "login", // Send after FxA login
    LOGOUT: "logout", // Send after FxA logout
  }),
  PingType: Object.freeze({
    PRE: "pre-account",
    // TODO(bug 1530654): Implement post-account ping
    POST: "post-account",
  }),
  METRICS_STORE: "pre-account",
  _logger: null,
  _lastSendTime: 0,
  // The current ping type to send, one of PingType
  _pingType: null,
  // Indicates if we saw a login event and the UID is pending
  _uidPending: false,
  // Indicates that the Ecosystem ping is configured and ready to send pings
  _initialized: false,

  enabled() {
    // Never enabled when not Unified Telemetry (e.g. not enabled on Fennec)
    if (
      !Services.prefs.getBoolPref(TelemetryUtils.Preferences.Unified, false)
    ) {
      return false;
    }

    if (
      !Services.prefs.getBoolPref(
        TelemetryUtils.Preferences.EcosystemTelemetryEnabled,
        false
      )
    ) {
      return false;
    }

    return true;
  },

  /**
   * On startup, register all observers.
   */
  startup() {
    if (!this.enabled()) {
      return;
    }
    this._log.trace("Starting up.");
    this._addObservers();

    // Always start with a pre-account ping.
    // We switch to post-account if we detect a sync/login
    // On short sessions, when we don't see a login/sync, we always send a pre-account ping.
    this._pingType = this.PingType.PRE;
    this._initialized = true;
  },

  /**
   * Shutdown this ping.
   *
   * This will send a final ping with the SHUTDOWN reason.
   */
  shutdown() {
    if (!this.enabled()) {
      return;
    }
    this._log.trace("Shutting down.");
    this._submitPing(this._pingType, this.Reason.SHUTDOWN);

    this._removeObservers();
  },

  _addObservers() {
    // FxA login
    Services.obs.addObserver(this, ONLOGIN_NOTIFICATION);
    // FxA logout
    Services.obs.addObserver(this, ONLOGOUT_NOTIFICATION);
    // FxA/Sync - initialized and knows if client configured
    Services.obs.addObserver(this, "weave:service:ready");
    // FxA/Sync - done, uid might be available
    Services.obs.addObserver(this, "weave:service:login:change");
  },

  _removeObservers() {
    try {
      // removeObserver may throw, which could interrupt shutdown.
      Services.obs.removeObserver(this, ONLOGIN_NOTIFICATION);
      Services.obs.removeObserver(this, ONLOGOUT_NOTIFICATION);
      Services.obs.removeObserver(this, "weave:service:ready");
      Services.obs.removeObserver(this, "weave:service:login:change");
    } catch (ex) {}
  },

  observe(subject, topic, data) {
    this._log.trace(`observe, topic: ${topic}`);

    switch (topic) {
      case ONLOGIN_NOTIFICATION:
        // On new login, we will switch pings once UID is available
        this._uidPending = true;
        break;

      case ONLOGOUT_NOTIFICATION:
        // On logout we switch to pre-account again
        this._uidPending = false;
        this._pingType = this.PingType.PRE;
        this._submitPing(this._pingType, this.Reason.LOGOUT);
        break;

      case "weave:service:login:change":
        let uid = Policy.fxaUid();
        if (this._uidPending && uid) {
          // When we saw a login before and have a UID we switch to post-account
          this._submitPing(this._pingType, this.Reason.LOGIN);
          this._pingType = this.PingType.POST;
          this._uidPending = false;
        } else if (!this._uidPending && uid) {
          // We didn't see a login before, but have a UID.
          // This can only be true after a process restart and when the first sync happened
          // Next interval will be a post-account ping.
          this._pingType = this.PingType.POST;
        } else if (!uid) {
          // No uid means we keep sending in pre-account pings
          this._pingType = this.PingType.PRE;
        }
        break;

      case "weave:service:ready":
        // If client is configured, we send post-account pings
        if (Policy.isClientConfigured()) {
          this._pingType = this.PingType.POST;
        } else {
          // Otherwise we keep sending pre-account pings
          this._pingType = this.PingType.PRE;
        }
        break;
    }
  },

  periodicPing() {
    this._log.trace("periodic ping triggered");
    this._submitPing(this._pingType, this.Reason.PERIODIC);
  },

  /**
   * Submit an ecosystem ping.
   *
   * It will not send a ping if Ecosystem Telemetry is disabled
   * the module is not fully initialized or if the ping type is missing.
   *
   * It will automatically assemble the right payload and clear out Telemetry stores.
   *
   * @param {String} pingType The ping type to send. One of TelemetryEcosystemPing.PingType.
   * @param {String} reason The reason we're sending the ping. One of TelemetryEcosystemPing.Reason.
   */
  _submitPing(pingType, reason) {
    if (!this.enabled()) {
      this._log.trace(`_submitPing was called, but ping is not enabled. Bug?`);
      return;
    }

    if (!this._initialized || !pingType) {
      this._log.trace(
        `Not initialized or ping type undefined when sending. Bug?`
      );
      return;
    }

    if (pingType == this.PingType.POST) {
      // TODO(bug 1530654): Implement post-account ping
      this._log.trace(
        `Post-account ping not implemented yet. Sending pre-account instead.`
      );
      pingType = this.PingType.PRE;
    }

    this._log.trace(`_submitPing, ping type: ${pingType}, reason: ${reason}`);

    let now = Policy.monotonicNow();

    // Duration in seconds
    let duration = Math.round((now - this._lastSendTime) / 1000);
    this._lastSendTime = now;

    let payload = this._payload(reason, duration);

    // Never include the client ID.
    // We provide our own environment.
    const options = {
      addClientId: false,
      addEnvironment: true,
      overrideEnvironment: this._environment(),
      usePingSender: reason === this.Reason.SHUTDOWN,
    };

    Policy.sendPing(pingType, payload, options);
  },

  /**
   * Assemble payload for a new ping
   *
   * @param {String} reason The reason we're sending the ping. One of TelemetryEcosystemPing.Reason.
   * @param {Number} duration The duration since ping was last send in seconds.
   */
  _payload(reason, duration) {
    let payload = {
      reason,
      ecosystemClientId: this._ecosystemClientId(),
      duration,

      scalars: Telemetry.getSnapshotForScalars(
        this.METRICS_STORE,
        /* clear */ true,
        /* filter test */ true
      ),
      keyedScalars: Telemetry.getSnapshotForKeyedScalars(
        this.METRICS_STORE,
        /* clear */ true,
        /* filter test */ true
      ),
      histograms: Telemetry.getSnapshotForHistograms(
        this.METRICS_STORE,
        /* clear */ true,
        /* filter test */ true
      ),
      keyedHistograms: Telemetry.getSnapshotForKeyedHistograms(
        this.METRICS_STORE,
        /* clear */ true,
        /* filter test */ true
      ),
    };

    // UID is only allowed on post-account or login pings
    if (this._pingType == this.PingType.POST || reason == this.Reason.LOGIN) {
      let uid = Policy.fxaUid();
      if (uid) {
        // after additional data-review, we include the real uid.
        payload.uid = "00000000000000000000000000000000";
      }
    }

    return payload;
  },

  /**
   * Get the current ecosystem client ID
   *
   * TODO(bug 1530655): Replace by actual persisted ecosystem client ID as per spec
   */
  _ecosystemClientId() {
    return "unknown";
  },

  /**
   * Get the minimal environment to include in the ping
   */
  _environment() {
    let currentEnv = TelemetryEnvironment.currentEnvironment;
    let environment = {
      settings: {
        locale: currentEnv.settings.locale,
      },
      system: {
        memoryMB: currentEnv.system.memoryMB,
        os: {
          name: currentEnv.system.os.name,
          version: currentEnv.system.os.version,
          locale: currentEnv.system.os.locale,
        },
        cpu: {
          speedMHz: currentEnv.system.cpu.speedMHz,
        },
      },
      profile: {}, // added conditionally
    };

    if (currentEnv.profile.creationDate) {
      environment.profile.creationDate = currentEnv.profile.creationDate;
    }

    if (currentEnv.profile.firstUseDate) {
      environment.profile.firstUseDate = currentEnv.profile.firstUseDate;
    }

    return environment;
  },

  testReset() {
    this._initialized = false;
    this._lastSendTime = 0;
    this.startup();
  },

  get _log() {
    if (!this._logger) {
      this._logger = Log.repository.getLoggerWithMessagePrefix(
        LOGGER_NAME,
        LOGGER_PREFIX
      );
    }

    return this._logger;
  },
};
