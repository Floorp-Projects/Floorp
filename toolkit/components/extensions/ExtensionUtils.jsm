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
XPCOMUtils.defineLazyModuleGetter(this, "ExtensionManagement",
                                  "resource://gre/modules/ExtensionManagement.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "IndexedDB",
                                  "resource://gre/modules/IndexedDB.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "LanguageDetector",
                                  "resource:///modules/translation/LanguageDetector.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Locale",
                                  "resource://gre/modules/Locale.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "MessageChannel",
                                  "resource://gre/modules/MessageChannel.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Preferences",
                                  "resource://gre/modules/Preferences.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Schemas",
                                  "resource://gre/modules/Schemas.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "styleSheetService",
                                   "@mozilla.org/content/style-sheet-service;1",
                                   "nsIStyleSheetService");

/* globals IDBKeyRange */

function getConsole() {
  return new ConsoleAPI({
    maxLogLevelPref: "extensions.webextensions.log.level",
    prefix: "WebExtensions",
  });
}

XPCOMUtils.defineLazyGetter(this, "console", getConsole);

let nextId = 0;
XPCOMUtils.defineLazyGetter(this, "uniqueProcessID", () => Services.appinfo.uniqueProcessID);

function getUniqueId() {
  return `${nextId++}-${uniqueProcessID}`;
}

// The list of properties that themes are allowed to contain.
XPCOMUtils.defineLazyGetter(this, "gAllowedThemeProperties", () => {
  Cu.import("resource://gre/modules/ExtensionParent.jsm");
  let propertiesInBaseManifest = ExtensionParent.baseManifestProperties;

  // The properties found in the base manifest contain all of the properties that
  // themes are allowed to have. However, the list also contains several properties
  // that aren't allowed, so we need to filter them out first before the list can
  // be used to validate themes.
  return propertiesInBaseManifest.filter(prop => {
    const propertiesToRemove = ["background", "content_scripts", "permissions"];
    return !propertiesToRemove.includes(prop);
  });
});

/**
 * Validates a theme to ensure it only contains static resources.
 *
 * @param {Array<string>} manifestProperties The list of top-level keys found in the
 *    the extension's manifest.
 * @returns {Array<string>} A list of invalid properties or an empty list
 *    if none are found.
 */
function validateThemeManifest(manifestProperties) {
  let invalidProps = [];
  for (let propName of manifestProperties) {
    if (propName != "theme" && !gAllowedThemeProperties.includes(propName)) {
      invalidProps.push(propName);
    }
  }
  return invalidProps;
}

let StartupCache = {
  DB_NAME: "ExtensionStartupCache",

  SCHEMA_VERSION: 1,

  STORE_NAMES: Object.freeze(["locales", "manifests", "schemas"]),

  dbPromise: null,

  cacheInvalidated: 0,

  initDB(db) {
    for (let name of StartupCache.STORE_NAMES) {
      try {
        db.deleteObjectStore(name);
      } catch (e) {
        // Don't worry if the store doesn't already exist.
      }
      db.createObjectStore(name);
    }
  },

  clearAddonData(id) {
    let range = IDBKeyRange.bound([id], [id, "\uFFFF"]);

    return Promise.all([
      this.locales.delete(range),
      this.manifests.delete(range),
    ]).catch(e => {
      // Ignore the error. It happens when we try to flush the add-on
      // data after the AddonManager has flushed the entire startup cache.
    });
  },

  async reallyOpen(invalidate = false) {
    if (this.dbPromise) {
      let db = await this.dbPromise;
      db.close();
    }

    if (invalidate) {
      this.cacheInvalidated = ExtensionManagement.cacheInvalidated;

      if (Services.appinfo.processType === Services.appinfo.PROCESS_TYPE_DEFAULT) {
        IndexedDB.deleteDatabase(this.DB_NAME, {storage: "persistent"});
      }
    }

    return IndexedDB.open(this.DB_NAME,
                          {storage: "persistent", version: this.SCHEMA_VERSION},
                          db => this.initDB(db));
  },

  async open() {
    if (ExtensionManagement.cacheInvalidated > this.cacheInvalidated) {
      this.dbPromise = this.reallyOpen(true);
    } else if (!this.dbPromise) {
      this.dbPromise = this.reallyOpen();
    }

    return this.dbPromise;
  },

  observe(subject, topic, data) {
    if (topic === "startupcache-invalidate") {
      this.dbPromise = this.reallyOpen(true).catch(e => {});
    }
  },
};

Services.obs.addObserver(StartupCache, "startupcache-invalidate", false);

class CacheStore {
  constructor(storeName) {
    this.storeName = storeName;
  }

  async get(key, createFunc) {
    let db;
    let value;
    try {
      db = await StartupCache.open();

      value = await db.objectStore(this.storeName)
                      .get(key);
    } catch (e) {
      Cu.reportError(e);

      return createFunc(key);
    }

    if (value === undefined) {
      value = await createFunc(key);

      db.objectStore(this.storeName, "readwrite")
        .put(value, key);
    }

    return value;
  }

  async delete(key) {
    let db = await StartupCache.open();

    return db.objectStore(this.storeName, "readwrite").delete(key);
  }
}

for (let name of StartupCache.STORE_NAMES) {
  StartupCache[name] = new CacheStore(name);
}

/**
 * An Error subclass for which complete error messages are always passed
 * to extensions, rather than being interpreted as an unknown error.
 */
class ExtensionError extends Error {}

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

/**
 * Similar to a WeakMap, but creates a new key with the given
 * constructor if one is not present.
 */
class DefaultWeakMap extends WeakMap {
  constructor(defaultConstructor, init) {
    super(init);
    this.defaultConstructor = defaultConstructor;
  }

  get(key) {
    if (!this.has(key)) {
      this.set(key, this.defaultConstructor(key));
    }
    return super.get(key);
  }
}

class DefaultMap extends Map {
  constructor(defaultConstructor, init) {
    super(init);
    this.defaultConstructor = defaultConstructor;
  }

  get(key) {
    if (!this.has(key)) {
      this.set(key, this.defaultConstructor(key));
    }
    return super.get(key);
  }
}

const _winUtils = new DefaultWeakMap(win => {
  return win.QueryInterface(Ci.nsIInterfaceRequestor)
            .getInterface(Ci.nsIDOMWindowUtils);
});
const getWinUtils = win => _winUtils.get(win);

function getInnerWindowID(window) {
  return getWinUtils(window).currentInnerWindowID;
}

class SpreadArgs extends Array {
  constructor(args) {
    super();
    this.push(...args);
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

        if (typeof imageData == "string") {
          imageData = {"19": imageData};
        }

        for (let size of Object.keys(imageData)) {
          if (!INTEGER.test(size)) {
            throw new ExtensionError(`Invalid icon size ${size}, must be an integer`);
          }
          result[size] = imageData[size];
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
            throw new ExtensionError(`Invalid icon size ${size}, must be an integer`);
          }

          let url = baseURI.resolve(path[size]);

          // The Chrome documentation specifies these parameters as
          // relative paths. We currently accept absolute URLs as well,
          // which means we need to check that the extension is allowed
          // to load them. This will throw an error if it's not allowed.
          try {
            Services.scriptSecurityManager.checkLoadURIStrWithPrincipal(
              extension.principal, url,
              Services.scriptSecurityManager.DISALLOW_SCRIPT);
          } catch (e) {
            throw new ExtensionError(`Illegal URL ${url}`);
          }

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

  convertImageURLToDataURL(imageURL, contentWindow, browserWindow, size = 18) {
    return new Promise((resolve, reject) => {
      let image = new contentWindow.Image();
      image.onload = function() {
        let canvas = contentWindow.document.createElement("canvas");
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

  // These URLs should already be properly escaped, but make doubly sure CSS
  // string escape characters are escaped here, since they could lead to a
  // sandbox break.
  escapeUrl(url) {
    return url.replace(/[\\\s"]/g, encodeURIComponent);
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
// new SingletonEventManager(context, "api.subAPI", fire => {
//   let listener = (...) => {
//     // Fire any listeners registered with addListener.
//     fire.async(arg1, arg2);
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
// to register the listener. |register| should return an
// unregister function that will unregister the listener.
function SingletonEventManager(context, name, register) {
  this.context = context;
  this.name = name;
  this.register = register;
  this.unregister = new Map();
}

SingletonEventManager.prototype = {
  addListener(callback, ...args) {
    if (this.unregister.has(callback)) {
      return;
    }

    let shouldFire = () => {
      if (this.context.unloaded) {
        dump(`${this.name} event fired after context unloaded.\n`);
      } else if (!this.context.active) {
        dump(`${this.name} event fired while context is inactive.\n`);
      } else if (this.unregister.has(callback)) {
        return true;
      }
      return false;
    };

    let fire = {
      sync: (...args) => {
        if (shouldFire()) {
          return this.context.runSafe(callback, ...args);
        }
      },
      async: (...args) => {
        return Promise.resolve().then(() => {
          if (shouldFire()) {
            return this.context.runSafe(callback, ...args);
          }
        });
      },
      raw: (...args) => {
        if (!shouldFire()) {
          throw new Error("Called raw() on unloaded/inactive context");
        }
        return callback(...args);
      },
      asyncWithoutClone: (...args) => {
        return Promise.resolve().then(() => {
          if (shouldFire()) {
            return this.context.runSafeWithoutClone(callback, ...args);
          }
        });
      },
    };


    let unregister = this.register(fire, ...args);
    this.unregister.set(callback, unregister);
    this.context.callOnClose(this);
  },

  removeListener(callback) {
    if (!this.unregister.has(callback)) {
      return;
    }

    let unregister = this.unregister.get(callback);
    this.unregister.delete(callback);
    try {
      unregister();
    } catch (e) {
      Cu.reportError(e);
    }
    if (this.unregister.size == 0) {
      this.context.forgetOnClose(this);
    }
  },

  hasListener(callback) {
    return this.unregister.has(callback);
  },

  revoke() {
    for (let callback of this.unregister.keys()) {
      this.removeListener(callback);
    }
  },

  close() {
    this.revoke();
  },

  api() {
    return {
      addListener: (...args) => this.addListener(...args),
      removeListener: (...args) => this.removeListener(...args),
      hasListener: (...args) => this.hasListener(...args),
      [Schemas.REVOKE]: () => this.revoke(),
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
      let scriptError = Cc["@mozilla.org/scripterror;1"]
        .createInstance(Ci.nsIScriptError);
      scriptError.init(msg, frame.filename, null, frame.lineNumber,
                       frame.columnNumber, Ci.nsIScriptError.warningFlag,
                       "content javascript");
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
 * A set with a limited number of slots, which flushes older entries as
 * newer ones are added.
 */
class LimitedSet extends Set {
  constructor(limit, iterable = undefined) {
    super(iterable);
    this.limit = limit;
  }

  truncate(limit) {
    for (let item of this) {
      if (this.size <= limit) {
        break;
      }
      this.delete(item);
    }
  }

  add(item) {
    if (!this.has(item) && this.size >= this.limit) {
      this.truncate(this.limit - 1);
    }
    super.add(item);
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
    doc.defaultView.addEventListener("load", function(event) {
      resolve(doc);
    }, {once: true});
  });
}

/**
 * Returns a Promise which resolves when the given event is dispatched to the
 * given element.
 *
 * @param {Element} element
 *        The element on which to listen.
 * @param {string} eventName
 *        The event to listen for.
 * @param {boolean} [useCapture = true]
 *        If true, listen for the even in the capturing rather than
 *        bubbling phase.
 * @param {Event} [test]
 *        An optional test function which, when called with the
 *        observer's subject and data, should return true if this is the
 *        expected event, false otherwise.
 * @returns {Promise<Event>}
 */
function promiseEvent(element, eventName, useCapture = true, test = event => true) {
  return new Promise(resolve => {
    function listener(event) {
      if (test(event)) {
        element.removeEventListener(eventName, listener, useCapture);
        resolve(event);
      }
    }
    element.addEventListener(eventName, listener, useCapture);
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

function getMessageManager(target) {
  if (target instanceof Ci.nsIFrameLoaderOwner) {
    return target.QueryInterface(Ci.nsIFrameLoaderOwner).frameLoader.messageManager;
  }
  return target.QueryInterface(Ci.nsIMessageSender);
}

function flushJarCache(jarPath) {
  Services.obs.notifyObservers(null, "flush-cache-entry", jarPath);
}

function PlatformInfo() {
  return Object.freeze({
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
}

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

const stylesheetMap = new DefaultMap(url => {
  let uri = NetUtil.newURI(url);
  return styleSheetService.preloadSheet(uri, styleSheetService.AGENT_SHEET);
});

/**
 * Defines a lazy getter for the given property on the given object. The
 * first time the property is accessed, the return value of the getter
 * is defined on the current `this` object with the given property name.
 * Importantly, this means that a lazy getter defined on an object
 * prototype will be invoked separately for each object instance that
 * it's accessed on.
 *
 * @param {object} object
 *        The prototype object on which to define the getter.
 * @param {string|Symbol} prop
 *        The property name for which to define the getter.
 * @param {function} getter
 *        The function to call in order to generate the final property
 *        value.
 */
function defineLazyGetter(object, prop, getter) {
  let redefine = (obj, value) => {
    Object.defineProperty(obj, prop, {
      enumerable: true,
      configurable: true,
      writable: true,
      value,
    });
    return value;
  };

  Object.defineProperty(object, prop, {
    enumerable: true,
    configurable: true,

    get() {
      return redefine(this, getter.call(this));
    },

    set(value) {
      redefine(this, value);
    },
  });
}

function findPathInObject(obj, path, printErrors = true) {
  let parent;
  for (let elt of path.split(".")) {
    if (!obj || !(elt in obj)) {
      if (printErrors) {
        Cu.reportError(`WebExtension API ${path} not found (it may be unimplemented by Firefox).`);
      }
      return null;
    }

    parent = obj;
    obj = obj[elt];
  }

  if (typeof obj === "function") {
    return obj.bind(parent);
  }
  return obj;
}

/**
 * Acts as a proxy for a message manager or message manager owner, and
 * tracks docShell swaps so that messages are always sent to the same
 * receiver, even if it is moved to a different <browser>.
 *
 * @param {nsIMessageSender|Element} target
 *        The target message manager on which to send messages, or the
 *        <browser> element which owns it.
 */
class MessageManagerProxy {
  constructor(target) {
    this.listeners = new DefaultMap(() => new Map());

    if (target instanceof Ci.nsIMessageSender) {
      Object.defineProperty(this, "messageManager", {
        value: target,
        configurable: true,
        writable: true,
      });
    } else {
      this.addListeners(target);
    }
  }

  /**
   * Disposes of the proxy object, removes event listeners, and drops
   * all references to the underlying message manager.
   *
   * Must be called before the last reference to the proxy is dropped,
   * unless the underlying message manager or <browser> is also being
   * destroyed.
   */
  dispose() {
    if (this.eventTarget) {
      this.removeListeners(this.eventTarget);
      this.eventTarget = null;
    } else {
      this.messageManager = null;
    }
  }

  /**
   * Returns true if the given target is the same as, or owns, the given
   * message manager.
   *
   * @param {nsIMessageSender|MessageManagerProxy|Element} target
   *        The message manager, MessageManagerProxy, or <browser>
   *        element agaisnt which to match.
   * @param {nsIMessageSender} messageManager
   *        The message manager against which to match `target`.
   *
   * @returns {boolean}
   *        True if `messageManager` is the same object as `target`, or
   *        `target` is a MessageManagerProxy or <browser> element that
   *        is tied to it.
   */
  static matches(target, messageManager) {
    return target === messageManager || target.messageManager === messageManager;
  }

  /**
   * @property {nsIMessageSender|null} messageManager
   *        The message manager that is currently being proxied. This
   *        may change during the life of the proxy object, so should
   *        not be stored elsewhere.
   */
  get messageManager() {
    return this.eventTarget && this.eventTarget.messageManager;
  }

  /**
   * Sends a message on the proxied message manager.
   *
   * @param {array} args
   *        Arguments to be passed verbatim to the underlying
   *        sendAsyncMessage method.
   * @returns {undefined}
   */
  sendAsyncMessage(...args) {
    if (this.messageManager) {
      return this.messageManager.sendAsyncMessage(...args);
    }
    /* globals uneval */
    Cu.reportError(`Cannot send message: Other side disconnected: ${uneval(args)}`);
  }

  get isDisconnected() {
    return !this.messageManager;
  }

  /**
   * Adds a message listener to the current message manager, and
   * transfers it to the new message manager after a docShell swap.
   *
   * @param {string} message
   *        The name of the message to listen for.
   * @param {nsIMessageListener} listener
   *        The listener to add.
   * @param {boolean} [listenWhenClosed = false]
   *        If true, the listener will receive messages which were sent
   *        after the remote side of the listener began closing.
   */
  addMessageListener(message, listener, listenWhenClosed = false) {
    this.messageManager.addMessageListener(message, listener, listenWhenClosed);
    this.listeners.get(message).set(listener, listenWhenClosed);
  }

  /**
   * Adds a message listener from the current message manager.
   *
   * @param {string} message
   *        The name of the message to stop listening for.
   * @param {nsIMessageListener} listener
   *        The listener to remove.
   */
  removeMessageListener(message, listener) {
    this.messageManager.removeMessageListener(message, listener);

    let listeners = this.listeners.get(message);
    listeners.delete(listener);
    if (!listeners.size) {
      this.listeners.delete(message);
    }
  }

  /**
   * @private
   * Iterates over all of the currently registered message listeners.
   */
  * iterListeners() {
    for (let [message, listeners] of this.listeners) {
      for (let [listener, listenWhenClosed] of listeners) {
        yield {message, listener, listenWhenClosed};
      }
    }
  }

  /**
   * @private
   * Adds docShell swap listeners to the message manager owner.
   *
   * @param {Element} target
   *        The target element.
   */
  addListeners(target) {
    target.addEventListener("SwapDocShells", this);

    for (let {message, listener, listenWhenClosed} of this.iterListeners()) {
      target.addMessageListener(message, listener, listenWhenClosed);
    }

    this.eventTarget = target;
  }

  /**
   * @private
   * Removes docShell swap listeners to the message manager owner.
   *
   * @param {Element} target
   *        The target element.
   */
  removeListeners(target) {
    target.removeEventListener("SwapDocShells", this);

    for (let {message, listener} of this.iterListeners()) {
      target.removeMessageListener(message, listener);
    }
  }

  handleEvent(event) {
    if (event.type == "SwapDocShells") {
      this.removeListeners(this.eventTarget);
      this.addListeners(event.detail);
    }
  }
}

this.ExtensionUtils = {
  defineLazyGetter,
  detectLanguage,
  extend,
  findPathInObject,
  flushJarCache,
  getConsole,
  getInnerWindowID,
  getMessageManager,
  getUniqueId,
  filterStack,
  getWinUtils,
  ignoreEvent,
  injectAPI,
  instanceOf,
  normalizeTime,
  promiseDocumentLoaded,
  promiseDocumentReady,
  promiseEvent,
  promiseObserved,
  runSafe,
  runSafeSync,
  runSafeSyncWithoutClone,
  runSafeWithoutClone,
  stylesheetMap,
  validateThemeManifest,
  DefaultMap,
  DefaultWeakMap,
  EventEmitter,
  ExtensionError,
  IconDetails,
  LimitedSet,
  LocaleData,
  MessageManagerProxy,
  SingletonEventManager,
  SpreadArgs,
  StartupCache,
};

XPCOMUtils.defineLazyGetter(this.ExtensionUtils, "PlatformInfo", PlatformInfo);
