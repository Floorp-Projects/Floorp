/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * An implementation of nsIAsyncShutdown* based on AsyncShutdown.sys.mjs
 */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.sys.mjs",
});

/**
 * Conversion between nsIPropertyBags and JS values.
 * This uses a conservative approach to avoid losing data and doesn't throw.
 * Don't use this if you need perfect serialization and deserialization.
 */
class PropertyBagConverter {
  /**
   * When the js value to convert is a primitive, it is stored in the property
   * bag under a key with this name.
   */
  get primitiveProperty() {
    return "PropertyBagConverter_primitive";
  }

  /**
   * Converts from a PropertyBag to a JS value.
   * @param {nsIPropertyBag} bag The PropertyBag to convert.
   * @returns {jsval} A JS value.
   */
  propertyBagToJsValue(bag) {
    if (!(bag instanceof Ci.nsIPropertyBag)) {
      return null;
    }
    let result = {};
    for (let { name, value: property } of bag.enumerator) {
      let value = this.#toValue(property);
      if (name == this.primitiveProperty) {
        return value;
      }
      result[name] = value;
    }
    return result;
  }

  #toValue(property) {
    if (property instanceof Ci.nsIPropertyBag) {
      return this.propertyBagToJsValue(property);
    }
    if (["number", "boolean"].includes(typeof property)) {
      return property;
    }
    try {
      return JSON.parse(property);
    } catch (ex) {
      // Not JSON.
    }
    return property;
  }

  /**
   * Converts from a JS value to a PropertyBag.
   * @param {jsval} val JS value to convert.
   * @returns {nsIPropertyBag} A PropertyBag.
   * @note function is converted to "(function)" and undefined to null.
   */
  jsValueToPropertyBag(val) {
    let bag = Cc["@mozilla.org/hash-property-bag;1"].createInstance(
      Ci.nsIWritablePropertyBag
    );
    if (val && typeof val == "object") {
      for (let k of Object.keys(val)) {
        bag.setProperty(k, this.#fromValue(val[k]));
      }
    } else {
      bag.setProperty(this.primitiveProperty, this.#fromValue(val));
    }
    return bag;
  }

  #fromValue(value) {
    if (typeof value == "function") {
      return "(function)";
    }
    if (value === undefined) {
      value = null;
    }
    if (["number", "boolean", "string"].includes(typeof value)) {
      return value;
    }
    return JSON.stringify(value);
  }
}

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
  this._byXpcomBlocker = new Map();
}
nsAsyncShutdownClient.prototype = {
  get jsclient() {
    return this._moduleClient;
  },
  get name() {
    return this._moduleClient.name;
  },
  get isClosed() {
    return this._moduleClient.isClosed;
  },
  addBlocker(
    /* nsIAsyncShutdownBlocker*/ xpcomBlocker,
    fileName,
    lineNumber,
    stack
  ) {
    // We need a Promise-based function with the same behavior as
    // `xpcomBlocker`. Furthermore, to support `removeBlocker`, we
    // need to ensure that we always get the same Promise-based
    // function if we call several `addBlocker`/`removeBlocker` several
    // times with the same `xpcomBlocker`.

    if (this._byXpcomBlocker.has(xpcomBlocker)) {
      throw new Error(
        `We have already registered the blocker (${xpcomBlocker.name})`
      );
    }

    // Ideally, this should be done with a WeakMap() with xpcomBlocker
    // as a key, but XPCWrappedNative objects cannot serve as WeakMap keys, see
    // bug 1834365.
    const moduleBlocker = () =>
      new Promise(
        // This promise is never resolved. By opposition to AsyncShutdown
        // blockers, `nsIAsyncShutdownBlocker`s are always lifted by calling
        // `removeBlocker`.
        () => xpcomBlocker.blockShutdown(this)
      );

    this._byXpcomBlocker.set(xpcomBlocker, moduleBlocker);
    this._moduleClient.addBlocker(xpcomBlocker.name, moduleBlocker, {
      fetchState: () =>
        new PropertyBagConverter().propertyBagToJsValue(xpcomBlocker.state),
      filename: fileName,
      lineNumber,
      stack,
    });
  },

  removeBlocker(xpcomBlocker) {
    let moduleBlocker = this._byXpcomBlocker.get(xpcomBlocker);
    if (!moduleBlocker) {
      return false;
    }
    this._byXpcomBlocker.delete(xpcomBlocker);
    return this._moduleClient.removeBlocker(moduleBlocker);
  },

  QueryInterface: ChromeUtils.generateQI(["nsIAsyncShutdownClient"]),
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
    return new PropertyBagConverter().jsValueToPropertyBag(
      this._moduleBarrier.state
    );
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

  QueryInterface: ChromeUtils.generateQI(["nsIAsyncShutdownBarrier"]),
};

export function nsAsyncShutdownService() {
  // Cache for the getters

  for (let _k of [
    // Parent process
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
        let wrapped = lazy.AsyncShutdown[k]; // May be undefined, if we're on the wrong process.
        let result = wrapped ? new nsAsyncShutdownClient(wrapped) : undefined;
        Object.defineProperty(this, k, {
          value: result,
        });
        return result;
      },
    });
  }

  // Hooks for testing purpose
  this.wrappedJSObject = {
    _propertyBagConverter: PropertyBagConverter,
  };
}

nsAsyncShutdownService.prototype = {
  makeBarrier(name) {
    return new nsAsyncShutdownBarrier(new lazy.AsyncShutdown.Barrier(name));
  },

  QueryInterface: ChromeUtils.generateQI(["nsIAsyncShutdownService"]),
};
