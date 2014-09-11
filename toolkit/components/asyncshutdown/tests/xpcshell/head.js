/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

let Cu = Components.utils;
let Cc = Components.classes;
let Ci = Components.interfaces;
let Cr = Components.results;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/AsyncShutdown.jsm");

let asyncShutdownService = Cc["@mozilla.org/async-shutdown-service;1"].
  getService(Ci.nsIAsyncShutdownService);


Services.prefs.setBoolPref("toolkit.asyncshutdown.testing", true);

/**
 * Utility function used to provide the same API for various sources
 * of async shutdown barriers.
 *
 * @param {string} kind One of
 * - "phase" to test an AsyncShutdown phase;
 * - "barrier" to test an instance of AsyncShutdown.Barrier;
 * - "xpcom-barrier" to test an instance of nsIAsyncShutdownBarrier;
 * - "xpcom-barrier-unwrapped" to test the field `jsclient` of a nsIAsyncShutdownClient.
 *
 * @return An object with the following methods:
 *   - addBlocker() - the same method as AsyncShutdown phases and barrier clients
 *   - wait() - trigger the resolution of the lock
 */
function makeLock(kind) {
  if (kind == "phase") {
    let topic = "test-Phase-" + ++makeLock.counter;
    let phase = AsyncShutdown._getPhase(topic);
    return {
      addBlocker: function(...args) {
        return phase.addBlocker(...args);
      },
      removeBlocker: function(blocker) {
        return phase.removeBlocker(blocker);
      },
      wait: function() {
        Services.obs.notifyObservers(null, topic, null);
        return Promise.resolve();
      }
    };
  } else if (kind == "barrier") {
    let name = "test-Barrier-" + ++makeLock.counter;
    let barrier = new AsyncShutdown.Barrier(name);
    return {
      addBlocker: barrier.client.addBlocker,
      removeBlocker: barrier.client.removeBlocker,
      wait: function() {
        return barrier.wait();
      }
    };
  } else if (kind == "xpcom-barrier") {
    let name = "test-xpcom-Barrier-" + ++makeLock.counter;
    let barrier = asyncShutdownService.makeBarrier(name);
    return {
      addBlocker: function(name, condition, state) {
        if (condition == null) {
          // Slight trick as `null` or `undefined` cannot be used as keys
          // for `xpcomMap`. Note that this has no incidence on the result
          // of the test as the XPCOM interface imposes that the condition
          // is a method, so it cannot be `null`/`undefined`.
          condition = "<this case can't happen with the xpcom interface>";
        }
        let blocker = makeLock.xpcomMap.get(condition);
        if (!blocker) {
          blocker = {
            name: name,
            state: state,
            blockShutdown: function(aBarrierClient) {
              return Task.spawn(function*() {
                try {
                  if (typeof condition == "function") {
                    yield Promise.resolve(condition());
                  } else {
                    yield Promise.resolve(condition);
                  }
                } finally {
                  aBarrierClient.removeBlocker(blocker);
                }
              });
            },
          };
          makeLock.xpcomMap.set(condition, blocker);
        }
        let {fileName, lineNumber, stack} = (new Error());
        return barrier.client.addBlocker(blocker, fileName, lineNumber, stack);
      },
      removeBlocker: function(condition) {
        let blocker = makeLock.xpcomMap.get(condition);
        if (!blocker) {
          return;
        }
        barrier.client.removeBlocker(blocker);
      },
      wait: function() {
        return new Promise(resolve => {
          barrier.wait(resolve);
        });
      }
    };
  } else if ("unwrapped-xpcom-barrier") {
    let name = "unwrapped-xpcom-barrier-" + ++makeLock.counter;
    let barrier = asyncShutdownService.makeBarrier(name);
    let client = barrier.client.jsclient;
    return {
      addBlocker: client.addBlocker,
      removeBlocker: client.removeBlocker,
      wait: function() {
        return new Promise(resolve => {
          barrier.wait(resolve);
        });
      }
    };
  } else {
    throw new TypeError("Unknown kind " + kind);
  }
}
makeLock.counter = 0;
makeLock.xpcomMap = new Map(); // Note: Not a WeakMap as we wish to handle non-gc-able keys (e.g. strings)

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

function get_exn(f) {
  try {
    f();
    return null;
  } catch (ex) {
    return ex;
  }
}

function do_check_exn(exn, constructor) {
  do_check_neq(exn, null);
  if (exn.name == constructor) {
    do_check_eq(exn.constructor.name, constructor);
    return;
  }
  do_print("Wrong error constructor");
  do_print(exn.constructor.name);
  do_print(exn.stack);
  do_check_true(false);
}
