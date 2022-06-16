/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { Log } = ChromeUtils.import("resource://gre/modules/Log.jsm");

const lazy = {};

ChromeUtils.defineModuleGetter(
  lazy,
  "CommonUtils",
  "resource://services-common/utils.js"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "PromiseUtils",
  "resource://gre/modules/PromiseUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "ServiceRequest",
  "resource://gre/modules/ServiceRequest.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "UpdateUtils",
  "resource://gre/modules/UpdateUtils.jsm"
);

var EXPORTED_SYMBOLS = ["CoveragePing"];

const COVERAGE_VERSION = "2";

const COVERAGE_ENABLED_PREF = "toolkit.coverage.enabled";
const LOG_LEVEL_PREF = "toolkit.coverage.log-level";
const OPT_OUT_PREF = "toolkit.coverage.opt-out";
const ALREADY_RUN_PREF = `toolkit.coverage.already-run.v${COVERAGE_VERSION}`;
const COVERAGE_UUID_PREF = `toolkit.coverage.uuid.v${COVERAGE_VERSION}`;
const TELEMETRY_ENABLED_PREF = "datareporting.healthreport.uploadEnabled";
const REPORTING_ENDPOINT_BASE_PREF = `toolkit.coverage.endpoint.base`;
const REPORTING_ENDPOINT = "submit/coverage/coverage";
const PING_SUBMISSION_TIMEOUT = 30 * 1000; // 30 seconds

const log = Log.repository.getLogger("Telemetry::CoveragePing");
log.level = Services.prefs.getIntPref(LOG_LEVEL_PREF, Log.Level.Error);
log.addAppender(new Log.ConsoleAppender(new Log.BasicFormatter()));

var CoveragePing = Object.freeze({
  async startup() {
    if (!Services.prefs.getBoolPref(COVERAGE_ENABLED_PREF, false)) {
      log.debug("coverage not enabled");
      return;
    }

    if (Services.prefs.getBoolPref(OPT_OUT_PREF, false)) {
      log.debug("user has set opt-out pref");
      return;
    }

    if (Services.prefs.getBoolPref(ALREADY_RUN_PREF, false)) {
      log.debug("already run on this profile");
      return;
    }

    if (!Services.prefs.getCharPref(REPORTING_ENDPOINT_BASE_PREF, null)) {
      log.error("no endpoint base set");
      return;
    }

    try {
      await this.reportTelemetrySetting();
    } catch (e) {
      log.error("unable to upload payload", e);
    }
  },

  // NOTE - this does not use existing Telemetry code or honor Telemetry opt-out prefs,
  // by design. It also sends no identifying data like the client ID. See the "coverage ping"
  // documentation for details.
  reportTelemetrySetting() {
    const enabled = Services.prefs.getBoolPref(TELEMETRY_ENABLED_PREF, false);

    const payload = {
      appVersion: Services.appinfo.version,
      appUpdateChannel: lazy.UpdateUtils.getUpdateChannel(false),
      osName: Services.sysinfo.getProperty("name"),
      osVersion: Services.sysinfo.getProperty("version"),
      telemetryEnabled: enabled,
    };

    let cachedUuid = Services.prefs.getCharPref(COVERAGE_UUID_PREF, null);
    if (!cachedUuid) {
      // Totally random UUID, just for detecting duplicates.
      cachedUuid = lazy.CommonUtils.generateUUID();
      Services.prefs.setCharPref(COVERAGE_UUID_PREF, cachedUuid);
    }

    let reportingEndpointBase = Services.prefs.getCharPref(
      REPORTING_ENDPOINT_BASE_PREF,
      null
    );

    let endpoint = `${reportingEndpointBase}/${REPORTING_ENDPOINT}/${COVERAGE_VERSION}/${cachedUuid}`;

    log.debug(`putting to endpoint ${endpoint} with payload:`, payload);

    let deferred = lazy.PromiseUtils.defer();

    let request = new lazy.ServiceRequest({ mozAnon: true });
    request.mozBackgroundRequest = true;
    request.timeout = PING_SUBMISSION_TIMEOUT;

    request.open("PUT", endpoint, true);
    request.overrideMimeType("text/plain");
    request.setRequestHeader("Content-Type", "application/json; charset=UTF-8");
    request.setRequestHeader("Date", new Date().toUTCString());

    let errorhandler = event => {
      let failure = event.type;
      log.error(`error making request to ${endpoint}: ${failure}`);
      deferred.reject(event);
    };

    request.onerror = errorhandler;
    request.ontimeout = errorhandler;
    request.onabort = errorhandler;

    request.onloadend = event => {
      let status = request.status;
      let statusClass = status - (status % 100);
      let success = false;

      if (statusClass === 200) {
        // We can treat all 2XX as success.
        log.info(`successfully submitted, status: ${status}`);
        success = true;
      } else if (statusClass === 400) {
        // 4XX means that something with the request was broken.

        // TODO: we should handle this better, but for now we should avoid resubmitting
        // broken requests by pretending success.
        success = true;
        log.error(
          `error submitting to ${endpoint}, status: ${status} - ping request broken?`
        );
      } else if (statusClass === 500) {
        // 5XX means there was a server-side error and we should try again later.
        log.error(
          `error submitting to ${endpoint}, status: ${status} - server error, should retry later`
        );
      } else {
        // We received an unexpected status code.
        log.error(
          `error submitting to ${endpoint}, status: ${status}, type: ${event.type}`
        );
      }

      if (success) {
        Services.prefs.setBoolPref(ALREADY_RUN_PREF, true);
        log.debug(`result from PUT: ${request.responseText}`);
        deferred.resolve();
      } else {
        deferred.reject(event);
      }
    };

    request.send(JSON.stringify(payload));

    return deferred.promise;
  },
});
