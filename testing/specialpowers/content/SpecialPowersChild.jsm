/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* This code is loaded in every child process that is started by mochitest in
 * order to be used as a replacement for UniversalXPConnect
 */

"use strict";

var EXPORTED_SYMBOLS = ["SpecialPowersChild"];

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const { ExtensionUtils } = ChromeUtils.import(
  "resource://gre/modules/ExtensionUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "MockFilePicker",
  "resource://specialpowers/MockFilePicker.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "MockColorPicker",
  "resource://specialpowers/MockColorPicker.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "MockPermissionPrompt",
  "resource://specialpowers/MockPermissionPrompt.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "SpecialPowersSandbox",
  "resource://specialpowers/SpecialPowersSandbox.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "WrapPrivileged",
  "resource://specialpowers/WrapPrivileged.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "NetUtil",
  "resource://gre/modules/NetUtil.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "AppConstants",
  "resource://gre/modules/AppConstants.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "PerTestCoverageUtils",
  "resource://testing-common/PerTestCoverageUtils.jsm"
);

// Allow stuff from this scope to be accessed from non-privileged scopes. This
// would crash if used outside of automation.
Cu.forcePermissiveCOWs();

function bindDOMWindowUtils(aWindow) {
  return aWindow && WrapPrivileged.wrap(aWindow.windowUtils);
}

// SPConsoleListener reflects nsIConsoleMessage objects into JS in a
// tidy, XPCOM-hiding way.  Messages that are nsIScriptError objects
// have their properties exposed in detail.  It also auto-unregisters
// itself when it receives a "sentinel" message.
function SPConsoleListener(callback) {
  this.callback = callback;
}

SPConsoleListener.prototype = {
  // Overload the observe method for both nsIConsoleListener and nsIObserver.
  // The topic will be null for nsIConsoleListener.
  observe(msg, topic) {
    let m = {
      message: msg.message,
      errorMessage: null,
      cssSelectors: null,
      sourceName: null,
      sourceLine: null,
      lineNumber: null,
      columnNumber: null,
      category: null,
      windowID: null,
      isScriptError: false,
      isConsoleEvent: false,
      isWarning: false,
      isException: false,
      isStrict: false,
    };
    if (msg instanceof Ci.nsIScriptError) {
      m.errorMessage = msg.errorMessage;
      m.cssSelectors = msg.cssSelectors;
      m.sourceName = msg.sourceName;
      m.sourceLine = msg.sourceLine;
      m.lineNumber = msg.lineNumber;
      m.columnNumber = msg.columnNumber;
      m.category = msg.category;
      m.windowID = msg.outerWindowID;
      m.innerWindowID = msg.innerWindowID;
      m.isScriptError = true;
      m.isWarning = (msg.flags & Ci.nsIScriptError.warningFlag) === 1;
      m.isException = (msg.flags & Ci.nsIScriptError.exceptionFlag) === 1;
      m.isStrict = (msg.flags & Ci.nsIScriptError.strictFlag) === 1;
    } else if (topic === "console-api-log-event") {
      // This is a dom/console event.
      let unwrapped = msg.wrappedJSObject;
      m.errorMessage = unwrapped.arguments[0];
      m.sourceName = unwrapped.filename;
      m.lineNumber = unwrapped.lineNumber;
      m.columnNumber = unwrapped.columnNumber;
      m.windowID = unwrapped.ID;
      m.innerWindowID = unwrapped.innerID;
      m.isConsoleEvent = true;
      m.isWarning = unwrapped.level === "warning";
    }

    Object.freeze(m);

    // Run in a separate runnable since console listeners aren't
    // supposed to touch content and this one might.
    Services.tm.dispatchToMainThread(() => {
      this.callback.call(undefined, m);
    });

    if (!m.isScriptError && !m.isConsoleEvent && m.message === "SENTINEL") {
      Services.obs.removeObserver(this, "console-api-log-event");
      Services.console.unregisterListener(this);
    }
  },

  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIConsoleListener,
    Ci.nsIObserver,
  ]),
};

class SpecialPowersChild extends JSWindowActorChild {
  constructor() {
    super();

    this._windowID = null;
    this.DOMWindowUtils = null;

    this._encounteredCrashDumpFiles = [];
    this._unexpectedCrashDumpFiles = {};
    this._crashDumpDir = null;
    this._serviceWorkerRegistered = false;
    this._serviceWorkerCleanUpRequests = new Map();
    Object.defineProperty(this, "Components", {
      configurable: true,
      enumerable: true,
      value: this.getFullComponents(),
    });
    this._createFilesOnError = null;
    this._createFilesOnSuccess = null;

    this._messageListeners = new ExtensionUtils.DefaultMap(() => new Set());

    this._consoleListeners = [];
    this._encounteredCrashDumpFiles = [];
    this._unexpectedCrashDumpFiles = {};
    this._crashDumpDir = null;
    this._mfl = null;
    this._applyingPermissions = false;
    this._observingPermissions = false;
    this._asyncObservers = new WeakMap();
    this._xpcomabi = null;
    this._os = null;
    this._pu = null;

    this._nextExtensionID = 0;
    this._extensionListeners = null;
  }

  handleEvent(aEvent) {
    // We don't actually care much about the "DOMWindowCreated" event.
    // We only listen to it to force creation of the actor.
  }

  actorCreated() {
    this.attachToWindow();
  }

  attachToWindow() {
    let window = this.contentWindow;
    if (!window.wrappedJSObject.SpecialPowers) {
      this._windowID = window.windowUtils.currentInnerWindowID;
      this.DOMWindowUtils = bindDOMWindowUtils(window);

      window.SpecialPowers = this;
      window.wrappedJSObject.SpecialPowers = this;
      if (this.IsInNestedFrame) {
        this.addPermission("allowXULXBL", true, window.document);
      }
    }
  }

  get window() {
    return this.contentWindow;
  }

  // Hack around devtools sometimes trying to JSON stringify us.
  toJSON() {
    return {};
  }

  toString() {
    return "[SpecialPowers]";
  }
  sanityCheck() {
    return "foo";
  }

  _addMessageListener(msgname, listener) {
    this._messageListeners.get(msgname).add(listener);
  }

  _removeMessageListener(msgname, listener) {
    this._messageListeners.get(msgname).delete(listener);
  }

  receiveMessage(message) {
    if (this._messageListeners.has(message.name)) {
      for (let listener of this._messageListeners.get(message.name)) {
        try {
          if (typeof listener === "function") {
            listener(message);
          } else {
            listener.receiveMessage(message);
          }
        } catch (e) {
          Cu.reportError(e);
        }
      }
    }

    switch (message.name) {
      case "SPProcessCrashService":
        if (message.json.type == "crash-observed") {
          for (let e of message.json.dumpIDs) {
            this._encounteredCrashDumpFiles.push(e.id + "." + e.extension);
          }
        }
        break;

      case "SPServiceWorkerRegistered":
        this._serviceWorkerRegistered = message.data.registered;
        break;

      case "SpecialPowers.FilesCreated":
        var createdHandler = this._createFilesOnSuccess;
        this._createFilesOnSuccess = null;
        this._createFilesOnError = null;
        if (createdHandler) {
          createdHandler(Cu.cloneInto(message.data, this.contentWindow));
        }
        break;

      case "SpecialPowers.FilesError":
        var errorHandler = this._createFilesOnError;
        this._createFilesOnSuccess = null;
        this._createFilesOnError = null;
        if (errorHandler) {
          errorHandler(message.data);
        }
        break;

      case "Spawn":
        let { task, args, caller, taskId } = message.data;
        return this._spawnTask(task, args, caller, taskId);

      case "Assert":
        {
          // An assertion has been done in a mochitest chrome script
          let { name, passed, stack, diag } = message.data;

          let SimpleTest =
            this.contentWindow && this.contentWindow.wrappedJSObject.SimpleTest;

          if (SimpleTest) {
            SimpleTest.record(
              passed,
              name,
              diag,
              stack && stack.formattedStack
            );
          } else {
            // Well, this is unexpected.
            dump(name + "\n");
          }
        }
        break;
    }
    return undefined;
  }

  registerProcessCrashObservers() {
    this.sendAsyncMessage("SPProcessCrashService", { op: "register-observer" });
  }

  unregisterProcessCrashObservers() {
    this.sendAsyncMessage("SPProcessCrashService", {
      op: "unregister-observer",
    });
  }

  /*
   * Privileged object wrapping API
   *
   * Usage:
   *   var wrapper = SpecialPowers.wrap(obj);
   *   wrapper.privilegedMethod(); wrapper.privilegedProperty;
   *   obj === SpecialPowers.unwrap(wrapper);
   *
   * These functions provide transparent access to privileged objects using
   * various pieces of deep SpiderMagic. Conceptually, a wrapper is just an
   * object containing a reference to the underlying object, where all method
   * calls and property accesses are transparently performed with the System
   * Principal. Moreover, objects obtained from the wrapper (including properties
   * and method return values) are wrapped automatically. Thus, after a single
   * call to SpecialPowers.wrap(), the wrapper layer is transitively maintained.
   *
   * Known Issues:
   *
   *  - The wrapping function does not preserve identity, so
   *    SpecialPowers.wrap(foo) !== SpecialPowers.wrap(foo). See bug 718543.
   *
   *  - The wrapper cannot see expando properties on unprivileged DOM objects.
   *    That is to say, the wrapper uses Xray delegation.
   *
   *  - The wrapper sometimes guesses certain ES5 attributes for returned
   *    properties. This is explained in a comment in the wrapper code above,
   *    and shouldn't be a problem.
   */
  wrap(obj) {
    return WrapPrivileged.wrap(obj);
  }
  unwrap(obj) {
    return WrapPrivileged.unwrap(obj);
  }
  isWrapper(val) {
    return WrapPrivileged.isWrapper(val);
  }

  /*
   * When content needs to pass a callback or a callback object to an API
   * accessed over SpecialPowers, that API may sometimes receive arguments for
   * whom it is forbidden to create a wrapper in content scopes. As such, we
   * need a layer to wrap the values in SpecialPowers wrappers before they ever
   * reach content.
   */
  wrapCallback(func) {
    return WrapPrivileged.wrapCallback(func);
  }
  wrapCallbackObject(obj) {
    return WrapPrivileged.wrapCallbackObject(obj);
  }

  /*
   * Used for assigning a property to a SpecialPowers wrapper, without unwrapping
   * the value that is assigned.
   */
  setWrapped(obj, prop, val) {
    if (!WrapPrivileged.isWrapper(obj)) {
      throw new Error(
        "You only need to use this for SpecialPowers wrapped objects"
      );
    }

    obj = WrapPrivileged.unwrap(obj);
    return Reflect.set(obj, prop, val);
  }

  /*
   * Create blank privileged objects to use as out-params for privileged functions.
   */
  createBlankObject() {
    return {};
  }

  /*
   * Because SpecialPowers wrappers don't preserve identity, comparing with ==
   * can be hazardous. Sometimes we can just unwrap to compare, but sometimes
   * wrapping the underlying object into a content scope is forbidden. This
   * function strips any wrappers if they exist and compare the underlying
   * values.
   */
  compare(a, b) {
    return WrapPrivileged.unwrap(a) === WrapPrivileged.unwrap(b);
  }

  get MockFilePicker() {
    return MockFilePicker;
  }

  get MockColorPicker() {
    return MockColorPicker;
  }

  get MockPermissionPrompt() {
    return MockPermissionPrompt;
  }

  quit() {
    this.sendAsyncMessage("SpecialPowers.Quit", {});
  }

  // fileRequests is an array of file requests. Each file request is an object.
  // A request must have a field |name|, which gives the base of the name of the
  // file to be created in the profile directory. If the request has a |data| field
  // then that data will be written to the file.
  createFiles(fileRequests, onCreation, onError) {
    return this.sendQuery("SpecialPowers.CreateFiles", fileRequests).then(
      onCreation,
      onError
    );
  }

  // Remove the files that were created using |SpecialPowers.createFiles()|.
  // This will be automatically called by |SimpleTest.finish()|.
  removeFiles() {
    this.sendAsyncMessage("SpecialPowers.RemoveFiles", {});
  }

  executeAfterFlushingMessageQueue(aCallback) {
    return this.sendQuery("Ping").then(aCallback);
  }

  async registeredServiceWorkers() {
    // For the time being, if parent_intercept is false, we can assume that
    // ServiceWorkers registered by the current test are all known to the SWM in
    // this process.
    if (
      !Services.prefs.getBoolPref("dom.serviceWorkers.parent_intercept", false)
    ) {
      let swm = Cc["@mozilla.org/serviceworkers/manager;1"].getService(
        Ci.nsIServiceWorkerManager
      );
      let regs = swm.getAllRegistrations();

      // XXX This is shared with SpecialPowersAPIParent.jsm
      let workers = new Array(regs.length);
      for (let i = 0; i < workers.length; ++i) {
        let { scope, scriptSpec } = regs.queryElementAt(
          i,
          Ci.nsIServiceWorkerRegistrationInfo
        );
        workers[i] = { scope, scriptSpec };
      }

      return workers;
    }

    // Please see the comment in SpecialPowersObserver.jsm above
    // this._serviceWorkerListener's assignment for what this returns.
    if (this._serviceWorkerRegistered) {
      // This test registered at least one service worker. Send a synchronous
      // call to the parent to make sure that it called unregister on all of its
      // service workers.
      let { workers } = await this.sendQuery("SPCheckServiceWorkers");
      return workers;
    }

    return [];
  }

  /*
   * Load a privileged script that runs same-process. This is different from
   * |loadChromeScript|, which will run in the parent process in e10s mode.
   */
  loadPrivilegedScript(aFunction) {
    var str = "(" + aFunction.toString() + ")();";
    let gGlobalObject = Cu.getGlobalForObject(this);
    let sb = Cu.Sandbox(gGlobalObject);
    var window = this.contentWindow;
    var mc = new window.MessageChannel();
    sb.port = mc.port1;
    try {
      let blob = new Blob([str], { type: "application/javascript" });
      let blobUrl = URL.createObjectURL(blob);
      Services.scriptloader.loadSubScript(blobUrl, sb);
    } catch (e) {
      throw WrapPrivileged.wrap(e);
    }

    return mc.port2;
  }

  _readUrlAsString(aUrl) {
    // Fetch script content as we can't use scriptloader's loadSubScript
    // to evaluate http:// urls...
    var scriptableStream = Cc[
      "@mozilla.org/scriptableinputstream;1"
    ].getService(Ci.nsIScriptableInputStream);

    var channel = NetUtil.newChannel({
      uri: aUrl,
      loadUsingSystemPrincipal: true,
    });
    var input = channel.open();
    scriptableStream.init(input);

    var str;
    var buffer = [];

    while ((str = scriptableStream.read(4096))) {
      buffer.push(str);
    }

    var output = buffer.join("");

    scriptableStream.close();
    input.close();

    var status;
    if (channel instanceof Ci.nsIHttpChannel) {
      status = channel.responseStatus;
    }

    if (status == 404) {
      throw new Error(
        `Error while executing chrome script '${aUrl}':\n` +
          "The script doesn't exist. Ensure you have registered it in " +
          "'support-files' in your mochitest.ini."
      );
    }

    return output;
  }

  loadChromeScript(urlOrFunction, sandboxOptions) {
    // Create a unique id for this chrome script
    let uuidGenerator = Cc["@mozilla.org/uuid-generator;1"].getService(
      Ci.nsIUUIDGenerator
    );
    let id = uuidGenerator.generateUUID().toString();

    // Tells chrome code to evaluate this chrome script
    let scriptArgs = { id, sandboxOptions };
    if (typeof urlOrFunction == "function") {
      scriptArgs.function = {
        body: "(" + urlOrFunction.toString() + ")();",
        name: urlOrFunction.name,
      };
    } else {
      // Note: We need to do this in the child since, even though
      // `_readUrlAsString` pretends to be synchronous, its channel
      // winds up spinning the event loop when loading HTTP URLs. That
      // leads to unexpected out-of-order operations if the child sends
      // a message immediately after loading the script.
      scriptArgs.function = {
        body: this._readUrlAsString(urlOrFunction),
      };
      scriptArgs.url = urlOrFunction;
    }
    this.sendAsyncMessage("SPLoadChromeScript", scriptArgs);

    // Returns a MessageManager like API in order to be
    // able to communicate with this chrome script
    let listeners = [];
    let chromeScript = {
      addMessageListener: (name, listener) => {
        listeners.push({ name, listener });
      },

      promiseOneMessage: name =>
        new Promise(resolve => {
          chromeScript.addMessageListener(name, function listener(message) {
            chromeScript.removeMessageListener(name, listener);
            resolve(message);
          });
        }),

      removeMessageListener: (name, listener) => {
        listeners = listeners.filter(
          o => o.name != name || o.listener != listener
        );
      },

      sendAsyncMessage: (name, message) => {
        this.sendAsyncMessage("SPChromeScriptMessage", { id, name, message });
      },

      sendQuery: (name, message) => {
        return this.sendQuery("SPChromeScriptMessage", { id, name, message });
      },

      destroy: () => {
        listeners = [];
        this._removeMessageListener("SPChromeScriptMessage", chromeScript);
      },

      receiveMessage: aMessage => {
        let messageId = aMessage.json.id;
        let name = aMessage.json.name;
        let message = aMessage.json.message;
        if (this.contentWindow) {
          message = new StructuredCloneHolder(message).deserialize(
            this.contentWindow
          );
        }
        // Ignore message from other chrome script
        if (messageId != id) {
          return null;
        }

        let result;
        if (aMessage.name == "SPChromeScriptMessage") {
          for (let listener of listeners.filter(o => o.name == name)) {
            result = listener.listener(message);
          }
        }
        return result;
      },
    };
    this._addMessageListener("SPChromeScriptMessage", chromeScript);

    return this.wrap(chromeScript);
  }

  async importInMainProcess(importString) {
    var message = await this.sendQuery("SPImportInMainProcess", importString);
    if (message.hadError) {
      throw new Error(
        "SpecialPowers.importInMainProcess failed with error " +
          message.errorMessage
      );
    }
  }

  get Services() {
    return WrapPrivileged.wrap(Services);
  }

  /*
   * A getter for the privileged Components object we have.
   */
  getFullComponents() {
    return Components;
  }

  /*
   * Convenient shortcuts to the standard Components abbreviations.
   */
  get Cc() {
    return WrapPrivileged.wrap(this.getFullComponents().classes);
  }
  get Ci() {
    return WrapPrivileged.wrap(this.getFullComponents().interfaces);
  }
  get Cu() {
    return WrapPrivileged.wrap(this.getFullComponents().utils);
  }
  get Cr() {
    return WrapPrivileged.wrap(this.getFullComponents().results);
  }

  getDOMWindowUtils(aWindow) {
    if (aWindow == this.contentWindow && this.DOMWindowUtils != null) {
      return this.DOMWindowUtils;
    }

    return bindDOMWindowUtils(aWindow);
  }

  /*
   * A method to get a DOMParser that can't parse XUL.
   */
  getNoXULDOMParser() {
    // If we create it with a system subject principal (so it gets a
    // nullprincipal), it won't be able to parse XUL by default.
    return WrapPrivileged.wrap(new DOMParser());
  }

  get InspectorUtils() {
    return WrapPrivileged.wrap(InspectorUtils);
  }

  get PromiseDebugging() {
    return WrapPrivileged.wrap(PromiseDebugging);
  }

  async waitForCrashes(aExpectingProcessCrash) {
    if (!aExpectingProcessCrash) {
      return;
    }

    var crashIds = this._encounteredCrashDumpFiles
      .filter(filename => {
        return filename.length === 40 && filename.endsWith(".dmp");
      })
      .map(id => {
        return id.slice(0, -4); // Strip the .dmp extension to get the ID
      });

    await this.sendQuery("SPProcessCrashManagerWait", {
      crashIds,
    });
  }

  async removeExpectedCrashDumpFiles(aExpectingProcessCrash) {
    var success = true;
    if (aExpectingProcessCrash) {
      var message = {
        op: "delete-crash-dump-files",
        filenames: this._encounteredCrashDumpFiles,
      };
      if (!(await this.sendQuery("SPProcessCrashService", message))) {
        success = false;
      }
    }
    this._encounteredCrashDumpFiles.length = 0;
    return success;
  }

  async findUnexpectedCrashDumpFiles() {
    var self = this;
    var message = {
      op: "find-crash-dump-files",
      crashDumpFilesToIgnore: this._unexpectedCrashDumpFiles,
    };
    var crashDumpFiles = await this.sendQuery("SPProcessCrashService", message);
    crashDumpFiles.forEach(function(aFilename) {
      self._unexpectedCrashDumpFiles[aFilename] = true;
    });
    return crashDumpFiles;
  }

  removePendingCrashDumpFiles() {
    var message = {
      op: "delete-pending-crash-dump-files",
    };
    return this.sendQuery("SPProcessCrashService", message);
  }

  _setTimeout(callback) {
    // for mochitest-browser
    if (typeof this.chromeWindow != "undefined") {
      this.chromeWindow.setTimeout(callback, 0);
    }
    // for mochitest-plain
    else {
      this.contentWindow.setTimeout(callback, 0);
    }
  }

  promiseTimeout(delay) {
    return new Promise(resolve => {
      this._setTimeout(resolve, delay);
    });
  }

  _delayCallbackTwice(callback) {
    let delayedCallback = () => {
      let delayAgain = aCallback => {
        // Using this._setTimeout doesn't work here
        // It causes failures in mochtests that use
        // multiple pushPrefEnv calls
        // For chrome/browser-chrome mochitests
        this._setTimeout(aCallback);
      };
      delayAgain(delayAgain.bind(this, callback));
    };
    return delayedCallback;
  }

  /* apply permissions to the system and when the test case is finished (SimpleTest.finish())
     we will revert the permission back to the original.

     inPermissions is an array of objects where each object has a type, action, context, ex:
     [{'type': 'SystemXHR', 'allow': 1, 'context': document},
      {'type': 'SystemXHR', 'allow': Ci.nsIPermissionManager.PROMPT_ACTION, 'context': document}]

     Allow can be a boolean value of true/false or ALLOW_ACTION/DENY_ACTION/PROMPT_ACTION/UNKNOWN_ACTION
  */
  async pushPermissions(inPermissions, callback) {
    inPermissions = Cu.waiveXrays(inPermissions);
    var pendingPermissions = [];
    var cleanupPermissions = [];

    for (var p in inPermissions) {
      var permission = inPermissions[p];
      var originalValue = Ci.nsIPermissionManager.UNKNOWN_ACTION;
      var context = Cu.unwaiveXrays(permission.context); // Sometimes |context| is a DOM object on which we expect
      // to be able to access .nodePrincipal, so we need to unwaive.
      if (
        await this.testPermission(
          permission.type,
          Ci.nsIPermissionManager.ALLOW_ACTION,
          context
        )
      ) {
        originalValue = Ci.nsIPermissionManager.ALLOW_ACTION;
      } else if (
        await this.testPermission(
          permission.type,
          Ci.nsIPermissionManager.DENY_ACTION,
          context
        )
      ) {
        originalValue = Ci.nsIPermissionManager.DENY_ACTION;
      } else if (
        await this.testPermission(
          permission.type,
          Ci.nsIPermissionManager.PROMPT_ACTION,
          context
        )
      ) {
        originalValue = Ci.nsIPermissionManager.PROMPT_ACTION;
      } else if (
        await this.testPermission(
          permission.type,
          Ci.nsICookiePermission.ACCESS_SESSION,
          context
        )
      ) {
        originalValue = Ci.nsICookiePermission.ACCESS_SESSION;
      }

      let principal = this._getPrincipalFromArg(context);
      if (principal.isSystemPrincipal) {
        continue;
      }

      let perm;
      if (typeof permission.allow !== "boolean") {
        perm = permission.allow;
      } else {
        perm = permission.allow
          ? Ci.nsIPermissionManager.ALLOW_ACTION
          : Ci.nsIPermissionManager.DENY_ACTION;
      }

      if (permission.remove) {
        perm = Ci.nsIPermissionManager.UNKNOWN_ACTION;
      }

      if (originalValue == perm) {
        continue;
      }

      var todo = {
        op: "add",
        type: permission.type,
        permission: perm,
        value: perm,
        principal,
        expireType:
          typeof permission.expireType === "number" ? permission.expireType : 0, // default: EXPIRE_NEVER
        expireTime:
          typeof permission.expireTime === "number" ? permission.expireTime : 0,
      };

      var cleanupTodo = Object.assign({}, todo);

      if (permission.remove) {
        todo.op = "remove";
      }

      pendingPermissions.push(todo);

      /* Push original permissions value or clear into cleanup array */
      if (originalValue == Ci.nsIPermissionManager.UNKNOWN_ACTION) {
        cleanupTodo.op = "remove";
      } else {
        cleanupTodo.value = originalValue;
        cleanupTodo.permission = originalValue;
      }
      cleanupPermissions.push(cleanupTodo);
    }

    if (pendingPermissions.length > 0) {
      // The callback needs to be delayed twice. One delay is because the pref
      // service doesn't guarantee the order it calls its observers in, so it
      // may notify the observer holding the callback before the other
      // observers have been notified and given a chance to make the changes
      // that the callback checks for. The second delay is because pref
      // observers often defer making their changes by posting an event to the
      // event loop.
      if (!this._observingPermissions) {
        this._observingPermissions = true;
        // If specialpowers is in main-process, then we can add a observer
        // to get all 'perm-changed' signals. Otherwise, it can't receive
        // all signals, so we register a observer in specialpowersobserver(in
        // main-process) and get signals from it.
        if (this.isMainProcess()) {
          this.permissionObserverProxy._specialPowersAPI = this;
          Services.obs.addObserver(
            this.permissionObserverProxy,
            "perm-changed"
          );
        } else {
          this.registerObservers("perm-changed");
          // bind() is used to set 'this' to SpecialPowersAPI itself.
          this._addMessageListener(
            "specialpowers-perm-changed",
            this.permChangedProxy.bind(this)
          );
        }
      }
      this._permissionsUndoStack.push(cleanupPermissions);
      this._pendingPermissions.push([
        pendingPermissions,
        this._delayCallbackTwice(callback),
      ]);
      this._applyPermissions();
    } else {
      this._setTimeout(callback);
    }
  }

  /*
   * This function should be used when specialpowers is in content process but
   * it want to get the notification from chrome space.
   *
   * This function will call Services.obs.addObserver in SpecialPowersObserver
   * (that is in chrome process) and forward the data received to SpecialPowers
   * via messageManager.
   * You can use this._addMessageListener("specialpowers-YOUR_TOPIC") to fire
   * the callback.
   *
   * To get the expected data, you should modify
   * SpecialPowersObserver.prototype._registerObservers.observe. Or the message
   * you received from messageManager will only contain 'aData' from Service.obs.
   */
  registerObservers(topic) {
    var msg = {
      op: "add",
      observerTopic: topic,
    };
    return this.sendQuery("SPObserverService", msg);
  }

  permChangedProxy(aMessage) {
    let permission = aMessage.json.permission;
    let aData = aMessage.json.aData;
    this._permissionObserver.observe(permission, aData);
  }

  popPermissions(callback) {
    let promise = new Promise(resolve => {
      if (this._permissionsUndoStack.length > 0) {
        // See pushPermissions comment regarding delay.
        let cb = this._delayCallbackTwice(resolve);
        /* Each pop from the stack will yield an object {op/type/permission/value/url/appid/isInIsolatedMozBrowserElement} or null */
        this._pendingPermissions.push([this._permissionsUndoStack.pop(), cb]);
        this._applyPermissions();
      } else {
        if (this._observingPermissions) {
          this._observingPermissions = false;
          this._removeMessageListener(
            "specialpowers-perm-changed",
            this.permChangedProxy.bind(this)
          );
        }
        this._setTimeout(resolve);
      }
    });
    if (callback) {
      promise.then(callback);
    }
    return promise;
  }

  flushPermissions(callback) {
    while (this._permissionsUndoStack.length > 1) {
      this.popPermissions(null);
    }

    return this.popPermissions(callback);
  }

  setTestPluginEnabledState(newEnabledState, pluginName) {
    return this.sendQuery("SPSetTestPluginEnabledState", {
      newEnabledState,
      pluginName,
    });
  }

  /*
    Iterate through one atomic set of permissions actions and perform allow/deny as appropriate.
    All actions performed must modify the relevant permission.
  */
  _applyPermissions() {
    if (this._applyingPermissions || this._pendingPermissions.length <= 0) {
      return;
    }

    /* Set lock and get prefs from the _pendingPrefs queue */
    this._applyingPermissions = true;
    var transaction = this._pendingPermissions.shift();
    var pendingActions = transaction[0];
    var callback = transaction[1];
    var lastPermission = pendingActions[pendingActions.length - 1];

    var self = this;
    this._permissionObserver._self = self;
    this._permissionObserver._lastPermission = lastPermission;
    this._permissionObserver._callback = callback;
    this._permissionObserver._nextCallback = function() {
      self._applyingPermissions = false;
      // Now apply any permissions that may have been queued while we were applying
      self._applyPermissions();
    };

    for (var idx in pendingActions) {
      var perm = pendingActions[idx];
      this.sendAsyncMessage("SPPermissionManager", perm);
    }
  }

  async pushPrefEnv(inPrefs, callback = null) {
    await this.sendQuery("PushPrefEnv", inPrefs).then(callback);
    await this.promiseTimeout(0);
  }

  async popPrefEnv(callback = null) {
    await this.sendQuery("PopPrefEnv").then(callback);
    await this.promiseTimeout(0);
  }

  async flushPrefEnv(callback = null) {
    await this.sendQuery("FlushPrefEnv").then(callback);
    await this.promiseTimeout(0);
  }

  _addObserverProxy(notification) {
    if (notification in this._proxiedObservers) {
      this._addMessageListener(
        notification,
        this._proxiedObservers[notification]
      );
    }
  }
  _removeObserverProxy(notification) {
    if (notification in this._proxiedObservers) {
      this._removeMessageListener(
        notification,
        this._proxiedObservers[notification]
      );
    }
  }

  addObserver(obs, notification, weak) {
    // Make sure the parent side exists, or we won't get any notifications.
    this.sendAsyncMessage("Wakeup");

    this._addObserverProxy(notification);
    obs = Cu.waiveXrays(obs);
    if (
      typeof obs == "object" &&
      obs.observe.name != "SpecialPowersCallbackWrapper"
    ) {
      obs.observe = WrapPrivileged.wrapCallback(obs.observe);
    }
    Services.obs.addObserver(obs, notification, weak);
  }
  removeObserver(obs, notification) {
    this._removeObserverProxy(notification);
    Services.obs.removeObserver(Cu.waiveXrays(obs), notification);
  }
  notifyObservers(subject, topic, data) {
    Services.obs.notifyObservers(subject, topic, data);
  }

  /**
   * An async observer is useful if you're listening for a
   * notification that normally is only used by C++ code or chrome
   * code (so it runs in the SystemGroup), but we need to know about
   * it for a test (which runs as web content). If we used
   * addObserver, we would assert when trying to enter web content
   * from a runnabled labeled by the SystemGroup. An async observer
   * avoids this problem.
   */
  addAsyncObserver(obs, notification, weak) {
    obs = Cu.waiveXrays(obs);
    if (
      typeof obs == "object" &&
      obs.observe.name != "SpecialPowersCallbackWrapper"
    ) {
      obs.observe = WrapPrivileged.wrapCallback(obs.observe);
    }
    let asyncObs = (...args) => {
      Services.tm.dispatchToMainThread(() => {
        if (typeof obs == "function") {
          obs(...args);
        } else {
          obs.observe.call(undefined, ...args);
        }
      });
    };
    this._asyncObservers.set(obs, asyncObs);
    Services.obs.addObserver(asyncObs, notification, weak);
  }
  removeAsyncObserver(obs, notification) {
    let asyncObs = this._asyncObservers.get(Cu.waiveXrays(obs));
    Services.obs.removeObserver(asyncObs, notification);
  }

  can_QI(obj) {
    return obj.QueryInterface !== undefined;
  }
  do_QueryInterface(obj, iface) {
    return obj.QueryInterface(Ci[iface]);
  }

  call_Instanceof(obj1, obj2) {
    obj1 = WrapPrivileged.unwrap(obj1);
    obj2 = WrapPrivileged.unwrap(obj2);
    return obj1 instanceof obj2;
  }

  // Returns a privileged getter from an object. GetOwnPropertyDescriptor does
  // not work here because xray wrappers don't properly implement it.
  //
  // This terribleness is used by dom/base/test/test_object.html because
  // <object> and <embed> tags will spawn plugins if their prototype is touched,
  // so we need to get and cache the getter of |hasRunningPlugin| if we want to
  // call it without paradoxically spawning the plugin.
  do_lookupGetter(obj, name) {
    return Object.prototype.__lookupGetter__.call(obj, name);
  }

  // Mimic the get*Pref API
  getBoolPref(...args) {
    return Services.prefs.getBoolPref(...args);
  }
  getIntPref(...args) {
    return Services.prefs.getIntPref(...args);
  }
  getCharPref(...args) {
    return Services.prefs.getCharPref(...args);
  }
  getComplexValue(prefName, iid) {
    return Services.prefs.getComplexValue(prefName, iid);
  }

  getParentBoolPref(prefName, defaultValue) {
    return this._getParentPref(prefName, "BOOL", { defaultValue });
  }
  getParentIntPref(prefName, defaultValue) {
    return this._getParentPref(prefName, "INT", { defaultValue });
  }
  getParentCharPref(prefName, defaultValue) {
    return this._getParentPref(prefName, "CHAR", { defaultValue });
  }

  // Mimic the set*Pref API
  setBoolPref(prefName, value) {
    return this._setPref(prefName, "BOOL", value);
  }
  setIntPref(prefName, value) {
    return this._setPref(prefName, "INT", value);
  }
  setCharPref(prefName, value) {
    return this._setPref(prefName, "CHAR", value);
  }
  setComplexValue(prefName, iid, value) {
    return this._setPref(prefName, "COMPLEX", value, iid);
  }

  // Mimic the clearUserPref API
  clearUserPref(prefName) {
    let msg = {
      op: "clear",
      prefName,
      prefType: "",
    };
    return this.sendQuery("SPPrefService", msg);
  }

  // Private pref functions to communicate to chrome
  async _getParentPref(prefName, prefType, { defaultValue, iid }) {
    let msg = {
      op: "get",
      prefName,
      prefType,
      iid, // Only used with complex prefs
      defaultValue, // Optional default value
    };
    let val = await this.sendQuery("SPPrefService", msg);
    if (val == null) {
      throw new Error(`Error getting pref '${prefName}'`);
    }
    return val;
  }
  _getPref(prefName, prefType, { defaultValue }) {
    switch (prefType) {
      case "BOOL":
        return Services.prefs.getBoolPref(prefName);
      case "INT":
        return Services.prefs.getIntPref(prefName);
      case "CHAR":
        return Services.prefs.getCharPref(prefName);
    }
    return undefined;
  }
  _setPref(prefName, prefType, prefValue, iid) {
    let msg = {
      op: "set",
      prefName,
      prefType,
      iid, // Only used with complex prefs
      prefValue,
    };
    return this.sendQuery("SPPrefService", msg);
  }

  _getMUDV(window) {
    return window.docShell.contentViewer;
  }
  // XXX: these APIs really ought to be removed, they're not e10s-safe.
  // (also they're pretty Firefox-specific)
  _getTopChromeWindow(window) {
    return window.docShell.rootTreeItem.domWindow;
  }
  _getAutoCompletePopup(window) {
    return this._getTopChromeWindow(window).document.getElementById(
      "PopupAutoComplete"
    );
  }
  addAutoCompletePopupEventListener(window, eventname, listener) {
    this._getAutoCompletePopup(window).addEventListener(eventname, listener);
  }
  removeAutoCompletePopupEventListener(window, eventname, listener) {
    this._getAutoCompletePopup(window).removeEventListener(eventname, listener);
  }
  get formHistory() {
    let tmp = {};
    ChromeUtils.import("resource://gre/modules/FormHistory.jsm", tmp);
    return WrapPrivileged.wrap(tmp.FormHistory);
  }
  getFormFillController(window) {
    return Cc["@mozilla.org/satchel/form-fill-controller;1"].getService(
      Ci.nsIFormFillController
    );
  }
  attachFormFillControllerTo(window) {
    this.getFormFillController().attachPopupElementToBrowser(
      window.docShell,
      this._getAutoCompletePopup(window)
    );
  }
  detachFormFillControllerFrom(window) {
    this.getFormFillController().detachFromBrowser(window.docShell);
  }
  isBackButtonEnabled(window) {
    return !this._getTopChromeWindow(window)
      .document.getElementById("Browser:Back")
      .hasAttribute("disabled");
  }
  // XXX end of problematic APIs

  addChromeEventListener(type, listener, capture, allowUntrusted) {
    this.docShell.chromeEventHandler.addEventListener(
      type,
      listener,
      capture,
      allowUntrusted
    );
  }
  removeChromeEventListener(type, listener, capture) {
    this.docShell.chromeEventHandler.removeEventListener(
      type,
      listener,
      capture
    );
  }

  // Note: each call to registerConsoleListener MUST be paired with a
  // call to postConsoleSentinel; when the callback receives the
  // sentinel it will unregister itself (_after_ calling the
  // callback).  SimpleTest.expectConsoleMessages does this for you.
  // If you register more than one console listener, a call to
  // postConsoleSentinel will zap all of them.
  registerConsoleListener(callback) {
    let listener = new SPConsoleListener(callback);
    Services.console.registerListener(listener);

    // listen for dom/console events as well
    Services.obs.addObserver(listener, "console-api-log-event");
  }
  postConsoleSentinel() {
    Services.console.logStringMessage("SENTINEL");
  }
  resetConsole() {
    Services.console.reset();
  }

  getFullZoom(window) {
    return this._getMUDV(window).fullZoom;
  }
  getDeviceFullZoom(window) {
    return this._getMUDV(window).deviceFullZoom;
  }
  setFullZoom(window, zoom) {
    this._getMUDV(window).fullZoom = zoom;
  }
  getTextZoom(window) {
    return this._getMUDV(window).textZoom;
  }
  setTextZoom(window, zoom) {
    this._getMUDV(window).textZoom = zoom;
  }

  getOverrideDPPX(window) {
    return this._getMUDV(window).overrideDPPX;
  }
  setOverrideDPPX(window, dppx) {
    this._getMUDV(window).overrideDPPX = dppx;
  }

  emulateMedium(window, mediaType) {
    this._getMUDV(window).emulateMedium(mediaType);
  }
  stopEmulatingMedium(window) {
    this._getMUDV(window).stopEmulatingMedium();
  }

  // Takes a snapshot of the given window and returns a <canvas>
  // containing the image. When the window is same-process, the canvas
  // is returned synchronously. When it is out-of-process (or when a
  // BrowsingContext or FrameLoaderOwner is passed instead of a Window),
  // a promise which resolves to such a canvas is returned instead.
  snapshotWindowWithOptions(content, rect, bgcolor, options) {
    function getImageData(rect, bgcolor, options) {
      let el = content.document.createElementNS(
        "http://www.w3.org/1999/xhtml",
        "canvas"
      );
      if (rect === undefined) {
        rect = {
          top: content.scrollY,
          left: content.scrollX,
          width: content.innerWidth,
          height: content.innerHeight,
        };
      }
      if (bgcolor === undefined) {
        bgcolor = "rgb(255,255,255)";
      }
      if (options === undefined) {
        options = {};
      }

      el.width = rect.width;
      el.height = rect.height;
      let ctx = el.getContext("2d");

      let flags = 0;
      for (let option in options) {
        flags |= options[option] && ctx[option];
      }

      ctx.drawWindow(
        content,
        rect.left,
        rect.top,
        rect.width,
        rect.height,
        bgcolor,
        flags
      );

      return ctx.getImageData(0, 0, el.width, el.height);
    }

    let toCanvas = imageData => {
      let el = this.document.createElementNS(
        "http://www.w3.org/1999/xhtml",
        "canvas"
      );
      el.width = imageData.width;
      el.height = imageData.height;

      if (ImageData.isInstance(imageData)) {
        let ctx = el.getContext("2d");
        ctx.putImageData(imageData, 0, 0);
      }

      return el;
    };

    if (Window.isInstance(content)) {
      // Hack around tests that try to snapshot 0 width or height
      // elements.
      if (rect && !(rect.width && rect.height)) {
        return toCanvas(rect);
      }

      // This is an in-process window. Snapshot it synchronously.
      return toCanvas(getImageData(rect, bgcolor, options));
    }

    // This is a remote window or frame. Snapshot it asynchronously and
    // return a promise for the result. Alas, consumers expect us to
    // return a <canvas> element rather than an ImageData object, so we
    // need to convert the result from the remote snapshot to a local
    // canvas.
    return this.spawn(content, [rect, bgcolor, options], getImageData).then(
      toCanvas
    );
  }

  snapshotWindow(win, withCaret, rect, bgcolor) {
    return this.snapshotWindowWithOptions(win, rect, bgcolor, {
      DRAWWINDOW_DRAW_CARET: withCaret,
    });
  }

  snapshotRect(win, rect, bgcolor) {
    return this.snapshotWindowWithOptions(win, rect, bgcolor);
  }

  gc() {
    this.DOMWindowUtils.garbageCollect();
  }

  forceGC() {
    Cu.forceGC();
  }

  forceShrinkingGC() {
    Cu.forceShrinkingGC();
  }

  forceCC() {
    Cu.forceCC();
  }

  finishCC() {
    Cu.finishCC();
  }

  ccSlice(budget) {
    Cu.ccSlice(budget);
  }

  // Due to various dependencies between JS objects and C++ objects, an ordinary
  // forceGC doesn't necessarily clear all unused objects, thus the GC and CC
  // needs to run several times and when no other JS is running.
  // The current number of iterations has been determined according to massive
  // cross platform testing.
  exactGC(callback) {
    let count = 0;

    function genGCCallback(cb) {
      return function() {
        Cu.forceCC();
        if (++count < 3) {
          Cu.schedulePreciseGC(genGCCallback(cb));
        } else if (cb) {
          cb();
        }
      };
    }

    Cu.schedulePreciseGC(genGCCallback(callback));
  }

  nondeterministicGetWeakMapKeys(m) {
    return ChromeUtils.nondeterministicGetWeakMapKeys(m);
  }

  getMemoryReports() {
    try {
      Cc["@mozilla.org/memory-reporter-manager;1"]
        .getService(Ci.nsIMemoryReporterManager)
        .getReports(() => {}, null, () => {}, null, false);
    } catch (e) {}
  }

  setGCZeal(zeal) {
    Cu.setGCZeal(zeal);
  }

  isMainProcess() {
    try {
      return (
        Services.appinfo.processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT
      );
    } catch (e) {}
    return true;
  }

  get XPCOMABI() {
    if (this._xpcomabi != null) {
      return this._xpcomabi;
    }

    var xulRuntime = Services.appinfo.QueryInterface(Ci.nsIXULRuntime);

    this._xpcomabi = xulRuntime.XPCOMABI;
    return this._xpcomabi;
  }

  // The optional aWin parameter allows the caller to specify a given window in
  // whose scope the runnable should be dispatched. If aFun throws, the
  // exception will be reported to aWin.
  executeSoon(aFun, aWin) {
    // Create the runnable in the scope of aWin to avoid running into COWs.
    var runnable = {};
    if (aWin) {
      runnable = Cu.createObjectIn(aWin);
    }
    runnable.run = aFun;
    Cu.dispatch(runnable, aWin);
  }

  get OS() {
    if (this._os != null) {
      return this._os;
    }

    this._os = Services.appinfo.OS;
    return this._os;
  }

  get useRemoteSubframes() {
    return this.docShell.nsILoadContext.useRemoteSubframes;
  }

  addSystemEventListener(target, type, listener, useCapture) {
    Services.els.addSystemEventListener(target, type, listener, useCapture);
  }
  removeSystemEventListener(target, type, listener, useCapture) {
    Services.els.removeSystemEventListener(target, type, listener, useCapture);
  }

  // helper method to check if the event is consumed by either default group's
  // event listener or system group's event listener.
  defaultPreventedInAnyGroup(event) {
    // FYI: Event.defaultPrevented returns false in content context if the
    //      event is consumed only by system group's event listeners.
    return event.defaultPrevented;
  }

  getDOMRequestService() {
    var serv = Services.DOMRequest;
    var res = {};
    var props = [
      "createRequest",
      "createCursor",
      "fireError",
      "fireSuccess",
      "fireDone",
      "fireDetailedError",
    ];
    for (var i in props) {
      let prop = props[i];
      res[prop] = function() {
        return serv[prop].apply(serv, arguments);
      };
    }
    return res;
  }

  addCategoryEntry(category, entry, value, persists, replace) {
    Services.catMan.addCategoryEntry(category, entry, value, persists, replace);
  }

  deleteCategoryEntry(category, entry, persists) {
    Services.catMan.deleteCategoryEntry(category, entry, persists);
  }
  openDialog(win, args) {
    return win.openDialog.apply(win, args);
  }
  // This is a blocking call which creates and spins a native event loop
  spinEventLoop(win) {
    // simply do a sync XHR back to our windows location.
    var syncXHR = new win.XMLHttpRequest();
    syncXHR.open("GET", win.location, false);
    syncXHR.send();
  }

  // :jdm gets credit for this.  ex: getPrivilegedProps(window, 'location.href');
  getPrivilegedProps(obj, props) {
    var parts = props.split(".");
    for (var i = 0; i < parts.length; i++) {
      var p = parts[i];
      if (obj[p] != undefined) {
        obj = obj[p];
      } else {
        return null;
      }
    }
    return obj;
  }

  _browsingContextForTarget(target) {
    if (BrowsingContext.isInstance(target)) {
      return target;
    }
    if (Element.isInstance(target)) {
      return target.browsingContext;
    }

    return BrowsingContext.getFromWindow(target);
  }

  /**
   * Runs a task in the context of the given frame, and returns a
   * promise which resolves to the return value of that task.
   *
   * The given frame may be in-process or out-of-process. Either way,
   * the task will run asynchronously, in a sandbox with access to the
   * frame's content window via its `content` global. Any arguments
   * passed will be copied via structured clone, as will its return
   * value.
   *
   * The sandbox also has access to an Assert object, as provided by
   * Assert.jsm. Any assertion methods called before the task resolves
   * will be relayed back to the test environment of the caller.
   *
   * @param {BrowsingContext or FrameLoaderOwner or WindowProxy} target
   *        The target in which to run the task. This may be any element
   *        which implements the FrameLoaderOwner interface (including
   *        HTML <iframe> elements and XUL <browser> elements) or a
   *        WindowProxy (either in-process or remote).
   * @param {Array<any>} args
   *        An array of arguments to pass to the task. All arguments
   *        must be structured clone compatible, and will be cloned
   *        before being passed to the task.
   * @param {function} task
   *        The function to run in the context of the target. The
   *        function will be stringified and re-evaluated in the context
   *        of the target's content window. It may return any structured
   *        clone compatible value, or a Promise which resolves to the
   *        same, which will be returned to the caller.
   *
   * @returns {Promise<any>}
   *        A promise which resolves to the return value of the task, or
   *        which rejects if the task raises an exception. As this is
   *        being written, the rejection value will always be undefined
   *        in the cases where the task throws an error, though that may
   *        change in the future.
   */
  spawn(target, args, task) {
    let browsingContext = this._browsingContextForTarget(target);

    return this.sendQuery("Spawn", {
      browsingContext,
      args,
      task: String(task),
      caller: SpecialPowersSandbox.getCallerInfo(Components.stack.caller),
    });
  }

  snapshotContext(target, rect, background) {
    let browsingContext = this._browsingContextForTarget(target);

    return this.sendQuery("Snapshot", {
      browsingContext,
      rect,
      background,
    }).then(imageData => {
      return this.contentWindow.createImageBitmap(imageData);
    });
  }

  _spawnTask(task, args, caller, taskId) {
    let sb = new SpecialPowersSandbox(null, data => {
      this.sendAsyncMessage("ProxiedAssert", { taskId, data });
    });

    sb.sandbox.SpecialPowers = this;
    Object.defineProperty(sb.sandbox, "content", {
      get: () => {
        return this.contentWindow;
      },
      enumerable: true,
    });

    return sb.execute(task, args, caller);
  }

  getFocusedElementForWindow(targetWindow, aDeep) {
    var outParam = {};
    Services.focus.getFocusedElementForWindow(targetWindow, aDeep, outParam);
    return outParam.value;
  }

  get focusManager() {
    return Services.focus;
  }

  activeWindow() {
    return Services.focus.activeWindow;
  }

  focusedWindow() {
    return Services.focus.focusedWindow;
  }

  focus(aWindow) {
    // This is called inside TestRunner._makeIframe without aWindow, because of assertions in oop mochitests
    // With aWindow, it is called in SimpleTest.waitForFocus to allow popup window opener focus switching
    if (aWindow) {
      aWindow.focus();
    }

    try {
      let actor = aWindow
        ? aWindow.getWindowGlobalChild().getActor("SpecialPowers")
        : this;
      actor.sendAsyncMessage("SpecialPowers.Focus", {});
    } catch (e) {
      Cu.reportError(e);
    }
  }

  getClipboardData(flavor, whichClipboard) {
    if (whichClipboard === undefined) {
      whichClipboard = Services.clipboard.kGlobalClipboard;
    }

    var xferable = Cc["@mozilla.org/widget/transferable;1"].createInstance(
      Ci.nsITransferable
    );
    xferable.init(this.docShell);
    xferable.addDataFlavor(flavor);
    Services.clipboard.getData(xferable, whichClipboard);
    var data = {};
    try {
      xferable.getTransferData(flavor, data);
    } catch (e) {}
    data = data.value || null;
    if (data == null) {
      return "";
    }

    return data.QueryInterface(Ci.nsISupportsString).data;
  }

  clipboardCopyString(str) {
    Cc["@mozilla.org/widget/clipboardhelper;1"]
      .getService(Ci.nsIClipboardHelper)
      .copyString(str);
  }

  supportsSelectionClipboard() {
    return Services.clipboard.supportsSelectionClipboard();
  }

  swapFactoryRegistration(cid, contractID, newFactory) {
    newFactory = Cu.waiveXrays(newFactory);

    var componentRegistrar = Components.manager.QueryInterface(
      Ci.nsIComponentRegistrar
    );

    var currentCID = componentRegistrar.contractIDToCID(contractID);
    var currentFactory = Components.manager.getClassObject(
      Cc[contractID],
      Ci.nsIFactory
    );
    if (cid) {
      componentRegistrar.unregisterFactory(currentCID, currentFactory);
    } else {
      let uuidGenerator = Cc["@mozilla.org/uuid-generator;1"].getService(
        Ci.nsIUUIDGenerator
      );
      cid = uuidGenerator.generateUUID();
    }

    // Restore the original factory.
    componentRegistrar.registerFactory(cid, "", contractID, newFactory);
    return { originalCID: currentCID };
  }

  _getElement(aWindow, id) {
    return typeof id == "string" ? aWindow.document.getElementById(id) : id;
  }

  dispatchEvent(aWindow, target, event) {
    var el = this._getElement(aWindow, target);
    return el.dispatchEvent(event);
  }

  get isDebugBuild() {
    return Cc["@mozilla.org/xpcom/debug;1"].getService(Ci.nsIDebug2)
      .isDebugBuild;
  }
  assertionCount() {
    var debugsvc = Cc["@mozilla.org/xpcom/debug;1"].getService(Ci.nsIDebug2);
    return debugsvc.assertionCount;
  }

  /**
   * Get the message manager associated with an <iframe mozbrowser>.
   */
  getBrowserFrameMessageManager(aFrameElement) {
    return this.wrap(aFrameElement.frameLoader.messageManager);
  }

  _getPrincipalFromArg(arg) {
    let principal;
    let secMan = Services.scriptSecurityManager;

    if (typeof arg == "string") {
      // It's an URL.
      let uri = Services.io.newURI(arg);
      principal = secMan.createContentPrincipal(uri, {});
    } else if (arg.nodePrincipal) {
      // It's a document.
      // In some tests the arg is a wrapped DOM element, so we unwrap it first.
      principal = WrapPrivileged.unwrap(arg).nodePrincipal;
    } else {
      let uri = Services.io.newURI(arg.url);
      let attrs = arg.originAttributes || {};
      principal = secMan.createContentPrincipal(uri, attrs);
    }

    return principal;
  }

  async addPermission(type, allow, arg, expireType, expireTime) {
    let principal = this._getPrincipalFromArg(arg);
    if (principal.isSystemPrincipal) {
      return; // nothing to do
    }

    let permission;
    if (typeof allow !== "boolean") {
      permission = allow;
    } else {
      permission = allow
        ? Ci.nsIPermissionManager.ALLOW_ACTION
        : Ci.nsIPermissionManager.DENY_ACTION;
    }

    var msg = {
      op: "add",
      type,
      permission,
      principal,
      expireType: typeof expireType === "number" ? expireType : 0,
      expireTime: typeof expireTime === "number" ? expireTime : 0,
    };

    await this.sendQuery("SPPermissionManager", msg);
  }

  async removePermission(type, arg) {
    let principal = this._getPrincipalFromArg(arg);
    if (principal.isSystemPrincipal) {
      return; // nothing to do
    }

    var msg = {
      op: "remove",
      type,
      principal,
    };

    await this.sendQuery("SPPermissionManager", msg);
  }

  async hasPermission(type, arg) {
    let principal = this._getPrincipalFromArg(arg);
    if (principal.isSystemPrincipal) {
      return true; // system principals have all permissions
    }

    var msg = {
      op: "has",
      type,
      principal,
    };

    return this.sendQuery("SPPermissionManager", msg);
  }

  async testPermission(type, value, arg) {
    let principal = this._getPrincipalFromArg(arg);
    if (principal.isSystemPrincipal) {
      return true; // system principals have all permissions
    }

    var msg = {
      op: "test",
      type,
      value,
      principal,
    };
    return this.sendQuery("SPPermissionManager", msg);
  }

  isContentWindowPrivate(win) {
    return PrivateBrowsingUtils.isContentWindowPrivate(win);
  }

  async notifyObserversInParentProcess(subject, topic, data) {
    if (subject) {
      throw new Error("Can't send subject to another process!");
    }
    if (this.isMainProcess()) {
      this.notifyObservers(subject, topic, data);
      return;
    }
    var msg = {
      op: "notify",
      observerTopic: topic,
      observerData: data,
    };
    await this.sendQuery("SPObserverService", msg);
  }

  removeAllServiceWorkerData() {
    return this.sendQuery("SPRemoveAllServiceWorkers", {});
  }

  removeServiceWorkerDataForExampleDomain() {
    return this.sendQuery("SPRemoveServiceWorkerDataForExampleDomain", {});
  }

  cleanUpSTSData(origin, flags) {
    return this.sendQuery("SPCleanUpSTSData", { origin, flags: flags || 0 });
  }

  async requestDumpCoverageCounters(cb) {
    // We want to avoid a roundtrip between child and parent.
    if (!PerTestCoverageUtils.enabled) {
      return;
    }

    await this.sendQuery("SPRequestDumpCoverageCounters", {});
  }

  async requestResetCoverageCounters(cb) {
    // We want to avoid a roundtrip between child and parent.
    if (!PerTestCoverageUtils.enabled) {
      return;
    }
    await this.sendQuery("SPRequestResetCoverageCounters", {});
  }

  loadExtension(ext, handler) {
    if (this._extensionListeners == null) {
      this._extensionListeners = new Set();

      this._addMessageListener("SPExtensionMessage", msg => {
        for (let listener of this._extensionListeners) {
          try {
            listener(msg);
          } catch (e) {
            Cu.reportError(e);
          }
        }
      });
    }

    // Note, this is not the addon is as used by the AddonManager etc,
    // this is just an identifier used for specialpowers messaging
    // between this content process and the chrome process.
    let id = this._nextExtensionID++;

    handler = Cu.waiveXrays(handler);
    ext = Cu.waiveXrays(ext);

    let sp = this;
    let state = "uninitialized";
    let extension = {
      get state() {
        return state;
      },

      startup() {
        state = "pending";
        return sp.sendQuery("SPStartupExtension", { id }).then(
          () => {
            state = "running";
          },
          () => {
            state = "failed";
            sp._extensionListeners.delete(listener);
            return Promise.reject("startup failed");
          }
        );
      },

      unload() {
        state = "unloading";
        return sp.sendQuery("SPUnloadExtension", { id }).finally(() => {
          sp._extensionListeners.delete(listener);
          state = "unloaded";
        });
      },

      sendMessage(...args) {
        sp.sendAsyncMessage("SPExtensionMessage", { id, args });
      },
    };

    this.sendAsyncMessage("SPLoadExtension", { ext, id });

    let listener = msg => {
      if (msg.data.id == id) {
        if (msg.data.type == "extensionSetId") {
          extension.id = msg.data.args[0];
          extension.uuid = msg.data.args[1];
        } else if (msg.data.type in handler) {
          handler[msg.data.type](
            ...Cu.cloneInto(msg.data.args, this.contentWindow)
          );
        } else {
          dump(`Unexpected: ${msg.data.type}\n`);
        }
      }
    };

    this._extensionListeners.add(listener);
    return extension;
  }

  invalidateExtensionStorageCache() {
    this.notifyObserversInParentProcess(
      null,
      "extension-invalidate-storage-cache",
      ""
    );
  }

  allowMedia(window, enable) {
    window.docShell.allowMedia = enable;
  }

  createChromeCache(name, url) {
    let principal = this._getPrincipalFromArg(url);
    return WrapPrivileged.wrap(
      new this.contentWindow.CacheStorage(name, principal)
    );
  }

  loadChannelAndReturnStatus(url, loadUsingSystemPrincipal) {
    const BinaryInputStream = Components.Constructor(
      "@mozilla.org/binaryinputstream;1",
      "nsIBinaryInputStream",
      "setInputStream"
    );

    return new Promise(function(resolve) {
      let listener = {
        httpStatus: 0,

        onStartRequest(request) {
          request.QueryInterface(Ci.nsIHttpChannel);
          this.httpStatus = request.responseStatus;
        },

        onDataAvailable(request, stream, offset, count) {
          new BinaryInputStream(stream).readByteArray(count);
        },

        onStopRequest(request, status) {
          /* testing here that the redirect was not followed. If it was followed
            we would see a http status of 200 and status of NS_OK */

          let httpStatus = this.httpStatus;
          resolve({ status, httpStatus });
        },
      };
      let uri = NetUtil.newURI(url);
      let channel = NetUtil.newChannel({ uri, loadUsingSystemPrincipal });

      channel.loadFlags |= Ci.nsIChannel.LOAD_DOCUMENT_URI;
      channel.QueryInterface(Ci.nsIHttpChannelInternal);
      channel.documentURI = uri;
      channel.asyncOpen(listener);
    });
  }

  get ParserUtils() {
    if (this._pu != null) {
      return this._pu;
    }

    let pu = Cc["@mozilla.org/parserutils;1"].getService(Ci.nsIParserUtils);
    // We need to create and return our own wrapper.
    this._pu = {
      sanitize(src, flags) {
        return pu.sanitize(src, flags);
      },
      convertToPlainText(src, flags, wrapCol) {
        return pu.convertToPlainText(src, flags, wrapCol);
      },
      parseFragment(fragment, flags, isXML, baseURL, element) {
        let baseURI = baseURL ? NetUtil.newURI(baseURL) : null;
        return pu.parseFragment(
          WrapPrivileged.unwrap(fragment),
          flags,
          isXML,
          baseURI,
          WrapPrivileged.unwrap(element)
        );
      },
    };
    return this._pu;
  }

  createDOMWalker(node, showAnonymousContent) {
    node = WrapPrivileged.unwrap(node);
    let walker = Cc["@mozilla.org/inspector/deep-tree-walker;1"].createInstance(
      Ci.inIDeepTreeWalker
    );
    walker.showAnonymousContent = showAnonymousContent;
    walker.init(node.ownerDocument, NodeFilter.SHOW_ALL);
    walker.currentNode = node;
    return {
      get firstChild() {
        return WrapPrivileged.wrap(walker.firstChild());
      },
      get lastChild() {
        return WrapPrivileged.wrap(walker.lastChild());
      },
    };
  }

  observeMutationEvents(mo, node, nativeAnonymousChildList, subtree) {
    WrapPrivileged.unwrap(mo).observe(WrapPrivileged.unwrap(node), {
      nativeAnonymousChildList,
      subtree,
    });
  }

  doCommand(window, cmd) {
    return window.docShell.doCommand(cmd);
  }

  isCommandEnabled(window, cmd) {
    return window.docShell.isCommandEnabled(cmd);
  }

  setCommandNode(window, node) {
    return window.docShell.contentViewer
      .QueryInterface(Ci.nsIContentViewerEdit)
      .setCommandNode(node);
  }

  /* Bug 1339006 Runnables of nsIURIClassifier.classify may be labeled by
   * SystemGroup, but some test cases may run as web content. That would assert
   * when trying to enter web content from a runnable labeled by the
   * SystemGroup. To avoid that, we run classify from SpecialPowers which is
   * chrome-privileged and allowed to run inside SystemGroup
   */

  doUrlClassify(principal, eventTarget, callback) {
    let classifierService = Cc[
      "@mozilla.org/url-classifier/dbservice;1"
    ].getService(Ci.nsIURIClassifier);

    let wrapCallback = (...args) => {
      Services.tm.dispatchToMainThread(() => {
        if (typeof callback == "function") {
          callback(...args);
        } else {
          callback.onClassifyComplete.call(undefined, ...args);
        }
      });
    };

    return classifierService.classify(
      WrapPrivileged.unwrap(principal),
      eventTarget,
      wrapCallback
    );
  }

  // TODO: Bug 1353701 - Supports custom event target for labelling.
  doUrlClassifyLocal(uri, tables, callback) {
    let classifierService = Cc[
      "@mozilla.org/url-classifier/dbservice;1"
    ].getService(Ci.nsIURIClassifier);

    let wrapCallback = results => {
      Services.tm.dispatchToMainThread(() => {
        if (typeof callback == "function") {
          callback(WrapPrivileged.wrap(results));
        } else {
          callback.onClassifyComplete.call(
            undefined,
            WrapPrivileged.wrap(results)
          );
        }
      });
    };

    let feature = classifierService.createFeatureWithTables(
      "test",
      tables.split(","),
      []
    );
    return classifierService.asyncClassifyLocalWithFeatures(
      WrapPrivileged.unwrap(uri),
      [feature],
      Ci.nsIUrlClassifierFeature.blacklist,
      wrapCallback
    );
  }
}

SpecialPowersChild.prototype._proxiedObservers = {
  "specialpowers-http-notify-request": function(aMessage) {
    let uri = aMessage.json.uri;
    Services.obs.notifyObservers(
      null,
      "specialpowers-http-notify-request",
      uri
    );
  },

  "specialpowers-service-worker-shutdown": function(aMessage) {
    Services.obs.notifyObservers(null, "specialpowers-service-worker-shutdown");
  },
};

SpecialPowersChild.prototype.permissionObserverProxy = {
  // 'this' in permChangedObserverProxy is the permChangedObserverProxy
  // object itself. The '_specialPowersAPI' will be set to the 'SpecialPowersChild'
  // object to call the member function in SpecialPowersChild.
  _specialPowersAPI: null,
  observe(aSubject, aTopic, aData) {
    if (aTopic == "perm-changed") {
      var permission = aSubject.QueryInterface(Ci.nsIPermission);
      this._specialPowersAPI._permissionObserver.observe(permission, aData);
    }
  },
};

SpecialPowersChild.prototype._permissionObserver = {
  _self: null,
  _lastPermission: {},
  _callBack: null,
  _nextCallback: null,
  _obsDataMap: {
    deleted: "remove",
    added: "add",
  },
  observe(permission, aData) {
    if (this._self._applyingPermissions) {
      if (permission.type == this._lastPermission.type) {
        this._self._setTimeout(this._callback);
        this._self._setTimeout(this._nextCallback);
        this._callback = null;
        this._nextCallback = null;
      }
    } else {
      var found = false;
      for (
        var i = 0;
        !found && i < this._self._permissionsUndoStack.length;
        i++
      ) {
        var undos = this._self._permissionsUndoStack[i];
        for (var j = 0; j < undos.length; j++) {
          var undo = undos[j];
          if (
            undo.op == this._obsDataMap[aData] &&
            undo.type == permission.type
          ) {
            // Remove this undo item if it has been done by others(not
            // specialpowers itself.)
            undos.splice(j, 1);
            found = true;
            break;
          }
        }
        if (!undos.length) {
          // Remove the empty row in permissionsUndoStack
          this._self._permissionsUndoStack.splice(i, 1);
        }
      }
    }
  },
};

SpecialPowersChild.prototype.EARLY_BETA_OR_EARLIER =
  AppConstants.EARLY_BETA_OR_EARLIER;

// Due to an unfortunate accident of history, when this API was
// subclassed using `Thing.prototype = new SpecialPowersChild()`, existing
// code depends on all SpecialPowers instances using the same arrays for
// these.
Object.assign(SpecialPowersChild.prototype, {
  _permissionsUndoStack: [],
  _pendingPermissions: [],
});
