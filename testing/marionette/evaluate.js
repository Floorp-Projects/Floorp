/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
ChromeUtils.import("resource://gre/modules/Timer.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.import("chrome://marionette/content/assert.js");
const {
  element,
  WebElement,
} = ChromeUtils.import("chrome://marionette/content/element.js", {});
const {
  JavaScriptError,
  ScriptTimeoutError,
} = ChromeUtils.import("chrome://marionette/content/error.js", {});
const {Log} = ChromeUtils.import("chrome://marionette/content/log.js", {});

XPCOMUtils.defineLazyGetter(this, "log", Log.get);

this.EXPORTED_SYMBOLS = ["evaluate", "sandbox", "Sandboxes"];

const ARGUMENTS = "__webDriverArguments";
const CALLBACK = "__webDriverCallback";
const COMPLETE = "__webDriverComplete";
const DEFAULT_TIMEOUT = 10000; // ms
const FINISH = "finish";
const MARIONETTE_SCRIPT_FINISHED = "marionetteScriptFinished";

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
 * `marionetteScriptFinished` global is appended, and can be accessed
 * through `arguments[arguments.length - 1]`.
 *
 * The `timeout` option specifies the duration for how long the
 * script should be allowed to run before it is interrupted and aborted.
 * An interrupted script will cause a {@link ScriptTimeoutError} to occur.
 *
 * The `async` option indicates that the script will not return
 * until the `marionetteScriptFinished` global callback is invoked,
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
 * @param {string=} sandboxName
 *     Name of the sandbox.  Elevated system privileges, equivalent to
 *     chrome space, will be given if it is <tt>system</tt>.
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
      sandboxName = null,
      timeout = DEFAULT_TIMEOUT,
    } = {}) {
  let scriptTimeoutID, timeoutHandler, unloadHandler;

  let promise = new Promise((resolve, reject) => {
    let src = "";
    sb[COMPLETE] = resolve;
    timeoutHandler = () => reject(new ScriptTimeoutError(`Timed out after ${timeout} ms`));
    unloadHandler = sandbox.cloneInto(
        () => reject(new JavaScriptError("Document was unloaded")),
        sb);

    if (async) {
      sb[CALLBACK] = sb[COMPLETE];
    }
    sb[ARGUMENTS] = sandbox.cloneInto(args, sb);

    // callback function made private
    // so that introspection is possible
    // on the arguments object
    if (async) {
      sb[CALLBACK] = sb[COMPLETE];
      src += `${ARGUMENTS}.push(rv => ${CALLBACK}(rv));`;
    }

    src += `(function() { ${script} }).apply(null, ${ARGUMENTS})`;

    // marionetteScriptFinished is not WebDriver conformant,
    // hence it is only exposed to immutable sandboxes
    if (sandboxName) {
      sb[MARIONETTE_SCRIPT_FINISHED] = sb[CALLBACK];
    }

    // timeout and unload handlers
    scriptTimeoutID = setTimeout(timeoutHandler, timeout);
    sb.window.onunload = unloadHandler;

    let res;
    try {
      res = Cu.evalInSandbox(src, sb, "1.8", file, line);
    } catch (e) {
      reject(new JavaScriptError(e));
    }

    if (!async) {
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
 * @param {Object} obj
 *     Arbitrary object containing web elements.
 * @param {element.Store=} seenEls
 *     Known element store to look up web elements from.  If undefined,
 *     the web element references are returned instead.
 * @param {WindowProxy=} window
 *     Current browsing context, if <var>seenEls</var> is provided.
 *
 * @return {Object}
 *     Same object as provided by <var>obj</var> with the web elements
 *     replaced by DOM elements.
 *
 * @throws {NoSuchElementError}
 *     If <var>seenEls</var> is given and the web element reference
 *     has not been seen before.
 * @throws {StaleElementReferenceError}
 *     If <var>seenEls</var> is given and the element has gone stale,
 *     indicating it is no longer attached to the DOM, or its node
 *     document is no longer the active document.
 */
evaluate.fromJSON = function(obj, seenEls = undefined, window = undefined) {
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
        return obj.map(e => evaluate.fromJSON(e, seenEls, window));

      // web elements
      } else if (WebElement.isReference(obj)) {
        let webEl = WebElement.fromJSON(obj);
        if (seenEls) {
          return seenEls.get(webEl, window);
        }
        return webEl;
      }

      // arbitrary objects
      let rv = {};
      for (let prop in obj) {
        rv[prop] = evaluate.fromJSON(obj[prop], seenEls, window);
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
 * <ul>
 *
 * <li>
 * Primitives are returned as is.
 *
 * <li>
 * Collections, such as <code>Array</code>, <code>NodeList</code>,
 * <code>HTMLCollection</code> et al. are expanded to arrays and
 * then recursed.
 *
 * <li>
 * Elements that are not known web elements are added to the
 * <var>seenEls</var> element store.  Once known, the elements'
 * associated web element representation is returned.
 *
 * <li>
 * Objects with custom JSON representations, i.e. if they have a
 * callable <code>toJSON</code> function, are returned verbatim.
 * This means their internal integrity <em>are not</em> checked.
 * Be careful.
 *
 * <li>
 * Other arbitrary objects are first tested for cyclic references
 * and then recursed into.
 *
 * </ul>
 *
 * @param {Object} obj
 *     Object to be marshaled.
 * @param {element.Store} seenEls
 *     Element store to use for lookup of web element references.
 *
 * @return {Object}
 *     Same object as provided by <var>obj</var> with the elements
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
    let webEl = seenEls.add(obj);
    return webEl.toJSON();

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
 * Cu.isDeadWrapper does not return true for a dead sandbox that was
 * assosciated with and extension popup. This provides a way to still
 * test for a dead object.
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
 * Unlike for {@link Components.utils.cloneInto}, <var>obj</var> may
 * contain functions and DOM elemnets.
 */
sandbox.cloneInto = function(obj, sb) {
  return Cu.cloneInto(obj, sb, {cloneFunctions: true, wrapReflectors: true});
};

/**
 * Augment given sandbox by an adapter that has an <code>exports</code>
 * map property, or a normal map, of function names and function
 * references.
 *
 * @param {Sandbox} sb
 *     The sandbox to augment.
 * @param {Object} adapter
 *     Object that holds an <code>exports</code> property, or a map, of
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
