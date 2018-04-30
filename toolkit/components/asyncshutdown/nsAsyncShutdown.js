/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * An implementation of nsIAsyncShutdown* based on AsyncShutdown.jsm
 */

"use strict";

var XPCOMUtils = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm", {}).XPCOMUtils;
ChromeUtils.defineModuleGetter(this, "AsyncShutdown",
  "resource://gre/modules/AsyncShutdown.jsm");


/**
 * Conversion between nsIPropertyBag and JS object
 */
var PropertyBagConverter = {
  // From nsIPropertyBag to JS
  toObject(bag) {
    if (!(bag instanceof Ci.nsIPropertyBag)) {
      throw new TypeError("Not a property bag");
    }
    let result = {};
    let enumerator = bag.enumerator;
    while (enumerator.hasMoreElements()) {
      let {name, value: property} = enumerator.getNext().QueryInterface(Ci.nsIProperty);
      let value = this.toValue(property);
      result[name] = value;
    }
    return result;
  },
  toValue(property) {
    if (typeof property != "object") {
      return property;
    }
    if (Array.isArray(property)) {
      return property.map(this.toValue, this);
    }
    if (property && property instanceof Ci.nsIPropertyBag) {
      return this.toObject(property);
    }
    return property;
  },

  // From JS to nsIPropertyBag
  fromObject(obj) {
    if (obj == null || typeof obj != "object") {
      throw new TypeError("Invalid object: " + obj);
    }
    let bag = Cc["@mozilla.org/hash-property-bag;1"].
      createInstance(Ci.nsIWritablePropertyBag);
    for (let k of Object.keys(obj)) {
      let value = this.fromValue(obj[k]);
      bag.setProperty(k, value);
    }
    return bag;
  },
  fromValue(value) {
    if (typeof value == "function") {
      return null; // Emulating the behavior of JSON.stringify with functions
    }
    if (Array.isArray(value)) {
      return value.map(this.fromValue, this);
    }
    if (value == null || typeof value != "object") {
      // Auto-converted to nsIVariant
      return value;
    }
    return this.fromObject(value);
  },
};



/**
 * Construct an instance of nsIAsyncShutdownClient from a
 * AsyncShutdown.Barrier client.
 *
 * @param {object} moduleClient A client, as returned from the `client`
 * property of an instance of `AsyncShutdown.Barrier`. This client will
 * serve as back-end for methods `addBlocker` and `removeBlocker`.
 * @constructor
 */
function nsAsyncShutdownClient(moduleClient) {
  if (!moduleClient) {
    throw new TypeError("nsAsyncShutdownClient expects one argument");
  }
  this._moduleClient = moduleClient;
  this._byName = new Map();
}
nsAsyncShutdownClient.prototype = {
  _getPromisified(xpcomBlocker) {
    let candidate = this._byName.get(xpcomBlocker.name);
    if (!candidate) {
      return null;
    }
    if (candidate.xpcom === xpcomBlocker) {
      return candidate.jsm;
    }
    return null;
  },
  _setPromisified(xpcomBlocker, moduleBlocker) {
    let candidate = this._byName.get(xpcomBlocker.name);
    if (!candidate) {
      this._byName.set(xpcomBlocker.name, {xpcom: xpcomBlocker,
                                           jsm: moduleBlocker});
      return;
    }
    if (candidate.xpcom === xpcomBlocker) {
      return;
    }
    throw new Error("We have already registered a distinct blocker with the same name: " + xpcomBlocker.name);
  },
  _deletePromisified(xpcomBlocker) {
    let candidate = this._byName.get(xpcomBlocker.name);
    if (!candidate || candidate.xpcom !== xpcomBlocker) {
      return false;
    }
    this._byName.delete(xpcomBlocker.name);
    return true;
  },
  get jsclient() {
    return this._moduleClient;
  },
  get name() {
    return this._moduleClient.name;
  },
  addBlocker(/* nsIAsyncShutdownBlocker*/ xpcomBlocker,
      fileName, lineNumber, stack) {
    // We need a Promise-based function with the same behavior as
    // `xpcomBlocker`. Furthermore, to support `removeBlocker`, we
    // need to ensure that we always get the same Promise-based
    // function if we call several `addBlocker`/`removeBlocker` several
    // times with the same `xpcomBlocker`.
    //
    // Ideally, this should be done with a WeakMap() with xpcomBlocker
    // as a key, but XPConnect NativeWrapped objects cannot serve as
    // WeakMap keys.
    //
    let moduleBlocker = this._getPromisified(xpcomBlocker);
    if (!moduleBlocker) {
      moduleBlocker = () => new Promise(
        // This promise is never resolved. By opposition to AsyncShutdown
        // blockers, `nsIAsyncShutdownBlocker`s are always lifted by calling
        // `removeBlocker`.
        () => xpcomBlocker.blockShutdown(this)
      );

      this._setPromisified(xpcomBlocker, moduleBlocker);
    }

    this._moduleClient.addBlocker(xpcomBlocker.name,
      moduleBlocker,
      {
        fetchState: () => {
          let state = xpcomBlocker.state;
          if (state) {
            return PropertyBagConverter.toValue(state);
          }
          return null;
        },
        filename: fileName,
        lineNumber,
        stack,
      });
  },

  removeBlocker(xpcomBlocker) {
    let moduleBlocker = this._getPromisified(xpcomBlocker);
    if (!moduleBlocker) {
      return false;
    }
    this._deletePromisified(xpcomBlocker);
    return this._moduleClient.removeBlocker(moduleBlocker);
  },

  /* ........ QueryInterface .............. */
  QueryInterface: ChromeUtils.generateQI([Ci.nsIAsyncShutdownBarrier]),
  classID: Components.ID("{314e9e96-cc37-4d5c-843b-54709ce11426}"),
};

/**
 * Construct an instance of nsIAsyncShutdownBarrier from an instance
 * of AsyncShutdown.Barrier.
 *
 * @param {object} moduleBarrier an instance if
 * `AsyncShutdown.Barrier`. This instance will serve as back-end for
 * all methods.
 * @constructor
 */
function nsAsyncShutdownBarrier(moduleBarrier) {
  this._client = new nsAsyncShutdownClient(moduleBarrier.client);
  this._moduleBarrier = moduleBarrier;
}
nsAsyncShutdownBarrier.prototype = {
  get state() {
    return PropertyBagConverter.fromValue(this._moduleBarrier.state);
  },
  get client() {
    return this._client;
  },
  wait(onReady) {
    this._moduleBarrier.wait().then(() => {
      onReady.done();
    });
    // By specification, _moduleBarrier.wait() cannot reject.
  },

  /* ........ QueryInterface .............. */
  QueryInterface: ChromeUtils.generateQI([Ci.nsIAsyncShutdownBarrier]),
  classID: Components.ID("{29a0e8b5-9111-4c09-a0eb-76cd02bf20fa}"),
};

function nsAsyncShutdownService() {
  // Cache for the getters

  for (let _k of
   [// Parent process
    "profileBeforeChange",
    "profileChangeTeardown",
    "quitApplicationGranted",
    "sendTelemetry",

    // Child processes
    "contentChildShutdown",

    // All processes
    "webWorkersShutdown",
    "xpcomWillShutdown",
    ]) {
    let k = _k;
    Object.defineProperty(this, k, {
      configurable: true,
      get() {
        delete this[k];
        let wrapped = AsyncShutdown[k]; // May be undefined, if we're on the wrong process.
        let result = wrapped ? new nsAsyncShutdownClient(wrapped) : undefined;
        Object.defineProperty(this, k, {
          value: result
        });
        return result;
      }
    });
  }

  // Hooks for testing purpose
  this.wrappedJSObject = {
    _propertyBagConverter: PropertyBagConverter
  };
}
nsAsyncShutdownService.prototype = {
  makeBarrier(name) {
    return new nsAsyncShutdownBarrier(new AsyncShutdown.Barrier(name));
  },

  /* ........ QueryInterface .............. */
  QueryInterface: ChromeUtils.generateQI([Ci.nsIAsyncShutdownService]),
  classID: Components.ID("{35c496de-a115-475d-93b5-ffa3f3ae6fe3}"),
};


this.NSGetFactory = XPCOMUtils.generateNSGetFactory([
    nsAsyncShutdownService,
    nsAsyncShutdownBarrier,
    nsAsyncShutdownClient,
]);
