/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/Timer.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

Cu.import("chrome://marionette/content/error.js");

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
 * @param {string} script
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
 * @throws JavaScriptError
 *   If an Error was thrown whilst evaluating the script.
 * @throws ScriptTimeoutError
 *   If the script was interrupted due to script timeout.
 */
evaluate.sandbox = function (sb, script, args = [], opts = {}) {
  let scriptTimeoutID, timeoutHandler, unloadHandler;

  let promise = new Promise((resolve, reject) => {
    let src = "";
    sb[COMPLETE] = resolve;
    timeoutHandler = () => reject(new ScriptTimeoutError("Timed out"));
    unloadHandler = sandbox.cloneInto(
        () => reject(new JavaScriptError("Document was unloaded during execution")),
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
    scriptTimeoutID = setTimeout(timeoutHandler, opts.timeout || DEFAULT_TIMEOUT);
    sb.window.onunload = unloadHandler;

    let res;
    try {
      res = Cu.evalInSandbox(src, sb, "1.8", opts.filename || "dummy file", 0);
    } catch (e) {
      let err = new JavaScriptError(
          e,
          "execute_script",
          opts.filename,
          opts.line,
          script);
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

this.sandbox = {};

/**
 * Provides a safe way to take an object defined in a privileged scope and
 * create a structured clone of it in a less-privileged scope.  It returns
 * a reference to the clone.
 *
 * Unlike for |Components.utils.cloneInto|, |obj| may contain functions
 * and DOM elemnets.
 */
sandbox.cloneInto = function (obj, sb) {
  return Cu.cloneInto(obj, sb, {cloneFunctions: true, wrapReflectors: true});
};

/**
 * Augment given sandbox by an adapter that has an {@code exports}
 * map property, or a normal map, of function names and function
 * references.
 *
 * @param {Sandbox} sb
 *     The sandbox to augment.
 * @param {Object} adapter
 *     Object that holds an {@code exports} property, or a map, of
 *     function names and function references.
 *
 * @return {Sandbox}
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
 * @param {Window} window
 *     The DOM Window object.
 * @param {nsIPrincipal=} principal
 *     An optional, custom principal to prefer over the Window.  Useful if
 *     you need elevated security permissions.
 *
 * @return {Sandbox}
 *     The created sandbox.
 */
sandbox.create = function (window, principal = null, opts = {}) {
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
sandbox.createMutable = function (window) {
  let opts = {
    wantComponents: false,
    wantXrays: false,
  };
  return sandbox.create(window, null, opts);
};

sandbox.createSystemPrincipal = function (window) {
  let principal = Cc["@mozilla.org/systemprincipal;1"]
      .createInstance(Ci.nsIPrincipal);
  return sandbox.create(window, principal);
};

sandbox.createSimpleTest = function (window, harness) {
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
      if (fresh || sb.window != this.window_) {
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

/**
 * Stores scripts imported from the local end through the
 * {@code GeckoDriver#importScript} command.
 *
 * Imported scripts are prepended to the script that is evaluated
 * on each call to {@code GeckoDriver#executeScript},
 * {@code GeckoDriver#executeAsyncScript}, and
 * {@code GeckoDriver#executeJSScript}.
 *
 * Usage:
 *
 *     let importedScripts = new evaluate.ScriptStorage();
 *     importedScripts.add(firstScript);
 *     importedScripts.add(secondScript);
 *
 *     let scriptToEval = importedScripts.concat(script);
 *     // firstScript and secondScript are prepended to script
 *
 */
evaluate.ScriptStorage = class extends Set {

  /**
   * Produce a string of all stored scripts.
   *
   * The stored scripts are concatenated into a string, with optional
   * additional scripts then appended.
   *
   * @param {...string} addional
   *     Optional scripts to include.
   *
   * @return {string}
   *     Concatenated string consisting of stored scripts and additional
   *     scripts, in that order.
   */
  concat(...additional) {
    let rv = "";
    for (let s of this) {
      rv = s + rv;
    }
    for (let s of additional) {
      rv = rv + s;
    }
    return rv;
  }

  toJson() {
    return Array.from(this);
  }

};

/**
 * Service that enables the script storage service to be queried from
 * content space.
 *
 * The storage can back multiple |ScriptStorage|, each typically belonging
 * to a |Context|.  Since imported scripts' scope are global and not
 * scoped to the current browsing context, all imported scripts are stored
 * in chrome space and fetched by content space as needed.
 *
 * Usage in chrome space:
 *
 *     let service = new evaluate.ScriptStorageService(
 *         [Context.CHROME, Context.CONTENT]);
 *     let storage = service.for(Context.CHROME);
 *     let scriptToEval = storage.concat(script);
 *
 */
evaluate.ScriptStorageService = class extends Map {

  /**
   * Create the service.
   *
   * An optional array of names for script storages to initially create
   * can be provided.
   *
   * @param {Array.<string>=} initialStorages
   *     List of names of the script storages to create initially.
   */
  constructor(initialStorages = []) {
    super(initialStorages.map(name => [name, new evaluate.ScriptStorage()]));
  }

  /**
   * Retrieve the scripts associated with the given context.
   *
   * @param {Context} context
   *     Context to retrieve the scripts from.
   *
   * @return {ScriptStorage}
   *     Scrips associated with given |context|.
   */
  for(context) {
    return this.get(context);
  }

  processMessage(msg) {
    switch (msg.name) {
      case "Marionette:getImportedScripts":
        let storage = this.for.apply(this, msg.json);
        return storage.toJson();

      default:
        throw new TypeError("Unknown message: " + msg.name);
    }
  }

  // TODO(ato): The idea of services in chrome space
  // can be generalised at some later time (see cookies.js:38).
  receiveMessage(msg) {
    try {
      return this.processMessage(msg);
    } catch (e) {
      logger.error(e);
    }
  }

};

evaluate.ScriptStorageService.prototype.QueryInterface =
    XPCOMUtils.generateQI([
      Ci.nsIMessageListener,
      Ci.nsISupportsWeakReference,
    ]);

/**
 * Bridges the script storage in chrome space, to make it possible to
 * retrieve a {@code ScriptStorage} associated with a given
 * {@code Context} from content space.
 *
 * Usage in content space:
 *
 *     let client = new evaluate.ScriptStorageServiceClient(chromeProxy);
 *     let storage = client.for(Context.CONTENT);
 *     let scriptToEval = storage.concat(script);
 *
 */
evaluate.ScriptStorageServiceClient = class {

  /**
   * @param {proxy.SyncChromeSender} chromeProxy
   *     Proxy for communicating with chrome space.
   */
  constructor(chromeProxy) {
    this.chrome = chromeProxy;
  }

  /**
   * Retrieve scripts associated with the given context.
   *
   * @param {Context} context
   *     Context to retrieve scripts from.
   *
   * @return {ScriptStorage}
   *     Scripts associated with given |context|.
   */
  for(context) {
    let scripts = this.chrome.getImportedScripts(context)[0];
    return new evaluate.ScriptStorage(scripts);
  }

};
