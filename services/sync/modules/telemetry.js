/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

this.EXPORTED_SYMBOLS = ["SyncTelemetry"];

Cu.import("resource://services-sync/browserid_identity.js");
Cu.import("resource://services-sync/main.js");
Cu.import("resource://services-sync/status.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-common/observers.js");
Cu.import("resource://services-common/async.js");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/TelemetryController.jsm");
Cu.import("resource://gre/modules/FxAccounts.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/osfile.jsm", this);

let constants = {};
Cu.import("resource://services-sync/constants.js", constants);

var fxAccountsCommon = {};
Cu.import("resource://gre/modules/FxAccountsCommon.js", fxAccountsCommon);

XPCOMUtils.defineLazyServiceGetter(this, "Telemetry",
                                   "@mozilla.org/base/telemetry;1",
                                   "nsITelemetry");

const log = Log.repository.getLogger("Sync.Telemetry");

const TOPICS = [
  "profile-before-change",
  "weave:service:sync:start",
  "weave:service:sync:finish",
  "weave:service:sync:error",

  "weave:engine:sync:start",
  "weave:engine:sync:finish",
  "weave:engine:sync:error",
  "weave:engine:sync:applied",
  "weave:engine:sync:uploaded",
  "weave:engine:validate:finish",
  "weave:engine:validate:error",
];

const PING_FORMAT_VERSION = 1;

// The set of engines we record telemetry for - any other engines are ignored.
const ENGINES = new Set(["addons", "bookmarks", "clients", "forms", "history",
                         "passwords", "prefs", "tabs", "extension-storage"]);

// A regex we can use to replace the profile dir in error messages. We use a
// regexp so we can simply replace all case-insensitive occurences.
// This escaping function is from:
// https://developer.mozilla.org/en/docs/Web/JavaScript/Guide/Regular_Expressions
const reProfileDir = new RegExp(
        OS.Constants.Path.profileDir.replace(/[.*+?^${}()|[\]\\]/g, "\\$&"),
        "gi");

function transformError(error, engineName) {
  if (Async.isShutdownException(error)) {
    return { name: "shutdownerror" };
  }

  if (typeof error === "string") {
    if (error.startsWith("error.")) {
      // This is hacky, but I can't imagine that it's not also accurate.
      return { name: "othererror", error };
    }
    // There's a chance the profiledir is in the error string which is PII we
    // want to avoid including in the ping.
    error = error.replace(reProfileDir, "[profileDir]");
    return { name: "unexpectederror", error };
  }

  if (error.failureCode) {
    return { name: "othererror", error: error.failureCode };
  }

  if (error instanceof AuthenticationError) {
    return { name: "autherror", from: error.source };
  }

  if (error instanceof Ci.mozIStorageError) {
    return { name: "sqlerror", code: error.result };
  }

  let httpCode = error.status ||
    (error.response && error.response.status) ||
    error.code;

  if (httpCode) {
    return { name: "httperror", code: httpCode };
  }

  if (error.result) {
    return { name: "nserror", code: error.result };
  }

  return {
    name: "unexpectederror",
    // as above, remove the profile dir value.
    error: String(error).replace(reProfileDir, "[profileDir]")
  }
}

function tryGetMonotonicTimestamp() {
  try {
    return Telemetry.msSinceProcessStart();
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

class EngineRecord {
  constructor(name) {
    // startTime is in ms from process start, but is monotonic (unlike Date.now())
    // so we need to keep both it and when.
    this.startTime = tryGetMonotonicTimestamp();
    this.name = name;
  }

  toJSON() {
    let result = Object.assign({}, this);
    delete result.startTime;
    return result;
  }

  finished(error) {
    let took = timeDeltaFrom(this.startTime);
    if (took > 0) {
      this.took = took;
    }
    if (error) {
      this.failureReason = transformError(error, this.name);
    }
  }

  recordApplied(counts) {
    if (this.incoming) {
      log.error(`Incoming records applied multiple times for engine ${this.name}!`);
      return;
    }
    if (this.name === "clients" && !counts.failed) {
      // ignore successful application of client records
      // since otherwise they show up every time and are meaningless.
      return;
    }

    let incomingData = {};
    let properties = ["applied", "failed", "newFailed", "reconciled"];
    // Only record non-zero properties and only record incoming at all if
    // there's at least one property we care about.
    for (let property of properties) {
      if (counts[property]) {
        incomingData[property] = counts[property];
        this.incoming = incomingData;
      }
    }
  }

  recordValidation(validationResult) {
    if (this.validation) {
      log.error(`Multiple validations occurred for engine ${this.name}!`);
      return;
    }
    let { problems, version, duration, recordCount } = validationResult;
    let validation = {
      version: version || 0,
      checked: recordCount || 0,
    };
    if (duration > 0) {
      validation.took = Math.round(duration);
    }
    let summarized = problems.getSummary(true).filter(({count}) => count > 0);
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
      failureReason: transformError(e)
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

class TelemetryRecord {
  constructor(allowedEngines) {
    this.allowedEngines = allowedEngines;
    // Our failure reason. This property only exists in the generated ping if an
    // error actually occurred.
    this.failureReason = undefined;
    this.uid = "";
    this.when = Date.now();
    this.startTime = tryGetMonotonicTimestamp();
    this.took = 0; // will be set later.

    // All engines that have finished (ie, does not include the "current" one)
    // We omit this from the ping if it's empty.
    this.engines = [];
    // The engine that has started but not yet stopped.
    this.currentEngine = null;
  }

  toJSON() {
    let result = {
      when: this.when,
      uid: this.uid,
      took: this.took,
      failureReason: this.failureReason,
      status: this.status,
      deviceID: this.deviceID,
      devices: this.devices,
    };
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
      log.error("Finished called for the sync before the current engine finished");
      this.currentEngine.finished(null);
      this.onEngineStop(this.currentEngine.name);
    }
    if (error) {
      this.failureReason = transformError(error);
    }

    // We don't bother including the "devices" field if we can't come up with a
    // UID or device ID for *this* device -- If that's the case, any data we'd
    // put there would be likely to be full of garbage anyway.
    let includeDeviceInfo = false;
    try {
      this.uid = Weave.Service.identity.hashedUID();
      let deviceID = Weave.Service.identity.deviceID();
      if (deviceID) {
        // Combine the raw device id with the metrics uid to create a stable
        // unique identifier that can't be mapped back to the user's FxA
        // identity without knowing the metrics HMAC key.
        this.deviceID = Utils.sha256(deviceID + this.uid);
        includeDeviceInfo = true;
      }
    } catch (e) {
      this.uid = "0".repeat(32);
      this.deviceID = undefined;
    }

    if (includeDeviceInfo) {
      let remoteDevices = Weave.Service.clientsEngine.remoteClients;
      this.devices = remoteDevices.map(device => {
        return {
          os: device.os,
          version: device.version,
          id: Utils.sha256(device.id + this.uid)
        };
      });
    }

    // Check for engine statuses. -- We do this now, and not in engine.finished
    // to make sure any statuses that get set "late" are recorded
    for (let engine of this.engines) {
      let status = Status.engines[engine.name];
      if (status && status !== constants.ENGINE_SUCCEEDED) {
        engine.status = status;
      }
    }

    let statusObject = {};

    let serviceStatus = Status.service;
    if (serviceStatus && serviceStatus !== constants.STATUS_OK) {
      statusObject.service = serviceStatus;
      this.status = statusObject;
    }
    let syncStatus = Status.sync;
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
      log.error(`Being told that engine ${engineName} has started, but current engine ${
        this.currentEngine.name} hasn't stopped`);
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
      log.error(`Error triggered on ${engineName} when no current engine exists: ${error}`);
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

  onEngineValidated(engineName, validationData) {
    if (this._shouldIgnoreEngine(engineName, false)) {
      return;
    }
    let engine = this.engines.find(e => e.name === engineName);
    if (!engine && this.currentEngine && engineName === this.currentEngine.name) {
      engine = this.currentEngine;
    }
    if (engine) {
      engine.recordValidation(validationData);
    } else {
      log.warn(`Validation event triggered for engine ${engineName}, which hasn't been synced!`);
    }
  }

  onEngineValidateError(engineName, error) {
    if (this._shouldIgnoreEngine(engineName, false)) {
      return;
    }
    let engine = this.engines.find(e => e.name === engineName);
    if (!engine && this.currentEngine && engineName === this.currentEngine.name) {
      engine = this.currentEngine;
    }
    if (engine) {
      engine.recordValidationError(error);
    } else {
      log.warn(`Validation failure event triggered for engine ${engineName}, which hasn't been synced!`);
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
      log.info(`Notification for engine ${engineName}, but we aren't recording telemetry for it`);
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

class SyncTelemetryImpl {
  constructor(allowedEngines) {
    log.level = Log.Level[Svc.Prefs.get("log.logger.telemetry", "Trace")];
    // This is accessible so we can enable custom engines during tests.
    this.allowedEngines = allowedEngines;
    this.current = null;
    this.setupObservers();

    this.payloads = [];
    this.discarded = 0;
    this.maxPayloadCount = Svc.Prefs.get("telemetry.maxPayloadCount");
    this.submissionInterval = Svc.Prefs.get("telemetry.submissionInterval") * 1000;
    this.lastSubmissionTime = Telemetry.msSinceProcessStart();
  }

  getPingJSON(reason) {
    return {
      why: reason,
      discarded: this.discarded || undefined,
      version: PING_FORMAT_VERSION,
      syncs: this.payloads.slice(),
    };
  }

  finish(reason) {
    // Note that we might be in the middle of a sync right now, and so we don't
    // want to touch this.current.
    let result = this.getPingJSON(reason);
    this.payloads = [];
    this.discarded = 0;
    this.submit(result);
  }

  setupObservers() {
    for (let topic of TOPICS) {
      Observers.add(topic, this, this);
    }
  }

  shutdown() {
    this.finish("shutdown");
    for (let topic of TOPICS) {
      Observers.remove(topic, this, this);
    }
  }

  submit(record) {
    // We still call submit() with possibly illegal payloads so that tests can
    // know that the ping was built. We don't end up submitting them, however.
    if (record.syncs.length) {
      log.trace(`submitting ${record.syncs.length} sync record(s) to telemetry`);
      TelemetryController.submitExternalPing("sync", record);
    }
  }


  onSyncStarted() {
    if (this.current) {
      log.warn("Observed weave:service:sync:start, but we're already recording a sync!");
      // Just discard the old record, consistent with our handling of engines, above.
      this.current = null;
    }
    this.current = new TelemetryRecord(this.allowedEngines);
  }

  _checkCurrent(topic) {
    if (!this.current) {
      log.warn(`Observed notification ${topic} but no current sync is being recorded.`);
      return false;
    }
    return true;
  }

  onSyncFinished(error) {
    if (!this.current) {
      log.warn("onSyncFinished but we aren't recording");
      return;
    }
    this.current.finished(error);
    if (this.payloads.length < this.maxPayloadCount) {
      this.payloads.push(this.current.toJSON());
    } else {
      ++this.discarded;
    }
    this.current = null;
    if ((Telemetry.msSinceProcessStart() - this.lastSubmissionTime) > this.submissionInterval) {
      this.finish("schedule");
      this.lastSubmissionTime = Telemetry.msSinceProcessStart();
    }
  }

  observe(subject, topic, data) {
    log.trace(`observed ${topic} ${data}`);

    switch (topic) {
      case "profile-before-change":
        this.shutdown();
        break;

      /* sync itself state changes */
      case "weave:service:sync:start":
        this.onSyncStarted();
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

      default:
        log.warn(`unexpected observer topic ${topic}`);
        break;
    }
  }
}

this.SyncTelemetry = new SyncTelemetryImpl(ENGINES);
