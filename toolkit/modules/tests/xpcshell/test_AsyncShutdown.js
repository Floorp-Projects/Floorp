let Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/AsyncShutdown.jsm");

Services.prefs.setBoolPref("toolkit.asyncshutdown.testing", true);

/**
 * An asynchronous task that takes several ticks to complete.
 *
 * @param {*=} resolution The value with which the resulting promise will be
 * resolved once the task is complete. This may be a rejected promise,
 * in which case the resulting promise will itself be rejected.
 * @param {object=} outResult An object modified by side-effect during the task.
 * Initially, its field |isFinished| is set to |false|. Once the task is
 * complete, its field |isFinished| is set to |true|.
 *
 * @return {promise} A promise fulfilled once the task is complete
 */
function longRunningAsyncTask(resolution = undefined, outResult = {}) {
  outResult.isFinished = false;
  if (!("countFinished" in outResult)) {
    outResult.countFinished = 0;
  }
  let deferred = Promise.defer();
  do_timeout(100, function() {
    ++outResult.countFinished;
    outResult.isFinished = true;
    deferred.resolve(resolution);
  });
  return deferred.promise;
}

/**
 * Generate a unique notification topic.
 */
function getUniqueTopic() {
  const PREFIX = "testing-phases-";
  return PREFIX + ++getUniqueTopic.counter;
}
getUniqueTopic.counter = 0;

add_task(function test_no_condition() {
  do_print("Testing a phase with no condition");
  let topic = getUniqueTopic();
  AsyncShutdown._getPhase(topic);
  Services.obs.notifyObservers(null, topic, null);
  do_print("Phase with no condition didn't lock");
});

add_task(function test_simple_async() {
  do_print("Testing various combinations of a phase with a single condition");
  for (let arg of [undefined, null, "foo", 100, new Error("BOOM")]) {
    for (let resolution of [arg, Promise.reject(arg)]) {
      for (let success of [false, true]) {
        // Asynchronous phase
        do_print("Asynchronous test with " + arg + ", " + resolution);
        let topic = getUniqueTopic();
        let outParam = { isFinished: false };
        AsyncShutdown._getPhase(topic).addBlocker(
          "Async test",
            function() {
              if (success) {
                return longRunningAsyncTask(resolution, outParam);
              } else {
                throw resolution;
              }
            }
        );
        do_check_false(outParam.isFinished);
        Services.obs.notifyObservers(null, topic, null);
        do_check_eq(outParam.isFinished, success);
      }

      // Synchronous phase - just test that we don't throw/freeze
      do_print("Synchronous test with " + arg + ", " + resolution);
      let topic = getUniqueTopic();
      AsyncShutdown._getPhase(topic).addBlocker(
        "Sync test",
        resolution
      );
      Services.obs.notifyObservers(null, topic, null);
    }
  }
});

add_task(function test_many() {
  do_print("Testing various combinations of a phase with many conditions");
  let topic = getUniqueTopic();
  let phase = AsyncShutdown._getPhase(topic);
  let outParams = [];
  for (let arg of [undefined, null, "foo", 100, new Error("BOOM")]) {
    for (let resolution of [arg, Promise.reject(arg)]) {
      let outParam = { isFinished: false };
      phase.addBlocker(
        "Test",
        () => longRunningAsyncTask(resolution, outParam)
      );
    }
  }
  do_check_true(outParams.every((x) => !x.isFinished));
  Services.obs.notifyObservers(null, topic, null);
  do_check_true(outParams.every((x) => x.isFinished));
});

function get_exn(f) {
  try {
    f();
    return null;
  } catch (ex) {
    return ex;
  }
}

add_task(function test_various_failures() {
  do_print("Ensure that we cannot add a condition for a phase that is already complete");
  let topic = getUniqueTopic();
  let phase = AsyncShutdown._getPhase(topic);
  Services.obs.notifyObservers(null, topic, null);
  let exn = get_exn(() => phase.addBlocker("Test", true));
  do_check_true(!!exn);

  do_print("Ensure that an incomplete blocker causes a TypeError");

  exn = get_exn(() => phase.addBlocker());
  do_check_eq(exn.name, "TypeError");

  exn = get_exn(() => phase.addBlocker(null, true));
  do_check_eq(exn.name, "TypeError");
});

add_task(function() {
  Services.prefs.clearUserPref("toolkit.asyncshutdown.testing");
});

function run_test() {
  run_next_test();
}
