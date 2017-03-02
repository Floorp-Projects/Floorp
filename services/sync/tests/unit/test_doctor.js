/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { Doctor, REPAIR_ADVANCE_PERIOD } = Cu.import("resource://services-sync/doctor.js", {});

initTestLogging("Trace");

function mockDoctor(mocks) {
  // Clone the object and put mocks in that.
  return Object.assign({}, Doctor, mocks);
}

add_task(async function test_repairs_start() {
  let repairStarted = false;
  let problems = {
    missingChildren: ["a", "b", "c"],
  }
  let validator = {
    validate(engine) {
      return problems;
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
