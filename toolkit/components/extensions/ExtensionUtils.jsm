/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["ExtensionUtils"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const Cr = Components.results;

const INTEGER = /^[1-9]\d*$/;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AppConstants",
                                  "resource://gre/modules/AppConstants.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ConsoleAPI",
                                  "resource://gre/modules/Console.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "LanguageDetector",
                                  "resource:///modules/translation/LanguageDetector.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Locale",
                                  "resource://gre/modules/Locale.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "MessageChannel",
                                  "resource://gre/modules/MessageChannel.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Preferences",
                                  "resource://gre/modules/Preferences.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PromiseUtils",
                                  "resource://gre/modules/PromiseUtils.jsm");

function getConsole() {
  return new ConsoleAPI({
    maxLogLevelPref: "extensions.webextensions.log.level",
    prefix: "WebExtensions",
  });
}

XPCOMUtils.defineLazyGetter(this, "console", getConsole);

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
  if (context.unloaded) {
    Cu.reportError("runSafeSync called after context unloaded");
    return;
  }

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
  if (context.unloaded) {
    dump(`runSafe failure: context is already unloaded ${filterStack(new Error())}\n`);
    return undefined;
  }
  return runSafeWithoutClone(f, ...args);
}

function getInnerWindowID(window) {
  return window.QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIDOMWindowUtils)
    .currentInnerWindowID;
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

let gContextId = 0;

class BaseContext {
  constructor(envType, extension) {
    this.envType = envType;
    this.onClose = new Set();
    this.checkedLastError = false;
    this._lastError = null;
    this.contextId = `${++gContextId}-${Services.appinfo.uniqueProcessID}`;
    this.unloaded = false;
    this.extension = extension;
    this.jsonSandbox = null;
    this.active = true;

    this.docShell = null;
    this.contentWindow = null;
    this.innerWindowID = 0;
  }

  setContentWindow(contentWindow) {
    let {document} = contentWindow;
    let docShell = contentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                                .getInterface(Ci.nsIDocShell);

    this.innerWindowID = getInnerWindowID(contentWindow);

    let onPageShow = event => {
      if (!event || event.target === document) {
        this.docShell = docShell;
        this.contentWindow = contentWindow;
        this.active = true;
      }
    };
    let onPageHide = event => {
      if (!event || event.target === document) {
        // Put this off until the next tick.
        Promise.resolve().then(() => {
          this.docShell = null;
          this.contentWindow = null;
          this.active = false;
        });
      }
    };

    onPageShow();
    contentWindow.addEventListener("pagehide", onPageHide, true);
    contentWindow.addEventListener("pageshow", onPageShow, true);
    this.callOnClose({
      close: () => {
        onPageHide();
        if (this.active) {
          contentWindow.removeEventListener("pagehide", onPageHide, true);
          contentWindow.removeEventListener("pageshow", onPageShow, true);
        }
      },
    });
  }

  get cloneScope() {
    throw new Error("Not implemented");
  }

  get principal() {
    throw new Error("Not implemented");
  }

  runSafe(...args) {
    if (this.unloaded) {
      Cu.reportError("context.runSafe called after context unloaded");
    } else if (!this.active) {
      Cu.reportError("context.runSafe called while context is inactive");
    } else {
      return runSafeSync(this, ...args);
    }
  }

  runSafeWithoutClone(...args) {
    if (this.unloaded) {
      Cu.reportError("context.runSafeWithoutClone called after context unloaded");
    } else if (!this.active) {
      Cu.reportError("context.runSafeWithoutClone called while context is inactive");
    } else {
      return runSafeSyncWithoutClone(...args);
    }
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

  /**
   * Safely call JSON.stringify() on an object that comes from an
   * extension.
   *
   * @param {array<any>} args Arguments for JSON.stringify()
   * @returns {string} The stringified representation of obj
   */
  jsonStringify(...args) {
    if (!this.jsonSandbox) {
      this.jsonSandbox = Cu.Sandbox(this.principal, {
        sameZoneAs: this.cloneScope,
        wantXrays: false,
      });
    }

    return Cu.waiveXrays(this.jsonSandbox.JSON).stringify(...args);
  }

  callOnClose(obj) {
    this.onClose.add(obj);
  }

  forgetOnClose(obj) {
    this.onClose.delete(obj);
  }

  /**
   * A wrapper around MessageChannel.sendMessage which adds the extension ID
   * to the recipient object, and ensures replies are not processed after the
   * context has been unloaded.
   *
   * @param {nsIMessageManager} target
   * @param {string} messageName
   * @param {object} data
   * @param {object} [options]
   * @param {object} [options.sender]
   * @param {object} [options.recipient]
   *
   * @returns {Promise}
   */
  sendMessage(target, messageName, data, options = {}) {
    options.recipient = options.recipient || {};
    options.sender = options.sender || {};

    options.recipient.extensionId = this.extension.id;
    options.sender.extensionId = this.extension.id;
    options.sender.contextId = this.contextId;

    return MessageChannel.sendMessage(target, messageName, data, options);
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
   * Normalizes the given error object for use by the target scope. If
   * the target is an error object which belongs to that scope, it is
   * returned as-is. If it is an ordinary object with a `message`
   * property, it is converted into an error belonging to the target
   * scope. If it is an Error object which does *not* belong to the
   * clone scope, it is reported, and converted to an unexpected
   * exception error.
   *
   * @param {Error|object} error
   * @returns {Error}
   */
  normalizeError(error) {
    if (error instanceof this.cloneScope.Error) {
      return error;
    }
    if (!instanceOf(error, "Object")) {
      Cu.reportError(error);
      error = {message: "An unexpected error occurred"};
    }
    return new this.cloneScope.Error(error.message);
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
    this.lastError = this.normalizeError(error);
    try {
      return callback();
    } finally {
      if (!this.checkedLastError) {
        Cu.reportError(`Unchecked lastError value: ${this.lastError}`);
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
    let runSafe = this.runSafe.bind(this);
    if (promise instanceof this.cloneScope.Promise) {
      runSafe = this.runSafeWithoutClone.bind(this);
    }

    if (callback) {
      promise.then(
        args => {
          if (this.unloaded) {
            dump(`Promise resolved after context unloaded\n`);
          } else if (!this.active) {
            dump(`Promise resolved while context is inactive\n`);
          } else if (args instanceof SpreadArgs) {
            runSafe(callback, ...args);
          } else {
            runSafe(callback, args);
          }
        },
        error => {
          this.withLastError(error, () => {
            if (this.unloaded) {
              dump(`Promise rejected after context unloaded\n`);
            } else if (!this.active) {
              dump(`Promise rejected while context is inactive\n`);
            } else {
              this.runSafeWithoutClone(callback);
            }
          });
        });
    } else {
      return new this.cloneScope.Promise((resolve, reject) => {
        promise.then(
          value => {
            if (this.unloaded) {
              dump(`Promise resolved after context unloaded\n`);
            } else if (!this.active) {
              dump(`Promise resolved while context is inactive\n`);
            } else {
              runSafe(resolve, value);
            }
          },
          value => {
            if (this.unloaded) {
              dump(`Promise rejected after context unloaded: ${value && value.message}\n`);
            } else if (!this.active) {
              dump(`Promise rejected while context is inactive: ${value && value.message}\n`);
            } else {
              this.runSafeWithoutClone(reject, this.normalizeError(value));
            }
          });
      });
    }
  }

  unload() {
    this.unloaded = true;

    MessageChannel.abortResponses({
      extensionId: this.extension.id,
      contextId: this.contextId,
    });

    for (let obj of this.onClose) {
      obj.close();
    }
  }
}

// Manages icon details for toolbar buttons in the |pageAction| and
// |browserAction| APIs.
let IconDetails = {
  // Normalizes the various acceptable input formats into an object
  // with icon size as key and icon URL as value.
  //
  // If a context is specified (function is called from an extension):
  // Throws an error if an invalid icon size was provided or the
  // extension is not allowed to load the specified resources.
  //
  // If no context is specified, instead of throwing an error, this
  // function simply logs a warning message.
  normalize(details, extension, context = null) {
    let result = {};

    try {
      if (details.imageData) {
        let imageData = details.imageData;

        // The global might actually be from Schema.jsm, which
        // normalizes most of our arguments. In that case it won't have
        // an ImageData property. But Schema.jsm doesn't normalize
        // actual ImageData objects, so they will come from a global
        // with the right property.
        if (instanceOf(imageData, "ImageData")) {
          imageData = {"19": imageData};
        }

        for (let size of Object.keys(imageData)) {
          if (!INTEGER.test(size)) {
            throw new Error(`Invalid icon size ${size}, must be an integer`);
          }
          result[size] = this.convertImageDataToDataURL(imageData[size], context);
        }
      }

      if (details.path) {
        let path = details.path;
        if (typeof path != "object") {
          path = {"19": path};
        }

        let baseURI = context ? context.uri : extension.baseURI;

        for (let size of Object.keys(path)) {
          if (!INTEGER.test(size)) {
            throw new Error(`Invalid icon size ${size}, must be an integer`);
          }

          let url = baseURI.resolve(path[size]);

          // The Chrome documentation specifies these parameters as
          // relative paths. We currently accept absolute URLs as well,
          // which means we need to check that the extension is allowed
          // to load them. This will throw an error if it's not allowed.
          Services.scriptSecurityManager.checkLoadURIStrWithPrincipal(
            extension.principal, url,
            Services.scriptSecurityManager.DISALLOW_SCRIPT);

          result[size] = url;
        }
      }
    } catch (e) {
      // Function is called from extension code, delegate error.
      if (context) {
        throw e;
      }
      // If there's no context, it's because we're handling this
      // as a manifest directive. Log a warning rather than
      // raising an error.
      extension.manifestError(`Invalid icon data: ${e}`);
    }

    return result;
  },

  // Returns the appropriate icon URL for the given icons object and the
  // screen resolution of the given window.
  getPreferredIcon(icons, extension = null, size = 16) {
    const DEFAULT = "chrome://browser/content/extension.svg";

    let bestSize = null;
    if (icons[size]) {
      bestSize = size;
    } else if (icons[2 * size]) {
      bestSize = 2 * size;
    } else {
      let sizes = Object.keys(icons)
                        .map(key => parseInt(key, 10))
                        .sort((a, b) => a - b);

      bestSize = sizes.find(candidate => candidate > size) || sizes.pop();
    }

    if (bestSize) {
      return {size: bestSize, icon: icons[bestSize]};
    }

    return {size, icon: DEFAULT};
  },

  convertImageURLToDataURL(imageURL, context, browserWindow, size = 18) {
    return new Promise((resolve, reject) => {
      let image = new context.contentWindow.Image();
      image.onload = function() {
        let canvas = context.contentWindow.document.createElement("canvas");
        let ctx = canvas.getContext("2d");
        let dSize = size * browserWindow.devicePixelRatio;

        // Scales the image while maintaing width to height ratio.
        // If the width and height differ, the image is centered using the
        // smaller of the two dimensions.
        let dWidth, dHeight, dx, dy;
        if (this.width > this.height) {
          dWidth = dSize;
          dHeight = image.height * (dSize / image.width);
          dx = 0;
          dy = (dSize - dHeight) / 2;
        } else {
          dWidth = image.width * (dSize / image.height);
          dHeight = dSize;
          dx = (dSize - dWidth) / 2;
          dy = 0;
        }

        ctx.drawImage(this, 0, 0, this.width, this.height, dx, dy, dWidth, dHeight);
        resolve(canvas.toDataURL("image/png"));
      };
      image.onerror = reject;
      image.src = imageURL;
    });
  },

  convertImageDataToDataURL(imageData, context) {
    let document = context.contentWindow.document;
    let canvas = document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
    canvas.width = imageData.width;
    canvas.height = imageData.height;
    canvas.getContext("2d").putImageData(imageData, 0, 0);

    return canvas.toDataURL("image/png");
  },
};

const LISTENERS = Symbol("listeners");

class EventEmitter {
  constructor() {
    this[LISTENERS] = new Map();
  }

  /**
   * Adds the given function as a listener for the given event.
   *
   * The listener function may optionally return a Promise which
   * resolves when it has completed all operations which event
   * dispatchers may need to block on.
   *
   * @param {string} event
   *       The name of the event to listen for.
   * @param {function(string, ...any)} listener
   *        The listener to call when events are emitted.
   */
  on(event, listener) {
    if (!this[LISTENERS].has(event)) {
      this[LISTENERS].set(event, new Set());
    }

    this[LISTENERS].get(event).add(listener);
  }

  /**
   * Removes the given function as a listener for the given event.
   *
   * @param {string} event
   *       The name of the event to stop listening for.
   * @param {function(string, ...any)} listener
   *        The listener function to remove.
   */
  off(event, listener) {
    if (this[LISTENERS].has(event)) {
      let set = this[LISTENERS].get(event);

      set.delete(listener);
      if (!set.size) {
        this[LISTENERS].delete(event);
      }
    }
  }

  /**
   * Triggers all listeners for the given event, and returns a promise
   * which resolves when all listeners have been called, and any
   * promises they have returned have likewise resolved.
   *
   * @param {string} event
   *       The name of the event to emit.
   * @param {any} args
   *        Arbitrary arguments to pass to the listener functions, after
   *        the event name.
   * @returns {Promise}
   */
  emit(event, ...args) {
    let listeners = this[LISTENERS].get(event) || new Set();

    let promises = Array.from(listeners, listener => {
      return runSafeSyncWithoutClone(listener, event, ...args);
    });

    return Promise.all(promises);
  }
}

function LocaleData(data) {
  this.defaultLocale = data.defaultLocale;
  this.selectedLocale = data.selectedLocale;
  this.locales = data.locales || new Map();
  this.warnedMissingKeys = new Set();

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
  localizeMessage(message, substitutions = [], options = {}) {
    let defaultOptions = {
      locale: this.selectedLocale,
      defaultValue: "",
      cloneScope: null,
    };

    options = Object.assign(defaultOptions, options);

    let locales = new Set([this.BUILTIN, options.locale, this.defaultLocale]
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
          }
          // For any series of contiguous `$`s, the first is dropped, and
          // the rest remain in the output string.
          return dollarSigns;
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

    if (!this.warnedMissingKeys.has(message)) {
      let error = `Unknown localization message ${message}`;
      if (options.cloneScope) {
        error = new options.cloneScope.Error(error);
      }
      Cu.reportError(error);
      this.warnedMissingKeys.add(message);
    }
    return options.defaultValue;
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
      return this.localizeMessage(message, [], {locale, defaultValue: matched});
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
// ExtensionContext in the chrome process or ExtensionContext in a
// content process). |name| is for debugging. |register| is a function
// to register the listener. |register| is only called once, even if
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
      Promise.resolve(callback).then(callback => {
        if (this.context.unloaded) {
          dump(`${this.name} event fired after context unloaded.\n`);
        } else if (this.callbacks.has(callback)) {
          this.context.runSafe(callback, ...args);
        }
      });
    }
  },

  fireWithoutClone(...args) {
    for (let callback of this.callbacks) {
      this.context.runSafeWithoutClone(callback, ...args);
    }
  },

  close() {
    if (this.callbacks.size) {
      this.unregister();
    }
    this.callbacks = Object.freeze([]);
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
    let wrappedCallback = (...args) => {
      if (this.context.unloaded) {
        dump(`${this.name} event fired after context unloaded.\n`);
      } else if (this.unregister.has(callback)) {
        return callback(...args);
      }
    };

    let unregister = this.register(wrappedCallback, ...args);
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

/**
 * Returns a Promise which resolves when the given document is fully
 * loaded.
 *
 * @param {Document} doc The document to await the load of.
 * @returns {Promise<Document>}
 */
function promiseDocumentLoaded(doc) {
  if (doc.readyState == "complete") {
    return Promise.resolve(doc);
  }

  return new Promise(resolve => {
    doc.defaultView.addEventListener("load", function onReady(event) {
      doc.defaultView.removeEventListener("load", onReady);
      resolve(doc);
    });
  });
}

/**
 * Returns a Promise which resolves the given observer topic has been
 * observed.
 *
 * @param {string} topic
 *        The topic to observe.
 * @param {function(nsISupports, string)} [test]
 *        An optional test function which, when called with the
 *        observer's subject and data, should return true if this is the
 *        expected notification, false otherwise.
 * @returns {Promise<object>}
 */
function promiseObserved(topic, test = () => true) {
  return new Promise(resolve => {
    let observer = (subject, topic, data) => {
      if (test(subject, data)) {
        Services.obs.removeObserver(observer, topic);
        resolve({subject, data});
      }
    };
    Services.obs.addObserver(observer, topic, false);
  });
}


/*
 * Messaging primitives.
 */

let gNextPortId = 1;

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
          if (!this.context.active) {
            // TODO: Send error as a response.
            Cu.reportError("Message received on port for an inactive content script");
          } else if (!this.disconnected) {
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
      this.disconnectByOtherEnd();
    }
  },

  disconnectByOtherEnd() {
    if (this.disconnected) {
      return;
    }

    for (let listener of this.disconnectListeners) {
      listener();
    }

    this.handleDisconnection();
  },

  disconnect() {
    if (this.disconnected) {
      // disconnect() may be called without side effects even after the port is
      // closed - https://developer.chrome.com/extensions/runtime#type-Port
      return;
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
// |messageManagers| is an array of MessageManagers used to receive messages.
// |sender| is an object describing the sender (usually giving its extension id, tabId, etc.)
// |filter| is a recipient filter to apply to incoming messages from the broker.
// |delegate| is an object that must implement a few methods:
//    getSender(context, messageManagerTarget, sender): returns a MessageSender
//      See https://developer.chrome.com/extensions/runtime#type-MessageSender.
function Messenger(context, messageManagers, sender, filter, delegate) {
  this.context = context;
  this.messageManagers = messageManagers;
  this.sender = sender;
  this.filter = filter;
  this.delegate = delegate;

  MessageChannel.setupMessageManagers(messageManagers);
}

Messenger.prototype = {
  _sendMessage(messageManager, message, data, recipient) {
    let options = {
      recipient,
      sender: this.sender,
      responseType: MessageChannel.RESPONSE_FIRST,
    };

    return this.context.sendMessage(messageManager, message, data, options);
  },

  sendMessage(messageManager, msg, recipient, responseCallback) {
    let promise = this._sendMessage(messageManager, "Extension:Message", msg, recipient)
      .catch(error => {
        if (error.result == MessageChannel.RESULT_NO_HANDLER) {
          return Promise.reject({message: "Could not establish connection. Receiving end does not exist."});
        } else if (error.result != MessageChannel.RESULT_NO_RESPONSE) {
          return Promise.reject({message: error.message});
        }
      });

    return this.context.wrapPromise(promise, responseCallback);
  },

  onMessage(name) {
    return new SingletonEventManager(this.context, name, callback => {
      let listener = {
        messageFilterPermissive: this.filter,

        filterMessage: (sender, recipient) => {
          // Ignore the message if it was sent by this Messenger.
          return !MessageChannel.matchesFilter(this.sender, sender);
        },

        receiveMessage: ({target, data: message, sender, recipient}) => {
          if (!this.context.active) {
            return;
          }

          if (this.delegate) {
            this.delegate.getSender(this.context, target, sender);
          }

          let sendResponse;
          let response = undefined;
          let promise = new Promise(resolve => {
            sendResponse = value => {
              resolve(value);
              response = promise;
            };
          });

          message = Cu.cloneInto(message, this.context.cloneScope);
          sender = Cu.cloneInto(sender, this.context.cloneScope);
          sendResponse = Cu.exportFunction(sendResponse, this.context.cloneScope);

          // Note: We intentionally do not use runSafe here so that any
          // errors are propagated to the message sender.
          let result = callback(message, sender, sendResponse);
          if (result instanceof this.context.cloneScope.Promise) {
            return result;
          } else if (result === true) {
            return promise;
          }
          return response;
        },
      };

      MessageChannel.addListener(this.messageManagers, "Extension:Message", listener);
      return () => {
        MessageChannel.removeListener(this.messageManagers, "Extension:Message", listener);
      };
    }).api();
  },

  connect(messageManager, name, recipient) {
    let portId = `${gNextPortId++}-${Services.appinfo.uniqueProcessID}`;
    let port = new Port(this.context, messageManager, name, portId, null);
    let msg = {name, portId};
    this._sendMessage(messageManager, "Extension:Connect", msg, recipient)
      .catch(e => port.disconnectByOtherEnd());
    return port.api();
  },

  onConnect(name) {
    return new SingletonEventManager(this.context, name, callback => {
      let listener = {
        messageFilterPermissive: this.filter,

        filterMessage: (sender, recipient) => {
          // Ignore the port if it was created by this Messenger.
          return !MessageChannel.matchesFilter(this.sender, sender);
        },

        receiveMessage: ({target, data: message, sender, recipient}) => {
          let {name, portId} = message;
          let mm = getMessageManager(target);
          if (this.delegate) {
            this.delegate.getSender(this.context, target, sender);
          }
          let port = new Port(this.context, mm, name, portId, sender);
          this.context.runSafeWithoutClone(callback, port.api());
          return true;
        },
      };

      MessageChannel.addListener(this.messageManagers, "Extension:Connect", listener);
      return () => {
        MessageChannel.removeListener(this.messageManagers, "Extension:Connect", listener);
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

/**
 * An object that runs the implementation of a schema API. Instantiations of
 * this interfaces are used by Schemas.jsm.
 *
 * @interface
 */
class SchemaAPIInterface {
  /**
   * Calls this as a function that returns its return value.
   *
   * @abstract
   * @param {Array} args The parameters for the function.
   * @returns {*} The return value of the invoked function.
   */
  callFunction(args) {
    throw new Error("Not implemented");
  }

  /**
   * Calls this as a function and ignores its return value.
   *
   * @abstract
   * @param {Array} args The parameters for the function.
   */
  callFunctionNoReturn(args) {
    throw new Error("Not implemented");
  }

  /**
   * Calls this as a function that completes asynchronously.
   *
   * @abstract
   * @param {Array} args The parameters for the function.
   * @param {function(*)} [callback] The callback to be called when the function
   *     completes.
   * @returns {Promise|undefined} Must be void if `callback` is set, and a
   *     promise otherwise. The promise is resolved when the function completes.
   */
  callAsyncFunction(args, callback) {
    throw new Error("Not implemented");
  }

  /**
   * Retrieves the value of this as a property.
   *
   * @abstract
   * @returns {*} The value of the property.
   */
  getProperty() {
    throw new Error("Not implemented");
  }

  /**
   * Assigns the value to this as property.
   *
   * @abstract
   * @param {string} value The new value of the property.
   */
  setProperty(value) {
    throw new Error("Not implemented");
  }

  /**
   * Registers a `listener` to this as an event.
   *
   * @abstract
   * @param {function} listener The callback to be called when the event fires.
   * @param {Array} args Extra parameters for EventManager.addListener.
   * @see EventManager.addListener
   */
  addListener(listener, args) {
    throw new Error("Not implemented");
  }

  /**
   * Checks whether `listener` is listening to this as an event.
   *
   * @abstract
   * @param {function} listener The event listener.
   * @returns {boolean} Whether `listener` is registered with this as an event.
   * @see EventManager.hasListener
   */
  hasListener(listener) {
    throw new Error("Not implemented");
  }

  /**
   * Unregisters `listener` from this as an event.
   *
   * @abstract
   * @param {function} listener The event listener.
   * @see EventManager.removeListener
   */
  removeListener(listener) {
    throw new Error("Not implemented");
  }
}

/**
 * An object that runs a locally implemented API.
 */
class LocalAPIImplementation extends SchemaAPIInterface {
  /**
   * Constructs an implementation of the `name` method or property of `pathObj`.
   *
   * @param {object} pathObj The object containing the member with name `name`.
   * @param {string} name The name of the implemented member.
   * @param {BaseContext} context The context in which the schema is injected.
   */
  constructor(pathObj, name, context) {
    super();
    this.pathObj = pathObj;
    this.name = name;
    this.context = context;
  }

  callFunction(args) {
    return this.pathObj[this.name](...args);
  }

  callFunctionNoReturn(args) {
    this.pathObj[this.name](...args);
  }

  callAsyncFunction(args, callback) {
    let promise;
    try {
      promise = this.pathObj[this.name](...args) || Promise.resolve();
    } catch (e) {
      promise = Promise.reject(e);
    }
    return this.context.wrapPromise(promise, callback);
  }

  getProperty() {
    return this.pathObj[this.name];
  }

  setProperty(value) {
    this.pathObj[this.name] = value;
  }

  addListener(listener, args) {
    this.pathObj[this.name].addListener.call(null, listener, ...args);
  }

  hasListener(listener) {
    return this.pathObj[this.name].hasListener.call(null, listener);
  }

  removeListener(listener) {
    this.pathObj[this.name].removeListener.call(null, listener);
  }
}

let nextId = 1;

/**
 * An object that runs an remote implementation of an API.
 */
class ProxyAPIImplementation extends SchemaAPIInterface {
  /**
   * @param {string} namespace The full path to the namespace that contains the
   *     `name` member. This may contain dots, e.g. "storage.local".
   * @param {string} name The name of the method or property.
   * @param {ChildAPIManager} childApiManager The owner of this implementation.
   */
  constructor(namespace, name, childApiManager) {
    super();
    this.path = `${namespace}.${name}`;
    this.childApiManager = childApiManager;
  }

  callFunctionNoReturn(args) {
    this.childApiManager.messageManager.sendAsyncMessage("API:Call", {
      childId: this.childApiManager.id,
      path: this.path,
      args,
    });
  }

  callAsyncFunction(args, callback) {
    let callId = nextId++;
    let deferred = PromiseUtils.defer();
    this.childApiManager.callPromises.set(callId, deferred);

    this.childApiManager.messageManager.sendAsyncMessage("API:Call", {
      childId: this.childApiManager.id,
      callId,
      path: this.path,
      args,
    });

    return this.childApiManager.context.wrapPromise(deferred.promise, callback);
  }

  addListener(listener, args) {
    let set = this.childApiManager.listeners.get(this.path);
    if (!set) {
      set = new Set();
      this.childApiManager.listeners.set(this.path, set);
    }

    set.add(listener);

    if (set.size == 1) {
      args = args.slice(1);

      this.childApiManager.messageManager.sendAsyncMessage("API:AddListener", {
        childId: this.childApiManager.id,
        path: this.path,
        args,
      });
    }
  }

  removeListener(listener) {
    let set = this.childApiManager.listeners.get(this.path);
    if (!set) {
      return;
    }
    set.remove(listener);

    if (set.size == 0) {
      this.childApiManager.messageManager.sendAsyncMessage("API:RemoveListener", {
        childId: this.childApiManager.id,
        path: this.path,
      });
    }
  }

  hasListener(listener) {
    let set = this.childApiManager.listeners.get(this.path);
    return set ? set.has(listener) : false;
  }
}

// We create one instance of this class for every extension context
// that needs to use remote APIs. It uses the message manager to
// communicate with the ParentAPIManager singleton in
// Extension.jsm. It handles asynchronous function calls as well as
// event listeners.
class ChildAPIManager {
  constructor(context, messageManager, localApis, contextData) {
    this.context = context;
    this.messageManager = messageManager;

    // The root namespace of all locally implemented APIs. If an extension calls
    // an API that does not exist in this object, then the implementation is
    // delegated to the ParentAPIManager.
    this.localApis = localApis;

    let id = String(context.extension.id) + "." + String(context.contextId);
    this.id = id;

    let data = {childId: id, extensionId: context.extension.id, principal: context.principal};
    Object.assign(data, contextData);
    messageManager.sendAsyncMessage("API:CreateProxyContext", data);

    messageManager.addMessageListener("API:RunListener", this);
    messageManager.addMessageListener("API:CallResult", this);

    // Map[path -> Set[listener]]
    // path is, e.g., "runtime.onMessage".
    this.listeners = new Map();

    // Map[callId -> Deferred]
    this.callPromises = new Map();
  }

  receiveMessage({name, data}) {
    if (data.childId != this.id) {
      return;
    }

    switch (name) {
      case "API:RunListener":
        let listeners = this.listeners.get(data.path);
        for (let callback of listeners) {
          runSafe(this.context, callback, ...data.args);
        }
        break;

      case "API:CallResult":
        let deferred = this.callPromises.get(data.callId);
        if (data.lastError) {
          deferred.reject({message: data.lastError});
        } else {
          deferred.resolve(new SpreadArgs(data.args));
        }
        this.callPromises.delete(data.callId);
        break;
    }
  }

  close() {
    this.messageManager.sendAsyncMessage("API:CloseProxyContext", {childId: this.id});
  }

  get cloneScope() {
    return this.context.cloneScope;
  }

  shouldInject(namespace, name, restrictions) {
    // Do not generate content script APIs, unless explicitly allowed.
    if (this.context.envType === "content_child" &&
        (!restrictions || !restrictions.includes("content"))) {
      return false;
    }
    return true;
  }

  getImplementation(namespace, name) {
    let pathObj = this.localApis;
    if (pathObj) {
      for (let part of namespace.split(".")) {
        pathObj = pathObj[part];
        if (!pathObj) {
          break;
        }
      }
      if (pathObj && name in pathObj) {
        return new LocalAPIImplementation(pathObj, name, this.context);
      }
    }

    // No local API found, defer implementation to the parent.
    return new ProxyAPIImplementation(namespace, name, this);
  }

  hasPermission(permission) {
    return this.context.extension.permissions.has(permission);
  }
}

/**
 * This object loads the ext-*.js scripts that define the extension API.
 *
 * This class instance is shared with the scripts that it loads, so that the
 * ext-*.js scripts and the instantiator can communicate with each other.
 */
class SchemaAPIManager extends EventEmitter {
  /**
   * @param {string} processType
   *     "main" - The main, one and only chrome browser process.
   *     "addon" - An addon process.
   *     "content" - A content process.
   */
  constructor(processType) {
    super();
    this.processType = processType;
    this.global = this._createExtGlobal();
    this._scriptScopes = [];
    this._schemaApis = {
      addon_parent: [],
      addon_child: [],
      content_parent: [],
      content_child: [],
    };
  }

  /**
   * Create a global object that is used as the shared global for all ext-*.js
   * scripts that are loaded via `loadScript`.
   *
   * @returns {object} A sandbox that is used as the global by `loadScript`.
   */
  _createExtGlobal() {
    let global = Cu.Sandbox(Services.scriptSecurityManager.getSystemPrincipal(), {
      wantXrays: false,
      sandboxName: `Namespace of ext-*.js scripts for ${this.processType}`,
    });
    Object.defineProperty(global, "console", {get() { return console; }});
    global.extensions = this;
    global.global = global;
    global.Cc = Cc;
    global.Ci = Ci;
    global.Cu = Cu;
    global.Cr = Cr;
    XPCOMUtils.defineLazyModuleGetter(global, "require",
                                      "resource://devtools/shared/Loader.jsm");
    global.XPCOMUtils = XPCOMUtils;
    return global;
  }

  /**
   * Load an ext-*.js script. The script runs in its own scope, if it wishes to
   * share state with another script it can assign to the `global` variable. If
   * it wishes to communicate with this API manager, use `extensions`.
   *
   * @param {string} scriptUrl The URL of the ext-*.js script.
   */
  loadScript(scriptUrl) {
    // Create the object in the context of the sandbox so that the script runs
    // in the sandbox's context instead of here.
    let scope = Cu.createObjectIn(this.global);

    Services.scriptloader.loadSubScript(scriptUrl, scope, "UTF-8");

    // Save the scope to avoid it being garbage collected.
    this._scriptScopes.push(scope);
  }

  /**
   * Called by an ext-*.js script to register an API.
   *
   * @param {string} namespace The API namespace.
   *     Intended to match the namespace of the generated API, but not used at
   *     the moment - see bugzil.la/1295774.
   * @param {string} envType Restricts the API to contexts that run in the
   *    given environment. Must be one of the following:
   *     - "addon_parent" - addon APIs that runs in the main process.
   *     - "addon_child" - addon APIs that runs in an addon process.
   *     - "content_parent" - content script APIs that runs in the main process.
   *     - "content_child" - content script APIs that runs in a content process.
   * @param {function(BaseContext)} getAPI A function that returns an object
   *     that will be merged with |chrome| and |browser|. The next example adds
   *     the create, update and remove methods to the tabs API.
   *
   *     registerSchemaAPI("tabs", "addon_parent", (context) => ({
   *       tabs: { create, update },
   *     }));
   *     registerSchemaAPI("tabs", "addon_parent", (context) => ({
   *       tabs: { remove },
   *     }));
   */
  registerSchemaAPI(namespace, envType, getAPI) {
    this._schemaApis[envType].push({namespace, getAPI});
  }

  /**
   * Exports all registered scripts to `obj`.
   *
   * @param {BaseContext} context The context for which the API bindings are
   *     generated.
   * @param {object} obj The destination of the API.
   */
  generateAPIs(context, obj) {
    let apis = this._schemaApis[context.envType];
    if (!apis) {
      Cu.reportError(`No APIs have been registered for ${context.envType}`);
      return;
    }
    SchemaAPIManager.generateAPIs(context, apis, obj);
  }

  /**
   * Mash together all the APIs from `apis` into `obj`.
   *
   * @param {BaseContext} context The context for which the API bindings are
   *     generated.
   * @param {Array} apis A list of objects, see `registerSchemaAPI`.
   * @param {object} obj The destination of the API.
   */
  static generateAPIs(context, apis, obj) {
    // Recursively copy properties from source to dest.
    function copy(dest, source) {
      for (let prop in source) {
        let desc = Object.getOwnPropertyDescriptor(source, prop);
        if (typeof(desc.value) == "object") {
          if (!(prop in dest)) {
            dest[prop] = {};
          }
          copy(dest[prop], source[prop]);
        } else {
          Object.defineProperty(dest, prop, desc);
        }
      }
    }

    for (let api of apis) {
      api = api.getAPI(context);
      copy(obj, api);
    }
  }
}

/**
 * Convert any of several different representations of a date/time to a Date object.
 * Accepts several formats:
 * a Date object, an ISO8601 string, or a number of milliseconds since the epoch as
 * either a number or a string.
 *
 * @param {Date|string|number} date
 *      The date to convert.
 * @returns {Date}
 *      A Date object
 */
function normalizeTime(date) {
  // Of all the formats we accept the "number of milliseconds since the epoch as a string"
  // is an outlier, everything else can just be passed directly to the Date constructor.
  return new Date((typeof date == "string" && /^\d+$/.test(date))
                        ? parseInt(date, 10) : date);
}

this.ExtensionUtils = {
  detectLanguage,
  extend,
  flushJarCache,
  getConsole,
  getInnerWindowID,
  ignoreEvent,
  injectAPI,
  instanceOf,
  normalizeTime,
  promiseDocumentLoaded,
  promiseDocumentReady,
  promiseObserved,
  runSafe,
  runSafeSync,
  runSafeSyncWithoutClone,
  runSafeWithoutClone,
  BaseContext,
  DefaultWeakMap,
  EventEmitter,
  EventManager,
  IconDetails,
  LocalAPIImplementation,
  LocaleData,
  Messenger,
  PlatformInfo,
  SchemaAPIInterface,
  SingletonEventManager,
  SpreadArgs,
  ChildAPIManager,
  SchemaAPIManager,
};
