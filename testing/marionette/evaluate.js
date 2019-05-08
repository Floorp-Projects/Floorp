/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {clearTimeout, setTimeout} = ChromeUtils.import("resource://gre/modules/Timer.jsm");
const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

const {assert} = ChromeUtils.import("chrome://marionette/content/assert.js");
const {
  element,
  WebElement,
} = ChromeUtils.import("chrome://marionette/content/element.js");
const {
  JavaScriptError,
  ScriptTimeoutError,
} = ChromeUtils.import("chrome://marionette/content/error.js");
const {Log} = ChromeUtils.import("chrome://marionette/content/log.js");

XPCOMUtils.defineLazyGetter(this, "log", Log.get);

this.EXPORTED_SYMBOLS = ["evaluate", "sandbox", "Sandboxes"];

const ARGUMENTS = "__webDriverArguments";
const CALLBACK = "__webDriverCallback";
const COMPLETE = "__webDriverComplete";
const DEFAULT_TIMEOUT = 10000; // ms
const FINISH = "finish";

/** @namespace */
this.evaluate = {};

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
 * @param {boolean=} [async=false] async
 *     Indicates if the script should return immediately or wait for
 *     the callback to be invoked before returning.
 * @param {string=} [file="dummy file"] file
 *     File location of the program in the client.
 * @param {number=} [line=0] line
 *     Line number of th eprogram in the client.
 * @param {number=} [timeout=DEFAULT_TIMEOUT] timeout
 *     Duration in milliseconds before interrupting the script.
 *
 * @return {Promise}
 *     A promise that when resolved will give you the return value from
 *     the script.  Note that the return value requires serialisation before
 *     it can be sent to the client.
 *
 * @throws {JavaScriptError}
 *   If an {@link Error} was thrown whilst evaluating the script.
 * @throws {ScriptTimeoutError}
 *   If the script was interrupted due to script timeout.
 */
evaluate.sandbox = function(sb, script, args = [],
    {
      async = false,
      file = "dummy file",
      line = 0,
      timeout = DEFAULT_TIMEOUT,
    } = {}) {
  let unloadHandler;

  // timeout handler
  let scriptTimeoutID, timeoutPromise;
  if (timeout !== null) {
    timeoutPromise = new Promise((resolve, reject) => {
      scriptTimeoutID = setTimeout(() => {
        reject(new ScriptTimeoutError(`Timed out after ${timeout} ms`));
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
        () => reject(new JavaScriptError("Document was unloaded")), sb);
    sb.window.onunload = unloadHandler;

    let promises = [
      Cu.evalInSandbox(src, sb, "1.8", file, line),
      timeoutPromise,
    ];

    // Wait for the immediate result of calling evalInSandbox, or a timeout.
    // Only resolve the promise if the scriptPromise was resolved and is not
    // async, because the latter has to call resolve() itself.
    Promise.race(promises).then(value => {
      if (!async) {
        resolve(value);
      }
    }, err => {
      reject(err);
    });
  });

  // This block is mainly for async scripts, which escape the inner promise
  // when calling resolve() on their own. The timeout promise will be re-used
  // to break out after the initially setup timeout.
  return Promise.race([promise, timeoutPromise]).catch(err => {
    // Only raise valid errors for both the sync and async scripts.
    if (err instanceof ScriptTimeoutError) {
      throw err;
    }
    throw new JavaScriptError(err);
  }).finally(() => {
    clearTimeout(scriptTimeoutID);
    sb.window.removeEventListener("unload", unloadHandler);
  });
};

/**
 * Convert any web elements in arbitrary objects to DOM elements by
 * looking them up in the seen element store.
 *
 * @param {Object} obj
 *     Arbitrary object containing web elements.
 * @param {element.Store=} seenEls
 *     Known element store to look up web elements from.  If undefined,
 *     the web element references are returned instead.
 * @param {WindowProxy=} win
 *     Current browsing context, if `seenEls` is provided.
 *
 * @return {Object}
 *     Same object as provided by `obj` with the web elements
 *     replaced by DOM elements.
 *
 * @throws {NoSuchElementError}
 *     If `seenEls` is given and the web element reference has not
 *     been seen before.
 * @throws {StaleElementReferenceError}
 *     If `seenEls` is given and the element has gone stale, indicating
 *     it is no longer attached to the DOM, or its node document
 *     is no longer the active document.
 */
evaluate.fromJSON = function(obj, seenEls = undefined, win = undefined) {
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
        return obj.map(e => evaluate.fromJSON(e, seenEls, win));

      // web elements
      } else if (WebElement.isReference(obj)) {
        let webEl = WebElement.fromJSON(obj);
        if (seenEls) {
          return seenEls.get(webEl, win);
        }
        return webEl;
      }

      // arbitrary objects
      let rv = {};
      for (let prop in obj) {
        rv[prop] = evaluate.fromJSON(obj[prop], seenEls, win);
      }
      return rv;
  }
};

/**
 * Marshal arbitrary objects to JSON-safe primitives that can be
 * transported over the Marionette protocol.
 *
 * The marshaling rules are as follows:
 *
 * - Primitives are returned as is.
 *
 * - Collections, such as `Array<`, `NodeList`, `HTMLCollection`
 *   et al. are expanded to arrays and then recursed.
 *
 * - Elements that are not known web elements are added to the
 *   `seenEls` element store.  Once known, the elements' associated
 *   web element representation is returned.
 *
 * - Objects with custom JSON representations, i.e. if they have
 *   a callable `toJSON` function, are returned verbatim.  This means
 *   their internal integrity _are not_ checked.  Be careful.
 *
 * -  Other arbitrary objects are first tested for cyclic references
 *    and then recursed into.
 *
 * @param {Object} obj
 *     Object to be marshaled.
 * @param {element.Store} seenEls
 *     Element store to use for lookup of web element references.
 *
 * @return {Object}
 *     Same object as provided by `obj` with the elements
 *     replaced by web elements.
 *
 * @throws {JavaScriptError}
 *     If an object contains cyclic references.
 */
evaluate.toJSON = function(obj, seenEls) {
  const t = Object.prototype.toString.call(obj);

  // null
  if (t == "[object Undefined]" || t == "[object Null]") {
    return null;

  // primitives
  } else if (t == "[object Boolean]" ||
      t == "[object Number]" ||
      t == "[object String]") {
    return obj;

  // Array, NodeList, HTMLCollection, et al.
  } else if (element.isCollection(obj)) {
    assert.acyclic(obj);
    return [...obj].map(el => evaluate.toJSON(el, seenEls));

  // WebElement
  } else if (WebElement.isReference(obj)) {
    return obj;

  // Element (HTMLElement, SVGElement, XULElement, et al.)
  } else if (element.isElement(obj)) {
    return seenEls.add(obj);

  // custom JSON representation
  } else if (typeof obj.toJSON == "function") {
    let unsafeJSON = obj.toJSON();
    return evaluate.toJSON(unsafeJSON, seenEls);
  }

  // arbitrary objects + files
  let rv = {};
  for (let prop in obj) {
    assert.acyclic(obj[prop]);

    try {
      rv[prop] = evaluate.toJSON(obj[prop], seenEls);
    } catch (e) {
      if (e.result == Cr.NS_ERROR_NOT_IMPLEMENTED) {
        log.debug(`Skipping ${prop}: ${e.message}`);
      } else {
        throw e;
      }
    }
  }
  return rv;
};

/**
 * Tests if an arbitrary object is cyclic.
 *
 * Element prototypes are by definition acyclic, even when they
 * contain cyclic references.  This is because `evaluate.toJSON`
 * ensures they are marshaled as web elements.
 *
 * @param {*} value
 *     Object to test for cyclical references.
 *
 * @return {boolean}
 *     True if object is cyclic, false otherwise.
 */
evaluate.isCyclic = function(value, stack = []) {
  let t = Object.prototype.toString.call(value);

  // null
  if (t == "[object Undefined]" || t == "[object Null]") {
    return false;

  // primitives
  } else if (t == "[object Boolean]" ||
      t == "[object Number]" ||
      t == "[object String]") {
    return false;

  // HTMLElement, SVGElement, XULElement, et al.
  } else if (element.isElement(value)) {
    return false;

  // Array, NodeList, HTMLCollection, et al.
  } else if (element.isCollection(value)) {
    if (stack.includes(value)) {
      return true;
    }
    stack.push(value);

    for (let i = 0; i < value.length; i++) {
      if (evaluate.isCyclic(value[i], stack)) {
        return true;
      }
    }

    stack.pop();
    return false;
  }

  // arbitrary objects
  if (stack.includes(value)) {
    return true;
  }
  stack.push(value);

  for (let prop in value) {
    if (evaluate.isCyclic(value[prop], stack)) {
      return true;
    }
  }

  stack.pop();
  return false;
};

/**
 * `Cu.isDeadWrapper` does not return true for a dead sandbox that
 * was assosciated with and extension popup.  This provides a way to
 * still test for a dead object.
 *
 * @param {Object} obj
 *     A potentially dead object.
 * @param {string} prop
 *     Name of a property on the object.
 *
 * @returns {boolean}
 *     True if <var>obj</var> is dead, false otherwise.
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
 * Unlike for {@link Components.utils.cloneInto}, `obj` may contain
 * functions and DOM elements.
 */
sandbox.cloneInto = function(obj, sb) {
  return Cu.cloneInto(obj, sb, {cloneFunctions: true, wrapReflectors: true});
};

/**
 * Augment given sandbox by an adapter that has an `exports` map
 * property, or a normal map, of function names and function references.
 *
 * @param {Sandbox} sb
 *     The sandbox to augment.
 * @param {Object} adapter
 *     Object that holds an `exports` property, or a map, of function
 *     names and function references.
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
 * @param {Window} win
 *     The DOM Window object.
 * @param {nsIPrincipal=} principal
 *     An optional, custom principal to prefer over the Window.  Useful if
 *     you need elevated security permissions.
 *
 * @return {Sandbox}
 *     The created sandbox.
 */
sandbox.create = function(win, principal = null, opts = {}) {
  let p = principal || win;
  opts = Object.assign({
    sameZoneAs: win,
    sandboxPrototype: win,
    wantComponents: true,
    wantXrays: true,
    wantGlobalProperties: ["ChromeUtils"],
  }, opts);
  return new Cu.Sandbox(p, opts);
};

/**
 * Creates a mutable sandbox, where changes to the global scope
 * will have lasting side-effects.
 *
 * @param {Window} win
 *     The DOM Window object.
 *
 * @return {Sandbox}
 *     The created sandbox.
 */
sandbox.createMutable = function(win) {
  let opts = {
    wantComponents: false,
    wantXrays: false,
  };
  // Note: We waive Xrays here to match potentially-accidental old behavior.
  return Cu.waiveXrays(sandbox.create(win, null, opts));
};

sandbox.createSystemPrincipal = function(win) {
  let principal = Cc["@mozilla.org/systemprincipal;1"]
      .createInstance(Ci.nsIPrincipal);
  return sandbox.create(win, principal);
};

sandbox.createSimpleTest = function(win, harness) {
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
   * @param {boolean=} [fresh=false] fresh
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

  /** Clears cache of sandboxes. */
  clear() {
    this.boxes_.clear();
  }
};
