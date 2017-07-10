/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/Timer.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

Cu.import("chrome://marionette/content/element.js");
const {
  error,
  JavaScriptError,
  ScriptTimeoutError,
  WebDriverError,
} = Cu.import("chrome://marionette/content/error.js", {});

const logger = Log.repository.getLogger("Marionette");

this.EXPORTED_SYMBOLS = ["evaluate", "sandbox", "Sandboxes"];

const ARGUMENTS = "__webDriverArguments";
const CALLBACK = "__webDriverCallback";
const COMPLETE = "__webDriverComplete";
const DEFAULT_TIMEOUT = 10000; // ms
const FINISH = "finish";
const MARIONETTE_SCRIPT_FINISHED = "marionetteScriptFinished";
const ELEMENT_KEY = "element";
const W3C_ELEMENT_KEY = "element-6066-11e4-a52e-4f735466cecf";

this.evaluate = {};

/**
 * Evaluate a script in given sandbox.
 *
 * If the option {@code directInject} is not specified, the script will
 * be executed as a function with the {@code args} argument applied.
 *
 * The arguments provided by the {@code args} argument are exposed through
 * the {@code arguments} object available in the script context, and if
 * the script is executed asynchronously with the {@code async}
 * option, an additional last argument that is synonymous to the
 * {@code marionetteScriptFinished} global is appended, and can be
 * accessed through {@code arguments[arguments.length - 1]}.
 *
 * The {@code timeout} option specifies the duration for how long the
 * script should be allowed to run before it is interrupted and aborted.
 * An interrupted script will cause a ScriptTimeoutError to occur.
 *
 * The {@code async} option indicates that the script will not return
 * until the {@code marionetteScriptFinished} global callback is invoked,
 * which is analogous to the last argument of the {@code arguments}
 * object.
 *
 * The option {@code directInject} causes the script to be evaluated
 * without being wrapped in a function and the provided arguments will
 * be disregarded.  This will cause such things as root scope return
 * statements to throw errors because they are not used inside a function.
 *
 * The {@code filename} option is used in error messages to provide
 * information on the origin script file in the local end.
 *
 * The {@code line} option is used in error messages, along with
 * {@code filename}, to provide the line number in the origin script
 * file on the local end.
 *
 * @param {nsISandbox) sb
 *     The sandbox the script will be evaluted in.
 * @param {string} script
 *     The script to evaluate.
 * @param {Array.<?>=} args
 *     A sequence of arguments to call the script with.
 * @param {Object.<string, ?>=} opts
 *     Dictionary of options:
 *
 *       async (boolean)
 *         Indicates if the script should return immediately or wait
 *         for the callback be invoked before returning.
 *       debug (boolean)
 *         Attaches an {@code onerror} event listener.
 *       directInject (boolean)
 *         Evaluates the script without wrapping it in a function.
 *       filename (string)
 *         File location of the program in the client.
 *       line (number)
 *         Line number of the program in the client.
 *       sandboxName (string)
 *         Name of the sandbox.  Elevated system privileges, equivalent
 *         to chrome space, will be given if it is "system".
 *       timeout (boolean)
 *         Duration in milliseconds before interrupting the script.
 *
 * @return {Promise}
 *     A promise that when resolved will give you the return value from
 *     the script.  Note that the return value requires serialisation before
 *     it can be sent to the client.
 *
 * @throws {JavaScriptError}
 *   If an Error was thrown whilst evaluating the script.
 * @throws {ScriptTimeoutError}
 *   If the script was interrupted due to script timeout.
 */
evaluate.sandbox = function(sb, script, args = [], opts = {}) {
  let scriptTimeoutID, timeoutHandler, unloadHandler;

  let promise = new Promise((resolve, reject) => {
    let src = "";
    sb[COMPLETE] = resolve;
    timeoutHandler = () => reject(new ScriptTimeoutError("Timed out"));
    unloadHandler = sandbox.cloneInto(
        () => reject(new JavaScriptError("Document was unloaded")),
        sb);

    // wrap in function
    if (!opts.directInject) {
      if (opts.async) {
        sb[CALLBACK] = sb[COMPLETE];
      }
      sb[ARGUMENTS] = sandbox.cloneInto(args, sb);

      // callback function made private
      // so that introspection is possible
      // on the arguments object
      if (opts.async) {
        sb[CALLBACK] = sb[COMPLETE];
        src += `${ARGUMENTS}.push(rv => ${CALLBACK}(rv));`;
      }

      src += `(function() { ${script} }).apply(null, ${ARGUMENTS})`;

      // marionetteScriptFinished is not WebDriver conformant,
      // hence it is only exposed to immutable sandboxes
      if (opts.sandboxName) {
        sb[MARIONETTE_SCRIPT_FINISHED] = sb[CALLBACK];
      }
    }

    // onerror is not hooked on by default because of the inability to
    // differentiate content errors from chrome errors.
    //
    // see bug 1128760 for more details
    if (opts.debug) {
      sb.window.onerror = (msg, url, line) => {
        let err = new JavaScriptError(`${msg} at ${url}:${line}`);
        reject(err);
      };
    }

    // timeout and unload handlers
    const timeout = opts.timeout || DEFAULT_TIMEOUT;
    scriptTimeoutID = setTimeout(timeoutHandler, timeout);
    sb.window.onunload = unloadHandler;

    const file = opts.filename || "dummy file";
    const line = opts.line || 0;

    let res;
    try {
      res = Cu.evalInSandbox(src, sb, "1.8", file, 0);
    } catch (e) {
      let err = new JavaScriptError(e, {
        fnName: "execute_script",
        file,
        line,
        script,
      });
      reject(err);
    }

    if (!opts.async) {
      resolve(res);
    }
  });

  return promise.then(res => {
    clearTimeout(scriptTimeoutID);
    sb.window.removeEventListener("unload", unloadHandler);
    return res;
  });
};

/**
 * Convert any web elements in arbitrary objects to DOM elements by
 * looking them up in the seen element store.
 *
 * @param {?} obj
 *     Arbitrary object containing web elements.
 * @param {element.Store} seenEls
 *     Element store to use for lookup of web element references.
 * @param {Window} win
 *     Window.
 * @param {ShadowRoot} shadowRoot
 *     Shadow root.
 *
 * @return {?}
 *     Same object as provided by |obj| with the web elements replaced
 *     by DOM elements.
 */
evaluate.fromJSON = function(obj, seenEls, win, shadowRoot = undefined) {
  switch (typeof obj) {
    case "boolean":
    case "number":
    case "string":
    default:
      return obj;

    case "object":
      if (obj === null) {
        return obj;

      // arrays
      } else if (Array.isArray(obj)) {
        return obj.map(e => evaluate.fromJSON(e, seenEls, win, shadowRoot));

      // web elements
      } else if (Object.keys(obj).includes(element.Key) ||
          Object.keys(obj).includes(element.LegacyKey)) {
        /* eslint-disable */
        let uuid = obj[element.Key] || obj[element.LegacyKey];
        let el = seenEls.get(uuid, {frame: win, shadowRoot: shadowRoot});
        /* eslint-enable */
        if (!el) {
          throw new WebDriverError(`Unknown element: ${uuid}`);
        }
        return el;

      }

      // arbitrary objects
      let rv = {};
      for (let prop in obj) {
        rv[prop] = evaluate.fromJSON(obj[prop], seenEls, win, shadowRoot);
      }
      return rv;
  }
};

/**
 * Convert arbitrary objects to JSON-safe primitives that can be
 * transported over the Marionette protocol.
 *
 * Any DOM elements are converted to web elements by looking them up
 * and/or adding them to the element store provided.
 *
 * @param {?} obj
 *     Object to be marshaled.
 * @param {element.Store} seenEls
 *     Element store to use for lookup of web element references.
 *
 * @return {?}
 *     Same object as provided by |obj| with the elements replaced by
 *     web elements.
 */
evaluate.toJSON = function(obj, seenEls) {
  const t = Object.prototype.toString.call(obj);

  // null
  if (t == "[object Undefined]" || t == "[object Null]") {
    return null;

  // literals
  } else if (t == "[object Boolean]" ||
      t == "[object Number]" ||
      t == "[object String]") {
    return obj;

  // Array, NodeList, HTMLCollection, et al.
  } else if (element.isCollection(obj)) {
    return [...obj].map(el => evaluate.toJSON(el, seenEls));

  // HTMLElement
  } else if ("nodeType" in obj && obj.nodeType == obj.ELEMENT_NODE) {
    let uuid = seenEls.add(obj);
    return element.makeWebElement(uuid);

  // custom JSON representation
  } else if (typeof obj["toJSON"] == "function") {
    let unsafeJSON = obj.toJSON();
    return evaluate.toJSON(unsafeJSON, seenEls);
  }

  // arbitrary objects + files
  let rv = {};
  for (let prop in obj) {
    try {
      rv[prop] = evaluate.toJSON(obj[prop], seenEls);
    } catch (e) {
      if (e.result == Cr.NS_ERROR_NOT_IMPLEMENTED) {
        logger.debug(`Skipping ${prop}: ${e.message}`);
      } else {
        throw e;
      }
    }
  }
  return rv;
};

/**
 * Cu.isDeadWrapper does not return true for a dead sandbox that was
 * assosciated with and extension popup. This provides a way to still
 * test for a dead object.
 *
 * @param {?} obj
 *     A potentially dead object.
 * @param {string} prop
 *     Name of a property on the object.
 *
 * @returns {boolean}
 *     True if |obj| is dead, false otherwise.
 */
evaluate.isDead = function(obj, prop) {
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

this.sandbox = {};

/**
 * Provides a safe way to take an object defined in a privileged scope and
 * create a structured clone of it in a less-privileged scope.  It returns
 * a reference to the clone.
 *
 * Unlike for |Components.utils.cloneInto|, |obj| may contain functions
 * and DOM elemnets.
 */
sandbox.cloneInto = function(obj, sb) {
  return Cu.cloneInto(obj, sb, {cloneFunctions: true, wrapReflectors: true});
};

/**
 * Augment given sandbox by an adapter that has an {@code exports}
 * map property, or a normal map, of function names and function
 * references.
 *
 * @param {Sandbox} sb
 *     The sandbox to augment.
 * @param {Object} adapter
 *     Object that holds an {@code exports} property, or a map, of
 *     function names and function references.
 *
 * @return {Sandbox}
 *     The augmented sandbox.
 */
sandbox.augment = function(sb, adapter) {
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
 * @param {Window} window
 *     The DOM Window object.
 * @param {nsIPrincipal=} principal
 *     An optional, custom principal to prefer over the Window.  Useful if
 *     you need elevated security permissions.
 *
 * @return {Sandbox}
 *     The created sandbox.
 */
sandbox.create = function(window, principal = null, opts = {}) {
  let p = principal || window;
  opts = Object.assign({
    sameZoneAs: window,
    sandboxPrototype: window,
    wantComponents: true,
    wantXrays: true,
  }, opts);
  return new Cu.Sandbox(p, opts);
};

/**
 * Creates a mutable sandbox, where changes to the global scope
 * will have lasting side-effects.
 *
 * @param {Window} window
 *     The DOM Window object.
 *
 * @return {Sandbox}
 *     The created sandbox.
 */
sandbox.createMutable = function(window) {
  let opts = {
    wantComponents: false,
    wantXrays: false,
  };
  return sandbox.create(window, null, opts);
};

sandbox.createSystemPrincipal = function(window) {
  let principal = Cc["@mozilla.org/systemprincipal;1"]
      .createInstance(Ci.nsIPrincipal);
  return sandbox.create(window, principal);
};

sandbox.createSimpleTest = function(window, harness) {
  let sb = sandbox.create(window);
  sb = sandbox.augment(sb, harness);
  sb[FINISH] = () => sb[COMPLETE](harness.generate_results());
  return sb;
};

/**
 * Sandbox storage.  When the user requests a sandbox by a specific name,
 * if one exists in the storage this will be used as long as its window
 * reference is still valid.
 */
this.Sandboxes = class {
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
   * @param {boolean} fresh
   *     Remove old sandbox by name first, if it exists.
   *
   * @return {Sandbox}
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

  /**
   * Clears cache of sandboxes.
   */
  clear() {
    this.boxes_.clear();
  }
};
