/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

var Cu = Components.utils;
var Cc = Components.classes;
var Ci = Components.interfaces;
var Cr = Components.results;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/AsyncShutdown.jsm");

var asyncShutdownService = Cc["@mozilla.org/async-shutdown-service;1"].
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
      addBlocker(...args) {
        return phase.addBlocker(...args);
      },
      removeBlocker(blocker) {
        return phase.removeBlocker(blocker);
      },
      wait() {
        Services.obs.notifyObservers(null, topic);
        return Promise.resolve();
      }
    };
  } else if (kind == "barrier") {
    let name = "test-Barrier-" + ++makeLock.counter;
    let barrier = new AsyncShutdown.Barrier(name);
    return {
      addBlocker: barrier.client.addBlocker,
      removeBlocker: barrier.client.removeBlocker,
      wait() {
        return barrier.wait();
      }
    };
  } else if (kind == "xpcom-barrier") {
    let name = "test-xpcom-Barrier-" + ++makeLock.counter;
    let barrier = asyncShutdownService.makeBarrier(name);
    return {
      addBlocker(blockerName, condition, state) {
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
            name: blockerName,
            state,
            blockShutdown(aBarrierClient) {
              return (async function() {
                try {
                  if (typeof condition == "function") {
                    await Promise.resolve(condition());
                  } else {
                    await Promise.resolve(condition);
                  }
                } finally {
                  aBarrierClient.removeBlocker(blocker);
                }
              })();
            },
          };
          makeLock.xpcomMap.set(condition, blocker);
        }
        let {fileName, lineNumber, stack} = (new Error());
        return barrier.client.addBlocker(blocker, fileName, lineNumber, stack);
      },
      removeBlocker(condition) {
        let blocker = makeLock.xpcomMap.get(condition);
        if (!blocker) {
          return;
        }
        barrier.client.removeBlocker(blocker);
      },
      wait() {
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
      wait() {
        return new Promise(resolve => {
          barrier.wait(resolve);
        });
      }
    };
  }
  throw new TypeError("Unknown kind " + kind);
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
  return new Promise(resolve => {
    do_timeout(100, function() {
      ++outResult.countFinished;
      outResult.isFinished = true;
      resolve(resolution);
    });
  });
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
  Assert.notEqual(exn, null);
  if (exn.name == constructor) {
    Assert.equal(exn.constructor.name, constructor);
    return;
  }
  info("Wrong error constructor");
  info(exn.constructor.name);
  info(exn.stack);
  Assert.ok(false);
}
