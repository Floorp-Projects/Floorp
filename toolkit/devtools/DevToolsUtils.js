/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* General utilities used throughout devtools. */

let { Promise: promise } = Components.utils.import("resource://gre/modules/commonjs/sdk/core/promise.js", {});
let { Services } = Components.utils.import("resource://gre/modules/Services.jsm", {});

/**
 * Turn the error |aError| into a string, without fail.
 */
this.safeErrorString = function safeErrorString(aError) {
  try {
    let errorString = aError.toString();
    if (typeof errorString === "string") {
      // Attempt to attach a stack to |errorString|. If it throws an error, or
      // isn't a string, don't use it.
      try {
        if (aError.stack) {
          let stack = aError.stack.toString();
          if (typeof stack === "string") {
            errorString += "\nStack: " + stack;
          }
        }
      } catch (ee) { }

      return errorString;
    }
  } catch (ee) { }

  return "<failed trying to find error description>";
}

/**
 * Report that |aWho| threw an exception, |aException|.
 */
this.reportException = function reportException(aWho, aException) {
  let msg = aWho + " threw an exception: " + safeErrorString(aException);

  dump(msg + "\n");

  if (Components.utils.reportError) {
    /*
     * Note that the xpcshell test harness registers an observer for
     * console messages, so when we're running tests, this will cause
     * the test to quit.
     */
    Components.utils.reportError(msg);
  }
}

/**
 * Given a handler function that may throw, return an infallible handler
 * function that calls the fallible handler, and logs any exceptions it
 * throws.
 *
 * @param aHandler function
 *      A handler function, which may throw.
 * @param aName string
 *      A name for aHandler, for use in error messages. If omitted, we use
 *      aHandler.name.
 *
 * (SpiderMonkey does generate good names for anonymous functions, but we
 * don't have a way to get at them from JavaScript at the moment.)
 */
this.makeInfallible = function makeInfallible(aHandler, aName) {
  if (!aName)
    aName = aHandler.name;

  return function (/* arguments */) {
    try {
      return aHandler.apply(this, arguments);
    } catch (ex) {
      let who = "Handler function";
      if (aName) {
        who += " " + aName;
      }
      reportException(who, ex);
    }
  }
}

const executeSoon = aFn => {
  Services.tm.mainThread.dispatch({
    run: this.makeInfallible(aFn)
  }, Components.interfaces.nsIThread.DISPATCH_NORMAL);
}

/**
 * Like Array.prototype.forEach, but doesn't cause jankiness when iterating over
 * very large arrays by yielding to the browser and continuing execution on the
 * next tick.
 *
 * @param Array aArray
 *        The array being iterated over.
 * @param Function aFn
 *        The function called on each item in the array.
 * @returns Promise
 *          A promise that is resolved once the whole array has been iterated
 *          over.
 */
this.yieldingEach = function yieldingEach(aArray, aFn) {
  const deferred = promise.defer();

  let i = 0;
  let len = aArray.length;

  (function loop() {
    const start = Date.now();

    while (i < len) {
      // Don't block the main thread for longer than 16 ms at a time. To
      // maintain 60fps, you have to render every frame in at least 16ms; we
      // aren't including time spent in non-JS here, but this is Good
      // Enough(tm).
      if (Date.now() - start > 16) {
        executeSoon(loop);
        return;
      }

      try {
        aFn(aArray[i++]);
      } catch (e) {
        deferred.reject(e);
        return;
      }
    }

    deferred.resolve();
  }());

  return deferred.promise;
}
