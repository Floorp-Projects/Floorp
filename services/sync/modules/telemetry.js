/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [
  "ErrorSanitizer", // for testing.
  "SyncRecord",
  "SyncTelemetry",
];

// Support for Sync-and-FxA-related telemetry, which is submitted in a special-purpose
// telemetry ping called the "sync ping", documented here:
//
//  ../../../toolkit/components/telemetry/docs/data/sync-ping.rst
//
// The sync ping contains identifiers that are linked to the user's Firefox Account
// and are separate from the main telemetry client_id, so this file is also responsible
// for ensuring that we can delete those pings upon user request, by plumbing its
// identifiers into the "deletion-request" ping.

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { Log } = ChromeUtils.import("resource://gre/modules/Log.jsm");

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  Async: "resource://services-common/async.js",
  AuthenticationError: "resource://services-sync/sync_auth.js",
  FxAccounts: "resource://gre/modules/FxAccounts.jsm",
  ObjectUtils: "resource://gre/modules/ObjectUtils.jsm",
  Observers: "resource://services-common/observers.js",
  OS: "resource://gre/modules/osfile.jsm",
  Resource: "resource://services-sync/resource.js",
  Status: "resource://services-sync/status.js",
  Svc: "resource://services-sync/util.js",
  TelemetryController: "resource://gre/modules/TelemetryController.jsm",
  TelemetryEnvironment: "resource://gre/modules/TelemetryEnvironment.jsm",
  TelemetryUtils: "resource://gre/modules/TelemetryUtils.jsm",
  Weave: "resource://services-sync/main.js",
});

XPCOMUtils.defineLazyGetter(lazy, "fxAccounts", () => {
  return ChromeUtils.import(
    "resource://gre/modules/FxAccounts.jsm"
  ).getFxAccountsSingleton();
});

let constants = ChromeUtils.import("resource://services-sync/constants.js");

XPCOMUtils.defineLazyGetter(
  lazy,
  "WeaveService",
  () => Cc["@mozilla.org/weave/service;1"].getService().wrappedJSObject
);
const log = Log.repository.getLogger("Sync.Telemetry");

const TOPICS = [
  // For tracking change to account/device identifiers.
  "fxaccounts:new_device_id",
  "fxaccounts:onlogout",
  "weave:service:ready",
  "weave:service:login:got-hashed-id",

  // For whole-of-sync metrics.
  "weave:service:sync:start",
  "weave:service:sync:finish",
  "weave:service:sync:error",

  // For individual engine metrics.
  "weave:engine:sync:start",
  "weave:engine:sync:finish",
  "weave:engine:sync:error",
  "weave:engine:sync:applied",
  "weave:engine:sync:step",
  "weave:engine:sync:uploaded",
  "weave:engine:validate:finish",
  "weave:engine:validate:error",

  // For ad-hoc telemetry events.
  "weave:telemetry:event",
  "weave:telemetry:histogram",
  "fxa:telemetry:event",

  "weave:telemetry:migration",
];

const PING_FORMAT_VERSION = 1;

const EMPTY_UID = "0".repeat(32);

// The set of engines we record telemetry for - any other engines are ignored.
const ENGINES = new Set([
  "addons",
  "bookmarks",
  "clients",
  "forms",
  "history",
  "passwords",
  "prefs",
  "tabs",
  "extension-storage",
  "addresses",
  "creditcards",
]);

function tryGetMonotonicTimestamp() {
  try {
    return Services.telemetry.msSinceProcessStart();
  } catch (e) {
    log.warn("Unable to get a monotonic timestamp!");
    return -1;
  }
}

function timeDeltaFrom(monotonicStartTime) {
  let now = tryGetMonotonicTimestamp();
  if (monotonicStartTime !== -1 && now !== -1) {
    return Math.round(now - monotonicStartTime);
  }
  return -1;
}

const NS_ERROR_MODULE_BASE_OFFSET = 0x45;
const NS_ERROR_MODULE_NETWORK = 6;

// A reimplementation of NS_ERROR_GET_MODULE, which surprisingly doesn't seem
// to exist anywhere in .js code in a way that can be reused.
// This is taken from DownloadCore.jsm.
function NS_ERROR_GET_MODULE(code) {
  return ((code & 0x7fff0000) >> 16) - NS_ERROR_MODULE_BASE_OFFSET;
}

// Converts extra integer fields to strings, rounds floats to three
// decimal places (nanosecond precision for timings), and removes profile
// directory paths and URLs from potential error messages.
function normalizeExtraTelemetryFields(extra) {
  let result = {};
  for (let key in extra) {
    let value = extra[key];
    let type = typeof value;
    if (type == "string") {
      result[key] = ErrorSanitizer.cleanErrorMessage(value);
    } else if (type == "number") {
      result[key] = Number.isInteger(value)
        ? value.toString(10)
        : value.toFixed(3);
    } else if (type != "undefined") {
      throw new TypeError(
        `Invalid type ${type} for extra telemetry field ${key}`
      );
    }
  }
  return lazy.ObjectUtils.isEmpty(result) ? undefined : result;
}

// The `ErrorSanitizer` has 2 main jobs:
// * Remove PII from errors, such as the profile dir or URLs the user might
//   have visited.
// * Normalize errors so different locales or operating systems etc don't
//   generate different messages for the same underlying error.
// * [TODO] Normalize errors so environmental factors don't influence message.
//   For example, timestamps or GUIDs should be replaced with something static.
class ErrorSanitizer {
  // Things we normalize - this isn't exhaustive, but covers the common error messages we see.
  // Eg:
  // > Win error 112 during operation write on file [profileDir]\weave\addonsreconciler.json (Espacio en disco insuficiente. )
  // > Win error 112 during operation write on file [profileDir]\weave\addonsreconciler.json (Diskte yeterli yer yok. )
  // > <snip many other translations of the error>
  // > Unix error 28 during operation write on file [profileDir]/weave/addonsreconciler.json (No space left on device)
  // These tend to crowd out other errors we might care about (eg, top 5 errors for some engines are
  // variations of the "no space left on device")

  // Note that only errors that have same-but-different errors on Windows and Unix are here - we
  // still sanitize ones that aren't in these maps to remove the translations etc - eg,
  // `ERROR_SHARING_VIOLATION` doesn't really have a unix equivalent, so doesn't appear here, but
  // we still strip the translations to avoid the variants.
  static E_NO_SPACE_ON_DEVICE = "OS error [No space left on device]";
  static E_PERMISSION_DENIED = "OS error [Permission denied]";
  static E_NO_FILE_OR_DIR = "OS error [File/Path not found]";
  static E_NO_MEM = "OS error [No memory]";

  static WindowsErrorSubstitutions = {
    "2": this.E_NO_FILE_OR_DIR, // ERROR_FILE_NOT_FOUND
    "3": this.E_NO_FILE_OR_DIR, // ERROR_PATH_NOT_FOUND
    "5": this.E_PERMISSION_DENIED, // ERROR_ACCESS_DENIED
    "8": this.E_NO_MEM, // ERROR_NOT_ENOUGH_MEMORY
    "112": this.E_NO_SPACE_ON_DEVICE, // ERROR_DISK_FULL
  };

  static UnixErrorSubstitutions = {
    "2": this.E_NO_FILE_OR_DIR, // ENOENT
    "12": this.E_NO_MEM, // ENOMEM
    "13": this.E_PERMISSION_DENIED, // EACCESS
    "28": this.E_NO_SPACE_ON_DEVICE, // ENOSPC
  };

  static reWinError = /^(?<head>Win error (?<errno>\d+))(?<detail>.*) \(.*\r?\n?\)$/m;
  static reUnixError = /^(?<head>Unix error (?<errno>\d+))(?<detail>.*) \(.*\)$/;

  static #cleanOSErrorMessage(error) {
    let match = this.reWinError.exec(error);
    if (match) {
      let head =
        this.WindowsErrorSubstitutions[match.groups.errno] || match.groups.head;
      return head + match.groups.detail.replaceAll("\\", "/");
    }
    match = this.reUnixError.exec(error);
    if (match) {
      let head =
        this.UnixErrorSubstitutions[match.groups.errno] || match.groups.head;
      return head + match.groups.detail;
    }
    return error;
  }

  // A regex we can use to replace the profile dir in error messages. We use a
  // regexp so we can simply replace all case-insensitive occurences.
  // This escaping function is from:
  // https://developer.mozilla.org/en/docs/Web/JavaScript/Guide/Regular_Expressions
  static reProfileDir = new RegExp(
    lazy.OS.Constants.Path.profileDir.replace(/[.*+?^${}()|[\]\\]/g, "\\$&"),
    "gi"
  );

  // The "public" entry-point.
  static cleanErrorMessage(error) {
    // There's a chance the profiledir is in the error string which is PII we
    // want to avoid including in the ping.
    error = error.replace(this.reProfileDir, "[profileDir]");
    // MSG_INVALID_URL from /dom/bindings/Errors.msg -- no way to access this
    // directly from JS.
    if (error.endsWith("is not a valid URL.")) {
      error = "<URL> is not a valid URL.";
    }
    // Try to filter things that look somewhat like a URL (in that they contain a
    // colon in the middle of non-whitespace), in case anything else is including
    // these in error messages. Note that JSON.stringified stuff comes through
    // here, so we explicitly ignore double-quotes as well.
    error = error.replace(/[^\s"]+:[^\s"]+/g, "<URL>");

    // Anywhere that's normalized the guid in errors we can easily filter
    // to make it easier to aggregate these types of errors
    error = error.replace(/<guid: ([^>]+)>/g, "<GUID>");
    return this.#cleanOSErrorMessage(error);
  }
}

// This function validates the payload of a telemetry "event" - this can be
// removed once there are APIs available for the telemetry modules to collect
// these events (bug 1329530) - but for now we simulate that planned API as
// best we can.
function validateTelemetryEvent(eventDetails) {
  let { object, method, value, extra } = eventDetails;
  // Do do basic validation of the params - everything except "extra" must
  // be a string. method and object are required.
  if (
    typeof method != "string" ||
    typeof object != "string" ||
    (value && typeof value != "string") ||
    (extra && typeof extra != "object")
  ) {
    log.warn("Invalid event parameters - wrong types", eventDetails);
    return false;
  }
  // length checks.
  if (
    method.length > 20 ||
    object.length > 20 ||
    (value && value.length > 80)
  ) {
    log.warn("Invalid event parameters - wrong lengths", eventDetails);
    return false;
  }

  // extra can be falsey, or an object with string names and values.
  if (extra) {
    if (Object.keys(extra).length > 10) {
      log.warn("Invalid event parameters - too many extra keys", eventDetails);
      return false;
    }
    for (let [ename, evalue] of Object.entries(extra)) {
      if (
        typeof ename != "string" ||
        ename.length > 15 ||
        typeof evalue != "string" ||
        evalue.length > 85
      ) {
        log.warn(
          `Invalid event parameters: extra item "${ename} is invalid`,
          eventDetails
        );
        return false;
      }
    }
  }
  return true;
}

class EngineRecord {
  constructor(name) {
    // startTime is in ms from process start, but is monotonic (unlike Date.now())
    // so we need to keep both it and when.
    this.startTime = tryGetMonotonicTimestamp();
    this.name = name;

    // This allows cases like bookmarks-buffered to have a separate name from
    // the bookmarks engine.
    let engineImpl = lazy.Weave.Service.engineManager.get(name);
    if (engineImpl && engineImpl.overrideTelemetryName) {
      this.overrideTelemetryName = engineImpl.overrideTelemetryName;
    }
  }

  toJSON() {
    let result = { name: this.overrideTelemetryName || this.name };
    let properties = [
      "took",
      "status",
      "failureReason",
      "incoming",
      "outgoing",
      "validation",
      "steps",
    ];
    for (let property of properties) {
      result[property] = this[property];
    }
    return result;
  }

  finished(error) {
    let took = timeDeltaFrom(this.startTime);
    if (took > 0) {
      this.took = took;
    }
    if (error) {
      this.failureReason = SyncTelemetry.transformError(error);
    }
  }

  recordApplied(counts) {
    if (this.incoming) {
      log.error(
        `Incoming records applied multiple times for engine ${this.name}!`
      );
      return;
    }
    if (this.name === "clients" && !counts.failed) {
      // ignore successful application of client records
      // since otherwise they show up every time and are meaningless.
      return;
    }

    let incomingData = {};
    // Counts has extra stuff used for logging, but we only care about a few
    let properties = ["applied", "failed"];
    // Only record non-zero properties and only record incoming at all if
    // there's at least one property we care about.
    for (let property of properties) {
      if (counts[property]) {
        incomingData[property] = counts[property];
        this.incoming = incomingData;
      }
    }
  }

  recordStep(stepResult) {
    let step = {
      name: stepResult.name,
    };
    if (stepResult.took > 0) {
      step.took = Math.round(stepResult.took);
    }
    if (stepResult.counts) {
      let counts = stepResult.counts.filter(({ count }) => count > 0);
      if (counts.length) {
        step.counts = counts;
      }
    }
    if (this.steps) {
      this.steps.push(step);
    } else {
      this.steps = [step];
    }
  }

  recordValidation(validationResult) {
    if (this.validation) {
      log.error(`Multiple validations occurred for engine ${this.name}!`);
      return;
    }
    let { problems, version, took, checked } = validationResult;
    let validation = {
      version: version || 0,
      checked: checked || 0,
    };
    if (took > 0) {
      validation.took = Math.round(took);
    }
    let summarized = problems.filter(({ count }) => count > 0);
    if (summarized.length) {
      validation.problems = summarized;
    }
    this.validation = validation;
  }

  recordValidationError(e) {
    if (this.validation) {
      log.error(`Multiple validations occurred for engine ${this.name}!`);
      return;
    }

    this.validation = {
      failureReason: SyncTelemetry.transformError(e),
    };
  }

  recordUploaded(counts) {
    if (counts.sent || counts.failed) {
      if (!this.outgoing) {
        this.outgoing = [];
      }
      this.outgoing.push({
        sent: counts.sent || undefined,
        failed: counts.failed || undefined,
      });
    }
  }
}

// The record of a single "sync" - typically many of these are submitted in
// a single ping (ie, as a 'syncs' array)
class SyncRecord {
  constructor(allowedEngines, why) {
    this.allowedEngines = allowedEngines;
    // Our failure reason. This property only exists in the generated ping if an
    // error actually occurred.
    this.failureReason = undefined;
    this.syncNodeType = null;
    this.when = Date.now();
    this.startTime = tryGetMonotonicTimestamp();
    this.took = 0; // will be set later.
    this.why = why;

    // All engines that have finished (ie, does not include the "current" one)
    // We omit this from the ping if it's empty.
    this.engines = [];
    // The engine that has started but not yet stopped.
    this.currentEngine = null;
  }

  toJSON() {
    let result = {
      when: this.when,
      took: this.took,
      failureReason: this.failureReason,
      status: this.status,
    };
    if (this.why) {
      result.why = this.why;
    }
    let engines = [];
    for (let engine of this.engines) {
      engines.push(engine.toJSON());
    }
    if (engines.length > 0) {
      result.engines = engines;
    }
    return result;
  }

  finished(error) {
    this.took = timeDeltaFrom(this.startTime);
    if (this.currentEngine != null) {
      log.error(
        "Finished called for the sync before the current engine finished"
      );
      this.currentEngine.finished(null);
      this.onEngineStop(this.currentEngine.name);
    }
    if (error) {
      this.failureReason = SyncTelemetry.transformError(error);
    }

    this.syncNodeType = lazy.Weave.Service.identity.telemetryNodeType;

    // Check for engine statuses. -- We do this now, and not in engine.finished
    // to make sure any statuses that get set "late" are recorded
    for (let engine of this.engines) {
      let status = lazy.Status.engines[engine.name];
      if (status && status !== constants.ENGINE_SUCCEEDED) {
        engine.status = status;
      }
    }

    let statusObject = {};

    let serviceStatus = lazy.Status.service;
    if (serviceStatus && serviceStatus !== constants.STATUS_OK) {
      statusObject.service = serviceStatus;
      this.status = statusObject;
    }
    let syncStatus = lazy.Status.sync;
    if (syncStatus && syncStatus !== constants.SYNC_SUCCEEDED) {
      statusObject.sync = syncStatus;
      this.status = statusObject;
    }
  }

  onEngineStart(engineName) {
    if (this._shouldIgnoreEngine(engineName, false)) {
      return;
    }

    if (this.currentEngine) {
      log.error(
        `Being told that engine ${engineName} has started, but current engine ${this.currentEngine.name} hasn't stopped`
      );
      // Just discard the current engine rather than making up data for it.
    }
    this.currentEngine = new EngineRecord(engineName);
  }

  onEngineStop(engineName, error) {
    // We only care if it's the current engine if we have a current engine.
    if (this._shouldIgnoreEngine(engineName, !!this.currentEngine)) {
      return;
    }
    if (!this.currentEngine) {
      // It's possible for us to get an error before the start message of an engine
      // (somehow), in which case we still want to record that error.
      if (!error) {
        return;
      }
      log.error(
        `Error triggered on ${engineName} when no current engine exists: ${error}`
      );
      this.currentEngine = new EngineRecord(engineName);
    }
    this.currentEngine.finished(error);
    this.engines.push(this.currentEngine);
    this.currentEngine = null;
  }

  onEngineApplied(engineName, counts) {
    if (this._shouldIgnoreEngine(engineName)) {
      return;
    }
    this.currentEngine.recordApplied(counts);
  }

  onEngineStep(engineName, step) {
    if (this._shouldIgnoreEngine(engineName)) {
      return;
    }
    this.currentEngine.recordStep(step);
  }

  onEngineValidated(engineName, validationData) {
    if (this._shouldIgnoreEngine(engineName, false)) {
      return;
    }
    let engine = this.engines.find(e => e.name === engineName);
    if (
      !engine &&
      this.currentEngine &&
      engineName === this.currentEngine.name
    ) {
      engine = this.currentEngine;
    }
    if (engine) {
      engine.recordValidation(validationData);
    } else {
      log.warn(
        `Validation event triggered for engine ${engineName}, which hasn't been synced!`
      );
    }
  }

  onEngineValidateError(engineName, error) {
    if (this._shouldIgnoreEngine(engineName, false)) {
      return;
    }
    let engine = this.engines.find(e => e.name === engineName);
    if (
      !engine &&
      this.currentEngine &&
      engineName === this.currentEngine.name
    ) {
      engine = this.currentEngine;
    }
    if (engine) {
      engine.recordValidationError(error);
    } else {
      log.warn(
        `Validation failure event triggered for engine ${engineName}, which hasn't been synced!`
      );
    }
  }

  onEngineUploaded(engineName, counts) {
    if (this._shouldIgnoreEngine(engineName)) {
      return;
    }
    this.currentEngine.recordUploaded(counts);
  }

  _shouldIgnoreEngine(engineName, shouldBeCurrent = true) {
    if (!this.allowedEngines.has(engineName)) {
      log.info(
        `Notification for engine ${engineName}, but we aren't recording telemetry for it`
      );
      return true;
    }
    if (shouldBeCurrent) {
      if (!this.currentEngine || engineName != this.currentEngine.name) {
        log.error(`Notification for engine ${engineName} but it isn't current`);
        return true;
      }
    }
    return false;
  }
}

// The entire "sync ping" - it includes all the syncs, events etc recorded in
// the ping.
class SyncTelemetryImpl {
  constructor(allowedEngines) {
    log.manageLevelFromPref("services.sync.log.logger.telemetry");
    // This is accessible so we can enable custom engines during tests.
    this.allowedEngines = allowedEngines;
    this.current = null;
    this.setupObservers();

    this.payloads = [];
    this.discarded = 0;
    this.events = [];
    this.histograms = {};
    this.migrations = [];
    this.maxEventsCount = lazy.Svc.Prefs.get("telemetry.maxEventsCount", 1000);
    this.maxPayloadCount = lazy.Svc.Prefs.get("telemetry.maxPayloadCount");
    this.submissionInterval =
      lazy.Svc.Prefs.get("telemetry.submissionInterval") * 1000;
    this.lastSubmissionTime = Services.telemetry.msSinceProcessStart();
    this.lastUID = EMPTY_UID;
    this.lastSyncNodeType = null;
    this.currentSyncNodeType = null;
    // Note that the sessionStartDate is somewhat arbitrary - the telemetry
    // modules themselves just use `new Date()`. This means that our startDate
    // isn't going to be the same as the sessionStartDate in the main pings,
    // but that's OK for now - if it's a problem we'd need to change the
    // telemetry modules to expose what it thinks the sessionStartDate is.
    let sessionStartDate = new Date();
    this.sessionStartDate = lazy.TelemetryUtils.toLocalTimeISOString(
      lazy.TelemetryUtils.truncateToHours(sessionStartDate)
    );
    lazy.TelemetryController.registerSyncPingShutdown(() => this.shutdown());
  }

  sanitizeFxaDeviceId(deviceId) {
    return lazy.fxAccounts.telemetry.sanitizeDeviceId(deviceId);
  }

  prepareFxaDevices(devices) {
    // For non-sync users, the data per device is limited -- just an id and a
    // type (and not even the id yet). For sync users, if we can correctly map
    // the fxaDevice to a sync device, then we can get os and version info,
    // which would be quite unfortunate to lose.
    let extraInfoMap = new Map();
    if (this.syncIsEnabled()) {
      for (let client of this.getClientsEngineRecords()) {
        if (client.fxaDeviceId) {
          extraInfoMap.set(client.fxaDeviceId, {
            os: client.os,
            version: client.version,
            syncID: this.sanitizeFxaDeviceId(client.id),
          });
        }
      }
    }
    // Finally, sanitize and convert to the proper format.
    return devices.map(d => {
      let { os, version, syncID } = extraInfoMap.get(d.id) || {
        os: undefined,
        version: undefined,
        syncID: undefined,
      };
      return {
        id: this.sanitizeFxaDeviceId(d.id) || EMPTY_UID,
        type: d.type,
        os,
        version,
        syncID,
      };
    });
  }

  syncIsEnabled() {
    return lazy.WeaveService.enabled && lazy.WeaveService.ready;
  }

  // Separate for testing.
  getClientsEngineRecords() {
    if (!this.syncIsEnabled()) {
      throw new Error("Bug: syncIsEnabled() must be true, check it first");
    }
    return lazy.Weave.Service.clientsEngine.remoteClients;
  }

  updateFxaDevices(devices) {
    if (!devices) {
      return {};
    }
    let me = devices.find(d => d.isCurrentDevice);
    let id = me ? this.sanitizeFxaDeviceId(me.id) : undefined;
    let cleanDevices = this.prepareFxaDevices(devices);
    return { deviceID: id, devices: cleanDevices };
  }

  getFxaDevices() {
    return lazy.fxAccounts.device.recentDeviceList;
  }

  getPingJSON(reason) {
    let { devices, deviceID } = this.updateFxaDevices(this.getFxaDevices());
    return {
      os: lazy.TelemetryEnvironment.currentEnvironment.system.os,
      why: reason,
      devices,
      discarded: this.discarded || undefined,
      version: PING_FORMAT_VERSION,
      syncs: this.payloads.slice(),
      uid: this.lastUID,
      syncNodeType: this.lastSyncNodeType || undefined,
      deviceID,
      sessionStartDate: this.sessionStartDate,
      events: this.events.length == 0 ? undefined : this.events,
      migrations: this.migrations.length == 0 ? undefined : this.migrations,
      histograms:
        Object.keys(this.histograms).length == 0 ? undefined : this.histograms,
    };
  }

  _addMigrationRecord(type, info) {
    log.debug("Saw telemetry migration info", type, info);
    // Updates to this need to be documented in `sync-ping.rst`
    switch (type) {
      case "webext-storage":
        this.migrations.push({
          type: "webext-storage",
          entries: +info.entries,
          entriesSuccessful: +info.entries_successful,
          extensions: +info.extensions,
          extensionsSuccessful: +info.extensions_successful,
          openFailure: !!info.open_failure,
        });
        break;
      default:
        throw new Error("Bug: Unknown migration record type " + type);
    }
  }

  finish(reason) {
    // Note that we might be in the middle of a sync right now, and so we don't
    // want to touch this.current.
    let result = this.getPingJSON(reason);
    this.payloads = [];
    this.discarded = 0;
    this.events = [];
    this.migrations = [];
    this.histograms = {};
    this.submit(result);
  }

  setupObservers() {
    for (let topic of TOPICS) {
      lazy.Observers.add(topic, this, this);
    }
  }

  shutdown() {
    this.finish("shutdown");
    for (let topic of TOPICS) {
      lazy.Observers.remove(topic, this, this);
    }
  }

  submit(record) {
    if (!this.isProductionSyncUser()) {
      return false;
    }
    // We still call submit() with possibly illegal payloads so that tests can
    // know that the ping was built. We don't end up submitting them, however.
    let numEvents = record.events ? record.events.length : 0;
    let numMigrations = record.migrations ? record.migrations.length : 0;
    if (record.syncs.length || numEvents || numMigrations) {
      log.trace(
        `submitting ${record.syncs.length} sync record(s) and ` +
          `${numEvents} event(s) to telemetry`
      );
      lazy.TelemetryController.submitExternalPing("sync", record, {
        usePingSender: true,
      }).catch(err => {
        log.error("failed to submit ping", err);
      });
      return true;
    }
    return false;
  }

  isProductionSyncUser() {
    // If FxA isn't production then we treat sync as not being production.
    // Further, there's the deprecated "services.sync.tokenServerURI" pref we
    // need to consider - fxa doesn't consider that as if that's the only
    // pref set, they *are* running a production fxa, just not production sync.
    if (
      !lazy.FxAccounts.config.isProductionConfig() ||
      Services.prefs.prefHasUserValue("services.sync.tokenServerURI")
    ) {
      log.trace(`Not sending telemetry ping for self-hosted Sync user`);
      return false;
    }
    return true;
  }

  onSyncStarted(data) {
    const why = data && JSON.parse(data).why;
    if (this.current) {
      log.warn(
        "Observed weave:service:sync:start, but we're already recording a sync!"
      );
      // Just discard the old record, consistent with our handling of engines, above.
      this.current = null;
    }
    this.current = new SyncRecord(this.allowedEngines, why);
  }

  // We need to ensure that the telemetry `deletion-request` ping always contains the user's
  // current sync device ID, because if the user opts out of telemetry then the deletion ping
  // will be immediately triggered for sending, and we won't have a chance to fill it in later.
  // This keeps the `deletion-ping` up-to-date when the user's account state changes.
  onAccountInitOrChange() {
    // We don't submit sync pings for self-hosters, so don't need to collect their device ids either.
    if (!this.isProductionSyncUser()) {
      return;
    }
    // Awkwardly async, but no need to await. If the user's account state changes while
    // this promise is in flight, it will reject and we won't record any data in the ping.
    // (And a new notification will trigger us to try again with the new state).
    lazy.fxAccounts.device
      .getLocalId()
      .then(deviceId => {
        let sanitizedDeviceId = lazy.fxAccounts.telemetry.sanitizeDeviceId(
          deviceId
        );
        // In the past we did not persist the FxA metrics identifiers to disk,
        // so this might be missing until we can fetch it from the server for the
        // first time. There will be a fresh notification tirggered when it's available.
        if (sanitizedDeviceId) {
          // Sanitized device ids are 64 characters long, but telemetry limits scalar strings to 50.
          // The first 32 chars are sufficient to uniquely identify the device, so just send those.
          // It's hard to change the sync ping itself to only send 32 chars, to b/w compat reasons.
          sanitizedDeviceId = sanitizedDeviceId.substr(0, 32);
          Services.telemetry.scalarSet(
            "deletion.request.sync_device_id",
            sanitizedDeviceId
          );
        }
      })
      .catch(err => {
        log.warn(
          `Failed to set sync identifiers in the deletion-request ping: ${err}`
        );
      });
  }

  // This keeps the `deletion-request` ping up-to-date when the user signs out,
  // clearing the now-nonexistent sync device id.
  onAccountLogout() {
    Services.telemetry.scalarSet("deletion.request.sync_device_id", "");
  }

  _checkCurrent(topic) {
    if (!this.current) {
      // This is only `info` because it happens when we do a tabs "quick-write"
      log.info(
        `Observed notification ${topic} but no current sync is being recorded.`
      );
      return false;
    }
    return true;
  }

  _shouldSubmitForDataChange() {
    let newID = lazy.fxAccounts.telemetry.getSanitizedUID() || EMPTY_UID;
    let oldID = this.lastUID;
    if (
      newID != EMPTY_UID &&
      oldID != EMPTY_UID &&
      // Both are "real" uids, so we care if they've changed.
      newID != oldID
    ) {
      log.trace(
        `shouldSubmitForDataChange - uid from '${oldID}' -> '${newID}'`
      );
      return true;
    }
    // We've gone from knowing one of the ids to not knowing it (which we
    // ignore) or we've gone from not knowing it to knowing it (which is fine),
    // Now check the node type because a change there also means we should
    // submit.
    if (
      this.lastSyncNodeType &&
      this.currentSyncNodeType != this.lastSyncNodeType
    ) {
      log.trace(
        `shouldSubmitForDataChange - nodeType from '${this.lastSyncNodeType}' -> '${this.currentSyncNodeType}'`
      );
      return true;
    }
    log.trace("shouldSubmitForDataChange - no need to submit");
    return false;
  }

  maybeSubmitForDataChange() {
    if (this._shouldSubmitForDataChange()) {
      log.info(
        "Early submission of sync telemetry due to changed IDs/NodeType"
      );
      this.finish("idchange"); // this actually submits.
      this.lastSubmissionTime = Services.telemetry.msSinceProcessStart();
    }

    // Only update the last UIDs if we actually know them.
    let current_uid = lazy.fxAccounts.telemetry.getSanitizedUID();
    if (current_uid) {
      this.lastUID = current_uid;
    }
    if (this.currentSyncNodeType) {
      this.lastSyncNodeType = this.currentSyncNodeType;
    }
  }

  maybeSubmitForInterval() {
    // We want to submit the ping every `this.submissionInterval` but only when
    // there's no current sync in progress, otherwise we may end up submitting
    // the sync and the events caused by it in different pings.
    if (
      this.current == null &&
      Services.telemetry.msSinceProcessStart() - this.lastSubmissionTime >
        this.submissionInterval
    ) {
      this.finish("schedule");
      this.lastSubmissionTime = Services.telemetry.msSinceProcessStart();
    }
  }

  onSyncFinished(error) {
    if (!this.current) {
      log.warn("onSyncFinished but we aren't recording");
      return;
    }
    this.current.finished(error);
    this.currentSyncNodeType = this.current.syncNodeType;
    let current = this.current;
    this.current = null;
    this.takeTelemetryRecord(current);
  }

  takeTelemetryRecord(record) {
    // We check for "data change" before appending the current sync to payloads,
    // as it is the current sync which has the data with the new data, and thus
    // must go in the *next* submission.
    this.maybeSubmitForDataChange();
    if (this.payloads.length < this.maxPayloadCount) {
      this.payloads.push(record.toJSON());
    } else {
      ++this.discarded;
    }
    // If we are submitting due to timing, it's desirable that the most recent
    // sync is included, so we check after appending the record.
    this.maybeSubmitForInterval();
  }

  _addHistogram(hist) {
    let histogram = Services.telemetry.getHistogramById(hist);
    let s = histogram.snapshot();
    this.histograms[hist] = s;
  }

  _recordEvent(eventDetails) {
    this.maybeSubmitForDataChange();

    if (this.events.length >= this.maxEventsCount) {
      log.warn("discarding event - already queued our maximum", eventDetails);
      return;
    }

    let { object, method, value, extra } = eventDetails;
    if (extra) {
      extra = normalizeExtraTelemetryFields(extra);
      eventDetails = { object, method, value, extra };
    }

    if (!validateTelemetryEvent(eventDetails)) {
      // we've already logged what the problem is...
      return;
    }
    log.debug("recording event", eventDetails);

    if (extra && lazy.Resource.serverTime && !extra.serverTime) {
      extra.serverTime = String(lazy.Resource.serverTime);
    }
    let category = "sync";
    let ts = Math.floor(tryGetMonotonicTimestamp());

    // An event record is a simple array with at least 4 items.
    let event = [ts, category, method, object];
    // It may have up to 6 elements if |extra| is defined
    if (value) {
      event.push(value);
      if (extra) {
        event.push(extra);
      }
    } else if (extra) {
      event.push(null); // a null for the empty value.
      event.push(extra);
    }
    this.events.push(event);
    this.maybeSubmitForInterval();
  }

  observe(subject, topic, data) {
    log.trace(`observed ${topic} ${data}`);

    switch (topic) {
      case "weave:service:ready":
      case "weave:service:login:got-hashed-id":
      case "fxaccounts:new_device_id":
        this.onAccountInitOrChange();
        break;

      case "fxaccounts:onlogout":
        this.onAccountLogout();
        break;

      /* sync itself state changes */
      case "weave:service:sync:start":
        this.onSyncStarted(data);
        break;

      case "weave:service:sync:finish":
        if (this._checkCurrent(topic)) {
          this.onSyncFinished(null);
        }
        break;

      case "weave:service:sync:error":
        // argument needs to be truthy (this should always be the case)
        this.onSyncFinished(subject || "Unknown");
        break;

      /* engine sync state changes */
      case "weave:engine:sync:start":
        if (this._checkCurrent(topic)) {
          this.current.onEngineStart(data);
        }
        break;
      case "weave:engine:sync:finish":
        if (this._checkCurrent(topic)) {
          this.current.onEngineStop(data, null);
        }
        break;

      case "weave:engine:sync:error":
        if (this._checkCurrent(topic)) {
          // argument needs to be truthy (this should always be the case)
          this.current.onEngineStop(data, subject || "Unknown");
        }
        break;

      /* engine counts */
      case "weave:engine:sync:applied":
        if (this._checkCurrent(topic)) {
          this.current.onEngineApplied(data, subject);
        }
        break;

      case "weave:engine:sync:step":
        if (this._checkCurrent(topic)) {
          this.current.onEngineStep(data, subject);
        }
        break;

      case "weave:engine:sync:uploaded":
        if (this._checkCurrent(topic)) {
          this.current.onEngineUploaded(data, subject);
        }
        break;

      case "weave:engine:validate:finish":
        if (this._checkCurrent(topic)) {
          this.current.onEngineValidated(data, subject);
        }
        break;

      case "weave:engine:validate:error":
        if (this._checkCurrent(topic)) {
          this.current.onEngineValidateError(data, subject || "Unknown");
        }
        break;

      case "weave:telemetry:event":
      case "fxa:telemetry:event":
        this._recordEvent(subject);
        break;

      case "weave:telemetry:histogram":
        this._addHistogram(data);
        break;

      case "weave:telemetry:migration":
        this._addMigrationRecord(data, subject);
        break;

      default:
        log.warn(`unexpected observer topic ${topic}`);
        break;
    }
  }

  // Transform an exception into a standard description. Exposed here for when
  // this module isn't directly responsible for knowing the transform should
  // happen (for example, when including an error in the |extra| field of
  // event telemetry)
  transformError(error) {
    // Certain parts of sync will use this pattern as a way to communicate to
    // processIncoming to abort the processing. However, there's no guarantee
    // this can only happen then.
    if (typeof error == "object" && error.code && error.cause) {
      error = error.cause;
    }
    if (lazy.Async.isShutdownException(error)) {
      return { name: "shutdownerror" };
    }

    if (typeof error === "string") {
      if (error.startsWith("error.")) {
        // This is hacky, but I can't imagine that it's not also accurate.
        return { name: "othererror", error };
      }
      error = ErrorSanitizer.cleanErrorMessage(error);
      return { name: "unexpectederror", error };
    }

    if (error instanceof lazy.AuthenticationError) {
      return { name: "autherror", from: error.source };
    }

    let httpCode =
      error.status || (error.response && error.response.status) || error.code;

    if (httpCode) {
      return { name: "httperror", code: httpCode };
    }

    if (error.failureCode) {
      return { name: "othererror", error: error.failureCode };
    }

    if (error.result) {
      // many "nsresult" errors are actually network errors - if they are
      // associated with the "network" module we assume that's true.
      // We also assume NS_ERROR_ABORT is such an error - for almost everything
      // we care about, it acually is (eg, if the connection fails early enough
      // or if we have a captive portal etc) - we don't lose anything by this
      // assumption, it's just that the error will no longer be in the "nserror"
      // category, so our analysis can still find them.
      if (
        error.result == Cr.NS_ERROR_ABORT ||
        NS_ERROR_GET_MODULE(error.result) == NS_ERROR_MODULE_NETWORK
      ) {
        return { name: "httperror", code: error.result };
      }
      return { name: "nserror", code: error.result };
    }
    // It's probably an Error object, but it also could be some
    // other object that may or may not override toString to do
    // something useful.
    let msg = String(error);
    if (msg.startsWith("[object")) {
      // Nothing useful in the default, check for a string "message" property.
      if (typeof error.message == "string") {
        msg = String(error.message);
      } else {
        // Hopefully it won't come to this...
        msg = JSON.stringify(error);
      }
    }
    return {
      name: "unexpectederror",
      error: ErrorSanitizer.cleanErrorMessage(msg),
    };
  }
}

var SyncTelemetry = new SyncTelemetryImpl(ENGINES);
