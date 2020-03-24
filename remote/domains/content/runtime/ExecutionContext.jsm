/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["ExecutionContext"];

const uuidGen = Cc["@mozilla.org/uuid-generator;1"].getService(
  Ci.nsIUUIDGenerator
);

const TYPED_ARRAY_CLASSES = [
  "Uint8Array",
  "Uint8ClampedArray",
  "Uint16Array",
  "Uint32Array",
  "Int8Array",
  "Int16Array",
  "Int32Array",
  "Float32Array",
  "Float64Array",
];
function uuid() {
  return uuidGen
    .generateUUID()
    .toString()
    .slice(1, -1);
}

/**
 * This class represent a debuggable context onto which we can evaluate Javascript.
 * This is typically a document, but it could also be a worker, an add-on, ... or
 * any kind of context involving JS scripts.
 *
 * @param {Debugger} dbg
 *   A Debugger instance that we can use to inspect the given global.
 * @param {GlobalObject} debuggee
 *   The debuggable context's global object. This is typically the document window
 *   object. But it can also be any global object, like a worker global scope object.
 */
class ExecutionContext {
  constructor(dbg, debuggee, id, isDefault) {
    this._debugger = dbg;
    this._debuggee = this._debugger.addDebuggee(debuggee);

    // Here, we assume that debuggee is a window object and we will propably have
    // to adapt that once we cover workers or contexts that aren't a document.
    const { windowUtils } = debuggee;
    this.windowId = windowUtils.currentInnerWindowID;
    this.id = id;
    this.frameId = debuggee.docShell.browsingContext.id.toString();
    this.isDefault = isDefault;

    this._remoteObjects = new Map();
  }

  destructor() {
    this._debugger.removeDebuggee(this._debuggee);
  }

  hasRemoteObject(id) {
    return this._remoteObjects.has(id);
  }

  getRemoteObject(id) {
    return this._remoteObjects.get(id);
  }

  releaseObject(id) {
    return this._remoteObjects.delete(id);
  }

  /**
   * Evaluate a Javascript expression.
   *
   * @param {String} expression
   *   The JS expression to evaluate against the JS context.
   * @param {boolean} returnByValue
   *     Whether the result is expected to be a JSON object
   *     that should be sent by value.
   *
   * @return {Object} A multi-form object depending if the execution
   *   succeed or failed. If the expression failed to evaluate,
   *   it will return an object with an `exceptionDetails` attribute
   *   matching the `ExceptionDetails` CDP type. Otherwise it will
   *   return an object with `result` attribute whose type is
   *   `RemoteObject` CDP type.
   */
  evaluate(expression, returnByValue) {
    let rv = this._debuggee.executeInGlobal(expression);
    if (!rv) {
      return {
        exceptionDetails: {
          text: "Evaluation terminated!",
        },
      };
    }

    if (rv.throw) {
      return this._returnError(rv.throw);
    }

    let result;
    if (returnByValue) {
      result = this._toRemoteObjectByValue(result);
    } else {
      result = this._toRemoteObject(rv.return);
    }

    return { result };
  }

  /**
   * Given a Debugger.Object reference for an Exception, return a JSON object
   * describing the exception by following CDP ExceptionDetails specification.
   */
  _returnError(exception) {
    if (
      this._debuggee.executeInGlobalWithBindings("exception instanceof Error", {
        exception,
      }).return
    ) {
      const text = this._debuggee.executeInGlobalWithBindings(
        "exception.message",
        { exception }
      ).return;
      return {
        exceptionDetails: {
          text,
        },
      };
    }

    // If that isn't an Error, consider the exception as a JS value
    return {
      exceptionDetails: {
        exception: this._toRemoteObject(exception),
      },
    };
  }

  async callFunctionOn(
    functionDeclaration,
    callArguments = [],
    returnByValue = false,
    awaitPromise = false,
    objectId = null
  ) {
    // Map the given objectId to a JS reference.
    let thisArg = null;
    if (objectId) {
      thisArg = this._remoteObjects.get(objectId);
      if (!thisArg) {
        throw new Error(`Unable to get target object with id: ${objectId}`);
      }
    }

    // First evaluate the function
    const fun = this._debuggee.executeInGlobal("(" + functionDeclaration + ")");
    if (!fun) {
      return {
        exceptionDetails: {
          text: "Evaluation terminated!",
        },
      };
    }
    if (fun.throw) {
      return this._returnError(fun.throw);
    }

    // Then map all input arguments, which are matching CDP's CallArguments type,
    // into JS values
    const args = callArguments.map(arg => this._fromCallArgument(arg));

    // Finally, call the function with these arguments
    const rv = fun.return.apply(thisArg, args);
    if (rv.throw) {
      return this._returnError(rv.throw);
    }

    let result = rv.return;

    if (result && result.isPromise && awaitPromise) {
      if (result.promiseState === "fulfilled") {
        result = result.promiseValue;
      } else if (result.promiseState === "rejected") {
        return this._returnError(result.promiseReason);
      } else {
        try {
          const promiseResult = await result.unsafeDereference();
          result = this._debuggee.makeDebuggeeValue(promiseResult);
        } catch (e) {
          // The promise has been rejected
          return this._returnError(e);
        }
      }
    }

    if (returnByValue) {
      result = this._toRemoteObjectByValue(result);
    } else {
      result = this._toRemoteObject(result);
    }

    return { result };
  }

  getProperties({ objectId, ownProperties }) {
    let obj = this.getRemoteObject(objectId);
    if (!obj) {
      throw new Error("Cannot find object with id = " + objectId);
    }
    const result = [];
    const serializeObject = (obj, isOwn) => {
      for (const propertyName of obj.getOwnPropertyNames()) {
        const descriptor = obj.getOwnPropertyDescriptor(propertyName);
        result.push({
          name: propertyName,

          configurable: descriptor.configurable,
          enumerable: descriptor.enumerable,
          writable: descriptor.writable,
          value: this._toRemoteObject(descriptor.value),
          get: descriptor.get
            ? this._toRemoteObject(descriptor.get)
            : undefined,
          set: descriptor.set
            ? this._toRemoteObject(descriptor.set)
            : undefined,

          isOwn,
        });
      }
    };

    // When `ownProperties` is set to true, we only iterate over own properties.
    // Otherwise, we also iterate over propreties inherited from the prototype chain.
    serializeObject(obj, true);

    if (!ownProperties) {
      while (true) {
        obj = obj.proto;
        if (!obj) {
          break;
        }
        serializeObject(obj, false);
      }
    }

    return {
      result,
    };
  }

  /**
   * Given a CDP `CallArgument`, return a JS value that represent this argument.
   * Note that `CallArgument` is actually very similar to `RemoteObject`
   */
  _fromCallArgument(arg) {
    if (arg.objectId) {
      if (!this._remoteObjects.has(arg.objectId)) {
        throw new Error(`Cannot find object with ID: ${arg.objectId}`);
      }
      return this._remoteObjects.get(arg.objectId);
    }

    if (arg.unserializableValue) {
      switch (arg.unserializableValue) {
        case "-0":
          return -0;
        case "Infinity":
          return Infinity;
        case "-Infinity":
          return -Infinity;
        case "NaN":
          return NaN;
        default:
          if (/^\d+n$/.test(arg.unserializableValue)) {
            // eslint-disable-next-line no-undef
            return BigInt(arg.unserializableValue.slice(0, -1));
          }
          throw new Error("Couldn't parse value object in call argument");
      }
    }

    return this._deserialize(arg.value);
  }

  /**
   * Given a JS value, create a copy of it within the debugee compartment.
   */
  _deserialize(obj) {
    if (typeof obj !== "object") {
      return obj;
    }
    const result = this._debuggee.executeInGlobalWithBindings(
      "JSON.parse(obj)",
      { obj: JSON.stringify(obj) }
    );
    if (result.throw) {
      throw new Error("Unable to deserialize object");
    }
    return result.return;
  }

  /**
   * Given a `Debugger.Object` object, return a JSON-serializable description of it
   * matching `RemoteObject` CDP type.
   *
   * @param {Debugger.Object} debuggerObj
   *  The object to serialize
   * @return {RemoteObject}
   *  The serialized description of the given object
   */
  _toRemoteObject(debuggerObj) {
    // First handle all non-primitive values which are going to be wrapped by the
    // Debugger API into Debugger.Object instances
    if (debuggerObj instanceof Debugger.Object) {
      const objectId = uuid();
      this._remoteObjects.set(objectId, debuggerObj);
      const rawObj = debuggerObj.unsafeDereference();

      // Map the Debugger API `class` attribute to CDP `subtype`
      const cls = debuggerObj.class;
      let subtype;
      if (debuggerObj.isProxy) {
        subtype = "proxy";
      } else if (cls == "Array") {
        subtype = "array";
      } else if (cls == "RegExp") {
        subtype = "regexp";
      } else if (cls == "Date") {
        subtype = "date";
      } else if (cls == "Map") {
        subtype = "map";
      } else if (cls == "Set") {
        subtype = "set";
      } else if (cls == "WeakMap") {
        subtype = "weakmap";
      } else if (cls == "WeakSet") {
        subtype = "weakset";
      } else if (cls == "Error") {
        subtype = "error";
      } else if (cls == "Promise") {
        subtype = "promise";
      } else if (TYPED_ARRAY_CLASSES.includes(cls)) {
        subtype = "typedarray";
      } else if (Node.isInstance(rawObj)) {
        subtype = "node";
      }

      const type = typeof rawObj;
      return { objectId, type, subtype };
    }

    // Now, handle all values that Debugger API isn't wrapping into Debugger.API.
    // This is all the primitive JS types.
    const type = typeof debuggerObj;

    // Symbol and BigInt are primitive values but aren't serializable.
    // CDP expects them to be considered as objects, with an objectId to later inspect
    // them.
    if (type == "symbol" || type == "bigint") {
      const objectId = uuid();
      this._remoteObjects.set(objectId, debuggerObj);
      return { objectId, type };
    }

    // A few primitive type can't be serialized and CDP has special case for them
    let unserializableValue = undefined;
    if (Object.is(debuggerObj, NaN)) {
      unserializableValue = "NaN";
    } else if (Object.is(debuggerObj, -0)) {
      unserializableValue = "-0";
    } else if (Object.is(debuggerObj, Infinity)) {
      unserializableValue = "Infinity";
    } else if (Object.is(debuggerObj, -Infinity)) {
      unserializableValue = "-Infinity";
    }
    if (unserializableValue) {
      return {
        unserializableValue,
      };
    }

    // Otherwise, we serialize the primitive values as-is via `value` attribute

    // null is special as it has a dedicated subtype
    let subtype;
    if (debuggerObj === null) {
      subtype = "null";
    }

    return {
      type,
      subtype,
      value: debuggerObj,
    };
  }

  /**
   * Given a `Debugger.Object` object, return a JSON-serializable description of it
   * matching `RemoteObject` CDP type.
   *
   * @param {Debugger.Object} debuggerObj
   *  The object to serialize
   * @return {RemoteObject}
   *  The serialized description of the given object
   */
  _toRemoteObjectByValue(debuggerObj) {
    const type = typeof debuggerObj;

    if (type == "undefined") {
      return { type };
    }

    let unserializableValue = undefined;
    if (Object.is(debuggerObj, -0)) {
      unserializableValue = "-0";
    } else if (Object.is(debuggerObj, NaN)) {
      unserializableValue = "NaN";
    } else if (Object.is(debuggerObj, Infinity)) {
      unserializableValue = "Infinity";
    } else if (Object.is(debuggerObj, -Infinity)) {
      unserializableValue = "-Infinity";
    } else if (typeof debuggerObj == "bigint") {
      unserializableValue = `${debuggerObj}n`;
    }

    if (unserializableValue) {
      return {
        type,
        unserializableValue,
        description: unserializableValue,
      };
    }

    const value = this._serialize(debuggerObj);
    return {
      type: typeof value,
      value,
      description: value != null ? value.toString() : value,
    };
  }

  /**
   * Convert a given `Debugger.Object` to an object.
   *
   * @param {Debugger.Object} obj
   *  The object to convert
   *
   * @return {Object}
   *  The converted object
   */
  _serialize(debuggerObj) {
    const result = this._debuggee.executeInGlobalWithBindings(
      "JSON.stringify(e)",
      { e: debuggerObj }
    );
    if (result.throw) {
      throw new Error("Object is not serializable");
    }

    return JSON.parse(result.return);
  }
}
