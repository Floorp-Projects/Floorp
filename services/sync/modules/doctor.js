/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// A doctor for our collections. She can be asked to make a consultation, and
// may just diagnose an issue without attempting to cure it, may diagnose and
// attempt to cure, or may decide she is overworked and underpaid.
// Or something - naming is hard :)

"use strict";

this.EXPORTED_SYMBOLS = ["Doctor"];

const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-common/async.js");
Cu.import("resource://services-common/observers.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/resource.js");

Cu.import("resource://services-sync/util.js");
XPCOMUtils.defineLazyModuleGetter(this, "getRepairRequestor",
  "resource://services-sync/collection_repair.js");
XPCOMUtils.defineLazyModuleGetter(this, "getAllRepairRequestors",
  "resource://services-sync/collection_repair.js");

const log = Log.repository.getLogger("Sync.Doctor");

this.REPAIR_ADVANCE_PERIOD = 86400; // 1 day

this.Doctor = {
  anyClientsRepairing(service, collection, ignoreFlowID = null) {
    if (!service || !service.clientsEngine) {
      log.info("Missing clients engine, assuming we're in test code");
      return false;
    }
    return service.clientsEngine.remoteClients.some(client =>
      client.commands && client.commands.some(command => {
        if (command.command != "repairResponse" && command.command != "repairRequest") {
          return false;
        }
        if (!command.args || command.args.length != 1) {
          return false;
        }
        if (command.args[0].collection != collection) {
          return false;
        }
        if (ignoreFlowID != null && command.args[0].flowID == ignoreFlowID) {
          return false;
        }
        return true;
      })
    );
  },

  async consult(recentlySyncedEngines) {
    if (!Services.telemetry.canRecordBase) {
      log.info("Skipping consultation: telemetry reporting is disabled");
      return;
    }

    let engineInfos = this._getEnginesToValidate(recentlySyncedEngines);

    await this._runValidators(engineInfos);

    // We are called at the end of a sync, which is a good time to periodically
    // check each repairer to see if it can advance.
    if (this._now() - this.lastRepairAdvance > REPAIR_ADVANCE_PERIOD) {
      try {
        for (let [collection, requestor] of Object.entries(this._getAllRepairRequestors())) {
          try {
            let advanced = await requestor.continueRepairs();
            log.info(`${collection} reparier ${advanced ? "advanced" : "did not advance"}.`);
          } catch (ex) {
            if (Async.isShutdownException(ex)) {
              throw ex;
            }
            log.error(`${collection} repairer failed`, ex);
          }
        }
      } finally {
        this.lastRepairAdvance = this._now();
      }
    }
  },

  _getEnginesToValidate(recentlySyncedEngines) {
    let result = {};
    for (let e of recentlySyncedEngines) {
      let prefPrefix = `engine.${e.name}.`;
      if (!Svc.Prefs.get(prefPrefix + "validation.enabled", false)) {
        log.info(`Skipping check of ${e.name} - disabled via preferences`);
        continue;
      }
      // Check the last validation time for the engine.
      let lastValidation = Svc.Prefs.get(prefPrefix + "validation.lastTime", 0);
      let validationInterval = Svc.Prefs.get(prefPrefix + "validation.interval");
      let nowSeconds = this._now();

      if (nowSeconds - lastValidation < validationInterval) {
        log.info(`Skipping validation of ${e.name}: too recent since last validation attempt`);
        continue;
      }
      // Update the time now, even if we decline to actually perform a
      // validation. We don't want to check the rest of these more frequently
      // than once a day.
      Svc.Prefs.set(prefPrefix + "validation.lastTime", Math.floor(nowSeconds));

      // Validation only occurs a certain percentage of the time.
      let validationProbability = Svc.Prefs.get(prefPrefix + "validation.percentageChance", 0) / 100.0;
      if (validationProbability < Math.random()) {
        log.info(`Skipping validation of ${e.name}: Probability threshold not met`);
        continue;
      }

      let maxRecords = Svc.Prefs.get(prefPrefix + "validation.maxRecords");
      if (!maxRecords) {
        log.info(`Skipping validation of ${e.name}: No maxRecords specified`);
        continue;
      }
      // OK, so this is a candidate - the final decision will be based on the
      // number of records actually found.
      result[e.name] = { engine: e, maxRecords };
    }
    return result;
  },

  async _runValidators(engineInfos) {
    if (Object.keys(engineInfos).length == 0) {
      log.info("Skipping validation: no engines qualify");
      return;
    }

    if (Object.values(engineInfos).filter(i => i.maxRecords != -1).length != 0) {
      // at least some of the engines have maxRecord restrictions which require
      // us to ask the server for the counts.
      let countInfo = await this._fetchCollectionCounts();
      for (let [engineName, recordCount] of Object.entries(countInfo)) {
        if (engineName in engineInfos) {
          engineInfos[engineName].recordCount = recordCount;
        }
      }
    }

    for (let [engineName, { engine, maxRecords, recordCount }] of Object.entries(engineInfos)) {
      // maxRecords of -1 means "any number", so we can skip asking the server.
      // Used for tests.
      if (maxRecords >= 0 && recordCount > maxRecords) {
        log.debug(`Skipping validation for ${engineName} because ` +
                        `the number of records (${recordCount}) is greater ` +
                        `than the maximum allowed (${maxRecords}).`);
        continue;
      }
      let validator = engine.getValidator();
      if (!validator) {
        continue;
      }

      if (!await validator.canValidate()) {
        log.debug(`Skipping validation for ${engineName} because validator.canValidate() is false`);
        continue;
      }

      // Let's do it!
      Services.console.logStringMessage(
        `Sync is about to run a consistency check of ${engine.name}. This may be slow, and ` +
        `can be controlled using the pref "services.sync.${engine.name}.validation.enabled".\n` +
        `If you encounter any problems because of this, please file a bug.`);

      // Make a new flowID just incase we end up starting a repair.
      let flowID = Utils.makeGUID();
      try {
        // XXX - must get the flow ID to either the validator, or directly to
        // telemetry. I guess it's probably OK to always record a flowID even
        // if we don't end up repairing?
        log.info(`Running validator for ${engine.name}`);
        let result = await validator.validate(engine);
        Observers.notify("weave:engine:validate:finish", result, engine.name);
        // And see if we should repair.
        await this._maybeCure(engine, result, flowID);
      } catch (ex) {
        if (Async.isShutdownException(ex)) {
          throw ex;
        }
        log.error(`Failed to run validation on ${engine.name}!`, ex);
        Observers.notify("weave:engine:validate:error", ex, engine.name);
        // Keep validating -- there's no reason to think that a failure for one
        // validator would mean the others will fail.
      }
    }
  },

  async _maybeCure(engine, validationResults, flowID) {
    if (!this._shouldRepair(engine)) {
      log.info(`Skipping repair of ${engine.name} - disabled via preferences`);
      return;
    }

    let requestor = this._getRepairRequestor(engine.name);
    let didStart = false;
    if (requestor) {
      if (requestor.tryServerOnlyRepairs(validationResults)) {
        return; // TODO: It would be nice if we could request a validation to be
                // done on next sync.
      }
      didStart = await requestor.startRepairs(validationResults, flowID);
    }
    log.info(`${didStart ? "did" : "didn't"} start a repair of ${engine.name} with flowID ${flowID}`);
  },

  _shouldRepair(engine) {
    return Svc.Prefs.get(`engine.${engine.name}.repair.enabled`, false);
  },

  // mainly for mocking.
  async _fetchCollectionCounts() {
    let collectionCountsURL = Service.userBaseURL + "info/collection_counts";
    try {
      let infoResp = await Service._fetchInfo(collectionCountsURL);
      if (!infoResp.success) {
        log.error("Can't fetch collection counts: request to info/collection_counts responded with "
                        + infoResp.status);
        return {};
      }
      return infoResp.obj; // might throw because obj is a getter which parses json.
    } catch (ex) {
      if (Async.isShutdownException(ex)) {
        throw ex;
      }
      // Not running validation is totally fine, so we just write an error log and return.
      log.error("Caught error when fetching counts", ex);
      return {};
    }
  },

  get lastRepairAdvance() {
    return Svc.Prefs.get("doctor.lastRepairAdvance", 0);
  },

  set lastRepairAdvance(value) {
    Svc.Prefs.set("doctor.lastRepairAdvance", value);
  },

  // functions used so tests can mock them
  _now() {
    // We use the server time, which is SECONDS
    return AsyncResource.serverTime;
  },

  _getRepairRequestor(name) {
    return getRepairRequestor(name);
  },

  _getAllRepairRequestors() {
    return getAllRepairRequestors();
  }
};
