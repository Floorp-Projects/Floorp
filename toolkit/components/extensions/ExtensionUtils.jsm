/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["ExtensionUtils"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AppConstants",
                                  "resource://gre/modules/AppConstants.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "LanguageDetector",
                                  "resource:///modules/translation/LanguageDetector.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Locale",
                                  "resource://gre/modules/Locale.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Preferences",
                                  "resource://gre/modules/Preferences.jsm");

function filterStack(error) {
  return String(error.stack).replace(/(^.*(Task\.jsm|Promise-backend\.js).*\n)+/gm, "<Promise Chain>\n");
}

// Run a function and report exceptions.
function runSafeSyncWithoutClone(f, ...args) {
  try {
    return f(...args);
  } catch (e) {
    dump(`Extension error: ${e} ${e.fileName} ${e.lineNumber}\n[[Exception stack\n${filterStack(e)}Current stack\n${filterStack(Error())}]]\n`);
    Cu.reportError(e);
  }
}

// Run a function and report exceptions.
function runSafeWithoutClone(f, ...args) {
  if (typeof(f) != "function") {
    dump(`Extension error: expected function\n${filterStack(Error())}`);
    return;
  }

  Promise.resolve().then(() => {
    runSafeSyncWithoutClone(f, ...args);
  });
}

// Run a function, cloning arguments into context.cloneScope, and
// report exceptions. |f| is expected to be in context.cloneScope.
function runSafeSync(context, f, ...args) {
  try {
    args = Cu.cloneInto(args, context.cloneScope);
  } catch (e) {
    Cu.reportError(e);
    dump(`runSafe failure: cloning into ${context.cloneScope}: ${e}\n\n${filterStack(Error())}`);
  }
  return runSafeSyncWithoutClone(f, ...args);
}

// Run a function, cloning arguments into context.cloneScope, and
// report exceptions. |f| is expected to be in context.cloneScope.
function runSafe(context, f, ...args) {
  try {
    args = Cu.cloneInto(args, context.cloneScope);
  } catch (e) {
    Cu.reportError(e);
    dump(`runSafe failure: cloning into ${context.cloneScope}: ${e}\n\n${filterStack(Error())}`);
  }
  return runSafeWithoutClone(f, ...args);
}

// Return true if the given value is an instance of the given
// native type.
function instanceOf(value, type) {
  return {}.toString.call(value) == `[object ${type}]`;
}

// Extend the object |obj| with the property descriptors of each object in
// |args|.
function extend(obj, ...args) {
  for (let arg of args) {
    let props = [...Object.getOwnPropertyNames(arg),
                 ...Object.getOwnPropertySymbols(arg)];
    for (let prop of props) {
      let descriptor = Object.getOwnPropertyDescriptor(arg, prop);
      Object.defineProperty(obj, prop, descriptor);
    }
  }

  return obj;
}

// Similar to a WeakMap, but returns a particular default value for
// |get| if a key is not present.
function DefaultWeakMap(defaultValue) {
  this.defaultValue = defaultValue;
  this.weakmap = new WeakMap();
}

DefaultWeakMap.prototype = {
  get(key) {
    if (this.weakmap.has(key)) {
      return this.weakmap.get(key);
    }
    return this.defaultValue;
  },

  set(key, value) {
    if (key) {
      this.weakmap.set(key, value);
    } else {
      this.defaultValue = value;
    }
  },
};

class SpreadArgs extends Array {
  constructor(args) {
    super();
    this.push(...args);
  }
}

class BaseContext {
  constructor() {
    this.onClose = new Set();
    this.checkedLastError = false;
    this._lastError = null;
  }

  get cloneScope() {
    throw new Error("Not implemented");
  }

  get principal() {
    throw new Error("Not implemented");
  }

  checkLoadURL(url, options = {}) {
    let ssm = Services.scriptSecurityManager;

    let flags = ssm.STANDARD;
    if (!options.allowScript) {
      flags |= ssm.DISALLOW_SCRIPT;
    }
    if (!options.allowInheritsPrincipal) {
      flags |= ssm.DISALLOW_INHERIT_PRINCIPAL;
    }
    if (options.dontReportErrors) {
      flags |= ssm.DONT_REPORT_ERRORS;
    }

    try {
      ssm.checkLoadURIStrWithPrincipal(this.principal, url, flags);
    } catch (e) {
      return false;
    }
    return true;
  }

  callOnClose(obj) {
    this.onClose.add(obj);
  }

  forgetOnClose(obj) {
    this.onClose.delete(obj);
  }

  get lastError() {
    this.checkedLastError = true;
    return this._lastError;
  }

  set lastError(val) {
    this.checkedLastError = false;
    this._lastError = val;
  }

  /**
   * Sets the value of `.lastError` to `error`, calls the given
   * callback, and reports an error if the value has not been checked
   * when the callback returns.
   *
   * @param {object} error An object with a `message` property. May
   *     optionally be an `Error` object belonging to the target scope.
   * @param {function} callback The callback to call.
   * @returns {*} The return value of callback.
   */
  withLastError(error, callback) {
    if (!(error instanceof this.cloneScope.Error)) {
      error = new this.cloneScope.Error(error.message);
    }
    this.lastError = error;
    try {
      return callback();
    } finally {
      if (!this.checkedLastError) {
        Cu.reportError(`Unchecked lastError value: ${error}`);
      }
      this.lastError = null;
    }
  }

  /**
   * Wraps the given promise so it can be safely returned to extension
   * code in this context.
   *
   * If `callback` is provided, however, it is used as a completion
   * function for the promise, and no promise is returned. In this case,
   * the callback is called when the promise resolves or rejects. In the
   * latter case, `lastError` is set to the rejection value, and the
   * callback function must check `browser.runtime.lastError` or
   * `extension.runtime.lastError` in order to prevent it being reported
   * to the console.
   *
   * @param {Promise} promise The promise with which to wrap the
   *     callback. May resolve to a `SpreadArgs` instance, in which case
   *     each element will be used as a separate argument.
   *
   *     Unless the promise object belongs to the cloneScope global, its
   *     resolution value is cloned into cloneScope prior to calling the
   *     `callback` function or resolving the wrapped promise.
   *
   * @param {function} [callback] The callback function to wrap
   *
   * @returns {Promise|undefined} If callback is null, a promise object
   *     belonging to the target scope. Otherwise, undefined.
   */
  wrapPromise(promise, callback = null) {
    // Note: `promise instanceof this.cloneScope.Promise` returns true
    // here even for promises that do not belong to the content scope.
    let runSafe = runSafeSync.bind(null, this);
    if (promise.constructor === this.cloneScope.Promise) {
      runSafe = runSafeSyncWithoutClone;
    }

    if (callback) {
      promise.then(
        args => {
          if (args instanceof SpreadArgs) {
            runSafe(callback, ...args);
          } else {
            runSafe(callback, args);
          }
        },
        error => {
          this.withLastError(error, () => {
            runSafeSyncWithoutClone(callback);
          });
        });
    } else {
      return new this.cloneScope.Promise((resolve, reject) => {
        promise.then(
          value => { runSafe(resolve, value); },
          value => {
            if (!(value instanceof this.cloneScope.Error)) {
              value = new this.cloneScope.Error(value.message);
            }
            runSafeSyncWithoutClone(reject, value);
          });
      });
    }
  }

  unload() {
    for (let obj of this.onClose) {
      obj.close();
    }
  }
}

function LocaleData(data) {
  this.defaultLocale = data.defaultLocale;
  this.selectedLocale = data.selectedLocale;
  this.locales = data.locales || new Map();

  // Map(locale-name -> Map(message-key -> localized-string))
  //
  // Contains a key for each loaded locale, each of which is a
  // Map of message keys to their localized strings.
  this.messages = data.messages || new Map();

  if (data.builtinMessages) {
    this.messages.set(this.BUILTIN, data.builtinMessages);
  }
}

LocaleData.prototype = {
  // Representation of the object to send to content processes. This
  // should include anything the content process might need.
  serialize() {
    return {
      defaultLocale: this.defaultLocale,
      selectedLocale: this.selectedLocale,
      messages: this.messages,
      locales: this.locales,
    };
  },

  BUILTIN: "@@BUILTIN_MESSAGES",

  has(locale) {
    return this.messages.has(locale);
  },

  // https://developer.chrome.com/extensions/i18n
  localizeMessage(message, substitutions = [], locale = this.selectedLocale, defaultValue = "??") {
    let locales = new Set([this.BUILTIN, locale, this.defaultLocale]
                          .filter(locale => this.messages.has(locale)));

    // Message names are case-insensitive, so normalize them to lower-case.
    message = message.toLowerCase();
    for (let locale of locales) {
      let messages = this.messages.get(locale);
      if (messages.has(message)) {
        let str = messages.get(message);

        if (!Array.isArray(substitutions)) {
          substitutions = [substitutions];
        }

        let replacer = (matched, index, dollarSigns) => {
          if (index) {
            // This is not quite Chrome-compatible. Chrome consumes any number
            // of digits following the $, but only accepts 9 substitutions. We
            // accept any number of substitutions.
            index = parseInt(index, 10) - 1;
            return index in substitutions ? substitutions[index] : "";
          } else {
            // For any series of contiguous `$`s, the first is dropped, and
            // the rest remain in the output string.
            return dollarSigns;
          }
        };
        return str.replace(/\$(?:([1-9]\d*)|(\$+))/g, replacer);
      }
    }

    // Check for certain pre-defined messages.
    if (message == "@@ui_locale") {
      return this.uiLocale;
    } else if (message.startsWith("@@bidi_")) {
      let registry = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(Ci.nsIXULChromeRegistry);
      let rtl = registry.isLocaleRTL("global");

      if (message == "@@bidi_dir") {
        return rtl ? "rtl" : "ltr";
      } else if (message == "@@bidi_reversed_dir") {
        return rtl ? "ltr" : "rtl";
      } else if (message == "@@bidi_start_edge") {
        return rtl ? "right" : "left";
      } else if (message == "@@bidi_end_edge") {
        return rtl ? "left" : "right";
      }
    }

    Cu.reportError(`Unknown localization message ${message}`);
    return defaultValue;
  },

  // Localize a string, replacing all |__MSG_(.*)__| tokens with the
  // matching string from the current locale, as determined by
  // |this.selectedLocale|.
  //
  // This may not be called before calling either |initLocale| or
  // |initAllLocales|.
  localize(str, locale = this.selectedLocale) {
    if (!str) {
      return str;
    }

    return str.replace(/__MSG_([A-Za-z0-9@_]+?)__/g, (matched, message) => {
      return this.localizeMessage(message, [], locale, matched);
    });
  },

  // Validates the contents of a locale JSON file, normalizes the
  // messages into a Map of message key -> localized string pairs.
  addLocale(locale, messages, extension) {
    let result = new Map();

    // Chrome does not document the semantics of its localization
    // system very well. It handles replacements by pre-processing
    // messages, replacing |$[a-zA-Z0-9@_]+$| tokens with the value of their
    // replacements. Later, it processes the resulting string for
    // |$[0-9]| replacements.
    //
    // Again, it does not document this, but it accepts any number
    // of sequential |$|s, and replaces them with that number minus
    // 1. It also accepts |$| followed by any number of sequential
    // digits, but refuses to process a localized string which
    // provides more than 9 substitutions.
    if (!instanceOf(messages, "Object")) {
      extension.packagingError(`Invalid locale data for ${locale}`);
      return result;
    }

    for (let key of Object.keys(messages)) {
      let msg = messages[key];

      if (!instanceOf(msg, "Object") || typeof(msg.message) != "string") {
        extension.packagingError(`Invalid locale message data for ${locale}, message ${JSON.stringify(key)}`);
        continue;
      }

      // Substitutions are case-insensitive, so normalize all of their names
      // to lower-case.
      let placeholders = new Map();
      if (instanceOf(msg.placeholders, "Object")) {
        for (let key of Object.keys(msg.placeholders)) {
          placeholders.set(key.toLowerCase(), msg.placeholders[key]);
        }
      }

      let replacer = (match, name) => {
        let replacement = placeholders.get(name.toLowerCase());
        if (instanceOf(replacement, "Object") && "content" in replacement) {
          return replacement.content;
        }
        return "";
      };

      let value = msg.message.replace(/\$([A-Za-z0-9@_]+)\$/g, replacer);

      // Message names are also case-insensitive, so normalize them to lower-case.
      result.set(key.toLowerCase(), value);
    }

    this.messages.set(locale, result);
    return result;
  },

  get acceptLanguages() {
    let result = Preferences.get("intl.accept_languages", "", Ci.nsIPrefLocalizedString);
    return result.split(/\s*,\s*/g);
  },


  get uiLocale() {
    // Return the browser locale, but convert it to a Chrome-style
    // locale code.
    return Locale.getLocale().replace(/-/g, "_");
  },
};

// This is a generic class for managing event listeners. Example usage:
//
// new EventManager(context, "api.subAPI", fire => {
//   let listener = (...) => {
//     // Fire any listeners registered with addListener.
//     fire(arg1, arg2);
//   };
//   // Register the listener.
//   SomehowRegisterListener(listener);
//   return () => {
//     // Return a way to unregister the listener.
//     SomehowUnregisterListener(listener);
//   };
// }).api()
//
// The result is an object with addListener, removeListener, and
// hasListener methods. |context| is an add-on scope (either an
// ExtensionPage in the chrome process or ExtensionContext in a
// content process). |name| is for debugging. |register| is a function
// to register the listener. |register| is only called once, event if
// multiple listeners are registered. |register| should return an
// unregister function that will unregister the listener.
function EventManager(context, name, register) {
  this.context = context;
  this.name = name;
  this.register = register;
  this.unregister = null;
  this.callbacks = new Set();
}

EventManager.prototype = {
  addListener(callback) {
    if (typeof(callback) != "function") {
      dump(`Expected function\n${Error().stack}`);
      return;
    }

    if (!this.callbacks.size) {
      this.context.callOnClose(this);

      let fireFunc = this.fire.bind(this);
      let fireWithoutClone = this.fireWithoutClone.bind(this);
      fireFunc.withoutClone = fireWithoutClone;
      this.unregister = this.register(fireFunc);
    }
    this.callbacks.add(callback);
  },

  removeListener(callback) {
    if (!this.callbacks.size) {
      return;
    }

    this.callbacks.delete(callback);
    if (this.callbacks.size == 0) {
      this.unregister();

      this.context.forgetOnClose(this);
    }
  },

  hasListener(callback) {
    return this.callbacks.has(callback);
  },

  fire(...args) {
    for (let callback of this.callbacks) {
      runSafe(this.context, callback, ...args);
    }
  },

  fireWithoutClone(...args) {
    for (let callback of this.callbacks) {
      runSafeSyncWithoutClone(callback, ...args);
    }
  },

  close() {
    if (this.callbacks.size) {
      this.unregister();
    }
    this.callbacks = null;
  },

  api() {
    return {
      addListener: callback => this.addListener(callback),
      removeListener: callback => this.removeListener(callback),
      hasListener: callback => this.hasListener(callback),
    };
  },
};

// Similar to EventManager, but it doesn't try to consolidate event
// notifications. Each addListener call causes us to register once. It
// allows extra arguments to be passed to addListener.
function SingletonEventManager(context, name, register) {
  this.context = context;
  this.name = name;
  this.register = register;
  this.unregister = new Map();
  context.callOnClose(this);
}

SingletonEventManager.prototype = {
  addListener(callback, ...args) {
    let unregister = this.register(callback, ...args);
    this.unregister.set(callback, unregister);
  },

  removeListener(callback) {
    if (!this.unregister.has(callback)) {
      return;
    }

    let unregister = this.unregister.get(callback);
    this.unregister.delete(callback);
    unregister();
  },

  hasListener(callback) {
    return this.unregister.has(callback);
  },

  close() {
    for (let unregister of this.unregister.values()) {
      unregister();
    }
  },

  api() {
    return {
      addListener: (...args) => this.addListener(...args),
      removeListener: (...args) => this.removeListener(...args),
      hasListener: (...args) => this.hasListener(...args),
    };
  },
};

// Simple API for event listeners where events never fire.
function ignoreEvent(context, name) {
  return {
    addListener: function(callback) {
      let id = context.extension.id;
      let frame = Components.stack.caller;
      let msg = `In add-on ${id}, attempting to use listener "${name}", which is unimplemented.`;
      let winID = context.contentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID;
      let scriptError = Cc["@mozilla.org/scripterror;1"]
        .createInstance(Ci.nsIScriptError);
      scriptError.initWithWindowID(msg, frame.filename, null,
                                   frame.lineNumber, frame.columnNumber,
                                   Ci.nsIScriptError.warningFlag,
                                   "content javascript", winID);
      let consoleService = Cc["@mozilla.org/consoleservice;1"]
        .getService(Ci.nsIConsoleService);
      consoleService.logMessage(scriptError);
    },
    removeListener: function(callback) {},
    hasListener: function(callback) {},
  };
}

// Copy an API object from |source| into the scope |dest|.
function injectAPI(source, dest) {
  for (let prop in source) {
    // Skip names prefixed with '_'.
    if (prop[0] == "_") {
      continue;
    }

    let desc = Object.getOwnPropertyDescriptor(source, prop);
    if (typeof(desc.value) == "function") {
      Cu.exportFunction(desc.value, dest, {defineAs: prop});
    } else if (typeof(desc.value) == "object") {
      let obj = Cu.createObjectIn(dest, {defineAs: prop});
      injectAPI(desc.value, obj);
    } else {
      Object.defineProperty(dest, prop, desc);
    }
  }
}

/**
 * Returns a Promise which resolves when the given document's DOM has
 * fully loaded.
 *
 * @param {Document} doc The document to await the load of.
 * @returns {Promise<Document>}
 */
function promiseDocumentReady(doc) {
  if (doc.readyState == "interactive" || doc.readyState == "complete") {
    return Promise.resolve(doc);
  }

  return new Promise(resolve => {
    doc.addEventListener("DOMContentLoaded", function onReady(event) {
      if (event.target === event.currentTarget) {
        doc.removeEventListener("DOMContentLoaded", onReady, true);
        resolve(doc);
      }
    }, true);
  });
}

/*
 * Messaging primitives.
 */

var nextBrokerId = 1;

var MESSAGES = [
  "Extension:Message",
  "Extension:Connect",
];

// Receives messages from multiple message managers and directs them
// to a set of listeners. On the child side: one broker per frame
// script.  On the parent side: one broker total, covering both the
// global MM and the ppmm. Message must be tagged with a recipient,
// which is an object with properties. Listeners can filter for
// messages that have a certain value for a particular property in the
// recipient. (If a message doesn't specify the given property, it's
// considered a match.)
function MessageBroker(messageManagers) {
  this.messageManagers = messageManagers;
  for (let mm of this.messageManagers) {
    for (let message of MESSAGES) {
      mm.addMessageListener(message, this);
    }
  }

  this.listeners = {message: [], connect: []};
}

MessageBroker.prototype = {
  uninit() {
    for (let mm of this.messageManagers) {
      for (let message of MESSAGES) {
        mm.removeMessageListener(message, this);
      }
    }

    this.listeners = null;
  },

  makeId() {
    return nextBrokerId++;
  },

  addListener(type, listener, filter) {
    this.listeners[type].push({filter, listener});
  },

  removeListener(type, listener) {
    for (let i = 0; i < this.listeners[type].length; i++) {
      if (this.listeners[type][i].listener == listener) {
        this.listeners[type].splice(i, 1);
        return;
      }
    }
  },

  runListeners(type, target, data) {
    let listeners = [];
    for (let {listener, filter} of this.listeners[type]) {
      let pass = true;
      for (let prop in filter) {
        if (prop in data.recipient && filter[prop] != data.recipient[prop]) {
          pass = false;
          break;
        }
      }

      // Save up the list of listeners to call in case they modify the
      // set of listeners.
      if (pass) {
        listeners.push(listener);
      }
    }

    for (let listener of listeners) {
      listener(type, target, data.message, data.sender, data.recipient);
    }
  },

  receiveMessage({name, data, target}) {
    switch (name) {
      case "Extension:Message":
        this.runListeners("message", target, data);
        break;

      case "Extension:Connect":
        this.runListeners("connect", target, data);
        break;
    }
  },

  sendMessage(messageManager, type, message, sender, recipient) {
    let data = {message, sender, recipient};
    let names = {message: "Extension:Message", connect: "Extension:Connect"};
    messageManager.sendAsyncMessage(names[type], data);
  },
};

// Abstraction for a Port object in the extension API. Each port has a unique ID.
function Port(context, messageManager, name, id, sender) {
  this.context = context;
  this.messageManager = messageManager;
  this.name = name;
  this.id = id;
  this.listenerName = `Extension:Port-${this.id}`;
  this.disconnectName = `Extension:Disconnect-${this.id}`;
  this.sender = sender;
  this.disconnected = false;

  this.messageManager.addMessageListener(this.disconnectName, this, true);
  this.disconnectListeners = new Set();
}

Port.prototype = {
  api() {
    let portObj = Cu.createObjectIn(this.context.cloneScope);

    // We want a close() notification when the window is destroyed.
    this.context.callOnClose(this);

    let publicAPI = {
      name: this.name,
      disconnect: () => {
        this.disconnect();
      },
      postMessage: json => {
        if (this.disconnected) {
          throw new this.context.contentWindow.Error("Attempt to postMessage on disconnected port");
        }
        this.messageManager.sendAsyncMessage(this.listenerName, json);
      },
      onDisconnect: new EventManager(this.context, "Port.onDisconnect", fire => {
        let listener = () => {
          if (!this.disconnected) {
            fire();
          }
        };

        this.disconnectListeners.add(listener);
        return () => {
          this.disconnectListeners.delete(listener);
        };
      }).api(),
      onMessage: new EventManager(this.context, "Port.onMessage", fire => {
        let listener = ({data}) => {
          if (!this.disconnected) {
            fire(data);
          }
        };

        this.messageManager.addMessageListener(this.listenerName, listener);
        return () => {
          this.messageManager.removeMessageListener(this.listenerName, listener);
        };
      }).api(),
    };

    if (this.sender) {
      publicAPI.sender = this.sender;
    }

    injectAPI(publicAPI, portObj);
    return portObj;
  },

  handleDisconnection() {
    this.messageManager.removeMessageListener(this.disconnectName, this);
    this.context.forgetOnClose(this);
    this.disconnected = true;
  },

  receiveMessage(msg) {
    if (msg.name == this.disconnectName) {
      if (this.disconnected) {
        return;
      }

      for (let listener of this.disconnectListeners) {
        listener();
      }

      this.handleDisconnection();
    }
  },

  disconnect() {
    if (this.disconnected) {
      throw new this.context.contentWindow.Error("Attempt to disconnect() a disconnected port");
    }
    this.handleDisconnection();
    this.messageManager.sendAsyncMessage(this.disconnectName);
  },

  close() {
    this.disconnect();
  },
};

function getMessageManager(target) {
  if (target instanceof Ci.nsIFrameLoaderOwner) {
    return target.QueryInterface(Ci.nsIFrameLoaderOwner).frameLoader.messageManager;
  }
  return target;
}

// Each extension scope gets its own Messenger object. It handles the
// basics of sendMessage, onMessage, connect, and onConnect.
//
// |context| is the extension scope.
// |broker| is a MessageBroker used to receive and send messages.
// |sender| is an object describing the sender (usually giving its extension id, tabId, etc.)
// |filter| is a recipient filter to apply to incoming messages from the broker.
// |delegate| is an object that must implement a few methods:
//    getSender(context, messageManagerTarget, sender): returns a MessageSender
//      See https://developer.chrome.com/extensions/runtime#type-MessageSender.
function Messenger(context, broker, sender, filter, delegate) {
  this.context = context;
  this.broker = broker;
  this.sender = sender;
  this.filter = filter;
  this.delegate = delegate;
}

Messenger.prototype = {
  sendMessage(messageManager, msg, recipient, responseCallback) {
    let id = this.broker.makeId();
    let replyName = `Extension:Reply-${id}`;
    recipient.messageId = id;
    this.broker.sendMessage(messageManager, "message", msg, this.sender, recipient);

    let promise = new Promise((resolve, reject) => {
      let onClose;
      let listener = ({data: response}) => {
        messageManager.removeMessageListener(replyName, listener);
        this.context.forgetOnClose(onClose);

        if (response.gotData) {
          resolve(response.data);
        } else if (response.error) {
          reject(response.error);
        } else if (!responseCallback) {
          // As a special case, we don't call the callback variant if we
          // receive no response, but the promise needs to resolve or
          // reject in either case.
          resolve();
        }
      };
      onClose = {
        close() {
          messageManager.removeMessageListener(replyName, listener);
        },
      };

      messageManager.addMessageListener(replyName, listener);
      this.context.callOnClose(onClose);
    });

    return this.context.wrapPromise(promise, responseCallback);
  },

  onMessage(name) {
    return new SingletonEventManager(this.context, name, callback => {
      let listener = (type, target, message, sender, recipient) => {
        message = Cu.cloneInto(message, this.context.cloneScope);
        if (this.delegate) {
          this.delegate.getSender(this.context, target, sender);
        }
        sender = Cu.cloneInto(sender, this.context.cloneScope);

        let mm = getMessageManager(target);
        let replyName = `Extension:Reply-${recipient.messageId}`;

        new Promise((resolve, reject) => {
          let sendResponse = Cu.exportFunction(resolve, this.context.cloneScope);

          // Note: We intentionally do not use runSafe here so that any
          // errors are propagated to the message sender.
          let result = callback(message, sender, sendResponse);
          if (result instanceof Promise) {
            resolve(result);
          } else if (result !== true) {
            reject();
          }
        }).then(
          data => {
            mm.sendAsyncMessage(replyName, {data, gotData: true});
          },
          error => {
            if (error) {
              // The result needs to be structured-clonable, which
              // ordinary Error objects are not.
              try {
                error = {message: String(error.message), stack: String(error.stack)};
              } catch (e) {
                error = {message: String(error)};
              }
            }
            mm.sendAsyncMessage(replyName, {error, gotData: false});
          });
      };

      this.broker.addListener("message", listener, this.filter);
      return () => {
        this.broker.removeListener("message", listener);
      };
    }).api();
  },

  connect(messageManager, name, recipient) {
    let portId = this.broker.makeId();
    let port = new Port(this.context, messageManager, name, portId, null);
    let msg = {name, portId};
    this.broker.sendMessage(messageManager, "connect", msg, this.sender, recipient);
    return port.api();
  },

  onConnect(name) {
    return new EventManager(this.context, name, fire => {
      let listener = (type, target, message, sender, recipient) => {
        let {name, portId} = message;
        let mm = getMessageManager(target);
        if (this.delegate) {
          this.delegate.getSender(this.context, target, sender);
        }
        let port = new Port(this.context, mm, name, portId, sender);
        fire.withoutClone(port.api());
      };

      this.broker.addListener("connect", listener, this.filter);
      return () => {
        this.broker.removeListener("connect", listener);
      };
    }).api();
  },
};

function flushJarCache(jarFile) {
  Services.obs.notifyObservers(jarFile, "flush-cache-entry", null);
}

const PlatformInfo = Object.freeze({
  os: (function() {
    let os = AppConstants.platform;
    if (os == "macosx") {
      os = "mac";
    }
    return os;
  })(),
  arch: (function() {
    let abi = Services.appinfo.XPCOMABI;
    let [arch] = abi.split("-");
    if (arch == "x86") {
      arch = "x86-32";
    } else if (arch == "x86_64") {
      arch = "x86-64";
    }
    return arch;
  })(),
});

function detectLanguage(text) {
  return LanguageDetector.detectLanguage(text).then(result => ({
    isReliable: result.confident,
    languages: result.languages.map(lang => {
      return {
        language: lang.languageCode,
        percentage: lang.percent,
      };
    }),
  }));
}

this.ExtensionUtils = {
  detectLanguage,
  extend,
  flushJarCache,
  ignoreEvent,
  injectAPI,
  instanceOf,
  promiseDocumentReady,
  runSafe,
  runSafeSync,
  runSafeSyncWithoutClone,
  runSafeWithoutClone,
  BaseContext,
  DefaultWeakMap,
  EventManager,
  LocaleData,
  MessageBroker,
  Messenger,
  PlatformInfo,
  SingletonEventManager,
  SpreadArgs,
};
