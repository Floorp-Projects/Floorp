/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { clearTimeout, setTimeout } from "resource://gre/modules/Timer.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
});

const ARGUMENTS = "__webDriverArguments";
const CALLBACK = "__webDriverCallback";
const COMPLETE = "__webDriverComplete";
const DEFAULT_TIMEOUT = 10000; // ms
const FINISH = "finish";

/** @namespace */
export const evaluate = {};

/**
 * Evaluate a script in given sandbox.
 *
 * The the provided `script` will be wrapped in an anonymous function
 * with the `args` argument applied.
 *
 * The arguments provided by the `args<` argument are exposed
 * through the `arguments` object available in the script context,
 * and if the script is executed asynchronously with the `async`
 * option, an additional last argument that is synonymous to the
 * name `resolve` is appended, and can be accessed
 * through `arguments[arguments.length - 1]`.
 *
 * The `timeout` option specifies the duration for how long the
 * script should be allowed to run before it is interrupted and aborted.
 * An interrupted script will cause a {@link ScriptTimeoutError} to occur.
 *
 * The `async` option indicates that the script will not return
 * until the `resolve` callback is invoked,
 * which is analogous to the last argument of the `arguments` object.
 *
 * The `file` option is used in error messages to provide information
 * on the origin script file in the local end.
 *
 * The `line` option is used in error messages, along with `filename`,
 * to provide the line number in the origin script file on the local end.
 *
 * @param {nsISandbox} sb
 *     Sandbox the script will be evaluted in.
 * @param {string} script
 *     Script to evaluate.
 * @param {Array.<?>=} args
 *     A sequence of arguments to call the script with.
 * @param {object=} options
 * @param {boolean=} options.async
 *     Indicates if the script should return immediately or wait for
 *     the callback to be invoked before returning. Defaults to false.
 * @param {string=} options.file
 *     File location of the program in the client. Defaults to "dummy file".
 * @param {number=} options.line
 *     Line number of the program in the client. Defaults to 0.
 * @param {number=} options.timeout
 *     Duration in milliseconds before interrupting the script. Defaults to
 *     DEFAULT_TIMEOUT.
 *
 * @returns {Promise}
 *     A promise that when resolved will give you the return value from
 *     the script.  Note that the return value requires serialisation before
 *     it can be sent to the client.
 *
 * @throws {JavaScriptError}
 *   If an {@link Error} was thrown whilst evaluating the script.
 * @throws {ScriptTimeoutError}
 *   If the script was interrupted due to script timeout.
 */
evaluate.sandbox = function (
  sb,
  script,
  args = [],
  {
    async = false,
    file = "dummy file",
    line = 0,
    timeout = DEFAULT_TIMEOUT,
  } = {}
) {
  let unloadHandler;
  let marionetteSandbox = sandbox.create(sb.window);

  // timeout handler
  let scriptTimeoutID, timeoutPromise;
  if (timeout !== null) {
    timeoutPromise = new Promise((resolve, reject) => {
      scriptTimeoutID = setTimeout(() => {
        reject(
          new lazy.error.ScriptTimeoutError(`Timed out after ${timeout} ms`)
        );
      }, timeout);
    });
  }

  let promise = new Promise((resolve, reject) => {
    let src = "";
    sb[COMPLETE] = resolve;
    sb[ARGUMENTS] = sandbox.cloneInto(args, sb);

    // callback function made private
    // so that introspection is possible
    // on the arguments object
    if (async) {
      sb[CALLBACK] = sb[COMPLETE];
      src += `${ARGUMENTS}.push(rv => ${CALLBACK}(rv));`;
    }

    src += `(function() {
      ${script}
    }).apply(null, ${ARGUMENTS})`;

    unloadHandler = sandbox.cloneInto(
      () => reject(new lazy.error.JavaScriptError("Document was unloaded")),
      marionetteSandbox
    );
    marionetteSandbox.window.addEventListener("unload", unloadHandler);

    let promises = [
      Cu.evalInSandbox(
        src,
        sb,
        "1.8",
        file,
        line,
        /* enforceFilenameRestrictions */ false
      ),
      timeoutPromise,
    ];

    // Wait for the immediate result of calling evalInSandbox, or a timeout.
    // Only resolve the promise if the scriptPromise was resolved and is not
    // async, because the latter has to call resolve() itself.
    Promise.race(promises).then(
      value => {
        if (!async) {
          resolve(value);
        }
      },
      err => {
        reject(err);
      }
    );
  });

  // This block is mainly for async scripts, which escape the inner promise
  // when calling resolve() on their own. The timeout promise will be re-used
  // to break out after the initially setup timeout.
  return Promise.race([promise, timeoutPromise])
    .catch(err => {
      // Only raise valid errors for both the sync and async scripts.
      if (err instanceof lazy.error.ScriptTimeoutError) {
        throw err;
      }
      throw new lazy.error.JavaScriptError(err);
    })
    .finally(() => {
      clearTimeout(scriptTimeoutID);
      marionetteSandbox.window.removeEventListener("unload", unloadHandler);
    });
};

/**
 * `Cu.isDeadWrapper` does not return true for a dead sandbox that
 * was assosciated with and extension popup.  This provides a way to
 * still test for a dead object.
 *
 * @param {object} obj
 *     A potentially dead object.
 * @param {string} prop
 *     Name of a property on the object.
 *
 * @returns {boolean}
 *     True if <var>obj</var> is dead, false otherwise.
 */
evaluate.isDead = function (obj, prop) {
  try {
    obj[prop];
  } catch (e) {
    if (e.message.includes("dead object")) {
      return true;
    }
    throw e;
  }
  return false;
};

export const sandbox = {};

/**
 * Provides a safe way to take an object defined in a privileged scope and
 * create a structured clone of it in a less-privileged scope.  It returns
 * a reference to the clone.
 *
 * Unlike for {@link Components.utils.cloneInto}, `obj` may contain
 * functions and DOM elements.
 */
sandbox.cloneInto = function (obj, sb) {
  return Cu.cloneInto(obj, sb, { cloneFunctions: true, wrapReflectors: true });
};

/**
 * Augment given sandbox by an adapter that has an `exports` map
 * property, or a normal map, of function names and function references.
 *
 * @param {Sandbox} sb
 *     The sandbox to augment.
 * @param {object} adapter
 *     Object that holds an `exports` property, or a map, of function
 *     names and function references.
 *
 * @returns {Sandbox}
 *     The augmented sandbox.
 */
sandbox.augment = function (sb, adapter) {
  function* entries(obj) {
    for (let key of Object.keys(obj)) {
      yield [key, obj[key]];
    }
  }

  let funcs = adapter.exports || entries(adapter);
  for (let [name, func] of funcs) {
    sb[name] = func;
  }

  return sb;
};

/**
 * Creates a sandbox.
 *
 * @param {Window} win
 *     The DOM Window object.
 * @param {nsIPrincipal=} principal
 *     An optional, custom principal to prefer over the Window.  Useful if
 *     you need elevated security permissions.
 *
 * @returns {Sandbox}
 *     The created sandbox.
 */
sandbox.create = function (win, principal = null, opts = {}) {
  let p = principal || win;
  opts = Object.assign(
    {
      sameZoneAs: win,
      sandboxPrototype: win,
      wantComponents: true,
      wantXrays: true,
      wantGlobalProperties: ["ChromeUtils"],
    },
    opts
  );
  return new Cu.Sandbox(p, opts);
};

/**
 * Creates a mutable sandbox, where changes to the global scope
 * will have lasting side-effects.
 *
 * @param {Window} win
 *     The DOM Window object.
 *
 * @returns {Sandbox}
 *     The created sandbox.
 */
sandbox.createMutable = function (win) {
  let opts = {
    wantComponents: false,
    wantXrays: false,
  };
  // Note: We waive Xrays here to match potentially-accidental old behavior.
  return Cu.waiveXrays(sandbox.create(win, null, opts));
};

sandbox.createSystemPrincipal = function (win) {
  let principal = Cc["@mozilla.org/systemprincipal;1"].createInstance(
    Ci.nsIPrincipal
  );
  return sandbox.create(win, principal);
};

sandbox.createSimpleTest = function (win, harness) {
  let sb = sandbox.create(win);
  sb = sandbox.augment(sb, harness);
  sb[FINISH] = () => sb[COMPLETE](harness.generate_results());
  return sb;
};

/**
 * Sandbox storage.  When the user requests a sandbox by a specific name,
 * if one exists in the storage this will be used as long as its window
 * reference is still valid.
 *
 * @memberof evaluate
 */
export class Sandboxes {
  /**
   * @param {function(): Window} windowFn
   *     A function that returns the references to the current Window
   *     object.
   */
  constructor(windowFn) {
    this.windowFn_ = windowFn;
    this.boxes_ = new Map();
  }

  get window_() {
    return this.windowFn_();
  }

  /**
   * Factory function for getting a sandbox by name, or failing that,
   * creating a new one.
   *
   * If the sandbox' window does not match the provided window, a new one
   * will be created.
   *
   * @param {string} name
   *     The name of the sandbox to get or create.
   * @param {boolean=} [fresh=false] fresh
   *     Remove old sandbox by name first, if it exists.
   *
   * @returns {Sandbox}
   *     A used or fresh sandbox.
   */
  get(name = "default", fresh = false) {
    let sb = this.boxes_.get(name);
    if (sb) {
      if (fresh || evaluate.isDead(sb, "window") || sb.window != this.window_) {
        this.boxes_.delete(name);
        return this.get(name, false);
      }
    } else {
      if (name == "system") {
        sb = sandbox.createSystemPrincipal(this.window_);
      } else {
        sb = sandbox.create(this.window_);
      }
      this.boxes_.set(name, sb);
    }
    return sb;
  }

  /** Clears cache of sandboxes. */
  clear() {
    this.boxes_.clear();
  }
}
