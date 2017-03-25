/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { Doctor, REPAIR_ADVANCE_PERIOD } = Cu.import("resource://services-sync/doctor.js", {});
Cu.import("resource://gre/modules/Services.jsm");

initTestLogging("Trace");

function mockDoctor(mocks) {
  // Clone the object and put mocks in that.
  return Object.assign({}, Doctor, mocks);
}

add_task(async function test_validation_interval() {
  let now = 1000;
  let doctor = mockDoctor({
    _now() {
      // note that the function being mocked actually returns seconds.
      return now;
    },
  });

  let engine = {
    name: "test-engine",
    getValidator() {
      return {
        validate(e) {
          return {};
        }
      }
    },
  }

  // setup prefs which enable test-engine validation.
  Services.prefs.setBoolPref("services.sync.engine.test-engine.validation.enabled", true);
  Services.prefs.setIntPref("services.sync.engine.test-engine.validation.percentageChance", 100);
  Services.prefs.setIntPref("services.sync.engine.test-engine.validation.maxRecords", 1);
  // And say we should validate every 10 seconds.
  Services.prefs.setIntPref("services.sync.engine.test-engine.validation.interval", 10);

  deepEqual(doctor._getEnginesToValidate([engine]), {
    "test-engine": {
      engine,
      maxRecords: 1,
    }
  });
  // We haven't advanced the timestamp, so we should not validate again.
  deepEqual(doctor._getEnginesToValidate([engine]), {});
  // Advance our clock by 11 seconds.
  now += 11;
  // We should validate again.
  deepEqual(doctor._getEnginesToValidate([engine]), {
    "test-engine": {
      engine,
      maxRecords: 1,
    }
  });
});

add_task(async function test_repairs_start() {
  let repairStarted = false;
  let problems = {
    missingChildren: ["a", "b", "c"],
  }
  let validator = {
    validate(engine) {
      return problems;
    },
    canValidate() {
      return Promise.resolve(true);
    }
  }
  let engine = {
    name: "test-engine",
    getValidator() {
      return validator;
    }
  }
  let requestor = {
    startRepairs(validationInfo, flowID) {
      ok(flowID, "got a flow ID");
      equal(validationInfo, problems);
      repairStarted = true;
      return true;
    },
    tryServerOnlyRepairs() {
      return false;
    }
  }
  let doctor = mockDoctor({
    _getEnginesToValidate(recentlySyncedEngines) {
      deepEqual(recentlySyncedEngines, [engine]);
      return {
        "test-engine": { engine, maxRecords: -1 }
      };
    },
    _getRepairRequestor(engineName) {
      equal(engineName, engine.name);
      return requestor;
    },
    _shouldRepair(e) {
      return true;
    },
  });
  let promiseValidationDone = promiseOneObserver("weave:engine:validate:finish");
  await doctor.consult([engine]);
  await promiseValidationDone;
  ok(repairStarted);
});

add_task(async function test_repairs_advanced_daily() {
  let repairCalls = 0;
  let requestor = {
    continueRepairs() {
      repairCalls++;
    },
    tryServerOnlyRepairs() {
      return false;
    }
  }
  // start now at just after REPAIR_ADVANCE_PERIOD so we do a a first one.
  let now = REPAIR_ADVANCE_PERIOD + 1;
  let doctor = mockDoctor({
    _getEnginesToValidate() {
      return {}; // no validations.
    },
    _runValidators() {
      // do nothing.
    },
    _getAllRepairRequestors() {
      return {
        foo: requestor,
      }
    },
    _now() {
      return now;
    },
  });
  await doctor.consult();
  equal(repairCalls, 1);
  now += 10; // 10 seconds later...
  await doctor.consult();
  // should not have done another repair yet - it's too soon.
  equal(repairCalls, 1);
  // advance our pretend clock by the advance period (eg, day)
  now += REPAIR_ADVANCE_PERIOD;
  await doctor.consult();
  // should have done another repair
  equal(repairCalls, 2);
});

add_task(async function test_repairs_skip_if_cant_vaidate() {
  let validator = {
    canValidate() {
      return Promise.resolve(false);
    },
    validate() {
      ok(false, "Shouldn't validate");
    }
  }
  let engine = {
    name: "test-engine",
    getValidator() {
      return validator;
    }
  }
  let requestor = {
    startRepairs(validationInfo, flowID) {
      assert.ok(false, "Never should start repairs");
    },
    tryServerOnlyRepairs() {
      return false;
    }
  }
  let doctor = mockDoctor({
    _getEnginesToValidate(recentlySyncedEngines) {
      deepEqual(recentlySyncedEngines, [engine]);
      return {
        "test-engine": { engine, maxRecords: -1 }
      };
    },
    _getRepairRequestor(engineName) {
      equal(engineName, engine.name);
      return requestor;
    },
  });
  await doctor.consult([engine]);
});
