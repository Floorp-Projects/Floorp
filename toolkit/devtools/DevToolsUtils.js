/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* General utilities used throughout devtools. */

// hasChrome is provided as a global by the loader. It is true if we are running
// on the main thread, and false if we are running on a worker thread.
var { Ci, Cu } = require("chrome");
var Services = require("Services");
var { setTimeout } = require("Timer");

/**
 * Turn the error |aError| into a string, without fail.
 */
exports.safeErrorString = function safeErrorString(aError) {
  try {
    let errorString = aError.toString();
    if (typeof errorString == "string") {
      // Attempt to attach a stack to |errorString|. If it throws an error, or
      // isn't a string, don't use it.
      try {
        if (aError.stack) {
          let stack = aError.stack.toString();
          if (typeof stack == "string") {
            errorString += "\nStack: " + stack;
          }
        }
      } catch (ee) { }

      // Append additional line and column number information to the output,
      // since it might not be part of the stringified error.
      if (typeof aError.lineNumber == "number" && typeof aError.columnNumber == "number") {
        errorString += "Line: " + aError.lineNumber + ", column: " + aError.columnNumber;
      }

      return errorString;
    }
  } catch (ee) { }

  return "<failed trying to find error description>";
}

/**
 * Report that |aWho| threw an exception, |aException|.
 */
exports.reportException = function reportException(aWho, aException) {
  let msg = aWho + " threw an exception: " + exports.safeErrorString(aException);

  dump(msg + "\n");

  if (Cu.reportError) {
    /*
     * Note that the xpcshell test harness registers an observer for
     * console messages, so when we're running tests, this will cause
     * the test to quit.
     */
    Cu.reportError(msg);
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
exports.makeInfallible = function makeInfallible(aHandler, aName) {
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
      exports.reportException(who, ex);
    }
  }
}

/**
 * Interleaves two arrays element by element, returning the combined array, like
 * a zip. In the case of arrays with different sizes, undefined values will be
 * interleaved at the end along with the extra values of the larger array.
 *
 * @param Array a
 * @param Array b
 * @returns Array
 *          The combined array, in the form [a1, b1, a2, b2, ...]
 */
exports.zip = function zip(a, b) {
  if (!b) {
    return a;
  }
  if (!a) {
    return b;
  }
  const pairs = [];
  for (let i = 0, aLength = a.length, bLength = b.length;
       i < aLength || i < bLength;
       i++) {
    pairs.push([a[i], b[i]]);
  }
  return pairs;
};

/**
 * Waits for the next tick in the event loop to execute a callback.
 */
exports.executeSoon = function executeSoon(aFn) {
  Services.tm.mainThread.dispatch({
    run: exports.makeInfallible(aFn)
  }, Ci.nsIThread.DISPATCH_NORMAL);
};

/**
 * Waits for the next tick in the event loop.
 *
 * @return Promise
 *         A promise that is resolved after the next tick in the event loop.
 */
exports.waitForTick = function waitForTick() {
  let deferred = promise.defer();
  exports.executeSoon(deferred.resolve);
  return deferred.promise;
};

/**
 * Waits for the specified amount of time to pass.
 *
 * @param number aDelay
 *        The amount of time to wait, in milliseconds.
 * @return Promise
 *         A promise that is resolved after the specified amount of time passes.
 */
exports.waitForTime = function waitForTime(aDelay) {
  let deferred = promise.defer();
  setTimeout(deferred.resolve, aDelay);
  return deferred.promise;
};

/**
 * Like Array.prototype.forEach, but doesn't cause jankiness when iterating over
 * very large arrays by yielding to the browser and continuing execution on the
 * next tick.
 *
 * @param Array aArray
 *        The array being iterated over.
 * @param Function aFn
 *        The function called on each item in the array. If a promise is
 *        returned by this function, iterating over the array will be paused
 *        until the respective promise is resolved.
 * @returns Promise
 *          A promise that is resolved once the whole array has been iterated
 *          over, and all promises returned by the aFn callback are resolved.
 */
exports.yieldingEach = function yieldingEach(aArray, aFn) {
  const deferred = promise.defer();

  let i = 0;
  let len = aArray.length;
  let outstanding = [deferred.promise];

  (function loop() {
    const start = Date.now();

    while (i < len) {
      // Don't block the main thread for longer than 16 ms at a time. To
      // maintain 60fps, you have to render every frame in at least 16ms; we
      // aren't including time spent in non-JS here, but this is Good
      // Enough(tm).
      if (Date.now() - start > 16) {
        exports.executeSoon(loop);
        return;
      }

      try {
        outstanding.push(aFn(aArray[i], i++));
      } catch (e) {
        deferred.reject(e);
        return;
      }
    }

    deferred.resolve();
  }());

  return promise.all(outstanding);
}

/**
 * Like XPCOMUtils.defineLazyGetter, but with a |this| sensitive getter that
 * allows the lazy getter to be defined on a prototype and work correctly with
 * instances.
 *
 * @param Object aObject
 *        The prototype object to define the lazy getter on.
 * @param String aKey
 *        The key to define the lazy getter on.
 * @param Function aCallback
 *        The callback that will be called to determine the value. Will be
 *        called with the |this| value of the current instance.
 */
exports.defineLazyPrototypeGetter =
function defineLazyPrototypeGetter(aObject, aKey, aCallback) {
  Object.defineProperty(aObject, aKey, {
    configurable: true,
    get: function() {
      const value = aCallback.call(this);

      Object.defineProperty(this, aKey, {
        configurable: true,
        writable: true,
        value: value
      });

      return value;
    }
  });
}

/**
 * Safely get the property value from a Debugger.Object for a given key. Walks
 * the prototype chain until the property is found.
 *
 * @param Debugger.Object aObject
 *        The Debugger.Object to get the value from.
 * @param String aKey
 *        The key to look for.
 * @return Any
 */
exports.getProperty = function getProperty(aObj, aKey) {
  let root = aObj;
  try {
    do {
      const desc = aObj.getOwnPropertyDescriptor(aKey);
      if (desc) {
        if ("value" in desc) {
          return desc.value;
        }
        // Call the getter if it's safe.
        return exports.hasSafeGetter(desc) ? desc.get.call(root).return : undefined;
      }
      aObj = aObj.proto;
    } while (aObj);
  } catch (e) {
    // If anything goes wrong report the error and return undefined.
    exports.reportException("getProperty", e);
  }
  return undefined;
};

/**
 * Determines if a descriptor has a getter which doesn't call into JavaScript.
 *
 * @param Object aDesc
 *        The descriptor to check for a safe getter.
 * @return Boolean
 *         Whether a safe getter was found.
 */
exports.hasSafeGetter = function hasSafeGetter(aDesc) {
  let fn = aDesc.get;
  return fn && fn.callable && fn.class == "Function" && fn.script === undefined;
};

/**
 * Check if it is safe to read properties and execute methods from the given JS
 * object. Safety is defined as being protected from unintended code execution
 * from content scripts (or cross-compartment code).
 *
 * See bugs 945920 and 946752 for discussion.
 *
 * @type Object aObj
 *       The object to check.
 * @return Boolean
 *         True if it is safe to read properties from aObj, or false otherwise.
 */
exports.isSafeJSObject = function isSafeJSObject(aObj) {
  if (Cu.getGlobalForObject(aObj) ==
      Cu.getGlobalForObject(exports.isSafeJSObject)) {
    return true; // aObj is not a cross-compartment wrapper.
  }

  let principal = Cu.getObjectPrincipal(aObj);
  if (Services.scriptSecurityManager.isSystemPrincipal(principal)) {
    return true; // allow chrome objects
  }

  return Cu.isXrayWrapper(aObj);
};

exports.dumpn = function dumpn(str) {
  if (exports.dumpn.wantLogging) {
    dump("DBG-SERVER: " + str + "\n");
  }
}

// We want wantLogging to be writable. The exports object is frozen by the
// loader, so define it on dumpn instead.
exports.dumpn.wantLogging = false;

/**
 * A verbose logger for low-level tracing.
 */
exports.dumpv = function(msg) {
  if (exports.dumpv.wantVerbose) {
    exports.dumpn(msg);
  }
};

// We want wantLogging to be writable. The exports object is frozen by the
// loader, so define it on dumpn instead.
exports.dumpv.wantVerbose = false;

exports.dbg_assert = function dbg_assert(cond, e) {
  if (!cond) {
    return e;
  }
}


/**
 * Utility function for updating an object with the properties of another
 * object.
 *
 * @param aTarget Object
 *        The object being updated.
 * @param aNewAttrs Object
 *        The new attributes being set on the target.
 */
exports.update = function update(aTarget, aNewAttrs) {
  for (let key in aNewAttrs) {
    let desc = Object.getOwnPropertyDescriptor(aNewAttrs, key);

    if (desc) {
      Object.defineProperty(aTarget, key, desc);
    }
  }
}

/**
 * Defines a getter on a specified object that will be created upon first use.
 *
 * @param aObject
 *        The object to define the lazy getter on.
 * @param aName
 *        The name of the getter to define on aObject.
 * @param aLambda
 *        A function that returns what the getter should return.  This will
 *        only ever be called once.
 */
exports.defineLazyGetter = function defineLazyGetter(aObject, aName, aLambda) {
  Object.defineProperty(aObject, aName, {
    get: function () {
      delete aObject[aName];
      return aObject[aName] = aLambda.apply(aObject);
    },
    configurable: true,
    enumerable: true
  });
};

/**
 * Defines a getter on a specified object for a module.  The module will not
 * be imported until first use.
 *
 * @param aObject
 *        The object to define the lazy getter on.
 * @param aName
 *        The name of the getter to define on aObject for the module.
 * @param aResource
 *        The URL used to obtain the module.
 * @param aSymbol
 *        The name of the symbol exported by the module.
 *        This parameter is optional and defaults to aName.
 */
exports.defineLazyModuleGetter = function defineLazyModuleGetter(aObject, aName,
                                                                 aResource,
                                                                 aSymbol)
{
  this.defineLazyGetter(aObject, aName, function XPCU_moduleLambda() {
    var temp = {};
    Cu.import(aResource, temp);
    return temp[aSymbol || aName];
  });
};
