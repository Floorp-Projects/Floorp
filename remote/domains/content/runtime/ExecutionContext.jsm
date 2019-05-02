/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["ExecutionContext"];

const uuidGen = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator);

const TYPED_ARRAY_CLASSES = ["Uint8Array", "Uint8ClampedArray", "Uint16Array",
                             "Uint32Array", "Int8Array", "Int16Array", "Int32Array",
                             "Float32Array", "Float64Array"];
function uuid() {
  return uuidGen.generateUUID().toString().slice(1, -1);
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
  constructor(dbg, debuggee) {
    this._debugger = dbg;
    this._debuggee = this._debugger.addDebuggee(debuggee);

    this._remoteObjects = new Map();
  }

  destructor() {
    this._debugger.removeDebuggee(this._debuggee);
  }

  /**
   * Evaluate a Javascript expression.
   *
   * @param {String} expression
   *   The JS expression to evaluate against the JS context.
   * @return {Object} A multi-form object depending if the execution succeed or failed.
   *   If the expression failed to evaluate, it will return an object with an
   *   `exceptionDetails` attribute matching the `ExceptionDetails` CDP type.
   *   Otherwise it will return an object with `result` attribute whose type is
   *   `RemoteObject` CDP type.
   */
  evaluate(expression) {
    let rv = this._debuggee.executeInGlobal(expression);
    if (!rv) {
      return {
        exceptionDetails: {
          text: "Evaluation terminated!",
        },
      };
    }
    if (rv.throw) {
      if (this._debuggee.executeInGlobalWithBindings("e instanceof Error", {e: rv.throw}).return) {
        return {
          exceptionDetails: {
            text: this._debuggee.executeInGlobalWithBindings("e.message", {e: rv.throw}).return,
          },
        };
      }
      return {
        exceptionDetails: {
          exception: this._createRemoteObject(rv.throw),
        },
      };
    }
    return {
      result: this._createRemoteObject(rv.return),
    };
  }

  /**
   * Convert a given `Debugger.Object` to a JSON string.
   *
   * @param {Debugger.Object} obj
   *  The object to convert
   * @return {String}
   *  The JSON string
   */
  _serialize(obj) {
    const result = this._debuggee.executeInGlobalWithBindings("JSON.stringify(e)", {e: obj});
    if (result.throw) {
      throw new Error("Object is not serializable");
    }
    return JSON.parse(result.return);
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
  _createRemoteObject(debuggerObj) {
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
      } else if (cls == "Object" && Node.isInstance(rawObj)) {
        subtype = "node";
      }

      const type = typeof rawObj;
      return {objectId, type, subtype};
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
      return {objectId, type};
    }

    // A few primitive type can't be serialized and CDP has special case for them
    let unserializableValue = undefined;
    if (Object.is(debuggerObj, NaN))
      unserializableValue = "NaN";
    else if (Object.is(debuggerObj, -0))
      unserializableValue = "-0";
    else if (Object.is(debuggerObj, Infinity))
      unserializableValue = "Infinity";
    else if (Object.is(debuggerObj, -Infinity))
      unserializableValue = "-Infinity";
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
}
