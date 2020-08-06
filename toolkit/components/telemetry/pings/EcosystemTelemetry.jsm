/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This module sends the Telemetry Ecosystem pings periodically:
 * https://firefox-source-docs.mozilla.org/toolkit/components/telemetry/data/ecosystem-telemetry.html
 *
 * Note that ecosystem pings are only sent when the preference
 * `toolkit.telemetry.ecosystemtelemetry.enabled` is set to `true` - eventually
 * that will be the default, but you should check!
 *
 * Note also that these pings are currently only sent for users signed in to
 * Firefox with a Firefox account.
 *
 * If you are using the non-production FxA stack, pings are not sent by default.
 * To force them, you should set:
 * - toolkit.telemetry.ecosystemtelemetry.allowForNonProductionFxA: true
 *
 * If you are trying to debug this, you might also find the following
 * preferences useful:
 * - toolkit.telemetry.log.level: "Trace"
 * - toolkit.telemetry.log.dump: true
 */

"use strict";

var EXPORTED_SYMBOLS = ["EcosystemTelemetry"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm", this);

XPCOMUtils.defineLazyModuleGetters(this, {
  ONLOGIN_NOTIFICATION: "resource://gre/modules/FxAccountsCommon.js",
  ONLOGOUT_NOTIFICATION: "resource://gre/modules/FxAccountsCommon.js",
  ONVERIFIED_NOTIFICATION: "resource://gre/modules/FxAccountsCommon.js",
  ON_PRELOGOUT_NOTIFICATION: "resource://gre/modules/FxAccountsCommon.js",
  TelemetryController: "resource://gre/modules/TelemetryController.jsm",
  TelemetryUtils: "resource://gre/modules/TelemetryUtils.jsm",
  TelemetryEnvironment: "resource://gre/modules/TelemetryEnvironment.jsm",
  Log: "resource://gre/modules/Log.jsm",
  Services: "resource://gre/modules/Services.jsm",
  fxAccounts: "resource://gre/modules/FxAccounts.jsm",
  FxAccounts: "resource://gre/modules/FxAccounts.jsm",
  ClientID: "resource://gre/modules/ClientID.jsm",
});

XPCOMUtils.defineLazyServiceGetters(this, {
  Telemetry: ["@mozilla.org/base/telemetry;1", "nsITelemetry"],
});

const LOGGER_NAME = "Toolkit.Telemetry";
const LOGGER_PREFIX = "EcosystemTelemetry::";

XPCOMUtils.defineLazyGetter(this, "log", () => {
  return Log.repository.getLoggerWithMessagePrefix(LOGGER_NAME, LOGGER_PREFIX);
});

var Policy = {
  sendPing: (type, payload, options) =>
    TelemetryController.submitExternalPing(type, payload, options),
  monotonicNow: () => TelemetryUtils.monotonicNow(),
  // Returns a promise that resolves with the Ecosystem anonymized id.
  // Never rejects - will log an error and resolve with null on error.
  async getEcosystemAnonId() {
    try {
      let userData = await fxAccounts.getSignedInUser();
      if (!userData || !userData.verified) {
        log.debug("No ecosystem anonymized ID - no user or unverified user");
        return null;
      }
      return await fxAccounts.telemetry.ensureEcosystemAnonId();
    } catch (ex) {
      log.error("Failed to fetch the ecosystem anonymized ID", ex);
      return null;
    }
  },
  // Returns a promise that resolves with the current ecosystem client id.
  getEcosystemClientId() {
    return ClientID.getEcosystemClientID();
  },
};

var EcosystemTelemetry = {
  Reason: Object.freeze({
    PERIODIC: "periodic", // Send the ping in regular intervals
    SHUTDOWN: "shutdown", // Send the ping on shutdown
    LOGOUT: "logout", // Send after FxA logout
  }),
  PING_TYPE: "account-ecosystem",
  METRICS_STORE: "account-ecosystem",
  _lastSendTime: 0,
  // Indicates that the Ecosystem ping is configured and ready to send pings.
  _initialized: false,
  // The promise returned by Policy.getEcosystemAnonId()
  _promiseEcosystemAnonId: null,
  // Sets up _promiseEcosystemAnonId in the hope that it will be resolved by the
  // time we need it, and also already resolved when the user logs out.
  prepareEcosystemAnonId() {
    this._promiseEcosystemAnonId = Policy.getEcosystemAnonId();
  },

  enabled() {
    // Never enabled when not Unified Telemetry (e.g. not enabled on Fennec)
    // If not enabled, then it doesn't become enabled until the preferences
    // are adjusted and the browser is restarted.
    // Not enabled is different to "should I send pings?" - if enabled, then
    // observers will still be setup so we are ready to transition from not
    // sending pings into sending them.
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

    if (
      !FxAccounts.config.isProductionConfig() &&
      !Services.prefs.getBoolPref(
        TelemetryUtils.Preferences.EcosystemTelemetryAllowForNonProductionFxA,
        false
      )
    ) {
      log.info("Ecosystem telemetry disabled due to FxA non-production user");
      return false;
    }
    // We are enabled (although may or may not want to send pings.)
    return true;
  },

  /**
   * In what is an unfortunate level of coupling, FxA has hacks to call this
   * function before it sends any account related notifications. This allows us
   * to work correctly when logging out by ensuring we have the anonymized
   * ecosystem ID by then (as *at* logout time it's too late)
   */
  async prepareForFxANotification() {
    // Telemetry might not have initialized yet, so make sure we have.
    this.startup();
    // We need to ensure the promise fetching the anon ecosystem id has
    // resolved (but if we are pref'd off it will remain null.)
    if (this._promiseEcosystemAnonId) {
      await this._promiseEcosystemAnonId;
    }
  },

  /**
   * On startup, register all observers.
   */
  startup() {
    if (!this.enabled() || this._initialized) {
      return;
    }
    log.trace("Starting up.");

    // We "prime" the ecosystem id here - if it's not currently available, it
    // will be done in the background, so should be ready by the time we
    // actually need it.
    this.prepareEcosystemAnonId();

    this._addObservers();

    this._initialized = true;
  },

  /**
   * Shutdown this ping.
   *
   * This will send a final ping with the SHUTDOWN reason.
   */
  shutdown() {
    if (!this._initialized) {
      return;
    }
    log.trace("Shutting down.");
    this._submitPing(this.Reason.SHUTDOWN);

    this._removeObservers();
    this._initialized = false;
  },

  _addObservers() {
    // FxA login, verification and logout.
    Services.obs.addObserver(this, ONLOGIN_NOTIFICATION);
    Services.obs.addObserver(this, ONVERIFIED_NOTIFICATION);
    Services.obs.addObserver(this, ONLOGOUT_NOTIFICATION);
    Services.obs.addObserver(this, ON_PRELOGOUT_NOTIFICATION);
  },

  _removeObservers() {
    try {
      // removeObserver may throw, which could interrupt shutdown.
      Services.obs.removeObserver(this, ONLOGIN_NOTIFICATION);
      Services.obs.removeObserver(this, ONVERIFIED_NOTIFICATION);
      Services.obs.removeObserver(this, ONLOGOUT_NOTIFICATION);
      Services.obs.removeObserver(this, ON_PRELOGOUT_NOTIFICATION);
    } catch (ex) {}
  },

  observe(subject, topic, data) {
    log.trace(`observe, topic: ${topic}`);

    switch (topic) {
      // This is a bit messy - an already verified user will get
      // ONLOGIN_NOTIFICATION but *not* ONVERIFIED_NOTIFICATION. However, an
      // unverified user can't do the ecosystem dance with the profile server.
      // The only way to determine if the user is verified or not is via an
      // async method, and this isn't async, so...
      // Sadly, we just end up kicking off prepareEcosystemAnonId() twice in
      // that scenario, which will typically be rare and is handled by FxA. Note
      // also that we are just "priming" the ecosystem id here - if it's not
      // currently available, it will be done in the background, so should be
      // ready by the time we actually need it.
      case ONLOGIN_NOTIFICATION:
      case ONVERIFIED_NOTIFICATION:
        // If we sent these pings for non-account users and this is a login
        // notification, we'd want to submit now, so we have a fresh set of data
        // for the user.
        // But for now, all we need to do is start the promise to fetch the anon
        // ID.
        this.prepareEcosystemAnonId();
        break;

      case ONLOGOUT_NOTIFICATION:
        // On logout we submit what we have, then switch to the "no anon id"
        // state.
        // Returns the promise for tests.
        return this._submitPing(this.Reason.LOGOUT)
          .then(() => {
            // Ensure _promiseEcosystemAnonId() is now going to resolve as null.
            this.prepareEcosystemAnonId();
          })
          .catch(e => {
            log.error("ONLOGOUT promise chain failed", e);
          });

      case ON_PRELOGOUT_NOTIFICATION:
        // We don't need to do anything here - everything was done in startup.
        // However, we keep this here so someone doesn't erroneously think the
        // notification serves no purposes - it's the `observerPreloads` in
        // FxAccounts that matters!
        break;
    }
    return null;
  },

  // Called by TelemetryScheduler.jsm when periodic pings should be sent.
  periodicPing() {
    log.trace("periodic ping triggered");
    return this._submitPing(this.Reason.PERIODIC);
  },

  /**
   * Submit an ecosystem ping.
   *
   * It will not send a ping if Ecosystem Telemetry is disabled
   * the module is not fully initialized or if the ping type is missing.
   *
   * It will automatically assemble the right payload and clear out Telemetry stores.
   *
   * @param {String} reason The reason we're sending the ping. One of TelemetryEcosystemPing.Reason.
   */
  async _submitPing(reason) {
    if (!this.enabled()) {
      // It's possible we will end up here if FxA was using the production
      // stack at startup but no longer is.
      log.trace(`_submitPing was called, but ping is not enabled.`);
      return;
    }

    if (!this._initialized) {
      log.trace(`Not initialized when sending. Bug?`);
      return;
    }

    log.trace(`_submitPing, reason: ${reason}`);

    let now = Policy.monotonicNow();

    // Duration in seconds
    let duration = Math.round((now - this._lastSendTime) / 1000);
    this._lastSendTime = now;

    let payload = await this._payload(reason, duration);
    if (!payload) {
      // The reason for returning null will already have been logged.
      return;
    }

    // Never include the client ID.
    // We provide our own environment.
    const options = {
      addClientId: false,
      addEnvironment: true,
      overrideEnvironment: this._environment(),
      usePingSender: reason === this.Reason.SHUTDOWN,
    };

    let id = await Policy.sendPing(this.PING_TYPE, payload, options);
    log.info(`submitted ping ${id}`);
  },

  /**
   * Assemble payload for a new ping
   *
   * @param {String} reason The reason we're sending the ping. One of TelemetryEcosystemPing.Reason.
   * @param {Number} duration The duration since ping was last send in seconds.
   */
  async _payload(reason, duration) {
    let ecosystemAnonId = await this._promiseEcosystemAnonId;
    if (!ecosystemAnonId) {
      // This typically just means no user is logged in, so don't make too
      // much noise.
      log.info("Unable to determine the ecosystem anon id; skipping this ping");
      return null;
    }

    let payload = {
      reason,
      ecosystemAnonId,
      ecosystemClientId: await Policy.getEcosystemClientId(),
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

    return payload;
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
};
