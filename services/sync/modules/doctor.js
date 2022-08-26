/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// A doctor for our collections. She can be asked to make a consultation, and
// may just diagnose an issue without attempting to cure it, may diagnose and
// attempt to cure, or may decide she is overworked and underpaid.
// Or something - naming is hard :)

"use strict";

var EXPORTED_SYMBOLS = ["Doctor"];

const { Log } = ChromeUtils.import("resource://gre/modules/Log.jsm");
const { Async } = ChromeUtils.import("resource://services-common/async.js");
const { Observers } = ChromeUtils.import(
  "resource://services-common/observers.js"
);
const { Service } = ChromeUtils.import("resource://services-sync/service.js");
const { Resource } = ChromeUtils.import("resource://services-sync/resource.js");

const { Svc, Utils } = ChromeUtils.import("resource://services-sync/util.js");

const log = Log.repository.getLogger("Sync.Doctor");

var Doctor = {
  async consult(recentlySyncedEngines) {
    if (!Services.telemetry.canRecordBase) {
      log.info("Skipping consultation: telemetry reporting is disabled");
      return;
    }

    let engineInfos = this._getEnginesToValidate(recentlySyncedEngines);

    await this._runValidators(engineInfos);
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
      let validationInterval = Svc.Prefs.get(
        prefPrefix + "validation.interval"
      );
      let nowSeconds = this._now();

      if (nowSeconds - lastValidation < validationInterval) {
        log.info(
          `Skipping validation of ${e.name}: too recent since last validation attempt`
        );
        continue;
      }
      // Update the time now, even if we decline to actually perform a
      // validation. We don't want to check the rest of these more frequently
      // than once a day.
      Svc.Prefs.set(prefPrefix + "validation.lastTime", Math.floor(nowSeconds));

      // Validation only occurs a certain percentage of the time.
      let validationProbability =
        Svc.Prefs.get(prefPrefix + "validation.percentageChance", 0) / 100.0;
      if (validationProbability < Math.random()) {
        log.info(
          `Skipping validation of ${e.name}: Probability threshold not met`
        );
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
    if (!Object.keys(engineInfos).length) {
      log.info("Skipping validation: no engines qualify");
      return;
    }

    if (Object.values(engineInfos).filter(i => i.maxRecords != -1).length) {
      // at least some of the engines have maxRecord restrictions which require
      // us to ask the server for the counts.
      let countInfo = await this._fetchCollectionCounts();
      for (let [engineName, recordCount] of Object.entries(countInfo)) {
        if (engineName in engineInfos) {
          engineInfos[engineName].recordCount = recordCount;
        }
      }
    }

    for (let [
      engineName,
      { engine, maxRecords, recordCount },
    ] of Object.entries(engineInfos)) {
      // maxRecords of -1 means "any number", so we can skip asking the server.
      // Used for tests.
      if (maxRecords >= 0 && recordCount > maxRecords) {
        log.debug(
          `Skipping validation for ${engineName} because ` +
            `the number of records (${recordCount}) is greater ` +
            `than the maximum allowed (${maxRecords}).`
        );
        continue;
      }
      let validator = engine.getValidator();
      if (!validator) {
        // This is probably only possible in profile downgrade cases.
        log.warn(
          `engine.getValidator returned null for ${engineName} but the pref that controls validation is enabled.`
        );
        continue;
      }

      if (!(await validator.canValidate())) {
        log.debug(
          `Skipping validation for ${engineName} because validator.canValidate() is false`
        );
        continue;
      }

      // Let's do it!
      Services.console.logStringMessage(
        `Sync is about to run a consistency check of ${engine.name}. This may be slow, and ` +
          `can be controlled using the pref "services.sync.${engine.name}.validation.enabled".\n` +
          `If you encounter any problems because of this, please file a bug.`
      );

      try {
        log.info(`Running validator for ${engine.name}`);
        let result = await validator.validate(engine);
        let { problems, version, duration, recordCount } = result;
        Observers.notify(
          "weave:engine:validate:finish",
          {
            version,
            checked: recordCount,
            took: duration,
            problems: problems ? problems.getSummary(true) : null,
          },
          engine.name
        );
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

  // mainly for mocking.
  async _fetchCollectionCounts() {
    let collectionCountsURL = Service.userBaseURL + "info/collection_counts";
    try {
      let infoResp = await Service._fetchInfo(collectionCountsURL);
      if (!infoResp.success) {
        log.error(
          "Can't fetch collection counts: request to info/collection_counts responded with " +
            infoResp.status
        );
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

  // functions used so tests can mock them
  _now() {
    // We use the server time, which is SECONDS
    return Resource.serverTime;
  },
};
